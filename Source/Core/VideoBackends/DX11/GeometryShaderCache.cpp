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
#include "VideoBackends/DX11/GeometryShaderCache.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/HLSLCompiler.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{

GeometryShaderCache::GSCache* GeometryShaderCache::s_geometry_shaders;
const GeometryShaderCache::GSCacheEntry* GeometryShaderCache::s_last_entry;
GeometryShaderUid GeometryShaderCache::s_last_uid;
GeometryShaderUid GeometryShaderCache::s_external_last_uid;
const GeometryShaderCache::GSCacheEntry GeometryShaderCache::s_pass_entry;
static HLSLAsyncCompiler *s_compiler;
static Common::SpinLock<true> s_geometry_shaders_lock;

D3D::GeometryShaderPtr ClearGeometryShader;
D3D::GeometryShaderPtr CopyGeometryShader;

LinearDiskCache<GeometryShaderUid, u8> g_gs_disk_cache;

ID3D11GeometryShader* GeometryShaderCache::GetClearGeometryShader()
{
	return (g_ActiveConfig.iStereoMode > 0) ? ClearGeometryShader.get() : nullptr;
}
ID3D11GeometryShader* GeometryShaderCache::GetCopyGeometryShader()
{
	return (g_ActiveConfig.iStereoMode > 0) ? CopyGeometryShader.get() : nullptr;
}

D3D::ConstantStreamBuffer* gscbuf = nullptr;

D3D::BufferDescriptor  GeometryShaderCache::GetConstantBuffer()
{
	if (GeometryShaderManager::IsDirty())
	{
		const size_t gscbuf_size = sizeof(GeometryShaderConstants);
		gscbuf->AppendData((void*)&GeometryShaderManager::constants, gscbuf_size);
		GeometryShaderManager::Clear();
		ADDSTAT(stats.thisFrame.bytesUniformStreamed, gscbuf_size);
	}
	return gscbuf->GetDescriptor();
}

// this class will load the precompiled shaders into our cache
class GeometryShaderCacheInserter : public LinearDiskCacheReader<GeometryShaderUid, u8>
{
public:
	void Read(const GeometryShaderUid &key, const u8* value, u32 value_size)
	{
		GeometryShaderCache::InsertByteCode(key, value, value_size);
	}
};

const char* gs_clear_shader_code = R"hlsl(
struct VSOUTPUT
{
	float4 vPosition : SV_Position;
	float4 vColor0   : COLOR0;
};
struct GSOUTPUT
{
	float4 vPosition   : SV_Position;
	float4 vColor0   : COLOR0;
	uint slice    : SV_RenderTargetArrayIndex;
};
[maxvertexcount(6)]
void main(triangle VSOUTPUT o[3], inout TriangleStream<GSOUTPUT> Output)
{
	for(int slice = 0; slice < 2; slice++)
	{
		for(int i = 0; i < 3; i++)
		{
			GSOUTPUT OUT;
			OUT.vPosition = o[i].vPosition;
			OUT.vColor0 = o[i].vColor0;
			OUT.slice = slice;
			Output.Append(OUT);
		}
		Output.RestartStrip();
	}
}
)hlsl";

const char* gs_copy_shader_code = R"hlsl(
struct VSOUTPUT
{
	float4 vPosition : SV_Position;
	float3 vTexCoord : TEXCOORD0;
	float  vTexCoord1 : TEXCOORD1;
	float4  vTexCoord2 : TEXCOORD2;
	float4  vTexCoord3 : TEXCOORD2;
};
struct GSOUTPUT
{
	float4 vPosition : SV_Position;
	float3 vTexCoord : TEXCOORD0;
	float  vTexCoord1 : TEXCOORD1;
	float4  vTexCoord2 : TEXCOORD2;
	float4  vTexCoord3 : TEXCOORD3;
	uint slice    : SV_RenderTargetArrayIndex;
};
[maxvertexcount(6)]
void main(triangle VSOUTPUT o[3], inout TriangleStream<GSOUTPUT> Output)
{
	for(int slice = 0; slice < 2; slice++)
	{
		for(int i = 0; i < 3; i++)
		{
			GSOUTPUT OUT;
			OUT.vPosition = o[i].vPosition;
			OUT.vTexCoord = o[i].vTexCoord;
			OUT.vTexCoord1 = o[i].vTexCoord1;
			OUT.vTexCoord2 = o[i].vTexCoord2;
			OUT.vTexCoord3 = o[i].vTexCoord3;
			OUT.vTexCoord.z = slice;
			OUT.slice = slice;
			Output.Append(OUT);
		}
		Output.RestartStrip();
	}
}
)hlsl";

void GeometryShaderCache::Init()
{
	s_compiler = &HLSLAsyncCompiler::getInstance();
	s_geometry_shaders_lock.unlock();
	bool use_partial_buffer_update = D3D::SupportPartialContantBufferUpdate();
	u32 gbsize = static_cast<u32>(Common::AlignUpSizePow2(sizeof(GeometryShaderConstants), 16) * (use_partial_buffer_update ? 1024 : 1)); // must be a multiple of 16
	gscbuf = new D3D::ConstantStreamBuffer(gbsize);
	ID3D11Buffer* buf = gscbuf->GetBuffer();
	CHECK(buf != nullptr, "Create geometry shader constant buffer (size=%u)", gbsize);
	D3D::SetDebugObjectName(buf, "geometry shader constant buffer used to emulate the GX pipeline");

	// used when drawing clear quads
	ClearGeometryShader = D3D::CompileAndCreateGeometryShader(gs_clear_shader_code);
	CHECK(ClearGeometryShader != nullptr, "Create clear geometry shader");
	D3D::SetDebugObjectName(ClearGeometryShader.get(), "clear geometry shader");

	// used for buffer copy
	CopyGeometryShader = D3D::CompileAndCreateGeometryShader(gs_copy_shader_code);
	CHECK(CopyGeometryShader != nullptr, "Create copy geometry shader");
	D3D::SetDebugObjectName(CopyGeometryShader.get(), "copy geometry shader");

	Clear();

	if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

	pKey_t gameid = (pKey_t)GetMurmurHash3(reinterpret_cast<const u8*>(SConfig::GetInstance().m_strGameID.data()), (u32)SConfig::GetInstance().m_strGameID.size(), 0);
	s_geometry_shaders = GSCache::Create(
		gameid,
		GEOMETRYSHADERGEN_UID_VERSION,
		"Ishiiruka.gs",
		StringFromFormat("%s.gs", SConfig::GetInstance().m_strGameID.c_str())
	);

	std::string cache_filename = StringFromFormat("%sIDX11-%s-gs.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
		SConfig::GetInstance().m_strGameID.c_str());
	GeometryShaderCacheInserter inserter;
	g_gs_disk_cache.OpenAndRead(cache_filename, inserter);

	if (g_ActiveConfig.bCompileShaderOnStartup)
	{
		size_t shader_count = 0;
		s_geometry_shaders->ForEachMostUsedByCategory(gameid,
			[&](const GeometryShaderUid& it, size_t total)
		{
			GeometryShaderUid item = it;
			item.ClearHASH();
			item.CalculateUIDHash();
			CompileGShader(item, true);
			shader_count++;
			if ((shader_count & 7) == 0)
			{
				Host_UpdateTitle(StringFromFormat("Compiling Geometry Shaders %i %% (%i/%i)", (shader_count * 100) / total, shader_count, total));
				s_compiler->WaitForFinish();
			}
		},
			[](GSCacheEntry& entry)
		{
			return !entry.shader;
		}
		, true);
		s_compiler->WaitForFinish();
	}

	s_last_entry = nullptr;
}

// ONLY to be used during shutdown.
void GeometryShaderCache::Clear()
{
	if (s_geometry_shaders)
	{
		s_geometry_shaders_lock.lock();
		s_geometry_shaders->Persist();
		s_geometry_shaders->Clear([](GSCacheEntry& item)
		{
			item.Destroy();
		});
		s_geometry_shaders_lock.unlock();
	}
	s_last_entry = nullptr;
}

void GeometryShaderCache::Shutdown()
{
	if (s_compiler)
	{
		s_compiler->WaitForFinish();
	}
	if (gscbuf != nullptr)
	{
		delete gscbuf;
		gscbuf = nullptr;
	}

	ClearGeometryShader.reset();
	CopyGeometryShader.reset();


	Clear();
	delete s_geometry_shaders;
	s_geometry_shaders = nullptr;

	g_gs_disk_cache.Sync();
	g_gs_disk_cache.Close();
}

void GeometryShaderCache::CompileGShader(const GeometryShaderUid& uid, bool ongputhread)
{
	s_geometry_shaders_lock.lock();
	GSCacheEntry* entry = &s_geometry_shaders->GetOrAdd(uid);
	s_geometry_shaders_lock.unlock();
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
	ShaderCompilerWorkUnit *wunit = s_compiler->NewUnit(GEOMETRYSHADERGEN_BUFFERSIZE);
	wunit->GenerateCodeHandler = [uid](ShaderCompilerWorkUnit* wunit)
	{
		ShaderCode code;
		code.SetBuffer(wunit->code.data());
		GenerateGeometryShaderCode(code, uid.GetUidData(), API_D3D11);
		wunit->codesize = (u32)code.BufferSize();
	};

	wunit->entrypoint = "main";
#if defined(_DEBUG) || defined(DEBUGFAST)
	wunit->flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	wunit->flags = D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
	wunit->target = D3D::GeometryShaderVersionString();

	wunit->ResultHandler = [uid, entry](ShaderCompilerWorkUnit* wunit)
	{
		if (SUCCEEDED(wunit->cresult))
		{
			ID3DBlob* shaderBuffer = wunit->shaderbytecode;
			const u8* bytecode = (const u8*)shaderBuffer->GetBufferPointer();
			u32 bytecodelen = (u32)shaderBuffer->GetBufferSize();
			g_gs_disk_cache.Append(uid, bytecode, bytecodelen);
			PushByteCode(bytecode, bytecodelen, entry);
		}
		else
		{
			static int num_failures = 0;
			char szTemp[MAX_PATH];
			sprintf(szTemp, "%sbad_gs_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
			std::ofstream file;
			OpenFStream(file, szTemp, std::ios_base::out);
			file << ((const char *)wunit->code.data());
			file << ((const char *)wunit->error->GetBufferPointer());
			file.close();

			PanicAlert("Failed to compile geometry shader!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%s):\n%s",
				szTemp,
				D3D::GeometryShaderVersionString(),
				(char*)wunit->error->GetBufferPointer());
		}
	};
	s_compiler->CompileShaderAsync(wunit);
}

void GeometryShaderCache::PrepareShader(
	u32 primitive_type,
	const XFMemory &xfr,
	const u32 components,
	bool ongputhread)
{
	GeometryShaderUid uid;
	GetGeometryShaderUid(uid, primitive_type, xfr, components);
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
		// Check if the shader is a pass-through shader
		if (uid.GetUidData().IsPassthrough())
		{
			// Return the default pass-through shader
			s_last_entry = &s_pass_entry;
			return;
		}
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
	CompileGShader(uid, ongputhread);
}

bool GeometryShaderCache::TestShader()
{
	int count = 0;
	while (!s_last_entry->compiled)
	{
		s_compiler->ProcCompilationResults();
		if (g_ActiveConfig.bFullAsyncShaderCompilation)
		{
			break;
		}
		Common::cYield(count++);
	}
	return s_last_entry->shader != nullptr && s_last_entry->compiled;
}

void GeometryShaderCache::PushByteCode(const void* bytecode, unsigned int bytecodelen, GeometryShaderCache::GSCacheEntry* entry)
{
	entry->shader = std::move(D3D::CreateGeometryShaderFromByteCode(bytecode, bytecodelen));
	entry->compiled = true;
	if (entry->shader != nullptr)
	{
		// TODO: Somehow make the debug name a bit more specific
		D3D::SetDebugObjectName(entry->shader.get(), "a Geometry shader of GeometryShaderCache");
		INCSTAT(stats.numGeometryShadersCreated);
		SETSTAT(stats.numGeometryShadersAlive, static_cast<int>(s_geometry_shaders->size()));
	}
}

void GeometryShaderCache::InsertByteCode(const GeometryShaderUid &uid, const void* bytecode, u32 bytecodelen)
{
	GSCacheEntry* entry = &s_geometry_shaders->GetOrAdd(uid);
	entry->initialized.test_and_set();
	PushByteCode(bytecode, bytecodelen, entry);
}

}  // DX11
