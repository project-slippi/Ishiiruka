// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Added for Ishiiruka by Tino
#pragma once
#include "Common/CPUDetect.h"
#include "VideoCommon/VertexLoader_ColorFuncs.h"
#include "VideoCommon/VertexLoader_NormalFuncs.h"
#include "VideoCommon/VertexLoader_PositionFuncs.h"
#include "VideoCommon/VertexLoader_TextCoordFuncs.h"
#include "VideoCommon/BoundingBox.h"

enum EMtxMask
{
	maskTex0MatIdx = 0x1u,
	maskTex1MatIdx = 0x2u,
	maskTex2MatIdx = 0x4u,
	maskTex3MatIdx = 0x8u,
	maskTex4MatIdx = 0x10u,
	maskTex5MatIdx = 0x20u,
	maskTex6MatIdx = 0x40u,
	maskTex7MatIdx = 0x80u
};

enum EMtxComplements
{
	compTex0MatIdx = 0xFEu,
	compTex1MatIdx = 0xFCu,
	compTex2MatIdx = 0xF8u,
	compTex3MatIdx = 0xF0u,
	compTex4MatIdx = 0xE0u,
	compTex5MatIdx = 0xC0u,
	compTex6MatIdx = 0x80u
};

enum EVTXmask
{
	maskPosition = 0x300u,
	maskNormal = 0xC00u,
	maskColor0 = 0x3000u,
	maskColor1 = 0xC000u,
	maskTex0Coord = 0x30000u,
	maskTex1Coord = 0xC0000u,
	maskTex2Coord = 0x300000u,
	maskTex3Coord = 0xC00000u,
	maskTex4Coord = 0x3000000u,
	maskTex5Coord = 0xC000000u,
	maskTex6Coord = 0x30000000u,
	maskTex7Coord = 0xC0000000u
};

enum EVTXStride
{
	stridePosition = 8,
	strideNormal = 10,
	strideColor0 = 12,
	strideColor1 = 14,
	strideTex0Coord = 16,
	strideTex1Coord = 18,
	strideTex2Coord = 20,
	strideTex3Coord = 22,
	strideTex4Coord = 24,
	strideTex5Coord = 26,
	strideTex6Coord = 28,
	strideTex7Coord = 30
};

enum EVTXTexComplements
{
	compTex0Coord = 0xFFFC0000u,
	compTex1Coord = 0xFFF00000u,
	compTex2Coord = 0xFFC00000u,
	compTex3Coord = 0xFF000000u,
	compTex4Coord = 0xFC000000u,
	compTex5Coord = 0xF0000000u,
	compTex6Coord = 0xC0000000u,
};

enum EVAT0Mask
{
	maskPosElements = 0x1,
	maskPosFormat = 0xE,
	maskNormalElements = 0x200,
	maskNormalFormat = 0x1C00,
	maskColor0Elements = 0x2000,
	maskColor0Comp = 0x1C000,
	maskColor1Elements = 0x20000,
	maskColor1Comp = 0x1C0000,
	maskTex0CoordElements = 0x200000,
	maskTex0CoordFormat = 0x1C00000,
	maskNormalIndex3 = 0x80000000
};

enum EVAT0Stride
{
	stridePosFormat = 1,
	strideNormalElements = 9,
	strideNormalFormat = 10,
	strideColor0Elements = 13,
	strideColor0Comp = 14,
	strideColor1Elements = 17,
	strideColor1Comp = 18,
	strideTex0CoordElements = 21,
	strideTex0CoordFormat = 22,
	strideNormalIndex3 = 31
};

enum EVAT1Mask
{
	maskTex1CoordElements = 0x1u,
	maskTex1CoordFormat = 0xEu,
	maskTex2CoordElements = 0x200u,
	maskTex2CoordFormat = 0x1C00u,
	maskTex3CoordElements = 0x40000u,
	maskTex3CoordFormat = 0x380000u,
	maskTex4CoordElements = 0x8000000u,
	maskTex4CoordFormat = 0x70000000u,
	maskPosMatIdx = 0x80000000u
};

enum EVAT1Stride
{
	strideTex1CoordFormat = 1,
	strideTex2CoordElements = 9,
	strideTex2CoordFormat = 10,
	strideTex3CoordElements = 18,
	strideTex3CoordFormat = 19,
	strideTex4CoordElements = 27,
	strideTex4CoordFormat = 28,
	stridePosMatIdx = 31
};

enum EVAT2Mask
{
	maskTex5CoordElements = 0x20,
	maskTex5CoordFormat = 0x1C0,
	maskTex6CoordElements = 0x4000,
	maskTex6CoordFormat = 0x38000,
	maskTex7CoordElements = 0x800000,
	maskTex7CoordFormat = 0x7000000
};

enum EVAT2Stride
{
	strideTex5CoordElements = 5,
	strideTex5CoordFormat = 6,
	strideTex6CoordElements = 14,
	strideTex6CoordFormat = 15,
	strideTex7CoordElements = 23,
	strideTex7CoordFormat = 24
};

template<u32 iSSE, EVTXComponentFormat _format, s32 N>
__forceinline void PosFunction_DIRECT(TPipelineState &pipelinestate)
{
	switch (_format)
	{
	case FORMAT_UBYTE:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			if (N == 2)
			{
				_Pos_ReadDirect_UByte_SSSE3<false>(pipelinestate);
			}
			else
			{
				_Pos_ReadDirect_UByte_SSSE3<true>(pipelinestate);
			}
		}
		else
#endif
		{
			_Pos_ReadDirect<u8, N>(pipelinestate);
		}
		break;
	case FORMAT_BYTE:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			if (N == 2)
			{
				_Pos_ReadDirect_SByte_SSSE3<false>(pipelinestate);
			}
			else
			{
				_Pos_ReadDirect_SByte_SSSE3<true>(pipelinestate);
			}
		}
		else
#endif
		{
			_Pos_ReadDirect<s8, N>(pipelinestate);
		}
		break;
	case FORMAT_USHORT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			if (N == 2)
			{
				_Pos_ReadDirect_UShort_SSSE3<false>(pipelinestate);
			}
			else
			{
				_Pos_ReadDirect_UShort_SSSE3<true>(pipelinestate);
			}
		}
		else
#endif
		{
			_Pos_ReadDirect<u16, N>(pipelinestate);
		}
		break;
	case FORMAT_SHORT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			if (N == 2)
			{
				_Pos_ReadDirect_Short_SSSE3<false>(pipelinestate);
			}
			else
			{
				_Pos_ReadDirect_Short_SSSE3<true>(pipelinestate);
			}
		}
		else
#endif
		{
			_Pos_ReadDirect<s16, N>(pipelinestate);
		}
		break;
	case FORMAT_FLOAT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			if (N == 2)
			{
				_Pos_ReadDirect_Float_SSSE3<false>(pipelinestate);
			}
			else
			{
				_Pos_ReadDirect_Float_SSSE3<true>(pipelinestate);
			}
		}
		else
#endif
		{
			_Pos_ReadDirect<float, N>(pipelinestate);
		}
		break;
	default:
		break;
	}
}

template<u32 iSSE, EVTXComponentFormat _format, typename I, s32 N>
__forceinline void PosFunction_Index(TPipelineState &pipelinestate)
{
	switch (_format)
	{
	case FORMAT_UBYTE:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			if (N == 2)
			{
				_Pos_ReadIndex_UByte_SSSE3<I, false>(pipelinestate);
			}
			else
			{
				_Pos_ReadIndex_UByte_SSSE3<I, true>(pipelinestate);
			}
		}
		else
#endif
		{
			_Pos_ReadIndex<I, u8, N>(pipelinestate);
		}
		break;
	case FORMAT_BYTE:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			if (N == 2)
			{
				_Pos_ReadIndex_SByte_SSSE3<I, false>(pipelinestate);
			}
			else
			{
				_Pos_ReadIndex_SByte_SSSE3<I, true>(pipelinestate);
			}
		}
		else
#endif
		{
			_Pos_ReadIndex<I, s8, N>(pipelinestate);
		}
		break;
	case FORMAT_USHORT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			if (N == 2)
			{
				_Pos_ReadIndex_UShort_SSSE3<I, false>(pipelinestate);
			}
			else
			{
				_Pos_ReadIndex_UShort_SSSE3<I, true>(pipelinestate);
			}
		}
		else
#endif
		{
			_Pos_ReadIndex<I, u16, N>(pipelinestate);
		}
		break;
	case FORMAT_SHORT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			if (N == 2)
			{
				_Pos_ReadIndex_Short_SSSE3<I, false>(pipelinestate);
			}
			else
			{
				_Pos_ReadIndex_Short_SSSE3<I, true>(pipelinestate);
			}
		}
		else
#endif
		{
			_Pos_ReadIndex<I, s16, N>(pipelinestate);
		}
		break;
	case FORMAT_FLOAT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			if (N == 2)
			{
				_Pos_ReadIndex_Float_SSSE3<I, false>(pipelinestate);
			}
			else
			{
				_Pos_ReadIndex_Float_SSSE3<I, true>(pipelinestate);
			}
		}
		else
#endif
		{
			_Pos_ReadIndex<I, float, N>(pipelinestate);
		}
		break;
	default:
		break;
	}
}

template<u32 iSSE, EVTXComponentType _type, EVTXComponentFormat _format, EPosElements _elements>
__forceinline void PosFunction(TPipelineState &pipelinestate)
{
	if (_type == DIRECT)
	{
		PosFunction_DIRECT<iSSE, _format, 2 + _elements>(pipelinestate);
	}
	else if (_type == INDEX8)
	{
		PosFunction_Index<iSSE, _format, u8, 2 + _elements>(pipelinestate);
	}
	else if (_type == INDEX16)
	{
		PosFunction_Index<iSSE, _format, u16, 2 + _elements>(pipelinestate);
	}
}

template<u32 iSSE, EVTXComponentFormat _format, s32 N>
__forceinline void NormalFunction_DIRECT(TPipelineState &pipelinestate)
{
	switch (_format)
	{
	case FORMAT_UBYTE:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			_Normal_Direct_UByte_SSSE3<N>(pipelinestate);
		}
		else
#endif
		{
			_Normal_Direct<u8, N>(pipelinestate);
		}
		break;
	case FORMAT_BYTE:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			_Normal_Direct_SByte_SSSE3<N>(pipelinestate);
		}
		else
#endif
		{
			_Normal_Direct<s8, N>(pipelinestate);
		}
		break;
	case FORMAT_USHORT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			_Normal_Direct_UShort_SSSE3<N>(pipelinestate);
		}
		else
#endif
		{
			_Normal_Direct<u16, N>(pipelinestate);
		}
		break;
	case FORMAT_SHORT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			_Normal_Direct_Short_SSSE3<N>(pipelinestate);
		}
		else
#endif
		{
			_Normal_Direct<s16, N>(pipelinestate);
		}
		break;
	case FORMAT_FLOAT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			_Normal_Direct_FLOAT_SSSE3<N>(pipelinestate);
		}
		else
#endif
		{
			_Normal_Direct<float, N>(pipelinestate);
		}
		break;
	default:
		break;
	}
}

template<u32 iSSE, EVTXComponentFormat _format, typename I, s32 N>
__forceinline void NormalFunction_Index(TPipelineState &pipelinestate)
{
	switch (_format)
	{
	case FORMAT_UBYTE:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			_Normal_Index_UByte_SSSE3<I, N>(pipelinestate);
		}
		else
#endif
		{
			_Normal_Index_Offset<I, u8, N, 0>(pipelinestate);
		}
		break;
	case FORMAT_BYTE:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			_Normal_Index_SByte_SSSE3<I, N>(pipelinestate);
		}
		else
#endif
		{
			_Normal_Index_Offset<I, s8, N, 0>(pipelinestate);
		}
		break;
	case FORMAT_USHORT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			_Normal_Index_UShort_SSSE3<I, N>(pipelinestate);
		}
		else
#endif
		{
			_Normal_Index_Offset<I, u16, N, 0>(pipelinestate);
		}
		break;
	case FORMAT_SHORT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			_Normal_Index_Short_SSSE3<I, N>(pipelinestate);
		}
		else
#endif
		{
			_Normal_Index_Offset<I, s16, N, 0>(pipelinestate);
		}
		break;
	case FORMAT_FLOAT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			_Normal_Index_FLOAT_SSSE3<I, N>(pipelinestate);
		}
		else
#endif
		{
			_Normal_Index_Offset<I, float, N, 0>(pipelinestate);
		}
		break;
	default:
		break;
	}
}

template<u32 iSSE, EVTXComponentFormat _format, typename I>
__forceinline void NormalFunction_Index3(TPipelineState &pipelinestate)
{
	switch (_format)
	{
	case FORMAT_UBYTE:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			_Normal_Index3_UByte_SSSE3<I>(pipelinestate);
		}
		else
#endif
		{
			_Normal_Index_Offset3<I, u8>(pipelinestate);
		}
		break;
	case FORMAT_BYTE:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			_Normal_Index3_SByte_SSSE3<I>(pipelinestate);
		}
		else
#endif
		{
			_Normal_Index_Offset3<I, s8>(pipelinestate);
		}
		break;
	case FORMAT_USHORT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			_Normal_Index3_UShort_SSSE3<I>(pipelinestate);
		}
		else
#endif
		{
			_Normal_Index_Offset3<I, u16>(pipelinestate);
		}
		break;
	case FORMAT_SHORT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			_Normal_Index3_Short_SSSE3<I>(pipelinestate);
		}
		else
#endif
		{
			_Normal_Index_Offset3<I, s16>(pipelinestate);
		}
		break;
	case FORMAT_FLOAT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301)
		{
			_Normal_Index3_FLOAT_SSSE3<I>(pipelinestate);
		}
		else
#endif
		{
			_Normal_Index_Offset3<I, float>(pipelinestate);
		}
		break;
	default:
		break;
	}
}

template<u32 iSSE, EVTXComponentType _type, ENormalIndices _index3, ENormalElements _elements, EVTXComponentFormat _format>
__forceinline void NormalFunction(TPipelineState &pipelinestate)
{
	if (_type == DIRECT)
	{
		NormalFunction_DIRECT<iSSE, _format, 1 + 2 * _elements>(pipelinestate);
	}
	else if (_type == INDEX8)
	{
		if (_elements == NRM_NBT || _index3 == NRM_INDICES1)
		{
			NormalFunction_Index<iSSE, _format, u8, 1 + 2 * _elements>(pipelinestate);
		}
		else
		{
			NormalFunction_Index3<iSSE, _format, u8>(pipelinestate);
		}
	}
	else if (_type == INDEX16)
	{
		if (_elements == NRM_NBT || _index3 == NRM_INDICES1)
		{
			NormalFunction_Index<iSSE, _format, u16, 1 + 2 * _elements>(pipelinestate);
		}
		else
		{
			NormalFunction_Index3<iSSE, _format, u16>(pipelinestate);
		}
	}
}

template <EVTXComponentType _type, EVTXColorFormat _format>
__forceinline void ColorFunction(TPipelineState &pipelinestate)
{
	switch (_type)
	{
	case DIRECT:
		switch (_format)
		{
		case FORMAT_16B_565:	_Color_ReadDirect_16b_565(pipelinestate); break;
		case FORMAT_24B_888:	_Color_ReadDirect_24b_888(pipelinestate); break;
		case FORMAT_32B_888x:	_Color_ReadDirect_32b_888x(pipelinestate); break;
		case FORMAT_16B_4444:	_Color_ReadDirect_16b_4444(pipelinestate); break;
		case FORMAT_24B_6666:	_Color_ReadDirect_24b_6666(pipelinestate); break;
		case FORMAT_32B_8888:	_Color_ReadDirect_32b_8888(pipelinestate); break;
		default: break;
		}
		break;
	case INDEX8:
		switch (_format)
		{
		case FORMAT_16B_565:	Color_ReadIndex_16b_565<u8>(pipelinestate); break;
		case FORMAT_24B_888:	Color_ReadIndex_24b_888<u8>(pipelinestate); break;
		case FORMAT_32B_888x:	Color_ReadIndex_32b_888x<u8>(pipelinestate); break;
		case FORMAT_16B_4444:	Color_ReadIndex_16b_4444<u8>(pipelinestate); break;
		case FORMAT_24B_6666:	Color_ReadIndex_24b_6666<u8>(pipelinestate); break;
		case FORMAT_32B_8888:	Color_ReadIndex_32b_8888<u8>(pipelinestate); break;
		default: break;
		}
		break;
	case INDEX16:
		switch (_format)
		{
		case FORMAT_16B_565:	Color_ReadIndex_16b_565<u16>(pipelinestate); break;
		case FORMAT_24B_888:	Color_ReadIndex_24b_888<u16>(pipelinestate); break;
		case FORMAT_32B_888x:	Color_ReadIndex_32b_888x<u16>(pipelinestate); break;
		case FORMAT_16B_4444:	Color_ReadIndex_16b_4444<u16>(pipelinestate); break;
		case FORMAT_24B_6666:	Color_ReadIndex_24b_6666<u16>(pipelinestate); break;
		case FORMAT_32B_8888:	Color_ReadIndex_32b_8888<u16>(pipelinestate); break;
		default: break;
		}
		break;
	default:
		break;
	}
}

template<u32 iSSE, EVTXComponentFormat _format, s32 N>
__forceinline void TexCoordFunction_DIRECT(TPipelineState &pipelinestate)
{
	switch (_format)
	{
	case FORMAT_UBYTE:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301 && N == 2)
		{
			_TexCoord_ReadDirect_UByte2_SSSE3(pipelinestate);
		}
		else
#endif
		{
			_TexCoord_ReadDirect<u8, N>(pipelinestate);
		}
		break;
	case FORMAT_BYTE:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301 && N == 2)
		{
			_TexCoord_ReadDirect_SByte2_SSSE3(pipelinestate);
		}
		else
#endif
		{
			_TexCoord_ReadDirect<s8, N>(pipelinestate);
		}
		break;
	case FORMAT_USHORT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301 && N == 2)
		{
			_TexCoord_ReadDirect_UShort2_SSSE3(pipelinestate);
		}
		else
#endif
		{
			_TexCoord_ReadDirect<u16, N>(pipelinestate);
		}
		break;
	case FORMAT_SHORT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301 && N == 2)
		{
			_TexCoord_ReadDirect_Short2_SSSE3(pipelinestate);
		}
		else
#endif
		{
			_TexCoord_ReadDirect<s16, N>(pipelinestate);
		}
		break;
	case FORMAT_FLOAT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301 && N == 2)
		{
			_TexCoord_ReadDirect_Float2_SSSE3(pipelinestate);
		}
		else
#endif
		{
			_TexCoord_ReadDirect<float, N>(pipelinestate);
		}
		break;
	default:
		break;
	}
}

template<u32 iSSE, EVTXComponentFormat _format, typename I, u32 N>
__forceinline void TexCoordFunction_Index(TPipelineState &pipelinestate)
{
	switch (_format)
	{
	case FORMAT_UBYTE:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301 && N == 2)
		{
			_TexCoord_ReadIndex_UByte2_SSSE3<I>(pipelinestate);
		}
		else
#endif
		{
			_TexCoord_ReadIndex<I, u8, N>(pipelinestate);
		}
		break;
	case FORMAT_BYTE:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301 && N == 2)
		{
			_TexCoord_ReadIndex_SByte2_SSSE3<I>(pipelinestate);
		}
		else
#endif
		{
			_TexCoord_ReadIndex<I, s8, N>(pipelinestate);
		}
		break;
	case FORMAT_USHORT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301 && N == 2)
		{
			_TexCoord_ReadIndex_UShort2_SSSE3<I>(pipelinestate);
		}
		else
#endif
		{
			_TexCoord_ReadIndex<I, u16, N>(pipelinestate);
		}
		break;
	case FORMAT_SHORT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301 && N == 2)
		{
			_TexCoord_ReadIndex_Short2_SSSE3<I>(pipelinestate);
		}
		else
#endif
		{
			_TexCoord_ReadIndex<I, s16, N>(pipelinestate);
		}
		break;
	case FORMAT_FLOAT:
#if _M_SSE >= 0x301
		if (iSSE >= 0x301 && N == 2)
		{
			_TexCoord_ReadIndex_Float2_SSSE3<I>(pipelinestate);
		}
		else
#endif
		{
			_TexCoord_ReadIndex<I, float, N>(pipelinestate);
		}
		break;
	default:
		break;
	}
}

template<u32 iSSE, EVTXComponentType _type, EVTXComponentFormat _format, ETcoordElements _elements, bool mtxidx>
__forceinline void TexCoordElement(TPipelineState &pipelinestate)
{
	if (_type != NOT_PRESENT)
	{
		if (_type == DIRECT)
		{
			TexCoordFunction_DIRECT<iSSE, _format, 1 + _elements>(pipelinestate);
		}
		else if (_type == INDEX8)
		{
			TexCoordFunction_Index<iSSE, _format, u8, 1 + _elements>(pipelinestate);
		}
		else if (_type == INDEX16)
		{
			TexCoordFunction_Index<iSSE, _format, u16, 1 + _elements>(pipelinestate);
		}
	}
	if (mtxidx)
	{
		if (_type != NOT_PRESENT)
		{
			if (_elements == TC_ELEMENTS_2)
			{
				pipelinestate.Write(float(pipelinestate.curtexmtx[pipelinestate.texmtxwrite++]));
			}
			else
			{
				pipelinestate.Write(0.f);
				pipelinestate.Write(float(pipelinestate.curtexmtx[pipelinestate.texmtxwrite++]));
			}
		}
		else
		{
#if _M_SSE >= 0x200
			if (iSSE >= 0x200)
			{
				__m128 output = _mm_cvtsi32_ss(_mm_castsi128_ps(_mm_setzero_si128()), pipelinestate.curtexmtx[pipelinestate.texmtxwrite++]);
				_mm_storeu_ps((float*)pipelinestate.GetWritePosition(), _mm_shuffle_ps(output, output, 0x45));
				pipelinestate.WriteSkip(sizeof(float) * 3);
			}
			else
#endif
			{
				pipelinestate.Write(0.f);
				pipelinestate.Write(0.f);
				pipelinestate.Write(float(pipelinestate.curtexmtx[pipelinestate.texmtxwrite++]));
			}
		}
	}
}

template <int iSSE, u32 VtxDesc, u32 VAT0, u32 VAT1, u32 VAT2>
void TemplatedLoader(TPipelineState& pipelinestate)
{
	u32 loopcount = pipelinestate.count;
	while (loopcount)
	{
		if (VtxDesc & 0x200)
		{
			pipelinestate.flags &= ~TPS_SKIP_VERTEX;
		}
		pipelinestate.tcIndex = 0;
		pipelinestate.colIndex = 0;
		pipelinestate.texmtxwrite = pipelinestate.texmtxread = 0;
		if (VAT1 & maskPosMatIdx)
		{
			pipelinestate.curposmtx = pipelinestate.Read<u8>() & 0x3f;
		}
		if (VtxDesc & maskTex0MatIdx)
		{
			pipelinestate.curtexmtx[pipelinestate.texmtxread++] = pipelinestate.Read<u8>() & 0x3f;
		}
		if (VtxDesc & maskTex1MatIdx)
		{
			pipelinestate.curtexmtx[pipelinestate.texmtxread++] = pipelinestate.Read<u8>() & 0x3f;
		}
		if (VtxDesc & maskTex2MatIdx)
		{
			pipelinestate.curtexmtx[pipelinestate.texmtxread++] = pipelinestate.Read<u8>() & 0x3f;
		}
		if (VtxDesc & maskTex3MatIdx)
		{
			pipelinestate.curtexmtx[pipelinestate.texmtxread++] = pipelinestate.Read<u8>() & 0x3f;
		}
		if (VtxDesc & maskTex4MatIdx)
		{
			pipelinestate.curtexmtx[pipelinestate.texmtxread++] = pipelinestate.Read<u8>() & 0x3f;
		}
		if (VtxDesc & maskTex5MatIdx)
		{
			pipelinestate.curtexmtx[pipelinestate.texmtxread++] = pipelinestate.Read<u8>() & 0x3f;
		}
		if (VtxDesc & maskTex6MatIdx)
		{
			pipelinestate.curtexmtx[pipelinestate.texmtxread++] = pipelinestate.Read<u8>() & 0x3f;
		}
		if (VtxDesc & maskTex7MatIdx)
		{
			pipelinestate.curtexmtx[pipelinestate.texmtxread++] = pipelinestate.Read<u8>() & 0x3f;
		}

		// Write vertex position loader
		if (pipelinestate.flags & TPS_USE_BBOX)
		{
			BoundingBox::SetVertexBufferPosition(pipelinestate);
		}

		PosFunction<iSSE,
			static_cast<EVTXComponentType>((VtxDesc & maskPosition) >> stridePosition),
			static_cast<EVTXComponentFormat>((VAT0 & maskPosFormat) >> stridePosFormat),
			static_cast<EPosElements>(VAT0 & maskPosElements)>(pipelinestate);

		// Normals
		if (VtxDesc & maskNormal)
		{
			NormalFunction<iSSE,
				static_cast<EVTXComponentType>((VtxDesc & maskNormal) >> strideNormal),
				static_cast<ENormalIndices>((VAT0 & maskNormalIndex3) >> strideNormalIndex3),
				static_cast<ENormalElements>((VAT0 & maskNormalElements) >> strideNormalElements),
				static_cast<EVTXComponentFormat>((VAT0 & maskNormalFormat) >> strideNormalFormat)>(pipelinestate);
		}

		// Colors
		if (VtxDesc & maskColor0)
		{
			ColorFunction<static_cast<EVTXComponentType>((VtxDesc & maskColor0) >> strideColor0),
				static_cast<EVTXColorFormat>((VAT0 & maskColor0Comp) >> strideColor0Comp)>(pipelinestate);
		}
		if (VtxDesc & maskColor1)
		{
			ColorFunction<static_cast<EVTXComponentType>((VtxDesc & maskColor1) >> strideColor1),
				static_cast<EVTXColorFormat>((VAT0 & maskColor1Comp) >> strideColor1Comp)>(pipelinestate);
		}

		// Texture Corrds
		if ((VtxDesc & maskTex0MatIdx) || (VtxDesc & maskTex0Coord))
		{
			TexCoordElement<iSSE,
				static_cast<EVTXComponentType>((VtxDesc & maskTex0Coord) >> strideTex0Coord),
				static_cast<EVTXComponentFormat>((VAT0 & maskTex0CoordFormat) >> strideTex0CoordFormat),
				static_cast<ETcoordElements>((VAT0 & maskTex0CoordElements) >> strideTex0CoordElements),
				static_cast<bool>((VtxDesc & maskTex0MatIdx) == maskTex0MatIdx)>(pipelinestate);
		}
		else if ((VtxDesc & compTex0MatIdx) || (VtxDesc & compTex0Coord))
		{
			pipelinestate.tcIndex++;
		}
		if ((VtxDesc & maskTex1MatIdx) || (VtxDesc & maskTex1Coord))
		{
			TexCoordElement<iSSE,
				static_cast<EVTXComponentType>((VtxDesc & maskTex1Coord) >> strideTex1Coord),
				static_cast<EVTXComponentFormat>((VAT1 & maskTex1CoordFormat) >> strideTex1CoordFormat),
				static_cast<ETcoordElements>(VAT1 & maskTex1CoordElements),
				static_cast<bool>((VtxDesc & maskTex1MatIdx) == maskTex1MatIdx)>(pipelinestate);
		}
		else if ((VtxDesc & compTex1MatIdx) || (VtxDesc & compTex1Coord))
		{
			pipelinestate.tcIndex++;
		}
		if ((VtxDesc & maskTex2MatIdx) || (VtxDesc & maskTex2Coord))
		{
			TexCoordElement<iSSE,
				static_cast<EVTXComponentType>((VtxDesc & maskTex2Coord) >> strideTex2Coord),
				static_cast<EVTXComponentFormat>((VAT1 & maskTex2CoordFormat) >> strideTex2CoordFormat),
				static_cast<ETcoordElements>((VAT1 & maskTex2CoordElements) >> strideTex2CoordElements),
				static_cast<bool>((VtxDesc & maskTex2MatIdx) == maskTex2MatIdx)>(pipelinestate);
		}
		else if ((VtxDesc & compTex2MatIdx) || (VtxDesc & compTex2Coord))
		{
			pipelinestate.tcIndex++;
		}
		if ((VtxDesc & maskTex3MatIdx) || (VtxDesc & maskTex3Coord))
		{
			TexCoordElement<iSSE,
				static_cast<EVTXComponentType>((VtxDesc & maskTex3Coord) >> strideTex3Coord),
				static_cast<EVTXComponentFormat>((VAT1 & maskTex3CoordFormat) >> strideTex3CoordFormat),
				static_cast<ETcoordElements>((VAT1 & maskTex3CoordElements) >> strideTex3CoordElements),
				static_cast<bool>((VtxDesc & maskTex3MatIdx) == maskTex3MatIdx)>(pipelinestate);
		}
		else if ((VtxDesc & compTex3MatIdx) != NOT_PRESENT
			|| (VtxDesc & compTex3Coord) != NOT_PRESENT)
		{
			pipelinestate.tcIndex++;
		}
		if ((VtxDesc & maskTex4MatIdx) || (VtxDesc & maskTex4Coord))
		{
			TexCoordElement<iSSE,
				static_cast<EVTXComponentType>((VtxDesc & maskTex4Coord) >> strideTex4Coord),
				static_cast<EVTXComponentFormat>((VAT1 & maskTex4CoordFormat) >> strideTex4CoordFormat),
				static_cast<ETcoordElements>((VAT1 & maskTex4CoordElements) >> strideTex4CoordElements),
				static_cast<bool>((VtxDesc & maskTex4MatIdx) == maskTex4MatIdx)>(pipelinestate);
		}
		else if ((VtxDesc & compTex4MatIdx) || (VtxDesc & compTex4Coord))
		{
			pipelinestate.tcIndex++;
		}
		if ((VtxDesc & maskTex5MatIdx) || (VtxDesc & maskTex5Coord))
		{
			TexCoordElement<iSSE,
				static_cast<EVTXComponentType>((VtxDesc & maskTex5Coord) >> strideTex5Coord),
				static_cast<EVTXComponentFormat>((VAT2 & maskTex5CoordFormat) >> strideTex5CoordFormat),
				static_cast<ETcoordElements>((VAT2 & maskTex5CoordElements) >> strideTex5CoordElements),
				static_cast<bool>((VtxDesc & maskTex5MatIdx) == maskTex5MatIdx)>(pipelinestate);
		}
		else if ((VtxDesc & compTex5MatIdx) || (VtxDesc & compTex5Coord))
		{
			pipelinestate.tcIndex++;
		}
		if ((VtxDesc & maskTex6MatIdx) || (VtxDesc & maskTex6Coord))
		{
			TexCoordElement<iSSE,
				static_cast<EVTXComponentType>((VtxDesc & maskTex6Coord) >> strideTex6Coord),
				static_cast<EVTXComponentFormat>((VAT2 & maskTex6CoordFormat) >> strideTex6CoordFormat),
				static_cast<ETcoordElements>((VAT2 & maskTex6CoordElements) >> strideTex6CoordElements),
				static_cast<bool>((VtxDesc & maskTex6MatIdx) == maskTex6MatIdx)>(pipelinestate);
		}
		else if ((VtxDesc & compTex6MatIdx) || (VtxDesc & compTex6Coord))
		{
			pipelinestate.tcIndex++;
		}
		if ((VtxDesc & maskTex7MatIdx) || (VtxDesc & maskTex7Coord))
		{
			TexCoordElement<iSSE,
				static_cast<EVTXComponentType>((VtxDesc & maskTex7Coord) >> strideTex7Coord),
				static_cast<EVTXComponentFormat>((VAT2 & maskTex7CoordFormat) >> strideTex7CoordFormat),
				static_cast<ETcoordElements>((VAT2 & maskTex7CoordElements) >> strideTex7CoordElements),
				static_cast<bool>((VtxDesc & maskTex7MatIdx) == maskTex7MatIdx)>(pipelinestate);
		}
		if (pipelinestate.flags & TPS_USE_BBOX)
		{
			BoundingBox::Update();
		}
		pipelinestate.Write<u32>((u32)pipelinestate.curposmtx);
		if (VtxDesc & 0x200)
		{
			if (pipelinestate.flags & TPS_SKIP_VERTEX)
			{
				// reset the output buffer
				pipelinestate.SetWritePosition(pipelinestate.GetWritePosition() - pipelinestate.stride);
			}
		}
		--loopcount;
	}
}