// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <fstream>
#include <string>

#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DBlob.h"
#include "VideoBackends/D3D12/D3DShader.h"
#include "VideoCommon/HLSLCompiler.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{

namespace D3D
{



// code->bytecode
bool CompileShader(
	ShaderType type,
	const std::string& code,
	D3DBlob** blob,
	const D3D_SHADER_MACRO* pDefines,
	const char* pEntry, bool throwerror)
{
#if defined(_DEBUG) || defined(DEBUGFAST)
	UINT flags = D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY | D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT flags = D3DCOMPILE_SKIP_VALIDATION;
	if (type != DX12::D3D::ShaderType::Hull)
	{
		flags |= D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY | D3DCOMPILE_OPTIMIZATION_LEVEL3;
	}
	else
	{
		flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
	}
#endif

	std::string profile;
	const char* sufix = nullptr;
	switch (type)
	{
	case DX12::D3D::ShaderType::Vertex:
		profile = D3D::VertexShaderVersionString();
		sufix = "vs";
		break;
	case DX12::D3D::ShaderType::Pixel:
		profile = D3D::PixelShaderVersionString();
		sufix = "ps";
		break;
	case DX12::D3D::ShaderType::Geometry:
		profile = D3D::GeometryShaderVersionString();
		sufix = "gs";
		break;
	case DX12::D3D::ShaderType::Hull:
		profile = D3D::HullShaderVersionString();
		sufix = "hs";
		break;
	case DX12::D3D::ShaderType::Domain:
		profile = D3D::DomainShaderVersionString();
		sufix = "ds";
		break;
	case DX12::D3D::ShaderType::Compute:
		profile = D3D::ComputeShaderVersionString();
		sufix = "cs";
		break;
	default:
		return false;
		break;
	}

	ID3DBlob* shaderBuffer;
	ID3DBlob* errorBuffer;
	HRESULT hr = HLSLCompiler::getInstance().CompileShader(code.c_str(),
		code.length(), nullptr, pDefines, nullptr, pEntry != nullptr ? pEntry : "main", profile.c_str(),
		flags, 0, &shaderBuffer, &errorBuffer);

	if (errorBuffer)
	{
		INFO_LOG(VIDEO, "Shader compiler messages:\n%s",
			(const char*)errorBuffer->GetBufferPointer());
	}

	if (FAILED(hr))
	{
		static int num_failures = 0;
		std::string filename = StringFromFormat("%sbad_%s_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), sufix, num_failures++);
		std::ofstream file;
		OpenFStream(file, filename, std::ios_base::out);
		file << code;
		file << "\n";
		file << static_cast<const char*>(errorBuffer->GetBufferPointer());
		file.close();
		if (throwerror)
		{
			PanicAlert("Failed to compile shader: %s\nDebug info (%s):\n%s",
				filename.c_str(),
				profile,
				(char*)errorBuffer->GetBufferPointer());
		}
		*blob = nullptr;
		errorBuffer->Release();
	}
	else
	{
		*blob = new D3DBlob(shaderBuffer);
		shaderBuffer->Release();
	}

	return SUCCEEDED(hr);
}

bool CompileVertexShader(
	const std::string& code
	, D3DBlob** blob
	, const D3D_SHADER_MACRO* pDefines
	, const char* pEntry)
{
	return CompileShader(DX12::D3D::ShaderType::Vertex, code, blob, pDefines, pEntry);
}
bool CompileHullShader(
	const std::string& code
	, D3DBlob** blob
	, const D3D_SHADER_MACRO* pDefines
	, const char* pEntry)
{
	return CompileShader(DX12::D3D::ShaderType::Hull, code, blob, pDefines, pEntry);
}
bool CompileDomainShader(
	const std::string& code
	, D3DBlob** blob
	, const D3D_SHADER_MACRO* pDefines
	, const char* pEntry)
{
	return CompileShader(DX12::D3D::ShaderType::Domain, code, blob, pDefines, pEntry);
}
bool CompileGeometryShader(
	const std::string& code
	, D3DBlob** blob
	, const D3D_SHADER_MACRO* pDefines
	, const char* pEntry)
{
	return CompileShader(DX12::D3D::ShaderType::Geometry, code, blob, pDefines, pEntry);
}
bool CompilePixelShader(
	const std::string& code
	, D3DBlob** blob
	, const D3D_SHADER_MACRO* pDefines
	, const char* pEntry)
{
	return CompileShader(DX12::D3D::ShaderType::Pixel, code, blob, pDefines, pEntry);
}

}  // namespace

}  // namespace DX12
