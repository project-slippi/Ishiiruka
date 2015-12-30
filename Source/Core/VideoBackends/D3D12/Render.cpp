// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <cmath>
#include <string>
#include <strsafe.h>
#include <unordered_map>

#include "Common/MathUtil.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"

#include "VideoBackends/D3D12/BoundingBox.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/GeometryShaderCache.h"
#include "VideoBackends/D3D12/NativeVertexFormat.h"
#include "VideoBackends/D3D12/PixelShaderCache.h"
#include "VideoBackends/D3D12/Render.h"
#include "VideoBackends/D3D12/Television.h"
#include "VideoBackends/D3D12/TextureCache.h"
#include "VideoBackends/D3D12/VertexShaderCache.h"

#include "VideoCommon/AVIDump.h"
#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/FPSCounter.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{

static u32 s_last_multisamples = 1;
static bool s_last_stereo_mode = false;
static bool s_last_xfb_mode = false;

static Television s_television;

ID3D12Resource* access_efb_cbuf12 = nullptr;

D3D12_BLEND_DESC clearblendstates12[4] = {};
D3D12_DEPTH_STENCIL_DESC cleardepthstates12[3] = {};
D3D12_BLEND_DESC resetblendstate12 = {};
D3D12_DEPTH_STENCIL_DESC resetdepthstate12 = {};
D3D12_RASTERIZER_DESC resetraststate12 = {};

static ID3D12Resource* s_screenshot_texture = nullptr;
static void* s_screenshot_textureData = nullptr;
static D3DTexture2D* s_3d_vision_texture = nullptr;

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
	ZMode zmode;
	RasterizerState raster;

} gx_state;

StateCache gx_state_cache;

static void SetupDeviceObjects()
{
	s_television.Init();

	g_framebuffer_manager = std::make_unique<FramebufferManager>();

	float colmat[20]= {0.0f};
	colmat[0] = colmat[5] = colmat[10] = 1.0f;

	CheckHR(
		D3D::device12->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(20 * sizeof(float)),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&access_efb_cbuf12)
			)
		);

	// Copy inital data to access_efb_cbuf12.
	void *pAccessEfbCbuf12InitialData = nullptr;
	CheckHR(access_efb_cbuf12->Map(0, nullptr, &pAccessEfbCbuf12InitialData));
	memcpy(pAccessEfbCbuf12InitialData, colmat, sizeof(colmat));

	D3D12_DEPTH_STENCIL_DESC ddesc;
	ddesc.DepthEnable      = FALSE;
	ddesc.DepthWriteMask   = D3D12_DEPTH_WRITE_MASK_ZERO;
	ddesc.DepthFunc        = D3D12_COMPARISON_FUNC_ALWAYS;
	ddesc.StencilEnable    = FALSE;
	ddesc.StencilReadMask  = D3D11_DEFAULT_STENCIL_READ_MASK;
	ddesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	cleardepthstates12[0] = ddesc;

	ddesc.DepthWriteMask  = D3D12_DEPTH_WRITE_MASK_ALL;
	ddesc.DepthEnable     = TRUE;
	cleardepthstates12[1] = ddesc;

	ddesc.DepthWriteMask  = D3D12_DEPTH_WRITE_MASK_ZERO;
	cleardepthstates12[2] = ddesc;

	D3D12_BLEND_DESC blenddesc;
	blenddesc.AlphaToCoverageEnable = FALSE;
	blenddesc.IndependentBlendEnable = FALSE;
	blenddesc.RenderTarget[0].LogicOpEnable = FALSE;
	blenddesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	blenddesc.RenderTarget[0].BlendEnable = FALSE;
	blenddesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blenddesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	blenddesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	blenddesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blenddesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blenddesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blenddesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	resetblendstate12 = blenddesc;
	clearblendstates12[0] = resetblendstate12;

	blenddesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED|D3D12_COLOR_WRITE_ENABLE_GREEN|D3D12_COLOR_WRITE_ENABLE_BLUE;
	clearblendstates12[1] = blenddesc;

	blenddesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALPHA;
	clearblendstates12[2] = blenddesc;

	blenddesc.RenderTarget[0].RenderTargetWriteMask = 0;
	clearblendstates12[3] = blenddesc;

	ddesc.DepthEnable      = FALSE;
	ddesc.DepthWriteMask   = D3D12_DEPTH_WRITE_MASK_ZERO;
	ddesc.DepthFunc        = D3D12_COMPARISON_FUNC_LESS;
	ddesc.StencilEnable    = FALSE;
	ddesc.StencilReadMask  = D3D12_DEFAULT_STENCIL_READ_MASK;
	ddesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

	resetdepthstate12 = ddesc;

	D3D12_RASTERIZER_DESC rastdesc = CD3DX12_RASTERIZER_DESC(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_NONE, false, 0, 0.f, 0.f, false, false, false, 0, D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF);
	resetraststate12 = rastdesc;

	s_screenshot_texture = nullptr;
	s_screenshot_textureData = nullptr;
}

// Kill off all device objects
static void TeardownDeviceObjects()
{
	g_framebuffer_manager.reset();

	if (s_screenshot_texture)
	{
		D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(s_screenshot_texture);
		s_screenshot_texture = nullptr;
	}

	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(access_efb_cbuf12);
	access_efb_cbuf12 = nullptr;

	SAFE_RELEASE(s_3d_vision_texture);

	s_television.Shutdown();

	gx_state_cache.Clear();
}

void CreateScreenshotTexture()
{
	// We can't render anything outside of the backbuffer anyway, so use the backbuffer size as the screenshot buffer size.
	// This texture is released to be recreated when the window is resized in Renderer::SwapImpl.

	UINT screenshotBufferSize = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT +
		(((D3D::GetBackBufferWidth() * 4) + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)) *
		D3D::GetBackBufferHeight();

	CheckHR(
		D3D::device12->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(screenshotBufferSize),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&s_screenshot_texture)
			)
		);

	CheckHR(s_screenshot_texture->Map(0, nullptr, &s_screenshot_textureData));
}

static D3D12_BOX GetScreenshotSourceBox(const TargetRectangle& targetRc)
{
	// Since the screenshot buffer is copied back to the CPU, we can't access pixels that
	// fall outside the backbuffer bounds. Therefore, when crop is enabled and the target rect is
	// off-screen to the top/left, we clamp the origin at zero, as well as the bottom/right
	// coordinates at the backbuffer dimensions. This will result in a rectangle that can be
	// smaller than the backbuffer, but never larger.

	return CD3DX12_BOX(
		std::max(targetRc.left, 0),
		std::max(targetRc.top, 0),
		0,
		std::min(D3D::GetBackBufferWidth(), (unsigned int)targetRc.right),
		std::min(D3D::GetBackBufferHeight(), (unsigned int)targetRc.bottom),
		1);
}

// D3D12TODO: Validate this on D3D12.
static void Create3DVisionTexture(int width, int height)
{
	// Create a staging texture for 3D vision with signature information in the last row.
	// Nvidia 3D Vision supports full SBS, so there is no loss in resolution during this process.
	D3D12_SUBRESOURCE_DATA sysData;
	sysData.RowPitch = 4 * width * 2;
	sysData.pData = new u8[(height + 1) * sysData.RowPitch];
	LPNVSTEREOIMAGEHEADER header = (LPNVSTEREOIMAGEHEADER)((u8*)sysData.pData + height * sysData.RowPitch);
	header->dwSignature = NVSTEREO_IMAGE_SIGNATURE;
	header->dwWidth = width * 2;
	header->dwHeight = height + 1;
	header->dwBPP = 32;
	header->dwFlags = 0;

	s_3d_vision_texture = D3DTexture2D::Create(width * 2, height + 1, D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, &sysData);
	delete[] sysData.pData;
}

Renderer::Renderer(void *&window_handle)
{
	D3D::Create((HWND)window_handle);

	s_backbuffer_width = D3D::GetBackBufferWidth();
	s_backbuffer_height = D3D::GetBackBufferHeight();

	FramebufferManagerBase::SetLastXfbWidth(MAX_XFB_WIDTH);
	FramebufferManagerBase::SetLastXfbHeight(MAX_XFB_HEIGHT);

	UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);

	s_last_multisamples = g_ActiveConfig.iMultisamples;
	s_last_efb_scale = g_ActiveConfig.iEFBScale;
	s_last_stereo_mode = g_ActiveConfig.iStereoMode > 0;
	s_last_xfb_mode = g_ActiveConfig.bUseRealXFB;
	CalculateTargetSize(s_backbuffer_width, s_backbuffer_height);
	PixelShaderManager::SetEfbScaleChanged();

	SetupDeviceObjects();

	// Setup GX pipeline state
	gx_state.blend.blend_enable = false;
	gx_state.blend.write_mask = D3D11_COLOR_WRITE_ENABLE_ALL;
	gx_state.blend.src_blend = D3D12_BLEND_ONE;
	gx_state.blend.dst_blend = D3D12_BLEND_ZERO;
	gx_state.blend.blend_op = D3D12_BLEND_OP_ADD;
	gx_state.blend.use_dst_alpha = false;

	for (unsigned int k = 0;k < 8;k++)
	{
		gx_state.sampler[k].packed = 0;
	}

	gx_state.zmode.testenable = false;
	gx_state.zmode.updateenable = false;
	gx_state.zmode.func = ZMode::NEVER;

	gx_state.raster.cull_mode = D3D12_CULL_MODE_NONE;

	// Clear EFB textures
	float ClearColor[4] = { 0.f, 0.f, 0.f, 1.f };
	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE );
	D3D::current_command_list->ClearRenderTargetView(FramebufferManager::GetEFBColorTexture()->GetRTV12(), ClearColor, 0, nullptr);
	D3D::current_command_list->ClearDepthStencilView(FramebufferManager::GetEFBDepthTexture()->GetDSV12(), D3D12_CLEAR_FLAG_DEPTH, 0.f, 0, 0, nullptr);

	D3D12_VIEWPORT vp12 = { 0.f, 0.f, (float)s_target_width, (float)s_target_height, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D::current_command_list->RSSetViewports(1, &vp12);

	// Already transitioned to appropriate states a few lines up for the clears.
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());

	D3D::BeginFrame();
}

Renderer::~Renderer()
{
	D3D::EndFrame();
	D3D::WaitForOutstandingRenderingToComplete();
	TeardownDeviceObjects();
	D3D::Close();
}

void Renderer::RenderText(const std::string& text, int left, int top, u32 color)
{
	D3D::font.DrawTextScaled((float)(left+1), (float)(top+1), 20.f, 0.0f, color & 0xFF000000, text);
	D3D::font.DrawTextScaled((float)left, (float)top, 20.f, 0.0f, color, text);
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

// With D3D, we have to resize the backbuffer if the window changed
// size.
__declspec(noinline) bool Renderer::CheckForResize()
{
	RECT rcWindow;
	GetClientRect(D3D::hWnd, &rcWindow);
	int client_width = rcWindow.right - rcWindow.left;
	int client_height = rcWindow.bottom - rcWindow.top;

	// Sanity check
	if ((client_width != Renderer::GetBackbufferWidth() ||
		client_height != Renderer::GetBackbufferHeight()) &&
		client_width >= 4 && client_height >= 4)
	{
		return true;
	}

	return false;
}

TargetRectangle currentRectangle = {};

void Renderer::SetScissorRect(const TargetRectangle& rc)
{
	D3D::current_command_list->RSSetScissorRects(1, rc.AsRECT());

	currentRectangle = rc;
}

const TargetRectangle& Renderer::GetScissorRect()
{
	return currentRectangle;
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

	D3D::command_list_mgr->m_dirty_pso = true;

}

// This function allows the CPU to directly access the EFB.
// There are EFB peeks (which will read the color or depth of a pixel)
// and EFB pokes (which will change the color or depth of a pixel).
//
// The behavior of EFB peeks can only be modified by:
//  - GX_PokeAlphaRead
// The behavior of EFB pokes can be modified by:
//  - GX_PokeAlphaMode (TODO)
//  - GX_PokeAlphaUpdate (TODO)
//  - GX_PokeBlendMode (TODO)
//  - GX_PokeColorUpdate (TODO)
//  - GX_PokeDither (TODO)
//  - GX_PokeDstAlpha (TODO)
//  - GX_PokeZMode (TODO)
u32 Renderer::AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data)
{
	// TODO: This function currently is broken if anti-aliasing is enabled
	void* map12;
	ID3D12Resource* read_tex12;

	// Convert EFB dimensions to the ones of our render target
	EFBRectangle efbPixelRc;
	efbPixelRc.left = x;
	efbPixelRc.top = y;
	efbPixelRc.right = x + 1;
	efbPixelRc.bottom = y + 1;
	TargetRectangle targetPixelRc = Renderer::ConvertEFBRectangle(efbPixelRc);

	// Take the mean of the resulting dimensions; TODO: Don't use the center pixel, compute the average color instead
	D3D12_RECT RectToLock;
	if (type == PEEK_COLOR || type == PEEK_Z)
	{
		RectToLock.left = (targetPixelRc.left + targetPixelRc.right) / 2;
		RectToLock.top = (targetPixelRc.top + targetPixelRc.bottom) / 2;
		RectToLock.right = RectToLock.left + 1;
		RectToLock.bottom = RectToLock.top + 1;
	}
	else
	{
		RectToLock.left = targetPixelRc.left;
		RectToLock.right = targetPixelRc.right;
		RectToLock.top = targetPixelRc.top;
		RectToLock.bottom = targetPixelRc.bottom;
	}

	if (type == PEEK_Z)
	{
#ifdef USE_D3D12_FREQUENT_EXECUTION
		D3D::command_list_mgr->CPUAccessNotify();
#endif

		// depth buffers can only be completely CopySubresourceRegion'ed, so we're using DrawShadedTexQuad instead
		// D3D12TODO: Is above statement true on D3D12?
		D3D12_VIEWPORT vp12 = { 0.f, 0.f, 1.f, 1.f, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
		D3D::current_command_list->RSSetViewports(1, &vp12);

		D3D::current_command_list->SetGraphicsRootConstantBufferView(DESCRIPTOR_TABLE_PS_CBVONE, access_efb_cbuf12->GetGPUVirtualAddress());
		D3D::command_list_mgr->m_dirty_ps_cbv = true;

		FramebufferManager::GetEFBDepthReadTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBDepthReadTexture()->GetRTV12(), FALSE, nullptr);

		D3D::SetPointCopySampler();
		
		D3D::DrawShadedTexQuad(
			FramebufferManager::GetEFBDepthTexture(),
			&RectToLock,
			Renderer::GetTargetWidth(),
			Renderer::GetTargetHeight(),
			PixelShaderCache::GetColorCopyProgram12(true),
			VertexShaderCache::GetSimpleVertexShader12(),
			VertexShaderCache::GetSimpleInputLayout12(),
			D3D12_SHADER_BYTECODE(),
			1.0f,
			0,
			DXGI_FORMAT_R32_FLOAT,
			false,
			FramebufferManager::GetEFBDepthReadTexture()->GetMultisampled()
			);

		// copy to system memory
		D3D12_BOX box12 = CD3DX12_BOX(0, 0, 0, 1, 1, 1);
		read_tex12 = FramebufferManager::GetEFBDepthStagingBuffer12();

		D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
		dstLocation.pResource = read_tex12;
		dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dstLocation.PlacedFootprint.Offset = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
		dstLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32_FLOAT;
		dstLocation.PlacedFootprint.Footprint.Width = 1;
		dstLocation.PlacedFootprint.Footprint.Height = 1;
		dstLocation.PlacedFootprint.Footprint.Depth = 1;
		dstLocation.PlacedFootprint.Footprint.RowPitch = 1 * 4 /* width * 32bpp */;
		dstLocation.PlacedFootprint.Footprint.RowPitch = (dstLocation.PlacedFootprint.Footprint.RowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);

		D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
		srcLocation.pResource = FramebufferManager::GetEFBDepthReadTexture()->GetTex12();
		srcLocation.SubresourceIndex = 0;
		srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

		FramebufferManager::GetEFBDepthReadTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
		D3D::current_command_list->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, &box12);

		// Need to wait for the CPU to complete the copy (and all prior operations) before we can read it on the CPU.
		D3D::command_list_mgr->ExecuteQueuedWork(true);

		FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE );
		D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());

		// Restores proper viewport/scissor settings.
		g_renderer->RestoreAPIState();

		// read the data from system memory
		CheckHR(read_tex12->Map(0, nullptr, &map12));

		// depth buffer is inverted in the d3d backend
		float val12 = 1.0f - *((float*)((u8*)map12 + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT));
		u32 ret12 = 0;

		if (bpmem.zcontrol.pixel_format == PEControl::RGB565_Z16)
		{
			// if Z is in 16 bit format you must return a 16 bit integer
			ret12 = MathUtil::Clamp<u32>((u32)(val12 * 65536.0f), 0, 0xFFFF);
		}
		else
		{
			ret12 = MathUtil::Clamp<u32>((u32)(val12 * 16777216.0f), 0, 0xFFFFFF);
		}

		// TODO: in RE0 this value is often off by one in Video_DX9 (where this code is derived from), which causes lighting to disappear
		return ret12;
	}
	else if (type == PEEK_COLOR)
	{
#ifdef USE_D3D12_FREQUENT_EXECUTION
		D3D::command_list_mgr->CPUAccessNotify();
#endif

		// we can directly copy to system memory here
		read_tex12 = FramebufferManager::GetEFBColorStagingBuffer12();

		D3D12_BOX box12 = CD3DX12_BOX(RectToLock.left, RectToLock.top, 0, RectToLock.right, RectToLock.bottom, 1);

		D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
		dstLocation.pResource = read_tex12;
		dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dstLocation.PlacedFootprint.Offset = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
		dstLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dstLocation.PlacedFootprint.Footprint.Width = 1;
		dstLocation.PlacedFootprint.Footprint.Height = 1;
		dstLocation.PlacedFootprint.Footprint.Depth = 1;
		dstLocation.PlacedFootprint.Footprint.RowPitch = 1 * 4 /* width * 32bpp */;
		dstLocation.PlacedFootprint.Footprint.RowPitch = (dstLocation.PlacedFootprint.Footprint.RowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);

		D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
		srcLocation.pResource = FramebufferManager::GetResolvedEFBColorTexture()->GetTex12();
		srcLocation.SubresourceIndex = 0;
		srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

		FramebufferManager::GetResolvedEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
		D3D::current_command_list->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, &box12);

		// read the data from system memory
		CheckHR(read_tex12->Map(0, nullptr, &map12));

		u32 ret12 = 0;
		if (map12)
			ret12 = *(u32*)((u8*)map12 + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

		// check what to do with the alpha channel (GX_PokeAlphaRead)
		PixelEngine::UPEAlphaReadReg alpha_read_mode = PixelEngine::GetAlphaReadMode();

		if (bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24)
		{
			ret12 = RGBA8ToRGBA6ToRGBA8(ret12);
		}
		else if (bpmem.zcontrol.pixel_format == PEControl::RGB565_Z16)
		{
			ret12 = RGBA8ToRGB565ToRGBA8(ret12);
		}
		if (bpmem.zcontrol.pixel_format != PEControl::RGBA6_Z24)
		{
			ret12 |= 0xFF000000;
		}

		if (alpha_read_mode.ReadMode == 2) return ret12; // GX_READ_NONE
		else if (alpha_read_mode.ReadMode == 1) return (ret12 | 0xFF000000); // GX_READ_FF
		else /*if(alpha_read_mode.ReadMode == 0)*/ return (ret12 & 0x00FFFFFF); // GX_READ_00
	}	
	return poke_data;
}

void Renderer::PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points)
{
	D3D12_BLEND_DESC* blend_desc = nullptr;
	D3D12_DEPTH_STENCIL_DESC* depth_desc = nullptr;
	if (type == POKE_COLOR)
	{
		blend_desc = &resetblendstate12;
		depth_desc = &resetdepthstate12;
		D3D12_VIEWPORT vp12 = { 0.0f, 0.0f, (float)GetTargetWidth(), (float)GetTargetHeight(), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
		D3D::current_command_list->RSSetViewports(1, &vp12);

		FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, nullptr);
	}
	else // if (type == POKE_Z)
	{
		blend_desc = &clearblendstates12[3];
		depth_desc = &cleardepthstates12[1];
		D3D12_VIEWPORT vp12 = { 0.0f, 0.0f, (float)GetTargetWidth(), (float)GetTargetHeight(),
			1.0f - MathUtil::Clamp<float>(xfmem.viewport.farZ, 0.0f, 16777215.0f) / 16777216.0f,
			1.0f - MathUtil::Clamp<float>((xfmem.viewport.farZ - MathUtil::Clamp<float>(xfmem.viewport.zRange, 0.0f, 16777215.0f)), 0.0f, 16777215.0f) / 16777216.0f };
		D3D::current_command_list->RSSetViewports(1, &vp12);

		FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());
	}

	D3D::DrawEFBPokeQuads(type, points, num_points,
		blend_desc,
		depth_desc,
		FramebufferManager::GetEFBColorTexture()->GetMultisampled());
	
	g_renderer->RestoreAPIState();
}

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

	// In D3D, the viewport rectangle must fit within the render target.
	X = (X >= 0.f) ? X : 0.f;
	Y = (Y >= 0.f) ? Y : 0.f;
	Wd = (X + Wd <= GetTargetWidth()) ? Wd : (GetTargetWidth() - X);
	Ht = (Y + Ht <= GetTargetHeight()) ? Ht : (GetTargetHeight() - Y);

	D3D12_VIEWPORT vp = { X, Y, Wd, Ht, 0.0f, 1.0f };
	float nearz = xfmem.viewport.farZ - MathUtil::Clamp<float>(xfmem.viewport.zRange, 0.0f, 16777215.0f);
	float farz = xfmem.viewport.farZ;

	const bool nonStandartViewport = (nearz < 0.f || farz > 16777216.0f || nearz >= 16777216.0f || farz <= 0.f);
	if (!nonStandartViewport)
	{
		vp.MaxDepth = 1.0f - (MathUtil::Clamp<float>(nearz, 0.0f, 16777215.0f) / 16777216.0f);
		vp.MinDepth = 1.0f - (MathUtil::Clamp<float>(farz, 0.0f, 16777215.0f) / 16777216.0f);
	}
	D3D::current_command_list->RSSetViewports(1, &vp);
}

void Renderer::ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z)
{
	D3D12_BLEND_DESC *blend_desc = nullptr;

	if (colorEnable && alphaEnable) blend_desc = &clearblendstates12[0];
	else if (colorEnable) blend_desc = &clearblendstates12[1];
	else if (alphaEnable) blend_desc = &clearblendstates12[2];
	else blend_desc = &clearblendstates12[3];

	D3D12_DEPTH_STENCIL_DESC *depth_stencil_desc = nullptr;

	// TODO: Should we enable Z testing here?
	/*if (!bpmem.zmode.testenable) depth_stencil_desc = &cleardepthstates[0];
	else */if (zEnable) depth_stencil_desc = &cleardepthstates12[1];
	else /*if (!zEnable)*/ depth_stencil_desc = &cleardepthstates12[2];

	// Update the view port for clearing the picture
	TargetRectangle targetRc = Renderer::ConvertEFBRectangle(rc);
	
	D3D12_VIEWPORT vp12 = { (float)targetRc.left, (float)targetRc.top, (float)targetRc.GetWidth(), (float)targetRc.GetHeight(), 0.f, 1.f };
	D3D::current_command_list->RSSetViewports(1, &vp12);

	// Color is passed in bgra mode so we need to convert it to rgba
	u32 rgbaColor = (color & 0xFF00FF00) | ((color >> 16) & 0xFF) | ((color << 16) & 0xFF0000);
	D3D::DrawClearQuad(rgbaColor, 1.0f - (z & 0xFFFFFF) / 16777216.0f, blend_desc, depth_stencil_desc, FramebufferManager::GetEFBColorTexture()->GetMultisampled());

	// Restores proper viewport/scissor settings.
	g_renderer->RestoreAPIState();
}

void Renderer::ReinterpretPixelData(unsigned int convtype)
{
	// TODO: MSAA support..
	D3D12_RECT source = CD3DX12_RECT(0, 0, g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight());

	D3D12_SHADER_BYTECODE pixel_shader12 = {};

	if (convtype == 0)
	{
		pixel_shader12 = PixelShaderCache::ReinterpRGB8ToRGBA612(true);
	}
	else if (convtype == 2)
	{
		pixel_shader12 = PixelShaderCache::ReinterpRGBA6ToRGB812(true);
	} 
	else
	{
		ERROR_LOG(VIDEO, "Trying to reinterpret pixel data with unsupported conversion type %d", convtype);
		return;
	}

	D3D12_VIEWPORT vp12 = { 0.f, 0.f, (float)g_renderer->GetTargetWidth(), (float)g_renderer->GetTargetHeight(), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D::current_command_list->RSSetViewports(1, &vp12);

	FramebufferManager::GetEFBColorTempTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTempTexture()->GetRTV12(), FALSE, nullptr);

	D3D::SetPointCopySampler();
	D3D::DrawShadedTexQuad(
		FramebufferManager::GetEFBColorTexture(),
		&source,
		g_renderer->GetTargetWidth(),
		g_renderer->GetTargetHeight(),
		pixel_shader12,
		VertexShaderCache::GetSimpleVertexShader12(),
		VertexShaderCache::GetSimpleInputLayout12(),
		GeometryShaderCache::GetCopyGeometryShader12(),
		1.0f,
		0,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		false,
		FramebufferManager::GetEFBColorTempTexture()->GetMultisampled()
		);

	// Restores proper viewport/scissor settings.
	g_renderer->RestoreAPIState();

	FramebufferManager::SwapReinterpretTexture();

	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE );
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());
}

void Renderer::SetBlendMode(bool forceUpdate)
{
	// Our render target always uses an alpha channel, so we need to override the blend functions to assume a destination alpha of 1 if the render target isn't supposed to have an alpha channel
	// Example: D3DBLEND_DESTALPHA needs to be D3DBLEND_ONE since the result without an alpha channel is assumed to always be 1.
	bool target_has_alpha = bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24;
	const D3D12_BLEND d3dSrcFactors[8] =
	{
		D3D12_BLEND_ZERO,
		D3D12_BLEND_ONE,
		D3D12_BLEND_DEST_COLOR,
		D3D12_BLEND_INV_DEST_COLOR,
		D3D12_BLEND_SRC_ALPHA,
		D3D12_BLEND_INV_SRC_ALPHA, // NOTE: Use SRC1_ALPHA if dst alpha is enabled!
		(target_has_alpha) ? D3D12_BLEND_DEST_ALPHA : D3D12_BLEND_ONE,
		(target_has_alpha) ? D3D12_BLEND_INV_DEST_ALPHA : D3D12_BLEND_ZERO
	};
	const D3D12_BLEND d3dDestFactors[8] =
	{
		D3D12_BLEND_ZERO,
		D3D12_BLEND_ONE,
		D3D12_BLEND_SRC_COLOR,
		D3D12_BLEND_INV_SRC_COLOR,
		D3D12_BLEND_SRC_ALPHA,
		D3D12_BLEND_INV_SRC_ALPHA, // NOTE: Use SRC1_ALPHA if dst alpha is enabled!
		(target_has_alpha) ? D3D12_BLEND_DEST_ALPHA : D3D12_BLEND_ONE,
		(target_has_alpha) ? D3D12_BLEND_INV_DEST_ALPHA : D3D12_BLEND_ZERO
	};

	if (bpmem.blendmode.logicopenable && !bpmem.blendmode.blendenable && !forceUpdate)
		return;

	if (bpmem.blendmode.subtract)
	{
		gx_state.blend.blend_enable = true;
		gx_state.blend.blend_op = D3D12_BLEND_OP_REV_SUBTRACT;
		gx_state.blend.src_blend = D3D12_BLEND_ONE;
		gx_state.blend.dst_blend = D3D12_BLEND_ONE;
	}
	else
	{
		gx_state.blend.blend_enable = (u32)bpmem.blendmode.blendenable;
		if (bpmem.blendmode.blendenable)
		{
			gx_state.blend.blend_op = D3D12_BLEND_OP_ADD;
			gx_state.blend.src_blend = d3dSrcFactors[bpmem.blendmode.srcfactor];
			gx_state.blend.dst_blend = d3dDestFactors[bpmem.blendmode.dstfactor];
		}
	}

	D3D::command_list_mgr->m_dirty_pso = true;
}

bool Renderer::SaveScreenshot(const std::string &filename, const TargetRectangle& rc)
{
	if (!s_screenshot_texture)
		CreateScreenshotTexture();

	// copy back buffer to system memory
	bool saved_png = false;

	D3D12_BOX source_box = GetScreenshotSourceBox(rc);

	D3D12_TEXTURE_COPY_LOCATION dst = {};
	dst.pResource = s_screenshot_texture;
	dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst.PlacedFootprint.Offset = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
	dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dst.PlacedFootprint.Footprint.Width = D3D::GetBackBufferWidth();
	dst.PlacedFootprint.Footprint.Height = D3D::GetBackBufferHeight();
	dst.PlacedFootprint.Footprint.Depth = 1;
	dst.PlacedFootprint.Footprint.RowPitch = ((dst.PlacedFootprint.Footprint.Width * 4) + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);

	D3D12_TEXTURE_COPY_LOCATION src = {};
	src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src.SubresourceIndex = 0;
	src.pResource = D3D::GetBackBuffer()->GetTex12();

	D3D::GetBackBuffer()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
	D3D::current_command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, &source_box);

	D3D::command_list_mgr->ExecuteQueuedWork(true);

	saved_png = TextureToPng((u8*)s_screenshot_textureData + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, dst.PlacedFootprint.Footprint.RowPitch, filename, source_box.right - source_box.left, source_box.bottom - source_box.top, false);

	if (saved_png)
	{
		OSD::AddMessage(StringFromFormat("Saved %i x %i %s", rc.GetWidth(),
		                                 rc.GetHeight(), filename.c_str()));
	}
	else
	{
		OSD::AddMessage(StringFromFormat("Error saving %s", filename.c_str()));
	}

	return saved_png;
}

void formatBufferDump(const u8* in, u8* out, int w, int h, int p)
{
	for (int y = 0; y < h; ++y)
	{
		auto line = (in + (h - y - 1) * p);
		for (int x = 0; x < w; ++x)
		{
			out[0] = line[2];
			out[1] = line[1];
			out[2] = line[0];
			out += 3;
			line += 4;
		}
	}
}

// This function has the final picture. We adjust the aspect ratio here.
void Renderer::SwapImpl(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, const EFBRectangle& rc, float gamma)
{
	if (g_bSkipCurrentFrame || (!XFBWrited && !g_ActiveConfig.RealXFBEnabled()) || !fbWidth || !fbHeight)
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

	// Prepare to copy the XFBs to our backbuffer
	UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);
	TargetRectangle targetRc = GetTargetRectangle();

	D3D::GetBackBuffer()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV12(), FALSE, nullptr);

	float ClearColor[4] = { 0.f, 0.f, 0.f, 1.f };
	D3D::current_command_list->ClearRenderTargetView(D3D::GetBackBuffer()->GetRTV12(), ClearColor, 0, nullptr);

	// D3D12: Because scissor-testing is always enabled, change scissor rect to backbuffer in case EFB is smaller
	// than swap chain back buffer.
	D3D12_RECT backBufferRect = { 0L, 0L, GetBackbufferWidth(), GetBackbufferHeight() };
	D3D::current_command_list->RSSetScissorRects(1, &backBufferRect);

	// activate linear filtering for the buffer copies
	D3D::SetLinearCopySampler();

	if (g_ActiveConfig.bUseXFB && g_ActiveConfig.bUseRealXFB)
	{
		// TODO: Television should be used to render Virtual XFB mode as well.
		D3D12_VIEWPORT vp12 = { (float)targetRc.left, (float)targetRc.top, (float)targetRc.GetWidth(), (float)targetRc.GetHeight(), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
		D3D::current_command_list->RSSetViewports(1, &vp12);

		s_television.Submit(xfbAddr, fbStride, fbWidth, fbHeight);
		s_television.Render();
	}
	else if (g_ActiveConfig.bUseXFB)
	{
		const XFBSource* xfbSource;

		// draw each xfb source
		for (u32 i = 0; i < xfbCount; ++i)
		{
			xfbSource = (const XFBSource*)xfbSourceList[i];

			TargetRectangle drawRc;

			// use virtual xfb with offset
			int xfbHeight = xfbSource->srcHeight;
			int xfbWidth = xfbSource->srcWidth;
			int hOffset = ((s32)xfbSource->srcAddr - (s32)xfbAddr) / ((s32)fbStride * 2);

			drawRc.top = targetRc.top + hOffset * targetRc.GetHeight() / (s32)fbHeight;
			drawRc.bottom = targetRc.top + (hOffset + xfbHeight) * targetRc.GetHeight() / (s32)fbHeight;
			drawRc.left = targetRc.left + (targetRc.GetWidth() - xfbWidth * targetRc.GetWidth() / (s32)fbStride) / 2;
			drawRc.right = targetRc.left + (targetRc.GetWidth() + xfbWidth * targetRc.GetWidth() / (s32)fbStride) / 2;

			// The following code disables auto stretch.  Kept for reference.
			// scale draw area for a 1 to 1 pixel mapping with the draw target
			//float vScale = (float)fbHeight / (float)s_backbuffer_height;
			//float hScale = (float)fbWidth / (float)s_backbuffer_width;
			//drawRc.top *= vScale;
			//drawRc.bottom *= vScale;
			//drawRc.left *= hScale;
			//drawRc.right *= hScale;

			TargetRectangle sourceRc;
			sourceRc.left = 0;
			sourceRc.top = 0;
			sourceRc.right = (int)xfbSource->texWidth;
			sourceRc.bottom = (int)xfbSource->texHeight;

			sourceRc.right -= Renderer::EFBToScaledX(fbStride - fbWidth);

			BlitScreen(sourceRc, drawRc, xfbSource->tex, xfbSource->texWidth, xfbSource->texHeight, gamma);
		}
	}
	else
	{
		TargetRectangle sourceRc = Renderer::ConvertEFBRectangle(rc);

		// TODO: Improve sampling algorithm for the pixel shader so that we can use the multisampled EFB texture as source
		D3DTexture2D* read_texture = FramebufferManager::GetResolvedEFBColorTexture();

		BlitScreen(sourceRc, targetRc, read_texture, GetTargetWidth(), GetTargetHeight(), gamma);
	}

	// done with drawing the game stuff, good moment to save a screenshot
	if (s_bScreenshot)
	{
		std::lock_guard<std::mutex> guard(s_criticalScreenshot);

		SaveScreenshot(s_sScreenshotName, GetTargetRectangle());
		s_sScreenshotName.clear();
		s_bScreenshot = false;
		s_screenshotCompleted.Set();
	}

	// Dump frames
	static int w = 0, h = 0;
	if (SConfig::GetInstance().m_DumpFrames)
	{
		static int s_recordWidth;
		static int s_recordHeight;

		if (!s_screenshot_texture)
			CreateScreenshotTexture();

		D3D12_BOX source_box = GetScreenshotSourceBox(targetRc);

		unsigned int source_width = source_box.right - source_box.left;
		unsigned int source_height = source_box.bottom - source_box.top;

		D3D12_TEXTURE_COPY_LOCATION dst = {};
		dst.pResource = s_screenshot_texture;
		dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dst.PlacedFootprint.Offset = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
		dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dst.PlacedFootprint.Footprint.Width = GetTargetRectangle().GetWidth();
		dst.PlacedFootprint.Footprint.Height = GetTargetRectangle().GetHeight();
		dst.PlacedFootprint.Footprint.Depth = 1;
		dst.PlacedFootprint.Footprint.RowPitch = ((dst.PlacedFootprint.Footprint.Width * 4) + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);

		D3D12_TEXTURE_COPY_LOCATION src = {};
		src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		src.SubresourceIndex = 0;
		src.pResource = D3D::GetBackBuffer()->GetTex12();

		D3D::GetBackBuffer()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
		D3D::current_command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, &source_box);

		D3D::command_list_mgr->ExecuteQueuedWork(true);

		if (!bLastFrameDumped)
		{
			s_recordWidth = source_width;
			s_recordHeight = source_height;
			bAVIDumping = AVIDump::Start(D3D::hWnd, s_recordWidth, s_recordHeight);
			if (!bAVIDumping)
			{
				PanicAlert("Error dumping frames to AVI.");
			}
			else
			{
				std::string msg = StringFromFormat("Dumping Frames to \"%sframedump0.avi\" (%dx%d RGB24)",
					File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), s_recordWidth, s_recordHeight);

				OSD::AddMessage(msg, 2000);
			}
		}
		if (bAVIDumping)
		{
#ifdef USE_D3D11
			D3D11_MAPPED_SUBRESOURCE map;
			D3D::context->Map(s_screenshot_texture, 0, D3D11_MAP_READ, 0, &map);
#endif

			if (frame_data.empty() || w != s_recordWidth || h != s_recordHeight)
			{
				frame_data.resize(3 * s_recordWidth * s_recordHeight);
				w = s_recordWidth;
				h = s_recordHeight;
			}
			formatBufferDump((u8*)s_screenshot_textureData + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, &frame_data[0], source_width, source_height, dst.PlacedFootprint.Footprint.RowPitch);
			AVIDump::AddFrame(&frame_data[0], source_height, source_height);
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

	// Reset viewport for drawing text
	D3D12_VIEWPORT vp12 = { 0.0f, 0.0f, (float)GetBackbufferWidth(), (float)GetBackbufferHeight(), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D::current_command_list->RSSetViewports(1, &vp12);

	Renderer::DrawDebugText();

	OSD::DrawMessages();
	D3D::EndFrame();

	TextureCacheBase::Cleanup(frameCount);

	// Enable configuration changes
	UpdateActiveConfig();
	TextureCacheBase::OnConfigChanged(g_ActiveConfig);

	SetWindowSize(fbStride, fbHeight);

	const bool windowResized = CheckForResize();
	const bool fullscreen = g_ActiveConfig.bFullscreen && !g_ActiveConfig.bBorderlessFullscreen &&
		!SConfig::GetInstance().bRenderToMain;

	bool xfbchanged = s_last_xfb_mode != g_ActiveConfig.bUseRealXFB;

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
	if (CalculateTargetSize(s_backbuffer_width, s_backbuffer_height) ||
		xfbchanged ||
		windowResized ||
		s_last_efb_scale != g_ActiveConfig.iEFBScale ||
		s_last_multisamples != g_ActiveConfig.iMultisamples ||
		s_last_stereo_mode != (g_ActiveConfig.iStereoMode > 0))
	{
		s_last_xfb_mode = g_ActiveConfig.bUseRealXFB;
		s_last_multisamples = g_ActiveConfig.iMultisamples;
		PixelShaderCache::InvalidateMSAAShaders();

		if (windowResized)
		{
			// TODO: Aren't we still holding a reference to the back buffer right now?
			D3D::Reset();

			if (s_screenshot_texture)
			{
				D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(s_screenshot_texture);
				s_screenshot_texture = nullptr;
			}

			SAFE_RELEASE(s_3d_vision_texture);
			s_backbuffer_width = D3D::GetBackBufferWidth();
			s_backbuffer_height = D3D::GetBackBufferHeight();
		}

		UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);

		s_last_efb_scale = g_ActiveConfig.iEFBScale;
		s_last_stereo_mode = g_ActiveConfig.iStereoMode > 0;

		PixelShaderManager::SetEfbScaleChanged();

		D3D::GetBackBuffer()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		D3D::current_command_list->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV12(), FALSE, nullptr);

		g_framebuffer_manager.reset();
		g_framebuffer_manager = std::make_unique<FramebufferManager>();
		float clear_col[4] = { 0.f, 0.f, 0.f, 1.f };

		FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		D3D::current_command_list->ClearRenderTargetView(FramebufferManager::GetEFBColorTexture()->GetRTV12(), clear_col, 0, nullptr);
		
		FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE );
		D3D::current_command_list->ClearDepthStencilView(FramebufferManager::GetEFBDepthTexture()->GetDSV12(), D3D12_CLEAR_FLAG_DEPTH, 0.f, 0, 0, nullptr);
	}

	// begin next frame
	RestoreAPIState();
	D3D::BeginFrame();

	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE );
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());

	SetViewport();
}

void Renderer::ResetAPIState()
{
	CHECK(0, "This should never be called.. just required for inheritance.");
}

void Renderer::RestoreAPIState()
{
	// Restores viewport/scissor rects, which might have been
	// overwritten elsewhere (particularly the viewport).
	SetViewport();
	BPFunctions::SetScissor();
}

bool bOldUseDstAlpha = false;
D3DVertexFormat* pOldVertextFormat = nullptr;

void Renderer::ApplyState(bool bUseDstAlpha)
{
	if (bUseDstAlpha != bOldUseDstAlpha)
	{
		bOldUseDstAlpha = bUseDstAlpha;
		D3D::command_list_mgr->m_dirty_pso = true;
	}

	gx_state.blend.use_dst_alpha = bUseDstAlpha;

	if (D3D::command_list_mgr->m_dirty_samplers)
	{
		D3D12_GPU_DESCRIPTOR_HANDLE samplerGroupGpuHandle;
		samplerGroupGpuHandle = D3D::sampler_descriptor_heap_mgr->GetHandleForSamplerGroup(gx_state.sampler, 8);

		D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SAMPLER, samplerGroupGpuHandle);

		D3D::command_list_mgr->m_dirty_samplers = false;
	}

	VertexShaderCache::GetConstantBuffer12();
	PixelShaderCache::GetConstantBuffer12();
	GeometryShaderCache::GetConstantBuffer12();

	if (D3D::command_list_mgr->m_dirty_pso || pOldVertextFormat != reinterpret_cast<D3DVertexFormat*>(VertexLoaderManager::GetCurrentVertexFormat()))
	{
		pOldVertextFormat = reinterpret_cast<D3DVertexFormat*>(VertexLoaderManager::GetCurrentVertexFormat());

		D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = GeometryShaderCache::GetCurrentPrimitiveTopology();
		RasterizerState modifiableRastState = gx_state.raster;

		if (topologyType != D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
		{
			modifiableRastState.cull_mode = D3D12_CULL_MODE_NONE;
		}

		SmallPsoDesc pso_desc = {
			VertexShaderCache::GetActiveShader12(),     // D3D12_SHADER_BYTECODE VS;
			PixelShaderCache::GetActiveShader12(),      // D3D12_SHADER_BYTECODE PS;
			GeometryShaderCache::GetActiveShader12(),   // D3D12_SHADER_BYTECODE GS;
			pOldVertextFormat,							// D3D12_INPUT_LAYOUT_DESC InputLayout;
			gx_state.blend,                             // BlendState BlendState;
			modifiableRastState,                        // RasterizerState RasterizerState;
			gx_state.zmode,                             // ZMode DepthStencilState;
			g_ActiveConfig.iMultisamples
		};

		if (bUseDstAlpha)
		{
			// restore actual state
			SetBlendMode(false);
			SetLogicOpMode();
		}

		ID3D12PipelineState* pso = nullptr;
		CheckHR(gx_state_cache.GetPipelineStateObjectFromCache(&pso_desc, &pso, topologyType, PixelShaderCache::GetActiveShaderUid12(), VertexShaderCache::GetActiveShaderUid12(), GeometryShaderCache::GetActiveShaderUid12()));

		D3D::current_command_list->SetPipelineState(pso);

		D3D::command_list_mgr->m_dirty_pso = false;
	}
}

void Renderer::RestoreState()
{
}

void Renderer::ApplyCullDisable()
{
	// This functionality is handled directly in ApplyState.
}

void Renderer::RestoreCull()
{
	// This functionality is handled directly in ApplyState.
}

void Renderer::SetGenerationMode()
{
	const D3D12_CULL_MODE d3dCullModes[4] =
	{
		D3D12_CULL_MODE_NONE,
		D3D12_CULL_MODE_BACK,
		D3D12_CULL_MODE_FRONT,
		D3D12_CULL_MODE_BACK
	};

	// rastdc.FrontCounterClockwise must be false for this to work
	// TODO: GX_CULL_ALL not supported, yet!
	gx_state.raster.cull_mode = d3dCullModes[bpmem.genMode.cullmode];

	D3D::command_list_mgr->m_dirty_pso = true;
}

void Renderer::SetDepthMode()
{
	gx_state.zmode.hex = bpmem.zmode.hex;

	D3D::command_list_mgr->m_dirty_pso = true;
}

void Renderer::SetLogicOpMode()
{
	// D3D11 doesn't support logic blending, so this is a huge hack
	// TODO: Make use of D3D11.1's logic blending support

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
	const D3D12_BLEND_OP d3dLogicOps[16] =
	{
		D3D12_BLEND_OP_ADD,//0
		D3D12_BLEND_OP_ADD,//1
		D3D12_BLEND_OP_SUBTRACT,//2
		D3D12_BLEND_OP_ADD,//3
		D3D12_BLEND_OP_REV_SUBTRACT,//4
		D3D12_BLEND_OP_ADD,//5
		D3D12_BLEND_OP_MAX,//6
		D3D12_BLEND_OP_ADD,//7
		D3D12_BLEND_OP_MAX,//8
		D3D12_BLEND_OP_MAX,//9
		D3D12_BLEND_OP_ADD,//10
		D3D12_BLEND_OP_ADD,//11
		D3D12_BLEND_OP_ADD,//12
		D3D12_BLEND_OP_ADD,//13
		D3D12_BLEND_OP_ADD,//14
		D3D12_BLEND_OP_ADD//15
	};
	const D3D12_BLEND d3dLogicOpSrcFactors[16] =
	{
		D3D12_BLEND_ZERO,//0
		D3D12_BLEND_DEST_COLOR,//1
		D3D12_BLEND_ONE,//2
		D3D12_BLEND_ONE,//3
		D3D12_BLEND_DEST_COLOR,//4
		D3D12_BLEND_ZERO,//5
		D3D12_BLEND_INV_DEST_COLOR,//6
		D3D12_BLEND_INV_DEST_COLOR,//7
		D3D12_BLEND_INV_SRC_COLOR,//8
		D3D12_BLEND_INV_SRC_COLOR,//9
		D3D12_BLEND_INV_DEST_COLOR,//10
		D3D12_BLEND_ONE,//11
		D3D12_BLEND_INV_SRC_COLOR,//12
		D3D12_BLEND_INV_SRC_COLOR,//13
		D3D12_BLEND_INV_DEST_COLOR,//14
		D3D12_BLEND_ONE//15
	};
	const D3D12_BLEND d3dLogicOpDestFactors[16] =
	{
		D3D12_BLEND_ZERO,//0
		D3D12_BLEND_ZERO,//1
		D3D12_BLEND_INV_SRC_COLOR,//2
		D3D12_BLEND_ZERO,//3
		D3D12_BLEND_ONE,//4
		D3D12_BLEND_ONE,//5
		D3D12_BLEND_INV_SRC_COLOR,//6
		D3D12_BLEND_ONE,//7
		D3D12_BLEND_INV_DEST_COLOR,//8
		D3D12_BLEND_SRC_COLOR,//9
		D3D12_BLEND_INV_DEST_COLOR,//10
		D3D12_BLEND_INV_DEST_COLOR,//11
		D3D12_BLEND_INV_SRC_COLOR,//12
		D3D12_BLEND_ONE,//13
		D3D12_BLEND_INV_SRC_COLOR,//14
		D3D12_BLEND_ONE//15
	};

	if (bpmem.blendmode.logicopenable && !bpmem.blendmode.blendenable)
	{
		gx_state.blend.blend_enable = true;
		gx_state.blend.blend_op = d3dLogicOps[bpmem.blendmode.logicmode];
		gx_state.blend.src_blend = d3dLogicOpSrcFactors[bpmem.blendmode.logicmode];
		gx_state.blend.dst_blend = d3dLogicOpDestFactors[bpmem.blendmode.logicmode];
	}
	else
	{
		SetBlendMode(true);
	}

	D3D::command_list_mgr->m_dirty_pso = true;
}

void Renderer::SetDitherMode()
{
	// TODO: Set dither mode to bpmem.blendmode.dither
}

SamplerState oldState[8] = {};

void Renderer::SetSamplerState(int stage, int texindex, bool custom_tex)
{
	const FourTexUnits &tex = bpmem.tex[texindex];
	const TexMode0 &tm0 = tex.texMode0[stage];
	const TexMode1 &tm1 = tex.texMode1[stage];

	if (texindex)
		stage += 4;

	if (g_ActiveConfig.bForceFiltering)
	{
		gx_state.sampler[stage].min_filter = 6; // 4 (linear mip) | 2 (linear min)
		gx_state.sampler[stage].mag_filter = 1; // linear mag
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

	if (gx_state.sampler[stage].packed != oldState[stage].packed)
	{
		D3D::command_list_mgr->m_dirty_samplers = true;
		oldState[stage].packed = gx_state.sampler[stage].packed;
	}
}

void Renderer::SetInterlacingMode()
{
	// TODO
}

int Renderer::GetMaxTextureSize()
{
	return DX12::D3D::GetMaxTextureSize();
}

u16 Renderer::BBoxRead(int index)
{
	// Here we get the min/max value of the truncated position of the upscaled framebuffer.
	// So we have to correct them to the unscaled EFB sizes.
	int value = BBox::Get(index);

	if (index < 2)
	{
		// left/right
		value = value * EFB_WIDTH / s_target_width;
	}
	else
	{
		// up/down
		value = value * EFB_HEIGHT / s_target_height;
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
		value = value * s_target_width / EFB_WIDTH;
	}
	else
	{
		value = value * s_target_height / EFB_HEIGHT;
	}

	BBox::Set(index, value);
}

void Renderer::BlitScreen(TargetRectangle src, TargetRectangle dst, D3DTexture2D* src_texture, u32 src_width, u32 src_height, float gamma)
{
	if (g_ActiveConfig.iStereoMode == STEREO_SBS || g_ActiveConfig.iStereoMode == STEREO_TAB)
	{
		TargetRectangle leftRc, rightRc;
		ConvertStereoRectangle(dst, leftRc, rightRc);

		D3D12_VIEWPORT leftVp12 = { (float)leftRc.left, (float)leftRc.top, (float)leftRc.GetWidth(), (float)leftRc.GetHeight(), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
		D3D12_VIEWPORT rightVp12 = { (float)rightRc.left, (float)rightRc.top, (float)rightRc.GetWidth(), (float)rightRc.GetHeight(), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };

		// Swap chain backbuffer is never multisampled..

		D3D::current_command_list->RSSetViewports(1, &leftVp12);
		D3D::DrawShadedTexQuad(src_texture, src.AsRECT(), src_width, src_height, PixelShaderCache::GetColorCopyProgram12(false), VertexShaderCache::GetSimpleVertexShader12(), VertexShaderCache::GetSimpleInputLayout12(), D3D12_SHADER_BYTECODE(), gamma, 0, DXGI_FORMAT_R8G8B8A8_UNORM, false, false);

		D3D::current_command_list->RSSetViewports(1, &rightVp12);
		D3D::DrawShadedTexQuad(src_texture, src.AsRECT(), src_width, src_height, PixelShaderCache::GetColorCopyProgram12(false), VertexShaderCache::GetSimpleVertexShader12(), VertexShaderCache::GetSimpleInputLayout12(), D3D12_SHADER_BYTECODE(), gamma, 1, DXGI_FORMAT_R8G8B8A8_UNORM, false, false);
	}
	else if (g_ActiveConfig.iStereoMode == STEREO_3DVISION)
	{
		if (!s_3d_vision_texture)
			Create3DVisionTexture(s_backbuffer_width, s_backbuffer_height);

		D3D12_VIEWPORT leftVp12 = { (float)dst.left, (float)dst.top, (float)dst.GetWidth(), (float)dst.GetHeight(), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
		D3D12_VIEWPORT rightVp12 = { (float)(dst.left + s_backbuffer_width), (float)dst.top, (float)dst.GetWidth(), (float)dst.GetHeight(), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };

		// Render to staging texture which is double the width of the backbuffer
		s_3d_vision_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		D3D::current_command_list->OMSetRenderTargets(1, &s_3d_vision_texture->GetRTV12(), FALSE, nullptr);

		D3D::current_command_list->RSSetViewports(1, &leftVp12);
		D3D::DrawShadedTexQuad(src_texture, src.AsRECT(), src_width, src_height, PixelShaderCache::GetColorCopyProgram12(false), VertexShaderCache::GetSimpleVertexShader12(), VertexShaderCache::GetSimpleInputLayout12(), D3D12_SHADER_BYTECODE(), gamma, 0, DXGI_FORMAT_R8G8B8A8_UNORM, false, s_3d_vision_texture->GetMultisampled());

		D3D::current_command_list->RSSetViewports(1, &rightVp12);
		D3D::DrawShadedTexQuad(src_texture, src.AsRECT(), src_width, src_height, PixelShaderCache::GetColorCopyProgram12(false), VertexShaderCache::GetSimpleVertexShader12(), VertexShaderCache::GetSimpleInputLayout12(), D3D12_SHADER_BYTECODE(), gamma, 1, DXGI_FORMAT_R8G8B8A8_UNORM, false, s_3d_vision_texture->GetMultisampled());

		// Copy the left eye to the backbuffer, if Nvidia 3D Vision is enabled it should
		// recognize the signature and automatically include the right eye frame.
		// D3D12TODO: Does this work on D3D12?

		D3D12_BOX box = CD3DX12_BOX(0, 0, 0, s_backbuffer_width, s_backbuffer_height, 1);
		D3D12_TEXTURE_COPY_LOCATION dst = CD3DX12_TEXTURE_COPY_LOCATION(D3D::GetBackBuffer()->GetTex12(), 0);
		D3D12_TEXTURE_COPY_LOCATION src = CD3DX12_TEXTURE_COPY_LOCATION(s_3d_vision_texture->GetTex12(), 0);

		D3D::GetBackBuffer()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_DEST);
		s_3d_vision_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
		D3D::current_command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);

		// Restore render target to backbuffer
		D3D::GetBackBuffer()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		D3D::current_command_list->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV12(), FALSE, nullptr);
	}
	else
	{
		D3D12_VIEWPORT vp12 = { (float)dst.left, (float)dst.top, (float)dst.GetWidth(), (float)dst.GetHeight(), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
		D3D::current_command_list->RSSetViewports(1, &vp12);

		D3D::DrawShadedTexQuad(
			src_texture,
			src.AsRECT(),
			src_width,
			src_height,
			(g_Config.iStereoMode == STEREO_ANAGLYPH) ? PixelShaderCache::GetAnaglyphProgram12() : PixelShaderCache::GetColorCopyProgram12(false),
			VertexShaderCache::GetSimpleVertexShader12(),
			VertexShaderCache::GetSimpleInputLayout12(),
			D3D12_SHADER_BYTECODE(),
			gamma,
			0,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			false,
			false // Backbuffer never multisampled.
			);
	}
}

}  // namespace DX12
