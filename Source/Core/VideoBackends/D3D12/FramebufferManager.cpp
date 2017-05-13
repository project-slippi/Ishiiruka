// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/Memmap.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/PostProcessing.h"
#include "VideoBackends/D3D12/Render.h"
#include "VideoBackends/D3D12/StaticShaderCache.h"
#include "VideoBackends/D3D12/XFBEncoder.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{

FramebufferManager::Efb FramebufferManager::m_efb;
unsigned int FramebufferManager::m_target_width;
unsigned int FramebufferManager::m_target_height;

D3DTexture2D*& FramebufferManager::GetEFBColorTexture()
{
	return m_efb.color_tex;
}
D3DTexture2D*& FramebufferManager::GetEFBDepthTexture()
{
	return m_efb.depth_tex;
}
D3DTexture2D*& FramebufferManager::GetEFBColorTempTexture()
{
	return m_efb.color_temp_tex;
}

void FramebufferManager::SwapReinterpretTexture()
{
	D3DTexture2D* swaptex = GetEFBColorTempTexture();
	m_efb.color_temp_tex = GetEFBColorTexture();
	m_efb.color_tex = swaptex;
}

D3DTexture2D*& FramebufferManager::GetResolvedEFBColorTexture()
{
	if (g_ActiveConfig.iMultisamples > 1)
	{
		m_efb.resolved_color_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RESOLVE_DEST);
		m_efb.color_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);

		for (int i = 0; i < m_efb.slices; i++)
		{
			D3D::current_command_list->ResolveSubresource(m_efb.resolved_color_tex->GetTex(), D3D12CalcSubresource(0, i, 0, 1, m_efb.slices), m_efb.color_tex->GetTex(), D3D12CalcSubresource(0, i, 0, 1, m_efb.slices), DXGI_FORMAT_R8G8B8A8_UNORM);
		}
		return m_efb.resolved_color_tex;
	}
	else
	{
		return m_efb.color_tex;
	}
}

D3DTexture2D*& FramebufferManager::GetResolvedEFBDepthTexture()
{
	if (g_ActiveConfig.iMultisamples > 1)
	{
		ResolveDepthTexture();

		return m_efb.resolved_depth_tex;
	}
	else
	{
		return m_efb.depth_tex;
	}
}

void FramebufferManager::InitializeEFBCache(const D3D12_CLEAR_VALUE& color_clear_value, const D3D12_CLEAR_VALUE& depth_clear_value)
{
	ComPtr<ID3D12Resource> buff;
	D3D12_RESOURCE_DESC tex_desc;

	// Render buffer for AccessEFB (color data)
	tex_desc = CD3DX12_RESOURCE_DESC::Tex2D(color_clear_value.Format, EFB_WIDTH, EFB_HEIGHT, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	HRESULT hr = D3D::device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &tex_desc, D3D12_RESOURCE_STATE_COMMON, &color_clear_value, IID_PPV_ARGS(buff.ReleaseAndGetAddressOf()));
	CHECK(hr == S_OK, "create EFB color cache texture (hr=%#x)", hr);
	m_efb.color_cache_tex = new D3DTexture2D(buff.Get(), TEXTURE_BIND_FLAG_SHADER_RESOURCE | TEXTURE_BIND_FLAG_RENDER_TARGET, color_clear_value.Format, color_clear_value.Format, DXGI_FORMAT_UNKNOWN, color_clear_value.Format, false, D3D12_RESOURCE_STATE_COMMON);
	D3D::SetDebugObjectName12(m_efb.color_cache_tex->GetTex(), "EFB color cache texture");

	// AccessEFB - Sysmem buffer used to retrieve the pixel data from color_tex
	tex_desc = CD3DX12_RESOURCE_DESC::Buffer(EFB_CACHE_PITCH * EFB_HEIGHT);
	hr = D3D::device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE, &tex_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(m_efb.color_cache_buf.ReleaseAndGetAddressOf()));
	CHECK(hr == S_OK, "create EFB color cache buffer (hr=%#x)", hr);

	// Render buffer for AccessEFB (depth data)
	tex_desc = CD3DX12_RESOURCE_DESC::Tex2D(depth_clear_value.Format, EFB_WIDTH, EFB_HEIGHT, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	hr = D3D::device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &tex_desc, D3D12_RESOURCE_STATE_COMMON, &depth_clear_value, IID_PPV_ARGS(buff.ReleaseAndGetAddressOf()));
	CHECK(hr == S_OK, "create EFB depth cache texture (hr=%#x)", hr);
	m_efb.depth_cache_tex = new D3DTexture2D(buff.Get(), TEXTURE_BIND_FLAG_RENDER_TARGET, depth_clear_value.Format, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, depth_clear_value.Format, false, D3D12_RESOURCE_STATE_COMMON);
	D3D::SetDebugObjectName12(m_efb.depth_cache_tex->GetTex(), "EFB depth cache texture (used in g_renderer->AccessEFB)");

	// AccessEFB - Sysmem buffer used to retrieve the pixel data from depth_read_texture
	tex_desc = CD3DX12_RESOURCE_DESC::Buffer(EFB_CACHE_PITCH * EFB_HEIGHT);
	hr = D3D::device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE, &tex_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(m_efb.depth_cache_buf.ReleaseAndGetAddressOf()));
	CHECK(hr == S_OK, "create EFB depth cache buffer (hr=%#x)", hr);
	D3D::SetDebugObjectName12(m_efb.depth_cache_buf.Get(), "EFB depth cache buffer");

}

FramebufferManager::FramebufferManager(u32 target_width, u32 target_height)
{
	m_target_width = std::max(target_width, 16u);
	m_target_height = std::max(target_height, 16u);

	DXGI_SAMPLE_DESC sample_desc;
	sample_desc.Count = g_ActiveConfig.iMultisamples;
	sample_desc.Quality = 0;

	ComPtr<ID3D12Resource> buff;
	D3D12_RESOURCE_DESC text_desc;
	D3D12_CLEAR_VALUE clear_valueRTV = { DXGI_FORMAT_R8G8B8A8_UNORM,{ 0.0f, 0.0f, 0.0f, 1.0f } };
	D3D12_CLEAR_VALUE clear_valueDSV = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 0.0f, 0);
	clear_valueDSV.Color[3] = 0;
	HRESULT hr;

	m_EFBLayers = m_efb.slices = (g_ActiveConfig.iStereoMode > 0) ? 2 : 1;

	// EFB color texture - primary render target
	text_desc = CD3DX12_RESOURCE_DESC::Tex2D(clear_valueRTV.Format, m_target_width, m_target_height, m_efb.slices, 1, sample_desc.Count, sample_desc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	hr = D3D::device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &text_desc, D3D12_RESOURCE_STATE_COMMON, &clear_valueRTV, IID_PPV_ARGS(buff.ReleaseAndGetAddressOf()));
	CHECK(hr == S_OK, "create EFB color texture (hr=%#x)", hr);
	m_efb.color_tex = new D3DTexture2D(buff.Get(), TEXTURE_BIND_FLAG_SHADER_RESOURCE | TEXTURE_BIND_FLAG_RENDER_TARGET, clear_valueRTV.Format, clear_valueRTV.Format, DXGI_FORMAT_UNKNOWN, clear_valueRTV.Format, (sample_desc.Count > 1), D3D12_RESOURCE_STATE_COMMON);


	// Temporary EFB color texture - used in ReinterpretPixelData
	hr = D3D::device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &text_desc, D3D12_RESOURCE_STATE_COMMON, &clear_valueRTV, IID_PPV_ARGS(buff.ReleaseAndGetAddressOf()));
	CHECK(hr == S_OK, "create EFB color temp texture (hr=%#x)", hr);
	m_efb.color_temp_tex = new D3DTexture2D(buff.Get(), TEXTURE_BIND_FLAG_SHADER_RESOURCE | TEXTURE_BIND_FLAG_RENDER_TARGET, clear_valueRTV.Format, clear_valueRTV.Format, DXGI_FORMAT_UNKNOWN, clear_valueRTV.Format, (sample_desc.Count > 1), D3D12_RESOURCE_STATE_COMMON);

	D3D::SetDebugObjectName12(m_efb.color_temp_tex->GetTex(), "EFB color temp texture");

	// EFB depth buffer - primary depth buffer
	text_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, m_target_width, m_target_height, m_efb.slices, 1, sample_desc.Count, sample_desc.Quality, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	hr = D3D::device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &text_desc, D3D12_RESOURCE_STATE_COMMON, &clear_valueDSV, IID_PPV_ARGS(buff.ReleaseAndGetAddressOf()));
	CHECK(hr == S_OK, "create EFB depth texture (hr=%#x)", hr);
	m_efb.depth_tex = new D3DTexture2D(buff.Get(), TEXTURE_BIND_FLAG_SHADER_RESOURCE | TEXTURE_BIND_FLAG_DEPTH_STENCIL, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_UNKNOWN, (sample_desc.Count > 1), D3D12_RESOURCE_STATE_COMMON);

	D3D::SetDebugObjectName12(m_efb.depth_tex->GetTex(), "EFB depth texture");
	// For the rest of the depth data use plain float textures
	clear_valueDSV.Format = DXGI_FORMAT_R32_FLOAT;
	if (g_ActiveConfig.iMultisamples > 1)
	{
		// Framebuffer resolve textures (color+depth)
		text_desc = CD3DX12_RESOURCE_DESC::Tex2D(clear_valueRTV.Format, m_target_width, m_target_height, m_efb.slices, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
		hr = D3D::device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &text_desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(buff.ReleaseAndGetAddressOf()));
		CHECK(hr == S_OK, "create EFB color resolve texture (size: %dx%d)", m_target_width, m_target_height);
		m_efb.resolved_color_tex = new D3DTexture2D(buff.Get(), TEXTURE_BIND_FLAG_SHADER_RESOURCE | TEXTURE_BIND_FLAG_RENDER_TARGET, clear_valueRTV.Format, clear_valueRTV.Format, DXGI_FORMAT_UNKNOWN, clear_valueRTV.Format, false, D3D12_RESOURCE_STATE_COMMON);
		D3D::SetDebugObjectName12(m_efb.resolved_color_tex->GetTex(), "EFB color resolve texture shader resource view");

		text_desc = CD3DX12_RESOURCE_DESC::Tex2D(clear_valueDSV.Format, m_target_width, m_target_height, m_efb.slices, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
		hr = D3D::device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &text_desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(buff.ReleaseAndGetAddressOf()));
		CHECK(hr == S_OK, "create EFB depth resolve texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
		m_efb.resolved_depth_tex = new D3DTexture2D(buff.Get(), TEXTURE_BIND_FLAG_SHADER_RESOURCE | TEXTURE_BIND_FLAG_RENDER_TARGET, clear_valueDSV.Format, clear_valueDSV.Format, DXGI_FORMAT_UNKNOWN, clear_valueDSV.Format, false, D3D12_RESOURCE_STATE_COMMON);
		D3D::SetDebugObjectName12(m_efb.resolved_depth_tex->GetTex(), "EFB depth resolve texture shader resource view");
	}
	else
	{
		m_efb.resolved_color_tex = nullptr;
		m_efb.resolved_depth_tex = nullptr;
	}
	InitializeEFBCache(clear_valueRTV, clear_valueDSV);
}

FramebufferManager::~FramebufferManager()
{
	FramebufferManager::InvalidateEFBCache();
	SAFE_RELEASE(m_efb.color_tex);
	SAFE_RELEASE(m_efb.color_temp_tex);
	SAFE_RELEASE(m_efb.color_cache_tex);
	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_efb.color_cache_buf.Detach());

	SAFE_RELEASE(m_efb.resolved_color_tex);
	SAFE_RELEASE(m_efb.depth_tex);
	SAFE_RELEASE(m_efb.depth_cache_tex);
	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_efb.depth_cache_buf.Detach());

	SAFE_RELEASE(m_efb.resolved_depth_tex);
	D3D::command_list_mgr->ExecuteQueuedWork(true);
}

void FramebufferManager::CopyToRealXFB(u32 xfbAddr, u32 fbStride, u32 fbHeight, const EFBRectangle& sourceRc, float gamma)
{
	u8* dst = Memory::GetPointer(xfbAddr);
	D3DTexture2D* src_texture = GetResolvedEFBColorTexture();
	TargetRectangle scaled_rect = g_renderer->ConvertEFBRectangle(sourceRc);
	// The destination stride can differ from the copy region width, in which case the pixels
	// outside the copy region should not be written to.
	g_xfb_encoder->EncodeTextureToRam(dst, static_cast<u32>(sourceRc.GetWidth()), fbStride, fbHeight, src_texture, scaled_rect, m_target_width, m_target_height, gamma);
}

std::unique_ptr<XFBSourceBase> FramebufferManager::CreateXFBSource(unsigned int target_width, unsigned int target_height, unsigned int layers)
{
	return std::make_unique<XFBSource>(D3DTexture2D::Create(target_width, target_height,
		TEXTURE_BIND_FLAG_SHADER_RESOURCE | TEXTURE_BIND_FLAG_RENDER_TARGET,
		DXGI_FORMAT_R8G8B8A8_UNORM, 1, layers), layers);
}

void FramebufferManager::GetTargetSize(unsigned int* width, unsigned int* height)
{
	*width = m_target_width;
	*height = m_target_height;
}

void FramebufferManager::ResolveDepthTexture()
{
	// ResolveSubresource does not work with depth textures.
	// Instead, we use a shader that selects the minimum depth from all samples.

	D3D::SetViewportAndScissor(0, 0, m_target_width, m_target_height);

	m_efb.resolved_depth_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(0, &m_efb.resolved_depth_tex->GetRTV(), FALSE, nullptr);

	D3D::SetPointCopySampler();

	// Render a quad covering the entire target, writing SV_Depth.
	const D3D12_RECT source_rect = CD3DX12_RECT(0, 0, m_target_width, m_target_height);
	D3D::DrawShadedTexQuad(
		m_efb.depth_tex,
		&source_rect,
		m_target_width,
		m_target_height,
		StaticShaderCache::GetDepthCopyPixelShader(true),
		StaticShaderCache::GetSimpleVertexShader(),
		StaticShaderCache::GetSimpleVertexShaderInputLayout(),
		StaticShaderCache::GetCopyGeometryShader(),
		0,
		DXGI_FORMAT_R32_FLOAT
	);
	// Restores proper viewport/scissor settings.
	g_renderer->RestoreAPIState();
}

void XFBSource::DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight)
{
	u8* src = Memory::GetPointer(xfbAddr);
	g_xfb_encoder->DecodeToTexture(m_tex, src, fbWidth, fbHeight);
}

void XFBSource::CopyEFB(float gamma)
{
	bool apply_post_proccesing = g_renderer->GetPostProcessor()->ShouldTriggerOnSwap();
	bool depth_copy_required = g_renderer->GetPostProcessor()->XFBDepthDataRequired();
	if (depth_copy_required && !m_depthtex)
	{
		ComPtr<ID3D12Resource> dtexture;
		D3D12_RESOURCE_DESC texdesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_FLOAT,
			texWidth, texHeight, m_slices, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
		CheckHR(
			D3D::device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC(texdesc),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				nullptr,
				IID_PPV_ARGS(dtexture.ReleaseAndGetAddressOf())
			)
		);
		m_depthtex = new D3DTexture2D(
			dtexture.Get(),
			TEXTURE_BIND_FLAG_SHADER_RESOURCE | TEXTURE_BIND_FLAG_RENDER_TARGET,
			texdesc.Format,
			texdesc.Format,
			DXGI_FORMAT_UNKNOWN,
			texdesc.Format,
			false,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);
	}
	if (apply_post_proccesing)
	{
		g_renderer->GetPostProcessor()->PostProcessEFBToTexture(reinterpret_cast<uintptr_t>(m_tex));
	}
	g_renderer->GetPostProcessor()->OnEndFrame();
	if (!apply_post_proccesing || depth_copy_required)
	{
		if (g_ActiveConfig.iMultisamples > 1)
		{
			if (!apply_post_proccesing)
			{
				m_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RESOLVE_DEST);
				FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);

				for (UINT i = 0; i < (UINT)m_slices; i++)
				{
					UINT resource_idx = D3D12CalcSubresource(0, i, 0, 1, m_slices);
					D3D::current_command_list->ResolveSubresource(
						m_tex->GetTex(),
						resource_idx,
						FramebufferManager::GetEFBColorTexture()->GetTex(),
						resource_idx, DXGI_FORMAT_R8G8B8A8_UNORM);
				}
			}
			if (depth_copy_required)
			{
				// Copy EFB data to XFB and restore render target again
				D3D::SetViewportAndScissor(0, 0, texWidth, texHeight);

				const D3D12_RECT rect = CD3DX12_RECT(0, 0, texWidth, texHeight);

				m_depthtex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
				D3D::current_command_list->OMSetRenderTargets(1, &m_depthtex->GetRTV(), FALSE, nullptr);

				D3D::SetPointCopySampler();

				D3D::DrawShadedTexQuad(
					FramebufferManager::GetEFBDepthTexture(),
					&rect,
					g_renderer->GetTargetWidth(),
					g_renderer->GetTargetHeight(),
					StaticShaderCache::GetDepthCopyPixelShader(true),
					StaticShaderCache::GetSimpleVertexShader(),
					StaticShaderCache::GetSimpleVertexShaderInputLayout(),
					StaticShaderCache::GetCopyGeometryShader(),
					0,
					DXGI_FORMAT_R32_FLOAT,
					false,
					m_depthtex->GetMultisampled()
				);
			}
		}
		else
		{
			if (!apply_post_proccesing)
			{
				m_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_DEST);
				FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
				D3D::current_command_list->CopyResource(m_tex->GetTex(), FramebufferManager::GetEFBColorTexture()->GetTex());
			}
			if (depth_copy_required)
			{
				m_depthtex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_DEST);
				FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
				D3D::current_command_list->CopyResource(m_depthtex->GetTex(), FramebufferManager::GetEFBDepthTexture()->GetTex());
			}
		}
	}
	// Restores proper viewport/scissor settings.
	g_renderer->RestoreAPIState();
}

u32 FramebufferManager::GetEFBCachedColor(u32 x, u32 y)
{
	if (!m_efb.color_cache_data)
		PopulateEFBColorCache();

	const u32* row = reinterpret_cast<const u32*>(m_efb.color_cache_data + y * EFB_CACHE_PITCH);
	return row[x];
}

float FramebufferManager::GetEFBCachedDepth(u32 x, u32 y)
{
	if (!m_efb.depth_cache_data)
		PopulateEFBDepthCache();

	u32 row_offset = y * EFB_CACHE_PITCH;
	const float* row = reinterpret_cast<const float*>(m_efb.depth_cache_data + y * EFB_CACHE_PITCH);
	return row[x];
}

void FramebufferManager::SetEFBCachedColor(u32 x, u32 y, u32 value)
{
	if (!m_efb.color_cache_data)
		return;

	u32* row = reinterpret_cast<u32*>(m_efb.color_cache_data + y * EFB_CACHE_PITCH);
	row[x] = value;
}

void FramebufferManager::SetEFBCachedDepth(u32 x, u32 y, float value)
{
	if (!m_efb.depth_cache_data)
		return;

	float* row = reinterpret_cast<float*>(m_efb.depth_cache_data + y * EFB_CACHE_PITCH);
	row[x] = value;
}

void FramebufferManager::PopulateEFBColorCache()
{
	_dbg_assert_(!m_efb.color_readback_buffer_data, "cache is invalid");
	D3D::command_list_mgr->CPUAccessNotify();
	// for non-1xIR or multisampled cases, we need to copy to an intermediate texture first
	DX12::D3DTexture2D* src_texture;
	if (g_ActiveConfig.iEFBScale != SCALE_1X || g_ActiveConfig.iMultisamples > 1)
	{
		D3D12_RECT src_rect = { 0, 0, static_cast<LONG>(m_target_width), static_cast<LONG>(m_target_height) };
		D3D::SetViewportAndScissor(0, 0, EFB_WIDTH, EFB_HEIGHT);
		m_efb.color_cache_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		D3D::current_command_list->OMSetRenderTargets(1, &m_efb.color_cache_tex->GetRTV(), FALSE, nullptr);
		D3D::SetPointCopySampler();

		D3D::DrawShadedTexQuad(
			m_efb.color_tex,
			&src_rect,
			m_target_width,
			m_target_height,
			StaticShaderCache::GetColorCopyPixelShader(true),
			StaticShaderCache::GetSimpleVertexShader(),
			StaticShaderCache::GetSimpleVertexShaderInputLayout(),
			D3D12_SHADER_BYTECODE()
		);
		src_texture = m_efb.color_cache_tex;
	}
	else
	{
		// can copy directly from efb texture
		src_texture = m_efb.color_tex;
	}

	// copy to system memory
	D3D12_BOX src_box = CD3DX12_BOX(0, 0, 0, EFB_WIDTH, EFB_HEIGHT, 1);

	D3D12_TEXTURE_COPY_LOCATION dst_location = {};
	dst_location.pResource = m_efb.color_cache_buf.Get();
	dst_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst_location.PlacedFootprint.Offset = 0;
	dst_location.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dst_location.PlacedFootprint.Footprint.Width = EFB_WIDTH;
	dst_location.PlacedFootprint.Footprint.Height = EFB_HEIGHT;
	dst_location.PlacedFootprint.Footprint.Depth = 1;
	dst_location.PlacedFootprint.Footprint.RowPitch = EFB_CACHE_PITCH;

	D3D12_TEXTURE_COPY_LOCATION src_location = {};
	src_location.pResource = src_texture->GetTex();
	src_location.SubresourceIndex = 0;
	src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

	src_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
	D3D::current_command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, &src_box);


	// Need to wait for the CPU to complete the copy (and all prior operations) before we can read it on the CPU.
	D3D::command_list_mgr->ExecuteQueuedWork(true);
	D3D12_RANGE read_range = { 0, EFB_CACHE_PITCH * EFB_HEIGHT };
	HRESULT hr = m_efb.color_cache_buf->Map(0, &read_range, reinterpret_cast<void**>(&m_efb.color_cache_data));
	CHECK(SUCCEEDED(hr), "failed to map efb peek color cache texture (hr=%08X)", hr);
}

void FramebufferManager::PopulateEFBDepthCache()
{
	_dbg_assert_(!m_efb.depth_staging_buf_map.pData, "cache is invalid");
	D3D::command_list_mgr->CPUAccessNotify();
	// for non-1xIR or multisampled cases, we need to copy to an intermediate texture first
	DX12::D3DTexture2D* src_texture;
	if (g_ActiveConfig.iEFBScale != SCALE_1X || g_ActiveConfig.iMultisamples > 1)
	{
		D3D::SetViewportAndScissor(0, 0, EFB_WIDTH, EFB_HEIGHT);

		m_efb.depth_cache_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		D3D::current_command_list->OMSetRenderTargets(0, &m_efb.depth_cache_tex->GetRTV(), FALSE, nullptr);

		D3D::SetPointCopySampler();

		// Render a quad covering the entire target, writing SV_Depth.
		const D3D12_RECT source_rect = CD3DX12_RECT(0, 0, m_target_width, m_target_height);
		D3D::DrawShadedTexQuad(
			m_efb.depth_tex,
			&source_rect,
			m_target_width,
			m_target_height,
			StaticShaderCache::GetDepthCopyPixelShader(true),
			StaticShaderCache::GetSimpleVertexShader(),
			StaticShaderCache::GetSimpleVertexShaderInputLayout(),
			StaticShaderCache::GetCopyGeometryShader(),
			0,
			DXGI_FORMAT_R32_FLOAT
		);
		src_texture = m_efb.depth_cache_tex;
	}
	else
	{
		// can copy directly from efb texture
		src_texture = m_efb.depth_tex;
	}

	// copy to system memory
	D3D12_BOX src_box = CD3DX12_BOX(0, 0, 0, EFB_WIDTH, EFB_HEIGHT, 1);

	D3D12_TEXTURE_COPY_LOCATION dst_location = {};
	dst_location.pResource = m_efb.depth_cache_buf.Get();
	dst_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst_location.PlacedFootprint.Offset = 0;
	dst_location.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32_FLOAT;
	dst_location.PlacedFootprint.Footprint.Width = EFB_WIDTH;
	dst_location.PlacedFootprint.Footprint.Height = EFB_HEIGHT;
	dst_location.PlacedFootprint.Footprint.Depth = 1;
	dst_location.PlacedFootprint.Footprint.RowPitch = EFB_CACHE_PITCH;

	D3D12_TEXTURE_COPY_LOCATION src_location = {};
	src_location.pResource = src_texture->GetTex();
	src_location.SubresourceIndex = 0;
	src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

	src_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
	D3D::current_command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, &src_box);

	// Need to wait for the CPU to complete the copy (and all prior operations) before we can read it on the CPU.
	D3D::command_list_mgr->ExecuteQueuedWork(true);
	D3D12_RANGE read_range = { 0, EFB_CACHE_PITCH * EFB_HEIGHT };
	HRESULT hr = m_efb.depth_cache_buf->Map(0, &read_range, reinterpret_cast<void**>(&m_efb.depth_cache_data));
	CHECK(SUCCEEDED(hr), "failed to map efb peek color cache texture (hr=%08X)", hr);
}

void FramebufferManager::InvalidateEFBCache()
{
	D3D12_RANGE write_range = {};
	if (m_efb.color_cache_data)
	{
		m_efb.color_cache_buf->Unmap(0, &write_range);
		m_efb.color_cache_data = nullptr;
	}

	if (m_efb.depth_cache_data)
	{
		m_efb.depth_cache_buf->Unmap(0, &write_range);
		m_efb.depth_cache_data = nullptr;
	}
}

}  // namespace DX12
