// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <cmath>
#include <memory>
#include <string>
#include <strsafe.h>
#include <tuple>
#include <unordered_map>


#include "Common/Align.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/MathUtil.h"

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
#include "VideoBackends/D3D12/NativeVertexFormat.h"
#include "VideoBackends/D3D12/PostProcessing.h"
#include "VideoBackends/D3D12/Render.h"
#include "VideoBackends/D3D12/ShaderCache.h"
#include "VideoBackends/D3D12/ShaderConstantsManager.h"
#include "VideoBackends/D3D12/StaticShaderCache.h"
#include "VideoBackends/D3D12/TextureCache.h"

#include "VideoCommon/AVIDump.h"
#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/SamplerCommon.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{

enum CLEAR_BLEND_DESC
{
	CLEAR_BLEND_DESC_ALL_CHANNELS_ENABLED = 0,
	CLEAR_BLEND_DESC_RGB_CHANNELS_ENABLED = 1,
	CLEAR_BLEND_DESC_ALPHA_CHANNEL_ENABLED = 2,
	CLEAR_BLEND_DESC_ALL_CHANNELS_DISABLED = 3
};

enum CLEAR_DEPTH_DESC
{
	CLEAR_DEPTH_DESC_DEPTH_DISABLED = 0,
	CLEAR_DEPTH_DESC_DEPTH_ENABLED_WRITES_ENABLED = 1,
	CLEAR_DEPTH_DESC_DEPTH_ENABLED_WRITES_DISABLED = 2,
};

static D3D12_BLEND_DESC s_clear_blend_descs[4] = {};
static D3D12_DEPTH_STENCIL_DESC s_clear_depth_descs[3] = {};

// These are accessed in D3DUtil.
D3D12_BLEND_DESC g_reset_blend_desc = {};
D3D12_DEPTH_STENCIL_DESC g_reset_depth_desc = {};
D3D12_RASTERIZER_DESC g_reset_rast_desc = {};



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
static struct
{
	SamplerState sampler[16];
	BlendState blend;
	DepthState zmode;
	RasterizerState raster;

} gx_state;

StateCache gx_state_cache;


void Renderer::SetupDeviceObjects()
{
	g_framebuffer_manager = std::make_unique<FramebufferManager>(m_target_width, m_target_height);

	D3D12_DEPTH_STENCIL_DESC depth_desc;
	depth_desc.DepthEnable = FALSE;
	depth_desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depth_desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	depth_desc.StencilEnable = FALSE;
	depth_desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	depth_desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	s_clear_depth_descs[CLEAR_DEPTH_DESC_DEPTH_DISABLED] = depth_desc;

	depth_desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depth_desc.DepthEnable = TRUE;
	s_clear_depth_descs[CLEAR_DEPTH_DESC_DEPTH_ENABLED_WRITES_ENABLED] = depth_desc;

	depth_desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	s_clear_depth_descs[CLEAR_DEPTH_DESC_DEPTH_ENABLED_WRITES_DISABLED] = depth_desc;

	D3D12_BLEND_DESC blend_desc;
	blend_desc.AlphaToCoverageEnable = FALSE;
	blend_desc.IndependentBlendEnable = FALSE;
	blend_desc.RenderTarget[0].LogicOpEnable = FALSE;
	blend_desc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	blend_desc.RenderTarget[0].BlendEnable = FALSE;
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blend_desc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	blend_desc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	blend_desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blend_desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blend_desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	g_reset_blend_desc = blend_desc;
	s_clear_blend_descs[CLEAR_BLEND_DESC_ALL_CHANNELS_ENABLED] = g_reset_blend_desc;

	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
	s_clear_blend_descs[CLEAR_BLEND_DESC_RGB_CHANNELS_ENABLED] = blend_desc;

	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALPHA;
	s_clear_blend_descs[CLEAR_BLEND_DESC_ALPHA_CHANNEL_ENABLED] = blend_desc;

	blend_desc.RenderTarget[0].RenderTargetWriteMask = 0;
	s_clear_blend_descs[CLEAR_BLEND_DESC_ALL_CHANNELS_DISABLED] = blend_desc;

	depth_desc.DepthEnable = FALSE;
	depth_desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depth_desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	depth_desc.StencilEnable = FALSE;
	depth_desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	depth_desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

	g_reset_depth_desc = depth_desc;

	D3D12_RASTERIZER_DESC rast_desc = CD3DX12_RASTERIZER_DESC(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_NONE, false, 0, 0.f, 0.f, false, false, false, 0, D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF);
	g_reset_rast_desc = rast_desc;
}

// Kill off all device objects
void Renderer::TeardownDeviceObjects()
{
	g_framebuffer_manager.reset();
	gx_state_cache.Clear();
}

static D3D12_BOX GetScreenshotSourceBox(const TargetRectangle& target_rc, u32 width, u32 height)
{
	// Since the screenshot buffer is copied back to the CPU, we can't access pixels that
	// fall outside the backbuffer bounds. Therefore, when crop is enabled and the target rect is
	// off-screen to the top/left, we clamp the origin at zero, as well as the bottom/right
	// coordinates at the backbuffer dimensions. This will result in a rectangle that can be
	// smaller than the backbuffer, but never larger.

	return CD3DX12_BOX(
		std::max(target_rc.left, 0),
		std::max(target_rc.top, 0),
		0,
		std::min(width, static_cast<unsigned int>(target_rc.right)),
		std::min(height, static_cast<unsigned int>(target_rc.bottom)),
		1);
}

static void Create3DVisionTexture(int width, int height)
{
	// D3D12TODO: 3D Vision not implemented on D3D12 backend.
}

Renderer::Renderer(void*& window_handle)
{
	if (g_ActiveConfig.iStereoMode == STEREO_3DVISION)
	{
		PanicAlert("3DVision not implemented on D3D12 backend.");
		return;
	}
	
	m_backbuffer_width = D3D::GetBackBufferWidth();
	m_backbuffer_height = D3D::GetBackBufferHeight();

	m_last_multisamples = g_ActiveConfig.iMultisamples;
	m_last_efb_scale = g_ActiveConfig.iEFBScale;
	m_last_stereo_mode = g_ActiveConfig.iStereoMode;
	m_last_xfb_mode = g_ActiveConfig.bUseRealXFB;
	

	// Setup GX pipeline state
	gx_state.blend.blend_enable = false;
	gx_state.blend.write_mask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gx_state.blend.src_blend = D3D12_BLEND_ONE;
	gx_state.blend.dst_blend = D3D12_BLEND_ZERO;
	gx_state.blend.blend_op = D3D12_BLEND_OP_ADD;
	gx_state.blend.use_dst_alpha = false;

	for (unsigned int k = 0; k < 16; k++)
	{
		gx_state.sampler[k].hex = 0;
	}

	gx_state.zmode.testenable = false;
	gx_state.zmode.updateenable = false;
	gx_state.zmode.func = ZMode::NEVER;
	gx_state.zmode.reversed_depth = false;

	gx_state.raster.cull_mode = D3D12_CULL_MODE_NONE;

	// Already transitioned to appropriate states a few lines up for the clears.
	m_target_dirty = true;

	D3D::BeginFrame();
}

void Renderer::Init()
{
	FramebufferManagerBase::SetLastXfbWidth(MAX_XFB_WIDTH);
	FramebufferManagerBase::SetLastXfbHeight(MAX_XFB_HEIGHT);

	UpdateDrawRectangle();

	CalculateTargetSize();
	PixelShaderManager::SetEfbScaleChanged();

	SetupDeviceObjects();

	m_post_processor = std::make_unique<D3DPostProcessor>();
	if (!m_post_processor->Initialize())
		PanicAlert("D3D: Failed to initialize post processor.");

	// Clear EFB textures
	float clear_color[4] = { 0.f, 0.f, 0.f, 1.f };
	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	D3D::current_command_list->ClearRenderTargetView(FramebufferManager::GetEFBColorTexture()->GetRTV(), clear_color, 0, nullptr);
	D3D::current_command_list->ClearDepthStencilView(FramebufferManager::GetEFBDepthTexture()->GetDSV(), D3D12_CLEAR_FLAG_DEPTH, 0.f, 0, 0, nullptr);

	m_vp = { 0.f, 0.f, static_cast<float>(m_target_width), static_cast<float>(m_target_height), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D::current_command_list->RSSetViewports(1, &m_vp);
}

Renderer::~Renderer()
{
	D3D::EndFrame();
	D3D::WaitForOutstandingRenderingToComplete();
	if (m_frame_dump_buffer)
	{
		D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_frame_dump_buffer);
		m_frame_dump_buffer = nullptr;
	}
	if (m_frame_dump_render_texture)
	{
		m_frame_dump_render_texture->Release();
		m_frame_dump_render_texture = nullptr;
	}
	m_frame_dump_render_texture_width = 0;
	m_frame_dump_render_texture_height = 0;
	m_frame_dump_buffer_size = 0;
	TeardownDeviceObjects();
	m_post_processor.reset();
}

void Renderer::RenderText(const std::string& text, int left, int top, u32 color)
{
	D3D::font.DrawTextScaled(static_cast<float>(left + 1), static_cast<float>(top + 1), 20.f, 0.0f, color & 0xFF000000, text);
	D3D::font.DrawTextScaled(static_cast<float>(left), static_cast<float>(top), 20.f, 0.0f, color, text);
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
__declspec(noinline) bool Renderer::CheckForResize()
{
	RECT rc_window;
	GetClientRect(D3D::hWnd, &rc_window);
	int client_width = rc_window.right - rc_window.left;
	int client_height = rc_window.bottom - rc_window.top;

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
	m_scissor_rect = rc;
	m_scissor_dirty = true;
}

void Renderer::SetColorMask()
{
	// Only enable alpha channel if it's supported by the current EFB format
	UINT8 color_mask = 0;
	if (bpmem.alpha_test.TestResult() != AlphaTest::FAIL)
	{
		if (bpmem.blendmode.alphaupdate && (bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24))
			color_mask = D3D12_COLOR_WRITE_ENABLE_ALPHA;
		if (bpmem.blendmode.colorupdate)
			color_mask |= D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
	}
	gx_state.blend.write_mask = color_mask;

	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PSO, true);

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
	if (type == EFBAccessType::PeekZ)
	{
		// depth buffer is inverted in the d3d backend
		float val = 1.0f - FramebufferManager::GetEFBCachedDepth(x, y);
		u32 ret = 0;

		if (bpmem.zcontrol.pixel_format == PEControl::RGB565_Z16)
		{
			// if Z is in 16 bit format you must return a 16 bit integer
			ret = MathUtil::Clamp<u32>(static_cast<u32>(val * 65536.0f), 0, 0xFFFF);
		}
		else
		{
			ret = MathUtil::Clamp<u32>(static_cast<u32>(val * 16777216.0f), 0, 0xFFFFFF);
		}
		return ret;
	}
	else if (type == EFBAccessType::PeekColor)
	{
		u32 ret = FramebufferManager::GetEFBCachedColor(x, y);

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

		if (alpha_read_mode.ReadMode == 2)
		{
			return ret; // GX_READ_NONE
		}
		else if (alpha_read_mode.ReadMode == 1)
		{
			return (ret | 0xFF000000); // GX_READ_FF
		}
		else /*if(alpha_read_mode.ReadMode == 0)*/
		{
			return (ret & 0x00FFFFFF); // GX_READ_00
		}
	}
	return poke_data;
}

void Renderer::PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points)
{
	if (m_target_dirty)
	{
		FramebufferManager::RestoreEFBRenderTargets();
	}
	D3D::SetViewportAndScissor(0, 0, GetTargetWidth(), GetTargetHeight());
	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	if (type == EFBAccessType::PokeColor)
	{
		// In the D3D12 backend, the rt/db/viewport is passed into DrawEFBPokeQuads, and set there.
		D3D::DrawEFBPokeQuads(
			type,
			points,
			num_points,
			&g_reset_blend_desc,
			&g_reset_depth_desc,
			&FramebufferManager::GetEFBColorTexture()->GetRTV(),
			nullptr,
			FramebufferManager::GetEFBColorTexture()->GetMultisampled()
		);
	}
	else // if (type == POKE_Z)
	{
		FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		D3D::DrawEFBPokeQuads(
			type,
			points,
			num_points,
			&s_clear_blend_descs[CLEAR_BLEND_DESC_ALL_CHANNELS_DISABLED],
			&s_clear_depth_descs[CLEAR_DEPTH_DESC_DEPTH_ENABLED_WRITES_ENABLED],
			&FramebufferManager::GetEFBColorTexture()->GetRTV(),
			&FramebufferManager::GetEFBDepthTexture()->GetDSV(),
			FramebufferManager::GetEFBColorTexture()->GetMultisampled()
		);
	}
	RestoreAPIState();
	m_target_dirty = false;
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

	int scissor_x_offset = bpmem.scissorOffset.x * 2;
	int scissor_y_offset = bpmem.scissorOffset.y * 2;

	float x = Renderer::EFBToScaledXf(xfmem.viewport.xOrig - xfmem.viewport.wd - scissor_x_offset);
	float y = Renderer::EFBToScaledYf(xfmem.viewport.yOrig + xfmem.viewport.ht - scissor_y_offset);
	float width = Renderer::EFBToScaledXf(2.0f * xfmem.viewport.wd);
	float height = Renderer::EFBToScaledYf(-2.0f * xfmem.viewport.ht);

	float range = MathUtil::Clamp<float>(xfmem.viewport.zRange, 0.0f, 16777215.0f);
	float min_depth =
		MathUtil::Clamp<float>(xfmem.viewport.farZ - range, 0.0f, 16777215.0f) / 16777216.0f;
	float max_depth = MathUtil::Clamp<float>(xfmem.viewport.farZ, 0.0f, 16777215.0f) / 16777216.0f;

	if (width < 0.0f)
	{
		x += width;
		width = -width;
	}
	if (height < 0.0f)
	{
		y += height;
		height = -height;
	}

	// If an inverted depth range is used, which D3D doesn't support,
	// we need to calculate the depth range in the vertex shader.
	if (xfmem.viewport.zRange < 0.0f)
	{
		min_depth = 0.0f;
		max_depth = GX_MAX_DEPTH;
	}

	// In D3D, the viewport rectangle must fit within the render target.
	x = (x >= 0.f) ? x : 0.f;
	y = (y >= 0.f) ? y : 0.f;
	width = (x + width <= GetTargetWidth()) ? width : (GetTargetWidth() - x);
	height = (y + height <= GetTargetHeight()) ? height : (GetTargetHeight() - y);

	m_vp = { x, y, width, height, 1.0f - max_depth, 1.0f - min_depth };
	gx_state.zmode.reversed_depth = xfmem.viewport.zRange < 0;
	m_viewport_dirty = true;
}

void Renderer::ClearScreen(const EFBRectangle& rc, bool color_enable, bool alpha_enable, bool z_enable, u32 color, u32 z)
{
	D3D12_BLEND_DESC *blend_desc = nullptr;

	if (color_enable && alpha_enable)
		blend_desc = &s_clear_blend_descs[CLEAR_BLEND_DESC_ALL_CHANNELS_ENABLED];
	else if (color_enable)
		blend_desc = &s_clear_blend_descs[CLEAR_BLEND_DESC_RGB_CHANNELS_ENABLED];
	else if (alpha_enable)
		blend_desc = &s_clear_blend_descs[CLEAR_BLEND_DESC_ALPHA_CHANNEL_ENABLED];
	else
		blend_desc = &s_clear_blend_descs[CLEAR_BLEND_DESC_ALL_CHANNELS_DISABLED];

	D3D12_DEPTH_STENCIL_DESC *depth_stencil_desc = nullptr;

	// EXISTINGD3D11TODO: Should we enable Z testing here?
	/*if (!bpmem.zmode.testenable) depth_stencil_desc = &s_clear_depth_descs[CLEAR_DEPTH_DESC_DEPTH_DISABLED];
	else */if (z_enable)
	{
		FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		depth_stencil_desc = &s_clear_depth_descs[CLEAR_DEPTH_DESC_DEPTH_ENABLED_WRITES_ENABLED];
	}
	else /*if (!z_enable)*/
	{
		FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_READ);
		depth_stencil_desc = &s_clear_depth_descs[CLEAR_DEPTH_DESC_DEPTH_ENABLED_WRITES_DISABLED];
	}
	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);

	if (m_target_dirty)
	{
		FramebufferManager::RestoreEFBRenderTargets();
	}

	// Update the view port for clearing the picture
	TargetRectangle target_rc = Renderer::ConvertEFBRectangle(rc);

	D3D::SetViewportAndScissor(target_rc.left, target_rc.top, target_rc.GetWidth(), target_rc.GetHeight());

	// Color is passed in bgra mode so we need to convert it to rgba
	u32 rgba_color = (color & 0xFF00FF00) | ((color >> 16) & 0xFF) | ((color << 16) & 0xFF0000);
	if (xfmem.viewport.zRange < 0)
	{
		D3D::DrawClearQuad(rgba_color, (z & 0xFFFFFFu) / 16777216.0f, blend_desc, depth_stencil_desc, FramebufferManager::GetEFBColorTexture()->GetMultisampled());
	}
	else
	{
		D3D::DrawClearQuad(rgba_color, 1.0f - (z & 0xFFFFFFu) / 16777216.0f, blend_desc, depth_stencil_desc, FramebufferManager::GetEFBColorTexture()->GetMultisampled());
	}
	

	// Restores proper viewport/scissor settings.
	RestoreAPIState();
	FramebufferManager::InvalidateEFBCache();
}

void Renderer::ReinterpretPixelData(unsigned int convtype)
{
	// EXISTINGD3D11TODO: MSAA support..
	D3D12_RECT source = CD3DX12_RECT(0, 0, GetTargetWidth(), GetTargetHeight());

	D3D12_SHADER_BYTECODE pixel_shader = {};

	if (convtype == 0)
	{
		pixel_shader = StaticShaderCache::GetReinterpRGB8ToRGBA6PixelShader(true);
	}
	else if (convtype == 2)
	{
		pixel_shader = StaticShaderCache::GetReinterpRGBA6ToRGB8PixelShader(true);
	}
	else
	{
		ERROR_LOG(VIDEO, "Trying to reinterpret pixel data with unsupported conversion type %d", convtype);
		return;
	}
	D3D::SetViewportAndScissor(0, 0, GetTargetWidth(), GetTargetHeight());

	FramebufferManager::GetEFBColorTempTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTempTexture()->GetRTV(), FALSE, nullptr);

	D3D::SetPointCopySampler();
	D3D::DrawShadedTexQuad(
		FramebufferManager::GetEFBColorTexture(),
		&source,
		GetTargetWidth(),
		GetTargetHeight(),
		pixel_shader,
		StaticShaderCache::GetSimpleVertexShader(),
		StaticShaderCache::GetSimpleVertexShaderInputLayout(),
		StaticShaderCache::GetCopyGeometryShader(),
		0,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		false,
		FramebufferManager::GetEFBColorTempTexture()->GetMultisampled()
	);

	FramebufferManager::SwapReinterpretTexture();
	FramebufferManager::InvalidateEFBCache();
	// Restores proper viewport/scissor settings.
	RestoreAPIState();
}

void Renderer::SetBlendMode(bool force_update)
{
	// Our render target always uses an alpha channel, so we need to override the blend functions to assume a destination alpha of 1 if the render target isn't supposed to have an alpha channel
	// Example: D3DBLEND_DESTALPHA needs to be D3DBLEND_ONE since the result without an alpha channel is assumed to always be 1.
	bool target_has_alpha = bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24;
	const D3D12_BLEND d3d_src_factors[8] =
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
	const D3D12_BLEND d3d_dst_factors[8] =
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

	if (bpmem.blendmode.logicopenable && !bpmem.blendmode.blendenable && !force_update)
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
		gx_state.blend.blend_enable = static_cast<u32>(bpmem.blendmode.blendenable);
		if (bpmem.blendmode.blendenable)
		{
			gx_state.blend.blend_op = D3D12_BLEND_OP_ADD;
			gx_state.blend.src_blend = d3d_src_factors[bpmem.blendmode.srcfactor];
			gx_state.blend.dst_blend = d3d_dst_factors[bpmem.blendmode.dstfactor];
		}
	}

	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PSO, true);
}

// This function has the final picture. We adjust the aspect ratio here.
void Renderer::SwapImpl(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height, const EFBRectangle& rc, u64 ticks, float gamma)
{
	if ( (!m_xfb_written && !g_ActiveConfig.RealXFBEnabled()) || !fb_width || !fb_height)
	{
		Core::Callback_VideoCopiedToXFB(false);
		return;
	}

	u32 xfb_count = 0;
	const XFBSourceBase* const* xfb_source_list = FramebufferManager::GetXFBSource(xfb_addr, fb_stride, fb_height, &xfb_count);
	if ((!xfb_source_list || xfb_count == 0) && g_ActiveConfig.bUseXFB && !g_ActiveConfig.bUseRealXFB)
	{
		Core::Callback_VideoCopiedToXFB(false);
		return;
	}

	// Invalidate EFB access copies. Not strictly necessary, but this avoids having the buffers mapped when calling Present().
	FramebufferManager::InvalidateEFBCache();
	if (!g_ActiveConfig.bUseXFB)
		m_post_processor->OnEndFrame();

	// Prepare to copy the XFBs to our backbuffer
	UpdateDrawRectangle();
	TargetRectangle target_rc = GetTargetRectangle();

	D3D::GetBackBuffer()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), FALSE, nullptr);

	float clear_color[4] = { 0.f, 0.f, 0.f, 1.f };
	D3D::current_command_list->ClearRenderTargetView(D3D::GetBackBuffer()->GetRTV(), clear_color, 0, nullptr);
	// Copy the framebuffer to screen.	
	const TargetSize dst_size = { m_backbuffer_width, m_backbuffer_height };
	DrawFrame(target_rc, rc, xfb_addr, xfb_source_list, xfb_count, D3D::GetBackBuffer(), dst_size, fb_width, fb_stride, fb_height, gamma);
	
	// Dump frames
	if (IsFrameDumping())
	{
		DumpFrame(rc, xfb_addr, xfb_source_list, xfb_count, fb_width, fb_stride, fb_height, ticks);
	}

	// Reset viewport for drawing text
	D3D::SetViewportAndScissor(0, 0, GetBackbufferWidth(), GetBackbufferHeight());
	D3D::SetLinearCopySampler();
	Renderer::DrawDebugText();

	OSD::DrawMessages();
	D3D::EndFrame();

	g_texture_cache->Cleanup(frameCount);

	// Enable configuration changes
	UpdateActiveConfig();
	g_texture_cache->OnConfigChanged(g_ActiveConfig);

	SetWindowSize(fb_stride, fb_height);

	const bool window_resized = CheckForResize();	

	bool xfb_changed = m_last_xfb_mode != g_ActiveConfig.bUseRealXFB;

	if (FramebufferManagerBase::LastXfbWidth() != fb_stride || FramebufferManagerBase::LastXfbHeight() != fb_height)
	{
		xfb_changed = true;
		unsigned int xfb_w = (fb_stride < 1 || fb_stride > MAX_XFB_WIDTH) ? MAX_XFB_WIDTH : fb_stride;
		unsigned int xfb_h = (fb_height < 1 || fb_height > MAX_XFB_HEIGHT) ? MAX_XFB_HEIGHT : fb_height;
		FramebufferManagerBase::SetLastXfbWidth(xfb_w);
		FramebufferManagerBase::SetLastXfbHeight(xfb_h);
	}

	// Flip/present backbuffer to frontbuffer here
	D3D::Present();

	// Resize the back buffers NOW to avoid flickering
	if (CalculateTargetSize() ||
		xfb_changed ||
		window_resized ||
		m_last_efb_scale != g_ActiveConfig.iEFBScale ||
		m_last_multisamples != g_ActiveConfig.iMultisamples ||
		m_last_stereo_mode != g_ActiveConfig.iStereoMode)
	{
		m_last_xfb_mode = g_ActiveConfig.bUseRealXFB;
		// Block on any changes until the GPU catches up, so we can free resources safely.
		D3D::command_list_mgr->ExecuteQueuedWork(true);

		if (m_last_multisamples != g_ActiveConfig.iMultisamples)
		{
			m_last_multisamples = g_ActiveConfig.iMultisamples;
			StaticShaderCache::InvalidateMSAAShaders();
		}

		if (window_resized)
		{
			// TODO: Aren't we still holding a reference to the back buffer right now?
			D3D::Reset();
			m_backbuffer_width = D3D::GetBackBufferWidth();
			m_backbuffer_height = D3D::GetBackBufferHeight();
		}

		UpdateDrawRectangle();

		m_last_efb_scale = g_ActiveConfig.iEFBScale;

		PixelShaderManager::SetEfbScaleChanged();

		D3D::GetBackBuffer()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		D3D::current_command_list->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), FALSE, nullptr);

		g_framebuffer_manager.reset();
		g_framebuffer_manager = std::make_unique<FramebufferManager>(m_target_width, m_target_height);
		const float clear_color[4] = { 0.f, 0.f, 0.f, 1.f };

		FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		D3D::current_command_list->ClearRenderTargetView(FramebufferManager::GetEFBColorTexture()->GetRTV(), clear_color, 0, nullptr);

		FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		D3D::current_command_list->ClearDepthStencilView(FramebufferManager::GetEFBDepthTexture()->GetDSV(), D3D12_CLEAR_FLAG_DEPTH, 0.f, 0, 0, nullptr);
		if (m_last_stereo_mode != g_ActiveConfig.iStereoMode)
		{
			m_last_stereo_mode = g_ActiveConfig.iStereoMode;
			m_post_processor->SetReloadFlag();
		}
	}

	// begin next frame
	RestoreAPIState();
	D3D::BeginFrame();

	// if the configuration has changed, reload post processor (can fail, which will deactivate it)
	if (m_post_processor->RequiresReload())
	{
		D3D::command_list_mgr->ExecuteQueuedWork(true);
		m_post_processor->ReloadShaders();
	}
}

void Renderer::DrawFrame(const TargetRectangle& target_rc, const EFBRectangle& source_rc, u32 xfb_addr,
	const XFBSourceBase* const* xfb_sources, u32 xfb_count, D3DTexture2D* dst_texture, const TargetSize& dst_size, u32 fb_width,
	u32 fb_stride, u32 fb_height, float Gamma)
{
	if (g_ActiveConfig.bUseXFB)
	{
		if (g_ActiveConfig.bUseRealXFB)
			DrawRealXFB(target_rc, xfb_sources, xfb_count, dst_texture, dst_size, fb_width, fb_stride, fb_height);
		else
			DrawVirtualXFB(target_rc, xfb_addr, xfb_sources, xfb_count, dst_texture, dst_size, fb_width, fb_stride, fb_height, Gamma);
	}
	else
	{
		DrawEFB(target_rc, source_rc, dst_texture, dst_size, Gamma);
	}
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

		// Restore render target to backbuffer
		D3D::current_command_list->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), FALSE, nullptr);
		D3D::SetLinearCopySampler();
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
		//float vScale = static_cast<float>(fbHeight) / static_cast<float>(m_backbuffer_height);
		//float hScale = static_cast<float>(fbWidth) / static_cast<float>(m_backbuffer_width);
		//drawRc.top *= vScale;
		//drawRc.bottom *= vScale;CalculateTargetSize
		//drawRc.left *= hScale;
		//drawRc.right *= hScale;

		source_rc.right -= Renderer::EFBToScaledX(fb_stride - fb_width);

		TargetSize blit_size(xfb_source->texWidth, xfb_source->texHeight);
		BlitScreen(drawRc, source_rc, blit_size, xfb_source->m_tex, xfb_source->m_depthtex, dst_size, dst_texture, Gamma);
	}
}

void Renderer::DrawRealXFB(const TargetRectangle& target_rc, const XFBSourceBase* const* xfb_sources,
	u32 xfb_count, D3DTexture2D* dst_texture, const TargetSize& dst_size, u32 fb_width, u32 fb_stride, u32 fb_height)
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

		drawRc = target_rc;
		source_rc.right -= fb_stride - fb_width;

		TargetSize blit_size(xfb_source->texWidth, xfb_source->texHeight);
		BlitScreen(drawRc, source_rc, blit_size, xfb_source->m_tex, xfb_source->m_depthtex, dst_size, dst_texture, 1.0);
	}
}


void Renderer::ResetAPIState()
{
	CHECK(0, "This should never be called.. just required for inheritance.");
}

void Renderer::RestoreAPIState()
{
	// Restores viewport/scissor rects, which might have been
	// overwritten elsewhere (particularly the viewport).
	m_viewport_dirty = true;
	m_scissor_dirty = true;
	m_target_dirty = true;
}

void Renderer::ApplyState(bool use_dst_alpha)
{
	if (use_dst_alpha != m_previous_use_dst_alpha)
	{
		m_previous_use_dst_alpha = use_dst_alpha;
		D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PSO, true);
	}

	gx_state.blend.use_dst_alpha = use_dst_alpha;

	D3D12_SHADER_BYTECODE DS = ShaderCache::GetActiveDomainShaderBytecode();
	D3D12_SHADER_BYTECODE GS = ShaderCache::GetActiveGeometryShaderBytecode();
	D3D12_SHADER_BYTECODE HS = ShaderCache::GetActiveHullShaderBytecode();
	D3D12_SHADER_BYTECODE PS = ShaderCache::GetActivePixelShaderBytecode();
	D3D12_SHADER_BYTECODE VS = ShaderCache::GetActiveVertexShaderBytecode();

	if (D3D::command_list_mgr->GetCommandListDirtyState(COMMAND_LIST_STATE_PSO))
	{
		D3D::SetRootSignature(GS.pShaderBytecode != nullptr, HS.pShaderBytecode != nullptr);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE texture_group = TextureCache::GetTextureGroupHandle();
	if (texture_group.ptr)
	{
		DX12::D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV, texture_group);
		if (g_ActiveConfig.TessellationEnabled() && D3D::TessellationEnabled())
		{
			DX12::D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_DS_SRV, texture_group);
		}
	}

	if (D3D::command_list_mgr->GetCommandListDirtyState(COMMAND_LIST_STATE_SAMPLERS))
	{
		D3D12_GPU_DESCRIPTOR_HANDLE sample_group_gpu_handle;
		sample_group_gpu_handle = D3D::sampler_descriptor_heap_mgr->GetHandleForSamplerGroup(gx_state.sampler, 8);

		D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SAMPLER, sample_group_gpu_handle);
		if (g_ActiveConfig.TessellationEnabled() && D3D::TessellationEnabled())
		{
			D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_DS_SAMPLER, sample_group_gpu_handle);
		}
		D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_SAMPLERS, false);
	}



	// Uploads and binds required constant buffer data for all stages.
	bool current_command_list_executed = ShaderConstantsManager::LoadAndSetPixelShaderConstants();
	current_command_list_executed = current_command_list_executed || ShaderConstantsManager::LoadAndSetVertexShaderConstants();
	if (GS.pShaderBytecode != nullptr)
	{
		current_command_list_executed = current_command_list_executed || ShaderConstantsManager::LoadAndSetGeometryShaderConstants();
	}
	if (HS.pShaderBytecode != nullptr)
	{
		current_command_list_executed = current_command_list_executed || ShaderConstantsManager::LoadAndSetHullDomainShaderConstants();
	}

	if (current_command_list_executed)
	{
		RestoreAPIState();
	}
	if (m_viewport_dirty)
	{
		D3D::current_command_list->RSSetViewports(1, &m_vp);
		m_viewport_dirty = false;
	}
	if (m_scissor_dirty)
	{
		D3D12_RECT src_s_rect = *ConvertEFBRectangle(m_scissor_rect).AsRECT();
		D3D::current_command_list->RSSetScissorRects(1, &src_s_rect);
		m_scissor_dirty = false;
	}
	if (m_target_dirty)
	{
		FramebufferManager::RestoreEFBRenderTargets();
		m_target_dirty = false;
	}
	if (D3D::command_list_mgr->GetCommandListDirtyState(COMMAND_LIST_STATE_PSO) || m_previous_vertex_format != reinterpret_cast<D3DVertexFormat*>(VertexLoaderManager::GetCurrentVertexFormat()))
	{
		m_previous_vertex_format = reinterpret_cast<D3DVertexFormat*>(VertexLoaderManager::GetCurrentVertexFormat());

		D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = ShaderCache::GetCurrentPrimitiveTopology();
		RasterizerState modifiableRastState = gx_state.raster;

		if (topologyType != D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
		{
			modifiableRastState.cull_mode = D3D12_CULL_MODE_NONE;
		}
		else
		{
			if (ShaderCache::GetActiveDomainShaderBytecode().pShaderBytecode != nullptr)
			{
				topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
			}
		}

		SmallPsoDesc pso_desc = {
			DS,
			GS,
			HS,
			PS,
			VS,
			m_previous_vertex_format,					// D3D12_INPUT_LAYOUT_DESC InputLayout;
			gx_state.blend,                             // BlendState BlendState;
			modifiableRastState,                        // RasterizerState RasterizerState;
			gx_state.zmode,                             // ZMode DepthStencilState;
			g_ActiveConfig.iMultisamples
		};

		if (use_dst_alpha)
		{
			// restore actual state
			SetLogicOpMode();
		}

		ID3D12PipelineState* pso = nullptr;
		CheckHR(
			gx_state_cache.GetPipelineStateObjectFromCache(
				pso_desc,
				&pso,
				topologyType,
				ShaderCache::GetActiveGeometryShaderUid(),
				ShaderCache::GetActivePixelShaderUid(),
				ShaderCache::GetActiveVertexShaderUid(),
				ShaderCache::GetActiveTessellationShaderUid()
			)
		);

		D3D::current_command_list->SetPipelineState(pso);

		D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PSO, false);
	}
	FramebufferManager::InvalidateEFBCache();
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void Renderer::RestoreState()
{}

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
	const D3D12_CULL_MODE d3d_cull_modes[4] =
	{
		D3D12_CULL_MODE_NONE,
		D3D12_CULL_MODE_BACK,
		D3D12_CULL_MODE_FRONT,
		D3D12_CULL_MODE_BACK
	};

	// rastdc.FrontCounterClockwise must be false for this to work
	// EXISTINGD3D11TODO: GX_CULL_ALL not supported, yet!
	gx_state.raster.cull_mode = d3d_cull_modes[bpmem.genMode.cullmode];

	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PSO, true);
}

void Renderer::SetDepthMode()
{
	gx_state.zmode.testenable = (u32)bpmem.zmode.testenable;
	gx_state.zmode.func = bpmem.zmode.func;
	gx_state.zmode.updateenable = (u32)bpmem.zmode.updateenable;

	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PSO, true);
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

	const D3D12_LOGIC_OP  d3d_111_logic_op[16] =
	{
		D3D12_LOGIC_OP_CLEAR,
		D3D12_LOGIC_OP_AND,
		D3D12_LOGIC_OP_AND_REVERSE,
		D3D12_LOGIC_OP_COPY,
		D3D12_LOGIC_OP_AND_INVERTED,
		D3D12_LOGIC_OP_NOOP,
		D3D12_LOGIC_OP_XOR,
		D3D12_LOGIC_OP_OR,
		D3D12_LOGIC_OP_NOR,
		D3D12_LOGIC_OP_EQUIV,
		D3D12_LOGIC_OP_INVERT,
		D3D12_LOGIC_OP_OR_REVERSE,
		D3D12_LOGIC_OP_COPY_INVERTED,
		D3D12_LOGIC_OP_OR_INVERTED,
		D3D12_LOGIC_OP_NAND,
		D3D12_LOGIC_OP_SET
	};
	// fallbacks for devices that does not support logic blending
	const D3D12_BLEND_OP d3d_logic_ops[16] =
	{
		D3D12_BLEND_OP_ADD,
		D3D12_BLEND_OP_ADD,
		D3D12_BLEND_OP_SUBTRACT,
		D3D12_BLEND_OP_ADD,
		D3D12_BLEND_OP_REV_SUBTRACT,
		D3D12_BLEND_OP_ADD,
		D3D12_BLEND_OP_MAX,
		D3D12_BLEND_OP_ADD,
		D3D12_BLEND_OP_MAX,
		D3D12_BLEND_OP_MAX,
		D3D12_BLEND_OP_ADD,
		D3D12_BLEND_OP_ADD,
		D3D12_BLEND_OP_ADD,
		D3D12_BLEND_OP_ADD,
		D3D12_BLEND_OP_ADD,
		D3D12_BLEND_OP_ADD
	};
	const D3D12_BLEND d3d_logic_op_src_factors[16] =
	{
		D3D12_BLEND_ZERO,
		D3D12_BLEND_DEST_COLOR,
		D3D12_BLEND_ONE,
		D3D12_BLEND_ONE,
		D3D12_BLEND_DEST_COLOR,
		D3D12_BLEND_ZERO,
		D3D12_BLEND_INV_DEST_COLOR,
		D3D12_BLEND_INV_DEST_COLOR,
		D3D12_BLEND_INV_SRC_COLOR,
		D3D12_BLEND_INV_SRC_COLOR,
		D3D12_BLEND_INV_DEST_COLOR,
		D3D12_BLEND_ONE,
		D3D12_BLEND_INV_SRC_COLOR,
		D3D12_BLEND_INV_SRC_COLOR,
		D3D12_BLEND_INV_DEST_COLOR,
		D3D12_BLEND_ONE
	};
	const D3D12_BLEND d3d_logic_op_dest_factors[16] =
	{
		D3D12_BLEND_ZERO,
		D3D12_BLEND_ZERO,
		D3D12_BLEND_INV_SRC_COLOR,
		D3D12_BLEND_ZERO,
		D3D12_BLEND_ONE,
		D3D12_BLEND_ONE,
		D3D12_BLEND_INV_SRC_COLOR,
		D3D12_BLEND_ONE,
		D3D12_BLEND_INV_DEST_COLOR,
		D3D12_BLEND_SRC_COLOR,
		D3D12_BLEND_INV_DEST_COLOR,
		D3D12_BLEND_INV_DEST_COLOR,
		D3D12_BLEND_INV_SRC_COLOR,
		D3D12_BLEND_ONE,
		D3D12_BLEND_INV_SRC_COLOR,
		D3D12_BLEND_ONE
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
			gx_state.blend.blend_op = d3d_logic_ops[bpmem.blendmode.logicmode];
			gx_state.blend.src_blend = d3d_logic_op_src_factors[bpmem.blendmode.logicmode];
			gx_state.blend.dst_blend = d3d_logic_op_dest_factors[bpmem.blendmode.logicmode];
		}
	}
	else
	{
		gx_state.blend.logic_op_enabled = false;
		SetBlendMode(true);
	}

	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PSO, true);
}

void Renderer::SetSamplerState(int stage, int tex_index, bool custom_tex)
{
	const FourTexUnits& tex = bpmem.tex[tex_index];
	const TexMode0& tm0 = tex.texMode0[stage];
	const TexMode1& tm1 = tex.texMode1[stage];
	SamplerState new_state = {};

	if (tex_index)
		stage += 4;

	if (g_ActiveConfig.bForceFiltering)
	{
		new_state.min_filter = 6; // 4 (linear min) | 2 (linear mip)
		new_state.mag_filter = 1; // linear mag
	}
	else if (g_ActiveConfig.bDisableTextureFiltering)
	{
		gx_state.sampler[stage].min_filter = 0;
		gx_state.sampler[stage].mag_filter = 0;
	}
	else
	{
		new_state.min_filter = tm0.min_filter;
		new_state.mag_filter = tm0.mag_filter;
	}

	new_state.wrap_s = tm0.wrap_s;
	new_state.wrap_t = tm0.wrap_t;
	new_state.max_lod = tm1.max_lod;
	new_state.min_lod = tm1.min_lod;
	new_state.lod_bias = tm0.lod_bias;

	// custom textures may have higher resolution, so disable the max_lod
	if (custom_tex)
	{
		new_state.max_lod = 255;
	}

	if (new_state.hex != gx_state.sampler[stage].hex)
	{
		gx_state.sampler[stage].hex = new_state.hex;
		D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_SAMPLERS, true);
	}
}

void Renderer::SetInterlacingMode()
{
	// EXISTINGD3D11TODO
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

void Renderer::BBoxWrite(int index, u16 value)
{
	int local_value = value; // u16 isn't enough to multiply by the efb width
	if (index & 1)
		local_value--;
	if (index < 2)
	{
		local_value = local_value * m_target_width / EFB_WIDTH;
	}
	else
	{
		local_value = local_value * m_target_height / EFB_HEIGHT;
	}

	BBox::Set(index, local_value);
}

void Renderer::BlitScreen(TargetRectangle dst_rect, TargetRectangle src_rect, TargetSize src_size, D3DTexture2D* src_texture, D3DTexture2D* depth_texture,
	const TargetSize& dst_size, D3DTexture2D* dst_texture, float Gamma)
{
	if (g_ActiveConfig.iStereoMode == STEREO_SBS || g_ActiveConfig.iStereoMode == STEREO_TAB)
	{
		TargetRectangle left_rc, right_rc;
		std::tie(left_rc, right_rc) = ConvertStereoRectangle(dst_rect);

		m_post_processor->BlitScreen(left_rc, dst_size, reinterpret_cast<uintptr_t>(dst_texture),
			src_rect, src_size, reinterpret_cast<uintptr_t>(src_texture), reinterpret_cast<uintptr_t>(depth_texture), 0, Gamma);

		m_post_processor->BlitScreen(right_rc, dst_size, reinterpret_cast<uintptr_t>(dst_texture),
			src_rect, src_size, reinterpret_cast<uintptr_t>(src_texture), reinterpret_cast<uintptr_t>(depth_texture), 1, Gamma);
	}
	else if (g_ActiveConfig.iStereoMode == STEREO_3DVISION)
	{
		// D3D12TODO
		// Not currently supported on D3D12 backend. Implemented (but untested) code kept for reference.

		//if (!s_3d_vision_texture)
		//	Create3DVisionTexture(s_backbuffer_width, s_backbuffer_height);

		//D3D12_VIEWPORT leftVp12 = { (float)dst.left, (float)dst.top, (float)dst.GetWidth(), (float)dst.GetHeight(), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
		//D3D12_VIEWPORT rightVp12 = { (float)(dst.left + s_backbuffer_width), (float)dst.top, (float)dst.GetWidth(), (float)dst.GetHeight(), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };

		//// Render to staging texture which is double the width of the backbuffer
		//s_3d_vision_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		//D3D::current_command_list->OMSetRenderTargets(1, &s_3d_vision_texture->GetRTV12(), FALSE, nullptr);

		//D3D::current_command_list->RSSetViewports(1, &leftVp12);
		//D3D::DrawShadedTexQuad(src_texture, src.AsRECT(), src_width, src_height, StaticShaderCache::GetColorCopyPixelShader(false), StaticShaderCache::GetSimpleVertexShader(), StaticShaderCache::GetSimpleVertexShaderInputLayout(), D3D12_SHADER_BYTECODE(), gamma, 0, DXGI_FORMAT_R8G8B8A8_UNORM, false, s_3d_vision_texture->GetMultisampled());

		//D3D::current_command_list->RSSetViewports(1, &rightVp12);
		//D3D::DrawShadedTexQuad(src_texture, src.AsRECT(), src_width, src_height, StaticShaderCache::GetColorCopyPixelShader(false), StaticShaderCache::GetSimpleVertexShader(), StaticShaderCache::GetSimpleVertexShaderInputLayout(), D3D12_SHADER_BYTECODE(), gamma, 1, DXGI_FORMAT_R8G8B8A8_UNORM, false, s_3d_vision_texture->GetMultisampled());

		//// Copy the left eye to the backbuffer, if Nvidia 3D Vision is enabled it should
		//// recognize the signature and automatically include the right eye frame.
		//// D3D12TODO: Does this work on D3D12?

		//D3D12_BOX box = CD3DX12_BOX(0, 0, 0, s_backbuffer_width, s_backbuffer_height, 1);
		//D3D12_TEXTURE_COPY_LOCATION dst = CD3DX12_TEXTURE_COPY_LOCATION(D3D::GetBackBuffer()->GetTex12(), 0);
		//D3D12_TEXTURE_COPY_LOCATION src = CD3DX12_TEXTURE_COPY_LOCATION(s_3d_vision_texture->GetTex12(), 0);

		//D3D::GetBackBuffer()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_DEST);
		//s_3d_vision_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
		//D3D::current_command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);

		//// Restore render target to backbuffer
		//D3D::GetBackBuffer()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		//D3D::current_command_list->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV12(), FALSE, nullptr);
	}
	else
	{
		m_post_processor->BlitScreen(dst_rect, dst_size, reinterpret_cast<uintptr_t>(dst_texture),
			src_rect, src_size, reinterpret_cast<uintptr_t>(src_texture), reinterpret_cast<uintptr_t>(depth_texture), 0, Gamma);
	}
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
	m_frame_dump_render_texture = D3DTexture2D::Create(width, height, TEXTURE_BIND_FLAG_SHADER_RESOURCE | TEXTURE_BIND_FLAG_RENDER_TARGET,
		DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1);
}

void Renderer::PrepareFrameDumpBuffer(u32 width, u32 height)
{
	const unsigned int screenshot_buffer_size =
		Common::AlignUpSizePow2(width * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) *
		height;
	if (screenshot_buffer_size < m_frame_dump_buffer_size)
	{
		return;
	}
	m_frame_dump_buffer_size = screenshot_buffer_size;
	CheckHR(
		D3D::device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(m_frame_dump_buffer_size),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_frame_dump_buffer)
		)
	);
}

void  Renderer::DumpFrame(const EFBRectangle& source_rc, u32 xfb_addr,
	const XFBSourceBase* const* xfb_sources, u32 xfb_count, u32 fb_width,
	u32 fb_stride, u32 fb_height, u64 ticks)
{
	D3D12_BOX source_box;
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

	D3D12_TEXTURE_COPY_LOCATION dst_location = {};
	dst_location.pResource = m_frame_dump_buffer;
	dst_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst_location.PlacedFootprint.Offset = 0;
	dst_location.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dst_location.PlacedFootprint.Footprint.Width = src_width;
	dst_location.PlacedFootprint.Footprint.Height = src_height;
	dst_location.PlacedFootprint.Footprint.Depth = 1;
	dst_location.PlacedFootprint.Footprint.RowPitch = Common::AlignUpSizePow2(dst_location.PlacedFootprint.Footprint.Width * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	D3D12_TEXTURE_COPY_LOCATION src_location = {};
	src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src_location.SubresourceIndex = 0;
	src_location.pResource = src->GetTex();

	src->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
	D3D::current_command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, &source_box);

	D3D::command_list_mgr->ExecuteQueuedWork(true);

	void* screenshot_texture_map;
	D3D12_RANGE read_range = { 0, dst_location.PlacedFootprint.Footprint.RowPitch * box_height };
	CheckHR(m_frame_dump_buffer->Map(0, &read_range, &screenshot_texture_map));

	AVIDump::Frame state = AVIDump::FetchState(ticks);
	DumpFrameData(reinterpret_cast<const u8*>(screenshot_texture_map), box_width, box_height,
		dst_location.PlacedFootprint.Footprint.RowPitch, state);
	FinishFrameData();

	D3D12_RANGE write_range = {};
	m_frame_dump_buffer->Unmap(0, &write_range);
}

D3D12_BLEND_DESC Renderer::GetResetBlendDesc()
{
	return g_reset_blend_desc;
}

D3D12_DEPTH_STENCIL_DESC Renderer::GetResetDepthStencilDesc()
{
	return g_reset_depth_desc;
}
D3D12_RASTERIZER_DESC Renderer::GetResetRasterizerDesc()
{
	return g_reset_rast_desc;
}

}  // namespace DX12
