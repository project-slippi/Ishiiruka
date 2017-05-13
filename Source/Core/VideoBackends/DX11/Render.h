// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "VideoCommon/RenderBase.h"
#include "VideoBackends/DX11/D3DTexture.h"

struct XFBSourceBase;

namespace DX11
{

class Renderer : public ::Renderer
{
public:
	Renderer(void *&window_handle);
	~Renderer();
	void Init() override;
	void SetColorMask() override;
	void SetBlendMode(bool forceUpdate) override;
	void SetScissorRect(const EFBRectangle& rc) override;
	void SetGenerationMode() override;
	void SetDepthMode() override;
	void SetLogicOpMode() override;
	void SetSamplerState(int stage, int texindex, bool custom_tex) override;
	void SetInterlacingMode() override;
	void SetViewport() override;
	void SetFullscreen(bool enable_fullscreen) override;
	bool IsFullscreen() const override;
	// TODO: Fix confusing names (see ResetAPIState and RestoreAPIState)
	void ApplyState(bool bUseDstAlpha) override;
	void RestoreState() override;

	void ApplyCullDisable();
	void RestoreCull();

	void RenderText(const std::string& str, int left, int top, u32 color) override;

	u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) override;
	void PokeEFB(EFBAccessType type, const EfbPokeData* data, size_t num_points) override;
	u16 BBoxRead(int index) override;
	void BBoxWrite(int index, u16 value) override;

	void ResetAPIState() override;
	void RestoreAPIState() override;

	TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) override;

	void SwapImpl(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, const EFBRectangle& rc, u64 ticks, float Gamma = 1.0f) override;

	void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z) override;

	void ReinterpretPixelData(unsigned int convtype) override;
	bool CheckForResize();

private:
	// Draw either the EFB, or specified XFB sources to the currently-bound framebuffer.
	void DrawFrame(const TargetRectangle& target_rc, const EFBRectangle& source_rc, u32 xfb_addr,
		const XFBSourceBase* const* xfb_sources, u32 xfb_count, D3DTexture2D* dst_texture, const TargetSize& dst_size, u32 fb_width,
		u32 fb_stride, u32 fb_height, float Gamma);
	void DrawEFB(const TargetRectangle& target_rc, const EFBRectangle& source_rc, D3DTexture2D* dst_texture, const TargetSize& dst_size, float Gamma);
	void DrawVirtualXFB(const TargetRectangle& target_rc, u32 xfb_addr,
		const XFBSourceBase* const* xfb_sources, u32 xfb_count, D3DTexture2D* dst_texture, const TargetSize& dst_size, u32 fb_width,
		u32 fb_stride, u32 fb_height, float Gamma);
	void DrawRealXFB(const TargetRectangle& target_rc, u32 xfb_addr, D3DTexture2D* dst_texture, const TargetSize& dst_size, u32 fb_width, u32 fb_stride, u32 fb_height);
	void BlitScreen(TargetRectangle dst_rect, TargetRectangle src_rect, TargetSize src_size, D3DTexture2D* src_texture, D3DTexture2D* depth_texture,
		const TargetSize& dst_size, D3DTexture2D* dst_texture, float Gamma);

	void DumpFrame(const EFBRectangle& source_rc, u32 xfb_addr,
		const XFBSourceBase* const* xfb_sources, u32 xfb_count, u32 fb_width,
		u32 fb_stride, u32 fb_height, u64 ticks);

	void PrepareFrameDumpRenderTexture(u32 width, u32 height);
	void PrepareFrameDumpBuffer(u32 width, u32 height);
	void Create3DVisionTexture(u32 width, u32 height);
	void SetupDeviceObjects();
	
	D3DTexture2D* m_frame_dump_render_texture = nullptr;	
	D3D::Texture2dPtr m_frame_dump_staging_texture;
	D3DTexture2D* m_3d_vision_texture = nullptr;
	u32 m_frame_dump_render_texture_width = 0;
	u32 m_frame_dump_render_texture_height = 0;
	u32 m_frame_dump_staging_texture_width = 0;
	u32 m_frame_dump_staging_texture_height = 0;
	u32 m_3d_vision_texture_width = 0;
	u32 m_3d_vision_texture_height = 0;
	u32 m_last_multisamples = 0;
	int m_last_stereo_mode = 0;
	bool m_last_xfb_mode = false;
};

}