// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <list>
#include <d3dx9.h>
#include <cinttypes>
#include <memory>

#include "Common/StringUtil.h"
#include "Common/Common.h"
#include "Common/Atomic.h"
#include "Common/FileUtil.h"
#include "Common/Thread.h"
#include "Common/Timer.h"

#include "Core/Core.h"
#include "Core/ConfigManager.h"
#include "Core/Host.h"
#include "Core/Movie.h"

#include "VideoBackends/DX9/D3DUtil.h"
#include "VideoBackends/DX9/FramebufferManager.h"
#include "VideoBackends/DX9/PerfQuery.h"
#include "VideoBackends/DX9/PixelShaderCache.h"
#include "VideoBackends/DX9/Render.h"
#include "VideoBackends/DX9/VertexManager.h"
#include "VideoBackends/DX9/VertexShaderCache.h"
#include "VideoBackends/DX9/TextureCache.h"
#include "VideoBackends/DX9/TextureConverter.h"

#include "VideoCommon/AVIDump.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/FPSCounter.h"
#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/BPStructs.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFStructs.h"

namespace DX9
{

static int s_fps = 0;

static u32 s_blendMode;
static u32 s_LastAA;
static bool IS_AMD;
static float m_fMaxPointSize;
static bool s_vsync;
static bool s_b3D_RightFrame = false;
static LPDIRECT3DSURFACE9 ScreenShootMEMSurface = NULL;
static bool s_last_fullscreen_mode;

void SetupDeviceObjects()
{
	D3D::font.Init();
	VertexLoaderManager::Init();
	g_framebuffer_manager = std::make_unique<FramebufferManager>();

	VertexShaderManager::Dirty();
	PixelShaderManager::Dirty();
	TextureConverter::Init();

	// To avoid shader compilation stutters, read back all shaders from cache.
	VertexShaderCache::Init();
	PixelShaderCache::Init();
	g_vertex_manager->CreateDeviceObjects();
	static_cast<PerfQuery*>(g_perf_query.get())->CreateDeviceObjects();
	// Texture cache will recreate themselves over time.
}

// Kill off all POOL_DEFAULT device objects.
void TeardownDeviceObjects()
{
	if(ScreenShootMEMSurface)
		ScreenShootMEMSurface->Release();
	ScreenShootMEMSurface = NULL;
	D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
	D3D::dev->SetDepthStencilSurface(D3D::GetBackBufferDepthSurface());
	g_framebuffer_manager.reset();
	static_cast<PerfQuery*>(g_perf_query.get())->DestroyDeviceObjects();
	D3D::font.Shutdown();
	TextureCacheBase::Invalidate();
	VertexLoaderManager::Shutdown();
	VertexShaderCache::Shutdown();
	PixelShaderCache::Shutdown();
	TextureConverter::Shutdown();
	g_vertex_manager->DestroyDeviceObjects();
}

// Init functions
Renderer::Renderer(void *&window_handle)
{
	int fullScreenRes, w_temp, h_temp;
	s_blendMode = 0;
	// Multisample Anti-aliasing hasn't been implemented yet use supersamling instead
	int backbuffer_ms_mode = 0;

	RECT client;
	GetClientRect((HWND)window_handle, &client);
	w_temp = client.right - client.left;
	h_temp = client.bottom - client.top;

	for (fullScreenRes = 0; fullScreenRes < (int)D3D::GetAdapter(g_ActiveConfig.iAdapter).resolutions.size(); fullScreenRes++)
	{
		if ((D3D::GetAdapter(g_ActiveConfig.iAdapter).resolutions[fullScreenRes].xres == w_temp) && 
			(D3D::GetAdapter(g_ActiveConfig.iAdapter).resolutions[fullScreenRes].yres == h_temp))
			break;
	}
	if (fullScreenRes == D3D::GetAdapter(g_ActiveConfig.iAdapter).resolutions.size())
		fullScreenRes = 0;

	D3D::Create(g_ActiveConfig.iAdapter, (HWND)window_handle,
		fullScreenRes, backbuffer_ms_mode, false);

	IS_AMD = D3D::IsATIDevice();

	// Decide framebuffer size
	s_backbuffer_width = D3D::GetBackBufferWidth();
	s_backbuffer_height = D3D::GetBackBufferHeight();

	FramebufferManagerBase::SetLastXfbWidth(MAX_XFB_WIDTH);
	FramebufferManagerBase::SetLastXfbHeight(MAX_XFB_HEIGHT);

	UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);

	s_LastAA = g_ActiveConfig.iMultisamples - 1;
	int SupersampleCoeficient = (s_LastAA % 3) + 1;

	s_last_efb_scale = g_ActiveConfig.iEFBScale;
	CalculateTargetSize(s_backbuffer_width, s_backbuffer_height, SupersampleCoeficient);
	PixelShaderManager::SetEfbScaleChanged();

	// Make sure to use valid texture sizes
	D3D::FixTextureSize(s_target_width, s_target_height);

	// We're not using fixed function.
	// Let's just set the matrices to identity to be sure.
	D3DXMATRIX mtx;
	D3DXMatrixIdentity(&mtx);
	D3D::dev->SetTransform(D3DTS_VIEW, &mtx);
	D3D::dev->SetTransform(D3DTS_WORLD, &mtx);

	SetupDeviceObjects();

	for (int stage = 0; stage < 8; stage++)
		D3D::SetSamplerState(stage, D3DSAMP_MAXANISOTROPY, 1 << g_ActiveConfig.iMaxAnisotropy);

	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = s_backbuffer_width;
	vp.Height = s_backbuffer_height;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);
	D3D::dev->Clear(0, NULL, D3DCLEAR_TARGET, 0x0, 0, 0);

	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = s_target_width;
	vp.Height = s_target_height;
	D3D::dev->SetViewport(&vp);
	D3D::dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0,0,0), 1.0f, 0);
	D3D::dev->CreateOffscreenPlainSurface(s_backbuffer_width,s_backbuffer_height, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &ScreenShootMEMSurface, NULL );
	D3D::BeginFrame();
	// Initial state setup
	D3D::SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
	D3D::SetRenderState(D3DRS_FILLMODE, g_ActiveConfig.bWireFrame ? D3DFILL_WIREFRAME : D3DFILL_SOLID);	
	D3D::SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	D3D::SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	D3D::SetRenderState(D3DRS_ZENABLE, FALSE);
	D3D::SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	D3D::SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
	D3D::SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);	
	D3D::SetRenderState(D3DRS_POINTSCALEENABLE, FALSE);	
	m_fMaxPointSize = D3D::GetCaps().MaxPointSize;
	// Handle VSync on/off 
	s_vsync = g_ActiveConfig.IsVSync();
	m_bColorMaskChanged = false;
	m_bBlendModeChanged = false;
	m_bScissorRectChanged = false;	
	m_bGenerationModeChanged = false;
	m_bDepthModeChanged = false;
	m_bLogicOpModeChanged = false;
}

Renderer::~Renderer()
{
	TeardownDeviceObjects();
	D3D::EndFrame();
	D3D::Present();
	D3D::Close();
}

void Renderer::RenderText(const std::string &text, int left, int top, u32 color)
{
	TargetRectangle trc = GetTargetRectangle();
	D3D::font.DrawTextScaled((float)(trc.left + left + 1), (float)(trc.top + top + 1), 20, 20, 0.0f, color & 0xFF000000, text.c_str());
	D3D::font.DrawTextScaled((float)(trc.left + left), (float)(trc.top + top), 20, 20, 0.0f, color, text.c_str());
}

TargetRectangle Renderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
	TargetRectangle result;
	result.left   = EFBToScaledX(rc.left);
	result.top    = EFBToScaledY(rc.top);
	result.right  = EFBToScaledX(rc.right);
	result.bottom = EFBToScaledY(rc.bottom);
	return result;
}

}

void formatBufferDump(const u8* in, u8* out, int w, int h, int p)
{
	for (int y = 0; y < h; y++)
	{
		auto line = in + (h - y - 1) * p;
		for (int x = 0; x < w; x++)
		{
			memcpy(out, line, 3);
			out += 3;
			line += 4;
		}
	}
}

namespace DX9
{

// With D3D, we have to resize the backbuffer if the window changed
// size.
void Renderer::CheckForResize(bool &resized, bool &fullscreen, bool &fullscreencahnged)
{
	RECT rcWindow;
	GetClientRect(D3D::hWnd, &rcWindow);
	int client_width = rcWindow.right - rcWindow.left;
	int client_height = rcWindow.bottom - rcWindow.top;
	// Sanity check
	resized = (client_width != Renderer::GetBackbufferWidth()
		|| client_height != Renderer::GetBackbufferHeight()
		|| s_vsync != g_ActiveConfig.IsVSync()) &&
		client_width >= 4 && client_height >= 4;
	
	fullscreen = g_ActiveConfig.bFullscreen &&
		!SConfig::GetInstance().bRenderToMain;

	fullscreencahnged = s_last_fullscreen_mode != fullscreen;

	if (resized || fullscreencahnged)
	{
		// Handle vsync changes during execution
		s_vsync = g_ActiveConfig.IsVSync();
		if (!D3D::GetEXSupported())
		{
			TeardownDeviceObjects();
		}
		

		D3D::Reset();
		s_backbuffer_width = D3D::GetBackBufferWidth();
		s_backbuffer_height = D3D::GetBackBufferHeight();
		if(ScreenShootMEMSurface)
			ScreenShootMEMSurface->Release();
		D3D::dev->CreateOffscreenPlainSurface(Renderer::GetBackbufferWidth(), Renderer::GetBackbufferHeight(),
			D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &ScreenShootMEMSurface, NULL );

	}
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
	// if depth textures aren't supported by the hardware, just return
	if (type == PEEK_Z)
		if (FramebufferManager::GetEFBDepthTexture() == NULL)
			return 0;

	if (type == PEEK_Z)
	{
		u32 z = FramebufferManager::GetEFBCachedDepth(x, y);

		// if Z is in 16 bit format you must return a 16 bit integer
		if(bpmem.zcontrol.pixel_format == PEControl::RGB565_Z16) {
			z >>= 8;
		}
		return z;
	}
	else if(type == PEEK_COLOR)
	{
		u32 ret = FramebufferManager::GetEFBCachedColor(x, y);

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
		if(bpmem.zcontrol.pixel_format != PEControl::RGBA6_Z24)
		{
			ret |= 0xFF000000;
		}

		if(alpha_read_mode.ReadMode == 2) return ret; // GX_READ_NONE
		else if(alpha_read_mode.ReadMode == 1) return (ret | 0xFF000000); // GX_READ_FF
		else return (ret & 0x00FFFFFF); // GX_READ_00
	}
	return poke_data;
}

void Renderer::PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points)
{
	ResetAPIState();
	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width = GetTargetWidth();
	vp.Height = GetTargetHeight();
	float nearz = xfmem.viewport.farZ - MathUtil::Clamp<float>(xfmem.viewport.zRange, 0.0f, 16777215.0f);
	float farz = xfmem.viewport.farZ;

	const bool nonStandartViewport = g_ActiveConfig.bViewportCorrection && (nearz < 0.f || farz > 16777216.0f || nearz >= 16777216.0f || farz <= 0.f);
	if (nonStandartViewport)
	{
		vp.MinZ = 0.0f;
		vp.MaxZ = 1.0f;
	}
	else
	{
		// Some games set invalids values for z min and z max so fix them to the max an min alowed and let the shaders do this work
		vp.MaxZ = 1.0f - (MathUtil::Clamp<float>(nearz, 0.0f, 16777215.0f) / 16777216.0f);
		vp.MinZ = 1.0f - (MathUtil::Clamp<float>(farz, 0.0f, 16777215.0f) / 16777216.0f);
	}
	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
	D3D::dev->SetViewport(&vp);
	if (type == POKE_Z)
	{
		D3D::ChangeRenderState(D3DRS_COLORWRITEENABLE, 0);
		D3D::ChangeRenderState(D3DRS_ZENABLE, TRUE);
		D3D::ChangeRenderState(D3DRS_ZWRITEENABLE, TRUE);
		D3D::ChangeRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
	}
	else
	{
		D3D::ChangeRenderState(D3DRS_COLORWRITEENABLE, true);
		D3D::ChangeRenderState(D3DRS_ZENABLE, false);
		D3D::ChangeRenderState(D3DRS_ZWRITEENABLE, false);
		D3D::ChangeRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
	}
	D3D::DrawEFBPokeQuads(type, points, num_points, PixelShaderCache::GetClearProgram(), VertexShaderCache::GetClearVertexShader());

	RestoreAPIState();
}

void Renderer::ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z)
{
	// Reset rendering pipeline while keeping color masks and depth buffer settings
	ResetAPIState();

	DWORD color_mask = 0;
	if (alphaEnable)
		color_mask = D3DCOLORWRITEENABLE_ALPHA;
	if (colorEnable)
		color_mask |= D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;
	D3D::ChangeRenderState(D3DRS_COLORWRITEENABLE, color_mask);

	if (zEnable)
	{
		D3D::ChangeRenderState(D3DRS_ZENABLE, TRUE);
		D3D::ChangeRenderState(D3DRS_ZWRITEENABLE, TRUE);
		D3D::ChangeRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
	}	

	// Update the viewport for clearing the target EFB rect
	TargetRectangle targetRc = ConvertEFBRectangle(rc);
	D3DVIEWPORT9 vp;
	vp.X = targetRc.left;
	vp.Y = targetRc.top;
	vp.Width  = targetRc.GetWidth();
	vp.Height = targetRc.GetHeight();
	vp.MinZ = 0.0;
	vp.MaxZ = 1.0;
	D3D::dev->SetViewport(&vp);
	D3D::drawClearQuad(color, 1.0f - ((z & 0xFFFFFF) / 16777216.0f), PixelShaderCache::GetClearProgram(), VertexShaderCache::GetClearVertexShader());
	RestoreAPIState();
	
	FramebufferManager::InvalidateEFBCache();
}

void Renderer::ReinterpretPixelData(unsigned int convtype)
{
	RECT source;
	SetRect(&source, 0, 0, g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight());

	LPDIRECT3DPIXELSHADER9 pixel_shader;
	if (convtype == 0) pixel_shader = PixelShaderCache::ReinterpRGB8ToRGBA6();
	else if (convtype == 2) pixel_shader = PixelShaderCache::ReinterpRGBA6ToRGB8();
	else
	{
		ERROR_LOG(VIDEO, "Trying to reinterpret pixel data with unsupported conversion type %d", convtype);
		return;
	}

	// convert data and set the target texture as our new EFB
	g_renderer->ResetAPIState();
	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorReinterpretSurface());
	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = g_renderer->GetTargetWidth();
	vp.Height = g_renderer->GetTargetHeight();
	vp.MinZ = 0.0;
	vp.MaxZ = 1.0;
	D3D::dev->SetViewport(&vp);
	D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	D3D::drawShadedTexQuad(FramebufferManager::GetEFBColorTexture(), &source,
		g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight(),
		g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight(),
		pixel_shader, VertexShaderCache::GetSimpleVertexShader(0));
	FramebufferManager::SwapReinterpretTexture();
	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);	
	g_renderer->RestoreAPIState();
	FramebufferManager::InvalidateEFBCache();
}

bool Renderer::SaveScreenshot(const std::string &filename, const TargetRectangle &dst_rect)
{
	HRESULT hr = D3D::dev->GetRenderTargetData(D3D::GetBackBufferSurface(),ScreenShootMEMSurface);
	if(FAILED(hr))
	{
		PanicAlert("Error dumping surface data.");
		return false;
	}
	hr = PD3DXSaveSurfaceToFileA(filename.c_str(), D3DXIFF_PNG, ScreenShootMEMSurface, NULL, dst_rect.AsRECT());
	if(FAILED(hr))
	{
		PanicAlert("Error saving screen.");
		return false;
	}
	OSD::AddMessage(StringFromFormat("Saved %i x %i %s", dst_rect.GetWidth(), dst_rect.GetHeight(), filename.c_str()));

	return true;
}

// This function has the final picture. We adjust the aspect ratio here.
void Renderer::SwapImpl(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, const EFBRectangle& rc, float Gamma)
{
	if (Fifo::WillSkipCurrentFrame() || (!XFBWrited && !g_ActiveConfig.RealXFBEnabled()) || !fbWidth || !fbHeight)
	{
		if (SConfig::GetInstance().m_DumpFrames && !frame_data.empty())
			AVIDump::AddFrame(&frame_data[0], fbWidth, fbHeight);

		Core::Callback_VideoCopiedToXFB(false);
		return;
	}

	u32 xfbCount = 0;
	const XFBSourceBase* const* xfbSourceList = FramebufferManager::GetXFBSource(xfbAddr, fbStride, fbHeight, &xfbCount);
	if ((!xfbSourceList || xfbCount == 0) && g_ActiveConfig.bUseXFB && !g_ActiveConfig.bUseRealXFB)
	{
		if (SConfig::GetInstance().m_DumpFrames && !frame_data.empty())
			AVIDump::AddFrame(&frame_data[0], fbWidth, fbHeight);

		Core::Callback_VideoCopiedToXFB(false);
		return;
	}

	ResetAPIState();

	// Prepare to copy the XFBs to our backbuffer
	D3D::dev->SetDepthStencilSurface(NULL);
	D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
	D3D::dev->Clear(0, NULL, D3DCLEAR_TARGET, 0x0, 0, 0);
	UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);
	D3DVIEWPORT9 vp;
	const TargetRectangle Tr = GetTargetRectangle();
	int X = Tr.left;
	int Y = Tr.top;
	int Width  = Tr.right - Tr.left;
	int Height = Tr.bottom - Tr.top;
	if (X < 0)
	{
		Width = std::min(Width - X, s_backbuffer_width);
		X = 0;
	}
	if (Y < 0)
	{
		Height = std::min(Height - Y, s_backbuffer_height);
		Y = 0;
	}
	if (Width > s_backbuffer_width)
	{
		Width = s_backbuffer_width;
	}

	if (Height > s_backbuffer_height)
	{
		Height = s_backbuffer_height;
	}
	if(g_ActiveConfig.iStereoMode) {		
		VertexShaderManager::ResetView();
		if(s_b3D_RightFrame)
		{
			if (g_ActiveConfig.iStereoMode == STEREO_SHADER)
			{
				D3D::SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_GREEN);
			}
			else if (g_ActiveConfig.iStereoMode == STEREO_TAB)
			{
				Y =  (Y / 2) + (s_backbuffer_height / 2);
				Height = Height / 2;				
			}
			else
			{
				X = X / 2 + (s_backbuffer_width / 2);
				Width = Width / 2;				
			}			
			VertexShaderManager::TranslateView(-0.001f * g_ActiveConfig.iStereoDepth,0.0f);
			VertexShaderManager::RotateView(-0.0001f *g_ActiveConfig.iStereoConvergence,0.0f);
		}
		else
		{
			if (g_ActiveConfig.iStereoMode == STEREO_SHADER)
			{
				D3D::SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED);
			}
			else if (g_ActiveConfig.iStereoMode == STEREO_TAB)
			{
				Y =  Y / 2;
				Height = Height / 2;
			}
			else
			{
				X = X / 2;
				Width = Width / 2;
			}
			VertexShaderManager::TranslateView(0.001f *g_ActiveConfig.iStereoDepth, 0.0f);
			VertexShaderManager::RotateView(0.0001f * g_ActiveConfig.iStereoConvergence, 0.0f);
		}
		s_b3D_RightFrame = !s_b3D_RightFrame;		
	}	

	vp.X = X;
	vp.Y = Y;
	vp.Width = Width;
	vp.Height = Height;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;

	D3D::dev->SetViewport(&vp);

	D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	D3D::ChangeSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

	const XFBSource* xfbSource = NULL;

	if(g_ActiveConfig.bUseXFB)
	{
		// draw each xfb source
		// Render to the real buffer now.
		for (u32 i = 0; i < xfbCount; ++i)
		{
			xfbSource = (XFBSource*)xfbSourceList[i];
			
			MathUtil::Rectangle<float> drawRc;
			MathUtil::Rectangle<float> sourceRc;

			sourceRc.left = (float)xfbSource->sourceRc.left;
			sourceRc.right = (float)xfbSource->sourceRc.right;
			sourceRc.top = (float)xfbSource->sourceRc.top;
			sourceRc.bottom = (float)xfbSource->sourceRc.bottom;
			if (g_ActiveConfig.bUseRealXFB)
			{
				drawRc.top = -1;
				drawRc.bottom = 1;
				drawRc.left = -1;
				drawRc.right = 1;
				sourceRc.right -= fbStride - fbWidth;
			}
			else
			{
				// use virtual xfb with offset
				int xfbHeight = xfbSource->srcHeight;
				int xfbWidth = xfbSource->srcWidth;
				int hOffset = ((s32)xfbSource->srcAddr - (s32)xfbAddr) / ((s32)fbStride * 2);

				drawRc.bottom = 1.0f - (2.0f * (hOffset) / (float)fbHeight);
				drawRc.top = 1.0f - (2.0f * (hOffset + xfbHeight) / (float)fbHeight);
				drawRc.left = -(xfbWidth / (float)fbStride);
				drawRc.right = (xfbWidth / (float)fbStride);

				// The following code disables auto stretch.  Kept for reference.
				// scale draw area for a 1 to 1 pixel mapping with the draw target
				//float vScale = (float)fbHeight / (float)GetTargetRectangle().GetHeight();
				//float hScale = (float)fbWidth / (float)GetTargetRectangle().GetWidth();
				//drawRc.top *= vScale;
				//drawRc.bottom *= vScale;
				//drawRc.left *= hScale;
				//drawRc.right *= hScale;
				sourceRc.right -= Renderer::EFBToScaledX(fbStride - fbWidth);
			}

			xfbSource->Draw(sourceRc, drawRc, Width, Height);
		}
	}
	else
	{
		TargetRectangle targetRc = ConvertEFBRectangle(rc);
		LPDIRECT3DTEXTURE9 read_texture = FramebufferManager::GetEFBColorTexture();
		int multisamplemode = g_ActiveConfig.iMultisamples - 1;		
		if (multisamplemode == 0 && g_ActiveConfig.bUseScalingFilter)
		{
			multisamplemode = std::max(std::min((targetRc.GetWidth() / Width) - 1, 2), 0);
		}
		D3D::drawShadedTexQuad(read_texture,targetRc.AsRECT(),
			Renderer::GetTargetWidth(),Renderer::GetTargetHeight(),
			Width,Height,
			PixelShaderCache::GetColorCopyProgram(multisamplemode),
			VertexShaderCache::GetSimpleVertexShader(multisamplemode), Gamma);

	}
	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);
	D3D::RefreshSamplerState(0, D3DSAMP_MAGFILTER);

	if(g_ActiveConfig.iStereoMode == STEREO_SHADER)
	{
		DWORD color_mask = D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;
		D3D::SetRenderState(D3DRS_COLORWRITEENABLE, color_mask);
	}
	X = Tr.left;
	Y = Tr.top;
	Width  = Tr.right - Tr.left;
	Height = Tr.bottom - Tr.top;

	vp.X = X;
	vp.Y = Y;
	vp.Width = Width;
	vp.Height = Height;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);

	// Save screenshot
	if (s_bScreenshot)
	{
		std::lock_guard<std::mutex> lk(s_criticalScreenshot);
		SaveScreenshot(s_sScreenshotName, GetTargetRectangle());
		s_bScreenshot = false;
	}

	// Dump frames
	static int w = 0, h = 0;
	if (SConfig::GetInstance().m_DumpFrames)
	{
		static int s_recordWidth;
		static int s_recordHeight;

		HRESULT hr = D3D::dev->GetRenderTargetData(D3D::GetBackBufferSurface(),ScreenShootMEMSurface);
		if (!bLastFrameDumped)
		{
			s_recordWidth = GetTargetRectangle().GetWidth();
			s_recordHeight = GetTargetRectangle().GetHeight();
			bAVIDumping = AVIDump::Start(s_recordWidth, s_recordHeight, AVIDump::DumpFormat::FORMAT_BGR);
			if (!bAVIDumping)
			{
				PanicAlert("Error dumping frames to AVI.");
			}
			else
			{
				char msg [255];
				sprintf_s(msg,255, "Dumping Frames to \"%sframedump0.avi\" (%dx%d RGB24)",
					File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), s_recordWidth, s_recordHeight);
				OSD::AddMessage(msg, 2000);
			}
		}
		if (bAVIDumping)
		{
			D3DLOCKED_RECT rect;
			if (SUCCEEDED(ScreenShootMEMSurface->LockRect(&rect, GetTargetRectangle().AsRECT(), D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY)))
			{
				if (frame_data.empty() || w != s_recordWidth || h != s_recordHeight)
				{
					frame_data.resize(3 * s_recordWidth * s_recordHeight);
					w = s_recordWidth;
					h = s_recordHeight;
				}
				formatBufferDump((const u8*)rect.pBits, &frame_data[0], s_recordWidth, s_recordHeight, rect.Pitch);
				FlipImageData(&frame_data[0], w, h);
				AVIDump::AddFrame(&frame_data[0], GetTargetRectangle().GetWidth(), GetTargetRectangle().GetHeight());
				ScreenShootMEMSurface->UnlockRect();
			}
		}
		bLastFrameDumped = true;
	}
	else
	{
		if (bLastFrameDumped && bAVIDumping)
		{
			std::vector<u8>().swap(frame_data);
			w = h = 0;
			AVIDump::Stop();
			bAVIDumping = false;
			OSD::AddMessage("Stop dumping frames to AVI", 2000);
		}
		bLastFrameDumped = false;
	}

	Renderer::DrawDebugText();
	OSD::DrawMessages();
	D3D::EndFrame();	

	GFX_DEBUGGER_PAUSE_AT(NEXT_FRAME, true);
	
	TextureCacheBase::Cleanup(frameCount);
	// Flip/present backbuffer to frontbuffer here
	D3D::Present();
	// Enable configuration changes
	UpdateActiveConfig();
	TextureCacheBase::OnConfigChanged(g_ActiveConfig);

	SetWindowSize(fbStride, fbHeight);

	bool windowResized;
	bool fullscreen;
	bool fullscreen_changed;
	CheckForResize(windowResized, fullscreen, fullscreen_changed);;	
	
	bool xfbchanged = false;

	if (FramebufferManagerBase::LastXfbWidth() != fbStride || FramebufferManagerBase::LastXfbHeight() != fbHeight)
	{
		xfbchanged = true;
		unsigned int w = (fbStride < 1 || fbStride > MAX_XFB_WIDTH) ? MAX_XFB_WIDTH : fbStride;
		unsigned int h = (fbHeight < 1 || fbHeight > MAX_XFB_HEIGHT) ? MAX_XFB_HEIGHT : fbHeight;
		FramebufferManagerBase::SetLastXfbWidth(w);
		FramebufferManagerBase::SetLastXfbHeight(h);
	}

	u32 newAA = g_ActiveConfig.iMultisamples - 1;

	if (CalculateTargetSize(s_backbuffer_width, s_backbuffer_height, (newAA % 3) + 1)
		|| xfbchanged
		|| windowResized 
		|| fullscreen_changed
		|| s_last_efb_scale != g_ActiveConfig.iEFBScale 
		|| s_LastAA != newAA)
	{
		s_LastAA = newAA;

		UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);

		int SupersampleCoeficient = (s_LastAA % 3) + 1;

		s_last_efb_scale = g_ActiveConfig.iEFBScale;		
		PixelShaderManager::SetEfbScaleChanged();
		D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
		D3D::dev->SetDepthStencilSurface(D3D::GetBackBufferDepthSurface());

		if (windowResized || fullscreen_changed)
		{
			// Apply fullscreen state
			if (fullscreen_changed)
			{
				s_last_fullscreen_mode = fullscreen;
				// Notify the host that it is safe to exit fullscreen
				if (!fullscreen)
				{
					Host_RequestFullscreen(false);
				}
			}
			if (!D3D::GetEXSupported())
			{
				// device objects lost, so recreate all of them
				SetupDeviceObjects();
			}
			
		}
		else
		{
			// just resize the frame buffer
			g_framebuffer_manager.reset();
			g_framebuffer_manager = std::make_unique<FramebufferManager>();
		}
		D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
		D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
		D3D::dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0,0,0), 1.0f, 0);
		BPFunctions::SetScissor();
	}

	// Begin new frame
	D3D::BeginFrame();
	RestoreAPIState();

	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
}

// ALWAYS call RestoreAPIState for each ResetAPIState call you're doing
void Renderer::ResetAPIState()
{
	D3D::SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	D3D::SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	D3D::SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	D3D::SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	D3D::SetRenderState(D3DRS_ZENABLE, FALSE);
	D3D::SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	D3D::SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
}

void Renderer::RestoreAPIState()
{
	// Gets us back into a more game-like state.
	D3D::SetRenderState(D3DRS_FILLMODE, g_ActiveConfig.bWireFrame ? D3DFILL_WIREFRAME : D3DFILL_SOLID);
	D3D::SetRenderState(D3DRS_SCISSORTESTENABLE, true);	
	BPFunctions::SetScissor();
	m_bColorMaskChanged = true;
	m_bGenerationModeChanged = true;
	m_bScissorRectChanged = true;
	m_bDepthModeChanged = true;
	m_bLogicOpModeChanged = true;
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

	// TODO: ceil, floor or just cast to int?
	int X = EFBToScaledX((int)ceil(xfmem.viewport.xOrig - xfmem.viewport.wd - scissorXOff));
	int Y = EFBToScaledY((int)ceil(xfmem.viewport.yOrig + xfmem.viewport.ht - scissorYOff));
	int Wd = EFBToScaledX((int)ceil(2.0f * xfmem.viewport.wd));
	int Ht = EFBToScaledY((int)ceil(-2.0f * xfmem.viewport.ht));
	if (Wd < 0)
	{
		X += Wd;
		Wd = -Wd;
	}
	if (Ht < 0)
	{
		Y += Ht;
		Ht = -Ht;
	}

	// In D3D, the viewport rectangle must fit within the render target.
	X = (X >= 0) ? X : 0;
	Y = (Y >= 0) ? Y : 0;
	Wd = (X + Wd <= GetTargetWidth()) ? Wd : (GetTargetWidth() - X);
	Ht = (Y + Ht <= GetTargetHeight()) ? Ht : (GetTargetHeight() - Y);

	D3DVIEWPORT9 vp;
	vp.X = X;
	vp.Y = Y;
	vp.Width = Wd;
	vp.Height = Ht;

	float nearz = xfmem.viewport.farZ - MathUtil::Clamp<float>(xfmem.viewport.zRange, 0.0f, 16777215.0f);
	float farz = xfmem.viewport.farZ;

	const bool nonStandartViewport = g_ActiveConfig.bViewportCorrection && (nearz < 0.f || farz > 16777216.0f || nearz >= 16777216.0f || farz <= 0.f);
	if (nonStandartViewport)
	{
		vp.MinZ = 0.0f;
		vp.MaxZ = 1.0f;
	}
	else
	{
		// Some games set invalids values for z min and z max so fix them to the max an min alowed and let the shaders do this work
		vp.MaxZ = 1.0f - (MathUtil::Clamp<float>(nearz, 0.0f, 16777215.0f) / 16777216.0f);
		vp.MinZ = 1.0f - (MathUtil::Clamp<float>(farz, 0.0f, 16777215.0f) / 16777216.0f);
	}
	D3D::dev->SetViewport(&vp);
}

void Renderer::ApplyState(bool bUseDstAlpha)
{
	if(m_bGenerationModeChanged)
	{
		_SetGenerationMode();
	}
	
	if(m_bDepthModeChanged)
	{
		_SetDepthMode();
	}

	if(m_bColorMaskChanged)
	{
		_SetColorMask();
	}

	if(m_bLogicOpModeChanged)
	{
		_SetLogicOpMode();
	}

	if (m_bBlendModeChanged)
	{
		_SetBlendMode(false);
	}

	if(m_bScissorRectChanged)
	{
		_SetScissorRect();
	}

	if (bUseDstAlpha)
	{
		// If we get here we are sure that we are using dst alpha pass. (bpmem.dstalpha.enable)
		// Alpha write is enabled. (because bpmem.blendmode.alphaupdate && bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24)
		// We must disable blend because we want to write alpha value directly to the alpha channel without modifications.
		D3D::ChangeRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA);
		D3D::ChangeRenderState(D3DRS_ALPHABLENDENABLE, false);
		if(bpmem.zmode.testenable && bpmem.zmode.updateenable)
		{
			// This is needed to draw to the correct pixels in multi-pass algorithms
			// to avoid z-fighting and grants that you write to the same pixels
			// affected by the last pass
			D3D::ChangeRenderState(D3DRS_ZWRITEENABLE, false);
			D3D::ChangeRenderState(D3DRS_ZFUNC, D3DCMP_EQUAL);
		}
	}
	FramebufferManager::InvalidateEFBCache();
}

void Renderer::RestoreState()
{
	D3D::RefreshRenderState(D3DRS_COLORWRITEENABLE);
	D3D::RefreshRenderState(D3DRS_ALPHABLENDENABLE);
	if(bpmem.zmode.testenable && bpmem.zmode.updateenable)
	{
		D3D::RefreshRenderState(D3DRS_ZWRITEENABLE);
		D3D::RefreshRenderState(D3DRS_ZFUNC);
	}
}

void Renderer::_SetScissorRect()
{
	m_bScissorRectChanged = false;
	D3D::dev->SetScissorRect(m_ScissorRect.AsRECT());
}

void Renderer::SetScissorRect(const TargetRectangle& rc)
{
	m_ScissorRect = rc;
	m_bScissorRectChanged = true;
}

void Renderer::SetColorMask()
{
	m_bColorMaskChanged = true;
}

void Renderer::_SetColorMask()
{
	m_bColorMaskChanged = false;
	// Only enable alpha channel if it's supported by the current EFB format
	DWORD color_mask = 0;
	if (bpmem.alpha_test.TestResult() != AlphaTest::FAIL)
	{
		if (bpmem.blendmode.alphaupdate && (bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24))
			color_mask = D3DCOLORWRITEENABLE_ALPHA;
		if (bpmem.blendmode.colorupdate)
			color_mask |= D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;
	}
	D3D::SetRenderState(D3DRS_COLORWRITEENABLE, color_mask);
}
void Renderer::SetBlendMode(bool forceUpdate)
{
	if (forceUpdate)
	{
		_SetBlendMode(forceUpdate);
	}
	else
	{
		m_bBlendModeChanged = true;
	}
}

void Renderer::_SetBlendMode(bool forceUpdate)
{
	m_bBlendModeChanged = false;
	// Our render target always uses an alpha channel, so we need to override the blend functions to assume a destination alpha of 1 if the render target isn't supposed to have an alpha channel
	// Example: D3DBLEND_DESTALPHA needs to be D3DBLEND_ONE since the result without an alpha channel is assumed to always be 1.
	bool target_has_alpha = bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24;
	//really useful for debugging shader and blending errors
	bool use_DstAlpha = bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate && target_has_alpha;
	bool use_DualSource = use_DstAlpha && g_ActiveConfig.backend_info.bSupportsDualSourceBlend;
	const D3DBLEND d3dSrcFactors[8] =
	{
		D3DBLEND_ZERO,
		D3DBLEND_ONE,
		D3DBLEND_DESTCOLOR,
		D3DBLEND_INVDESTCOLOR,
		(use_DualSource) ? D3DBLEND_SRCCOLOR2 : D3DBLEND_SRCALPHA,
		(use_DualSource) ? D3DBLEND_INVSRCCOLOR2 : D3DBLEND_INVSRCALPHA,
		(target_has_alpha) ? D3DBLEND_DESTALPHA : D3DBLEND_ONE,
		(target_has_alpha) ? D3DBLEND_INVDESTALPHA : D3DBLEND_ZERO
	};
	const D3DBLEND d3dDestFactors[8] =
	{
		D3DBLEND_ZERO,
		D3DBLEND_ONE,
		D3DBLEND_SRCCOLOR,
		D3DBLEND_INVSRCCOLOR,
		(use_DualSource) ? D3DBLEND_SRCCOLOR2 : D3DBLEND_SRCALPHA,
		(use_DualSource) ? D3DBLEND_INVSRCCOLOR2 : D3DBLEND_INVSRCALPHA,
		(target_has_alpha) ? D3DBLEND_DESTALPHA : D3DBLEND_ONE,
		(target_has_alpha) ? D3DBLEND_INVDESTALPHA : D3DBLEND_ZERO
	};

	if (bpmem.blendmode.logicopenable && !bpmem.blendmode.blendenable && !forceUpdate)
	{
		D3D::SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE , false);
		return;
	}

	bool blend_enable = bpmem.blendmode.subtract || bpmem.blendmode.blendenable;
	D3D::SetRenderState(D3DRS_ALPHABLENDENABLE, blend_enable);
	D3D::SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, blend_enable && g_ActiveConfig.backend_info.bSupportsSeparateAlphaFunction);
	if (blend_enable)
	{
		D3DBLENDOP op = D3DBLENDOP_ADD;
		u32 srcidx = bpmem.blendmode.srcfactor;
		u32 dstidx = bpmem.blendmode.dstfactor;
		if (bpmem.blendmode.subtract)
		{
			op = D3DBLENDOP_REVSUBTRACT;
			srcidx = BlendMode::ONE;
			dstidx = BlendMode::ONE;
		}
		D3D::SetRenderState(D3DRS_BLENDOP, op);
		D3D::SetRenderState(D3DRS_SRCBLEND, d3dSrcFactors[srcidx]);
		D3D::SetRenderState(D3DRS_DESTBLEND, d3dDestFactors[dstidx]);
		if (g_ActiveConfig.backend_info.bSupportsSeparateAlphaFunction)
		{
			if (use_DualSource)
			{			
				op = D3DBLENDOP_ADD;
				srcidx = BlendMode::ONE;
				dstidx = BlendMode::ZERO;
			}
			else
			{
				// we can't use D3DBLEND_DESTCOLOR or D3DBLEND_INVDESTCOLOR for source in alpha channel so use their alpha equivalent instead
				if (srcidx == BlendMode::DSTCLR) srcidx = BlendMode::DSTALPHA;
				if (srcidx == BlendMode::INVDSTCLR) srcidx = BlendMode::INVDSTALPHA;
				// we can't use D3DBLEND_SRCCOLOR or D3DBLEND_INVSRCCOLOR for destination in alpha channel so use their alpha equivalent instead
				if (dstidx == BlendMode::SRCCLR) dstidx = BlendMode::SRCALPHA;
				if (dstidx == BlendMode::INVSRCCLR) dstidx = BlendMode::INVSRCALPHA;
			}
			D3D::SetRenderState(D3DRS_BLENDOPALPHA, op);
			D3D::SetRenderState(D3DRS_SRCBLENDALPHA, d3dSrcFactors[srcidx]);
			D3D::SetRenderState(D3DRS_DESTBLENDALPHA, d3dDestFactors[dstidx]);
		}		
	}	
}

void Renderer::SetGenerationMode()
{
	m_bGenerationModeChanged = true;
}

void Renderer::_SetGenerationMode()
{
	m_bGenerationModeChanged = false;
	const D3DCULL d3dCullModes[4] =
	{
		D3DCULL_NONE,
		D3DCULL_CCW,
		D3DCULL_CW,
		D3DCULL_CCW
	};
	D3D::SetRenderState(D3DRS_CULLMODE, d3dCullModes[bpmem.genMode.cullmode]);
}

void Renderer::SetDepthMode()
{
	m_bDepthModeChanged = true;
}

void Renderer::_SetDepthMode()
{
	m_bDepthModeChanged = false;
	const D3DCMPFUNC d3dCmpFuncs[8] =
	{
		D3DCMP_NEVER,
		D3DCMP_GREATER,
		D3DCMP_EQUAL,
		D3DCMP_GREATEREQUAL,
		D3DCMP_LESS,
		D3DCMP_NOTEQUAL,
		D3DCMP_LESSEQUAL,
		D3DCMP_ALWAYS
	};

	D3D::SetRenderState(D3DRS_ZENABLE, bpmem.zmode.testenable);
	if (bpmem.zmode.testenable)
	{		
		D3D::SetRenderState(D3DRS_ZWRITEENABLE, bpmem.zmode.updateenable);
		D3D::SetRenderState(D3DRS_ZFUNC, d3dCmpFuncs[bpmem.zmode.func]);
	}
	else
	{
		// if the test is disabled write is disabled too		
		D3D::SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		D3D::SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
	}
}

void Renderer::SetLogicOpMode()
{
	m_bLogicOpModeChanged = true;
}

void Renderer::_SetLogicOpMode()
{
	m_bLogicOpModeChanged = false;
	// D3D9 doesn't support logic blending, so this is a huge hack

	//		0	0x00
	//		1	Source & destination
	//		2	Source & ~destination
	//		3	Source
	//		4	~Source & destination
	//		5	Destination
	//		6	Source ^ destination =  Source & ~destination | ~Source & destination
	//		7	Source | destination
	//		8	~(Source | destination)
	//		9	~(Source ^ destination) = ~Source & ~destination | Source & destination
	//		10	~Destination
	//		11	Source | ~destination
	//		12	~Source
	//		13	~Source | destination
	//		14	~(Source & destination)
	//		15	0xff
	const D3DBLENDOP d3dLogicOpop[16] =
	{
		D3DBLENDOP_ADD,
		D3DBLENDOP_ADD,
		D3DBLENDOP_SUBTRACT,
		D3DBLENDOP_ADD,
		D3DBLENDOP_REVSUBTRACT,
		D3DBLENDOP_ADD,
		D3DBLENDOP_MAX,
		D3DBLENDOP_ADD,
		D3DBLENDOP_MAX,
		D3DBLENDOP_MAX,
		D3DBLENDOP_ADD,
		D3DBLENDOP_ADD,
		D3DBLENDOP_ADD,
		D3DBLENDOP_ADD,
		D3DBLENDOP_ADD,
		D3DBLENDOP_ADD
	};
	const D3DBLEND d3dLogicOpSrcFactors[16] =
	{
		D3DBLEND_ZERO,
		D3DBLEND_DESTCOLOR,
		D3DBLEND_ONE,
		D3DBLEND_ONE,
		D3DBLEND_DESTCOLOR,
		D3DBLEND_ZERO,
		D3DBLEND_INVDESTCOLOR,
		D3DBLEND_INVDESTCOLOR,
		D3DBLEND_INVSRCCOLOR,
		D3DBLEND_INVSRCCOLOR,
		D3DBLEND_INVDESTCOLOR,
		D3DBLEND_ONE,
		D3DBLEND_INVSRCCOLOR,
		D3DBLEND_INVSRCCOLOR,
		D3DBLEND_INVDESTCOLOR,
		D3DBLEND_ONE
	};
	const D3DBLEND d3dLogicOpDestFactors[16] =
	{
		D3DBLEND_ZERO,
		D3DBLEND_ZERO,
		D3DBLEND_INVSRCCOLOR,
		D3DBLEND_ZERO,
		D3DBLEND_ONE,
		D3DBLEND_ONE,
		D3DBLEND_INVSRCCOLOR,
		D3DBLEND_ONE,
		D3DBLEND_INVDESTCOLOR,
		D3DBLEND_SRCCOLOR,
		D3DBLEND_INVDESTCOLOR,
		D3DBLEND_INVDESTCOLOR,
		D3DBLEND_INVSRCCOLOR,
		D3DBLEND_ONE,
		D3DBLEND_INVSRCCOLOR,
		D3DBLEND_ONE
	};

	if (bpmem.blendmode.logicopenable && !bpmem.blendmode.blendenable)
	{
		D3D::SetRenderState(D3DRS_ALPHABLENDENABLE, true);
		D3D::SetRenderState(D3DRS_BLENDOP, d3dLogicOpop[bpmem.blendmode.logicmode]);
		D3D::SetRenderState(D3DRS_SRCBLEND, d3dLogicOpSrcFactors[bpmem.blendmode.logicmode]);
		D3D::SetRenderState(D3DRS_DESTBLEND, d3dLogicOpDestFactors[bpmem.blendmode.logicmode]);
	}
	else
	{
		_SetBlendMode(false);
	}
}

void Renderer::SetDitherMode()
{
	// No way to emulate this properly with the defaul pipeline
	// and teorically we don't need it because we alway use full color precision.
	// To Allow a correct emulation  the output of the pixel shaders
	// should be reduced to the decired precision and then
	// a postproccesing shader should be used to emulate the correct
	// dithering matrix and the aproximation equation	
}

void Renderer::SetInterlacingMode()
{
	// TODO
}

void Renderer::SetSamplerState(int stage, int texindex, bool custom_tex)
{
	const D3DTEXTUREFILTERTYPE d3dMipFilters[4] =
	{
		D3DTEXF_NONE,
		D3DTEXF_POINT,
		D3DTEXF_LINEAR,
		D3DTEXF_NONE, //reserved
	};
	const D3DTEXTUREADDRESS d3dClamps[4] =
	{
		D3DTADDRESS_CLAMP,
		D3DTADDRESS_WRAP,
		D3DTADDRESS_MIRROR,
		D3DTADDRESS_WRAP //reserved
	};

	const FourTexUnits &tex = bpmem.tex[texindex];
	const TexMode0 &tm0 = tex.texMode0[stage];
	const TexMode1 &tm1 = tex.texMode1[stage];

	D3DTEXTUREFILTERTYPE min, mag, mip;
	if (g_ActiveConfig.bForceFiltering)
	{
		min = mag = mip = D3DTEXF_LINEAR;
	}
	else if (g_ActiveConfig.bDisableTextureFiltering)
	{
		min = mag = mip = D3DTEXF_NONE;
	}
	else
	{
		min = (tm0.min_filter & 4) ? D3DTEXF_LINEAR : D3DTEXF_POINT;
		mag = tm0.mag_filter ? D3DTEXF_LINEAR : D3DTEXF_POINT;
		mip = d3dMipFilters[tm0.min_filter & 3];
	}
	if (texindex)
		stage += 4;

	if (mag == D3DTEXF_LINEAR && min == D3DTEXF_LINEAR && g_ActiveConfig.iMaxAnisotropy)
	{
		min = D3DTEXF_ANISOTROPIC;
	}
	D3D::SetSamplerState(stage, D3DSAMP_MINFILTER, min);
	D3D::SetSamplerState(stage, D3DSAMP_MAGFILTER, mag);
	D3D::SetSamplerState(stage, D3DSAMP_MIPFILTER, mip);

	D3D::SetSamplerState(stage, D3DSAMP_ADDRESSU, d3dClamps[tm0.wrap_s]);
	D3D::SetSamplerState(stage, D3DSAMP_ADDRESSV, d3dClamps[tm0.wrap_t]);

	float lodbias = (s32)tm0.lod_bias / 32.0f;
	D3D::SetSamplerState(stage, D3DSAMP_MIPMAPLODBIAS, *(DWORD*)&lodbias);
	D3D::SetSamplerState(stage, D3DSAMP_MAXMIPLEVEL, tm1.min_lod >> 4);
}

int Renderer::GetMaxTextureSize()
{
	return D3D::GetCaps().MaxTextureWidth;
}

}  // namespace DX9
