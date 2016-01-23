// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Modified for Ishiiruka by Tino
#include "Common/CPUDetect.h"
#include "VideoCommon/VertexLoader_TextCoord.h"
#include "VideoCommon/VertexLoader_TextCoordFuncs.h"

bool VertexLoader_TextCoord::Initialized = false;

static void LOADERDECL TexCoord_Read_Dummy()
{
	g_PipelineState.tcIndex++;
}

template <typename T, s32 N>
void LOADERDECL TexCoord_ReadDirect()
{
	_TexCoord_ReadDirect<T, N>(g_PipelineState);
}

template <typename I, typename T, s32 N>
void LOADERDECL TexCoord_ReadIndex()
{
	_TexCoord_ReadIndex<I, T, N>(g_PipelineState);
}

#if _M_SSE >= 0x301
void LOADERDECL TexCoord_ReadDirect_UByte2_SSSE3()
{
	_TexCoord_ReadDirect_UByte2_SSSE3(g_PipelineState);
}

void LOADERDECL TexCoord_ReadDirect_SByte2_SSSE3()
{
	_TexCoord_ReadDirect_SByte2_SSSE3(g_PipelineState);
}

void LOADERDECL TexCoord_ReadDirect_UShort2_SSSE3()
{
	_TexCoord_ReadDirect_UShort2_SSSE3(g_PipelineState);
}

void LOADERDECL TexCoord_ReadDirect_Short2_SSSE3()
{
	_TexCoord_ReadDirect_Short2_SSSE3(g_PipelineState);
}

void LOADERDECL TexCoord_ReadDirect_Float2_SSSE3()
{
	_TexCoord_ReadDirect_Float2_SSSE3(g_PipelineState);
}

template <typename I>
void LOADERDECL TexCoord_ReadIndex_UByte2_SSSE3()
{
	_TexCoord_ReadIndex_UByte2_SSSE3<I>(g_PipelineState);
}

template <typename I>
void LOADERDECL TexCoord_ReadIndex_SByte2_SSSE3()
{
	_TexCoord_ReadIndex_SByte2_SSSE3<I>(g_PipelineState);
}

template <typename I>
void LOADERDECL TexCoord_ReadIndex_UShort2_SSSE3()
{
	_TexCoord_ReadIndex_UShort2_SSSE3<I>(g_PipelineState);
}

template <typename I>
void LOADERDECL TexCoord_ReadIndex_Short2_SSSE3()
{
	_TexCoord_ReadIndex_Short2_SSSE3<I>(g_PipelineState);
}

template <typename I>
void LOADERDECL TexCoord_ReadIndex_Float2_SSSE3()
{
	_TexCoord_ReadIndex_Float2_SSSE3<I>(g_PipelineState);
}
#endif

static TPipelineFunction tableReadTexCoord[4][5][2] = {
		{
			{ NULL, NULL },
			{ NULL, NULL },
			{ NULL, NULL },
			{ NULL, NULL },
			{ NULL, NULL }
		},
		{
			{ TexCoord_ReadDirect<u8, 1>, TexCoord_ReadDirect<u8, 2> },
			{ TexCoord_ReadDirect<s8, 1>, TexCoord_ReadDirect<s8, 2> },
			{ TexCoord_ReadDirect<u16, 1>, TexCoord_ReadDirect<u16, 2> },
			{ TexCoord_ReadDirect<s16, 1>, TexCoord_ReadDirect<s16, 2> },
			{ TexCoord_ReadDirect<float, 1>, TexCoord_ReadDirect<float, 2> }
		},
		{
			{ TexCoord_ReadIndex<u8, u8, 1>, TexCoord_ReadIndex<u8, u8, 2> },
			{ TexCoord_ReadIndex<u8, s8, 1>, TexCoord_ReadIndex<u8, s8, 2> },
			{ TexCoord_ReadIndex<u8, u16, 1>, TexCoord_ReadIndex<u8, u16, 2> },
			{ TexCoord_ReadIndex<u8, s16, 1>, TexCoord_ReadIndex<u8, s16, 2> },
			{ TexCoord_ReadIndex<u8, float, 1>, TexCoord_ReadIndex<u8, float, 2> }
		},
		{
			{ TexCoord_ReadIndex<u16, u8, 1>, TexCoord_ReadIndex<u16, u8, 2> },
			{ TexCoord_ReadIndex<u16, s8, 1>, TexCoord_ReadIndex<u16, s8, 2> },
			{ TexCoord_ReadIndex<u16, u16, 1>, TexCoord_ReadIndex<u16, u16, 2> },
			{ TexCoord_ReadIndex<u16, s16, 1>, TexCoord_ReadIndex<u16, s16, 2> },
			{ TexCoord_ReadIndex<u16, float, 1>, TexCoord_ReadIndex<u16, float, 2> }
		}
};

static s32 tableReadTexCoordVertexSize[4][5][2] = {
		{
			{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }
		},
		{
			{ 1, 2 }, { 1, 2 }, { 2, 4 }, { 2, 4 }, { 4, 8 }
		},
		{
			{ 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 }
		},
		{
			{ 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 }
		}
};

void VertexLoader_TextCoord::Init(void)
{
	if (Initialized)
	{
		return;
	}
	Initialized = true;
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		tableReadTexCoord[DIRECT][FORMAT_UBYTE][TC_ELEMENTS_2] = TexCoord_ReadDirect_UByte2_SSSE3;
		tableReadTexCoord[DIRECT][FORMAT_BYTE][TC_ELEMENTS_2] = TexCoord_ReadDirect_SByte2_SSSE3;
		tableReadTexCoord[DIRECT][FORMAT_USHORT][TC_ELEMENTS_2] = TexCoord_ReadDirect_UShort2_SSSE3;
		tableReadTexCoord[DIRECT][FORMAT_SHORT][TC_ELEMENTS_2] = TexCoord_ReadDirect_Short2_SSSE3;
		tableReadTexCoord[DIRECT][FORMAT_FLOAT][TC_ELEMENTS_2] = TexCoord_ReadDirect_Float2_SSSE3;

		tableReadTexCoord[INDEX8][FORMAT_UBYTE][TC_ELEMENTS_2] = TexCoord_ReadIndex_UByte2_SSSE3<u8>;
		tableReadTexCoord[INDEX8][FORMAT_BYTE][TC_ELEMENTS_2] = TexCoord_ReadIndex_SByte2_SSSE3<u8>;
		tableReadTexCoord[INDEX8][FORMAT_USHORT][TC_ELEMENTS_2] = TexCoord_ReadIndex_UShort2_SSSE3<u8>;
		tableReadTexCoord[INDEX8][FORMAT_SHORT][TC_ELEMENTS_2] = TexCoord_ReadIndex_Short2_SSSE3<u8>;
		tableReadTexCoord[INDEX8][FORMAT_FLOAT][TC_ELEMENTS_2] = TexCoord_ReadIndex_Float2_SSSE3<u8>;

		tableReadTexCoord[INDEX16][FORMAT_UBYTE][TC_ELEMENTS_2] = TexCoord_ReadIndex_UByte2_SSSE3<u16>;
		tableReadTexCoord[INDEX16][FORMAT_BYTE][TC_ELEMENTS_2] = TexCoord_ReadIndex_SByte2_SSSE3<u16>;
		tableReadTexCoord[INDEX16][FORMAT_USHORT][TC_ELEMENTS_2] = TexCoord_ReadIndex_UShort2_SSSE3<u16>;
		tableReadTexCoord[INDEX16][FORMAT_SHORT][TC_ELEMENTS_2] = TexCoord_ReadIndex_Short2_SSSE3<u16>;
		tableReadTexCoord[INDEX16][FORMAT_FLOAT][TC_ELEMENTS_2] = TexCoord_ReadIndex_Float2_SSSE3<u16>;
	}
#endif
}

u32 VertexLoader_TextCoord::GetSize(u32 _type, u32 _format, u32 _elements)
{
	return tableReadTexCoordVertexSize[_type][_format][_elements];
}

TPipelineFunction VertexLoader_TextCoord::GetFunction(u32 _type, u32 _format, u32 _elements)
{
	return tableReadTexCoord[_type][_format][_elements];
}

TPipelineFunction VertexLoader_TextCoord::GetDummyFunction()
{
	return TexCoord_Read_Dummy;
}
