// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Added for Ishiiruka by Tino
#if defined(_M_X86_64)
#include <xmmintrin.h>
#include <emmintrin.h>
#endif

#include "VideoCommon/VertexLoader_Mtx.h"

void LOADERDECL Vertexloader_Mtx::PosMtx_ReadDirect_UByte()
{
	g_PipelineState.curposmtx = g_PipelineState.Read<u8>() & 0x3f;
}

void LOADERDECL Vertexloader_Mtx::PosMtx_Write()
{
	g_PipelineState.Write<u32>(g_PipelineState.curposmtx);
}

void LOADERDECL Vertexloader_Mtx::TexMtx_ReadDirect_UByte()
{
	g_PipelineState.curtexmtx[g_PipelineState.texmtxread++] = g_PipelineState.Read<u8>() & 0x3f;
}

void LOADERDECL Vertexloader_Mtx::TexMtx_Write_Float()
{
	g_PipelineState.Write(float(g_PipelineState.curtexmtx[g_PipelineState.texmtxwrite++]));
}

void LOADERDECL Vertexloader_Mtx::TexMtx_Write_Float2()
{
	g_PipelineState.Write(0.f);
	g_PipelineState.Write(float(g_PipelineState.curtexmtx[g_PipelineState.texmtxwrite++]));
}

void LOADERDECL Vertexloader_Mtx::TexMtx_Write_Float3()
{
#if _M_SSE >= 0x200
	__m128 output = _mm_cvtsi32_ss(_mm_castsi128_ps(_mm_setzero_si128()), g_PipelineState.curtexmtx[g_PipelineState.texmtxwrite++]);
	_mm_storeu_ps((float*)g_PipelineState.GetWritePosition(), _mm_shuffle_ps(output, output, 0x45));
	g_PipelineState.WriteSkip(sizeof(float) * 3);
#else
	g_PipelineState.Write(0.f);
	g_PipelineState.Write(0.f);
	g_PipelineState.Write(float(g_PipelineState.curtexmtx[g_PipelineState.texmtxwrite++]));
#endif
}