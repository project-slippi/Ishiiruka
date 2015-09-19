// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include "Common/ConstantBuffer.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/XFMemory.h"
#include "VideoCommon/PixelShaderGen.h"

class PointerWrap;

// The non-API dependent parts.
class PixelShaderManager
{
public:
	static const size_t ConstantBufferSize = C_PENVCONST_END * 4;
	static void Init(bool use_integer_constants);
	static void Dirty();
	static void Shutdown();
	static void DoState(PointerWrap &p);
	static float* GetBufferToUpdate(u32 const_number, u32 size);
	static const float* GetBuffer();
	static inline bool IsDirty() { return m_buffer.IsDirty(); }
	static inline void Clear() { m_buffer.Clear(); }	
	static void EnableDirtyRegions();
	static void DisableDirtyRegions();
	static const regionvector &GetDirtyRegions();
	static void SetConstants(); // sets pixel shader constants

	// constant management, should be called after memory is committed
	static void SetTevColor(int index, int component, s32 value);
	static void SetTevKonstColor(int index, int component, s32 value);
	static void SetAlpha();
	static void SetDestAlpha();
	static void SetTexDims(int texmapid, u32 width, u32 height);
	static void SetZTextureBias();
	static void SetViewportChanged();
	static void SetEfbScaleChanged();
	static void SetZSlope(float dfdx, float dfdy, float f0);
	static void SetIndMatrixChanged(int matrixidx);
	static void SetTevKSelChanged(int id);
	static void SetZTextureTypeChanged();
	static void SetIndTexScaleChanged(bool high);
	static void SetTexCoordChanged(u8 texmapid);
	static void SetFogColorChanged();
	static void SetFogParamChanged();
	static void SetFogRangeAdjustChanged();
	static void SetColorMatrix(const float* pmatrix);
	static void InvalidateXFRange(int start, int end);
	static void SetMaterialColorChanged(int index);
	static void SetFlags(int index, int mask, int value);
private:
	static void SetPSTextureDims(int texid);
	alignas(16) static float psconstants[ConstantBufferSize];
	static ConstatBuffer m_buffer;
};