// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Added for Ishiiruka by Tino
#pragma once
#include "VideoCommon/NativeVertexFormat.h"
#define ASHIFT 24
#define AMASK 0xFF000000

__forceinline void _SetCol(TPipelineState &pipelinestate, u32 val)
{
	pipelinestate.Write(val);
	pipelinestate.colIndex++;
}

//color comes in format BARG in 16 bits
//BARG -> AABBGGRR
__forceinline void _SetCol4444(TPipelineState &pipelinestate, u16 val)
{
	u32 col = (val & 0xF0);				// col  = 000000R0;
	col |= (val & 0xF) << 12;		// col |= 0000G000;
	col |= (((u32)val) & 0xF000) << 8;	// col |= 00B00000;
	col |= (((u32)val) & 0x0F00) << 20;	// col |= A0000000;
	col |= col >> 4;					// col =  A0B0G0R0 | 0A0B0G0R;
	_SetCol(pipelinestate, col);
}

//color comes in format RGBA
//RRRRRRGG GGGGBBBB BBAAAAAA
__forceinline void _SetCol6666(TPipelineState &pipelinestate, u32 val)
{
	u32 col = (val >> 16) & 0xFC;
	col |= (val >> 2) & 0xFC00;
	col |= (val << 12) & 0xFC0000;
	col |= (val << 26) & 0xFC000000;
	col |= (col >> 6) & 0x03030303;
	_SetCol(pipelinestate, col);
}

//color comes in RGB
//RRRRRGGG GGGBBBBB
__forceinline void _SetCol565(TPipelineState &pipelinestate, u16 val)
{
	u32 col = (val >> 8) & 0xF8;
	col |= (val << 5) & 0xFC00;
	col |= (((u32)val) << 19) & 0xF80000;
	col |= (col >> 5) & 0x070007;
	col |= (col >> 6) & 0x000300;
	_SetCol(pipelinestate, col | AMASK);
}

__forceinline u32 _Read24(const u8 *addr)
{
	return (*(const u32 *)addr) | AMASK;
}

__forceinline u32 _Read32(const u8 *addr)
{
	return *(const u32 *)addr;
}

__forceinline void _Color_ReadDirect_24b_888(TPipelineState &pipelinestate)
{
	_SetCol(pipelinestate, _Read24(pipelinestate.GetReadPosition()));
	pipelinestate.ReadSkip(3);
}

__forceinline void _Color_ReadDirect_32b_888x(TPipelineState &pipelinestate)
{
	_SetCol(pipelinestate, _Read24(pipelinestate.GetReadPosition()));
	pipelinestate.ReadSkip(4);
}

__forceinline void _Color_ReadDirect_16b_565(TPipelineState &pipelinestate)
{
	_SetCol565(pipelinestate, pipelinestate.Read<u16>());
}

__forceinline void _Color_ReadDirect_16b_4444(TPipelineState &pipelinestate)
{
	_SetCol4444(pipelinestate, pipelinestate.ReadUnswapped<u16>());
}

__forceinline void _Color_ReadDirect_24b_6666(TPipelineState &pipelinestate)
{
	_SetCol6666(pipelinestate, Common::swap32(pipelinestate.GetReadPosition() - 1));
	pipelinestate.ReadSkip(3);
}
// F|RES: i am not 100 percent sure, but the colElements seems to be important for rendering only
// at least it fixes mario party 4
//
//	if (colElements[colIndex])	
//	else
//		col |= 0xFF<<ASHIFT;
//
__forceinline void _Color_ReadDirect_32b_8888(TPipelineState &pipelinestate)
{
	// TODO (mb2): check this
	u32 col = pipelinestate.ReadUnswapped<u32>();

	// "kill" the alpha
	if (!pipelinestate.colElements[pipelinestate.colIndex])
		col |= 0xFF << ASHIFT;

	_SetCol(pipelinestate, col);
}

template <typename T>
__forceinline u8* IndexedColorPosition(TPipelineState &pipelinestate)
{
	auto const index = pipelinestate.Read<T>();
	return cached_arraybases[ARRAY_COLOR + pipelinestate.colIndex] + (index * g_main_cp_state.array_strides[ARRAY_COLOR + pipelinestate.colIndex]);
}

template <typename I>
__forceinline void Color_ReadIndex_16b_565(TPipelineState &pipelinestate)
{
	u16 val = Common::swap16(*(const u16 *)IndexedColorPosition<I>(pipelinestate));
	_SetCol565(pipelinestate, val);
}

template <typename I>
__forceinline void Color_ReadIndex_24b_888(TPipelineState &pipelinestate)
{
	const u8 *iAddress = IndexedColorPosition<I>(pipelinestate);
	_SetCol(pipelinestate, _Read24(iAddress));
}

template <typename I>
__forceinline void Color_ReadIndex_32b_888x(TPipelineState &pipelinestate)
{
	const u8 *iAddress = IndexedColorPosition<I>(pipelinestate);
	_SetCol(pipelinestate, _Read24(iAddress));
}

template <typename I>
__forceinline void Color_ReadIndex_16b_4444(TPipelineState &pipelinestate)
{
	u16 val = *(const u16 *)IndexedColorPosition<I>(pipelinestate);
	_SetCol4444(pipelinestate, val);
}

template <typename I>
__forceinline void Color_ReadIndex_24b_6666(TPipelineState &pipelinestate)
{
	const u8* pData = IndexedColorPosition<I>(pipelinestate) - 1;
	u32 val = Common::swap32(pData);
	_SetCol6666(pipelinestate, val);
}

template <typename I>
__forceinline void Color_ReadIndex_32b_8888(TPipelineState &pipelinestate)
{
	const u8 *iAddress = IndexedColorPosition<I>(pipelinestate);
	_SetCol(pipelinestate, _Read32(iAddress));
}