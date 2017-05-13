// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/Memmap.h"
#include "VideoBackends/DX11/D3DBase.h"
#include "VideoBackends/DX11/D3DState.h"
#include "VideoBackends/DX11/D3DUtil.h"
#include "VideoBackends/DX11/FramebufferManager.h"
#include "VideoBackends/DX11/GeometryShaderCache.h"
#include "VideoBackends/DX11/PixelShaderCache.h"
#include "VideoBackends/DX11/PostProcessing.h"
#include "VideoBackends/DX11/Render.h"
#include "VideoBackends/DX11/VertexShaderCache.h"
#include "VideoBackends/DX11/XFBEncoder.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{

static XFBEncoder s_xfbEncoder;

FramebufferManager::Efb FramebufferManager::m_efb;
u32 FramebufferManager::m_target_width;
u32 FramebufferManager::m_target_height;
D3DTexture2D* &FramebufferManager::GetEFBColorTexture()
{
	return m_efb.color_tex;
}
D3DTexture2D* &FramebufferManager::GetEFBDepthTexture()
{
	return m_efb.depth_tex;
}

D3DTexture2D* &FramebufferManager::GetResolvedEFBColorTexture()
{
	if (g_ActiveConfig.iMultisamples > 1)
	{
		for (int i = 0; i < m_efb.slices; i++)
			D3D::context->ResolveSubresource(m_efb.resolved_color_tex->GetTex(), D3D11CalcSubresource(0, i, 1), m_efb.color_tex->GetTex(), D3D11CalcSubresource(0, i, 1), DXGI_FORMAT_R8G8B8A8_UNORM);
		return m_efb.resolved_color_tex;
	}
	else
		return m_efb.color_tex;
}

D3DTexture2D* &FramebufferManager::GetResolvedEFBDepthTexture()
{
	if (g_ActiveConfig.iMultisamples > 1)
	{
		// ResolveSubresource does not work with depth textures.
		// Instead, we use a shader that selects the minimum depth from all samples.

		// Clear render state, and enable depth writes.
		g_renderer->ResetAPIState();

		// Set up to render to resolved depth texture.
		CD3D11_VIEWPORT viewport(0.f, 0.f, (float)m_target_width, (float)m_target_height);
		D3D::context->RSSetViewports(1, &viewport);
		D3D::context->OMSetRenderTargets(1, &m_efb.resolved_depth_tex->GetRTV(), nullptr);

		const D3D11_RECT target_rect = CD3D11_RECT(0, 0, m_target_width, m_target_height);
		D3D::drawShadedTexQuad(m_efb.depth_tex->GetSRV(), &target_rect, m_target_width, m_target_height,
			PixelShaderCache::GetDepthResolveProgram(), VertexShaderCache::GetSimpleVertexShader(),
			VertexShaderCache::GetSimpleInputLayout(), GeometryShaderCache::GetCopyGeometryShader());

		D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());
		g_renderer->RestoreAPIState();
		return m_efb.resolved_depth_tex;
	}
	else
	{
		return m_efb.depth_tex;
	}
}

void FramebufferManager::InitializeEFBCache()
{
	ID3D11Texture2D* buf;
	D3D11_TEXTURE2D_DESC tex_desc;
	HRESULT hr;

	// Render buffer for AccessEFB (color data)
	tex_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, EFB_WIDTH, EFB_HEIGHT, 1, 1, D3D11_BIND_RENDER_TARGET);
	hr = D3D::device->CreateTexture2D(&tex_desc, nullptr, &buf);
	CHECK(hr == S_OK, "create EFB color cache texture (hr=%#x)", hr);
	m_efb.color_cache_tex = new D3DTexture2D(buf, D3D11_BIND_RENDER_TARGET);
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_cache_tex->GetTex(), "EFB color read texture (used in g_renderer->AccessEFB)");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_cache_tex->GetRTV(), "EFB color read texture render target view (used in g_renderer->AccessEFB)");

	// AccessEFB - Sysmem buffer used to retrieve the pixel data from color_tex
	tex_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, EFB_WIDTH, EFB_HEIGHT, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE);
	hr = D3D::device->CreateTexture2D(&tex_desc, nullptr, &m_efb.color_cache_buf);
	CHECK(hr == S_OK, "create EFB color cache buffer (hr=%#x)", hr);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_cache_buf, "EFB color staging texture (used for g_renderer->AccessEFB)");

	// Render buffer for AccessEFB (depth data)
	tex_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32_FLOAT, EFB_WIDTH, EFB_HEIGHT, 1, 1, D3D11_BIND_RENDER_TARGET);
	hr = D3D::device->CreateTexture2D(&tex_desc, nullptr, &buf);
	CHECK(hr == S_OK, "create EFB depth cache read texture (hr=%#x)", hr);
	m_efb.depth_cache_tex = new D3DTexture2D(buf, D3D11_BIND_RENDER_TARGET);
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_cache_tex->GetTex(), "EFB depth read texture (used in g_renderer->AccessEFB)");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_cache_tex->GetRTV(), "EFB depth read texture render target view (used in g_renderer->AccessEFB)");

	// AccessEFB - Sysmem buffer used to retrieve the pixel data from depth_read_texture
	tex_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32_FLOAT, EFB_WIDTH, EFB_HEIGHT, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE);
	hr = D3D::device->CreateTexture2D(&tex_desc, nullptr, &m_efb.depth_cache_buf);
	CHECK(hr == S_OK, "create EFB depth staging buffer (hr=%#x)", hr);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_cache_buf, "EFB depth cache buffer (used for g_renderer->AccessEFB)");
}

FramebufferManager::FramebufferManager(u32 target_width, u32 target_height)
{
	m_target_width = std::max(target_width, 16u);
	m_target_height = std::max(target_height, 16u);

	DXGI_SAMPLE_DESC sample_desc;
	sample_desc.Count = g_ActiveConfig.iMultisamples;
	sample_desc.Quality = 0;

	ID3D11Texture2D* buf;
	D3D11_TEXTURE2D_DESC tex_desc;
	HRESULT hr;

	m_EFBLayers = m_efb.slices = (g_ActiveConfig.iStereoMode > 0) ? 2 : 1;

	// EFB color texture - primary render target
	tex_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, m_target_width, m_target_height, m_efb.slices, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, sample_desc.Count, sample_desc.Quality);
	hr = D3D::device->CreateTexture2D(&tex_desc, nullptr, &buf);
	CHECK(hr == S_OK, "create EFB color texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
	m_efb.color_tex = new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET), DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM, (sample_desc.Count > 1));
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_tex->GetTex(), "EFB color texture");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_tex->GetSRV(), "EFB color texture shader resource view");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_tex->GetRTV(), "EFB color texture render target view");

	// Temporary EFB color texture - used in ReinterpretPixelData
	tex_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, m_target_width, m_target_height, m_efb.slices, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, sample_desc.Count, sample_desc.Quality);
	hr = D3D::device->CreateTexture2D(&tex_desc, nullptr, &buf);
	CHECK(hr == S_OK, "create EFB color temp texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
	m_efb.color_temp_tex = new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET), DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM, (sample_desc.Count > 1));
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_temp_tex->GetTex(), "EFB color temp texture");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_temp_tex->GetSRV(), "EFB color temp texture shader resource view");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.color_temp_tex->GetRTV(), "EFB color temp texture render target view");

	// EFB depth buffer - primary depth buffer
	tex_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32_TYPELESS, m_target_width, m_target_height, m_efb.slices, 1, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, sample_desc.Count, sample_desc.Quality);
	hr = D3D::device->CreateTexture2D(&tex_desc, nullptr, &buf);
	CHECK(hr == S_OK, "create EFB depth texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
	m_efb.depth_tex = new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE), DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_UNKNOWN, (sample_desc.Count > 1));
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_tex->GetTex(), "EFB depth texture");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_tex->GetDSV(), "EFB depth texture depth stencil view");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.depth_tex->GetSRV(), "EFB depth texture shader resource view");

	if (g_ActiveConfig.iMultisamples > 1)
	{
		// Framebuffer resolve textures (color+depth)
		tex_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, m_target_width, m_target_height, m_efb.slices, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, 1);
		hr = D3D::device->CreateTexture2D(&tex_desc, nullptr, &buf);
		CHECK(hr == S_OK, "create EFB color resolve texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
		m_efb.resolved_color_tex = new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET), DXGI_FORMAT_R8G8B8A8_UNORM);
		SAFE_RELEASE(buf);
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.resolved_color_tex->GetTex(), "EFB color resolve texture");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.resolved_color_tex->GetSRV(), "EFB color resolve texture shader resource view");

		tex_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32_FLOAT, m_target_width, m_target_height, m_efb.slices, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
		hr = D3D::device->CreateTexture2D(&tex_desc, nullptr, &buf);
		CHECK(hr == S_OK, "create EFB depth resolve texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
		m_efb.resolved_depth_tex = new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET), DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R32_FLOAT);
		SAFE_RELEASE(buf);
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.resolved_depth_tex->GetTex(), "EFB depth resolve texture");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_efb.resolved_depth_tex->GetSRV(), "EFB depth resolve texture shader resource view");
	}
	else
	{
		m_efb.resolved_color_tex = nullptr;
		m_efb.resolved_depth_tex = nullptr;
	}
	InitializeEFBCache();
	s_xfbEncoder.Init();
}

FramebufferManager::~FramebufferManager()
{
	s_xfbEncoder.Shutdown();
	FramebufferManager::InvalidateEFBCache();
	SAFE_RELEASE(m_efb.color_tex);
	SAFE_RELEASE(m_efb.color_temp_tex);
	SAFE_RELEASE(m_efb.color_cache_buf);
	SAFE_RELEASE(m_efb.color_cache_tex);
	SAFE_RELEASE(m_efb.resolved_color_tex);
	SAFE_RELEASE(m_efb.depth_tex);
	SAFE_RELEASE(m_efb.depth_cache_buf);
	SAFE_RELEASE(m_efb.depth_cache_tex);
	SAFE_RELEASE(m_efb.resolved_depth_tex);
}

void FramebufferManager::CopyToRealXFB(u32 xfbAddr, u32 fbStride, u32 fbHeight, const EFBRectangle& sourceRc, float Gamma)
{
	u8* dst = Memory::GetPointer(xfbAddr);
	// The destination stride can differ from the copy region width, in which case the pixels
	// outside the copy region should not be written to.
	s_xfbEncoder.Encode(dst, static_cast<u32>(sourceRc.GetWidth()), fbHeight, sourceRc, Gamma);
}

std::unique_ptr<XFBSourceBase> FramebufferManager::CreateXFBSource(u32 target_width, u32 target_height, u32 layers)
{
	return std::make_unique<XFBSource>(D3DTexture2D::Create(target_width, target_height,
		(D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE),
		D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM, 1, layers), layers);
}

void FramebufferManager::GetTargetSize(u32 *width, u32 *height)
{
	*width = m_target_width;
	*height = m_target_height;
}

void XFBSource::DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight)
{
	// DX11's XFB decoder does not use this function.
	// YUYV data is decoded in Render::Swap.
}

void XFBSource::CopyEFB(float Gamma)
{
	bool apply_post_proccesing = g_renderer->GetPostProcessor()->ShouldTriggerOnSwap();
	bool depth_copy_required = g_renderer->GetPostProcessor()->XFBDepthDataRequired();
	if (depth_copy_required && !depthtex)
	{
		depthtex = D3DTexture2D::Create(texWidth, texHeight,
			(D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE),
			D3D11_USAGE_DEFAULT, DXGI_FORMAT_R32_FLOAT, 1, m_slices);
	}
	if (apply_post_proccesing)
	{
		g_renderer->GetPostProcessor()->PostProcessEFBToTexture(reinterpret_cast<uintptr_t>(tex));
	}
	g_renderer->GetPostProcessor()->OnEndFrame();
	if (!apply_post_proccesing || depth_copy_required)
	{
		if (g_ActiveConfig.iMultisamples > 1)
		{
			if (!apply_post_proccesing)
			{
				for (UINT i = 0; i < (UINT)m_slices; i++)
				{
					UINT resource_idx = D3D11CalcSubresource(0, i, 1);
					D3D::context->ResolveSubresource(
						tex->GetTex(),
						resource_idx,
						FramebufferManager::GetEFBColorTexture()->GetTex(),
						resource_idx, DXGI_FORMAT_R8G8B8A8_UNORM);
				}
			}
			if (depth_copy_required)
			{
				g_renderer->ResetAPIState();
				const D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)texWidth, (float)texHeight);
				D3D::context->OMSetRenderTargets(1, &depthtex->GetRTV(), nullptr);
				D3D::context->RSSetViewports(1, &vp);
				D3D::SetPointCopySampler();
				D3D::drawShadedTexQuad(FramebufferManager::GetEFBDepthTexture()->GetSRV(), nullptr,
					g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight(),
					PixelShaderCache::GetColorCopyProgram(true), VertexShaderCache::GetSimpleVertexShader(),
					VertexShaderCache::GetSimpleInputLayout(), GeometryShaderCache::GetCopyGeometryShader(), 1.0, 0, texWidth, texHeight);
				D3D::context->OMSetRenderTargets(1,
					&FramebufferManager::GetEFBColorTexture()->GetRTV(),
					FramebufferManager::GetEFBDepthTexture()->GetDSV());
				g_renderer->RestoreAPIState();
			}
		}
		else
		{
			if (!apply_post_proccesing)
				D3D::context->CopyResource(tex->GetTex(), FramebufferManager::GetEFBColorTexture()->GetTex());
			if (depth_copy_required)
			{
				D3D::context->CopyResource(depthtex->GetTex(), FramebufferManager::GetEFBDepthTexture()->GetTex());
			}
		}
	}
}

u32 FramebufferManager::GetEFBCachedColor(u32 x, u32 y)
{
	if (!m_efb.color_cache_buf_map.pData)
		PopulateEFBColorCache();

	u32 row_offset = y * m_efb.color_cache_buf_map.RowPitch;
	u32* row = reinterpret_cast<u32*>(reinterpret_cast<u8*>(m_efb.color_cache_buf_map.pData) + row_offset);
	return row[x];
}

float FramebufferManager::GetEFBCachedDepth(u32 x, u32 y)
{
	if (!m_efb.depth_cache_buf_map.pData)
		PopulateEFBDepthCache();

	u32 row_offset = y * m_efb.depth_cache_buf_map.RowPitch;
	float* row = reinterpret_cast<float*>(reinterpret_cast<u8*>(m_efb.depth_cache_buf_map.pData) + row_offset);
	return row[x];
}

void FramebufferManager::SetEFBCachedColor(u32 x, u32 y, u32 value)
{
	if (!m_efb.color_cache_buf_map.pData)
		return;

	u32 row_offset = y * m_efb.color_cache_buf_map.RowPitch;
	u32* row = reinterpret_cast<u32*>(reinterpret_cast<u8*>(m_efb.color_cache_buf_map.pData) + row_offset);
	row[x] = value;
}

void FramebufferManager::SetEFBCachedDepth(u32 x, u32 y, float value)
{
	if (!m_efb.color_cache_buf_map.pData)
		return;

	u32 row_offset = y * m_efb.depth_cache_buf_map.RowPitch;
	float* row = reinterpret_cast<float*>(reinterpret_cast<u8*>(m_efb.depth_cache_buf_map.pData) + row_offset);
	row[x] = value;
}

void FramebufferManager::PopulateEFBColorCache()
{
	_dbg_assert_(!m_efb.color_cache_buf_map.pData, "cache is invalid");

	// for non-1xIR or multisampled cases, we need to copy to an intermediate texture first
	ID3D11Texture2D* src_texture;
	if (g_ActiveConfig.iEFBScale != SCALE_1X || g_ActiveConfig.iMultisamples > 1)
	{
		g_renderer->ResetAPIState();

		CD3D11_RECT src_rect(0, 0, m_target_width, m_target_height);
		CD3D11_VIEWPORT vp(0.0f, 0.0f, EFB_WIDTH, EFB_HEIGHT);
		D3D::context->RSSetViewports(1, &vp);
		D3D::context->OMSetRenderTargets(1, &m_efb.color_cache_tex->GetRTV(), nullptr);
		D3D::SetPointCopySampler();

		D3D::drawShadedTexQuad(m_efb.color_tex->GetSRV(), &src_rect, m_target_width, m_target_height,
			PixelShaderCache::GetColorCopyProgram(true),
			VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(),
			nullptr, 1.0f, 0);

		D3D::context->OMSetRenderTargets(1, &GetEFBColorTexture()->GetRTV(), GetEFBDepthTexture()->GetDSV());
		g_renderer->RestoreAPIState();

		src_texture = m_efb.color_cache_tex->GetTex();
	}
	else
	{
		// can copy directly from efb texture
		src_texture = m_efb.color_tex->GetTex();
	}

	D3D::context->CopySubresourceRegion(m_efb.color_cache_buf, 0, 0, 0, 0, src_texture, 0, nullptr);

	HRESULT hr = D3D::context->Map(m_efb.color_cache_buf, 0, D3D11_MAP_READ_WRITE, 0, &m_efb.color_cache_buf_map);
	CHECK(SUCCEEDED(hr), "failed to map efb peek color cache texture (hr=%08X)", hr);
}

void FramebufferManager::PopulateEFBDepthCache()
{
	_dbg_assert_(!m_efb.depth_cache_buf_map.pData, "cache is invalid");

	// for non-1xIR or multisampled cases, we need to copy to an intermediate texture first
	ID3D11Texture2D* src_texture;
	if (g_ActiveConfig.iEFBScale != SCALE_1X || g_ActiveConfig.iMultisamples > 1)
	{
		g_renderer->ResetAPIState();

		CD3D11_RECT src_rect(0, 0, m_target_width, m_target_height);
		CD3D11_VIEWPORT vp(0.0f, 0.0f, EFB_WIDTH, EFB_HEIGHT);
		D3D::context->RSSetViewports(1, &vp);
		D3D::context->OMSetRenderTargets(1, &m_efb.depth_cache_tex->GetRTV(), nullptr);
		D3D::SetPointCopySampler();

		// MSAA has to go through a different path for depth
		D3D::drawShadedTexQuad(m_efb.depth_tex->GetSRV(), &src_rect, m_target_width, m_target_height,
			(g_ActiveConfig.iMultisamples > 1) ? PixelShaderCache::GetDepthResolveProgram() : PixelShaderCache::GetColorCopyProgram(false),
			VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(),
			nullptr, 1.0f, 0);

		D3D::context->OMSetRenderTargets(1, &GetEFBColorTexture()->GetRTV(), GetEFBDepthTexture()->GetDSV());
		g_renderer->RestoreAPIState();

		src_texture = m_efb.depth_cache_tex->GetTex();
	}
	else
	{
		// can copy directly from efb texture
		src_texture = m_efb.depth_tex->GetTex();
	}

	D3D::context->CopySubresourceRegion(m_efb.depth_cache_buf, 0, 0, 0, 0, src_texture, 0, nullptr);

	HRESULT hr = D3D::context->Map(m_efb.depth_cache_buf, 0, D3D11_MAP_READ_WRITE, 0, &m_efb.depth_cache_buf_map);
	CHECK(SUCCEEDED(hr), "failed to map efb peek depth cache texture (hr=%08X)", hr);
}

void FramebufferManager::InvalidateEFBCache()
{
	if (m_efb.color_cache_buf_map.pData)
	{
		D3D::context->Unmap(m_efb.color_cache_buf, 0);
		m_efb.color_cache_buf_map.pData = nullptr;
	}

	if (m_efb.depth_cache_buf_map.pData)
	{
		D3D::context->Unmap(m_efb.depth_cache_buf, 0);
		m_efb.depth_cache_buf_map.pData = nullptr;
	}
}

}  // namespace DX11
