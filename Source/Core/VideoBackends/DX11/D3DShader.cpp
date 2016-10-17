// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <fstream>
#include <string>

#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"

#include "VideoBackends/DX11/D3DBase.h"
#include "VideoBackends/DX11/D3DShader.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/HLSLCompiler.h"

namespace DX11
{

namespace D3D
{

VertexShaderPtr CreateVertexShaderFromByteCode(const void* bytecode, size_t len)
{
	VertexShaderPtr v_shader;
	HRESULT hr = D3D::device->CreateVertexShader(bytecode, len, nullptr, ToAddr(v_shader));
	if (FAILED(hr))
	{
		PanicAlert("CreateVertexShaderFromByteCode failed at %s %d\n", __FILE__, __LINE__);
	}
	return v_shader;
}

HullShaderPtr CreateHullShaderFromByteCode(const void* bytecode, size_t len)
{
	HullShaderPtr h_shader;
	HRESULT hr = D3D::device->CreateHullShader(bytecode, len, nullptr, ToAddr(h_shader));
	if (FAILED(hr))
	{
		PanicAlert("CreatHullShaderFromByteCode failed at %s %d\n", __FILE__, __LINE__);
	}
	return h_shader;
}

DomainShaderPtr CreateDomainShaderFromByteCode(const void* bytecode, size_t len)
{
	DomainShaderPtr d_shader;
	HRESULT hr = D3D::device->CreateDomainShader(bytecode, len, nullptr, ToAddr(d_shader));
	if (FAILED(hr))
	{
		PanicAlert("CreateDomainShaderFromByteCode failed at %s %d\n", __FILE__, __LINE__);
	}
	return d_shader;
}

GeometryShaderPtr CreateGeometryShaderFromByteCode(const void* bytecode, size_t len)
{
	GeometryShaderPtr g_shader;
	HRESULT hr = D3D::device->CreateGeometryShader(bytecode, len, nullptr, ToAddr(g_shader));
	if (FAILED(hr))
	{
		PanicAlert("CreateGeometryShaderFromByteCode failed at %s %d\n", __FILE__, __LINE__);
	}

	return g_shader;
}

PixelShaderPtr CreatePixelShaderFromByteCode(const void* bytecode, size_t len)
{
	PixelShaderPtr p_shader;
	HRESULT hr = D3D::device->CreatePixelShader(bytecode, len, nullptr, ToAddr(p_shader));
	if (FAILED(hr))
	{
		PanicAlert("CreatePixelShaderFromByteCode failed at %s %d\n", __FILE__, __LINE__);
	}
	return p_shader;
}

ID3D11PixelShader* CreatePixelShaderFromByteCodePtr(const void* bytecode, size_t len)
{
	ID3D11PixelShader* p_shader;
	HRESULT hr = D3D::device->CreatePixelShader(bytecode, len, nullptr, &p_shader);
	if (FAILED(hr))
	{
		PanicAlert("CreatePixelShaderFromByteCode failed at %s %d\n", __FILE__, __LINE__);
	}
	return p_shader;
}

ComputeShaderPtr CreateComputeShaderFromByteCode(const void* bytecode, size_t len)
{
	ComputeShaderPtr c_shader;
	HRESULT hr = D3D::device->CreateComputeShader(bytecode, len, nullptr, ToAddr(c_shader));
	if (FAILED(hr))
	{
		PanicAlert("CreateComputeShaderFromByteCode failed at %s %d\n", __FILE__, __LINE__);
	}
	return c_shader;
}

// code->bytecode
bool CompileShader(
	ShaderType type,
	const std::string& code,
	D3DBlob& blob,
	const D3D_SHADER_MACRO* pDefines,
	const char* pEntry, bool throwerror)
{
#if defined(_DEBUG) || defined(DEBUGFAST)
	UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY;
#else
	UINT flags = D3DCOMPILE_SKIP_VALIDATION;
	if (type != DX11::D3D::ShaderType::Hull)
	{
		flags |= D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY | D3DCOMPILE_OPTIMIZATION_LEVEL3;
	}
	else
	{
		flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
	}
#endif

	char const *profile = nullptr;
	char const *sufix = nullptr;
	switch (type)
	{
	case DX11::D3D::ShaderType::Vertex:
		profile = D3D::VertexShaderVersionString();
		sufix = "vs";
		break;
	case DX11::D3D::ShaderType::Pixel:
		profile = D3D::PixelShaderVersionString();
		sufix = "ps";
		break;
	case DX11::D3D::ShaderType::Geometry:
		profile = D3D::GeometryShaderVersionString();
		sufix = "gs";
		break;
	case DX11::D3D::ShaderType::Hull:
		profile = D3D::HullShaderVersionString();
		sufix = "hs";
		break;
	case DX11::D3D::ShaderType::Domain:
		profile = D3D::DomainShaderVersionString();
		sufix = "ds";
		break;
	case DX11::D3D::ShaderType::Compute:
		profile = D3D::ComputeShaderVersionString();
		sufix = "cs";
		break;
	default:
		return false;
		break;
	}



	ID3DBlobPtr shaderBuffer;
	ID3DBlobPtr errorBuffer;
	HRESULT hr = HLSLCompiler::getInstance().CompileShader(code.c_str(),
		code.length(), nullptr, pDefines, nullptr, pEntry != nullptr ? pEntry : "main", profile,
		flags, 0, ToAddr(shaderBuffer), ToAddr(errorBuffer));

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
		file << (const char*)errorBuffer->GetBufferPointer();
		file.close();
		if (throwerror)
		{
			PanicAlert("Failed to compile shader: %s\nDebug info (%s):\n%s",
				filename.c_str(),
				profile,
				(char*)errorBuffer->GetBufferPointer());
		}
		blob = nullptr;
	}
	else
	{
		blob = std::move(shaderBuffer);
	}

	return SUCCEEDED(hr);
}

VertexShaderPtr CompileAndCreateVertexShader(const std::string& code, const D3D_SHADER_MACRO* pDefines, const char* pEntry)
{
	D3DBlob blob;
	if (CompileShader(DX11::D3D::ShaderType::Vertex, code, blob, pDefines, pEntry))
	{
		return CreateVertexShaderFromByteCode(blob);
	}
	return nullptr;
}

HullShaderPtr CompileAndCreateHullShader(const std::string& code, const D3D_SHADER_MACRO* pDefines, const char* pEntry, bool throwError)
{
	D3DBlob blob;
	if (CompileShader(DX11::D3D::ShaderType::Hull, code, blob, pDefines, pEntry, throwError))
	{
		return CreateHullShaderFromByteCode(blob);
	}
	return nullptr;
}

DomainShaderPtr CompileAndCreateDomainShader(const std::string& code, const D3D_SHADER_MACRO* pDefines, const char* pEntry, bool throwError)
{
	D3DBlob blob;
	if (CompileShader(DX11::D3D::ShaderType::Domain, code, blob, pDefines, pEntry, throwError))
	{
		return CreateDomainShaderFromByteCode(blob);
	}
	return nullptr;
}

GeometryShaderPtr CompileAndCreateGeometryShader(const std::string& code, const D3D_SHADER_MACRO* pDefines, const char* pEntry)
{
	D3DBlob blob;
	if (CompileShader(DX11::D3D::ShaderType::Geometry, code, blob, pDefines, pEntry))
	{
		return CreateGeometryShaderFromByteCode(blob);
	}
	return nullptr;
}

PixelShaderPtr CompileAndCreatePixelShader(const std::string& code, const D3D_SHADER_MACRO* pDefines, const char* pEntry)
{
	D3DBlob blob;
	if (CompileShader(DX11::D3D::ShaderType::Pixel, code, blob, pDefines, pEntry))
	{
		return CreatePixelShaderFromByteCode(blob);
	}
	return nullptr;
}

ID3D11PixelShader* CompileAndCreatePixelShaderPtr(const std::string& code, const D3D_SHADER_MACRO* pDefines, const char* pEntry)
{
	D3DBlob blob;
	if (CompileShader(DX11::D3D::ShaderType::Pixel, code, blob, pDefines, pEntry))
	{
		return CreatePixelShaderFromByteCodePtr(blob.Data(), blob.Size());
	}
	return nullptr;
}

ComputeShaderPtr CompileAndCreateComputeShader(const std::string& code, const D3D_SHADER_MACRO* pDefines, const char* pEntry)
{
	D3DBlob blob;
	if (CompileShader(DX11::D3D::ShaderType::Compute, code, blob, pDefines, pEntry))
	{
		return CreateComputeShaderFromByteCode(blob);
	}
	return nullptr;
}

}  // namespace

}  // namespace DX11
