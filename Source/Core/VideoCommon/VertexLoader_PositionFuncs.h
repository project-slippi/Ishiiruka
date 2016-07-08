// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Added for Ishiiruka by Tino
#pragma once
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexLoadingSSE.h"

template <typename T>
__forceinline float PosScale(TPipelineState &pipelinestate, T val)
{
	return val * pipelinestate.posScale;
}

template <>
__forceinline float PosScale(TPipelineState &pipelinestate, float val)
{
	return val;
}

template <typename T>
__forceinline u8* IndexedDataPosition(TPipelineState &pipelinestate)
{
	auto const index = pipelinestate.Read<T>();
	if (index == std::numeric_limits<T>::max())
	{
		g_PipelineState.skippedVertices++;
		pipelinestate.flags |= TPS_SKIP_VERTEX;
	}
	return cached_arraybases[ARRAY_POSITION] + (index * g_main_cp_state.array_strides[ARRAY_POSITION]);
}

template <typename T, int N>
__forceinline void _Pos_ReadDirect(TPipelineState &pipelinestate)
{
	static_assert(N <= 3, "N > 3 is not sane!");

	for (int i = 0; i < 3; ++i)
		pipelinestate.Write(i < N ? PosScale(pipelinestate, pipelinestate.Read<T>()) : 0.f);
}

template <typename I, typename T, int N>
__forceinline void _Pos_ReadIndex(TPipelineState &pipelinestate)
{
	static_assert(!std::numeric_limits<I>::is_signed, "Only unsigned I is sane!");
	static_assert(N <= 3, "N > 3 is not sane!");

	auto const data = reinterpret_cast<const T*>(IndexedDataPosition<I>(pipelinestate));

	for (int i = 0; i < 3; ++i)
		pipelinestate.Write(i < N ? PosScale(pipelinestate, Common::FromBigEndian(data[i])) : 0.f);
}

#if _M_SSE >= 0x301
template <bool three>
__forceinline void _Pos_ReadDirect_UByte_SSSE3(TPipelineState &pipelinestate)
{
	const u32* pData = reinterpret_cast<const u32*>(pipelinestate.GetReadPosition());
	const __m128 scale = _mm_set_ps1(pipelinestate.posScale);
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	if (three)
	{
		pipelinestate.ReadSkip(3);
		UByte3ToFloat3_SSSE3(pData, scale, dst);
	}
	else
	{
		pipelinestate.ReadSkip(2);
		UByte2ToFloat3_SSSE3(pData, scale, dst);
	}
	pipelinestate.WriteSkip(sizeof(float) * 3);
}

template <bool three>
__forceinline void _Pos_ReadDirect_SByte_SSSE3(TPipelineState &pipelinestate)
{
	const u32* pData = reinterpret_cast<const u32*>(pipelinestate.GetReadPosition());
	const __m128 scale = _mm_set_ps1(pipelinestate.posScale);
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	if (three)
	{
		pipelinestate.ReadSkip(3);
		SByte3ToFloat3_SSSE3(pData, scale, dst);
	}
	else
	{
		pipelinestate.ReadSkip(2);
		SByte2ToFloat3_SSSE3(pData, scale, dst);
	}
	pipelinestate.WriteSkip(sizeof(float) * 3);
}

template <bool three>
__forceinline void _Pos_ReadDirect_UShort_SSSE3(TPipelineState &pipelinestate)
{
	const u8* pData = pipelinestate.GetReadPosition();
	const __m128 scale = _mm_set_ps1(pipelinestate.posScale);
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	if (three)
	{
		pipelinestate.ReadSkip(3 * sizeof(u16));
		UShort3ToFloat3_SSSE3(reinterpret_cast<const __m128i*>(pData), scale, dst);
	}
	else
	{
		pipelinestate.ReadSkip(2 * sizeof(u16));
		UShort2ToFloat3_SSSE3(reinterpret_cast<const u32*>(pData), scale, dst);
	}
	pipelinestate.WriteSkip(sizeof(float) * 3);
}

template <bool three>
__forceinline void _Pos_ReadDirect_Short_SSSE3(TPipelineState &pipelinestate)
{
	const u8* pData = pipelinestate.GetReadPosition();
	const __m128 scale = _mm_set_ps1(pipelinestate.posScale);
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	if (three)
	{
		pipelinestate.ReadSkip(3 * sizeof(s16));
		Short3ToFloat3_SSSE3(reinterpret_cast<const __m128i*>(pData), scale, dst);
	}
	else
	{
		pipelinestate.ReadSkip(2 * sizeof(s16));
		Short2ToFloat3_SSSE3(reinterpret_cast<const u32*>(pData), scale, dst);
	}
	pipelinestate.WriteSkip(sizeof(float) * 3);
}

template <bool three>
__forceinline void _Pos_ReadDirect_Float_SSSE3(TPipelineState &pipelinestate)
{
	const __m128i* pData = reinterpret_cast<const __m128i*>(pipelinestate.GetReadPosition());
	__m128i* dst = reinterpret_cast<__m128i*>(pipelinestate.GetWritePosition());
	if (three)
	{
		pipelinestate.ReadSkip(sizeof(float) * 3);
		Float3ToFloat3sse3(dst, pData);
	}
	else
	{
		pipelinestate.ReadSkip(sizeof(float) * 2);
		Float2ToFloat3sse3(dst, pData);
	}
	pipelinestate.WriteSkip(sizeof(float) * 3);
}

template <typename I, bool three>
__forceinline void _Pos_ReadIndex_UByte_SSSE3(TPipelineState &pipelinestate)
{
	const u32* pData = reinterpret_cast<const u32*>(IndexedDataPosition<I>(pipelinestate));
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	const __m128 scale = _mm_set_ps1(pipelinestate.posScale);
	if (three)
	{
		UByte3ToFloat3_SSSE3(pData, scale, dst);
	}
	else
	{
		UByte2ToFloat3_SSSE3(pData, scale, dst);
	}
	pipelinestate.WriteSkip(sizeof(float) * 3);
}

template <typename I, bool three>
__forceinline void _Pos_ReadIndex_SByte_SSSE3(TPipelineState &pipelinestate)
{
	const u32* pData = reinterpret_cast<const u32*>(IndexedDataPosition<I>(pipelinestate));
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	const __m128 scale = _mm_set_ps1(pipelinestate.posScale);
	if (three)
	{
		SByte3ToFloat3_SSSE3(pData, scale, dst);
	}
	else
	{
		SByte2ToFloat3_SSSE3(pData, scale, dst);
	}
	pipelinestate.WriteSkip(sizeof(float) * 3);
}

template <typename I, bool three>
__forceinline void _Pos_ReadIndex_UShort_SSSE3(TPipelineState &pipelinestate)
{
	const u8* pData = IndexedDataPosition<I>(pipelinestate);
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	const __m128 scale = _mm_set_ps1(pipelinestate.posScale);
	if (three)
	{
		UShort3ToFloat3_SSSE3(reinterpret_cast<const __m128i*>(pData), scale, dst);
	}
	else
	{
		UShort2ToFloat3_SSSE3(reinterpret_cast<const u32*>(pData), scale, dst);
	}
	pipelinestate.WriteSkip(sizeof(float) * 3);
}

template <typename I, bool three>
__forceinline void _Pos_ReadIndex_Short_SSSE3(TPipelineState &pipelinestate)
{
	const u8* pData = IndexedDataPosition<I>(pipelinestate);
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	const __m128 scale = _mm_set_ps1(pipelinestate.posScale);
	if (three)
	{
		Short3ToFloat3_SSSE3(reinterpret_cast<const __m128i*>(pData), scale, dst);
	}
	else
	{
		Short2ToFloat3_SSSE3(reinterpret_cast<const u32*>(pData), scale, dst);
	}
	pipelinestate.WriteSkip(sizeof(float) * 3);
}

template <typename I, bool three>
__forceinline void _Pos_ReadIndex_Float_SSSE3(TPipelineState &pipelinestate)
{
	const __m128i* pData = reinterpret_cast<const __m128i*>(IndexedDataPosition<I>(pipelinestate));
	__m128i* dst = reinterpret_cast<__m128i*>(pipelinestate.GetWritePosition());
	if (three)
	{
		Float3ToFloat3sse3(dst, pData);
	}
	else
	{
		Float2ToFloat3sse3(dst, pData);
	}
	pipelinestate.WriteSkip(sizeof(float) * 3);
}


#endif

enum EPosElements
{
	POS_ELEMENTS_2 = 0,
	POS_ELEMENTS_3 = 1,
	POS_ELEMENTS
};