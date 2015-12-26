// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/FileUtil.h"
#include "Common/LinearDiskCache.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"

#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DShader.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/Globals.h"
#include "VideoBackends/D3D12/VertexShaderCache.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/HLSLCompiler.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VertexShaderManager.h"

namespace DX12 {

VertexShaderCache::VSCache VertexShaderCache::s_vshaders;
const VertexShaderCache::VSCacheEntry *VertexShaderCache::s_last_entry;
VertexShaderUid VertexShaderCache::s_last_uid;
VertexShaderUid VertexShaderCache::s_external_last_uid;
UidChecker<VertexShaderUid, ShaderCode> VertexShaderCache::s_vertex_uid_checker;

static HLSLAsyncCompiler *s_compiler;
static Common::SpinLock<true> s_vshaders_lock;

static D3DBlob* SimpleVertexShaderBlob = {};
static D3DBlob* ClearVertexShaderBlob = {};
static D3D12_INPUT_LAYOUT_DESC SimpleLayout12 = {};
static D3D12_INPUT_LAYOUT_DESC ClearLayout12 = {};

LinearDiskCache<VertexShaderUid, u8> g_vs_disk_cache;

D3D12_SHADER_BYTECODE VertexShaderCache::GetSimpleVertexShader12()
{
	D3D12_SHADER_BYTECODE shader = {};
	shader.BytecodeLength = SimpleVertexShaderBlob->Size();
	shader.pShaderBytecode = SimpleVertexShaderBlob->Data();

	return shader;
}
D3D12_SHADER_BYTECODE VertexShaderCache::GetClearVertexShader12() 
{
	D3D12_SHADER_BYTECODE shader = {};
	shader.BytecodeLength = ClearVertexShaderBlob->Size();
	shader.pShaderBytecode = ClearVertexShaderBlob->Data();

	return shader;
}
D3D12_INPUT_LAYOUT_DESC VertexShaderCache::GetSimpleInputLayout12() { return SimpleLayout12; }
D3D12_INPUT_LAYOUT_DESC VertexShaderCache::GetClearInputLayout12() { return ClearLayout12; }

ID3D12Resource* vscbuf12 = nullptr;
D3D12_GPU_VIRTUAL_ADDRESS vscbuf12GPUVA = {};

void* vscbuf12data = nullptr;
const UINT vscbuf12paddedSize = (sizeof(float) * VertexShaderManager::ConstantBufferSize + 0xff) & ~0xff;

#define vscbuf12Slots 10000
UINT currentVscbuf12 = 0; // 0 - vscbuf12Slots;

void VertexShaderCache::GetConstantBuffer12()
{
	if (VertexShaderManager::IsDirty())
	{
		currentVscbuf12 = (currentVscbuf12 + 1) % vscbuf12Slots;
		const size_t stride = currentVscbuf12 * vscbuf12paddedSize;
		const size_t size = sizeof(float) * VertexShaderManager::ConstantBufferSize;
		memcpy((u8*)vscbuf12data + stride, VertexShaderManager::GetBuffer(), size);

		VertexShaderManager::Clear();

		ADDSTAT(stats.thisFrame.bytesUniformStreamed, size);
		D3D::command_list_mgr->m_dirty_vs_cbv = true;
	}
	
	
	if (D3D::command_list_mgr->m_dirty_vs_cbv)
	{
		const size_t stride = currentVscbuf12 * vscbuf12paddedSize;
		D3D::current_command_list->SetGraphicsRootConstantBufferView(
			DESCRIPTOR_TABLE_VS_CBV,
			vscbuf12GPUVA + stride
			);

		if (g_ActiveConfig.bEnablePixelLighting)
			D3D::current_command_list->SetGraphicsRootConstantBufferView(
				DESCRIPTOR_TABLE_PS_CBVTWO,
				vscbuf12GPUVA + stride
				);

		D3D::command_list_mgr->m_dirty_vs_cbv = false;
	}
}

// this class will load the precompiled shaders into our cache
class VertexShaderCacheInserter : public LinearDiskCacheReader<VertexShaderUid, u8>
{
public:
	void Read(const VertexShaderUid &key, const u8* value, u32 value_size)
	{
		D3DBlob* blob = new D3DBlob(value_size, value);
		VertexShaderCache::InsertByteCode(key, blob);
		blob->Release();
	}
};

const char simple_shader_code[] = {
	"struct VSOUTPUT\n"
	"{\n"
	"float4 vPosition : SV_POSITION;\n"
	"float3 vTexCoord : TEXCOORD0;\n"
	"float  vTexCoord1 : TEXCOORD1;\n"
	"};\n"
	"VSOUTPUT main(float4 inPosition : SV_POSITION,float4 inTEX0 : TEXCOORD0)\n"
	"{\n"
	"VSOUTPUT OUT;\n"
	"OUT.vPosition = inPosition;\n"
	"OUT.vTexCoord = inTEX0.xyz;\n"
	"OUT.vTexCoord1 = inTEX0.w;\n"
	"return OUT;\n"
	"}\n"
};

const char clear_shader_code[] = {
	"struct VSOUTPUT\n"
	"{\n"
	"float4 vPosition   : SV_POSITION;\n"
	"float4 vColor0   : COLOR0;\n"
	"};\n"
	"VSOUTPUT main(float4 inPosition : SV_POSITION,float4 inColor0: COLOR0)\n"
	"{\n"
	"VSOUTPUT OUT;\n"
	"OUT.vPosition = inPosition;\n"
	"OUT.vColor0 = inColor0;\n"
	"return OUT;\n"
	"}\n"
};

void VertexShaderCache::Init()
{
	s_vshaders_lock.unlock();
	s_compiler = &HLSLAsyncCompiler::getInstance();

	const D3D12_INPUT_ELEMENT_DESC simpleelems[2] =
	{
		{ "SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

	};

	SimpleLayout12.NumElements = ARRAYSIZE(simpleelems);
	SimpleLayout12.pInputElementDescs = new D3D12_INPUT_ELEMENT_DESC[ARRAYSIZE(simpleelems)];
	memcpy((void*)SimpleLayout12.pInputElementDescs, simpleelems, sizeof(simpleelems));

	const D3D12_INPUT_ELEMENT_DESC clearelems[2] =
	{
		{ "SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	ClearLayout12.NumElements = ARRAYSIZE(clearelems);
	ClearLayout12.pInputElementDescs = new D3D12_INPUT_ELEMENT_DESC[ARRAYSIZE(clearelems)];
	memcpy((void*)ClearLayout12.pInputElementDescs, clearelems, sizeof(clearelems));

	unsigned int vscbuf12sizeInBytes = vscbuf12paddedSize * vscbuf12Slots;

	CheckHR(
		D3D::device12->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vscbuf12sizeInBytes),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&vscbuf12)
			)
		);

	D3D::SetDebugObjectName12(vscbuf12, "vertex shader constant buffer used to emulate the GX pipeline");

	// Obtain persistent CPU pointer to PS Constant Buffer
	CheckHR(vscbuf12->Map(0, nullptr, &vscbuf12data));

	// Obtain GPU VA for buffer, used at binding time.
	vscbuf12GPUVA = vscbuf12->GetGPUVirtualAddress();

	D3D::CompileVertexShader(simple_shader_code, &SimpleVertexShaderBlob);
	D3D::CompileVertexShader(clear_shader_code, &ClearVertexShaderBlob);

	Clear();

	if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

	SETSTAT(stats.numVertexShadersCreated, 0);
	SETSTAT(stats.numVertexShadersAlive, 0);

	// Intentionally share the same cache as DX11, as the shaders are identical. Reduces recompilation when switching APIs.
	std::string cache_filename = StringFromFormat("%sIDX11-%s-vs.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
		SConfig::GetInstance().m_strUniqueID.c_str());

	VertexShaderCacheInserter inserter;
	g_vs_disk_cache.OpenAndRead(cache_filename, inserter);

	if (g_Config.bEnableShaderDebugging)
		Clear();
	s_last_entry = nullptr;
	s_last_uid = {};
	VertexShaderManager::DisableDirtyRegions();
}

void VertexShaderCache::Clear()
{
	s_vshaders_lock.lock();
	for (auto& iter : s_vshaders)
		iter.second.Destroy();
	s_vshaders.clear();
	s_vshaders_lock.unlock();
	s_vertex_uid_checker.Invalidate();

	s_last_entry = nullptr;
}

void VertexShaderCache::Shutdown()
{
	if(s_compiler)
	{
		s_compiler->WaitForFinish();
	}
	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(vscbuf12);

	SAFE_RELEASE(SimpleVertexShaderBlob);
	SAFE_RELEASE(ClearVertexShaderBlob);
	SAFE_DELETE(SimpleLayout12.pInputElementDescs);
	SAFE_DELETE(ClearLayout12.pInputElementDescs);

	Clear();
	g_vs_disk_cache.Sync();
	g_vs_disk_cache.Close();
}

void VertexShaderCache::PrepareShader(
	u32 components,
	const XFMemory
	&xfr,
	const BPMemory &bpm,
	bool ongputhread)
{
	VertexShaderUid uid;
	GetVertexShaderUidD3D11(uid, components, xfr, bpm);
	if (ongputhread)
	{
		s_compiler->ProcCompilationResults();
		if (s_last_entry)
		{
			if (uid == s_last_uid)
			{
				return;
			}
		}
		s_last_uid = uid;
		D3D::command_list_mgr->m_dirty_pso = true;
#if defined(_DEBUG) || defined(DEBUGFAST)
		if (g_ActiveConfig.bEnableShaderDebugging)
		{
			ShaderCode code;
			GenerateVertexShaderCodeD3D11(code, components, xfr, bpm);
			s_vertex_uid_checker.AddToIndexAndCheck(code, uid, "Vertex", "v");
		}
#endif
		GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
	}
	else
	{
		if (s_external_last_uid == uid)
		{
			return;
		}
		s_external_last_uid = uid;
	}
	s_vshaders_lock.lock();
	VSCacheEntry* entry = &s_vshaders[uid];
	s_vshaders_lock.unlock();
	if (ongputhread)
	{
		s_last_entry = entry;
	}
	// Compile only when we have a new instance
	if (entry->initialized.test_and_set())
	{
		return;
	}
	ShaderCode code;
	ShaderCompilerWorkUnit *wunit = s_compiler->NewUnit(VERTEXSHADERGEN_BUFFERSIZE);
	code.SetBuffer(wunit->code.data());
	GenerateVertexShaderCodeD3D11(code, components, xfr, bpm);
	wunit->codesize = (u32)code.BufferSize();
	wunit->entrypoint = "main";
	wunit->flags = D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;
	wunit->target = D3D::VertexShaderVersionString();
	wunit->ResultHandler = [uid, entry](ShaderCompilerWorkUnit* wunit)
	{
		if (SUCCEEDED(wunit->cresult))
		{
			D3DBlob* shaderBuffer = new D3DBlob(wunit->shaderbytecode);
			const u8* bytecode = shaderBuffer->Data();
			u32 bytecodelen = (u32)shaderBuffer->Size();
			g_vs_disk_cache.Append(uid, bytecode, bytecodelen);
			PushByteCode(entry, shaderBuffer);
			shaderBuffer->Release();
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
			sprintf(szTemp, "%sbad_vs_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
			std::ofstream file;
			OpenFStream(file, szTemp, std::ios_base::out);
			file << ((const char*)wunit->code.data());
			file.close();

			PanicAlert("Failed to compile vertex shader!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%s):\n%s",
				szTemp,
				D3D::VertexShaderVersionString(),
				(char*)wunit->error->GetBufferPointer());
		}
	};
	s_compiler->CompileShaderAsync(wunit);
}

bool VertexShaderCache::TestShader()
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
	return s_last_entry->shader12.pShaderBytecode != nullptr && s_last_entry->compiled;
}

void VertexShaderCache::PushByteCode(VertexShaderCache::VSCacheEntry* entry, D3DBlob* bcodeblob)
{

	// In D3D12, shader bytecode is needed at Pipeline State creation time. The D3D11 path already kept shader bytecode around
	// for subsequent InputLayout creation, so just take advantage of that.

	entry->SetByteCode(bcodeblob);
	entry->shader12.BytecodeLength = bcodeblob->Size();
	entry->shader12.pShaderBytecode = bcodeblob->Data();
	entry->compiled = true;

	INCSTAT(stats.numVertexShadersCreated);
	SETSTAT(stats.numVertexShadersAlive, (int)s_vshaders.size());
}

bool VertexShaderCache::InsertByteCode(const VertexShaderUid &uid, D3DBlob* bcodeblob)
{
	// Make an entry in the table
	VSCacheEntry* entry = &s_vshaders[uid];

	// In D3D12, shader bytecode is needed at Pipeline State creation time. The D3D11 path already kept shader bytecode around
	// for subsequent InputLayout creation, so just take advantage of that.

	entry->SetByteCode(bcodeblob);
	entry->shader12.BytecodeLength = bcodeblob->Size();
	entry->shader12.pShaderBytecode = bcodeblob->Data();
	entry->initialized.test_and_set();
	entry->compiled = true;
	SETSTAT(stats.numVertexShadersAlive, (int)s_vshaders.size());
	return true;
}

}  // namespace DX12
