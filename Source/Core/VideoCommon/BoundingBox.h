// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/NativeVertexFormat.h"

// Bounding Box manager

namespace BoundingBox
{

// Determines if bounding box is active
extern bool active;

// Bounding box current coordinates
extern u16 coords[4];

extern u8 * bufferPos;
extern const TPipelineState *pState;

enum
{
	LEFT = 0,
	RIGHT = 1,
	TOP = 2,
	BOTTOM = 3
};

__forceinline void SetVertexBufferPosition(TPipelineState &pipelinestate)
{
	bufferPos = pipelinestate.GetWritePosition();
	pState = &pipelinestate;
}

void LOADERDECL SetVertexBufferPosition();
void LOADERDECL Update();
void Prepare(const VAT& vat, int primitive, const TVtxDesc& vtxDesc, const PortableVertexDeclaration& vtxDecl);

// Save state
void DoState(PointerWrap& p);

}; // end of namespace BoundingBox
