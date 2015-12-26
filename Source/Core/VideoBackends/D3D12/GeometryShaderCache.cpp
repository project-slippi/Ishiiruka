// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/FileUtil.h"
#include "Common/LinearDiskCache.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"

#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DShader.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/GeometryShaderCache.h"
#include "VideoBackends/D3D12/Globals.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/HLSLCompiler.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{

GeometryShaderCache::GSCache GeometryShaderCache::s_geometry_shaders;
const GeometryShaderCache::GSCacheEntry* GeometryShaderCache::s_last_entry;
GeometryShaderUid GeometryShaderCache::s_last_uid;
GeometryShaderUid GeometryShaderCache::s_external_last_uid;
UidChecker<GeometryShaderUid, ShaderCode> GeometryShaderCache::geometry_uid_checker;
GeometryShaderCache::GSCacheEntry GeometryShaderCache::s_pass_entry;
static HLSLAsyncCompiler *s_compiler;
static Common::SpinLock<true> s_geometry_shaders_lock;

D3D12_PRIMITIVE_TOPOLOGY_TYPE currentPrimitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

D3D12_PRIMITIVE_TOPOLOGY_TYPE GeometryShaderCache::GetCurrentPrimitiveTopology()
{
	return currentPrimitiveTopology;
}

D3DBlob* ClearGeometryShaderBlob = nullptr;
D3DBlob* CopyGeometryShaderBlob = nullptr;

LinearDiskCache<GeometryShaderUid, u8> g_gs_disk_cache;

D3D12_SHADER_BYTECODE GeometryShaderCache::GetClearGeometryShader12()
{
	D3D12_SHADER_BYTECODE bytecode = {};
	if (g_ActiveConfig.iStereoMode > 0)
	{
		bytecode.BytecodeLength = ClearGeometryShaderBlob->Size();
		bytecode.pShaderBytecode = ClearGeometryShaderBlob->Data();
	}

	return bytecode;
}

D3D12_SHADER_BYTECODE GeometryShaderCache::GetCopyGeometryShader12()
{
	D3D12_SHADER_BYTECODE bytecode = {};
	if (g_ActiveConfig.iStereoMode > 0)
	{
		bytecode.BytecodeLength = CopyGeometryShaderBlob->Size();
		bytecode.pShaderBytecode = CopyGeometryShaderBlob->Data();
	}

	return bytecode;
}

ID3D12Resource* gscbuf12 = nullptr;
D3D12_GPU_VIRTUAL_ADDRESS gscbuf12GPUVA = {};
void* gscbuf12data = nullptr;
static const UINT gscbuf12paddedSize = (sizeof(GeometryShaderConstants) + 0xff) & ~0xff;

#define gscbuf12Slots 5000
unsigned int currentGscbuf12 = 0; // 0 - gscbuf12Slots;

void GeometryShaderCache::GetConstantBuffer12()
{
	if (GeometryShaderManager::IsDirty())
	{
		currentGscbuf12 = (currentGscbuf12 + 1) % gscbuf12Slots;

		memcpy((u8*)gscbuf12data + gscbuf12paddedSize * currentGscbuf12, &GeometryShaderManager::constants, sizeof(GeometryShaderConstants));
		
		GeometryShaderManager::Clear();
		
		ADDSTAT(stats.thisFrame.bytesUniformStreamed, sizeof(GeometryShaderConstants));

		D3D::command_list_mgr->m_dirty_gs_cbv = true;
	}

	if (D3D::command_list_mgr->m_dirty_gs_cbv)
	{
		D3D::current_command_list->SetGraphicsRootConstantBufferView(
			DESCRIPTOR_TABLE_GS_CBV,
			gscbuf12GPUVA + gscbuf12paddedSize * currentGscbuf12
			);

		D3D::command_list_mgr->m_dirty_gs_cbv = false;
	}
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

const char clear_shader_code[] = {
	"struct VSOUTPUT\n"
	"{\n"
	"	float4 vPosition   : POSITION;\n"
	"	float4 vColor0   : COLOR0;\n"
	"};\n"
	"struct GSOUTPUT\n"
	"{\n"
	"	float4 vPosition   : POSITION;\n"
	"	float4 vColor0   : COLOR0;\n"
	"	uint slice    : SV_RenderTargetArrayIndex;\n"
	"};\n"
	"[maxvertexcount(6)]\n"
	"void main(triangle VSOUTPUT o[3], inout TriangleStream<GSOUTPUT> Output)\n"
	"{\n"
	"for(int slice = 0; slice < 2; slice++)\n"
	"{\n"
	"	for(int i = 0; i < 3; i++)\n"
	"	{\n"
	"		GSOUTPUT OUT;\n"
	"		OUT.vPosition = o[i].vPosition;\n"
	"		OUT.vColor0 = o[i].vColor0;\n"
	"		OUT.slice = slice;\n"
	"		Output.Append(OUT);\n"
	"	}\n"
	"	Output.RestartStrip();\n"
	"}\n"
	"}\n"
};

const char copy_shader_code[] = {
	"struct VSOUTPUT\n"
	"{\n"
	"	float4 vPosition : POSITION;\n"
	"	float3 vTexCoord : TEXCOORD0;\n"
	"	float  vTexCoord1 : TEXCOORD1;\n"
	"};\n"
	"struct GSOUTPUT\n"
	"{\n"
	"	float4 vPosition : POSITION;\n"
	"	float3 vTexCoord : TEXCOORD0;\n"
	"	float  vTexCoord1 : TEXCOORD1;\n"
	"	uint slice    : SV_RenderTargetArrayIndex;\n"
	"};\n"
	"[maxvertexcount(6)]\n"
	"void main(triangle VSOUTPUT o[3], inout TriangleStream<GSOUTPUT> Output)\n"
	"{\n"
	"for(int slice = 0; slice < 2; slice++)\n"
	"{\n"
	"	for(int i = 0; i < 3; i++)\n"
	"	{\n"
	"		GSOUTPUT OUT;\n"
	"		OUT.vPosition = o[i].vPosition;\n"
	"		OUT.vTexCoord = o[i].vTexCoord;\n"
	"		OUT.vTexCoord.z = slice;\n"
	"		OUT.vTexCoord1 = o[i].vTexCoord1;\n"
	"		OUT.slice = slice;\n"
	"		Output.Append(OUT);\n"
	"	}\n"
	"	Output.RestartStrip();\n"
	"}\n"
	"}\n"
};

void GeometryShaderCache::Init()
{
	s_pass_entry.compiled = true;
	s_pass_entry.initialized.test_and_set();
	s_compiler = &HLSLAsyncCompiler::getInstance();
	s_geometry_shaders_lock.unlock();
	unsigned int gscbuf12sizeInBytes = gscbuf12paddedSize * gscbuf12Slots;

	CheckHR(
		D3D::device12->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(gscbuf12sizeInBytes),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&gscbuf12)
		)
		);

	D3D::SetDebugObjectName12(gscbuf12, "vertex shader constant buffer used to emulate the GX pipeline");

	// Obtain persistent CPU pointer to GS Constant Buffer
	CheckHR(gscbuf12->Map(0, nullptr, &gscbuf12data));
	// Obtain GPU VA for buffer, used at binding time.
	gscbuf12GPUVA = gscbuf12->GetGPUVirtualAddress();

	// used when drawing clear quads
	D3D::CompileGeometryShader(clear_shader_code, &ClearGeometryShaderBlob);

	// used for buffer copy
	D3D::CompileGeometryShader(copy_shader_code, &CopyGeometryShaderBlob);

	Clear();

	if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

	// Intentionally share the same cache as DX11, as the shaders are identical. Reduces recompilation when switching APIs.
	std::string cache_filename = StringFromFormat("%sIDX11-%s-gs.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
		SConfig::GetInstance().m_strUniqueID.c_str());
	GeometryShaderCacheInserter inserter;
	g_gs_disk_cache.OpenAndRead(cache_filename, inserter);

	if (g_Config.bEnableShaderDebugging)
		Clear();

	s_last_entry = nullptr;
	s_last_uid = {};
}

// ONLY to be used during shutdown.
void GeometryShaderCache::Clear()
{
	for (auto& iter : s_geometry_shaders)
		iter.second.Destroy();
	s_geometry_shaders.clear();
	s_geometry_shaders_lock.unlock();
	geometry_uid_checker.Invalidate();

	s_last_entry = nullptr;
}

void GeometryShaderCache::Shutdown()
{
	if (s_compiler)
	{
		s_compiler->WaitForFinish();
	}
	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(gscbuf12);
	ClearGeometryShaderBlob->Release();
	CopyGeometryShaderBlob->Release();

	Clear();
	g_gs_disk_cache.Sync();
	g_gs_disk_cache.Close();
}

void GeometryShaderCache::PrepareShader(
	u32 primitive_type,
	const XFMemory &xfr,
	const u32 components,
	bool ongputhread)
{
	GeometryShaderUid uid;
	GetGeometryShaderUid(uid, primitive_type, API_D3D11, xfr, components);
	if (ongputhread)
	{
		switch (primitive_type)
		{
		case PRIMITIVE_TRIANGLES:
			currentPrimitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			break;
		case PRIMITIVE_LINES:
			currentPrimitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
			break;
		case PRIMITIVE_POINTS:
			currentPrimitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
			break;
		default:
			CHECK(0, "Invalid primitive type.");
			break;
		}
		s_compiler->ProcCompilationResults();
#if defined(_DEBUG) || defined(DEBUGFAST)
		if (g_ActiveConfig.bEnableShaderDebugging)
		{
			ShaderCode code;
			GenerateGeometryShaderCode(code, primitive_type, API_D3D11, xfr, components);
			geometry_uid_checker.AddToIndexAndCheck(code, uid, "Geometry", "g");
		}
#endif
		// Check if the shader is already set
		if (s_last_entry)
		{
			if (uid == s_last_uid)
			{
				return;
			}
		}
		s_last_uid = uid;
		D3D::command_list_mgr->m_dirty_pso = true;
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
	
	s_geometry_shaders_lock.lock();
	GSCacheEntry* entry = &s_geometry_shaders[uid];
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
	ShaderCode code;
	ShaderCompilerWorkUnit *wunit = s_compiler->NewUnit(GEOMETRYSHADERGEN_BUFFERSIZE);
	code.SetBuffer(wunit->code.data());
	GenerateGeometryShaderCode(code, primitive_type, API_D3D11, xfr, components);
	wunit->codesize = (u32)code.BufferSize();
	wunit->entrypoint = "main";
	wunit->flags = D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_OPTIMIZATION_LEVEL3;
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
			shaderBuffer->Release();
			wunit->shaderbytecode = nullptr;
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
	return s_last_entry->compiled;
}

void GeometryShaderCache::PushByteCode(const void* bytecode, unsigned int bytecodelen, GeometryShaderCache::GSCacheEntry* entry)
{
	// In D3D12, shader bytecode is needed at Pipeline State creation time, so make a copy (as LinearDiskCache frees original after load).
	entry->shader12.BytecodeLength = bytecodelen;
	entry->shader12.pShaderBytecode = new u8[bytecodelen];
	memcpy(const_cast<void*>(entry->shader12.pShaderBytecode), bytecode, bytecodelen);
	entry->compiled = true;
	INCSTAT(stats.numGeometryShadersCreated);
	SETSTAT(stats.numGeometryShadersAlive, s_geometry_shaders.size());
}

bool GeometryShaderCache::InsertByteCode(const GeometryShaderUid &uid, const void* bytecode, unsigned int bytecodelen)
{
	// Make an entry in the table
	GSCacheEntry* newentry = &s_geometry_shaders[uid];

	// In D3D12, shader bytecode is needed at Pipeline State creation time, so make a copy (as LinearDiskCache frees original after load).
	newentry->shader12.BytecodeLength = bytecodelen;
	newentry->shader12.pShaderBytecode = new u8[bytecodelen];
	memcpy(const_cast<void*>(newentry->shader12.pShaderBytecode), bytecode, bytecodelen);
	newentry->initialized.test_and_set();
	newentry->compiled = true;
	return true;
}

}  // DX12
