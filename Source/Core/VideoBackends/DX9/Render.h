// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/RenderBase.h"

namespace DX9
{

class Renderer : public ::Renderer
{
private:
	bool m_bColorMaskChanged;
	bool m_bBlendModeChanged;
	bool m_bScissorRectChanged;
	bool m_bViewPortChanged;
	bool m_bViewPortChangedRequested;
	TargetRectangle m_ScissorRect;
	D3DVIEWPORT9 m_vp;
	bool m_bGenerationModeChanged;
	bool m_bDepthModeChanged;
	bool m_bLogicOpModeChanged;

	void _SetColorMask();
	void _SetViewport();
	void _SetBlendMode(bool forceUpdate);
	void _SetScissorRect();
	void _SetGenerationMode();
	void _SetDepthMode();
	void _SetLogicOpMode();
public:
	Renderer(void *&window_handle);
	~Renderer();

	void SetColorMask();
	void SetBlendMode(bool forceUpdate);
	void SetScissorRect(const TargetRectangle& rc);
	void SetGenerationMode();
	void SetDepthMode();
	void SetLogicOpMode();
	void SetDitherMode();
	void SetSamplerState(int stage, int texindex, bool custom_tex);
	void SetInterlacingMode();
	void SetViewport();

	void ApplyState(bool bUseDstAlpha);
	void RestoreState();

	void RenderText(const std::string& pstr, int left, int top, u32 color);

	u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data);
	void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) override;
	u16 BBoxRead(int index) override
	{
		return 0;
	};
	void BBoxWrite(int index, u16 value) override
	{};

	void ResetAPIState();
	void RestoreAPIState();

	TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc);

	void SwapImpl(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, const EFBRectangle& rc, u64 ticks, float Gamma = 1.0f);

	void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z);

	void ReinterpretPixelData(unsigned int convtype);

	static void CheckForResize(bool &resized);
};

}  // namespace DX9