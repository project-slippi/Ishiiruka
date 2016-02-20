// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/Memmap.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/Render.h"
#include "VideoBackends/D3D12/StaticShaderCache.h"
#include "VideoBackends/D3D12/XFBEncoder.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{

static XFBEncoder s_xfbEncoder;

FramebufferManager::Efb FramebufferManager::m_efb;
unsigned int FramebufferManager::m_target_width;
unsigned int FramebufferManager::m_target_height;

D3DTexture2D*& FramebufferManager::GetEFBColorTexture() { return m_efb.color_tex; }
ID3D12Resource*& FramebufferManager::GetEFBColorStagingBuffer() { return m_efb.color_staging_buf; }

D3DTexture2D*& FramebufferManager::GetEFBDepthTexture() { return m_efb.depth_tex; }
D3DTexture2D*& FramebufferManager::GetEFBDepthReadTexture() { return m_efb.depth_read_tex; }
ID3D12Resource*& FramebufferManager::GetEFBDepthStagingBuffer() { return m_efb.depth_staging_buf; }

D3DTexture2D*& FramebufferManager::GetEFBColorTempTexture() { return m_efb.color_temp_tex; }

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
			D3D::current_command_list->ResolveSubresource(m_efb.resolved_color_tex->GetTex(), D3D11CalcSubresource(0, i, 1), m_efb.color_tex->GetTex(), D3D11CalcSubresource(0, i, 1), DXGI_FORMAT_R8G8B8A8_UNORM);
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

FramebufferManager::FramebufferManager()
{
	m_target_width = std::max(Renderer::GetTargetWidth(), 1);
	m_target_height = std::max(Renderer::GetTargetHeight(), 1);

	DXGI_SAMPLE_DESC sample_desc;
	sample_desc.Count = g_ActiveConfig.iMultisamples;
	sample_desc.Quality = 0;

	ID3D12Resource* buf12;
	D3D12_RESOURCE_DESC texdesc12;
	D3D12_CLEAR_VALUE optimized_clear_valueRTV = { DXGI_FORMAT_R8G8B8A8_UNORM,{ 0.0f, 0.0f, 0.0f, 1.0f } };
	D3D12_CLEAR_VALUE optimized_clear_valueDSV = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 0.0f, 0);

	HRESULT hr;

	m_EFBLayers = m_efb.slices = (g_ActiveConfig.iStereoMode > 0) ? 2 : 1;

	// EFB color texture - primary render target
	texdesc12 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, m_target_width, m_target_height, m_efb.slices, 1, sample_desc.Count, sample_desc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	hr = D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texdesc12, D3D12_RESOURCE_STATE_COMMON, &optimized_clear_valueRTV, IID_PPV_ARGS(&buf12));

	m_efb.color_tex = new D3DTexture2D(buf12, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET), DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM, (sample_desc.Count > 1), D3D12_RESOURCE_STATE_COMMON);
	SAFE_RELEASE(buf12);

	// Temporary EFB color texture - used in ReinterpretPixelData
	texdesc12 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, m_target_width, m_target_height, m_efb.slices, 1, sample_desc.Count, sample_desc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	CheckHR(D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texdesc12, D3D12_RESOURCE_STATE_COMMON, &optimized_clear_valueRTV, IID_PPV_ARGS(&buf12)));
	m_efb.color_temp_tex = new D3DTexture2D(buf12, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET), DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM, (sample_desc.Count > 1), D3D12_RESOURCE_STATE_COMMON);
	SAFE_RELEASE(buf12);
	D3D::SetDebugObjectName12(m_efb.color_temp_tex->GetTex(), "EFB color temp texture");

	// Render buffer for AccessEFB (color data)
	texdesc12 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, EFB_WIDTH, EFB_HEIGHT, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	CheckHR(D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texdesc12, D3D12_RESOURCE_STATE_COMMON, &optimized_clear_valueRTV, IID_PPV_ARGS(&buf12)));
	m_efb.color_read_tex = new D3DTexture2D(buf12, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET), DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM, false, D3D12_RESOURCE_STATE_COMMON);
	SAFE_RELEASE(buf12);
	D3D::SetDebugObjectName12(m_efb.color_read_tex->GetTex(), "EFB color read texture");

	// AccessEFB - Sysmem buffer used to retrieve the pixel data from color_tex
	texdesc12 = CD3DX12_RESOURCE_DESC::Buffer(D3D::AlignValue(EFB_WIDTH * sizeof(int), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * EFB_HEIGHT);
	CheckHR(D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE, &texdesc12, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_efb.color_staging_buf)));
	CHECK(hr == S_OK, "create EFB color staging buffer (hr=%#x)", hr);

	// EFB depth buffer - primary depth buffer
	texdesc12 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, m_target_width, m_target_height, m_efb.slices, 1, sample_desc.Count, sample_desc.Quality, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	CheckHR(D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texdesc12, D3D12_RESOURCE_STATE_COMMON, &optimized_clear_valueDSV, IID_PPV_ARGS(&buf12)));

	m_efb.depth_tex = new D3DTexture2D(buf12, (D3D11_BIND_FLAG)(D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE), DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_UNKNOWN, (sample_desc.Count > 1), D3D12_RESOURCE_STATE_COMMON);
	SAFE_RELEASE(buf12);
	D3D::SetDebugObjectName12(m_efb.depth_tex->GetTex(), "EFB depth texture");

	// Render buffer for AccessEFB (depth data)
	texdesc12 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_FLOAT, EFB_WIDTH, EFB_HEIGHT, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	optimized_clear_valueRTV.Format = DXGI_FORMAT_R32_FLOAT;
	hr = D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texdesc12, D3D12_RESOURCE_STATE_COMMON, &optimized_clear_valueRTV, IID_PPV_ARGS(&buf12));
	CHECK(hr == S_OK, "create EFB depth read texture (hr=%#x)", hr);

	m_efb.depth_read_tex = new D3DTexture2D(buf12, D3D11_BIND_RENDER_TARGET, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, false, D3D12_RESOURCE_STATE_COMMON);

	SAFE_RELEASE(buf12);
	D3D::SetDebugObjectName12(m_efb.depth_read_tex->GetTex(), "EFB depth read texture (used in Renderer::AccessEFB)");

	// AccessEFB - Sysmem buffer used to retrieve the pixel data from depth_read_texture
	texdesc12 = CD3DX12_RESOURCE_DESC::Buffer(D3D::AlignValue(EFB_WIDTH * sizeof(float), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * EFB_HEIGHT);
	hr = D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE, &texdesc12, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_efb.depth_staging_buf));
	CHECK(hr == S_OK, "create EFB depth staging buffer (hr=%#x)", hr);

	D3D::SetDebugObjectName12(m_efb.depth_staging_buf, "EFB depth staging texture (used for Renderer::AccessEFB)");

	if (g_ActiveConfig.iMultisamples > 1)
	{
		// Framebuffer resolve textures (color+depth)
		texdesc12 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, m_target_width, m_target_height, m_efb.slices, 1);
		hr = D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texdesc12, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&buf12));
		CHECK(hr == S_OK, "create EFB color resolve texture (size: %dx%d)", m_target_width, m_target_height);
		m_efb.resolved_color_tex = new D3DTexture2D(buf12, D3D11_BIND_SHADER_RESOURCE, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, false, D3D12_RESOURCE_STATE_COMMON);
		SAFE_RELEASE(buf12);
		D3D::SetDebugObjectName12(m_efb.resolved_color_tex->GetTex(), "EFB color resolve texture shader resource view");

		texdesc12 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_FLOAT, m_target_width, m_target_height, m_efb.slices, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
		hr = D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texdesc12, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&buf12));
		CHECK(hr == S_OK, "create EFB depth resolve texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
		m_efb.resolved_depth_tex = new D3DTexture2D(buf12, (D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE), DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_UNKNOWN, false, D3D12_RESOURCE_STATE_COMMON);
		SAFE_RELEASE(buf12);
		D3D::SetDebugObjectName12(m_efb.resolved_depth_tex->GetTex(), "EFB depth resolve texture shader resource view");
	}
	else
	{
		m_efb.resolved_color_tex = nullptr;
		m_efb.resolved_depth_tex = nullptr;
	}

	s_xfbEncoder.Init();
}

FramebufferManager::~FramebufferManager()
{
	s_xfbEncoder.Shutdown();
	FramebufferManager::InvalidateEFBPeekCache();
	SAFE_RELEASE(m_efb.color_tex);
	SAFE_RELEASE(m_efb.color_temp_tex);
	SAFE_RELEASE(m_efb.color_read_tex);
	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_efb.color_staging_buf);

	SAFE_RELEASE(m_efb.resolved_color_tex);
	SAFE_RELEASE(m_efb.depth_tex);
	SAFE_RELEASE(m_efb.depth_read_tex);
	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_efb.depth_staging_buf);

	SAFE_RELEASE(m_efb.resolved_depth_tex);
}

void FramebufferManager::CopyToRealXFB(u32 xfbAddr, u32 fbStride, u32 fbHeight, const EFBRectangle& sourceRc, float gamma)
{
	u8* dst = Memory::GetPointer(xfbAddr);
	s_xfbEncoder.Encode(dst, fbStride / 2, fbHeight, sourceRc, gamma);
}

std::unique_ptr<XFBSourceBase> FramebufferManager::CreateXFBSource(unsigned int target_width, unsigned int target_height, unsigned int layers)
{
	return std::make_unique<XFBSource>(D3DTexture2D::Create(target_width, target_height,
		(D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE),
		D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM, 1, layers), layers);
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

	const D3D12_VIEWPORT vp = { 0.f, 0.f, static_cast<float>(m_target_width), static_cast<float>(m_target_height), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D::current_command_list->RSSetViewports(1, &vp);

	m_efb.resolved_depth_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(0, &m_efb.resolved_depth_tex->GetRTV(), FALSE, nullptr);

	m_efb.depth_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

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
		1.0,
		0,
		DXGI_FORMAT_R32_FLOAT
		);

	m_efb.color_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_efb.depth_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	D3D::current_command_list->OMSetRenderTargets(1, &m_efb.color_tex->GetRTV(), FALSE, &m_efb.depth_tex->GetDSV());

	// Restores proper viewport/scissor settings.
	g_renderer->RestoreAPIState();
}

void XFBSource::DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight)
{
	// DX12's XFB decoder does not use this function.
	// YUYV data is decoded in Render::Swap.
}

void XFBSource::CopyEFB(float gamma)
{
	// Copy EFB data to XFB and restore render target again
	const D3D12_VIEWPORT vp12 = { 0.f, 0.f,  static_cast<float>(texWidth), static_cast<float>(texHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D::current_command_list->RSSetViewports(1, &vp12);

	const D3D12_RECT rect = CD3DX12_RECT(0, 0, texWidth, texHeight);

	m_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &m_tex->GetRTV(), FALSE, nullptr);

	D3D::SetPointCopySampler();

	D3D::DrawShadedTexQuad(
		FramebufferManager::GetEFBColorTexture(),
		&rect,
		Renderer::GetTargetWidth(),
		Renderer::GetTargetHeight(),
		StaticShaderCache::GetColorCopyPixelShader(true),
		StaticShaderCache::GetSimpleVertexShader(),
		StaticShaderCache::GetSimpleVertexShaderInputLayout(),
		StaticShaderCache::GetCopyGeometryShader(),
		gamma,
		0,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		false,
		m_tex->GetMultisampled()
		);

	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV());

	// Restores proper viewport/scissor settings.
	g_renderer->RestoreAPIState();
}

u32 FramebufferManager::AccessEFBPeekColorCache(u32 x, u32 y)
{
	if (!m_efb.color_readback_buffer_data)
		PopulateEFBPeekColorCache();

	u32 row_offset = y * D3D::AlignValue(EFB_WIDTH * sizeof(int), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	u32* row = reinterpret_cast<u32*>(reinterpret_cast<u8*>(m_efb.color_readback_buffer_data) + row_offset);
	return row[x];
}

float FramebufferManager::AccessEFBPeekDepthCache(u32 x, u32 y)
{
	if (!m_efb.depth_readback_buffer_data)
		PopulateEFBPeekDepthCache();

	u32 row_offset = y * D3D::AlignValue(EFB_WIDTH * sizeof(float), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	float* row = reinterpret_cast<float*>(reinterpret_cast<u8*>(m_efb.depth_readback_buffer_data) + row_offset);
	return row[x];
}

void FramebufferManager::UpdateEFBPeekColorCache(u32 x, u32 y, u32 value)
{
	if (!m_efb.color_readback_buffer_data)
		return;

	u32 row_offset = y * D3D::AlignValue(EFB_WIDTH * sizeof(int), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	u32* row = reinterpret_cast<u32*>(reinterpret_cast<u8*>(m_efb.color_readback_buffer_data) + row_offset);
	row[x] = value;
}

void FramebufferManager::UpdateEFBPeekDepthCache(u32 x, u32 y, float value)
{
	if (!m_efb.depth_readback_buffer_data)
		return;

	u32 row_offset = y * D3D::AlignValue(EFB_WIDTH * sizeof(float), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	float* row = reinterpret_cast<float*>(reinterpret_cast<u8*>(m_efb.depth_readback_buffer_data) + row_offset);
	row[x] = value;
}

void FramebufferManager::PopulateEFBPeekColorCache()
{
	_dbg_assert_(!m_efb.color_readback_buffer_data, "cache is invalid");
	D3D::command_list_mgr->CPUAccessNotify();
	// for non-1xIR or multisampled cases, we need to copy to an intermediate texture first
	DX12::D3DTexture2D* src_texture;
	if (g_ActiveConfig.iEFBScale != SCALE_1X || g_ActiveConfig.iMultisamples > 1)
	{
		D3D12_RECT src_rect = { 0, 0, static_cast<LONG>(m_target_width), static_cast<LONG>(m_target_height) };
		D3D12_VIEWPORT vp = { 0.0f, 0.0f, static_cast<float>(EFB_WIDTH),static_cast<float>(EFB_HEIGHT), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
		D3D::current_command_list->RSSetViewports(1, &vp);
		m_efb.color_read_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		D3D::current_command_list->OMSetRenderTargets(1, &m_efb.color_read_tex->GetRTV(), FALSE, nullptr);
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
		
		m_efb.color_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_efb.depth_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		D3D::current_command_list->OMSetRenderTargets(1, &m_efb.color_tex->GetRTV(), FALSE, &m_efb.depth_tex->GetDSV());
		g_renderer->RestoreAPIState();

		src_texture = m_efb.color_read_tex;
	}
	else
	{
		// can copy directly from efb texture
		src_texture = m_efb.color_tex;
	}

	// copy to system memory
	D3D12_BOX src_box = CD3DX12_BOX(0, 0, 0, EFB_WIDTH, EFB_HEIGHT, 1);

	D3D12_TEXTURE_COPY_LOCATION dst_location = {};
	dst_location.pResource = m_efb.color_staging_buf;
	dst_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst_location.PlacedFootprint.Offset = 0;
	dst_location.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dst_location.PlacedFootprint.Footprint.Width = EFB_WIDTH;
	dst_location.PlacedFootprint.Footprint.Height = EFB_HEIGHT;
	dst_location.PlacedFootprint.Footprint.Depth = 1;
	dst_location.PlacedFootprint.Footprint.RowPitch = D3D::AlignValue(EFB_WIDTH * sizeof(int), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	D3D12_TEXTURE_COPY_LOCATION src_location = {};
	src_location.pResource = src_texture->GetTex();
	src_location.SubresourceIndex = 0;
	src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

	src_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
	D3D::current_command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, &src_box);
	// Need to wait for the CPU to complete the copy (and all prior operations) before we can read it on the CPU.
	D3D::command_list_mgr->ExecuteQueuedWork(true);

	HRESULT hr = m_efb.color_staging_buf->Map(0, nullptr, &m_efb.color_readback_buffer_data);
	CHECK(SUCCEEDED(hr), "failed to map efb peek color cache texture (hr=%08X)", hr);
}

void FramebufferManager::PopulateEFBPeekDepthCache()
{
	_dbg_assert_(!m_efb.depth_staging_buf_map.pData, "cache is invalid");
	D3D::command_list_mgr->CPUAccessNotify();
	// for non-1xIR or multisampled cases, we need to copy to an intermediate texture first
	DX12::D3DTexture2D* src_texture;
	if (g_ActiveConfig.iEFBScale != SCALE_1X || g_ActiveConfig.iMultisamples > 1)
	{
		const D3D12_VIEWPORT vp12 = { 0.f, 0.f, static_cast<float>(EFB_WIDTH), static_cast<float>(EFB_HEIGHT), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
		D3D::current_command_list->RSSetViewports(1, &vp12);

		m_efb.depth_read_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		D3D::current_command_list->OMSetRenderTargets(0, &m_efb.depth_read_tex->GetRTV(), FALSE, nullptr);

		m_efb.depth_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

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
			1.0,
			0,
			DXGI_FORMAT_R32_FLOAT
			);		
		src_texture = m_efb.depth_read_tex;
	}
	else
	{
		// can copy directly from efb texture
		src_texture = m_efb.depth_tex;
	}

	// copy to system memory
	D3D12_BOX src_box = CD3DX12_BOX(0, 0, 0, EFB_WIDTH, EFB_HEIGHT, 1);

	D3D12_TEXTURE_COPY_LOCATION dst_location = {};
	dst_location.pResource = m_efb.depth_staging_buf;
	dst_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst_location.PlacedFootprint.Offset = 0;
	dst_location.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32_FLOAT;
	dst_location.PlacedFootprint.Footprint.Width = EFB_WIDTH;
	dst_location.PlacedFootprint.Footprint.Height = EFB_HEIGHT;
	dst_location.PlacedFootprint.Footprint.Depth = 1;
	dst_location.PlacedFootprint.Footprint.RowPitch = D3D::AlignValue(EFB_WIDTH * sizeof(float), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	D3D12_TEXTURE_COPY_LOCATION src_location = {};
	src_location.pResource = src_texture->GetTex();
	src_location.SubresourceIndex = 0;
	src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

	src_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
	D3D::current_command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, &src_box);
	

	// Restores proper viewport/scissor settings.
	g_renderer->RestoreAPIState();
	// Need to wait for the CPU to complete the copy (and all prior operations) before we can read it on the CPU.
	D3D::command_list_mgr->ExecuteQueuedWork(true);
	m_efb.color_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_efb.depth_tex->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	D3D::current_command_list->OMSetRenderTargets(1, &m_efb.color_tex->GetRTV(), FALSE, &m_efb.depth_tex->GetDSV());
	HRESULT hr = m_efb.depth_staging_buf->Map(0, nullptr, &m_efb.depth_readback_buffer_data);
	CHECK(SUCCEEDED(hr), "failed to map efb peek color cache texture (hr=%08X)", hr);
}

void FramebufferManager::InvalidateEFBPeekCache()
{
	if (m_efb.color_readback_buffer_data)
	{
		m_efb.color_staging_buf->Unmap(0, nullptr);
		m_efb.color_readback_buffer_data = nullptr;
	}

	if (m_efb.depth_readback_buffer_data)
	{
		m_efb.depth_staging_buf->Unmap(0, nullptr);
		m_efb.depth_readback_buffer_data = nullptr;
	}
}

}  // namespace DX12
