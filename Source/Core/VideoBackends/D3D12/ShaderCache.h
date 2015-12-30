// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"

namespace DX12
{

class D3DBlob;

enum SHADER_STAGE
{
	SHADER_STAGE_GEOMETRY_SHADER = 0,
	SHADER_STAGE_PIXEL_SHADER = 1,
	SHADER_STAGE_VERTEX_SHADER = 2,
	SHADER_STAGE_COUNT = 3
};

class ShaderCache final
{
public:
	static void Init();
	static void Clear();
	static void Shutdown();

	static void PrepareShaders(
		DSTALPHA_MODE ps_dst_alpha_mode,
		u32 gs_primitive_type,
		u32 components,
		const XFMemory &xfr,
		const BPMemory &bpm, bool on_gpu_thread);

	static bool TestShaders();

	static void InsertByteCode(const ShaderGeneratorInterface& uid, SHADER_STAGE stage, D3DBlob* bytecode);

	static D3D12_SHADER_BYTECODE GetActiveShaderBytecode(SHADER_STAGE stage);

	// The various uid flavors inherit from ShaderGeneratorInterface.
	static const ShaderGeneratorInterface* GetActiveShaderUid(SHADER_STAGE stage);
	static D3D12_SHADER_BYTECODE GetShaderFromUid(SHADER_STAGE stage, const ShaderGeneratorInterface* uid);

	static D3D12_PRIMITIVE_TOPOLOGY_TYPE GetCurrentPrimitiveTopology();

};

}
