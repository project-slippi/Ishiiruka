// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Common.h"
#include "Common/MathUtil.h"
#include "VideoCommon/VideoBackendBase.h"

// Global flag to signal if FifoRecorder is active.
extern bool g_bRecordFifoData;
// These are accurate (disregarding AA modes).
enum
{
	EFB_WIDTH = 640,
	EFB_HEIGHT = 528,
};

// Max XFB width is 720. You can only copy out 640 wide areas of efb to XFB
// so you need multiple copies to do the full width.
// The VI can do horizontal scaling (TODO: emulate).
const u32 MAX_XFB_WIDTH = 720;

// Although EFB height is 528, 576-line XFB's can be created either with
// vertical scaling by the EFB copy operation or copying to multiple XFB's
// that are next to each other in memory (TODO: handle that situation).
const u32 MAX_XFB_HEIGHT = 576;

// Logging
// ----------
void HandleGLError();


// This structure should only be used to represent a rectangle in EFB
// coordinates, where the origin is at the upper left and the frame dimensions
// are 640 x 528.
typedef MathUtil::Rectangle<int> EFBRectangle;

// This structure should only be used to represent a rectangle in standard target
// coordinates, where the origin is at the lower left and the frame dimensions
// depend on the resolution settings. Use Renderer::ConvertEFBRectangle to
// convert an EFBRectangle to a TargetRectangle.
struct TargetRectangle : public MathUtil::Rectangle<int>
{
	constexpr TargetRectangle() = default;

	constexpr TargetRectangle(int theLeft, int theTop, int theRight, int theBottom)
		: Rectangle<int>(theLeft, theTop, theRight, theBottom)
	{}

#ifdef _WIN32
	// Only used by D3D backend.
	const RECT *AsRECT() const
	{
		// The types are binary compatible so this works.
		return (const RECT *)this;
	}
	RECT *AsRECT()
	{
		// The types are binary compatible so this works.
		return (RECT *)this;
	}
#endif
};

// This structure describes a texture or screen resolution.
struct TargetSize final
{
	constexpr TargetSize() = default;
	constexpr TargetSize(int new_width, int new_height) : width(new_width), height(new_height)
	{}

	void Set(int new_width, int new_height)
	{
		width = new_width; height = new_height;
	}

	bool operator==(const TargetSize& rhs) const
	{
		return std::tie(width, height) == std::tie(rhs.width, rhs.height);
	}
	bool operator!=(const TargetSize& rhs) const
	{
		return std::tie(width, height) != std::tie(rhs.width, rhs.height);
	}
	bool operator<=(const TargetSize& rhs) const
	{
		return std::tie(width, height) <= std::tie(rhs.width, rhs.height);
	}
	bool operator>=(const TargetSize& rhs) const
	{
		return std::tie(width, height) >= std::tie(rhs.width, rhs.height);
	}
	bool operator<(const TargetSize& rhs) const
	{
		return std::tie(width, height) < std::tie(rhs.width, rhs.height);
	}
	bool operator>(const TargetSize& rhs) const
	{
		return std::tie(width, height) > std::tie(rhs.width, rhs.height);
	}

	int width = 1;
	int height = 1;
};

#ifdef _WIN32
#define PRIM_LOG(...) DEBUG_LOG(VIDEO, __VA_ARGS__)
#else
#define PRIM_LOG(...) DEBUG_LOG(VIDEO, ##__VA_ARGS__)
#endif

// warning: mapping buffer should be disabled to use this
// #define LOG_VTX() DEBUG_LOG(VIDEO, "vtx: %f %f %f, ", ((float*)VertexManagerBase::s_pCurBufferPointer)[-3], ((float*)VertexManagerBase::s_pCurBufferPointer)[-2], ((float*)VertexManagerBase::s_pCurBufferPointer)[-1]);

#define LOG_VTX()

typedef enum
{
	API_NONE = 0,
	API_OPENGL = 1,
	API_D3D9_SM30 = 2,
	API_D3D9_SM20 = 4,
	API_D3D9 = 6,
	API_D3D11 = 8,
	API_VULKAN = 16
} API_TYPE;

// Can be used for RGBA->BGRA or BGRA->RGBA
inline u32 RGBA8ToBGRA8(u32 src)
{
	u32 color = src;
	color &= 0xFF00FF00;
	color |= (src >> 16) & 0xFF;
	color |= (src << 16) & 0xFF0000;
	return color;
}

inline u32 RGBA8ToRGBA6ToRGBA8(u32 src)
{
	u32 color = src;
	color &= 0xFCFCFCFC;
	color |= (color >> 6) & 0x03030303;
	return color;
}

inline u32 RGBA8ToRGB565ToRGBA8(u32 src)
{
	u32 color = (src & 0xF8FCF8);
	color |= (color >> 5) & 0x070007;
	color |= (color >> 6) & 0x000300;
	color |= 0xFF000000;
	return color;
}

inline u32 Z24ToZ16ToZ24(u32 src)
{
	return (src & 0xFFFF00) | (src >> 16);
}

struct s_svar
{
	const char *name;
	const unsigned int reg;
	const unsigned int size;
};
