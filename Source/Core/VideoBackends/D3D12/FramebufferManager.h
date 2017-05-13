// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include "Common/Align.h"

#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/D3DTexture.h"
#include "VideoCommon/FramebufferManagerBase.h"

namespace DX12
{

// On the GameCube, the game sends a request for the graphics processor to
// transfer its internal EFB (Embedded Framebuffer) to an area in GameCube RAM
// called the XFB (External Framebuffer). The size and location of the XFB is
// decided at the time of the copy, and the format is always YUYV. The video
// interface is given a pointer to the XFB, which will be decoded and
// displayed on the TV.
//
// There are two ways for Dolphin to emulate this:
//
// Real XFB mode:
//
// Dolphin will behave like the GameCube and encode the EFB to
// a portion of GameCube RAM. The emulated video interface will decode the data
// for output to the screen.
//
// Advantages: Behaves exactly like the GameCube.
// Disadvantages: Resolution will be limited.
//
// Virtual XFB mode:
//
// When a request is made to copy the EFB to an XFB, Dolphin
// will remember the RAM location and size of the XFB in a Virtual XFB list.
// The video interface will look up the XFB in the list and use the enhanced
// data stored there, if available.
//
// Advantages: Enables high resolution graphics, better than real hardware.
// Disadvantages: If the GameCube CPU writes directly to the XFB (which is
// possible but uncommon), the Virtual XFB will not capture this information.

// There may be multiple XFBs in GameCube RAM. This is the maximum number to
// virtualize.

struct XFBSource final : public XFBSourceBase
{
	XFBSource(D3DTexture2D* tex, int slices) : m_tex(tex), m_depthtex(nullptr), m_slices(slices)
	{}
	~XFBSource()
	{
		SAFE_RELEASE(m_tex); SAFE_RELEASE(m_depthtex);
	}

	void DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight) override;
	void CopyEFB(float gamma) override;

	D3DTexture2D* m_tex;
	D3DTexture2D* m_depthtex;
	const int m_slices;
};

class FramebufferManager final : public FramebufferManagerBase
{
public:
	FramebufferManager(u32 target_width, u32 target_height);
	~FramebufferManager();

	static D3DTexture2D*& GetEFBColorTexture();
	static D3DTexture2D*& GetEFBDepthTexture();
	static D3DTexture2D*& GetResolvedEFBColorTexture();
	static D3DTexture2D*& GetResolvedEFBDepthTexture();

	static D3DTexture2D*& GetEFBColorTempTexture();
	static void SwapReinterpretTexture();

	static void ResolveDepthTexture();

	static inline void RestoreEFBRenderTargets()
	{
		D3D::current_command_list->OMSetRenderTargets(1,
			&FramebufferManager::GetEFBColorTexture()->GetRTV(), FALSE,
			&FramebufferManager::GetEFBDepthTexture()->GetDSV());
	}

	static u32 GetEFBCachedColor(u32 x, u32 y);
	static float GetEFBCachedDepth(u32 x, u32 y);
	static void SetEFBCachedColor(u32 x, u32 y, u32 value);
	static void SetEFBCachedDepth(u32 x, u32 y, float value);
	static void PopulateEFBColorCache();
	static void PopulateEFBDepthCache();
	static void InvalidateEFBCache();

private:
	std::unique_ptr<XFBSourceBase> CreateXFBSource(unsigned int target_width, unsigned int target_height, unsigned int layers) override;
	void GetTargetSize(unsigned int* width, unsigned int* height) override;
	static void InitializeEFBCache(const D3D12_CLEAR_VALUE& color_clear_value, const D3D12_CLEAR_VALUE& depth_clear_value);
	void CopyToRealXFB(u32 xfbAddr, u32 fbStride, u32 fbHeight, const EFBRectangle& sourceRc, float gamma) override;


	static struct Efb
	{
		D3DTexture2D* color_tex{};
		D3DTexture2D* resolved_color_tex{};

		D3DTexture2D* depth_tex{};
		D3DTexture2D* resolved_depth_tex{};

		D3DTexture2D* color_temp_tex{};

		// EFB Cache

		D3DTexture2D* color_cache_tex{};
		ComPtr<ID3D12Resource> color_cache_buf;
		u8* color_cache_data{};

		D3DTexture2D* depth_cache_tex{};
		ComPtr<ID3D12Resource> depth_cache_buf;
		u8* depth_cache_data{};

		int slices{};
	} m_efb;
	static constexpr size_t EFB_CACHE_PITCH = Common::AlignUpSizePow2(EFB_WIDTH * sizeof(int), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	static unsigned int m_target_width;
	static unsigned int m_target_height;
};

}  // namespace DX12
