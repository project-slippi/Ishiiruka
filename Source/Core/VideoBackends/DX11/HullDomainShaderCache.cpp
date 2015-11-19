// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/FileUtil.h"
#include "Common/LinearDiskCache.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"

#include "VideoBackends/DX11/D3DBase.h"
#include "VideoBackends/DX11/D3DPtr.h"
#include "VideoBackends/DX11/D3DShader.h"
#include "VideoBackends/DX11/D3DUtil.h"
#include "VideoBackends/DX11/FramebufferManager.h"
#include "VideoBackends/DX11/HullDomainShaderCache.h"
#include "VideoBackends/DX11/Globals.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/TessellationShaderManager.h"
#include "VideoCommon/HLSLCompiler.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{

	HullDomainShaderCache::HDCache HullDomainShaderCache::s_hulldomain_shaders;
	const HullDomainShaderCache::HDCacheEntry* HullDomainShaderCache::s_last_entry;
	TessellationShaderUid HullDomainShaderCache::s_last_uid;
	TessellationShaderUid HullDomainShaderCache::s_external_last_uid;
	UidChecker<TessellationShaderUid, ShaderCode> HullDomainShaderCache::HullDomain_uid_checker;

	static HLSLAsyncCompiler *s_compiler;
	static Common::SpinLock<true> s_hulldomain_shaders_lock;

	std::unique_ptr<D3D::ConstantStreamBuffer> hdscbuf;
	static UINT s_hdscbuf_offset = 0;
	static UINT s_hdscbuf_size = 0;

	LinearDiskCache<TessellationShaderUid, u8> g_hs_disk_cache;
	LinearDiskCache<TessellationShaderUid, u8> g_ds_disk_cache;

	std::tuple<ID3D11Buffer*, UINT, UINT>  HullDomainShaderCache::GetConstantBuffer()
	{
		if (TessellationShaderManager::IsDirty())
		{
			s_hdscbuf_size = sizeof(TessellationShaderConstants);
			s_hdscbuf_offset = hdscbuf->AppendData((void*)&TessellationShaderManager::constants, s_hdscbuf_size);
			TessellationShaderManager::Clear();
			ADDSTAT(stats.thisFrame.bytesUniformStreamed, s_hdscbuf_size);
			s_hdscbuf_size = (UINT)(((s_hdscbuf_size + 255) & (~255)) / 16);
		}
		return std::tuple<ID3D11Buffer*, UINT, UINT>(hdscbuf->GetBuffer(), s_hdscbuf_offset, s_hdscbuf_size);
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
		u32 gbsize = ROUND_UP(sizeof(TessellationShaderConstants), 16) * (use_partial_buffer_update ? 1024 : 1); // must be a multiple of 16
		hdscbuf.reset(new D3D::ConstantStreamBuffer(gbsize));
		ID3D11Buffer* buf = hdscbuf->GetBuffer();
		CHECK(buf != nullptr, "Create Hull Domain shader constant buffer (size=%u)", gbsize);
		D3D::SetDebugObjectName(buf, "Hull Domain shader constant buffer used to emulate the GX pipeline");
		Clear();

		if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
			File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

		std::string h_cache_filename = StringFromFormat("%sIDX11-%s-hs.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
			SConfig::GetInstance().m_strUniqueID.c_str());
		HullShaderCacheInserter hinserter;
		g_hs_disk_cache.OpenAndRead(h_cache_filename, hinserter);

		std::string d_cache_filename = StringFromFormat("%sIDX11-%s-ds.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
			SConfig::GetInstance().m_strUniqueID.c_str());
		DomainShaderCacheInserter dinserter;
		g_ds_disk_cache.OpenAndRead(d_cache_filename, dinserter);
		SETSTAT(stats.numDomainShadersCreated, 0);
		SETSTAT(stats.numDomainShadersAlive, 0);		
		SETSTAT(stats.numHullShadersCreated, 0);
		SETSTAT(stats.numHullShadersAlive, 0);
		if (g_Config.bEnableShaderDebugging)
			Clear();
		s_last_entry = nullptr;
	}

	// ONLY to be used during shutdown.
	void HullDomainShaderCache::Clear()
	{
		for (auto& iter : s_hulldomain_shaders)
			iter.second.Destroy();
		s_hulldomain_shaders.clear();
		s_hulldomain_shaders_lock.unlock();
		HullDomain_uid_checker.Invalidate();
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
		g_hs_disk_cache.Sync();
		g_hs_disk_cache.Close();
		g_ds_disk_cache.Sync();
		g_ds_disk_cache.Close();
	}

	void HullDomainShaderCache::PrepareShader(
		const XFMemory &xfr,
		const BPMemory &bpm,
		const PrimitiveType primitiveType,
		const u32 components,
		bool ongputhread)
	{
		if (primitiveType != PrimitiveType::PRIMITIVE_TRIANGLES || !g_ActiveConfig.TessellationEnabled() || !g_ActiveConfig.PixelLightingEnabled(xfr, components))
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
		GetTessellationShaderUid(uid, API_D3D11, xfr, bpm, components);
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
		s_hulldomain_shaders_lock.lock();
		HDCacheEntry* entry = &s_hulldomain_shaders[uid];
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
		GenerateTessellationShaderCode(code, API_D3D11, xfr, bpm, components);
		memcpy(wunitd->code.data(), wunit->code.data(), code.BufferSize());
		
		wunit->codesize = (u32)code.BufferSize();
		wunit->entrypoint = "HS_TFO";
		wunit->flags = D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_SKIP_OPTIMIZATION;
		wunit->target = D3D::HullShaderVersionString();

		wunitd->codesize = (u32)code.BufferSize();
		wunitd->entrypoint = "DS_TFO";
		wunitd->flags = D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_OPTIMIZATION_LEVEL3;
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
				char szTemp[MAX_PATH];
				sprintf(szTemp, "%sbad_hs_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
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
				char szTemp[MAX_PATH];
				sprintf(szTemp, "%sbad_ds_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
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
				SETSTAT(stats.numDomainShadersAlive, s_hulldomain_shaders.size());
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
				SETSTAT(stats.numHullShadersAlive, s_hulldomain_shaders.size());
			}
		}
	}

	void HullDomainShaderCache::InsertByteCode(const TessellationShaderUid &uid, const void* bytecode, u32 bytecodelen, bool isdomain)
	{
		HDCacheEntry* entry = &s_hulldomain_shaders[uid];
		entry->initialized.test_and_set();
		PushByteCode(bytecode, bytecodelen, entry, isdomain);
	}

}  // DX11
