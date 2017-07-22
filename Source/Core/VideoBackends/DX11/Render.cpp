// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <cmath>
#include <memory>
#include <string>
#include <strsafe.h>
#include <array>
#include <tuple>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/MathUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/Movie.h"

#include "VideoBackends/DX11/BoundingBox.h"
#include "VideoBackends/DX11/D3DBase.h"
#include "VideoBackends/DX11/D3DPtr.h"
#include "VideoBackends/DX11/D3DState.h"
#include "VideoBackends/DX11/D3DUtil.h"
#include "VideoBackends/DX11/FramebufferManager.h"
#include "VideoBackends/DX11/PostProcessing.h"

#include "VideoBackends/DX11/PixelShaderCache.h"
#include "VideoBackends/DX11/GeometryShaderCache.h"
#include "VideoBackends/DX11/HullDomainShaderCache.h"
#include "VideoBackends/DX11/Render.h"
#include "VideoBackends/DX11/Television.h"
#include "VideoBackends/DX11/TextureCache.h"
#include "VideoBackends/DX11/VertexShaderCache.h"

#include "VideoCommon/AVIDump.h"
#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/SamplerCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{

static Television s_television;

D3D::BlendStatePtr clearblendstates[4];
D3D::DepthStencilStatePtr cleardepthstates[3];
D3D::BlendStatePtr resetblendstate;
D3D::DepthStencilStatePtr resetdepthstate;
D3D::RasterizerStatePtr resetraststate;

// Nvidia stereo blitting struct defined in "nvstereo.h" from the Nvidia SDK
typedef struct _Nv_Stereo_Image_Header
{
	unsigned int    dwSignature;
	unsigned int    dwWidth;
	unsigned int    dwHeight;
	unsigned int    dwBPP;
	unsigned int    dwFlags;
} NVSTEREOIMAGEHEADER, *LPNVSTEREOIMAGEHEADER;

#define NVSTEREO_IMAGE_SIGNATURE 0x4433564e

// GX pipeline state
struct
{
	SamplerState sampler[8];
	BlendState blend;
	DepthState zmode;
	RasterizerState raster;

} gx_state;

StateCache gx_state_cache;

void Renderer::SetupDeviceObjects()
{
	s_television.Init();

	g_framebuffer_manager = std::make_unique<FramebufferManager>(m_target_width, m_target_height);
	HRESULT hr;

	D3D11_DEPTH_STENCIL_DESC ddesc;
	ddesc.DepthEnable = FALSE;
	ddesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	ddesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	ddesc.StencilEnable = FALSE;
	ddesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	ddesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	hr = D3D::device->CreateDepthStencilState(&ddesc, D3D::ToAddr(cleardepthstates[0]));
	CHECK(hr == S_OK, "Create depth state for Renderer::ClearScreen");
	ddesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	ddesc.DepthEnable = TRUE;
	hr = D3D::device->CreateDepthStencilState(&ddesc, D3D::ToAddr(cleardepthstates[1]));
	CHECK(hr == S_OK, "Create depth state for Renderer::ClearScreen");
	ddesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	hr = D3D::device->CreateDepthStencilState(&ddesc, D3D::ToAddr(cleardepthstates[2]));
	CHECK(hr == S_OK, "Create depth state for Renderer::ClearScreen");
	D3D::SetDebugObjectName(cleardepthstates[0].get(), "depth state for Renderer::ClearScreen (depth buffer disabled)");
	D3D::SetDebugObjectName(cleardepthstates[1].get(), "depth state for Renderer::ClearScreen (depth buffer enabled, writing enabled)");
	D3D::SetDebugObjectName(cleardepthstates[2].get(), "depth state for Renderer::ClearScreen (depth buffer enabled, writing disabled)");

	D3D11_BLEND_DESC blenddesc;
	blenddesc.AlphaToCoverageEnable = FALSE;
	blenddesc.IndependentBlendEnable = FALSE;
	blenddesc.RenderTarget[0].BlendEnable = FALSE;
	blenddesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	blenddesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blenddesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	blenddesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blenddesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blenddesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blenddesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	hr = D3D::device->CreateBlendState(&blenddesc, D3D::ToAddr(resetblendstate));
	CHECK(hr == S_OK, "Create blend state for Renderer::ResetAPIState");
	D3D::SetDebugObjectName(resetblendstate.get(), "blend state for Renderer::ResetAPIState");

	clearblendstates[0] = resetblendstate.Share();

	blenddesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
	hr = D3D::device->CreateBlendState(&blenddesc, D3D::ToAddr(clearblendstates[1]));
	CHECK(hr == S_OK, "Create blend state for Renderer::ClearScreen");

	blenddesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALPHA;
	hr = D3D::device->CreateBlendState(&blenddesc, D3D::ToAddr(clearblendstates[2]));
	CHECK(hr == S_OK, "Create blend state for Renderer::ClearScreen");

	blenddesc.RenderTarget[0].RenderTargetWriteMask = 0;
	hr = D3D::device->CreateBlendState(&blenddesc, D3D::ToAddr(clearblendstates[3]));
	CHECK(hr == S_OK, "Create blend state for Renderer::ClearScreen");

	ddesc.DepthEnable = FALSE;
	ddesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	ddesc.DepthFunc = D3D11_COMPARISON_GREATER;
	ddesc.StencilEnable = FALSE;
	ddesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	ddesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	hr = D3D::device->CreateDepthStencilState(&ddesc, D3D::ToAddr(resetdepthstate));
	CHECK(hr == S_OK, "Create depth state for Renderer::ResetAPIState");
	D3D::SetDebugObjectName(resetdepthstate.get(), "depth stencil state for Renderer::ResetAPIState");

	D3D11_RASTERIZER_DESC rastdesc = CD3D11_RASTERIZER_DESC(D3D11_FILL_SOLID, D3D11_CULL_NONE, false, 0, 0.f, 0.f, false, false, false, false);
	hr = D3D::device->CreateRasterizerState(&rastdesc, D3D::ToAddr(resetraststate));
	CHECK(hr == S_OK, "Create rasterizer state for Renderer::ResetAPIState");
	D3D::SetDebugObjectName(resetraststate.get(), "rasterizer state for Renderer::ResetAPIState");
}

// Kill off all device objects
static void TeardownDeviceObjects()
{
	g_framebuffer_manager.reset();

	clearblendstates[0].reset();
	clearblendstates[1].reset();
	clearblendstates[2].reset();
	clearblendstates[3].reset();
	cleardepthstates[0].reset();
	cleardepthstates[1].reset();
	cleardepthstates[2].reset();
	resetblendstate.reset();
	resetdepthstate.reset();
	resetraststate.reset();
	s_television.Shutdown();

	gx_state_cache.Clear();
}

static D3D11_BOX GetScreenshotSourceBox(const TargetRectangle& targetRc, u32 width, u32 height)
{
	// Since the screenshot buffer is copied back to the CPU via Map(), we can't access pixels that
	// fall outside the backbuffer bounds. Therefore, when crop is enabled and the target rect is
	// off-screen to the top/left, we clamp the origin at zero, as well as the bottom/right
	// coordinates at the backbuffer dimensions. This will result in a rectangle that can be
	// smaller than the backbuffer, but never larger.
	return CD3D11_BOX(
		std::max(targetRc.left, 0),
		std::max(targetRc.top, 0),
		0,
		std::min(width, (unsigned int)targetRc.right),
		std::min(height, (unsigned int)targetRc.bottom),
		1);
}

void Renderer::Create3DVisionTexture(u32 width, u32 height)
{
	if (width == m_3d_vision_texture_width
		&& height == m_3d_vision_texture_height)
	{
		return;
	}
	if (m_3d_vision_texture)
	{
		m_3d_vision_texture->Release();
	}
	m_3d_vision_texture_width = width;
	m_3d_vision_texture_height = height;
	// Create a staging texture for 3D vision with signature information in the last row.
	// Nvidia 3D Vision supports full SBS, so there is no loss in resolution during this process.
	D3D11_SUBRESOURCE_DATA sysData;
	sysData.SysMemPitch = 4 * width * 2;
	std::vector<u8> data((height + 1) * sysData.SysMemPitch);
	sysData.pSysMem = data.data();
	LPNVSTEREOIMAGEHEADER header = (LPNVSTEREOIMAGEHEADER)((u8*)sysData.pSysMem + height * sysData.SysMemPitch);
	header->dwSignature = NVSTEREO_IMAGE_SIGNATURE;
	header->dwWidth = width * 2;
	header->dwHeight = height;
	header->dwBPP = 32;
	header->dwFlags = 0;
	m_3d_vision_texture = D3DTexture2D::Create(width * 2, height + 1, D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, &sysData);
}

void Renderer::PrepareFrameDumpRenderTexture(u32 width, u32 height)
{
	if (width == m_frame_dump_render_texture_width
		&& height == m_frame_dump_render_texture_height)
	{
		return;
	}
	if (m_frame_dump_render_texture)
	{
		m_frame_dump_render_texture->Release();
		m_frame_dump_render_texture = nullptr;
	}
	m_frame_dump_render_texture_width = width;
	m_frame_dump_render_texture_height = height;
	m_frame_dump_render_texture = D3DTexture2D::Create(width, height,
		(D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE),
		D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1);
}
void Renderer::PrepareFrameDumpBuffer(u32 width, u32 height)
{
	if (width == m_frame_dump_staging_texture_width
		&& height == m_frame_dump_staging_texture_height)
	{
		return;
	}
	m_frame_dump_staging_texture.reset();
	// We can't render anything outside of the backbuffer anyway, so use the backbuffer size as the screenshot buffer size.
	// This texture is released to be recreated when the window is resized in Renderer::SwapImpl.
	D3D11_TEXTURE2D_DESC scrtex_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE);
	HRESULT hr = D3D::device->CreateTexture2D(&scrtex_desc, nullptr, D3D::ToAddr(m_frame_dump_staging_texture));
	CHECK(hr == S_OK, "Create screenshot staging texture");
	D3D::SetDebugObjectName(m_frame_dump_staging_texture.get(), "staging screenshot texture");
}
Renderer::Renderer(void *&window_handle)
{
	D3D::Create((HWND)window_handle);
	
	m_backbuffer_width = D3D::GetBackBufferWidth();
	m_backbuffer_height = D3D::GetBackBufferHeight();

	m_last_multisamples = g_ActiveConfig.iMultisamples;
	m_last_efb_scale = g_ActiveConfig.iEFBScale;
	m_last_stereo_mode = g_ActiveConfig.iStereoMode;
	m_last_xfb_mode = g_ActiveConfig.bUseRealXFB;
	

	// Setup GX pipeline state
	gx_state.blend.blend_enable = false;
	gx_state.blend.write_mask = D3D11_COLOR_WRITE_ENABLE_ALL;
	gx_state.blend.src_blend = D3D11_BLEND_ONE;
	gx_state.blend.dst_blend = D3D11_BLEND_ZERO;
	gx_state.blend.blend_op = D3D11_BLEND_OP_ADD;
	gx_state.blend.use_dst_alpha = false;

	for (unsigned int k = 0; k < 8; k++)
	{
		gx_state.sampler[k].packed = 0;
	}

	gx_state.zmode.testenable = false;
	gx_state.zmode.updateenable = false;
	gx_state.zmode.func = ZMode::NEVER;
	gx_state.zmode.reversed_depth = false;

	gx_state.raster.cull_mode = D3D11_CULL_NONE;

	m_3d_vision_texture = nullptr;
	m_frame_dump_render_texture = nullptr;
	m_frame_dump_staging_texture.reset();
}

void Renderer::Init()
{
	UpdateDrawRectangle();
	FramebufferManagerBase::SetLastXfbWidth(MAX_XFB_WIDTH);
	FramebufferManagerBase::SetLastXfbHeight(MAX_XFB_HEIGHT);
	CalculateTargetSize();
	PixelShaderManager::SetEfbScaleChanged();
	SetupDeviceObjects();

	m_post_processor = std::make_unique<D3DPostProcessor>();
	if (!m_post_processor->Initialize())
		PanicAlert("D3D: Failed to initialize post processor.");
	
	// Clear EFB textures
	float ClearColor[4] = { 0.f, 0.f, 0.f, 1.f };
	D3D::context->ClearRenderTargetView(FramebufferManager::GetEFBColorTexture()->GetRTV(), ClearColor);
	D3D::context->ClearDepthStencilView(FramebufferManager::GetEFBDepthTexture()->GetDSV(), D3D11_CLEAR_DEPTH, 1.f, 0);

	D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)m_target_width, (float)m_target_height);
	D3D::context->RSSetViewports(1, &vp);
	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());
	D3D::BeginFrame();
}

Renderer::~Renderer()
{
	m_post_processor.reset();
	TeardownDeviceObjects();
	if (m_3d_vision_texture)
	{
		m_3d_vision_texture->Release();
	}
	if (m_frame_dump_render_texture)
	{
		m_frame_dump_render_texture->Release();
	}
	m_3d_vision_texture = nullptr;
	m_frame_dump_render_texture = nullptr;
	m_frame_dump_staging_texture.reset();
	D3D::EndFrame();
	D3D::Present();
	D3D::Close();
}

void Renderer::RenderText(const std::string& text, int left, int top, u32 color)
{
	TargetRectangle trc = GetTargetRectangle();
	const int nBackbufferWidth = trc.right - trc.left;
	const int nBackbufferHeight = trc.bottom - trc.top;

	float scalex = 1 / (float)nBackbufferWidth * 2.f;
	float scaley = 1 / (float)nBackbufferHeight * 2.f;

	D3D::font.DrawTextScaled((float)left + 1, (float)top + 1, 20.f, 0.0f, color & 0xFF000000, text.c_str(), scalex, scaley);
	D3D::font.DrawTextScaled((float)left, (float)top, 20.f, 0.0f, color, text.c_str(), scalex, scaley);
}

TargetRectangle Renderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
	TargetRectangle result;
	result.left = EFBToScaledX(rc.left);
	result.top = EFBToScaledY(rc.top);
	result.right = EFBToScaledX(rc.right);
	result.bottom = EFBToScaledY(rc.bottom);
	return result;
}

// With D3D, we have to resize the backbuffer if the window changed
// size.
bool Renderer::CheckForResize()
{
	RECT rcWindow;
	GetClientRect(D3D::hWnd, &rcWindow);
	int client_width = rcWindow.right - rcWindow.left;
	int client_height = rcWindow.bottom - rcWindow.top;

	// Get the top-left corner of the client area in screen coordinates
	POINT originPoint = { 0, 0 };
	ClientToScreen(D3D::hWnd, &originPoint);
	g_renderer->SetWindowRectangle(originPoint.x, originPoint.x + client_width, originPoint.y, originPoint.y + client_height);

	// Sanity check
	if ((client_width != g_renderer->GetBackbufferWidth() ||
		client_height != g_renderer->GetBackbufferHeight()) &&
		client_width >= 4 && client_height >= 4)
	{
		return true;
	}

	return false;
}

void Renderer::SetScissorRect(const EFBRectangle& rc)
{
	D3D::context->RSSetScissorRects(1, ConvertEFBRectangle(rc).AsRECT());
}

void Renderer::SetColorMask()
{
	// Only enable alpha channel if it's supported by the current EFB format
	UINT8 color_mask = 0;
	if (bpmem.alpha_test.TestResult() != AlphaTest::FAIL)
	{
		if (bpmem.blendmode.alphaupdate && (bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24))
			color_mask = D3D11_COLOR_WRITE_ENABLE_ALPHA;
		if (bpmem.blendmode.colorupdate)
			color_mask |= D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
	}
	gx_state.blend.write_mask = color_mask;
}

// This function allows the CPU to directly access the EFB.
// There are EFB peeks (which will read the color or depth of a pixel)
// and EFB pokes (which will change the color or depth of a pixel).
//
// The behavior of EFB peeks can only be modified by:
//	- GX_PokeAlphaRead
// The behavior of EFB pokes can be modified by:
//	- GX_PokeAlphaMode (TODO)
//	- GX_PokeAlphaUpdate (TODO)
//	- GX_PokeBlendMode (TODO)
//	- GX_PokeColorUpdate (TODO)
//	- GX_PokeDither (TODO)
//	- GX_PokeDstAlpha (TODO)
//	- GX_PokeZMode (TODO)
u32 Renderer::AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data)
{
	if (type == EFBAccessType::PeekZ)
	{
		float val = FramebufferManager::GetEFBCachedDepth(x, y);
		// depth buffer is inverted in the d3d backend
		val = 1.0f - val;
		u32 ret = 0;
		if (bpmem.zcontrol.pixel_format == PEControl::RGB565_Z16)
		{
			// if Z is in 16 bit format you must return a 16 bit integer
			ret = MathUtil::Clamp<u32>((u32)(val * 65536.0f), 0, 0xFFFF);
		}
		else
		{
			ret = MathUtil::Clamp<u32>((u32)(val * 16777216.0f), 0, 0xFFFFFF);
		}
		return ret;
	}
	else if (type == EFBAccessType::PeekColor)
	{
		u32 ret = FramebufferManager::GetEFBCachedColor(x, y);
		// our internal buffers are RGBA, yet a BGRA value is expected
		ret = RGBA8ToBGRA8(ret);

		// check what to do with the alpha channel (GX_PokeAlphaRead)
		PixelEngine::UPEAlphaReadReg alpha_read_mode = PixelEngine::GetAlphaReadMode();

		if (bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24)
		{
			ret = RGBA8ToRGBA6ToRGBA8(ret);
		}
		else if (bpmem.zcontrol.pixel_format == PEControl::RGB565_Z16)
		{
			ret = RGBA8ToRGB565ToRGBA8(ret);
		}
		if (bpmem.zcontrol.pixel_format != PEControl::RGBA6_Z24)
		{
			ret |= 0xFF000000;
		}

		if (alpha_read_mode.ReadMode == 2) return ret; // GX_READ_NONE
		else if (alpha_read_mode.ReadMode == 1) return (ret | 0xFF000000); // GX_READ_FF
		else /*if(alpha_read_mode.ReadMode == 0)*/ return (ret & 0x00FFFFFF); // GX_READ_00
	}
	return 0;
}

void Renderer::PokeEFB(EFBAccessType type, const EfbPokeData* data, size_t num_points)
{
	ResetAPIState();

	D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.0f, 0.0f, (float)GetTargetWidth(), (float)GetTargetHeight());
	D3D::context->RSSetViewports(1, &vp);

	if (type == EFBAccessType::PokeColor)
	{
		D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), nullptr);
	}
	else // if (type == POKE_Z)
	{
		D3D::stateman->PushBlendState(clearblendstates[3].get());
		D3D::stateman->PushDepthState(cleardepthstates[1].get());

		D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(),
			FramebufferManager::GetEFBDepthTexture()->GetDSV());
	}

	D3D::DrawEFBPokeQuads(type, data, num_points);

	if (type == EFBAccessType::PokeZ)
	{
		D3D::stateman->PopDepthState();
		D3D::stateman->PopBlendState();
	}

	RestoreAPIState();
}

// Called from VertexShaderManager
void Renderer::SetViewport()
{
	// reversed gxsetviewport(xorig, yorig, width, height, nearz, farz)
	// [0] = width/2
	// [1] = height/2
	// [2] = 16777215 * (farz - nearz)
	// [3] = xorig + width/2 + 342
	// [4] = yorig + height/2 + 342
	// [5] = 16777215 * farz

	// D3D crashes for zero viewports
	if (xfmem.viewport.wd == 0 || xfmem.viewport.ht == 0)
		return;

	int scissorXOff = bpmem.scissorOffset.x * 2;
	int scissorYOff = bpmem.scissorOffset.y * 2;

	float X = Renderer::EFBToScaledXf(xfmem.viewport.xOrig - xfmem.viewport.wd - scissorXOff);
	float Y = Renderer::EFBToScaledYf(xfmem.viewport.yOrig + xfmem.viewport.ht - scissorYOff);
	float Wd = Renderer::EFBToScaledXf(2.0f * xfmem.viewport.wd);
	float Ht = Renderer::EFBToScaledYf(-2.0f * xfmem.viewport.ht);
	float range = MathUtil::Clamp<float>(xfmem.viewport.zRange, 0.0f, 16777215.0f);
	float min_depth =
		MathUtil::Clamp<float>(xfmem.viewport.farZ - range, 0.0f, 16777215.0f) / 16777216.0f;
	float max_depth = MathUtil::Clamp<float>(xfmem.viewport.farZ, 0.0f, 16777215.0f) / 16777216.0f;

	if (Wd < 0.0f)
	{
		X += Wd;
		Wd = -Wd;
	}
	if (Ht < 0.0f)
	{
		Y += Ht;
		Ht = -Ht;
	}
	
	// If an inverted depth range is used, which D3D doesn't support,
	// we need to calculate the depth range in the vertex shader.
	if (xfmem.viewport.zRange < 0.0f)
	{
		min_depth = 0.0f;
		max_depth = GX_MAX_DEPTH;
	}

	// In D3D, the viewport rectangle must fit within the render target.
	X = (X >= 0.f) ? X : 0.f;
	Y = (Y >= 0.f) ? Y : 0.f;
	Wd = (X + Wd <= GetTargetWidth()) ? Wd : (GetTargetWidth() - X);
	Ht = (Y + Ht <= GetTargetHeight()) ? Ht : (GetTargetHeight() - Y);

	// We use an inverted depth range here to apply the Reverse Z trick.
	// This trick makes sure we match the precision provided by the 1:0
	// clipping depth range on the hardware.
	D3D11_VIEWPORT vp = CD3D11_VIEWPORT(X, Y, Wd, Ht, 1.0f - max_depth, 1.0f - min_depth);
	D3D::context->RSSetViewports(1, &vp);
	gx_state.zmode.reversed_depth = xfmem.viewport.zRange < 0;
}

void Renderer::ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z)
{
	ResetAPIState();

	if (colorEnable && alphaEnable) D3D::stateman->PushBlendState(clearblendstates[0].get());
	else if (colorEnable) D3D::stateman->PushBlendState(clearblendstates[1].get());
	else if (alphaEnable) D3D::stateman->PushBlendState(clearblendstates[2].get());
	else D3D::stateman->PushBlendState(clearblendstates[3].get());

	// TODO: Should we enable Z testing here?
	/*if (!bpmem.zmode.testenable) D3D::stateman->PushDepthState(cleardepthstates[0]);
	else */if (zEnable) D3D::stateman->PushDepthState(cleardepthstates[1].get());
	else /*if (!zEnable)*/ D3D::stateman->PushDepthState(cleardepthstates[2].get());

	// Update the view port for clearing the picture
	TargetRectangle targetRc = Renderer::ConvertEFBRectangle(rc);
	D3D11_VIEWPORT vp = CD3D11_VIEWPORT((float)targetRc.left, (float)targetRc.top, (float)targetRc.GetWidth(), (float)targetRc.GetHeight(), 0.f, 1.f);
	D3D::context->RSSetViewports(1, &vp);

	// Color is passed in bgra mode so we need to convert it to rgba
	u32 rgbaColor = (color & 0xFF00FF00) | ((color >> 16) & 0xFF) | ((color << 16) & 0xFF0000);
	if (xfmem.viewport.zRange < 0)
	{
		D3D::drawClearQuad(rgbaColor, (z & 0xFFFFFF) / 16777216.0f);
	}
	else
	{
		D3D::drawClearQuad(rgbaColor, 1.0f - ((z & 0xFFFFFF) / 16777216.0f));
	}

	D3D::stateman->PopDepthState();
	D3D::stateman->PopBlendState();

	RestoreAPIState();

	FramebufferManager::InvalidateEFBCache();
}

void Renderer::ReinterpretPixelData(unsigned int convtype)
{
	// TODO: MSAA support..
	D3D11_RECT source = CD3D11_RECT(0, 0, GetTargetWidth(), GetTargetHeight());

	ID3D11PixelShader* pixel_shader;
	if (convtype == 0) pixel_shader = PixelShaderCache::ReinterpRGB8ToRGBA6(true);
	else if (convtype == 2) pixel_shader = PixelShaderCache::ReinterpRGBA6ToRGB8(true);
	else
	{
		ERROR_LOG(VIDEO, "Trying to reinterpret pixel data with unsupported conversion type %d", convtype);
		return;
	}

	// convert data and set the target texture as our new EFB
	ResetAPIState();
	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTempTexture()->GetRTV(), nullptr);
	D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)GetTargetWidth(), (float)GetTargetHeight());
	D3D::context->RSSetViewports(1, &vp);

	D3D::SetPointCopySampler();
	D3D::drawShadedTexQuad(
		FramebufferManager::GetEFBColorTexture()->GetSRV(),
		&source,
		GetTargetWidth(),
		GetTargetHeight(),
		pixel_shader,
		VertexShaderCache::GetSimpleVertexShader(),
		VertexShaderCache::GetSimpleInputLayout());

	RestoreAPIState();

	FramebufferManager::SwapReinterpretTexture();
	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());

	FramebufferManager::InvalidateEFBCache();
}

void Renderer::SetBlendMode(bool forceUpdate)
{
	// Our render target always uses an alpha channel, so we need to override the blend functions to assume a destination alpha of 1 if the render target isn't supposed to have an alpha channel
	// Example: D3DBLEND_DESTALPHA needs to be D3DBLEND_ONE since the result without an alpha channel is assumed to always be 1.
	bool target_has_alpha = bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24;
	const D3D11_BLEND d3dSrcFactors[8] =
	{
		D3D11_BLEND_ZERO,
		D3D11_BLEND_ONE,
		D3D11_BLEND_DEST_COLOR,
		D3D11_BLEND_INV_DEST_COLOR,
		D3D11_BLEND_SRC_ALPHA,
		D3D11_BLEND_INV_SRC_ALPHA, // NOTE: Use SRC1_ALPHA if dst alpha is enabled!
		(target_has_alpha) ? D3D11_BLEND_DEST_ALPHA : D3D11_BLEND_ONE,
		(target_has_alpha) ? D3D11_BLEND_INV_DEST_ALPHA : D3D11_BLEND_ZERO
	};
	const D3D11_BLEND d3dDestFactors[8] =
	{
		D3D11_BLEND_ZERO,
		D3D11_BLEND_ONE,
		D3D11_BLEND_SRC_COLOR,
		D3D11_BLEND_INV_SRC_COLOR,
		D3D11_BLEND_SRC_ALPHA,
		D3D11_BLEND_INV_SRC_ALPHA, // NOTE: Use SRC1_ALPHA if dst alpha is enabled!
		(target_has_alpha) ? D3D11_BLEND_DEST_ALPHA : D3D11_BLEND_ONE,
		(target_has_alpha) ? D3D11_BLEND_INV_DEST_ALPHA : D3D11_BLEND_ZERO
	};

	if (bpmem.blendmode.logicopenable && !bpmem.blendmode.blendenable && !forceUpdate)
		return;

	if (bpmem.blendmode.subtract)
	{
		gx_state.blend.blend_enable = true;
		gx_state.blend.blend_op = D3D11_BLEND_OP_REV_SUBTRACT;
		gx_state.blend.src_blend = D3D11_BLEND_ONE;
		gx_state.blend.dst_blend = D3D11_BLEND_ONE;
	}
	else
	{
		gx_state.blend.blend_enable = (u32)bpmem.blendmode.blendenable;
		if (bpmem.blendmode.blendenable)
		{
			gx_state.blend.blend_op = D3D11_BLEND_OP_ADD;
			gx_state.blend.src_blend = d3dSrcFactors[bpmem.blendmode.srcfactor];
			gx_state.blend.dst_blend = d3dDestFactors[bpmem.blendmode.dstfactor];
		}
	}
}

// This function has the final picture. We adjust the aspect ratio here.
void Renderer::SwapImpl(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, const EFBRectangle& rc, u64 ticks, float Gamma)
{
	if ((!m_xfb_written && !g_ActiveConfig.RealXFBEnabled()) || !fbWidth || !fbHeight)
	{
		Core::Callback_VideoCopiedToXFB(false);
		return;
	}

	u32 xfbCount = 0;
	const XFBSourceBase* const* xfbSourceList = FramebufferManager::GetXFBSource(xfbAddr, fbStride, fbHeight, &xfbCount);
	if ((!xfbSourceList || xfbCount == 0) && g_ActiveConfig.bUseXFB && !g_ActiveConfig.bUseRealXFB)
	{
		Core::Callback_VideoCopiedToXFB(false);
		return;
	}
	if (!g_ActiveConfig.bUseXFB)
		m_post_processor->OnEndFrame();
	ResetAPIState();
	// Prepare to copy the XFBs to our backbuffer
	UpdateDrawRectangle();
	const TargetRectangle targetRc = GetTargetRectangle();

	D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), nullptr);
	float ClearColor[4] = { 0.f, 0.f, 0.f, 1.f };
	D3D::context->ClearRenderTargetView(D3D::GetBackBuffer()->GetRTV(), ClearColor);

	// Copy the framebuffer to screen.	
	const TargetSize dst_size = { m_backbuffer_width, m_backbuffer_height };
	DrawFrame(targetRc, rc, xfbAddr, xfbSourceList, xfbCount, D3D::GetBackBuffer(), dst_size, fbWidth, fbStride, fbHeight, Gamma);

	// Dump frames
	if (IsFrameDumping())
	{
		DumpFrame(rc, xfbAddr, xfbSourceList, xfbCount, fbWidth, fbStride, fbHeight, ticks);
	}

	Renderer::DrawDebugText();
	OSD::DrawMessages();
	D3D::EndFrame();

	g_texture_cache->Cleanup(frameCount);

	// Enable configuration changes
	UpdateActiveConfig();
	g_texture_cache->OnConfigChanged(g_ActiveConfig);

	SetWindowSize(fbStride, fbHeight);

	const bool windowResized = CheckForResize();	

	bool xfbchanged = m_last_xfb_mode != g_ActiveConfig.bUseRealXFB;

	if (FramebufferManagerBase::LastXfbWidth() != fbStride || FramebufferManagerBase::LastXfbHeight() != fbHeight)
	{
		xfbchanged = true;
		unsigned int xfb_w = (fbStride < 1 || fbStride > MAX_XFB_WIDTH) ? MAX_XFB_WIDTH : fbStride;
		unsigned int xfb_h = (fbHeight < 1 || fbHeight > MAX_XFB_HEIGHT) ? MAX_XFB_HEIGHT : fbHeight;
		FramebufferManagerBase::SetLastXfbWidth(xfb_w);
		FramebufferManagerBase::SetLastXfbHeight(xfb_h);
	}

	// Flip/present backbuffer to frontbuffer here
	D3D::Present();

	// Resize the back buffers NOW to avoid flickering
	if (CalculateTargetSize()
		|| xfbchanged
		|| windowResized
		|| m_last_efb_scale != g_ActiveConfig.iEFBScale
		|| m_last_multisamples != g_ActiveConfig.iMultisamples
		|| m_last_stereo_mode != g_ActiveConfig.iStereoMode)
	{

		m_last_xfb_mode = g_ActiveConfig.bUseRealXFB;
		m_last_multisamples = g_ActiveConfig.iMultisamples;
		PixelShaderCache::InvalidateMSAAShaders();

		if (windowResized)
		{
			// TODO: Aren't we still holding a reference to the back buffer right now?
			D3D::Reset();
			m_backbuffer_width = D3D::GetBackBufferWidth();
			m_backbuffer_height = D3D::GetBackBufferHeight();
		}

		UpdateDrawRectangle();

		m_last_efb_scale = g_ActiveConfig.iEFBScale;

		PixelShaderManager::SetEfbScaleChanged();
		D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), nullptr);

		g_framebuffer_manager.reset();
		g_framebuffer_manager = std::make_unique<FramebufferManager>(m_target_width, m_target_height);
		float clear_col[4] = { 0.f, 0.f, 0.f, 1.f };
		D3D::context->ClearRenderTargetView(FramebufferManager::GetEFBColorTexture()->GetRTV(), clear_col);
		D3D::context->ClearDepthStencilView(FramebufferManager::GetEFBDepthTexture()->GetDSV(), D3D11_CLEAR_DEPTH, 1.f, 0);
		if (m_last_stereo_mode != g_ActiveConfig.iStereoMode)
		{
			m_last_stereo_mode = g_ActiveConfig.iStereoMode;
			m_post_processor->SetReloadFlag();
		}
	}

	// begin next frame
	D3D::BeginFrame();
	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());
	Renderer::RestoreAPIState();
	// if the configuration has changed, reload post processor (can fail, which will deactivate it)
	if (m_post_processor->RequiresReload())
		m_post_processor->ReloadShaders();
}

// ALWAYS call RestoreAPIState for each ResetAPIState call you're doing
void Renderer::ResetAPIState()
{
	D3D::stateman->PushBlendState(resetblendstate.get());
	D3D::stateman->PushDepthState(resetdepthstate.get());
	D3D::stateman->PushRasterizerState(resetraststate.get());
}

void Renderer::RestoreAPIState()
{
	// Gets us back into a more game-like state.
	D3D::stateman->PopBlendState();
	D3D::stateman->PopDepthState();
	D3D::stateman->PopRasterizerState();
	BPFunctions::SetScissor();
}

void Renderer::ApplyState(bool bUseDstAlpha)
{
	gx_state.blend.use_dst_alpha = bUseDstAlpha;
	D3D::stateman->PushBlendState(gx_state_cache.Get(gx_state.blend));
	D3D::stateman->PushDepthState(gx_state_cache.Get(gx_state.zmode));
	D3D::stateman->PushRasterizerState(gx_state_cache.Get(gx_state.raster));

	for (unsigned int stage = 0; stage < 8; stage++)
	{
		// TODO: cache SamplerState directly, not d3d object
		gx_state.sampler[stage].max_anisotropy = 1ull << g_ActiveConfig.iMaxAnisotropy;
		D3D::stateman->SetSampler(stage, gx_state_cache.Get(gx_state.sampler[stage]));
	}

	if (bUseDstAlpha)
	{
		// restore actual state
		SetLogicOpMode();
	}
	D3D::BufferDescriptor vbuffer = VertexShaderCache::GetConstantBuffer();
	D3D::BufferDescriptor pbuffer = PixelShaderCache::GetConstantBuffer();
	ID3D11GeometryShader* geometry_shader = GeometryShaderCache::GetActiveShader();
	ID3D11HullShader* hull_shader = HullDomainShaderCache::GetActiveHullShader();

	D3D::stateman->SetVertexConstants(vbuffer);
	if (geometry_shader)
	{
		D3D::stateman->SetGeometryConstants(GeometryShaderCache::GetConstantBuffer());
	}
	if (hull_shader)
	{
		D3D::stateman->SetHullDomainConstants(0, HullDomainShaderCache::GetConstantBuffer());
		D3D::stateman->SetHullDomainConstants(1, vbuffer);
		D3D::stateman->SetHullDomainConstants(2, pbuffer);
	}
	D3D::stateman->SetPixelConstants(0, pbuffer);
	D3D::stateman->SetPixelConstants(1, vbuffer);

	D3D::stateman->SetVertexShader(VertexShaderCache::GetActiveShader());
	D3D::stateman->SetGeometryShader(geometry_shader);
	D3D::stateman->SetHullShader(hull_shader);
	D3D::stateman->SetDomainShader(HullDomainShaderCache::GetActiveDomainShader());
	D3D::stateman->SetPixelShader(PixelShaderCache::GetActiveShader());

	FramebufferManager::InvalidateEFBCache();
}

void Renderer::RestoreState()
{
	D3D::stateman->PopBlendState();
	D3D::stateman->PopDepthState();
	D3D::stateman->PopRasterizerState();
}

void Renderer::ApplyCullDisable()
{
	RasterizerState rast = gx_state.raster;
	rast.cull_mode = D3D11_CULL_NONE;

	ID3D11RasterizerState* raststate = gx_state_cache.Get(rast);
	D3D::stateman->PushRasterizerState(raststate);
}

void Renderer::RestoreCull()
{
	D3D::stateman->PopRasterizerState();
}

void Renderer::SetGenerationMode()
{
	const D3D11_CULL_MODE d3dCullModes[4] =
	{
		D3D11_CULL_NONE,
		D3D11_CULL_BACK,
		D3D11_CULL_FRONT,
		D3D11_CULL_BACK
	};

	// rastdc.FrontCounterClockwise must be false for this to work
	// TODO: GX_CULL_ALL not supported, yet!
	gx_state.raster.cull_mode = d3dCullModes[bpmem.genMode.cullmode];
}

void Renderer::SetDepthMode()
{
	gx_state.zmode.testenable = (u32)bpmem.zmode.testenable;
	gx_state.zmode.func = bpmem.zmode.func;
	gx_state.zmode.updateenable = (u32)bpmem.zmode.updateenable;
}

void Renderer::SetLogicOpMode()
{
	// 0   0x00
	// 1   Source & destination
	// 2   Source & ~destination
	// 3   Source
	// 4   ~Source & destination
	// 5   Destination
	// 6   Source ^ destination =  Source & ~destination | ~Source & destination
	// 7   Source | destination
	// 8   ~(Source | destination)
	// 9   ~(Source ^ destination) = ~Source & ~destination | Source & destination
	// 10  ~Destination
	// 11  Source | ~destination
	// 12  ~Source
	// 13  ~Source | destination
	// 14  ~(Source & destination)
	// 15  0xff

	const D3D11_LOGIC_OP  d3d_111_logic_op[16] =
	{
		D3D11_LOGIC_OP_CLEAR,
		D3D11_LOGIC_OP_AND,
		D3D11_LOGIC_OP_AND_REVERSE,
		D3D11_LOGIC_OP_COPY,
		D3D11_LOGIC_OP_AND_INVERTED,
		D3D11_LOGIC_OP_NOOP,
		D3D11_LOGIC_OP_XOR,
		D3D11_LOGIC_OP_OR,
		D3D11_LOGIC_OP_NOR,
		D3D11_LOGIC_OP_EQUIV,
		D3D11_LOGIC_OP_INVERT,
		D3D11_LOGIC_OP_OR_REVERSE,
		D3D11_LOGIC_OP_COPY_INVERTED,
		D3D11_LOGIC_OP_OR_INVERTED,
		D3D11_LOGIC_OP_NAND,
		D3D11_LOGIC_OP_SET
	};
	// fallbacks for devices that does not support logic blending
	const D3D11_BLEND_OP d3dLogicOps[16] =
	{
		D3D11_BLEND_OP_ADD,
		D3D11_BLEND_OP_ADD,
		D3D11_BLEND_OP_SUBTRACT,
		D3D11_BLEND_OP_ADD,
		D3D11_BLEND_OP_REV_SUBTRACT,
		D3D11_BLEND_OP_ADD,
		D3D11_BLEND_OP_MAX,
		D3D11_BLEND_OP_ADD,
		D3D11_BLEND_OP_MAX,
		D3D11_BLEND_OP_MAX,
		D3D11_BLEND_OP_ADD,
		D3D11_BLEND_OP_ADD,
		D3D11_BLEND_OP_ADD,
		D3D11_BLEND_OP_ADD,
		D3D11_BLEND_OP_ADD,
		D3D11_BLEND_OP_ADD
	};
	const D3D11_BLEND d3dLogicOpSrcFactors[16] =
	{
		D3D11_BLEND_ZERO,
		D3D11_BLEND_DEST_COLOR,
		D3D11_BLEND_ONE,
		D3D11_BLEND_ONE,
		D3D11_BLEND_DEST_COLOR,
		D3D11_BLEND_ZERO,
		D3D11_BLEND_INV_DEST_COLOR,
		D3D11_BLEND_INV_DEST_COLOR,
		D3D11_BLEND_INV_SRC_COLOR,
		D3D11_BLEND_INV_SRC_COLOR,
		D3D11_BLEND_INV_DEST_COLOR,
		D3D11_BLEND_ONE,
		D3D11_BLEND_INV_SRC_COLOR,
		D3D11_BLEND_INV_SRC_COLOR,
		D3D11_BLEND_INV_DEST_COLOR,
		D3D11_BLEND_ONE
	};
	const D3D11_BLEND d3dLogicOpDestFactors[16] =
	{
		D3D11_BLEND_ZERO,
		D3D11_BLEND_ZERO,
		D3D11_BLEND_INV_SRC_COLOR,
		D3D11_BLEND_ZERO,
		D3D11_BLEND_ONE,
		D3D11_BLEND_ONE,
		D3D11_BLEND_INV_SRC_COLOR,
		D3D11_BLEND_ONE,
		D3D11_BLEND_INV_DEST_COLOR,
		D3D11_BLEND_SRC_COLOR,
		D3D11_BLEND_INV_DEST_COLOR,
		D3D11_BLEND_INV_DEST_COLOR,
		D3D11_BLEND_INV_SRC_COLOR,
		D3D11_BLEND_ONE,
		D3D11_BLEND_INV_SRC_COLOR,
		D3D11_BLEND_ONE
	};

	if (bpmem.blendmode.logicopenable
		&& !bpmem.blendmode.blendenable)
	{
		bool logicopenabled = bpmem.blendmode.logicmode != BlendMode::LogicOp::COPY;
		gx_state.blend.blend_enable = logicopenabled;
		gx_state.blend.logic_op_enabled = logicopenabled && (D3D::GetLogicOpSupported() || g_ActiveConfig.bForceLogicOpBlend);
		if (logicopenabled)
		{
			gx_state.blend.logic_op = d3d_111_logic_op[bpmem.blendmode.logicmode];
			// Set blending fallbacks in case device does not support logic blending
			gx_state.blend.blend_op = d3dLogicOps[bpmem.blendmode.logicmode];
			gx_state.blend.src_blend = d3dLogicOpSrcFactors[bpmem.blendmode.logicmode];
			gx_state.blend.dst_blend = d3dLogicOpDestFactors[bpmem.blendmode.logicmode];
		}
	}
	else
	{
		gx_state.blend.logic_op_enabled = false;
		SetBlendMode(true);
	}
}

void Renderer::SetSamplerState(int stage, int texindex, bool custom_tex)
{
	const FourTexUnits &tex = bpmem.tex[texindex];
	const TexMode0 &tm0 = tex.texMode0[stage];
	const TexMode1 &tm1 = tex.texMode1[stage];

	if (texindex)
		stage += 4;

	if (g_ActiveConfig.bForceFiltering)
	{
		gx_state.sampler[stage].min_filter = SamplerCommon::IsBpTexMode0MipmapsEnabled(tm0) ? 6 : 4;
		gx_state.sampler[stage].mag_filter = 1; // linear mag
	}
	else if (g_ActiveConfig.bDisableTextureFiltering)
	{
		gx_state.sampler[stage].min_filter = 0;
		gx_state.sampler[stage].mag_filter = 0;
	}
	else
	{
		gx_state.sampler[stage].min_filter = (u32)tm0.min_filter;
		gx_state.sampler[stage].mag_filter = (u32)tm0.mag_filter;
	}

	gx_state.sampler[stage].wrap_s = (u32)tm0.wrap_s;
	gx_state.sampler[stage].wrap_t = (u32)tm0.wrap_t;
	gx_state.sampler[stage].max_lod = (u32)tm1.max_lod;
	gx_state.sampler[stage].min_lod = (u32)tm1.min_lod;
	gx_state.sampler[stage].lod_bias = (s32)tm0.lod_bias;

	// custom textures may have higher resolution, so disable the max_lod
	if (custom_tex)
	{
		gx_state.sampler[stage].max_lod = 255;
	}
}

void Renderer::SetInterlacingMode()
{
	// TODO
}

u16 Renderer::BBoxRead(int index)
{
	// Here we get the min/max value of the truncated position of the upscaled framebuffer.
	// So we have to correct them to the unscaled EFB sizes.
	int value = BBox::Get(index);

	if (index < 2)
	{
		// left/right
		value = value * EFB_WIDTH / m_target_width;
	}
	else
	{
		// up/down
		value = value * EFB_HEIGHT / m_target_height;
	}
	if (index & 1)
		value++; // fix max values to describe the outer border

	return value;
}

void Renderer::BBoxWrite(int index, u16 _value)
{
	int value = _value; // u16 isn't enough to multiply by the efb width
	if (index & 1)
		value--;
	if (index < 2)
	{
		value = value * m_target_width / EFB_WIDTH;
	}
	else
	{
		value = value * m_target_height / EFB_HEIGHT;
	}
	BBox::Set(index, value);
}

void Renderer::DrawFrame(const TargetRectangle& target_rc, const EFBRectangle& source_rc, u32 xfb_addr,
	const XFBSourceBase* const* xfb_sources, u32 xfb_count, D3DTexture2D* dst_texture, const TargetSize& dst_size, u32 fb_width,
	u32 fb_stride, u32 fb_height, float Gamma)
{
	if (g_ActiveConfig.bUseXFB)
	{
		if (g_ActiveConfig.bUseRealXFB)
			DrawRealXFB(target_rc, xfb_addr, dst_texture, dst_size, fb_width, fb_stride, fb_height);
		else
			DrawVirtualXFB(target_rc, xfb_addr, xfb_sources, xfb_count, dst_texture, dst_size, fb_width, fb_stride, fb_height, Gamma);
	}
	else
	{
		DrawEFB(target_rc, source_rc, dst_texture, dst_size, Gamma);
	}
	// Restore render target to backbuffer
	D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), nullptr);
	D3D::SetLinearCopySampler();
}

void Renderer::DrawEFB(const TargetRectangle& t_rc, const EFBRectangle& source_rc, D3DTexture2D* dst_texture, const TargetSize& dst_size, float Gamma)
{
	TargetRectangle scaled_source_rc = Renderer::ConvertEFBRectangle(source_rc);
	TargetRectangle target_rc = { t_rc.left, t_rc.top, t_rc.right, t_rc.bottom };
	D3DTexture2D* tex = FramebufferManager::GetResolvedEFBColorTexture();
	TargetSize tex_size(m_target_width, m_target_height);
	D3DTexture2D* blit_depth_tex = nullptr;
	// Post processing active?
	if (m_post_processor->ShouldTriggerOnSwap())
	{
		TargetRectangle src_rect(scaled_source_rc);
		TargetSize src_size(tex_size);
		if (m_post_processor->RequiresDepthBuffer())
			blit_depth_tex = FramebufferManager::GetResolvedEFBDepthTexture();

		uintptr_t new_blit_tex;
		m_post_processor->PostProcess(&scaled_source_rc, &tex_size, &new_blit_tex,
			src_rect, src_size, reinterpret_cast<uintptr_t>(tex),
			src_rect, src_size, reinterpret_cast<uintptr_t>(blit_depth_tex));
		tex = reinterpret_cast<D3DTexture2D*>(new_blit_tex);

		
	}
	if (blit_depth_tex == nullptr 
		&& (m_post_processor->GetScalingShaderConfig()->RequiresDepthBuffer() 
			|| (m_post_processor->ShouldTriggerAfterBlit() && m_post_processor->RequiresDepthBuffer())))
	{
		blit_depth_tex = FramebufferManager::GetResolvedEFBDepthTexture();
	}
	BlitScreen(target_rc, scaled_source_rc, tex_size, tex, blit_depth_tex, dst_size, dst_texture, Gamma);
}

void Renderer::DrawVirtualXFB(const TargetRectangle& target_rc, u32 xfb_addr,
	const XFBSourceBase* const* xfb_sources, u32 xfb_count, D3DTexture2D* dst_texture, const TargetSize& dst_size, u32 fb_width,
	u32 fb_stride, u32 fb_height, float Gamma)
{
	// draw each xfb source
	for (u32 i = 0; i < xfb_count; ++i)
	{
		const XFBSource* xfb_source = static_cast<const XFBSource*>(xfb_sources[i]);

		TargetRectangle drawRc;

		TargetRectangle source_rc;
		source_rc.left = xfb_source->sourceRc.left;
		source_rc.top = xfb_source->sourceRc.top;
		source_rc.right = xfb_source->sourceRc.right;
		source_rc.bottom = xfb_source->sourceRc.bottom;

		// use virtual xfb with offset
		int xfb_height = xfb_source->srcHeight;
		int xfb_width = xfb_source->srcWidth;
		int hOffset = (static_cast<s32>(xfb_source->srcAddr) - static_cast<s32>(xfb_addr)) / (static_cast<s32>(fb_stride) * 2);

		drawRc.top = target_rc.top + hOffset * target_rc.GetHeight() / static_cast<s32>(fb_height);
		drawRc.bottom = target_rc.top + (hOffset + xfb_height) * target_rc.GetHeight() / static_cast<s32>(fb_height);
		drawRc.left = target_rc.left + (target_rc.GetWidth() - xfb_width * target_rc.GetWidth() / static_cast<s32>(fb_stride)) / 2;
		drawRc.right = target_rc.left + (target_rc.GetWidth() + xfb_width * target_rc.GetWidth() / static_cast<s32>(fb_stride)) / 2;

		// The following code disables auto stretch.  Kept for reference.
		// scale draw area for a 1 to 1 pixel mapping with the draw target
		//float vScale = static_cast<float>(fbHeight) / static_cast<float>(s_backbuffer_height);
		//float hScale = static_cast<float>(fbWidth) / static_cast<float>(s_backbuffer_width);
		//drawRc.top *= vScale;
		//drawRc.bottom *= vScale;CalculateTargetSize
		//drawRc.left *= hScale;
		//drawRc.right *= hScale;

		source_rc.right -= Renderer::EFBToScaledX(fb_stride - fb_width);

		TargetSize blit_size(xfb_source->texWidth, xfb_source->texHeight);
		BlitScreen(drawRc, source_rc, blit_size, xfb_source->tex, xfb_source->depthtex, dst_size, dst_texture, Gamma);
	}
}

void Renderer::DrawRealXFB(const TargetRectangle& target_rc, u32 xfb_addr, D3DTexture2D* dst_texture, const TargetSize& dst_size, u32 fb_width, u32 fb_stride, u32 fb_height)
{
	// Restore render target to backbuffer
	D3D::context->OMSetRenderTargets(1, &dst_texture->GetRTV(), nullptr);
	D3D::SetLinearCopySampler();
	// TODO: Television should be used to render Virtual XFB mode as well.
	D3D11_VIEWPORT vp = CD3D11_VIEWPORT((float)target_rc.left, (float)target_rc.top, (float)target_rc.GetWidth(), (float)target_rc.GetHeight());
	D3D::context->RSSetViewports(1, &vp);

	s_television.Submit(xfb_addr, fb_stride, fb_width, fb_height);
	s_television.Render();
	// Restore render target to backbuffer
	D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), nullptr);
}

void Renderer::BlitScreen(TargetRectangle dst_rect, TargetRectangle src_rect, TargetSize src_size, D3DTexture2D* src_texture, D3DTexture2D* depth_texture,
	const TargetSize& dst_size, D3DTexture2D* dst_texture, float Gamma)
{
	if (g_ActiveConfig.iStereoMode == STEREO_SBS || g_ActiveConfig.iStereoMode == STEREO_TAB)
	{
		TargetRectangle leftRc, rightRc;
		std::tie(leftRc, rightRc) = ConvertStereoRectangle(dst_rect);

		m_post_processor->BlitScreen(leftRc, dst_size, reinterpret_cast<uintptr_t>(dst_texture),
			src_rect, src_size, reinterpret_cast<uintptr_t>(src_texture), reinterpret_cast<uintptr_t>(depth_texture), 0, Gamma);

		m_post_processor->BlitScreen(rightRc, dst_size, reinterpret_cast<uintptr_t>(dst_texture),
			src_rect, src_size, reinterpret_cast<uintptr_t>(src_texture), reinterpret_cast<uintptr_t>(depth_texture), 1, Gamma);
	}
	else if (g_ActiveConfig.iStereoMode == STEREO_3DVISION)
	{
		Create3DVisionTexture(dst_size.width, dst_size.height);

		TargetRectangle leftRc;
		leftRc.left = dst_rect.left;
		leftRc.right = dst_rect.right;
		leftRc.top = dst_rect.top;
		leftRc.bottom = dst_rect.bottom;

		TargetRectangle rightRc;
		rightRc.left = dst_rect.left + dst_size.width;
		rightRc.right = dst_rect.right + dst_size.width;
		rightRc.top = dst_rect.top;
		rightRc.bottom = dst_rect.bottom;
		TargetSize side_size = { dst_size.width * 2, dst_size.height + 1};		
		// Render to staging texture which is double the width of the backbuffer
		m_post_processor->BlitScreen(leftRc, side_size, reinterpret_cast<uintptr_t>(m_3d_vision_texture),
			src_rect, src_size, reinterpret_cast<uintptr_t>(src_texture), reinterpret_cast<uintptr_t>(depth_texture), 0, Gamma);

		m_post_processor->BlitScreen(rightRc, side_size, reinterpret_cast<uintptr_t>(m_3d_vision_texture),
			src_rect, src_size, reinterpret_cast<uintptr_t>(src_texture), reinterpret_cast<uintptr_t>(depth_texture), 1, Gamma);

		// Copy the left eye to the backbuffer, if Nvidia 3D Vision is enabled it should
		// recognize the signature and automatically include the right eye frame.
		D3D11_BOX box = CD3D11_BOX(0, 0, 0, dst_size.width, dst_size.height, 1);
		D3D::context->CopySubresourceRegion(dst_texture->GetTex(), 0, 0, 0, 0, m_3d_vision_texture->GetTex(), 0, &box);

		// Restore render target to backbuffer
		D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), nullptr);
	}
	else
	{
		m_post_processor->BlitScreen(dst_rect, dst_size, reinterpret_cast<uintptr_t>(dst_texture),
			src_rect, src_size, reinterpret_cast<uintptr_t>(src_texture), reinterpret_cast<uintptr_t>(depth_texture), 0, Gamma);
	}
}

void  Renderer::DumpFrame(const EFBRectangle& source_rc, u32 xfb_addr,
	const XFBSourceBase* const* xfb_sources, u32 xfb_count, u32 fb_width,
	u32 fb_stride, u32 fb_height, u64 ticks)
{
	D3D11_BOX source_box;
	D3DTexture2D* src = nullptr;
	u32 src_width;
	u32 src_height;
	if (g_ActiveConfig.bInternalResolutionFrameDumps)
	{
		TargetRectangle render_rc = CalculateFrameDumpDrawRectangle();
		src_width = static_cast<u32>(render_rc.GetWidth());
		src_height = static_cast<u32>(render_rc.GetHeight());
		source_box = GetScreenshotSourceBox(render_rc, src_width, src_height);
		PrepareFrameDumpRenderTexture(src_width, src_height);
		src = m_frame_dump_render_texture;
		TargetSize dst_size = { (int)src_width ,(int)src_height };
		DrawFrame(render_rc, source_rc, xfb_addr, xfb_sources, xfb_count, m_frame_dump_render_texture, dst_size, fb_width, fb_stride, fb_height, 1.0);
	}
	else
	{
		src = D3D::GetBackBuffer();
		src_width = GetTargetRectangle().GetWidth();
		src_height = GetTargetRectangle().GetHeight();
		source_box = GetScreenshotSourceBox(m_target_rectangle, src_width, src_height);
	}
	PrepareFrameDumpBuffer(src_width, src_height);

	unsigned int box_width = source_box.right - source_box.left;
	unsigned int box_height = source_box.bottom - source_box.top;
	
	D3D::context->CopySubresourceRegion(m_frame_dump_staging_texture.get(), 0, 0, 0, 0, (ID3D11Resource*)src->GetTex(), 0, &source_box);
	D3D11_MAPPED_SUBRESOURCE map;
	D3D::context->Map(m_frame_dump_staging_texture.get(), 0, D3D11_MAP_READ, 0, &map);
	AVIDump::Frame state = AVIDump::FetchState(ticks);
	DumpFrameData(reinterpret_cast<const u8*>(map.pData), box_width, box_height,
		map.RowPitch, state);
	FinishFrameData();
	D3D::context->Unmap(m_frame_dump_staging_texture.get(), 0);
}

void Renderer::SetFullscreen(bool enable_fullscreen)
{
	D3D::SetFullscreenState(enable_fullscreen);
}

bool Renderer::IsFullscreen() const
{
	return D3D::GetFullscreenState();
}

}  // namespace DX11
