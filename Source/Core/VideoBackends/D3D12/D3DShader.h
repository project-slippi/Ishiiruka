// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "VideoBackends/D3D12/D3DBase.h"

class D3DBlob;

namespace DX12
{

namespace D3D
{
enum class ShaderType : u32
{
	Vertex,
	Pixel,
	Geometry,
	Hull,
	Domain,
	Compute
};
bool CompileShader(
	ShaderType type,
	const std::string& code,
	D3DBlob** blob,
	const D3D_SHADER_MACRO* pDefines = nullptr,
	const char* pEntry = nullptr, bool throwError = true);

// The returned bytecode buffers should be Released().
bool CompileVertexShader(
	const std::string& code
	, D3DBlob** blob
	, const D3D_SHADER_MACRO* pDefines = nullptr
	, const char* pEntry = nullptr);
bool CompileHullShader(
	const std::string& code
	, D3DBlob** blob
	, const D3D_SHADER_MACRO* pDefines = nullptr
	, const char* pEntry = nullptr);
bool CompileDomainShader(
	const std::string& code
	, D3DBlob** blob
	, const D3D_SHADER_MACRO* pDefines = nullptr
	, const char* pEntry = nullptr);
bool CompileGeometryShader(
	const std::string& code
	, D3DBlob** blob
	, const D3D_SHADER_MACRO* pDefines = nullptr
	, const char* pEntry = nullptr);
bool CompilePixelShader(
	const std::string& code
	, D3DBlob** blob
	, const D3D_SHADER_MACRO* pDefines = nullptr
	, const char* pEntry = nullptr);
}

}  // namespace DX12
