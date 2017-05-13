// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/Align.h"
#include "Common/FileUtil.h"
#include "Common/LinearDiskCache.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Host.h"

#include "VideoBackends/DX11/D3DBase.h"
#include "VideoBackends/DX11/D3DPtr.h"
#include "VideoBackends/DX11/D3DShader.h"
#include "VideoBackends/DX11/D3DUtil.h"
#include "VideoBackends/DX11/FramebufferManager.h"
#include "VideoBackends/DX11/HullDomainShaderCache.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/TessellationShaderManager.h"
#include "VideoCommon/HLSLCompiler.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{

HullDomainShaderCache::HDCache* HullDomainShaderCache::s_hulldomain_shaders;
const HullDomainShaderCache::HDCacheEntry* HullDomainShaderCache::s_last_entry;
TessellationShaderUid HullDomainShaderCache::s_last_uid;
TessellationShaderUid HullDomainShaderCache::s_external_last_uid;

static HLSLAsyncCompiler *s_compiler;
static Common::SpinLock<true> s_hulldomain_shaders_lock;

std::unique_ptr<D3D::ConstantStreamBuffer> hdscbuf;

LinearDiskCache<TessellationShaderUid, u8> g_hs_disk_cache;
LinearDiskCache<TessellationShaderUid, u8> g_ds_disk_cache;

D3D::BufferDescriptor  HullDomainShaderCache::GetConstantBuffer()
{
	if (TessellationShaderManager::IsDirty())
	{
		const size_t hdscbuf_size = sizeof(TessellationShaderConstants);
		hdscbuf->AppendData((void*)&TessellationShaderManager::constants, hdscbuf_size);
		TessellationShaderManager::Clear();
		ADDSTAT(stats.thisFrame.bytesUniformStreamed, hdscbuf_size);
	}
	return hdscbuf->GetDescriptor();
}

// this class will load the precompiled shaders into our cache
class HullShaderCacheInserter : public LinearDiskCacheReader<TessellationShaderUid, u8>
{
public:
	void Read(const TessellationShaderUid &key, const u8* value, u32 value_size)
	{
		HullDomainShaderCache::InsertByteCode(key, value, value_size, false);
	}
};

class DomainShaderCacheInserter : public LinearDiskCacheReader<TessellationShaderUid, u8>
{
public:
	void Read(const TessellationShaderUid &key, const u8* value, u32 value_size)
	{
		HullDomainShaderCache::InsertByteCode(key, value, value_size, true);
	}
};

void HullDomainShaderCache::Init()
{
	s_compiler = &HLSLAsyncCompiler::getInstance();
	s_hulldomain_shaders_lock.unlock();

	bool use_partial_buffer_update = D3D::SupportPartialContantBufferUpdate();
	u32 gbsize = static_cast<u32>(Common::AlignUpSizePow2(sizeof(TessellationShaderConstants), 16) * (use_partial_buffer_update ? 1024 : 1)); // must be a multiple of 16
	hdscbuf.reset(new D3D::ConstantStreamBuffer(gbsize));
	ID3D11Buffer* buf = hdscbuf->GetBuffer();
	CHECK(buf != nullptr, "Create Hull Domain shader constant buffer (size=%u)", gbsize);
	D3D::SetDebugObjectName(buf, "Hull Domain shader constant buffer used to emulate the GX pipeline");
	Clear();

	if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

	pKey_t gameid = (pKey_t)GetMurmurHash3(reinterpret_cast<const u8*>(SConfig::GetInstance().GetGameID().data()), (u32)SConfig::GetInstance().GetGameID().size(), 0);
	s_hulldomain_shaders = HDCache::Create(
		gameid,
		TESSELLATIONSHADERGEN_UID_VERSION,
		"Ishiiruka.ts",
		StringFromFormat("%s.ts", SConfig::GetInstance().GetGameID().c_str())
	);

	std::string h_cache_filename = StringFromFormat("%sIDX11-%s-hs.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
		SConfig::GetInstance().GetGameID().c_str());
	HullShaderCacheInserter hinserter;
	g_hs_disk_cache.OpenAndRead(h_cache_filename, hinserter);

	std::string d_cache_filename = StringFromFormat("%sIDX11-%s-ds.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
		SConfig::GetInstance().GetGameID().c_str());
	DomainShaderCacheInserter dinserter;
	g_ds_disk_cache.OpenAndRead(d_cache_filename, dinserter);
	SETSTAT(stats.numDomainShadersCreated, 0);
	SETSTAT(stats.numDomainShadersAlive, 0);
	SETSTAT(stats.numHullShadersCreated, 0);
	SETSTAT(stats.numHullShadersAlive, 0);
	if (g_ActiveConfig.bCompileShaderOnStartup)
	{
		size_t shader_count = 0;
		s_hulldomain_shaders->ForEachMostUsedByCategory(gameid,
			[&](const TessellationShaderUid& it, size_t total)
		{
			TessellationShaderUid item = it;
			item.ClearHASH();
			item.CalculateUIDHash();
			CompileHDShader(item, true);
			shader_count++;
			if ((shader_count & 7) == 0)
			{
				Host_UpdateTitle(StringFromFormat("Compiling Tessellation Shaders %i %% (%i/%i)", (shader_count * 100) / total, shader_count, total));
				s_compiler->WaitForFinish();
			}
		},
			[](HDCacheEntry& entry)
		{
			return !entry.domainshader;
		}
		, true);
		s_compiler->WaitForFinish();
	}
	s_last_entry = nullptr;
}

// ONLY to be used during shutdown.
void HullDomainShaderCache::Clear()
{
	if (s_hulldomain_shaders)
	{
		s_hulldomain_shaders_lock.lock();
		s_hulldomain_shaders->Persist();
		s_hulldomain_shaders->Clear([](HDCacheEntry& item)
		{
			item.Destroy();
		});
		s_hulldomain_shaders_lock.unlock();
	}
	s_last_entry = nullptr;
}

void HullDomainShaderCache::Shutdown()
{
	if (s_compiler)
	{
		s_compiler->WaitForFinish();
	}
	hdscbuf.reset();

	Clear();
	delete s_hulldomain_shaders;
	s_hulldomain_shaders = nullptr;

	g_hs_disk_cache.Sync();
	g_hs_disk_cache.Close();
	g_ds_disk_cache.Sync();
	g_ds_disk_cache.Close();
}

void HullDomainShaderCache::CompileHDShader(const TessellationShaderUid& uid, bool ongputhread)
{
	s_hulldomain_shaders_lock.lock();
	HDCacheEntry* entry = &s_hulldomain_shaders->GetOrAdd(uid);
	s_hulldomain_shaders_lock.unlock();
	if (ongputhread)
	{
		s_last_entry = entry;
	}
	// Compile only when we have a new instance
	if (entry->initialized.test_and_set())
	{
		return;
	}

	// Need to compile a new shader
	ShaderCode code;
	ShaderCompilerWorkUnit *wunit = s_compiler->NewUnit(TESSELLATIONSHADERGEN_BUFFERSIZE);
	ShaderCompilerWorkUnit *wunitd = s_compiler->NewUnit(TESSELLATIONSHADERGEN_BUFFERSIZE);
	code.SetBuffer(wunit->code.data());
	GenerateTessellationShaderCode(code, API_D3D11, uid.GetUidData());
	memcpy(wunitd->code.data(), wunit->code.data(), code.BufferSize());

	wunit->codesize = (u32)code.BufferSize();
	wunit->entrypoint = "HS_TFO";
#if defined(_DEBUG) || defined(DEBUGFAST)
	wunit->flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	wunit->flags = D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	wunit->target = D3D::HullShaderVersionString();

	wunitd->codesize = (u32)code.BufferSize();
	wunitd->entrypoint = "DS_TFO";
#if defined(_DEBUG) || defined(DEBUGFAST)
	wunit->flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	wunitd->flags = D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
	wunitd->target = D3D::DomainShaderVersionString();
	wunit->ResultHandler = [uid, entry](ShaderCompilerWorkUnit* wunit)
	{
		if (SUCCEEDED(wunit->cresult))
		{
			ID3DBlob* shaderBuffer = wunit->shaderbytecode;
			const u8* bytecode = (const u8*)shaderBuffer->GetBufferPointer();
			u32 bytecodelen = (u32)shaderBuffer->GetBufferSize();
			g_hs_disk_cache.Append(uid, bytecode, bytecodelen);
			PushByteCode(bytecode, bytecodelen, entry, false);
		}
		else
		{
			static int num_failures = 0;
			std::string szTemp = StringFromFormat("%sbad_hs_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
			std::ofstream file;
			OpenFStream(file, szTemp, std::ios_base::out);
			file << ((const char *)wunit->code.data());
			file << ((const char *)wunit->error->GetBufferPointer());
			file.close();

			PanicAlert("Failed to compile Hull shader!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%s):\n%s",
				szTemp,
				D3D::HullShaderVersionString(),
				(char*)wunit->error->GetBufferPointer());
		}
		entry->hcompiled = true;
	};

	wunitd->ResultHandler = [uid, entry](ShaderCompilerWorkUnit* wunitd)
	{
		if (SUCCEEDED(wunitd->cresult))
		{
			ID3DBlob* shaderBuffer = wunitd->shaderbytecode;
			const u8* bytecode = (const u8*)shaderBuffer->GetBufferPointer();
			u32 bytecodelen = (u32)shaderBuffer->GetBufferSize();
			g_ds_disk_cache.Append(uid, bytecode, bytecodelen);
			PushByteCode(bytecode, bytecodelen, entry, true);
		}
		else
		{
			static int num_failures = 0;
			std::string szTemp = StringFromFormat("%sbad_ds_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
			std::ofstream file;
			OpenFStream(file, szTemp, std::ios_base::out);
			file << ((const char *)wunitd->code.data());
			file << ((const char *)wunitd->error->GetBufferPointer());
			file.close();

			PanicAlert("Failed to compile Domain shader!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%s):\n%s",
				szTemp,
				D3D::DomainShaderVersionString(),
				(char*)wunitd->error->GetBufferPointer());
		}
		entry->dcompiled = true;
	};
	s_compiler->CompileShaderAsync(wunit);
	s_compiler->CompileShaderAsync(wunitd);
}

void HullDomainShaderCache::PrepareShader(
	const XFMemory &xfr,
	const BPMemory &bpm,
	const PrimitiveType primitiveType,
	const u32 components,
	bool ongputhread)
{
	if (!(primitiveType == PrimitiveType::PRIMITIVE_TRIANGLES
		&& g_ActiveConfig.TessellationEnabled()
		&& xfr.projection.type == GX_PERSPECTIVE
		&& (g_ActiveConfig.bForcedLighting || g_ActiveConfig.PixelLightingEnabled(xfr, components))))
	{
		if (ongputhread)
		{
			s_last_entry = nullptr;
		}
		else
		{
			s_external_last_uid.ClearUID();
		}
		return;
	}
	TessellationShaderUid uid;
	GetTessellationShaderUID(uid, xfr, bpm, components);
	if (ongputhread)
	{
		s_compiler->ProcCompilationResults();
		// Check if the shader is already set
		if (s_last_entry)
		{
			if (uid == s_last_uid)
			{
				return;
			}
		}
		s_last_uid = uid;
		GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
	}
	else
	{
		if (s_external_last_uid == uid)
		{
			return;
		}
		s_external_last_uid = uid;
	}
	CompileHDShader(uid, ongputhread);
}

bool HullDomainShaderCache::TestShader()
{
	if (s_last_entry == nullptr)
	{
		return true;
	}
	int count = 0;
	while (!(s_last_entry->hcompiled && s_last_entry->dcompiled))
	{
		s_compiler->ProcCompilationResults();
		if (g_ActiveConfig.bFullAsyncShaderCompilation)
		{
			break;
		}
		Common::cYield(count++);
	}
	return s_last_entry->hcompiled && s_last_entry->dcompiled;
}

void HullDomainShaderCache::PushByteCode(const void* bytecode, unsigned int bytecodelen, HullDomainShaderCache::HDCacheEntry* entry, bool isdomain)
{
	if (isdomain)
	{
		entry->domainshader = std::move(D3D::CreateDomainShaderFromByteCode(bytecode, bytecodelen));
		entry->dcompiled = true;
		if (entry->domainshader != nullptr)
		{
			// TODO: Somehow make the debug name a bit more specific
			D3D::SetDebugObjectName(entry->domainshader.get(), "a Domanin shader of HullDomainShaderCache");
			INCSTAT(stats.numDomainShadersCreated);
			SETSTAT(stats.numDomainShadersAlive, static_cast<int>(s_hulldomain_shaders->size()));
		}
	}
	else
	{
		entry->hullshader = std::move(D3D::CreateHullShaderFromByteCode(bytecode, bytecodelen));
		entry->hcompiled = true;
		if (entry->hullshader != nullptr)
		{
			// TODO: Somehow make the debug name a bit more specific
			D3D::SetDebugObjectName(entry->hullshader.get(), "a Hull shader of HullDomainShaderCache");
			INCSTAT(stats.numHullShadersCreated);
			SETSTAT(stats.numHullShadersAlive, static_cast<int>(s_hulldomain_shaders->size()));
		}
	}
}

void HullDomainShaderCache::InsertByteCode(const TessellationShaderUid &uid, const void* bytecode, u32 bytecodelen, bool isdomain)
{
	HDCacheEntry* entry = &s_hulldomain_shaders->GetOrAdd(uid);
	entry->initialized.test_and_set();
	PushByteCode(bytecode, bytecodelen, entry, isdomain);
}

}  // DX11
