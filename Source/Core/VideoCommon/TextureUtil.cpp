// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include "Common/CPUDetect.h"
#include "Common/Intrinsics.h"
#include "VideoCommon/TextureUtil.h"
#include "VideoCommon/LookUpTables.h"

namespace TextureUtil
{
#ifdef _WIN32
void ConvertRGBA_BGRA_SSE2(u32 *dst, const s32 dstPitch, u32 *pIn, const s32 width, const s32 height, const s32 pitch)
{
	// Converts RGBA to BGRA:
	// TODO: this would be totally unnecessary if we just change the TextureDecoder_RGBA to decode
	// to BGRA instead.
	for (s32 y = 0; y < height; y++, pIn += pitch)
	{
		u8 *pIn8 = (u8 *)pIn;
		u8 *pBits = (u8 *)((u8*)dst + (y * dstPitch));

		// Batch up loads/stores into 16 byte chunks to use SSE2 efficiently:
		s32 sse2blocks = (width * 4) / 16;
		s32 sse2remainder = (width * 4) & 15;

		// Do conversions in batches of 16 bytes:
		if (sse2blocks > 0)
		{
			// Generate a constant of all FF bytes:
			const __m128i allFFs128 = _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128());
			__m128i *src128 = (__m128i *)pIn8;
			__m128i *dst128 = (__m128i *)pBits;

			// Increment by 16 bytes at a time:
			for (int i = 0; i < sse2blocks; ++i, ++dst128, ++src128)
			{
				// Load up 4 colors simultaneously:
				__m128i rgba = _mm_loadu_si128(src128);
				// Swap the R and B components:
				// Isolate the B component and shift it left 16 bits:
				// ABGR
				const __m128i bMask = _mm_srli_epi32(allFFs128, 24);
				const __m128i bNew = _mm_slli_epi32(_mm_and_si128(rgba, bMask), 16);
				// Isolate the R component and shift it right 16 bits:
				const __m128i rMask = _mm_slli_epi32(bMask, 16);
				const __m128i rNew = _mm_srli_epi32(_mm_and_si128(rgba, rMask), 16);
				// Now mask off the old R and B components from the rgba data to get 0g0a:
				const __m128i _g_a = _mm_or_si128(
					_mm_and_si128(
						rgba,
						_mm_or_si128(
							_mm_slli_epi32(bMask, 8),
							_mm_slli_epi32(rMask, 8)
						)
					),
					_mm_or_si128(rNew, bNew)
				);
				// Finally, OR up all the individual components to get BGRA:
				const __m128i bgra = _mm_or_si128(_g_a, _mm_or_si128(rNew, bNew));
				_mm_storeu_si128(dst128, bgra);
			}
		}

		// Take the remainder colors at the end of the row that weren't able to
		// be included into the last 16 byte chunk:
		if (sse2remainder > 0)
		{
			for (s32 x = (sse2blocks * 16); x < (width * 4); x += 4)
			{
				pBits[x + 0] = pIn8[x + 2];
				pBits[x + 1] = pIn8[x + 1];
				pBits[x + 2] = pIn8[x + 0];
				pBits[x + 3] = pIn8[x + 3];
			}
		}
	}

	// Memory fence to make sure the stores are good:
	_mm_mfence();
}

void ConvertRGBA_BGRA_SSSE3(u32 *dst, const s32 dstPitch, u32 *pIn, const s32 width, const s32 height, const s32 pitch)
{
	__m128i mask = _mm_set_epi8(15, 12, 13, 14, 11, 8, 9, 10, 7, 4, 5, 6, 3, 0, 1, 2);
	for (s32 y = 0; y < height; y++, pIn += pitch)
	{
		u8 *pIn8 = (u8 *)pIn;
		u8 *pBits = (u8 *)((u8*)dst + (y * dstPitch));

		// Batch up loads/stores into 16 byte chunks to use SSE2 efficiently:
		int ssse3blocks = (width * 4) / 16;
		int ssse3remainder = (width * 4) & 15;

		// Do conversions in batches of 16 bytes:
		if (ssse3blocks > 0)
		{
			__m128i *src128 = (__m128i *)pIn8;
			__m128i *dst128 = (__m128i *)pBits;

			// Increment by 16 bytes at a time:
			for (s32 i = 0; i < ssse3blocks; ++i, ++dst128, ++src128)
			{
				_mm_storeu_si128(dst128, _mm_shuffle_epi8(_mm_loadu_si128(src128), mask));
			}
		}

		// Take the remainder colors at the end of the row that weren't able to
		// be included into the last 16 byte chunk:
		if (ssse3remainder > 0)
		{
			for (s32 x = (ssse3blocks * 16); x < (width * 4); x += 4)
			{
				pBits[x + 0] = pIn8[x + 2];
				pBits[x + 1] = pIn8[x + 1];
				pBits[x + 2] = pIn8[x + 0];
				pBits[x + 3] = pIn8[x + 3];
			}
		}
	}

	// Memory fence to make sure the stores are good:
	_mm_mfence();
}

void ConvertRGBA_BGRA(u32 *dst, const s32 dstPitch, u32 *pIn, const s32 width, const s32 height, const s32 pitch)
{
#if _M_SSE >= 0x301
	// Uses SSSE3 intrinsics to optimize RGBA -> BGRA swizzle:
	if (cpu_info.bSSSE3)
	{
		TextureUtil::ConvertRGBA_BGRA_SSSE3(dst, dstPitch, pIn, width, height, pitch);
	}
	else
#endif
		// Uses SSE2 intrinsics to optimize RGBA -> BGRA swizzle:
	{
		TextureUtil::ConvertRGBA_BGRA_SSE2(dst, dstPitch, pIn, width, height, pitch);
	}
}


void ConvertRGBA565_BGRA(u32 *dst, const s32 dstPitch, u16 *pIn, const s32 width, const s32 height, const s32 pitch)
{
	u16 *currentsrc = pIn;
	u32 *currentdst = dst;
	for (s32 i = 0; i < height; i++)
	{
		for (s32 j = 0; j < width; j++)
		{
			u16 val = currentsrc[j];
			u32 output = Convert5To8((val) & 0x1f);//blue
			output |= (Convert6To8((val >> 5) & 0x3f) << 8);//breen
			output |= (Convert5To8((val >> 11) & 0x1f) << 16);//red
			output |= (0xFF << 24);//alpha
			currentdst[j] = output;
		}
		currentdst += dstPitch;
		currentsrc += pitch;
	}
}

void ConvertRGBA565_RGBA(u32 *dst, const s32 dstPitch, u16 *pIn, const s32 width, const s32 height, const s32 pitch)
{
	u16 *currentsrc = pIn;
	u32 *currentdst = dst;
	for (s32 i = 0; i < height; i++)
	{
		for (s32 j = 0; j < width; j++)
		{
			u16 val = currentsrc[j];
			u32 output = Convert5To8((val >> 11) & 0x1f);//red
			output |= (Convert6To8((val >> 5) & 0x3f) << 8);//green
			output |= (Convert5To8((val) & 0x1f) << 16);//blue
			output |= (0xFF << 24);//alpha
			currentdst[j] = output;
		}
		currentdst += dstPitch;
		currentsrc += pitch;
	}
}
#endif
void CopyTextureData(u8 *pDst, const u8 *pSrc, const s32 width, const s32 height, const s32 srcpitch, const s32 dstpitch, const s32 pixelsize)
{
	const s32 rowsize = width * pixelsize;
	if (srcpitch == dstpitch && srcpitch == rowsize)
	{
		memcpy(pDst, pSrc, rowsize * height);
	}
	else
	{
		for (int y = 0; y < height; y++)
		{
			memcpy(pDst, pSrc, rowsize);
			pSrc += srcpitch;
			pDst += dstpitch;
		}
	}
}

void ExpandI8Data(u8 *pDst, const u8 *pSrc, const s32 width, const s32 height, const s32 srcpitch, const s32 dstpitch)
{
	for (int y = 0; y < height; y++)
	{
		for (int i = 0; i < width * 2; i += 2)
		{
			u8 data = pSrc[i >> 1];
			pDst[i] = data;
			pDst[i + 1] = data;
		}
		pSrc += srcpitch;
		pDst += dstpitch;
	}
}

void CopyCompressedTextureData(u8 *pDst, const u8 *pSrc, const s32 width, const s32 height, const s32 dstPitch, s32 numBytesPerBlock, const s32 dstpitch)
{
	s32 numBlocksStride = (dstPitch + 3) >> 2;
	s32 numBlocksWide = (width + 3) >> 2;
	s32 numBlocksHigh = (height + 3) >> 2;
	s32 stridebytes = numBlocksStride * numBytesPerBlock;
	s32 rowBytes = numBlocksWide * numBytesPerBlock;
	s32 numRows = numBlocksHigh;
	if (rowBytes == dstpitch && rowBytes == stridebytes)
	{
		memcpy(pDst, pSrc, rowBytes * numRows);
	}
	else
	{
		u8* pDestBits = pDst;
		const u8* pSrcBits = pSrc;
		// Copy stride line by line   
		for (s32 h = 0; h < numRows; h++)
		{
			memcpy(pDestBits, pSrcBits, rowBytes);
			pDestBits += dstpitch;
			pSrcBits += stridebytes;
		}
	}
}

s32 GetTextureSizeInBytes(u32 width, u32 height, PC_TexFormat fmt)
{
	static const s32 formatSize[11]
	{
		0,//PC_TEX_FMT_NONE
		4,//PC_TEX_FMT_BGRA32
		4,//PC_TEX_FMT_RGBA32
		1,//PC_TEX_FMT_I4_AS_I8 A hack which means the format is a packed 8-bit intensity texture. It is unpacked to A8L8 in D3DTexture.cpp
		2,//PC_TEX_FMT_IA4_AS_IA8
		1,//PC_TEX_FMT_I8
		2,//PC_TEX_FMT_IA8
		2,//PC_TEX_FMT_RGB565
		8,//PC_TEX_FMT_DXT1
		16,//PC_TEX_FMT_DXT3
		16,//PC_TEX_FMT_DXT5
	};
	if (fmt == PC_TEX_FMT_DXT1 || fmt == PC_TEX_FMT_DXT3 || fmt == PC_TEX_FMT_DXT5)
	{
		width = (width + 3) >> 2;
		height = (height + 3) >> 2;
	}
	return width * height * formatSize[fmt];
}

u32 CalculateLevelSize(u32 level_0_size, u32 level)
{
	return std::max(level_0_size >> level, 1u);
}
}
