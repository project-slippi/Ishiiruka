// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#include "Common/Common.h"
#include "Common/FileUtil.h"
#include "Common/LinearDiskCache.h"

#include "Core/ConfigManager.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/HLSLCompiler.h"

#include "VideoBackends/DX9/D3DBase.h"
#include "VideoBackends/DX9/D3DShader.h"
#include "VideoBackends/DX9/VertexShaderCache.h"


namespace DX9
{

VertexShaderCache::VSCache VertexShaderCache::vshaders;
const VertexShaderCache::VSCacheEntry *VertexShaderCache::last_entry;
VertexShaderUid VertexShaderCache::last_uid;
VertexShaderUid VertexShaderCache::external_last_uid;
static HLSLAsyncCompiler *Compiler;
static Common::SpinLock<true> vshaderslock;
#define MAX_SSAA_SHADERS 2

static LPDIRECT3DVERTEXSHADER9 SimpleVertexShader[MAX_SSAA_SHADERS];
static LPDIRECT3DVERTEXSHADER9 ClearVertexShader;

LinearDiskCache<VertexShaderUid, u8> g_vs_disk_cache;

LPDIRECT3DVERTEXSHADER9 VertexShaderCache::GetSimpleVertexShader(int level)
{
	return SimpleVertexShader[level ? 1 : 0];
}

LPDIRECT3DVERTEXSHADER9 VertexShaderCache::GetClearVertexShader()
{
	return ClearVertexShader;
}

// this class will load the precompiled shaders into our cache
class VertexShaderCacheInserter : public LinearDiskCacheReader<VertexShaderUid, u8>
{
public:
	void Read(const VertexShaderUid &key, const u8 *value, u32 value_size)
	{
		VertexShaderCache::InsertByteCode(key, value, value_size);
	}
};

void VertexShaderCache::Init()
{
	Compiler = &HLSLAsyncCompiler::getInstance();
	vshaderslock.unlock();
	const char* code = "struct VSOUTPUT\n"
		"{\n"
		"float4 vPosition : POSITION;\n"
		"float2 vTexCoord : TEXCOORD0;\n"
		"float vTexCoord1 : TEXCOORD1;\n"
		"};\n"
		"VSOUTPUT main(float4 inPosition : POSITION,float2 inTEX0 : TEXCOORD0,float2 inTEX1 : TEXCOORD1,float inTEX2 : TEXCOORD2)\n"
		"{\n"
		"VSOUTPUT OUT;\n"
		"OUT.vPosition = inPosition;\n"
		"OUT.vTexCoord = inTEX0;\n"
		"OUT.vTexCoord1 = inTEX2;\n"
		"return OUT;\n"
		"}\0";

	SimpleVertexShader[0] = D3D::CompileAndCreateVertexShader(code, (int)strlen(code));

	code = "struct VSOUTPUT\n"
		"{\n"
		"float4 vPosition   : POSITION;\n"
		"float4 vColor0   : COLOR0;\n"
		"};\n"
		"VSOUTPUT main(float4 inPosition : POSITION,float4 inColor0: COLOR0)\n"
		"{\n"
		"VSOUTPUT OUT;\n"
		"OUT.vPosition = inPosition;\n"
		"OUT.vColor0 = inColor0;\n"
		"return OUT;\n"
		"}\0";

	ClearVertexShader = D3D::CompileAndCreateVertexShader(code, (int)strlen(code));
	code = "struct VSOUTPUT\n"
		"{\n"
		"float4 vPosition   : POSITION;\n"
		"float4 vTexCoord   : TEXCOORD0;\n"
		"float  vTexCoord1   : TEXCOORD1;\n"
		"float4 vTexCoord2   : TEXCOORD2;\n"   
		"float4 vTexCoord3   : TEXCOORD3;\n"
		"};\n"
		"VSOUTPUT main(float4 inPosition : POSITION,float2 inTEX0 : TEXCOORD0,float2 inTEX1 : TEXCOORD1,float inTEX2 : TEXCOORD2)\n"
		"{\n"
		"VSOUTPUT OUT;"
		"OUT.vPosition = inPosition;\n"
		"OUT.vTexCoord  = inTEX0.xyyx;\n"
		"OUT.vTexCoord1 = inTEX2.x;\n"
		"OUT.vTexCoord2 = inTEX0.xyyx + (float4(-0.375f,-0.125f,-0.375f, 0.125f) * inTEX1.xyyx);\n"
		"OUT.vTexCoord3 = inTEX0.xyyx + (float4( 0.375f, 0.125f, 0.375f,-0.125f) * inTEX1.xyyx);\n"	
		"return OUT;\n"
		"}\0";
	SimpleVertexShader[1] = D3D::CompileAndCreateVertexShader(code, (int)strlen(code));

	Clear();	

	if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX).c_str());

	SETSTAT(stats.numVertexShadersCreated, 0);
	SETSTAT(stats.numVertexShadersAlive, 0);

	char cache_filename[MAX_PATH];
	sprintf(cache_filename, "%sIDX9-%s-vs.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
		SConfig::GetInstance().m_strUniqueID.c_str());
	VertexShaderCacheInserter inserter;
	vshaderslock.lock();
	g_vs_disk_cache.OpenAndRead(cache_filename, inserter);
	vshaderslock.unlock();

	if (g_Config.bEnableShaderDebugging)
		Clear();

	last_entry = NULL;
}

void VertexShaderCache::Clear()
{
	vshaderslock.lock();
	for (VSCache::iterator iter = vshaders.begin(); iter != vshaders.end(); ++iter)
		iter->second.Destroy();
	vshaders.clear();
	vshaderslock.unlock();

	last_entry = NULL;
}

void VertexShaderCache::Shutdown()
{
	if (Compiler)
	{
		Compiler->WaitForFinish();
	}
	for (int i = 0; i < MAX_SSAA_SHADERS; i++)
	{
		if (SimpleVertexShader[i])
			SimpleVertexShader[i]->Release();
		SimpleVertexShader[i] = NULL;
	}

	if (ClearVertexShader)
		ClearVertexShader->Release();
	ClearVertexShader = NULL;

	Clear();
	g_vs_disk_cache.Sync();
	g_vs_disk_cache.Close();
}

void VertexShaderCache::PrepareShader(u32 components, const XFMemory &xfr, const BPMemory &bpm, bool ongputhread)
{
	VertexShaderUid uid;
	GetVertexShaderUidD3D9(uid, components, xfr, bpm);
	if (ongputhread)
	{
		Compiler->ProcCompilationResults();
#if defined(_DEBUG) || defined(DEBUGFAST)
		if (g_ActiveConfig.bEnableShaderDebugging)
		{
			ShaderCode code;
			GenerateVertexShaderCodeD3D9(code, uid.GetUidData());
		}
#endif
		if (last_entry)
		{
			if (uid == last_uid)
			{
				return;
			}
		}
		last_uid = uid;
		GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
	}
	else
	{
		if (external_last_uid == uid)
		{
			return;
		}
		external_last_uid = uid;
	}
	vshaderslock.lock();
	VSCacheEntry *entry = &vshaders[uid];
	vshaderslock.unlock();
	if (ongputhread)
	{
		last_entry = entry;
	}
	// Compile only when we have a new instance
	if (entry->initialized.test_and_set())
	{
		return;
	}
	ShaderCompilerWorkUnit *wunit = Compiler->NewUnit(VERTEXSHADERGEN_BUFFERSIZE);
	wunit->GenerateCodeHandler = [uid](ShaderCompilerWorkUnit* wunit)
	{
		ShaderCode code;
		code.SetBuffer(wunit->code.data());
		GenerateVertexShaderCodeD3D9(code, uid.GetUidData());
		wunit->codesize = (u32)code.BufferSize();
	};
	wunit->entrypoint = "main";
	wunit->flags = D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_OPTIMIZATION_LEVEL3;
	wunit->target = D3D::VertexShaderVersionString();
	wunit->ResultHandler = [uid, entry](ShaderCompilerWorkUnit* wunit)
	{
		if (SUCCEEDED(wunit->cresult))
		{
			ID3DBlob* shaderBuffer = wunit->shaderbytecode;
			const u8* bytecode = (const u8*)shaderBuffer->GetBufferPointer();
			u32 bytecodelen = (u32)shaderBuffer->GetBufferSize();
			g_vs_disk_cache.Append(uid, bytecode, bytecodelen);
			PushByteCode(uid, bytecode, bytecodelen, entry);
#if defined(_DEBUG) || defined(DEBUGFAST)
			if (g_ActiveConfig.bEnableShaderDebugging)
			{
				entry->code = wunit->code.data();
			}
#endif
		}
		else
		{
			static int num_failures = 0;
			std::string filename = StringFromFormat("%sbad_vs_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
			std::ofstream file;
			OpenFStream(file, filename, std::ios_base::out);
			file << ((const char*)wunit->code.data());
			file << ((const char*)wunit->error->GetBufferPointer());
			file.close();

			PanicAlert("Failed to compile vertex shader!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%s):\n%s",
				filename,
				D3D::VertexShaderVersionString(),
				(char*)wunit->error->GetBufferPointer());
		}
	};
	Compiler->CompileShaderAsync(wunit);
}

bool VertexShaderCache::TestShader()
{
	int count = 0;
	while (!last_entry->compiled)
	{
		Compiler->ProcCompilationResults();
		if (g_ActiveConfig.bFullAsyncShaderCompilation)
		{
			break;
		}
		Common::cYield(count++);
	}
	if (last_entry->shader && last_entry->compiled)
	{
		D3D::SetVertexShader(last_entry->shader);
		return true;
	}
	return false;
}

void VertexShaderCache::PushByteCode(const VertexShaderUid &uid, const u8 *bytecode, int bytecodelen, VertexShaderCache::VSCacheEntry* entry)
{
	entry->shader = D3D::CreateVertexShaderFromByteCode(bytecode, bytecodelen);
	entry->compiled = true;
	if (entry->shader)
	{
		INCSTAT(stats.numVertexShadersCreated);
		SETSTAT(stats.numVertexShadersAlive, (int)vshaders.size());
	}
}

void VertexShaderCache::InsertByteCode(const VertexShaderUid &uid, const u8 *bytecode, int bytecodelen)
{
	VSCacheEntry *entry = &vshaders[uid];
	entry->initialized.test_and_set();
	PushByteCode(uid, bytecode, bytecodelen, entry);
}

}  // namespace DX9
