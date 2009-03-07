// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Common.h"

#include "D3DBase.h"

#include "Statistics.h"
#include "Profiler.h"
#include "VertexManager.h"
#include "OpcodeDecoding.h"
#include "IndexGenerator.h"
#include "VertexShaderManager.h"
#include "VertexShaderCache.h"
#include "PixelShaderManager.h"
#include "PixelShaderCache.h"
#include "Utils.h"
#include "NativeVertexFormat.h"
#include "NativeVertexWriter.h"
#include "TextureCache.h"

#include "BPStructs.h"
#include "XFStructs.h"

using namespace D3D;

// internal state for loading vertices
extern NativeVertexFormat *g_nativeVertexFmt;

namespace VertexManager
{

enum Collection
{
	C_NOTHING,
	C_TRIANGLES,
	C_LINES,
	C_POINTS,
};

static IndexGenerator indexGen;
static Collection collection;

static LPDIRECT3DVERTEXDECLARATION9 vDecl;

static u8 *fakeVBuffer;   // format undefined - NativeVertexFormat takes care of the declaration.
static u16 *fakeIBuffer;  // These are just straightforward 16-bit indices.

#define MAXVBUFFERSIZE 65536*3
#define MAXIBUFFERSIZE 65536*3

const Collection collectionTypeLUT[8] =
{
	C_TRIANGLES,//quads
	C_NOTHING,  //nothing
	C_TRIANGLES,//triangles
	C_TRIANGLES,//strip
	C_TRIANGLES,//fan
	C_LINES,    //lines
	C_LINES,    //linestrip
	C_POINTS    //guess :P
};

void CreateDeviceObjects();
void DestroyDeviceObjects();

bool Init()
{
	collection = C_NOTHING;
	fakeVBuffer = new u8[MAXVBUFFERSIZE];
	fakeIBuffer = new u16[MAXIBUFFERSIZE];
	CreateDeviceObjects();
	VertexManager::s_pCurBufferPointer = fakeVBuffer;
	return true;
}

void Shutdown()
{
	DestroyDeviceObjects();
	delete [] fakeVBuffer;
	delete [] fakeIBuffer;
}

void CreateDeviceObjects()
{
}

void BeginFrame()
{
}

void DestroyDeviceObjects()
{
}

void AddIndices(int _primitive, int _numVertices)
{
	switch (_primitive)
	{
	case GX_DRAW_QUADS:          indexGen.AddQuads(_numVertices);     return;    
	case GX_DRAW_TRIANGLES:      indexGen.AddList(_numVertices);      return;    
	case GX_DRAW_TRIANGLE_STRIP: indexGen.AddStrip(_numVertices);     return;
	case GX_DRAW_TRIANGLE_FAN:   indexGen.AddFan(_numVertices);       return;
	case GX_DRAW_LINE_STRIP:     indexGen.AddLineStrip(_numVertices); return;
	case GX_DRAW_LINES:		     indexGen.AddLineList(_numVertices);  return;
	case GX_DRAW_POINTS:         indexGen.AddPointList(_numVertices); return;
	}
}

int GetRemainingSize()
{
	return 60000;
}

void AddVertices(int _primitive, int _numVertices)
{
	if (_numVertices <= 0) //This check is pretty stupid... 
		return;
	
	Collection type = collectionTypeLUT[_primitive];
	if (type == C_NOTHING)
		return;

    DVSTARTPROFILE();
	_assert_msg_(type != C_NOTHING, "type == C_NOTHING!!", "WTF");
	
	if (indexGen.GetNumVerts() > 1000) // TODO(ector): Raise?
		Flush();

	ADDSTAT(stats.thisFrame.numPrims, _numVertices);

	if (collection != type)
	{
		//We are NOT collecting the right type.
		Flush();
		collection = type;
		u16 *ptr = 0;
		if (type != C_POINTS)
		{
			ptr = fakeIBuffer;
			indexGen.Start((unsigned short*)ptr);
			AddIndices(_primitive, _numVertices);
		}
		if (_numVertices >= MAXVBUFFERSIZE)
			MessageBox(NULL, "Too many vertices for the buffer", "Dolphin DX9 Video Plugin", MB_OK);
	}
	else //We are collecting the right type, keep going
	{
		_assert_msg_(vbufferwrite != 0, "collecting: vbufferwrite == 0!","WTF");
		INCSTAT(stats.thisFrame.numPrimitiveJoins);
		//Success, keep adding to unlocked buffer
		int last = indexGen.GetNumVerts();
		AddIndices(_primitive, _numVertices);
		if (_numVertices >= MAXVBUFFERSIZE)
			MessageBox(NULL, "Too many vertices for the buffer", "Dolphin DX9 Video Plugin", MB_OK);
	}
}

const D3DPRIMITIVETYPE pts[3] = 
{
	D3DPT_POINTLIST, //DUMMY
	D3DPT_TRIANGLELIST, 
	D3DPT_LINELIST,
};

void Flush()
{
	DVSTARTPROFILE();
	if (collection != C_NOTHING)
	{
		u32 usedtextures = 0;
		for (u32 i = 0; i < (u32)bpmem.genMode.numtevstages + 1; ++i) {
			if (bpmem.tevorders[i/2].getEnable(i & 1))
				usedtextures |= 1 << bpmem.tevorders[i/2].getTexMap(i & 1);
		}

		if (bpmem.genMode.numindstages > 0) {
			for (u32 i = 0; i < (u32)bpmem.genMode.numtevstages + 1; ++i) {
				if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages) {
					usedtextures |= 1 << bpmem.tevindref.getTexMap(bpmem.tevind[i].bt);
				}
			}
		}

		u32 nonpow2tex = 0;
		for (int i = 0; i < 8; i++)
		{
			// if (usedtextures & (1 << i)) {
			{
				FourTexUnits &tex = bpmem.tex[i >> 2];
				TextureCache::TCacheEntry* tentry = TextureCache::Load(i, 
					(tex.texImage3[i&3].image_base/* & 0x1FFFFF*/) << 5,
					tex.texImage0[i&3].width+1, tex.texImage0[i&3].height+1,
					tex.texImage0[i&3].format, tex.texTlut[i&3].tmem_offset<<9, 
					tex.texTlut[i&3].tlut_format);

				if (tentry) {
					// texture loaded fine, set dims for pixel shader
					if (tentry->isNonPow2) {
						PixelShaderManager::SetTexDims(i, tentry->w, tentry->h, tentry->mode.wrap_s, tentry->mode.wrap_t);
						nonpow2tex |= 1 << i;
						if (tentry->mode.wrap_s > 0) nonpow2tex |= 1 << (8 + i);
						if (tentry->mode.wrap_t > 0) nonpow2tex |= 1 << (16 + i);
					}
					// if texture is power of two, set to ones (since don't need scaling)
					// (the above seems to have changed - we set the width and height here too.
					else 
					{
						PixelShaderManager::SetTexDims(i, tentry->w, tentry->h, 0, 0);
					}
				}
				else
					ERROR_LOG(VIDEO, "error loading texture");
			}
		}
		PixelShaderManager::SetTexturesUsed(nonpow2tex);


		int numVertices = indexGen.GetNumVerts();
		if (numVertices)
		{
			PixelShaderCache::SetShader();  // TODO(ector): only do this if shader has changed
			VertexShaderCache::SetShader(g_nativeVertexFmt->m_components);  // TODO(ector): only do this if shader has changed
	
			// set global constants
			VertexShaderManager::SetConstants(false);
			PixelShaderManager::SetConstants();

			int stride = g_nativeVertexFmt->GetVertexStride();
			g_nativeVertexFmt->SetupVertexPointers();
			if (collection != C_POINTS)
			{
				int numPrimitives = indexGen.GetNumPrims();
				D3D::dev->DrawIndexedPrimitiveUP(pts[(int)collection], 
												 0, 
												 numVertices, 
												 numPrimitives,
												 fakeIBuffer,
												 D3DFMT_INDEX16,
												 fakeVBuffer,
												 stride);
			}
			else
			{
				D3D::dev->SetIndices(0);
				D3D::dev->DrawPrimitiveUP(D3DPT_POINTLIST, numVertices, fakeVBuffer, stride);
			}
			INCSTAT(stats.thisFrame.numDrawCalls);
		}

		collection = C_NOTHING;
		VertexManager::s_pCurBufferPointer = fakeVBuffer;
	}
}

}  // namespace
