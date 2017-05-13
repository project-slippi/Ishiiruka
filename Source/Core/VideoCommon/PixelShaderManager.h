// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include "Common/ConstantBuffer.h"
#include "VideoCommon/PixelShaderGen.h"

class PointerWrap;

// The non-API dependent parts.
class PixelShaderManager
{
public:
	static const size_t ConstantBufferSize = C_PENVCONST_END * 4;
	static void Init(bool use_integer_constants);
	static void Dirty();
	static void DoState(PointerWrap &p);
	static float* GetBufferToUpdate(u32 const_number, u32 size);
	static const float* GetBuffer();
	static inline bool IsDirty()
	{
		return m_buffer.IsDirty();
	}
	static inline void Clear()
	{
		m_buffer.Clear();
	}
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

	static inline void SetViewportChanged()
	{
		s_bViewPortChanged = true;
	}

	static inline void SetIndTexScaleChanged(bool high)
	{
		s_nIndTexScaleChanged |= high ? 0x0c : 0x03;
	}

	static inline void SetIndMatrixChanged(int matrixidx)
	{
		s_nIndTexMtxChanged |= 1 << matrixidx;
	}

	static inline void SetZTextureTypeChanged()
	{
		s_bZTextureTypeChanged = true;
	}

	static inline void SetTexCoordChanged(u8 texmapid)
	{
		s_nTexDimsChanged |= 1 << texmapid;
	}

	static inline void SetFogColorChanged()
	{
		s_bFogColorChanged = true;
	}

	static inline void SetFogParamChanged()
	{
		s_bFogParamChanged = true;
	}

	static inline void SetFogRangeAdjustChanged()
	{
		s_bFogRangeAdjustChanged = true;
	}

	static inline void SetMaterialColorChanged(int index)
	{
		s_materials_changed |= (1 << index);
	}

	static inline void SetEfbScaleChanged()
	{
		s_EfbScaleChanged = true;
	}

	static void SetZSlope(float dfdx, float dfdy, float f0);
	static void SetColorMatrix(const float* pmatrix);
	static void InvalidateXFRange(u32 start, u32 end);
	static void SetFlags(int index, int mask, int value);
private:
	static int s_nIndTexMtxChanged;
	static bool s_bAlphaChanged;
	static bool s_bZBiasChanged;
	static bool s_bZTextureTypeChanged;
	static bool s_bFogColorChanged;
	static bool s_bFogParamChanged;
	static bool s_bFogRangeAdjustChanged;
	static bool s_bViewPortChanged;
	static bool s_EfbScaleChanged;
	static u8 s_nTexDimsChanged;
	static u8 s_nIndTexScaleChanged;
	static int s_materials_changed;
	static void SetPSTextureDims(int texid);
	alignas(256) static float psconstants[ConstantBufferSize];
	static ConstatBuffer m_buffer;
};