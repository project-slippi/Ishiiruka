// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include "Common/Common.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/XFMemory.h"


class NativeVertexFormat;
class PointerWrap;

enum PrimitiveType
{
	PRIMITIVE_POINTS,
	PRIMITIVE_LINES,
	PRIMITIVE_TRIANGLES,
};

struct Slope
{
	float dfdx;
	float dfdy;
	float f0;
};

class VertexManagerBase
{
public:
	static constexpr u32 SMALLEST_POSSIBLE_VERTEX = sizeof(float) * 3;                 // 3 pos
	static constexpr u32 LARGEST_POSSIBLE_VERTEX = sizeof(float) * 45 + sizeof(u32) * 2; // 3 pos, 3*3 normal, 2*u32 color, 8*4 tex, 1 posMat 

	static constexpr u32 MAX_PRIMITIVES_PER_COMMAND = (u16)-1;

	static constexpr u32 MAXVBUFFERSIZE = ROUND_UP_POW2(MAX_PRIMITIVES_PER_COMMAND * LARGEST_POSSIBLE_VERTEX);

	// We may convert triangle-fans to triangle-lists, almost 3x as many indices.
	static constexpr u32 MAXIBUFFERSIZE = ROUND_UP_POW2(MAX_PRIMITIVES_PER_COMMAND * 3);

	VertexManagerBase();
	// needs to be virtual for DX11's dtor
	virtual ~VertexManagerBase();

	PrimitiveType GetPrimitiveType(int primitive);
	void PrepareForAdditionalData(int primitive, u32 count, u32 stride);

	virtual void PrepareShaders(PrimitiveType primitive, u32 components, const XFMemory &xfr, const BPMemory &bpm, bool ongputhread) = 0;
	void Flush()
	{
		if (m_is_flushed)
			return;
		DoFlush();
	}

	virtual std::unique_ptr<NativeVertexFormat> CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl) = 0;

	void DoState(PointerWrap& p);
	virtual void CreateDeviceObjects() {};
	virtual void DestroyDeviceObjects() {};
	std::pair<size_t, size_t> ResetFlushAspectRatioCount();
	inline void IncCurrentBufferPointer(size_t size)
	{
		m_pCurBufferPointer += size;
	}
	inline u8* GetCurrentBufferPointer() const
	{
		return m_pCurBufferPointer;
	}

	inline size_t GetRemainingSize()
	{
		return m_pEndBufferPointer - m_pCurBufferPointer;
	}
protected:
	bool m_is_flushed = true;
	bool m_shader_refresh_required = true;
	bool m_zslope_refresh_required = true;
	Slope m_zslope = { 0.0f, 0.0f, float(0xFFFFFF) };
	PrimitiveType m_current_primitive_type{};
	u8 *m_pCurBufferPointer = nullptr;
	u8 *m_pBaseBufferPointer = nullptr;
	u8 *m_pEndBufferPointer = nullptr;

	bool m_cull_all = false;

	void CalculateZSlope(const PortableVertexDeclaration &vert_decl, const u16* indices);
	virtual void vDoState(PointerWrap& p) {}
	virtual void ResetBuffer(u32 stride) = 0;

private:
	size_t m_flush_count_4_3 = 0;
	size_t m_flush_count_anamorphic = 0;

	void DoFlush();

	virtual void vFlush(bool useDstAlpha) = 0;
	virtual u16* GetIndexBuffer() = 0;
};

extern std::unique_ptr<VertexManagerBase> g_vertex_manager;