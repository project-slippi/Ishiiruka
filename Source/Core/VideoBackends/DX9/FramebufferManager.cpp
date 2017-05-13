// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/Memmap.h"

#include "VideoBackends/DX9/D3DBase.h"
#include "VideoBackends/DX9/FramebufferManager.h"
#include "VideoBackends/DX9/PixelShaderCache.h"
#include "VideoBackends/DX9/Render.h"
#include "VideoBackends/DX9/TextureConverter.h"
#include "VideoBackends/DX9/VertexShaderCache.h"

#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace DX9
{

// TODO: this is probably somewhere else
#define SAFE_RELEASE(p) if (p) { (p)->Release(); (p) = NULL; }

#undef CHECK
#define CHECK(hr, Message, ...) if (FAILED(hr)) { PanicAlert(__FUNCTION__ "Failed in %s at line %d: " Message, __FILE__, __LINE__, __VA_ARGS__); }

inline void GetSurface(IDirect3DTexture9* texture, IDirect3DSurface9** surface)
{
	if (!texture) return;
	texture->GetSurfaceLevel(0, surface);
}

FramebufferManager::Efb FramebufferManager::s_efb;
u32 FramebufferManager::m_target_width;
u32 FramebufferManager::m_target_height;

void FramebufferManager::InitializeEFBCache()
{
	// Render buffer for AccessEFB (color data)
	HRESULT hr = D3D::dev->CreateTexture(EFB_WIDTH, EFB_HEIGHT, 1, D3DUSAGE_RENDERTARGET, s_efb.color_surface_Format,
		D3DPOOL_DEFAULT, &s_efb.color_cache_texture, NULL);
	GetSurface(s_efb.color_cache_texture, &s_efb.color_cache_surf);
	CHECK(hr, "Create Color Read Texture (hr=%#x)", hr);

	// AccessEFB - Sysmem buffer used to retrieve the pixel data from color_ReadBuffer
	hr = D3D::dev->CreateOffscreenPlainSurface(EFB_WIDTH, EFB_HEIGHT, s_efb.color_surface_Format, D3DPOOL_SYSTEMMEM, &s_efb.color_cache_buf, NULL);
	CHECK(hr, "Create offscreen color surface (hr=%#x)", hr);
	if (s_efb.depth_textures_supported)
	{
		hr = D3D::dev->CreateTexture(EFB_WIDTH, EFB_HEIGHT, 1, D3DUSAGE_RENDERTARGET, s_efb.depth_cache_Format,
			D3DPOOL_DEFAULT, &s_efb.depth_cache_texture, NULL);
		GetSurface(s_efb.depth_cache_texture, &s_efb.depth_cache_surf);
		CHECK(hr, "Create depth read texture (hr=%#x)", hr);

		// AccessEFB - Sysmem buffer used to retrieve the pixel data from depth_ReadBuffer
		hr = D3D::dev->CreateOffscreenPlainSurface(EFB_WIDTH, EFB_HEIGHT, s_efb.depth_cache_Format, D3DPOOL_SYSTEMMEM, &s_efb.depth_cache_buf, NULL);
		CHECK(hr, "Create depth offscreen surface (hr=%#x)", hr);
	}

}

FramebufferManager::FramebufferManager(u32 target_width, u32 target_height)
{
	m_target_width = std::max(target_width, 16u);
	m_target_height = std::max(target_height, 16u);
	s_efb.depth_textures_supported = true;
	s_efb.color_surface_Format = D3DFMT_A8R8G8B8;

	// EFB color texture - primary render target
	HRESULT hr = D3D::dev->CreateTexture(m_target_width, m_target_height, 1, D3DUSAGE_RENDERTARGET, s_efb.color_surface_Format,
		D3DPOOL_DEFAULT, &s_efb.color_texture, NULL);
	GetSurface(s_efb.color_texture, &s_efb.color_surface);
	CHECK(hr, "Create color texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);



	// Select a Z-buffer texture format with hardware support
	s_efb.depth_surface_Format = D3D::GetSupportedDepthTextureFormat();
	if (s_efb.depth_surface_Format == D3DFMT_UNKNOWN)
	{
		// workaround for Intel GPUs etc: only create a depth _surface_
		s_efb.depth_textures_supported = false;
		s_efb.depth_surface_Format = D3D::GetSupportedDepthSurfaceFormat(s_efb.color_surface_Format);
		ERROR_LOG(VIDEO, "No supported depth texture format found, disabling Z peeks for EFB access.");
	}

	if (s_efb.depth_textures_supported)
	{
		// EFB depth buffer - primary depth buffer
		hr = D3D::dev->CreateTexture(m_target_width, m_target_height, 1, D3DUSAGE_DEPTHSTENCIL, s_efb.depth_surface_Format,
			D3DPOOL_DEFAULT, &s_efb.depth_texture, NULL);
		GetSurface(s_efb.depth_texture, &s_efb.depth_surface);
		CHECK(hr, "Framebuffer depth texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);

		// Render buffer for AccessEFB (depth data)
		D3DFORMAT DepthTexFormats[2];
		DepthTexFormats[0] = D3DFMT_D24X8;
		// This is expected to work on all hardware
		DepthTexFormats[1] = D3DFMT_A8R8G8B8;

		for (int i = 0; i < 2; ++i)
		{
			if (D3D::CheckTextureSupport(D3DUSAGE_RENDERTARGET, DepthTexFormats[i]))
			{
				s_efb.depth_cache_Format = DepthTexFormats[i];
				break;
			}
		}
	}
	else if (s_efb.depth_surface_Format)
	{
		// just create a depth surface
		hr = D3D::dev->CreateDepthStencilSurface(m_target_width, m_target_height, s_efb.depth_surface_Format, D3DMULTISAMPLE_NONE, 0, FALSE, &s_efb.depth_surface, NULL);
		CHECK(hr, "Framebuffer depth surface (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
	}

	// ReinterpretPixelData - EFB color data will be copy-converted to this texture and the buffers are swapped then
	hr = D3D::dev->CreateTexture(m_target_width, m_target_height, 1, D3DUSAGE_RENDERTARGET, s_efb.color_surface_Format,
		D3DPOOL_DEFAULT, &s_efb.color_reinterpret_texture, NULL);
	GetSurface(s_efb.color_reinterpret_texture, &s_efb.color_reinterpret_surface);
	CHECK(hr, "Create color reinterpret texture (size: %dx%d; hr=%#x)", m_target_width, m_target_height, hr);
	InitializeEFBCache();
}

FramebufferManager::~FramebufferManager()
{
	FramebufferManager::InvalidateEFBCache();
	SAFE_RELEASE(s_efb.depth_surface);
	SAFE_RELEASE(s_efb.color_surface);
	SAFE_RELEASE(s_efb.color_cache_surf);
	SAFE_RELEASE(s_efb.depth_cache_surf);
	SAFE_RELEASE(s_efb.color_cache_buf);
	SAFE_RELEASE(s_efb.depth_cache_surf);
	SAFE_RELEASE(s_efb.color_texture);
	SAFE_RELEASE(s_efb.color_cache_texture);
	SAFE_RELEASE(s_efb.depth_texture);
	SAFE_RELEASE(s_efb.depth_cache_texture);
	SAFE_RELEASE(s_efb.color_reinterpret_texture);
	SAFE_RELEASE(s_efb.color_reinterpret_surface);
	s_efb.color_surface_Format = D3DFMT_UNKNOWN;
	s_efb.depth_surface_Format = D3DFMT_UNKNOWN;
	s_efb.depth_cache_Format = D3DFMT_UNKNOWN;
}

std::unique_ptr<XFBSourceBase> FramebufferManager::CreateXFBSource(u32 target_width, u32 target_height, u32 layers)
{
	LPDIRECT3DTEXTURE9 tex;
	D3D::dev->CreateTexture(target_width, target_height, 1, D3DUSAGE_RENDERTARGET,
		s_efb.color_surface_Format, D3DPOOL_DEFAULT, &tex, NULL);

	return std::make_unique<XFBSource>(tex);
}

void FramebufferManager::GetTargetSize(u32 *width, u32 *height)
{
	*width = m_target_width;
	*height = m_target_height;
}

void XFBSource::Draw(const MathUtil::Rectangle<float> &sourcerc,
	const MathUtil::Rectangle<float> &drawrc, int width, int height) const
{
	int multisamplemode = g_ActiveConfig.iMultisamples - 1;
	if (multisamplemode == 0 && g_ActiveConfig.bUseScalingFilter)
	{
		multisamplemode = std::max(std::min((int)(sourcerc.GetWidth() / drawrc.GetWidth()) - 1, 2), 0);
	}
	D3D::drawShadedTexSubQuad(texture, &sourcerc, texWidth, texHeight, &drawrc, width, height,
		PixelShaderCache::GetColorCopyProgram(multisamplemode), VertexShaderCache::GetSimpleVertexShader(multisamplemode));
}

void XFBSource::DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight)
{
	TextureConverter::DecodeToTexture(xfbAddr, fbWidth, fbHeight, texture);
}

void FramebufferManager::CopyToRealXFB(u32 xfbAddr, u32 fbStride, u32 fbHeight, const EFBRectangle& sourceRc, float Gamma)
{
	u8* xfb_in_ram = Memory::GetPointer(xfbAddr);
	if (!xfb_in_ram)
	{
		WARN_LOG(VIDEO, "Tried to copy to invalid XFB address");
		return;
	}

	TargetRectangle targetRc = g_renderer->ConvertEFBRectangle(sourceRc);
	std::swap(targetRc.top, targetRc.bottom);
	TextureConverter::EncodeToRamYUYV(GetEFBColorTexture(), targetRc, xfb_in_ram, sourceRc.GetWidth(), fbStride, fbHeight, Gamma);
}

void XFBSource::CopyEFB(float Gamma)
{
	g_renderer->ResetAPIState(); // reset any game specific settings

	// Copy EFB data to XFB and restore render target again
	LPDIRECT3DSURFACE9 Rendersurf = NULL;
	texture->GetSurfaceLevel(0, &Rendersurf);
	D3D::dev->SetDepthStencilSurface(NULL);
	D3D::dev->SetRenderTarget(0, Rendersurf);

	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width = texWidth;
	vp.Height = texHeight;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);

	D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	D3D::ChangeSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);

	D3D::drawShadedTexQuad(
		FramebufferManager::GetEFBColorTexture(),
		nullptr,
		g_renderer->GetTargetWidth(),
		g_renderer->GetTargetHeight(),
		texWidth,
		texHeight,
		PixelShaderCache::GetColorCopyProgram(0),
		VertexShaderCache::GetSimpleVertexShader(0),
		Gamma);

	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);
	D3D::RefreshSamplerState(0, D3DSAMP_MAGFILTER);
	D3D::SetTexture(0, NULL);
	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());

	Rendersurf->Release();

	g_renderer->RestoreAPIState();
}

u32 FramebufferManager::GetEFBCachedDepth(u32 x, u32 y)
{
	if (!s_efb.depth_lock_rect.pBits)
		PopulateEFBDepthCache();

	u32 row_offset = y * s_efb.depth_lock_rect.Pitch;
	u32* row = reinterpret_cast<u32*>(reinterpret_cast<u8*>(s_efb.depth_lock_rect.pBits) + row_offset);
	return row[x];
}

u32 FramebufferManager::GetEFBCachedColor(u32 x, u32 y)
{
	if (!s_efb.color_lock_rect.pBits)
		PopulateEFBColorCache();

	u32 row_offset = y * s_efb.color_lock_rect.Pitch;
	u32* row = reinterpret_cast<u32*>(reinterpret_cast<u8*>(s_efb.color_lock_rect.pBits) + row_offset);
	return row[x];
}

void FramebufferManager::SetEFBCachedColor(u32 x, u32 y, u32 value)
{
	if (!s_efb.color_lock_rect.pBits)
		return;

	u32 row_offset = y * s_efb.color_lock_rect.Pitch;
	u32* row = reinterpret_cast<u32*>(reinterpret_cast<u8*>(s_efb.color_lock_rect.pBits) + row_offset);
	row[x] = value;
}

void FramebufferManager::SetEFBCachedDepth(u32 x, u32 y, u32 value)
{
	if (!s_efb.depth_lock_rect.pBits)
		PopulateEFBDepthCache();

	u32 row_offset = y * s_efb.depth_lock_rect.Pitch;
	u32* row = reinterpret_cast<u32*>(reinterpret_cast<u8*>(s_efb.depth_lock_rect.pBits) + row_offset);
	row[x] = value;
}

void FramebufferManager::PopulateEFBColorCache()
{
	_dbg_assert_(!s_efb.color_lock_rect.pBits, "cache is invalid");

	// We can't directly StretchRect to System buf because is not supported by all implementations
	// this is the only safe path that works in most cases
	HRESULT hr = D3D::dev->StretchRect(s_efb.color_surface, nullptr, s_efb.color_cache_surf, nullptr, D3DTEXF_LINEAR);
	CHECK(SUCCEEDED(hr), "failed to stretch efb peek color cache texture (hr=%08X)", hr);
	hr = D3D::dev->GetRenderTargetData(s_efb.color_cache_surf, s_efb.color_cache_buf);
	CHECK(SUCCEEDED(hr), "failed to get data from efb peek color cache texture (hr=%08X)", hr);
	hr = s_efb.color_cache_buf->LockRect(&s_efb.color_lock_rect, nullptr, D3DLOCK_READONLY);
	CHECK(SUCCEEDED(hr), "failed to map efb peek color cache texture (hr=%08X)", hr);
}

void FramebufferManager::PopulateEFBDepthCache()
{
	_dbg_assert_(!s_efb.dept_lock_rect.pBits, "cache is invalid");

	g_renderer->ResetAPIState(); // Reset any game specific settings
	D3D::dev->SetDepthStencilSurface(NULL);
	D3D::dev->SetRenderTarget(0, s_efb.depth_cache_surf);

	// Stretch picture with increased internal resolution
	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width = EFB_WIDTH;
	vp.Height = EFB_HEIGHT;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);

	float colmat[28] = { 0.0f };
	colmat[0] = colmat[5] = colmat[10] = 1.0f;
	PixelShaderManager::SetColorMatrix(colmat); // set transformation
	D3D::dev->SetPixelShaderConstantF(C_COLORMATRIX, PixelShaderManager::GetBuffer(), 7);
	LPDIRECT3DTEXTURE9 read_texture = FramebufferManager::GetEFBDepthTexture();

	D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);

	D3DFORMAT bformat = FramebufferManager::GetEFBDepthRTSurfaceFormat();

	D3D::drawShadedTexQuad(
		read_texture,
		nullptr,
		g_renderer->GetTargetWidth(),
		g_renderer->GetTargetHeight(),
		EFB_WIDTH, EFB_HEIGHT,
		PixelShaderCache::GetDepthMatrixProgram(0, bformat != FOURCC_RAWZ),
		VertexShaderCache::GetSimpleVertexShader(0));

	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);

	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
	g_renderer->RestoreAPIState();

	// Retrieve the pixel data to the local memory buffer
	HRESULT hr = D3D::dev->GetRenderTargetData(s_efb.depth_cache_surf, s_efb.depth_cache_buf);

	// EFB data successfully retrieved, now get the pixel data
	hr = s_efb.depth_cache_buf->LockRect(&s_efb.depth_lock_rect, nullptr, D3DLOCK_READONLY);
	CHECK(SUCCEEDED(hr), "failed to map efb peek depth cache texture (hr=%08X)", hr);
}

void FramebufferManager::InvalidateEFBCache()
{
	if (s_efb.color_lock_rect.pBits)
	{
		s_efb.color_cache_buf->UnlockRect();
		s_efb.color_lock_rect.pBits = nullptr;
	}

	if (s_efb.depth_lock_rect.pBits)
	{
		s_efb.depth_cache_buf->UnlockRect();
		s_efb.depth_lock_rect.pBits = nullptr;
	}
}

}  // namespace DX9
