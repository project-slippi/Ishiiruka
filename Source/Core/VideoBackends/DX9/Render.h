// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/RenderBase.h"

namespace DX9
{

class Renderer : public ::Renderer
{
public:
	Renderer(void *&window_handle);
	~Renderer();
	void Init() override;
	void SetColorMask();
	void SetBlendMode(bool forceUpdate);
	void SetScissorRect(const EFBRectangle& rc);
	void SetGenerationMode();
	void SetDepthMode();
	void SetLogicOpMode();
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

	bool CheckForResize();
private:
	bool m_bColorMaskChanged = true;
	bool m_bBlendModeChanged = true;
	bool m_bScissorRectChanged = true;
	bool m_bViewPortChanged = true;
	bool m_bViewPortChangedRequested = true;
	EFBRectangle m_ScissorRect{};
	D3DVIEWPORT9 m_vp{};
	bool m_bGenerationModeChanged = true;
	bool m_bDepthModeChanged = true;
	bool m_bLogicOpModeChanged = true;
	u32 m_blendMode = 0;
	u32 m_LastAA = 0;
	bool m_IS_AMD = false;
	float m_fMax_Point_Size = 0;
	bool m_vsync = false;
	bool m_b3D_RightFrame = false;	
	bool m_last_fullscreen_mode = false;

	void _SetColorMask();
	void _SetViewport();
	void _SetBlendMode(bool forceUpdate);
	void _SetScissorRect();
	void _SetGenerationMode();
	void _SetDepthMode();
	void _SetLogicOpMode();
	void SetupDeviceObjects();	
	void TeardownDeviceObjects();
};

}  // namespace DX9