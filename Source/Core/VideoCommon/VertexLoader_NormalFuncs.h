// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Added for Ishiiruka by Tino
#pragma once
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexLoadingSSE.h"

#define NUM_NRM_FORMAT 5
#define NUM_NRM_TYPE 4

enum ENormalElements
{
	NRM_NBT = 0,
	NRM_NBT3 = 1,
	NUM_NRM_ELEMENTS
};

enum ENormalIndices
{
	NRM_INDICES1 = 0,
	NRM_INDICES3 = 1,
	NUM_NRM_INDICES
};

__forceinline float nrmFracAdjust(s8 val)
{
	const float scale = (1.0f / (1U << 6));
	return val * scale;
}

__forceinline float nrmFracAdjust(u8 val)
{
	const float scale = (1.0f / (1U << 7));
	return val * scale;
}

__forceinline float nrmFracAdjust(s16 val)
{
	const float scale = (1.0f / (1U << 14));
	return val * scale;
}

__forceinline float nrmFracAdjust(u16 val)
{
	const float scale = (1.0f / (1U << 15));
	return val * scale;
}

__forceinline float nrmFracAdjust(float val)
{
	return val;
}

template <typename T, int N>
__forceinline void ReadIndirect(TPipelineState &pipelinestate, const T* data)
{
	static_assert(3 == N || 9 == N, "N is only sane as 3 or 9!");

	for (int i = 0; i != N; ++i)
	{
		pipelinestate.Write(nrmFracAdjust(Common::FromBigEndian(data[i])));
	}
}

template <typename T, int N>
__forceinline void _Normal_Direct(TPipelineState &pipelinestate)
{
	auto const source = reinterpret_cast<const T*>(pipelinestate.GetReadPosition());
	ReadIndirect<T, N * 3>(pipelinestate, source);
	pipelinestate.ReadSkip(N * 3 * sizeof(T));
}

template <typename I, typename T, int Offset>
__forceinline u8* IndexedNormalPosition(TPipelineState &pipelinestate)
{
	auto const index = pipelinestate.Read<I>();
	return cached_arraybases[ARRAY_NORMAL]
		+ (index * g_main_cp_state.array_strides[ARRAY_NORMAL]) + sizeof(T) * 3 * Offset;
}

template <typename I, typename T, int N, int Offset>
__forceinline void _Normal_Index_Offset(TPipelineState &pipelinestate)
{
	static_assert(!std::numeric_limits<I>::is_signed, "Only unsigned I is sane!");

	auto const data = reinterpret_cast<const T*>(IndexedNormalPosition<I, T, Offset>(pipelinestate));
	ReadIndirect<T, N * 3>(pipelinestate, data);
}

template <typename I, typename T>
__forceinline void _Normal_Index_Offset3(TPipelineState &pipelinestate)
{
	_Normal_Index_Offset<I, T, 1, 0>(pipelinestate);
	_Normal_Index_Offset<I, T, 1, 1>(pipelinestate);
	_Normal_Index_Offset<I, T, 1, 2>(pipelinestate);
}

#if _M_SSE >= 0x301

template <int N>
__forceinline void _Normal_Direct_UByte_SSSE3(TPipelineState &pipelinestate)
{
	u8* src = pipelinestate.GetReadPosition();
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	const float frac = (1.0f / float(1U << 7));
	const __m128 scale = _mm_set_ps(frac, frac, frac, frac);
	UByte3ToFloat3_SSSE3(reinterpret_cast<const u32*>(src), scale, dst);
	if (N > 1)
	{
		src += 3;
		dst += 3;
		UByte3ToFloat3_SSSE3(reinterpret_cast<const u32*>(src), scale, dst);
		src += 3;
		dst += 3;
		UByte3ToFloat3_SSSE3(reinterpret_cast<const u32*>(src), scale, dst);
	}
	src += 3;
	dst += 3;
	pipelinestate.SetReadPosition(src);
	pipelinestate.SetWritePosition(reinterpret_cast<u8*>(dst));
}

template <int N>
__forceinline void _Normal_Direct_SByte_SSSE3(TPipelineState &pipelinestate)
{
	u8* src = pipelinestate.GetReadPosition();
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	const float frac = (1.0f / float(1U << 6));
	const __m128 scale = _mm_set_ps(frac, frac, frac, frac);
	SByte3ToFloat3_SSSE3(reinterpret_cast<const u32*>(src), scale, dst);
	if (N > 1)
	{
		src += 3;
		dst += 3;
		SByte3ToFloat3_SSSE3(reinterpret_cast<const u32*>(src), scale, dst);
		src += 3;
		dst += 3;
		SByte3ToFloat3_SSSE3(reinterpret_cast<const u32*>(src), scale, dst);
	}
	src += 3;
	dst += 3;
	pipelinestate.SetReadPosition(src);
	pipelinestate.SetWritePosition(reinterpret_cast<u8*>(dst));
}

template <int N>
__forceinline void _Normal_Direct_UShort_SSSE3(TPipelineState &pipelinestate)
{
	u8* src = pipelinestate.GetReadPosition();
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	const float frac = (1.0f / float(1U << 15));
	const __m128 scale = _mm_set_ps(frac, frac, frac, frac);
	UShort3ToFloat3_SSSE3(reinterpret_cast<const __m128i*>(src), scale, dst);
	if (N > 1)
	{
		src += 3 * sizeof(u16);
		dst += 3;
		UShort3ToFloat3_SSSE3(reinterpret_cast<const __m128i*>(src), scale, dst);
		src += 3 * sizeof(u16);
		dst += 3;
		UShort3ToFloat3_SSSE3(reinterpret_cast<const __m128i*>(src), scale, dst);
	}
	src += 3 * sizeof(u16);
	dst += 3;
	pipelinestate.SetReadPosition(src);
	pipelinestate.SetWritePosition(reinterpret_cast<u8*>(dst));
}

template <int N>
__forceinline void _Normal_Direct_Short_SSSE3(TPipelineState &pipelinestate)
{
	u8* src = pipelinestate.GetReadPosition();
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	const float frac = (1.0f / float(1U << 14));
	const __m128 scale = _mm_set_ps(frac, frac, frac, frac);
	Short3ToFloat3_SSSE3(reinterpret_cast<const __m128i*>(src), scale, dst);
	if (N > 1)
	{
		src += 3 * sizeof(s16);
		dst += 3;
		Short3ToFloat3_SSSE3(reinterpret_cast<const __m128i*>(src), scale, dst);
		src += 3 * sizeof(s16);
		dst += 3;
		Short3ToFloat3_SSSE3(reinterpret_cast<const __m128i*>(src), scale, dst);
	}
	src += 3 * sizeof(s16);
	dst += 3;
	pipelinestate.SetReadPosition(src);
	pipelinestate.SetWritePosition(reinterpret_cast<u8*>(dst));
}

template <int N>
__forceinline void _Normal_Direct_FLOAT_SSSE3(TPipelineState &pipelinestate)
{
	float* src = reinterpret_cast<float*>(pipelinestate.GetReadPosition());
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	Float3ToFloat3sse3((__m128i*)dst, (const __m128i*)src);
	if (N > 1)
	{
		src += 3;
		dst += 3;
		Float3ToFloat3sse3((__m128i*)dst, (const __m128i*)src);
		src += 3;
		dst += 3;
		Float3ToFloat3sse3((__m128i*)dst, (const __m128i*)src);
	}
	src += 3;
	dst += 3;
	pipelinestate.SetReadPosition(reinterpret_cast<u8*>(src));
	pipelinestate.SetWritePosition(reinterpret_cast<u8*>(dst));
}

template <typename I, int N>
__forceinline void _Normal_Index_UByte_SSSE3(TPipelineState &pipelinestate)
{
	static_assert(!std::numeric_limits<I>::is_signed, "Only unsigned I is sane!");
	u8* src = IndexedNormalPosition<I, u8, 0>(pipelinestate);
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	const float frac = (1.0f / float(1U << 7));
	const __m128 scale = _mm_set_ps(frac, frac, frac, frac);
	UByte3ToFloat3_SSSE3(reinterpret_cast<const u32*>(src), scale, dst);
	if (N > 1)
	{
		src += 3;
		dst += 3;
		UByte3ToFloat3_SSSE3(reinterpret_cast<const u32*>(src), scale, dst);
		src += 3;
		dst += 3;
		UByte3ToFloat3_SSSE3(reinterpret_cast<const u32*>(src), scale, dst);
	}
	dst += 3;
	pipelinestate.SetWritePosition(reinterpret_cast<u8*>(dst));
}

template <typename I, int N>
__forceinline void _Normal_Index_SByte_SSSE3(TPipelineState &pipelinestate)
{
	static_assert(!std::numeric_limits<I>::is_signed, "Only unsigned I is sane!");
	u8* src = IndexedNormalPosition<I, s8, 0>(pipelinestate);
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	const float frac = (1.0f / float(1U << 6));
	const __m128 scale = _mm_set_ps(frac, frac, frac, frac);
	SByte3ToFloat3_SSSE3(reinterpret_cast<const u32*>(src), scale, dst);
	if (N > 1)
	{
		src += 3;
		dst += 3;
		SByte3ToFloat3_SSSE3(reinterpret_cast<const u32*>(src), scale, dst);
		src += 3;
		dst += 3;
		SByte3ToFloat3_SSSE3(reinterpret_cast<const u32*>(src), scale, dst);
	}
	dst += 3;
	pipelinestate.SetWritePosition(reinterpret_cast<u8*>(dst));
}

template <typename I, int N>
__forceinline void _Normal_Index_UShort_SSSE3(TPipelineState &pipelinestate)
{
	static_assert(!std::numeric_limits<I>::is_signed, "Only unsigned I is sane!");
	u8* src = IndexedNormalPosition<I, u16, 0>(pipelinestate);
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	const float frac = (1.0f / float(1U << 15));
	const __m128 scale = _mm_set_ps(frac, frac, frac, frac);
	UShort3ToFloat3_SSSE3(reinterpret_cast<const __m128i*>(src), scale, dst);
	if (N > 1)
	{
		src += 3 * sizeof(u16);
		dst += 3;
		UShort3ToFloat3_SSSE3(reinterpret_cast<const __m128i*>(src), scale, dst);
		src += 3 * sizeof(u16);
		dst += 3;
		UShort3ToFloat3_SSSE3(reinterpret_cast<const __m128i*>(src), scale, dst);
	}
	dst += 3;
	pipelinestate.SetWritePosition(reinterpret_cast<u8*>(dst));
}

template <typename I, int N>
__forceinline void _Normal_Index_Short_SSSE3(TPipelineState &pipelinestate)
{
	static_assert(!std::numeric_limits<I>::is_signed, "Only unsigned I is sane!");
	u8* src = IndexedNormalPosition<I, s16, 0>(pipelinestate);
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	const float frac = (1.0f / float(1U << 14));
	const __m128 scale = _mm_set_ps(frac, frac, frac, frac);
	Short3ToFloat3_SSSE3(reinterpret_cast<const __m128i*>(src), scale, dst);
	if (N > 1)
	{
		src += 3 * sizeof(s16);
		dst += 3;
		Short3ToFloat3_SSSE3(reinterpret_cast<const __m128i*>(src), scale, dst);
		src += 3 * sizeof(s16);
		dst += 3;
		Short3ToFloat3_SSSE3(reinterpret_cast<const __m128i*>(src), scale, dst);
	}
	dst += 3;
	pipelinestate.SetWritePosition(reinterpret_cast<u8*>(dst));
}

template <typename I, int N>
__forceinline void _Normal_Index_FLOAT_SSSE3(TPipelineState &pipelinestate)
{
	static_assert(!std::numeric_limits<I>::is_signed, "Only unsigned I is sane!");

	float* src = reinterpret_cast<float*>(IndexedNormalPosition<I, float, 0>(pipelinestate));
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	Float3ToFloat3sse3((__m128i*)dst, (const __m128i*)src);
	if (N > 1)
	{
		src += 3;
		dst += 3;
		Float3ToFloat3sse3((__m128i*)dst, (const __m128i*)src);
		src += 3;
		dst += 3;
		Float3ToFloat3sse3((__m128i*)dst, (const __m128i*)src);
	}
	dst += 3;
	pipelinestate.SetWritePosition(reinterpret_cast<u8*>(dst));
}

template <typename I>
__forceinline void _Normal_Index3_UByte_SSSE3(TPipelineState &pipelinestate)
{
	static_assert(!std::numeric_limits<I>::is_signed, "Only unsigned I is sane!");
	u32* src = reinterpret_cast<u32*>(IndexedNormalPosition<I, u8, 0>(pipelinestate));
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	const float frac = (1.0f / float(1U << 7));
	const __m128 scale = _mm_set_ps(frac, frac, frac, frac);
	UByte3ToFloat3_SSSE3(src, scale, dst);
	dst += 3;
	src = reinterpret_cast<u32*>(IndexedNormalPosition<I, u8, 1>(pipelinestate));
	UByte3ToFloat3_SSSE3(src, scale, dst);
	dst += 3;
	src = reinterpret_cast<u32*>(IndexedNormalPosition<I, u8, 2>(pipelinestate));
	UByte3ToFloat3_SSSE3(src, scale, dst);
	dst += 3;
	pipelinestate.SetWritePosition(reinterpret_cast<u8*>(dst));
}

template <typename I>
__forceinline void _Normal_Index3_SByte_SSSE3(TPipelineState &pipelinestate)
{
	static_assert(!std::numeric_limits<I>::is_signed, "Only unsigned I is sane!");
	u32* src = reinterpret_cast<u32*>(IndexedNormalPosition<I, s8, 0>(pipelinestate));
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	const float frac = (1.0f / float(1U << 6));
	const __m128 scale = _mm_set_ps(frac, frac, frac, frac);
	SByte3ToFloat3_SSSE3(src, scale, dst);
	dst += 3;
	src = reinterpret_cast<u32*>(IndexedNormalPosition<I, s8, 1>(pipelinestate));
	SByte3ToFloat3_SSSE3(src, scale, dst);
	dst += 3;
	src = reinterpret_cast<u32*>(IndexedNormalPosition<I, s8, 2>(pipelinestate));
	SByte3ToFloat3_SSSE3(src, scale, dst);
	dst += 3;
	pipelinestate.SetWritePosition(reinterpret_cast<u8*>(dst));
}

template <typename I>
__forceinline void _Normal_Index3_UShort_SSSE3(TPipelineState &pipelinestate)
{
	static_assert(!std::numeric_limits<I>::is_signed, "Only unsigned I is sane!");
	__m128i* src = reinterpret_cast<__m128i*>(IndexedNormalPosition<I, u16, 0>(pipelinestate));
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	const float frac = (1.0f / float(1U << 15));
	const __m128 scale = _mm_set_ps(frac, frac, frac, frac);
	UShort3ToFloat3_SSSE3(src, scale, dst);
	dst += 3;
	src = reinterpret_cast<__m128i*>(IndexedNormalPosition<I, u16, 1>(pipelinestate));
	UShort3ToFloat3_SSSE3(src, scale, dst);
	dst += 3;
	src = reinterpret_cast<__m128i*>(IndexedNormalPosition<I, u16, 2>(pipelinestate));
	UShort3ToFloat3_SSSE3(src, scale, dst);
	dst += 3;
	pipelinestate.SetWritePosition(reinterpret_cast<u8*>(dst));
}

template <typename I>
__forceinline void _Normal_Index3_Short_SSSE3(TPipelineState &pipelinestate)
{
	static_assert(!std::numeric_limits<I>::is_signed, "Only unsigned I is sane!");
	__m128i* src = reinterpret_cast<__m128i*>(IndexedNormalPosition<I, s16, 0>(pipelinestate));
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	const float frac = (1.0f / float(1U << 14));
	const __m128 scale = _mm_set_ps(frac, frac, frac, frac);
	Short3ToFloat3_SSSE3(src, scale, dst);
	dst += 3;
	src = reinterpret_cast<__m128i*>(IndexedNormalPosition<I, s16, 1>(pipelinestate));
	Short3ToFloat3_SSSE3(src, scale, dst);
	dst += 3;
	src = reinterpret_cast<__m128i*>(IndexedNormalPosition<I, s16, 2>(pipelinestate));
	Short3ToFloat3_SSSE3(src, scale, dst);
	dst += 3;
	pipelinestate.SetWritePosition(reinterpret_cast<u8*>(dst));
}

template <typename I>
__forceinline void _Normal_Index3_FLOAT_SSSE3(TPipelineState &pipelinestate)
{
	static_assert(!std::numeric_limits<I>::is_signed, "Only unsigned I is sane!");
	float* dst = reinterpret_cast<float*>(pipelinestate.GetWritePosition());
	float* src = reinterpret_cast<float*>(IndexedNormalPosition<I, float, 0>(pipelinestate));
	Float3ToFloat3sse3((__m128i*)dst, (const __m128i*)src);
	dst += 3;
	src = reinterpret_cast<float*>(IndexedNormalPosition<I, float, 1>(pipelinestate));
	Float3ToFloat3sse3((__m128i*)dst, (const __m128i*)src);
	dst += 3;
	src = reinterpret_cast<float*>(IndexedNormalPosition<I, float, 2>(pipelinestate));
	Float3ToFloat3sse3((__m128i*)dst, (const __m128i*)src);
	dst += 3;
	pipelinestate.SetWritePosition(reinterpret_cast<u8*>(dst));
}

#endif