// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/LinearDiskCache.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"

#include "Core/ConfigManager.h"

#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/NativeVertexFormat.h"
#include "VideoBackends/D3D12/ShaderCache.h"
#include "VideoBackends/D3D12/StaticShaderCache.h"

#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/SamplerCommon.h"

namespace DX12
{

static bool s_cache_is_corrupted = false;
LinearDiskCache<SmallPsoDiskDesc, u8> s_pso_disk_cache;

class PipelineStateCacheInserter : public LinearDiskCacheReader<SmallPsoDiskDesc, u8>
{
public:
	void Read(const SmallPsoDiskDesc &key, const u8* value, u32 value_size)
	{
		if (s_cache_is_corrupted)
			return;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.GS = ShaderCache::GetGeometryShaderFromUid(&key.gs_uid);
		desc.PS = ShaderCache::GetPixelShaderFromUid(&key.ps_uid);
		desc.VS = ShaderCache::GetVertexShaderFromUid(&key.vs_uid);
		desc.HS = ShaderCache::GetHullShaderFromUid(&key.hds_uid);
		desc.DS = ShaderCache::GetDomainShaderFromUid(&key.hds_uid);
		D3D::SetRootSignature(desc.GS.pShaderBytecode != nullptr, desc.HS.pShaderBytecode != nullptr, false);
		desc.pRootSignature = D3D::GetRootSignature();
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // This state changes in PSTextureEncoder::Encode.
		desc.DSVFormat = DXGI_FORMAT_D32_FLOAT; // This state changes in PSTextureEncoder::Encode.
		desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF;
		desc.NumRenderTargets = 1;
		desc.SampleMask = UINT_MAX;
		desc.SampleDesc = key.sample_desc;



		if (!desc.PS.pShaderBytecode || !desc.VS.pShaderBytecode)
		{
			s_cache_is_corrupted = true;
			return;
		}

		BlendState blend_state = {};
		blend_state.hex = key.blend_state_hex;
		desc.BlendState = StateCache::GetDesc(blend_state);

		DepthState depth_stencil_state = {};
		depth_stencil_state.packed = key.depth_stencil_state_hex;
		desc.DepthStencilState = StateCache::GetDesc(depth_stencil_state);

		RasterizerState rasterizer_state = {};
		rasterizer_state.hex = key.rasterizer_state_hex;
		desc.RasterizerState = StateCache::GetDesc(rasterizer_state);

		desc.PrimitiveTopologyType = key.topology;

		// search for a cached native vertex format
		const PortableVertexDeclaration& native_vtx_decl = key.vertex_declaration;
		std::unique_ptr<NativeVertexFormat>& native = (*VertexLoaderManager::GetNativeVertexFormatMap())[native_vtx_decl];

		if (!native)
		{
			native = g_vertex_manager->CreateNativeVertexFormat(native_vtx_decl);
		}

		desc.InputLayout = reinterpret_cast<D3DVertexFormat*>(native.get())->GetActiveInputLayout();

		desc.CachedPSO.CachedBlobSizeInBytes = value_size;
		desc.CachedPSO.pCachedBlob = value;

		ComPtr<ID3D12PipelineState> pso;
		HRESULT hr = D3D::device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(pso.ReleaseAndGetAddressOf()));

		if (FAILED(hr))
		{
			// Failure can occur if disk cache is corrupted, or a driver upgrade invalidates the existing blobs.
			// In this case, we need to clear the disk cache.
			s_cache_is_corrupted = true;
			return;
		}

		SmallPsoDesc small_desc = {};
		small_desc.blend_state.hex = key.blend_state_hex;
		small_desc.depth_stencil_state.packed = key.depth_stencil_state_hex;
		small_desc.rasterizer_state.hex = key.rasterizer_state_hex;
		small_desc.gs_bytecode = desc.GS;
		small_desc.vs_bytecode = desc.VS;
		small_desc.ps_bytecode = desc.PS;
		small_desc.hs_bytecode = desc.HS;
		small_desc.ds_bytecode = desc.DS;
		small_desc.input_Layout = reinterpret_cast<D3DVertexFormat*>(native.get());
		small_desc.sample_count = key.sample_desc.Count;
		gx_state_cache.m_small_pso_map[small_desc] = pso;
	}
};

StateCache::StateCache()
{
	m_current_pso_desc = {};

	m_current_pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // This state changes in PSTextureEncoder::Encode.
	m_current_pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT; // This state changes in PSTextureEncoder::Encode.
	m_current_pso_desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF;
	m_current_pso_desc.NumRenderTargets = 1;
	m_current_pso_desc.SampleMask = UINT_MAX;
	m_current_pso_desc.SampleDesc.Count = g_ActiveConfig.iMultisamples;
	m_current_pso_desc.SampleDesc.Quality = 0;
	m_enable_disk_cache = true;
}

void StateCache::Init()
{
	if (!gx_state_cache.m_enable_disk_cache)
	{
		return;
	}

	if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

	std::string cache_filename = StringFromFormat("%sIdx12-%s-pso.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
		SConfig::GetInstance().GetGameID().c_str());

	PipelineStateCacheInserter inserter;
	s_pso_disk_cache.OpenAndRead(cache_filename, inserter);

	if (s_cache_is_corrupted)
	{
		// If a PSO fails to create, that means either:
		// - The file itself is corrupt.
		// - A driver/HW change has occured, causing the existing cache blobs to be invalid.
		// 
		// In either case, we want to re-create the disk cache. This should not be a frequent occurence.

		s_pso_disk_cache.Close();

		gx_state_cache.m_small_pso_map.clear();

		File::Delete(cache_filename);

		s_pso_disk_cache.OpenAndRead(cache_filename, inserter);

		s_cache_is_corrupted = false;
	}
}

void StateCache::CheckDiskCacheState(IDXGIAdapter* adapter)
{
	DXGI_ADAPTER_DESC adapter_desc = {};
	adapter->GetDesc(&adapter_desc);
	gx_state_cache.m_enable_disk_cache = true;

	// Disable disk cache for drivers that have issues when recreating
	// identical PSOs from the cache blob.
	if (adapter_desc.VendorId == 0x1002)        // Microsoft WARP
	{
		gx_state_cache.m_enable_disk_cache = false;
	}
}

D3D12_SAMPLER_DESC StateCache::GetDesc(SamplerState state)
{
	const unsigned int d3d_mip_filters[4] =
	{
		TexMode0::TEXF_NONE,
		TexMode0::TEXF_POINT,
		TexMode0::TEXF_LINEAR,
		TexMode0::TEXF_NONE, //reserved
	};
	const D3D12_TEXTURE_ADDRESS_MODE d3d_clamps[4] =
	{
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP //reserved
	};

	D3D12_SAMPLER_DESC sampdc;

	unsigned int mip = d3d_mip_filters[state.min_filter & 3];

	sampdc.MaxAnisotropy = 1;
	// Only use anisotropic filtering if one or both of the filters are set to Linear.
	// If both filters are set to Point then using anisotropy is equivalent
	// to "forced filtering" which will cause visual glitches.
	if (g_ActiveConfig.iMaxAnisotropy > 1 && !SamplerCommon::IsBpTexMode0PointFilteringEnabled(state))
	{
		sampdc.Filter = D3D12_FILTER_ANISOTROPIC;
		sampdc.MaxAnisotropy = 1 << g_ActiveConfig.iMaxAnisotropy;
	}
	else if (state.min_filter & 4) // linear min filter
	{
		if (state.mag_filter) // linear mag filter
		{
			if (mip == TexMode0::TEXF_NONE)
				sampdc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			else if (mip == TexMode0::TEXF_POINT)
				sampdc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			else if (mip == TexMode0::TEXF_LINEAR)
				sampdc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		}
		else // point mag filter
		{
			if (mip == TexMode0::TEXF_NONE)
				sampdc.Filter = D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
			else if (mip == TexMode0::TEXF_POINT)
				sampdc.Filter = D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
			else if (mip == TexMode0::TEXF_LINEAR)
				sampdc.Filter = D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
		}
	}
	else // point min filter
	{
		if (state.mag_filter) // linear mag filter
		{
			if (mip == TexMode0::TEXF_NONE)
				sampdc.Filter = D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
			else if (mip == TexMode0::TEXF_POINT)
				sampdc.Filter = D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
			else if (mip == TexMode0::TEXF_LINEAR)
				sampdc.Filter = D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
		}
		else // point mag filter
		{
			if (mip == TexMode0::TEXF_NONE)
				sampdc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			else if (mip == TexMode0::TEXF_POINT)
				sampdc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			else if (mip == TexMode0::TEXF_LINEAR)
				sampdc.Filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
		}
	}

	sampdc.AddressU = d3d_clamps[state.wrap_s];
	sampdc.AddressV = d3d_clamps[state.wrap_t];
	sampdc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

	sampdc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

	sampdc.BorderColor[0] = sampdc.BorderColor[1] = sampdc.BorderColor[2] = sampdc.BorderColor[3] = 1.0f;

	sampdc.MaxLOD = SamplerCommon::IsBpTexMode0MipmapsEnabled(state) ? static_cast<float>(state.max_lod) / 16.f : 0.f;
	sampdc.MinLOD = std::min(static_cast<float>(state.min_lod) / 16.f, sampdc.MaxLOD);
	sampdc.MipLODBias = static_cast<s32>(state.lod_bias) / 32.0f;

	return sampdc;
}

D3D12_BLEND GetBlendingAlpha(D3D12_BLEND blend)
{
	switch (blend)
	{
	case D3D12_BLEND_SRC_COLOR:
		return D3D12_BLEND_SRC_ALPHA;
	case D3D12_BLEND_INV_SRC_COLOR:
		return D3D12_BLEND_INV_SRC_ALPHA;
	case D3D12_BLEND_DEST_COLOR:
		return D3D12_BLEND_DEST_ALPHA;
	case D3D12_BLEND_INV_DEST_COLOR:
		return D3D12_BLEND_INV_DEST_ALPHA;
	default:
		return blend;
	}
}

D3D12_BLEND_DESC StateCache::GetDesc(BlendState state)
{
	if (!state.blend_enable)
	{
		state.src_blend = D3D12_BLEND_ONE;
		state.dst_blend = D3D12_BLEND_ZERO;
		state.blend_op = D3D12_BLEND_OP_ADD;
		state.use_dst_alpha = false;
	}

	D3D12_BLEND_DESC blenddc = {
		FALSE, // BOOL AlphaToCoverageEnable;
		FALSE, // BOOL IndependentBlendEnable;
		{
			state.blend_enable && !(state.logic_op_enabled && !state.use_dst_alpha),	// BOOL BlendEnable;
			state.logic_op_enabled && !state.use_dst_alpha,	// BOOL LogicOpEnable;
			state.src_blend,      // D3D12_BLEND SrcBlend;
			state.dst_blend,      // D3D12_BLEND DestBlend;
			state.blend_op,       // D3D12_BLEND_OP BlendOp;
			state.src_blend,      // D3D12_BLEND SrcBlendAlpha;
			state.dst_blend,      // D3D12_BLEND DestBlendAlpha;
			state.blend_op,       // D3D12_BLEND_OP BlendOpAlpha;
			state.logic_op,       // D3D12_LOGIC_OP LogicOp
			state.write_mask      // UINT8 RenderTargetWriteMask;
		}
	};
	if (blenddc.RenderTarget[0].BlendEnable)
	{
		blenddc.RenderTarget[0].SrcBlendAlpha = GetBlendingAlpha(blenddc.RenderTarget[0].SrcBlend);
		blenddc.RenderTarget[0].DestBlendAlpha = GetBlendingAlpha(blenddc.RenderTarget[0].DestBlend);

		if (state.use_dst_alpha)
		{
			// Colors should blend against SRC1_ALPHA
			if (blenddc.RenderTarget[0].SrcBlend == D3D12_BLEND_SRC_ALPHA)
				blenddc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC1_ALPHA;
			else if (blenddc.RenderTarget[0].SrcBlend == D3D12_BLEND_INV_SRC_ALPHA)
				blenddc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_SRC1_ALPHA;

			// Colors should blend against SRC1_ALPHA
			if (blenddc.RenderTarget[0].DestBlend == D3D12_BLEND_SRC_ALPHA)
				blenddc.RenderTarget[0].DestBlend = D3D12_BLEND_SRC1_ALPHA;
			else if (blenddc.RenderTarget[0].DestBlend == D3D12_BLEND_INV_SRC_ALPHA)
				blenddc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC1_ALPHA;

			blenddc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			blenddc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
			blenddc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		}
	}
	return blenddc;
}

D3D12_RASTERIZER_DESC StateCache::GetDesc(RasterizerState state)
{
	return{
		D3D12_FILL_MODE_SOLID,
		state.cull_mode,
		false,
		0,
		0.f,
		0,
		false,
		true,
		false,
		0,
		D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
	};
}

inline D3D12_DEPTH_STENCIL_DESC StateCache::GetDesc(DepthState state)
{
	D3D12_DEPTH_STENCIL_DESC depthdc;

	depthdc.StencilEnable = FALSE;
	depthdc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	depthdc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

	D3D12_DEPTH_STENCILOP_DESC defaultStencilOp = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
	depthdc.FrontFace = defaultStencilOp;
	depthdc.BackFace = defaultStencilOp;

	const D3D12_COMPARISON_FUNC d3dCmpFuncs[8] =
	{
		D3D12_COMPARISON_FUNC_NEVER,
		D3D12_COMPARISON_FUNC_GREATER,
		D3D12_COMPARISON_FUNC_EQUAL,
		D3D12_COMPARISON_FUNC_GREATER_EQUAL,
		D3D12_COMPARISON_FUNC_LESS,
		D3D12_COMPARISON_FUNC_NOT_EQUAL,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_COMPARISON_FUNC_ALWAYS
	};

	const D3D12_COMPARISON_FUNC d3dInvCmpFuncs[8] =
	{
		D3D12_COMPARISON_FUNC_NEVER,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_COMPARISON_FUNC_EQUAL,
		D3D12_COMPARISON_FUNC_LESS,
		D3D12_COMPARISON_FUNC_GREATER_EQUAL,
		D3D12_COMPARISON_FUNC_NOT_EQUAL,
		D3D12_COMPARISON_FUNC_GREATER,
		D3D12_COMPARISON_FUNC_ALWAYS
	};

	if (state.testenable)
	{
		depthdc.DepthEnable = TRUE;
		depthdc.DepthWriteMask = state.updateenable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		depthdc.DepthFunc = state.reversed_depth ? d3dInvCmpFuncs[state.func] : d3dCmpFuncs[state.func];
	}
	else
	{
		// if the test is disabled write is disabled too
		depthdc.DepthEnable = FALSE;
		depthdc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	}

	return depthdc;
}

HRESULT StateCache::GetPipelineStateObjectFromCache(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& pso_desc, ID3D12PipelineState** pso)
{
	auto it = m_pso_map.find(pso_desc);

	if (it == m_pso_map.end())
	{
		// Not found, create new PSO.

		ComPtr<ID3D12PipelineState> new_pso;
		HRESULT hr = D3D::device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(new_pso.ReleaseAndGetAddressOf()));

		if (FAILED(hr))
		{
			return hr;
		}

		m_pso_map[pso_desc] = new_pso;
		*pso = new_pso.Get();
	}
	else
	{
		*pso = it->second.Get();
	}

	return S_OK;
}

HRESULT StateCache::GetPipelineStateObjectFromCache(const SmallPsoDesc& pso_desc, ID3D12PipelineState** pso, D3D12_PRIMITIVE_TOPOLOGY_TYPE topology, const GeometryShaderUid* gs_uid, const PixelShaderUid* ps_uid, const VertexShaderUid* vs_uid, const TessellationShaderUid* hds_uid)
{
	auto it = m_small_pso_map.find(pso_desc);

	if (it == m_small_pso_map.end())
	{
		// Not found, create new PSO.

		// RootSignature, SampleMask, NumRenderTargets, RTVFormats, DSVFormat
		// never change so they are set in constructor and forgotten.
		m_current_pso_desc.GS = pso_desc.gs_bytecode;
		m_current_pso_desc.PS = pso_desc.ps_bytecode;
		m_current_pso_desc.VS = pso_desc.vs_bytecode;
		m_current_pso_desc.HS = pso_desc.hs_bytecode;
		m_current_pso_desc.DS = pso_desc.ds_bytecode;

		m_current_pso_desc.pRootSignature = D3D::GetRootSignature();

		m_current_pso_desc.BlendState = GetDesc(pso_desc.blend_state);
		m_current_pso_desc.DepthStencilState = GetDesc(pso_desc.depth_stencil_state);
		m_current_pso_desc.RasterizerState = GetDesc(pso_desc.rasterizer_state);
		m_current_pso_desc.PrimitiveTopologyType = topology;
		m_current_pso_desc.InputLayout = pso_desc.input_Layout->GetActiveInputLayout();
		m_current_pso_desc.SampleDesc.Count = pso_desc.sample_count;

		ComPtr<ID3D12PipelineState> new_pso;
		HRESULT hr = D3D::device->CreateGraphicsPipelineState(&m_current_pso_desc, IID_PPV_ARGS(new_pso.ReleaseAndGetAddressOf()));

		if (FAILED(hr))
		{
			CheckHR(hr);
			return hr;
		}

		m_small_pso_map[pso_desc] = new_pso;
		*pso = new_pso.Get();

		if (m_enable_disk_cache)
		{
			// This contains all of the information needed to reconstruct a PSO at startup.
			SmallPsoDiskDesc disk_desc = {};
			disk_desc.blend_state_hex = pso_desc.blend_state.hex;
			disk_desc.depth_stencil_state_hex = pso_desc.depth_stencil_state.packed;
			disk_desc.rasterizer_state_hex = pso_desc.rasterizer_state.hex;
			disk_desc.gs_uid = *gs_uid;
			disk_desc.ps_uid = *ps_uid;
			disk_desc.vs_uid = *vs_uid;
			disk_desc.hds_uid = *hds_uid;
			disk_desc.vertex_declaration = pso_desc.input_Layout->GetVertexDeclaration();
			disk_desc.topology = topology;
			disk_desc.sample_desc.Count = g_ActiveConfig.iMultisamples;
			// This shouldn't fail.. but if it does, don't cache to disk.
			ComPtr<ID3DBlob> psoBlob;
			hr = new_pso->GetCachedBlob(psoBlob.ReleaseAndGetAddressOf());
			if (SUCCEEDED(hr))
			{
				s_pso_disk_cache.Append(disk_desc, reinterpret_cast<const u8*>(psoBlob->GetBufferPointer()), static_cast<u32>(psoBlob->GetBufferSize()));
			}
		}
	}
	else
	{
		*pso = it->second.Get();
	}

	return S_OK;
}

void StateCache::Clear()
{
	m_pso_map.clear();
	m_small_pso_map.clear();

	s_pso_disk_cache.Sync();
	s_pso_disk_cache.Close();
}
}  // namespace DX12
