// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/TessellationShaderGen.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"

namespace DX12
{

class D3DBlob;

class ShaderCache final
{
public:
	static void Init();
	static void Clear();
	static void Shutdown();

	static void PrepareShaders(
		PIXEL_SHADER_RENDER_MODE render_mode,
		u32 gs_primitive_type,
		u32 components,
		const XFMemory &xfr,
		const BPMemory &bpm, bool on_gpu_thread);

	static bool TestShaders();

	template<class UidType, class ShaderCacheType>
	static void InsertByteCode(const UidType& uid, ShaderCacheType* shader_cache, D3DBlob* bytecode_blob);

	static void InsertDSByteCode(const TessellationShaderUid& uid, D3DBlob* bytecode_blob);

	static void InsertHSByteCode(const TessellationShaderUid& uid, D3DBlob* bytecode_blob);


	static D3D12_SHADER_BYTECODE GetActiveDomainShaderBytecode();
	static D3D12_SHADER_BYTECODE GetActiveGeometryShaderBytecode();
	static D3D12_SHADER_BYTECODE GetActiveHullShaderBytecode();
	static D3D12_SHADER_BYTECODE GetActivePixelShaderBytecode();
	static D3D12_SHADER_BYTECODE GetActiveVertexShaderBytecode();

	// The various uid flavors inherit from ShaderGeneratorInterface.
	static const GeometryShaderUid* GetActiveGeometryShaderUid();
	static const PixelShaderUid*    GetActivePixelShaderUid();
	static const VertexShaderUid*   GetActiveVertexShaderUid();
	static const TessellationShaderUid* GetActiveTessellationShaderUid();

	static D3D12_SHADER_BYTECODE GetDomainShaderFromUid(const TessellationShaderUid* uid);
	static D3D12_SHADER_BYTECODE GetGeometryShaderFromUid(const GeometryShaderUid* uid);
	static D3D12_SHADER_BYTECODE GetHullShaderFromUid(const TessellationShaderUid* uid);
	static D3D12_SHADER_BYTECODE GetPixelShaderFromUid(const PixelShaderUid* uid);
	static D3D12_SHADER_BYTECODE GetVertexShaderFromUid(const VertexShaderUid* uid);


	static D3D12_PRIMITIVE_TOPOLOGY_TYPE GetCurrentPrimitiveTopology();
private:
	static void SetCurrentPrimitiveTopology(u32 gs_primitive_type);

	static void HandleGSUIDChange(
		const GeometryShaderUid& gs_uid,
		bool on_gpu_thread);

	static void HandlePSUIDChange(
		const PixelShaderUid& ps_uid,
		bool on_gpu_thread
	);

	static void HandleVSUIDChange(
		const VertexShaderUid& vs_uid,
		bool on_gpu_thread);

	static void HandleTSUIDChange(
		const TessellationShaderUid& ts_uid,
		bool on_gpu_thread);
};

}
