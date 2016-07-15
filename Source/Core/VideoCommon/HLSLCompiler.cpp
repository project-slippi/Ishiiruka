// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Added for Ishiiruka By Tino

#include "Common/CPUDetect.h"
#include "VideoCommon/HLSLCompiler.h"


ShaderCompilerWorkUnit::ShaderCompilerWorkUnit() :
	flags(0),
	cresult(0),
	code(),
	defines(nullptr),
	entrypoint(nullptr),
	target(nullptr),
	shaderbytecode(nullptr),
	error(nullptr),
	ResultHandler()
{

}

void ShaderCompilerWorkUnit::Clear()
{
	GenerateCodeHandler = {};
	ResultHandler = {};
	cresult = 0;
	defines = nullptr;
	entrypoint = nullptr;
	target = nullptr;
	flags = 0;
	if (shaderbytecode)
	{
		shaderbytecode->Release();
		shaderbytecode = nullptr;
	}
	if (error)
	{
		error->Release();
		error = nullptr;
	}
}

void ShaderCompilerWorkUnit::Release()
{
	if (shaderbytecode)
	{
		shaderbytecode->Release();
		shaderbytecode = nullptr;
	}
	if (error)
	{
		error->Release();
		error = nullptr;
	}
}

HLSLAsyncCompiler::HLSLAsyncCompiler() :
	m_repositoryIndex(0),
	m_input(256),
	m_output(256)
{
	WorkUnitRepository = new ShaderCompilerWorkUnit[256];
	Common::ThreadPool::RegisterWorker(this);
}

void HLSLAsyncCompiler::SetCompilerFunction(pD3DCompile compilerfunc)
{
	PD3DCompile = compilerfunc;
}

HLSLAsyncCompiler::~HLSLAsyncCompiler()
{
	delete[] WorkUnitRepository;
	Common::ThreadPool::UnregisterWorker(this);
}

bool HLSLAsyncCompiler::NextTask()
{
	ShaderCompilerWorkUnit* unit;
	if (m_input.try_pop(unit))
	{
		if (unit->GenerateCodeHandler)
		{
			unit->GenerateCodeHandler(unit);
		}
		unit->cresult = PD3DCompile(unit->code.data(),
			unit->codesize,
			nullptr,
			(const D3D_SHADER_MACRO*)unit->defines,
			nullptr,
			unit->entrypoint,
			unit->target,
			unit->flags, 0,
			&unit->shaderbytecode,
			&unit->error);
		m_output.push(unit);
		return true;
	}
	return false;
}
ShaderCompilerWorkUnit* HLSLAsyncCompiler::NewUnit(u32 codesize)
{
	Common::ThreadPool::NotifyWorkPending();
	u32 index = m_repositoryIndex.fetch_add(1);
	ShaderCompilerWorkUnit* result = &WorkUnitRepository[index & 255];
	result->Clear();
	if (result->code.size() < codesize)
	{
		result->code.resize(codesize);
	}
	result->codesize = codesize;
	return result;
}
void HLSLAsyncCompiler::CompileShaderAsync(ShaderCompilerWorkUnit* unit)
{
	m_input.push(unit);
}
void HLSLAsyncCompiler::ProcCompilationResults()
{
	if (!m_output.Empty())
	{
		ShaderCompilerWorkUnit* unit;
		while (m_output.try_pop(unit))
		{
			unit->ResultHandler(unit);
		}
	}
}
bool HLSLAsyncCompiler::CompilationFinished()
{
	return m_input.Empty();
}
void HLSLAsyncCompiler::WaitForCompilationFinished()
{
	u32 loopcount = 0;
	while (!m_input.Empty())
	{
		Common::cYield(loopcount++);
	}
}
void HLSLAsyncCompiler::WaitForFinish()
{
	ShaderCompilerWorkUnit* unit;
	WaitForCompilationFinished();
	if (!m_output.Empty())
	{
		while (m_output.try_pop(unit))
		{
			unit->ResultHandler(unit);
		}
	}
}



HLSLCompiler& HLSLCompiler::getInstance()
{
	static HLSLCompiler instance;
	// Instantiated on first use.
	return instance;
}

HLSLAsyncCompiler& HLSLAsyncCompiler::getInstance()
{
	return HLSLCompiler::getInstance().m_AsyncCompiler;
}

HLSLCompiler::HLSLCompiler() :
	hD3DCompilerDll(nullptr),
	PD3DCompile(nullptr),
	PD3DReflect(nullptr),
	d3dcompiler_dll_ref(0),
	m_AsyncCompiler()
{
	LoadCompiler();
	m_AsyncCompiler.SetCompilerFunction(PD3DCompile);
}

HLSLCompiler::~HLSLCompiler()
{
	UnloadCompiler();
}

HRESULT HLSLCompiler::CompileShader(LPCVOID pSrcData,
	SIZE_T SrcDataSize,
	LPCSTR pSourceName,
	const D3D_SHADER_MACRO *pDefines,
	ID3DInclude *pInclude,
	LPCSTR pEntrypoint,
	LPCSTR pTarget,
	UINT Flags1,
	UINT Flags2,
	ID3DBlob **ppCode,
	ID3DBlob **ppErrorMsgs)
{
	return PD3DCompile(pSrcData,
		SrcDataSize,
		pSourceName,
		pDefines,
		pInclude,
		pEntrypoint,
		pTarget,
		Flags1, Flags2,
		ppCode,
		ppErrorMsgs);
}

HRESULT HLSLCompiler::Reflect(
	LPCVOID bytecode,
	SIZE_T bytecodesize,
	REFIID interfaceid,
	void** interfaceobject)
{
	return PD3DReflect(bytecode, bytecodesize, interfaceid, interfaceobject);
}

HRESULT HLSLCompiler::LoadCompiler()
{
	if (d3dcompiler_dll_ref++ > 0) return S_OK;
	if (hD3DCompilerDll) return S_OK;

	// try to load D3DCompiler
	HRESULT hr = E_FAIL;
	for (unsigned int num = 49; num >= 42; --num)
	{
		std::string compilerfile = StringFromFormat("D3DCompiler_%d.dll", num);
		hD3DCompilerDll = LoadLibraryA(compilerfile.c_str());
		if (hD3DCompilerDll != nullptr)
		{
			NOTICE_LOG(VIDEO,
				"Successfully loaded %s.",
				compilerfile.c_str());
			hr = S_OK;
			break;
		}
	}
	if (FAILED(hr))
	{
		MessageBoxA(
			nullptr,
			"Failed to load any D3DCompiler dll",
			"Critical error",
			MB_OK | MB_ICONERROR);
		return hr;
	}
	PD3DCompile = (pD3DCompile)GetProcAddress(hD3DCompilerDll, "D3DCompile");
	if (PD3DCompile == nullptr)
		MessageBoxA(nullptr,
			"GetProcAddress failed for D3DCompile!",
			"Critical error",
			MB_OK | MB_ICONERROR);
	PD3DReflect = (D3DREFLECT)GetProcAddress(hD3DCompilerDll, "D3DReflect");
	if (PD3DReflect == NULL)
		MessageBoxA(NULL,
			"GetProcAddress failed for D3DReflect!",
			"Critical error", MB_OK | MB_ICONERROR);
	return S_OK;
}

void HLSLCompiler::UnloadCompiler()
{
	if (!d3dcompiler_dll_ref) return;
	if (--d3dcompiler_dll_ref != 0) return;

	if (hD3DCompilerDll) FreeLibrary(hD3DCompilerDll);
	hD3DCompilerDll = nullptr;
	PD3DCompile = nullptr;
}
