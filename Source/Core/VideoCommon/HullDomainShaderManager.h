// Copyright 2015+ Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Created for Ishiiruka
#pragma once

#include "VideoCommon/ConstantManager.h"
#include "VideoCommon/HullDomainShaderGen.h"

class PointerWrap;

// The non-API dependent parts.
class HullDomainShaderManager
{
	static bool dirty;
public:
	static void Init();
	static void Dirty();
	static bool IsDirty() { return dirty; }
	static void Clear() { dirty = false; }
	static void Shutdown();
	static void DoState(PointerWrap &p);

	static void SetConstants();
	static void SetViewportChanged();
	static void SetProjectionChanged();
	static void SetHDSTextureDims(int texid);
	static void SetTexDims(int texmapid, u32 width, u32 height);
	static void SetIndMatrixChanged(int matrixidx);
	static void SetIndTexScaleChanged(bool high);
	static void SetTexCoordChanged(u8 texmapid);
	static void SetFlags(int index, int mask, int value);
	static HullDomainShaderConstants constants;
};
