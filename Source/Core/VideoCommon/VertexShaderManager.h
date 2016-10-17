// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include "Common/ConstantBuffer.h"
#include "VideoCommon/VertexShaderGen.h"

class PointerWrap;

void UpdateProjectionHack(int iParams[], std::string sParams[]);

// The non-API dependent parts.
class VertexShaderManager
{
public:
	static const size_t ConstantBufferSize = C_VENVCONST_END * 4;
	static void Init();
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
	// constant management
	static void SetConstants();

	static void InvalidateXFRange(u32 start, u32 end);
	static void SetTexMatrixChangedA(u32 value);
	static void SetTexMatrixChangedB(u32 value);

	static inline void SetViewportChanged()
	{
		bViewportChanged = true;
	}

	static inline void SetProjectionChanged()
	{
		bProjectionChanged = true;
	}

	static inline void SetMaterialColorChanged(int index)
	{
		s_materials_changed |= (1 << index);
	}


	static void TranslateView(float x, float y, float z = 0.0f);
	static void RotateView(float x, float y);
	static void ResetView();
	// data: raw vertex data
	// out: 4 floats which will be initialized with the corresponding clip space coordinates
	// NOTE: g_fProjectionMatrix must be up to date when this is called
	// (i.e. VertexShaderManager::SetConstants needs to be called before using this!)
	static void TransformToClipSpace(const u8* data, const PortableVertexDeclaration &vtx_dcl, float *out);
private:
	static bool bProjectionChanged;
	static bool bViewportChanged;
	static int s_materials_changed;
	alignas(256) static float vsconstants[ConstantBufferSize];
	static ConstatBuffer m_buffer;
};