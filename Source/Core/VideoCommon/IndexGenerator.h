// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include "Common/CommonTypes.h"

class IndexGenerator
{
public:
	// Init
	static void Init();
	static void Start(u16 *Indexptr);

	static void AddIndices(int primitive, u32 numVertices);

	// returns numprimitives
	static inline u32 GetNumVerts()
	{
		return base_index;
	}

	static inline u32 GetIndexLen()
	{
		return (u32)(index_buffer_current - BASEIptr);
	}

	static inline u32 GetRemainingIndices()
	{
		const u32 max_index = 65534; // -1 is reserved for primitive restart (ogl + dx11)
		return max_index - base_index;
	}

	static inline u16* GetBasePointer()
	{
		return BASEIptr;
	}
private:
	// Triangles
	static void AddList(u32 numVerts);
	static void AddStrip(u32 numVerts);
	static void AddFan(u32 numVerts);
	static void AddQuads(u32 numVerts);
	static void AddQuads_nonstandard(u32 numVerts);

	// Lines
	static void AddLineList(u32 numVerts);
	static void AddLineStrip(u32 numVerts);

	// Points
	static void AddPoints(u32 numVerts);

	static u16* WriteTriangle(u16 *ptr, u32 index1, u32 index2, u32 index3);

	static u16 *index_buffer_current;
	static u16 *BASEIptr;
	static u32 base_index;
};
