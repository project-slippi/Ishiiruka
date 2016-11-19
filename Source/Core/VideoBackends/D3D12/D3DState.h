// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <stack>
#include <unordered_map>

#include "Common/BitField.h"
#include "Common/CommonTypes.h"
#include "Common/Hash.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/NativeVertexFormat.h"
#include "VideoBackends/D3D12/ShaderCache.h"

#include "VideoCommon/BPMemory.h"

namespace DX12
{

class PipelineStateCacheInserter;

union RasterizerState
{
	BitField<0, 2, D3D12_CULL_MODE> cull_mode;

	u32 hex;
};

union BlendState
{
	BitField<0, 1, u32> blend_enable;
	BitField<1, 3, D3D12_BLEND_OP> blend_op;
	BitField<4, 4, u8> write_mask;
	BitField<8, 5, D3D12_BLEND> src_blend;
	BitField<13, 5, D3D12_BLEND> dst_blend;
	BitField<18, 1, u32> use_dst_alpha;
	BitField<19, 1, u32> logic_op_enabled;
	BitField<20, 4, D3D12_LOGIC_OP> logic_op;
	u32 hex;
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

	u32 hex;
};

union DepthState
{
	BitField<0, 1, u32> testenable;
	BitField<1, 4, u32> func;
	BitField<5, 1, u32> updateenable;
	BitField<6, 1, u32> reversed_depth;

	u32 packed;
};

struct SmallPsoDesc
{
	D3D12_SHADER_BYTECODE ds_bytecode;
	D3D12_SHADER_BYTECODE gs_bytecode;
	D3D12_SHADER_BYTECODE hs_bytecode;
	D3D12_SHADER_BYTECODE ps_bytecode;
	D3D12_SHADER_BYTECODE vs_bytecode;
	D3DVertexFormat* input_Layout;
	BlendState blend_state;
	RasterizerState rasterizer_state;
	DepthState depth_stencil_state;
	int sample_count;
};

// The Bitfield members in BlendState, RasterizerState, and ZMode cause the..
// static_assert(std::is_trivially_copyable<K>::value, "K must be a trivially copyable type");
// .. check in LinearDiskCache to fail. So, just storing the packed u32 values.

struct SmallPsoDiskDesc
{
	u32 blend_state_hex;
	u32 rasterizer_state_hex;
	u32 depth_stencil_state_hex;
	PixelShaderUid ps_uid;
	VertexShaderUid vs_uid;
	GeometryShaderUid gs_uid;
	TessellationShaderUid hds_uid;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE topology;
	PortableVertexDeclaration vertex_declaration; // Used to construct the input layout.
	DXGI_SAMPLE_DESC sample_desc;
};

class StateCache
{
public:
	// Get D3D12 descs for the internal state bitfields.
	static D3D12_SAMPLER_DESC GetDesc(SamplerState state);
	static D3D12_BLEND_DESC GetDesc(BlendState state);
	static D3D12_RASTERIZER_DESC GetDesc(RasterizerState state);
	static D3D12_DEPTH_STENCIL_DESC GetDesc(DepthState state);

	HRESULT GetPipelineStateObjectFromCache(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& pso_desc, ID3D12PipelineState** pso);
	HRESULT GetPipelineStateObjectFromCache(const SmallPsoDesc& pso_desc, ID3D12PipelineState** pso, D3D12_PRIMITIVE_TOPOLOGY_TYPE topology, const GeometryShaderUid* gs_uid, const PixelShaderUid* ps_uid, const VertexShaderUid* vs_uid, const TessellationShaderUid* hds_uid);

	StateCache();

	static void Init();

	static void CheckDiskCacheState(IDXGIAdapter* adapter);

	void GetPipelineState(D3D12_GRAPHICS_PIPELINE_STATE_DESC** pso_desc)
	{
		*pso_desc = &m_current_pso_desc;
	};

	// Release all cached states and clear hash tables.
	void Clear();

private:

	friend DX12::PipelineStateCacheInserter;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC m_current_pso_desc;

	struct hash_pso_desc
	{
		size_t operator()(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& pso_desc) const
		{
			return GetHash64(reinterpret_cast<const u8*>(&pso_desc), static_cast<u32>(sizeof(pso_desc)), 0);
		}
	};

	struct equality_pipeline_state_desc
	{
		bool operator()(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& lhs, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& rhs) const
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
				lhs.RTVFormats[0],
				lhs.SampleDesc.Count) ==
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
					rhs.RTVFormats[0],
					rhs.SampleDesc.Count);
		}
	};

	std::unordered_map<D3D12_GRAPHICS_PIPELINE_STATE_DESC, ComPtr<ID3D12PipelineState>, hash_pso_desc, equality_pipeline_state_desc> m_pso_map;

	struct hash_small_pso_desc
	{
		size_t operator()(const SmallPsoDesc& pso_desc) const
		{
			size_t h = -1;
			h = h * 137 + (uintptr_t)pso_desc.vs_bytecode.pShaderBytecode;
			h = h * 137 + (uintptr_t)pso_desc.ps_bytecode.pShaderBytecode;
			h = h * 137 + (uintptr_t)pso_desc.gs_bytecode.pShaderBytecode;
			h = h * 137 + (uintptr_t)pso_desc.hs_bytecode.pShaderBytecode;
			h = h * 137 + (uintptr_t)pso_desc.ds_bytecode.pShaderBytecode;
			h = h * 137 + (uintptr_t)pso_desc.input_Layout;
			h = h * 137 + (uintptr_t)(((uintptr_t)pso_desc.blend_state.hex << 32) |
				pso_desc.depth_stencil_state.packed | (uintptr_t(pso_desc.rasterizer_state.hex) << 17) | (((uintptr_t)pso_desc.sample_count) << 48));
			return h;
		}
	};

	struct equality_small_pipeline_state_desc
	{
		bool operator()(const SmallPsoDesc& lhs, const SmallPsoDesc& rhs) const
		{
			return std::tie(lhs.ps_bytecode.pShaderBytecode, lhs.vs_bytecode.pShaderBytecode, lhs.gs_bytecode.pShaderBytecode, lhs.hs_bytecode.pShaderBytecode, lhs.ds_bytecode.pShaderBytecode,
				lhs.input_Layout, lhs.blend_state.hex, lhs.depth_stencil_state.packed, lhs.rasterizer_state.hex, lhs.sample_count) ==
				std::tie(rhs.ps_bytecode.pShaderBytecode, rhs.vs_bytecode.pShaderBytecode, rhs.gs_bytecode.pShaderBytecode, rhs.hs_bytecode.pShaderBytecode, rhs.ds_bytecode.pShaderBytecode,
					rhs.input_Layout, rhs.blend_state.hex, rhs.depth_stencil_state.packed, rhs.rasterizer_state.hex, rhs.sample_count);
		}
	};

	struct hash_shader_bytecode
	{
		size_t operator()(const D3D12_SHADER_BYTECODE& shader) const
		{
			return (uintptr_t)shader.pShaderBytecode;
		}
	};

	struct equality_shader_bytecode
	{
		bool operator()(const D3D12_SHADER_BYTECODE& lhs, const D3D12_SHADER_BYTECODE& rhs) const
		{
			return lhs.pShaderBytecode == rhs.pShaderBytecode;
		}
	};

	std::unordered_map<SmallPsoDesc, ComPtr<ID3D12PipelineState>, hash_small_pso_desc, equality_small_pipeline_state_desc> m_small_pso_map;
	bool m_enable_disk_cache = true;
};

}  // namespace DX12
