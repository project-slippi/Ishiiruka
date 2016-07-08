// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Modified For Ishiiruka By Tino
#include "VideoCommon/VertexLoader_Color.h"
#include "VideoCommon/VertexLoader_ColorFuncs.h"

void LOADERDECL Color_ReadDirect_24b_888()
{
	_Color_ReadDirect_24b_888(g_PipelineState);
}
void LOADERDECL Color_ReadDirect_32b_888x()
{
	_Color_ReadDirect_32b_888x(g_PipelineState);
}
void LOADERDECL Color_ReadDirect_16b_565()
{
	_Color_ReadDirect_16b_565(g_PipelineState);
}
void LOADERDECL Color_ReadDirect_16b_4444()
{
	_Color_ReadDirect_16b_4444(g_PipelineState);
}
void LOADERDECL Color_ReadDirect_24b_6666()
{
	_Color_ReadDirect_24b_6666(g_PipelineState);
}
void LOADERDECL Color_ReadDirect_32b_8888()
{
	_Color_ReadDirect_32b_8888(g_PipelineState);
}

void LOADERDECL Color_ReadIndex8_16b_565()
{
	Color_ReadIndex_16b_565<u8>(g_PipelineState);
}
void LOADERDECL Color_ReadIndex8_24b_888()
{
	Color_ReadIndex_24b_888<u8>(g_PipelineState);
}
void LOADERDECL Color_ReadIndex8_32b_888x()
{
	Color_ReadIndex_32b_888x<u8>(g_PipelineState);
}
void LOADERDECL Color_ReadIndex8_16b_4444()
{
	Color_ReadIndex_16b_4444<u8>(g_PipelineState);
}
void LOADERDECL Color_ReadIndex8_24b_6666()
{
	Color_ReadIndex_24b_6666<u8>(g_PipelineState);
}
void LOADERDECL Color_ReadIndex8_32b_8888()
{
	Color_ReadIndex_32b_8888<u8>(g_PipelineState);
}

void LOADERDECL Color_ReadIndex16_16b_565()
{
	Color_ReadIndex_16b_565<u16>(g_PipelineState);
}
void LOADERDECL Color_ReadIndex16_24b_888()
{
	Color_ReadIndex_24b_888<u16>(g_PipelineState);
}
void LOADERDECL Color_ReadIndex16_32b_888x()
{
	Color_ReadIndex_32b_888x<u16>(g_PipelineState);
}
void LOADERDECL Color_ReadIndex16_16b_4444()
{
	Color_ReadIndex_16b_4444<u16>(g_PipelineState);
}
void LOADERDECL Color_ReadIndex16_24b_6666()
{
	Color_ReadIndex_24b_6666<u16>(g_PipelineState);
}
void LOADERDECL Color_ReadIndex16_32b_8888()
{
	Color_ReadIndex_32b_8888<u16>(g_PipelineState);
}