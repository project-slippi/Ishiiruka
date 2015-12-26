// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <stack>
#include <unordered_map>

#include "Common/BitField.h"
#include "Common/CommonTypes.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/GeometryShaderCache.h"
#include "VideoBackends/D3D12/NativeVertexFormat.h"
#include "VideoBackends/D3D12/PixelShaderCache.h"
#include "VideoBackends/D3D12/VertexShaderCache.h"
#include "VideoCommon/BPMemory.h"

namespace DX12
{

union RasterizerState
{
	BitField<0, 2, D3D12_CULL_MODE> cull_mode;

	u32 packed;
};

union BlendState
{
	BitField<0, 1, u32> blend_enable;
	BitField<1, 3, D3D12_BLEND_OP> blend_op;
	BitField<4, 4, u8> write_mask;
	BitField<8, 5, D3D12_BLEND> src_blend;
	BitField<13, 5, D3D12_BLEND> dst_blend;
	BitField<18, 1, u32> use_dst_alpha;

	u32 packed;
};

union SamplerState
{
	BitField<0, 3, u32> min_filter;
	BitField<3, 1, u32> mag_filter;
	BitField<4, 8, u32> min_lod;
	BitField<12, 8, u32> max_lod;
	BitField<20, 8, s32> lod_bias;
	BitField<28, 2, u32> wrap_s;
	BitField<30, 2, u32> wrap_t;

	u32 packed;
};

struct SmallPsoDesc
{
	D3D12_SHADER_BYTECODE VS;
	D3D12_SHADER_BYTECODE PS;
	D3D12_SHADER_BYTECODE GS;
	D3DVertexFormat* InputLayout;
	BlendState BlendState;
	RasterizerState RasterizerState;
	ZMode DepthStencilState;
};

struct SmallPsoDiskDesc
{
	BlendState BlendState;
	RasterizerState RasterizerState;
	ZMode DepthStencilState;
	PixelShaderUid psUid;
	VertexShaderUid vsUid;
	GeometryShaderUid gsUid;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE topology;
	PortableVertexDeclaration vertexDeclaration; // Used to construct the input layout.
};

class PipelineStateCacheInserter;

class StateCache
{
public:
	// Get D3D12 descs for the internal state bitfields.
	static D3D12_SAMPLER_DESC GetDesc12(SamplerState state);
	static D3D12_BLEND_DESC GetDesc12(BlendState state);
	static D3D12_RASTERIZER_DESC GetDesc12(RasterizerState state);
	static D3D12_DEPTH_STENCIL_DESC GetDesc12(ZMode state);

	HRESULT GetPipelineStateObjectFromCache(D3D12_GRAPHICS_PIPELINE_STATE_DESC* pso_desc, ID3D12PipelineState** pso);
	HRESULT GetPipelineStateObjectFromCache(SmallPsoDesc* pso_desc, ID3D12PipelineState** pso, D3D12_PRIMITIVE_TOPOLOGY_TYPE topology, PixelShaderUid ps_uid, VertexShaderUid vs_uid, GeometryShaderUid gs_uid);

	StateCache();

	static void Init();

	void GetPipelineState(D3D12_GRAPHICS_PIPELINE_STATE_DESC** pso_desc) {
		*pso_desc = &m_current_pso_desc;
	};

	// Release all cached states and clear hash tables.
	void Clear();

private:

	friend DX12::PipelineStateCacheInserter;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC m_current_pso_desc;

	struct hash_pso_desc
	{
		size_t operator()(const D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc) const
		{
			return ((uintptr_t)pso_desc.PS.pShaderBytecode * 1000000) ^ ((uintptr_t)pso_desc.VS.pShaderBytecode * 1000) ^ ((uintptr_t)pso_desc.InputLayout.pInputElementDescs);
		}
	};

	struct equality_pipeline_state_desc
	{
		bool operator()(const D3D12_GRAPHICS_PIPELINE_STATE_DESC lhs, const D3D12_GRAPHICS_PIPELINE_STATE_DESC rhs) const
		{
			return std::tie(lhs.PS.pShaderBytecode, lhs.VS.pShaderBytecode, lhs.GS.pShaderBytecode,
				            lhs.RasterizerState.CullMode,
				            lhs.DepthStencilState.DepthEnable,
				            lhs.DepthStencilState.DepthFunc,
				            lhs.DepthStencilState.DepthWriteMask,
				            lhs.BlendState.RenderTarget[0].BlendEnable,
				            lhs.BlendState.RenderTarget[0].BlendOp,
				            lhs.BlendState.RenderTarget[0].DestBlend,
				            lhs.BlendState.RenderTarget[0].SrcBlend,
				            lhs.BlendState.RenderTarget[0].RenderTargetWriteMask,
				            lhs.RTVFormats[0]) ==
				   std::tie(rhs.PS.pShaderBytecode, rhs.VS.pShaderBytecode, rhs.GS.pShaderBytecode,
				            rhs.RasterizerState.CullMode,
				            rhs.DepthStencilState.DepthEnable,
				            rhs.DepthStencilState.DepthFunc,
				            rhs.DepthStencilState.DepthWriteMask,
				            rhs.BlendState.RenderTarget[0].BlendEnable,
				            rhs.BlendState.RenderTarget[0].BlendOp,
				            rhs.BlendState.RenderTarget[0].DestBlend,
				            rhs.BlendState.RenderTarget[0].SrcBlend,
				            rhs.BlendState.RenderTarget[0].RenderTargetWriteMask,
				            rhs.RTVFormats[0]);
		}
	};

	std::unordered_map<D3D12_GRAPHICS_PIPELINE_STATE_DESC, ID3D12PipelineState*, hash_pso_desc, equality_pipeline_state_desc> m_pso_map;

	struct hash_small_pso_desc
	{
		size_t operator()(const SmallPsoDesc pso_desc) const
		{
			return ((uintptr_t)pso_desc.VS.pShaderBytecode << 10) ^
				((uintptr_t)pso_desc.PS.pShaderBytecode) +
				pso_desc.BlendState.packed +
				pso_desc.DepthStencilState.hex;
		}
	};

	struct equality_small_pipeline_state_desc
	{
		bool operator()(const SmallPsoDesc lhs, const SmallPsoDesc rhs) const
		{
			return std::tie(lhs.PS.pShaderBytecode, lhs.VS.pShaderBytecode, lhs.GS.pShaderBytecode,
				            lhs.InputLayout, lhs.BlendState.packed, lhs.DepthStencilState.hex, lhs.RasterizerState.packed) ==
				   std::tie(rhs.PS.pShaderBytecode, rhs.VS.pShaderBytecode, rhs.GS.pShaderBytecode,
				            rhs.InputLayout, rhs.BlendState.packed, rhs.DepthStencilState.hex, rhs.RasterizerState.packed);
		}
	};

	struct hash_shader_bytecode
	{
		size_t operator()(const D3D12_SHADER_BYTECODE shader) const
		{
			return (uintptr_t)shader.pShaderBytecode;
		}
	};

	struct equality_shader_bytecode
	{
		bool operator()(const D3D12_SHADER_BYTECODE lhs, const D3D12_SHADER_BYTECODE rhs) const
		{
			return lhs.pShaderBytecode == rhs.pShaderBytecode;
		}
	};

	std::unordered_map<SmallPsoDesc, ID3D12PipelineState*, hash_small_pso_desc, equality_small_pipeline_state_desc> m_small_pso_map;
};

}  // namespace DX12
