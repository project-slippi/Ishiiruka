// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/VideoConfig.h"

//Init
u16 *IndexGenerator::index_buffer_current;
u16 *IndexGenerator::BASEIptr;
u32 IndexGenerator::base_index;

static void(*primitive_table[8])(u32);

void IndexGenerator::Init()
{
	primitive_table[GX_DRAW_QUADS] = IndexGenerator::AddQuads;
#if defined(_DEBUG) || defined(DEBUGFAST)
	primitive_table[GX_DRAW_QUADS_2] = IndexGenerator::AddQuads_nonstandard;
#else
	primitive_table[GX_DRAW_QUADS_2] = IndexGenerator::AddQuads;
#endif
	primitive_table[GX_DRAW_TRIANGLES] = IndexGenerator::AddList;
	primitive_table[GX_DRAW_TRIANGLE_STRIP] = IndexGenerator::AddStrip;
	primitive_table[GX_DRAW_TRIANGLE_FAN] = IndexGenerator::AddFan;
	primitive_table[GX_DRAW_LINES] = &IndexGenerator::AddLineList;
	primitive_table[GX_DRAW_LINE_STRIP] = &IndexGenerator::AddLineStrip;
	primitive_table[GX_DRAW_POINTS] = &IndexGenerator::AddPoints;
}

void IndexGenerator::Start(u16* Indexptr)
{
	index_buffer_current = Indexptr;
	BASEIptr = Indexptr;
	base_index = 0;
}

void IndexGenerator::AddIndices(int primitive, u32 numVerts)
{
	primitive_table[primitive](numVerts);
	base_index += numVerts;
}

// Triangles
__forceinline u16* IndexGenerator::WriteTriangle(u16* ptr, u32 index1, u32 index2, u32 index3)
{
	*ptr++ = index1;
	*ptr++ = index2;
	*ptr++ = index3;
	return ptr;
}

void IndexGenerator::AddList(u32 const numVerts)
{
	u32 i = base_index + 2;
	u32 top = (base_index + numVerts);
	u16* ptr = index_buffer_current;
	while (i < top)
	{
		ptr = WriteTriangle(ptr, i - 2, i - 1, i);
		i += 3;
	}
	index_buffer_current = ptr;
}

void IndexGenerator::AddStrip(u32 const numVerts)
{
	u16* ptr = index_buffer_current;
	u32 top = (base_index + numVerts);
	u32 a = base_index;
	u32 i = a + 2;
	u32 wind = 1;
	while (i < top)
	{
		u32 b = i - wind;
		wind ^= 1;
		u32 c = i - wind;
		ptr = WriteTriangle(
			ptr,
			a,
			b,
			c);
		++i;
		++a;
	}
	index_buffer_current = ptr;
}

/**
 * FAN simulator:
 *
 *   2---3
 *  / \ / \
 * 1---0---4
 *
 * would generate this triangles:
 * 012, 023, 034
 *
 * rotated (for better striping):
 * 120, 302, 034
 *
 * as odd ones have to winded, following strip is fine:
 * 12034
 *
 * so we use 6 indices for 3 triangles
 */

void IndexGenerator::AddFan(u32 numVerts)
{
	u32 i = base_index + 2;
	u32 top = (base_index + numVerts);
	u16* ptr = index_buffer_current;

	while (i < top)
	{
		ptr = WriteTriangle(ptr, base_index, i - 1, i);
		++i;
	}
	index_buffer_current = ptr;
}

/*
 * QUAD simulator
 *
 * 0---1   4---5
 * |\  |   |\  |
 * | \ |   | \ |
 * |  \|   |  \|
 * 3---2   7---6
 *
 * 012,023, 456,467 ...
 * or 120,302, 564,746
 * or as strip: 1203, 5647
 *
 * Warning:
 * A simple triangle has to be rendered for three vertices.
 * ZWW do this for sun rays
 */
void IndexGenerator::AddQuads(u32 numVerts)
{
	u32 i = base_index + 3;
	u32 top = (base_index + numVerts);
	u16* ptr = index_buffer_current;
	while (i < top)
	{
		ptr = WriteTriangle(ptr, i - 3, i - 2, i - 1);
		ptr = WriteTriangle(ptr, i - 3, i - 1, i - 0);
		i += 4;
	}

	// three vertices remaining, so render a triangle
	if (i == top)
	{
		ptr = WriteTriangle(ptr, top - 3, top - 2, top - 1);
	}
	index_buffer_current = ptr;
}

void IndexGenerator::AddQuads_nonstandard(u32 numVerts)
{
	WARN_LOG(VIDEO, "Non-standard primitive drawing command GL_DRAW_QUADS_2");
	AddQuads(numVerts);
}

// Lines
void IndexGenerator::AddLineList(u32 numVerts)
{
	u32 i = base_index + 1;
	u32 top = (base_index + numVerts);
	u16* ptr = index_buffer_current;
	while (i < top)
	{
		*ptr++ = i - 1;
		*ptr++ = i;
		i += 2;
	}
	index_buffer_current = ptr;
}

// shouldn't be used as strips as LineLists are much more common
// so converting them to lists
void IndexGenerator::AddLineStrip(u32 numVerts)
{
	u32 i = base_index + 1;
	u32 top = (base_index + numVerts);
	u16* ptr = index_buffer_current;
	while (i < top)
	{
		*ptr++ = i - 1;
		*ptr++ = i;
		++i;
	}
	index_buffer_current = ptr;
}

// Points
void IndexGenerator::AddPoints(u32 numVerts)
{
	u32 i = base_index;
	u32 top = (base_index + numVerts);
	u16 *ptr = index_buffer_current;
	while (i < top)
	{
		*ptr++ = i;
		++i;
	}
	index_buffer_current = ptr;
}
