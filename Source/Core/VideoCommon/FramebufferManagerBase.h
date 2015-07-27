// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <list>

#include "VideoCommon/VideoCommon.h"

inline bool addrRangesOverlap(u32 aLower, u32 aUpper, u32 bLower, u32 bUpper)
{
	return !((aLower >= bUpper) || (bLower >= aUpper));
}

struct XFBSourceBase
{
	virtual ~XFBSourceBase() {}

	virtual void DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight) = 0;

	virtual void CopyEFB(float Gamma) = 0;

	u32 srcAddr;
	u32 srcWidth;
	u32 srcHeight;

	u32 texWidth;
	u32 texHeight;

	TargetRectangle sourceRc;
};

class FramebufferManagerBase
{
public:
	enum
	{
		// There may be multiple XFBs in GameCube RAM. This is the maximum number to
		// virtualize.
		MAX_VIRTUAL_XFB = 8
	};

	FramebufferManagerBase();
	virtual ~FramebufferManagerBase();

	static void CopyToXFB(u32 xfbAddr, u32 fbStride, u32 fbHeight, const EFBRectangle& sourceRc, float Gamma);
	static const XFBSourceBase* const* GetXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32* xfbCount);

	static void SetLastXfbWidth(u32 width) { s_last_xfb_width = width; }
	static void SetLastXfbHeight(u32 height) { s_last_xfb_height = height; }
	static u32 LastXfbWidth() { return s_last_xfb_width; }
	static u32 LastXfbHeight() { return s_last_xfb_height; }

	static int ScaleToVirtualXfbWidth(int x);
	static int ScaleToVirtualXfbHeight(int y);

	static u32 GetEFBLayers() { return m_EFBLayers; }

protected:
	struct VirtualXFB
	{
		VirtualXFB() : xfbSource(nullptr) {}

		// Address and size in GameCube RAM
		u32 xfbAddr;
		u32 xfbWidth;
		u32 xfbHeight;

		XFBSourceBase *xfbSource;
	};

	typedef std::list<VirtualXFB> VirtualXFBListType;

	static u32 m_EFBLayers;

private:
	virtual XFBSourceBase* CreateXFBSource(u32 target_width, u32 target_height, u32 layers) = 0;
	// TODO: figure out why OGL is different for this guy
	virtual void GetTargetSize(u32 *width, u32 *height) = 0;

	static VirtualXFBListType::iterator FindVirtualXFB(u32 xfbAddr, u32 width, u32 height);

	static void ReplaceVirtualXFB();

	// TODO: merge these virtual funcs, they are nearly all the same
	virtual void CopyToRealXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc,float Gamma = 1.0f) = 0;
	static void CopyToVirtualXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc,float Gamma = 1.0f);

	static const XFBSourceBase* const* GetRealXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32* xfbCount);
	static const XFBSourceBase* const* GetVirtualXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32* xfbCount);

	static XFBSourceBase *m_realXFBSource; // Only used in Real XFB mode
	static VirtualXFBListType m_virtualXFBList; // Only used in Virtual XFB mode

	static const XFBSourceBase* m_overlappingXFBArray[MAX_VIRTUAL_XFB];

	static u32 s_last_xfb_width;
	static u32 s_last_xfb_height;
};

extern FramebufferManagerBase *g_framebuffer_manager;