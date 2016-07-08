// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Added for Ishiiruka by Tino
#pragma once
#include "Common/Common.h"
#include "Common/CPUDetect.h"
#include "Common/Intrinsics.h"

#if _M_SSE >= 0x301
static const __m128i kMaskSwap32_2 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x04050607L, 0x00010203L);
static const __m128i kMaskSwap32_3 = _mm_set_epi32(0xFFFFFFFFL, 0x08090A0BL, 0x04050607L, 0x00010203L);
static const __m128i kMaskSwap32_4 = _mm_set_epi32(0x0C0D0E0FL, 0x08090A0BL, 0x04050607L, 0x00010203L);
static const __m128i kMaskSwap16to32l_3 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFF0405L, 0xFFFF0203L, 0xFFFF0001L);
static const __m128i kMaskSwap16to32l_2 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFF0203L, 0xFFFF0001L);
static const __m128i kMaskSwap16to32h_3 = _mm_set_epi32(0xFFFFFFFFL, 0x0405FFFFL, 0x0203FFFFL, 0x0001FFFFL);
static const __m128i kMaskSwap16to32h_2 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x0203FFFFL, 0x0001FFFFL);
static const __m128i kMask8to32l_3 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFF02L, 0xFFFFFF01L, 0xFFFFFF00L);
static const __m128i kMask8to32l_2 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFF01L, 0xFFFFFF00L);
static const __m128i kMask8to32h_3 = _mm_set_epi32(0xFFFFFFFFL, 0x02FFFFFFL, 0x01FFFFFFL, 0x00FFFFFFL);
static const __m128i kMask8to32h_2 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x01FFFFFFL, 0x00FFFFFFL);

__forceinline void UByte2ToFloat2_SSSE3(const u32* pData, __m128 scale, __m64* pDest)
{
	__m128i coords = _mm_cvtsi32_si128(*pData);
	coords = _mm_shuffle_epi8(coords, kMask8to32l_2);
	__m128 out = _mm_cvtepi32_ps(coords);
	out = _mm_mul_ps(out, scale);
	_mm_storel_pi(pDest, out);
}
__forceinline void UByte2ToFloat3_SSSE3(const u32* pData, __m128 scale, float* pDest)
{
	__m128i coords = _mm_cvtsi32_si128(*pData);
	coords = _mm_shuffle_epi8(coords, kMask8to32l_2);
	__m128 out = _mm_cvtepi32_ps(coords);
	out = _mm_mul_ps(out, scale);
	_mm_storeu_ps(pDest, out);
}

__forceinline void UByte3ToFloat3_SSSE3(const u32* pData, __m128 scale, float* pDest)
{
	__m128i coords = _mm_cvtsi32_si128(*pData);
	coords = _mm_shuffle_epi8(coords, kMask8to32l_3);
	__m128 out = _mm_cvtepi32_ps(coords);
	out = _mm_mul_ps(out, scale);
	_mm_storeu_ps(pDest, out);
}

__forceinline void SByte2ToFloat2_SSSE3(const u32* pData, __m128 scale, __m64* pDest)
{
	__m128i coords = _mm_cvtsi32_si128(*pData);
	coords = _mm_shuffle_epi8(coords, kMask8to32h_2);
	coords = _mm_srai_epi32(coords, 24);
	__m128 out = _mm_cvtepi32_ps(coords);
	out = _mm_mul_ps(out, scale);
	_mm_storel_pi(pDest, out);
}

__forceinline void SByte2ToFloat3_SSSE3(const u32* pData, __m128 scale, float* pDest)
{
	__m128i coords = _mm_cvtsi32_si128(*pData);
	coords = _mm_shuffle_epi8(coords, kMask8to32h_2);
	coords = _mm_srai_epi32(coords, 24);
	__m128 out = _mm_cvtepi32_ps(coords);
	out = _mm_mul_ps(out, scale);
	_mm_storeu_ps(pDest, out);
}

__forceinline void SByte3ToFloat3_SSSE3(const u32* pData, __m128 scale, float* pDest)
{
	__m128i coords = _mm_cvtsi32_si128(*pData);
	coords = _mm_shuffle_epi8(coords, kMask8to32h_3);
	coords = _mm_srai_epi32(coords, 24);
	__m128 out = _mm_cvtepi32_ps(coords);
	out = _mm_mul_ps(out, scale);
	_mm_storeu_ps(pDest, out);
}

__forceinline void UShort2ToFloat2_SSSE3(const u32* pData, __m128 scale, __m64* pDest)
{
	__m128i coords = _mm_cvtsi32_si128(*pData);
	coords = _mm_shuffle_epi8(coords, kMaskSwap16to32l_2);
	__m128 out = _mm_cvtepi32_ps(coords);
	out = _mm_mul_ps(out, scale);
	_mm_storel_pi(pDest, out);
}

__forceinline void UShort2ToFloat3_SSSE3(const u32* pData, __m128 scale, float* pDest)
{
	__m128i coords = _mm_cvtsi32_si128(*pData);
	coords = _mm_shuffle_epi8(coords, kMaskSwap16to32l_2);
	__m128 out = _mm_cvtepi32_ps(coords);
	out = _mm_mul_ps(out, scale);
	_mm_storeu_ps(pDest, out);
}

__forceinline void UShort3ToFloat3_SSSE3(const __m128i* pData, __m128 scale, float* pDest)
{
	__m128i coords = _mm_loadl_epi64(pData);
	coords = _mm_shuffle_epi8(coords, kMaskSwap16to32l_3);
	__m128 out = _mm_cvtepi32_ps(coords);
	out = _mm_mul_ps(out, scale);
	_mm_storeu_ps(pDest, out);
}

__forceinline void Short2ToFloat2_SSSE3(const u32* pData, __m128 scale, __m64* pDest)
{
	__m128i coords = _mm_cvtsi32_si128(*pData);
	coords = _mm_shuffle_epi8(coords, kMaskSwap16to32h_2);
	coords = _mm_srai_epi32(coords, 16);
	__m128 out = _mm_cvtepi32_ps(coords);
	out = _mm_mul_ps(out, scale);
	_mm_storel_pi(pDest, out);
}

__forceinline void Short2ToFloat3_SSSE3(const u32* pData, __m128 scale, float* pDest)
{
	__m128i coords = _mm_cvtsi32_si128(*pData);
	coords = _mm_shuffle_epi8(coords, kMaskSwap16to32h_2);
	coords = _mm_srai_epi32(coords, 16);
	__m128 out = _mm_cvtepi32_ps(coords);
	out = _mm_mul_ps(out, scale);
	_mm_storeu_ps(pDest, out);
}

__forceinline void Short3ToFloat3_SSSE3(const __m128i* pData, __m128 scale, float* pDest)
{
	__m128i coords = _mm_loadl_epi64(pData);
	coords = _mm_shuffle_epi8(coords, kMaskSwap16to32h_3);
	coords = _mm_srai_epi32(coords, 16);
	__m128 out = _mm_cvtepi32_ps(coords);
	out = _mm_mul_ps(out, scale);
	_mm_storeu_ps(pDest, out);
}

__forceinline void Float2ToFloat2sse3(__m128i *dst, const __m128i *src)
{
	__m128i a = _mm_loadl_epi64(src);
	a = _mm_shuffle_epi8(a, kMaskSwap32_2);
	_mm_storel_epi64(dst, a);
}

__forceinline void Float2ToFloat3sse3(__m128i* dst, const __m128i* src)
{
	__m128i a = _mm_loadu_si128(src);
	a = _mm_shuffle_epi8(a, kMaskSwap32_2);
	_mm_storeu_si128(dst, a);
}

__forceinline void Float3ToFloat3sse3(__m128i* dst, const __m128i* src)
{
	__m128i a = _mm_loadu_si128(src);
	a = _mm_shuffle_epi8(a, kMaskSwap32_3);
	_mm_storeu_si128(dst, a);
}

__forceinline void Float4ToFloat4sse3(__m128i* dst, const __m128i* src)
{
	__m128i a = _mm_loadu_si128(src);
	a = _mm_shuffle_epi8(a, kMaskSwap32_4);
	_mm_storeu_si128(dst, a);
}

#endif