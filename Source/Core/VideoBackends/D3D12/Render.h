// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "VideoCommon/RenderBase.h"
#include "D3DTexture.h"

struct XFBSourceBase;

namespace DX12
{

class Renderer final : public ::Renderer
{
public:
	Renderer(void *&window_handle);
	~Renderer();
	void Init() override;

	void SetColorMask() override;
	void SetBlendMode(bool force_Update) override;
	void SetScissorRect(const EFBRectangle& rc) override;
	void SetGenerationMode() override;
	void SetDepthMode() override;
	void SetLogicOpMode() override;
	void SetSamplerState(int stage, int tex_index, bool custom_tex) override;
	void SetInterlacingMode() override;
	void SetViewport() override;

	// TODO: Fix confusing names (see ResetAPIState and RestoreAPIState)
	void ApplyState(bool use_dst_alpha) override;
	void RestoreState() override;

	void ApplyCullDisable();
	void RestoreCull();

	void RenderText(const std::string& text, int left, int top, u32 color) override;

	u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) override;
	void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) override;
	u16 BBoxRead(int index) override;
	void BBoxWrite(int index, u16 value) override;

	void ResetAPIState() override;
	void RestoreAPIState() override;

	TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) override;

	void SwapImpl(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height, const EFBRectangle& rc, u64 ticks, float gamma) override;

	void ClearScreen(const EFBRectangle& rc, bool color_enable, bool alpha_enable, bool z_enable, u32 color, u32 z) override;

	void ReinterpretPixelData(unsigned int conv_type) override;

	static bool CheckForResize();

	static D3D12_BLEND_DESC GetResetBlendDesc();
	static D3D12_DEPTH_STENCIL_DESC GetResetDepthStencilDesc();
	static D3D12_RASTERIZER_DESC GetResetRasterizerDesc();
private:
	// Draw either the EFB, or specified XFB sources to the currently-bound framebuffer.
	void DrawFrame(const TargetRectangle& target_rc, const EFBRectangle& source_rc, u32 xfb_addr,
		const XFBSourceBase* const* xfb_sources, u32 xfb_count, D3DTexture2D* dst_texture, const TargetSize& dst_size, u32 fb_width,
		u32 fb_stride, u32 fb_height, float Gamma);
	void DrawEFB(const TargetRectangle& target_rc, const EFBRectangle& source_rc, D3DTexture2D* dst_texture, const TargetSize& dst_size, float Gamma);
	void DrawVirtualXFB(const TargetRectangle& target_rc, u32 xfb_addr,
		const XFBSourceBase* const* xfb_sources, u32 xfb_count, D3DTexture2D* dst_texture, const TargetSize& dst_size, u32 fb_width,
		u32 fb_stride, u32 fb_height, float Gamma);
	void DrawRealXFB(const TargetRectangle& target_rc, const XFBSourceBase* const* xfb_sources,
		u32 xfb_count, D3DTexture2D* dst_texture, const TargetSize& dst_size, u32 fb_width, u32 fb_stride, u32 fb_height);
	void BlitScreen(TargetRectangle dst_rect, TargetRectangle src_rect, TargetSize src_size, D3DTexture2D* src_texture, D3DTexture2D* depth_texture, 
		const TargetSize& dst_size, D3DTexture2D* dst_texture, float Gamma);

	void DumpFrame(const EFBRectangle& source_rc, u32 xfb_addr,
		const XFBSourceBase* const* xfb_sources, u32 xfb_count, u32 fb_width,
		u32 fb_stride, u32 fb_height, u64 ticks);

	void PrepareFrameDumpRenderTexture(u32 width, u32 height);
	void PrepareFrameDumpBuffer(u32 width, u32 height);
	void SetupDeviceObjects();
	void TeardownDeviceObjects();

	D3DTexture2D* m_frame_dump_render_texture = nullptr;
	ID3D12Resource* m_frame_dump_buffer = nullptr;
	u32 m_frame_dump_buffer_size = 0;
	u32 m_frame_dump_render_texture_width = 0;
	u32 m_frame_dump_render_texture_height = 0;
	u32 m_last_multisamples = 1;
	int m_last_stereo_mode = false;
	bool m_last_xfb_mode = false;
	bool m_scissor_dirty = true;
	EFBRectangle m_scissor_rect{};
	bool m_viewport_dirty = true;
	D3D12_VIEWPORT m_vp;
	bool m_target_dirty = true;
	bool m_previous_use_dst_alpha = false;
	D3DVertexFormat* m_previous_vertex_format = nullptr;
};

}
