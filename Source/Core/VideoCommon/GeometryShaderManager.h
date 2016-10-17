// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/ConstantManager.h"

class PointerWrap;

// The non-API dependent parts.
class GeometryShaderManager
{
	static bool dirty;
	static bool s_projection_changed;
	static bool s_viewport_changed;
	static bool s_line_width_changed;
	static u32 s_texcoord_changed;
public:
	static void Init();
	static void Dirty();
	static inline bool IsDirty()
	{
		return dirty;
	}
	static inline void Clear()
	{
		dirty = false;
	}
	static void Shutdown();
	static void DoState(PointerWrap &p);

	static void SetConstants();
	static inline void SetViewportChanged()
	{
		s_viewport_changed = true;
	}

	static inline void SetProjectionChanged()
	{
		s_projection_changed = true;
	}

	static inline void SetLinePtWidthChanged()
	{
		s_line_width_changed = true;
	}

	static inline void SetTexCoordChanged(u8 texmapid)
	{
		s_texcoord_changed |= 1u << texmapid;
	}

	alignas(256) static GeometryShaderConstants constants;
};
