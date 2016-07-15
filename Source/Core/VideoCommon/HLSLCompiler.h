// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Added for Ishiiruka By Tino
#pragma once

#include <vector>
#include <D3Dcompiler.h>
#include "VideoCommon/ShaderGenCommon.h"
#include "Common/ThreadPool.h"

class HLSLAsyncCompiler;

class ShaderCompilerWorkUnit
{
	friend class HLSLAsyncCompiler;
	ShaderCompilerWorkUnit();
	ShaderCompilerWorkUnit(ShaderCompilerWorkUnit const&);
	void operator=(ShaderCompilerWorkUnit const&);
public:
	u32 codesize;
	u32 flags;
	HRESULT cresult;
	const char* entrypoint;
	const char* target;
	const void* defines;
	ID3DBlob* shaderbytecode;
	ID3DBlob* error;
	std::vector<char> code;
	std::function<void(ShaderCompilerWorkUnit*)> GenerateCodeHandler;
	std::function<void(ShaderCompilerWorkUnit*)> ResultHandler;
	void Clear();
	void Release();
};

class HLSLAsyncCompiler final : Common::IWorker
{
	friend class HLSLCompiler;
	pD3DCompile PD3DCompile;
	HLSLAsyncCompiler();
	std::atomic<s32> m_repositoryIndex;
	ShaderCompilerWorkUnit* WorkUnitRepository;
	Common::ManyToManyQueue<ShaderCompilerWorkUnit*, Common::CircularQueue<ShaderCompilerWorkUnit*>> m_input;
	Common::ManyToOneQueue<ShaderCompilerWorkUnit*, Common::CircularQueue<ShaderCompilerWorkUnit*>> m_output;
	HLSLAsyncCompiler(HLSLAsyncCompiler const&);
	void operator=(HLSLAsyncCompiler const&);
public:
	static HLSLAsyncCompiler& getInstance();
	void SetCompilerFunction(pD3DCompile compilerfunc);
	virtual ~HLSLAsyncCompiler();
	bool NextTask() override;
	ShaderCompilerWorkUnit* NewUnit(u32 codesize);
	void CompileShaderAsync(ShaderCompilerWorkUnit* unit);
	void ProcCompilationResults();
	bool CompilationFinished();
	void WaitForCompilationFinished();
	void WaitForFinish();
};

class HLSLCompiler
{
	friend class HLSLAsyncCompiler;
	HLSLCompiler();
	typedef HRESULT(WINAPI *D3DREFLECT)(LPCVOID, SIZE_T, REFIID, void**);
	HINSTANCE hD3DCompilerDll;
	pD3DCompile PD3DCompile;
	D3DREFLECT PD3DReflect;
	s32 d3dcompiler_dll_ref;
	HRESULT LoadCompiler();
	void UnloadCompiler();
	HLSLCompiler(HLSLCompiler const&);
	void operator=(HLSLCompiler const&);
	HLSLAsyncCompiler m_AsyncCompiler;
public:
	static HLSLCompiler& getInstance();
	HRESULT CompileShader(LPCVOID pSrcData,
		SIZE_T SrcDataSize,
		LPCSTR pSourceName,
		const D3D_SHADER_MACRO *pDefines,
		ID3DInclude *pInclude,
		LPCSTR pEntrypoint,
		LPCSTR pTarget,
		UINT Flags1,
		UINT Flags2,
		ID3DBlob **ppCode,
		ID3DBlob **ppErrorMsgs
	);
	HRESULT Reflect(LPCVOID, SIZE_T, REFIID, void**);
	virtual ~HLSLCompiler();
};