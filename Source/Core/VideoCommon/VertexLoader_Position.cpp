// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Modified for Ishiiruka by Tino
#include "Common/CPUDetect.h"
#include "VideoCommon/VertexLoader_Position.h"
#include "VideoCommon/VertexLoader_PositionFuncs.h"

bool VertexLoader_Position::Initialized = false;

template <typename T, int N>
void LOADERDECL Pos_ReadDirect()
{
	_Pos_ReadDirect<T, N>(g_PipelineState);
}

template <typename I, typename T, int N>
void LOADERDECL Pos_ReadIndex()
{
	_Pos_ReadIndex<I, T, N>(g_PipelineState);
}

#if _M_SSE >= 0x301

template <bool three>
void LOADERDECL Pos_ReadDirect_UByte_SSSE3()
{
	_Pos_ReadDirect_UByte_SSSE3<three>(g_PipelineState);
}

template <bool three>
void LOADERDECL Pos_ReadDirect_SByte_SSSE3()
{
	_Pos_ReadDirect_SByte_SSSE3<three>(g_PipelineState);
}

template <bool three>
void LOADERDECL Pos_ReadDirect_UShort_SSSE3()
{
	_Pos_ReadDirect_UShort_SSSE3<three>(g_PipelineState);
}

template <bool three>
void LOADERDECL Pos_ReadDirect_Short_SSSE3()
{
	_Pos_ReadDirect_Short_SSSE3<three>(g_PipelineState);
}

template <bool three>
void LOADERDECL Pos_ReadDirect_Float_SSSE3()
{
	_Pos_ReadDirect_Float_SSSE3<three>(g_PipelineState);
}

template <typename I, bool three>
void LOADERDECL Pos_ReadIndex_UByte_SSSE3()
{
	_Pos_ReadIndex_UByte_SSSE3<I, three>(g_PipelineState);
}

template <typename I, bool three>
void LOADERDECL Pos_ReadIndex_SByte_SSSE3()
{
	_Pos_ReadIndex_SByte_SSSE3<I, three>(g_PipelineState);
}

template <typename I, bool three>
void LOADERDECL Pos_ReadIndex_UShort_SSSE3()
{
	_Pos_ReadIndex_UShort_SSSE3<I, three>(g_PipelineState);
}

template <typename I, bool three>
void LOADERDECL Pos_ReadIndex_Short_SSSE3()
{
	_Pos_ReadIndex_Short_SSSE3<I, three>(g_PipelineState);
}

template <typename I, bool three>
void LOADERDECL Pos_ReadIndex_Float_SSSE3()
{
	_Pos_ReadIndex_Float_SSSE3<I, three>(g_PipelineState);
}
#endif

static TPipelineFunction tableReadPosition[4][5][2] = {
		{
			{ nullptr, nullptr },
			{ nullptr, nullptr },
			{ nullptr, nullptr },
			{ nullptr, nullptr },
			{ nullptr, nullptr }
		},
		{
			{ Pos_ReadDirect<u8, 2>, Pos_ReadDirect<u8, 3> },
			{ Pos_ReadDirect<s8, 2>, Pos_ReadDirect<s8, 3> },
			{ Pos_ReadDirect<u16, 2>, Pos_ReadDirect<u16, 3> },
			{ Pos_ReadDirect<s16, 2>, Pos_ReadDirect<s16, 3> },
			{ Pos_ReadDirect<float, 2>, Pos_ReadDirect<float, 3> }
		},
		{
			{ Pos_ReadIndex<u8, u8, 2>, Pos_ReadIndex<u8, u8, 3> },
			{ Pos_ReadIndex<u8, s8, 2>, Pos_ReadIndex<u8, s8, 3> },
			{ Pos_ReadIndex<u8, u16, 2>, Pos_ReadIndex<u8, u16, 3> },
			{ Pos_ReadIndex<u8, s16, 2>, Pos_ReadIndex<u8, s16, 3> },
			{ Pos_ReadIndex<u8, float, 2>, Pos_ReadIndex<u8, float, 3> }
		},
		{
			{ Pos_ReadIndex<u16, u8, 2>, Pos_ReadIndex<u16, u8, 3> },
			{ Pos_ReadIndex<u16, s8, 2>, Pos_ReadIndex<u16, s8, 3> },
			{ Pos_ReadIndex<u16, u16, 2>, Pos_ReadIndex<u16, u16, 3> },
			{ Pos_ReadIndex<u16, s16, 2>, Pos_ReadIndex<u16, s16, 3> },
			{ Pos_ReadIndex<u16, float, 2>, Pos_ReadIndex<u16, float, 3> }
		},
};

static int tableReadPositionVertexSize[4][5][2] = {
		{
			{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }
		},
		{
			{ 2, 3 }, { 2, 3 }, { 4, 6 }, { 4, 6 }, { 8, 12 }
		},
		{
			{ 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 }
		},
		{
			{ 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 }
		},
};

void VertexLoader_Position::Init(void)
{
	if (Initialized)
	{
		return;
	}
	Initialized = true;
#if _M_SSE >= 0x301

	if (cpu_info.bSSSE3)
	{
		tableReadPosition[DIRECT][FORMAT_UBYTE][POS_ELEMENTS_2] = Pos_ReadDirect_UByte_SSSE3<false>;
		tableReadPosition[DIRECT][FORMAT_BYTE][POS_ELEMENTS_2] = Pos_ReadDirect_SByte_SSSE3<false>;
		tableReadPosition[DIRECT][FORMAT_USHORT][POS_ELEMENTS_2] = Pos_ReadDirect_UShort_SSSE3<false>;
		tableReadPosition[DIRECT][FORMAT_SHORT][POS_ELEMENTS_2] = Pos_ReadDirect_Short_SSSE3<false>;
		tableReadPosition[DIRECT][FORMAT_FLOAT][POS_ELEMENTS_2] = Pos_ReadDirect_Float_SSSE3<false>;

		tableReadPosition[DIRECT][FORMAT_UBYTE][POS_ELEMENTS_3] = Pos_ReadDirect_UByte_SSSE3<true>;
		tableReadPosition[DIRECT][FORMAT_BYTE][POS_ELEMENTS_3] = Pos_ReadDirect_SByte_SSSE3<true>;
		tableReadPosition[DIRECT][FORMAT_USHORT][POS_ELEMENTS_3] = Pos_ReadDirect_UShort_SSSE3<true>;
		tableReadPosition[DIRECT][FORMAT_SHORT][POS_ELEMENTS_3] = Pos_ReadDirect_Short_SSSE3<true>;
		tableReadPosition[DIRECT][FORMAT_FLOAT][POS_ELEMENTS_3] = Pos_ReadDirect_Float_SSSE3<true>;

		tableReadPosition[INDEX8][FORMAT_UBYTE][POS_ELEMENTS_2] = Pos_ReadIndex_UByte_SSSE3<u8, false>;
		tableReadPosition[INDEX8][FORMAT_BYTE][POS_ELEMENTS_2] = Pos_ReadIndex_SByte_SSSE3<u8, false>;
		tableReadPosition[INDEX8][FORMAT_USHORT][POS_ELEMENTS_2] = Pos_ReadIndex_UShort_SSSE3<u8, false>;
		tableReadPosition[INDEX8][FORMAT_SHORT][POS_ELEMENTS_2] = Pos_ReadIndex_Short_SSSE3<u8, false>;
		tableReadPosition[INDEX8][FORMAT_FLOAT][POS_ELEMENTS_2] = Pos_ReadIndex_Float_SSSE3<u8, false>;

		tableReadPosition[INDEX8][FORMAT_UBYTE][POS_ELEMENTS_3] = Pos_ReadIndex_UByte_SSSE3<u8, true>;
		tableReadPosition[INDEX8][FORMAT_BYTE][POS_ELEMENTS_3] = Pos_ReadIndex_SByte_SSSE3<u8, true>;
		tableReadPosition[INDEX8][FORMAT_USHORT][POS_ELEMENTS_3] = Pos_ReadIndex_UShort_SSSE3<u8, true>;
		tableReadPosition[INDEX8][FORMAT_SHORT][POS_ELEMENTS_3] = Pos_ReadIndex_Short_SSSE3<u8, true>;
		tableReadPosition[INDEX8][FORMAT_FLOAT][POS_ELEMENTS_3] = Pos_ReadIndex_Float_SSSE3<u8, true>;

		tableReadPosition[INDEX16][FORMAT_UBYTE][POS_ELEMENTS_2] = Pos_ReadIndex_UByte_SSSE3<u16, false>;
		tableReadPosition[INDEX16][FORMAT_BYTE][POS_ELEMENTS_2] = Pos_ReadIndex_SByte_SSSE3<u16, false>;
		tableReadPosition[INDEX16][FORMAT_USHORT][POS_ELEMENTS_2] = Pos_ReadIndex_UShort_SSSE3<u16, false>;
		tableReadPosition[INDEX16][FORMAT_SHORT][POS_ELEMENTS_2] = Pos_ReadIndex_Short_SSSE3<u16, false>;
		tableReadPosition[INDEX16][FORMAT_FLOAT][POS_ELEMENTS_2] = Pos_ReadIndex_Float_SSSE3<u16, false>;

		tableReadPosition[INDEX16][FORMAT_UBYTE][POS_ELEMENTS_3] = Pos_ReadIndex_UByte_SSSE3<u16, true>;
		tableReadPosition[INDEX16][FORMAT_BYTE][POS_ELEMENTS_3] = Pos_ReadIndex_SByte_SSSE3<u16, true>;
		tableReadPosition[INDEX16][FORMAT_USHORT][POS_ELEMENTS_3] = Pos_ReadIndex_UShort_SSSE3<u16, true>;
		tableReadPosition[INDEX16][FORMAT_SHORT][POS_ELEMENTS_3] = Pos_ReadIndex_Short_SSSE3<u16, true>;
		tableReadPosition[INDEX16][FORMAT_FLOAT][POS_ELEMENTS_3] = Pos_ReadIndex_Float_SSSE3<u16, true>;
	}

#endif
}

u32 VertexLoader_Position::GetSize(u32 _type, u32 _format, u32 _elements)
{
	return tableReadPositionVertexSize[_type][_format][_elements];
}

TPipelineFunction VertexLoader_Position::GetFunction(u32 _type, u32 _format, u32 _elements)
{
	return tableReadPosition[_type][_format][_elements];
}