// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Modified for Ishiiruka by Tino

#pragma once
#include <limits>

#include "Common/NonCopyable.h"
#include "Common/CommonFuncs.h"

#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DataReader.h"

// m_components
enum
{
	//VB_HAS_POS=(1 << 0), // Implied, it always has pos! don't bother testing
	//VB_HAS_POSMTXIDX = (1 << 1), // Implied, always used
	VB_HAS_TEXMTXIDX0 = (1 << 0),
	VB_HAS_TEXMTXIDX1 = (1 << 1),
	VB_HAS_TEXMTXIDX2 = (1 << 2),
	VB_HAS_TEXMTXIDX3 = (1 << 3),
	VB_HAS_TEXMTXIDX4 = (1 << 4),
	VB_HAS_TEXMTXIDX5 = (1 << 5),
	VB_HAS_TEXMTXIDX6 = (1 << 6),
	VB_HAS_TEXMTXIDX7 = (1 << 7),
	VB_HAS_TEXMTXIDXALL = 0xff,


	VB_HAS_NRM0 = (1 << 8),
	VB_HAS_NRM1 = (1 << 9),
	VB_HAS_NRM2 = (1 << 10),
	VB_HAS_NRMALL = (7 << 8),

	VB_COL_SHIFT = 11,

	VB_HAS_COL0 = (1 << 11),
	VB_HAS_COL1 = (1 << 12),

	VB_HAS_UV0 = (1 << 13),
	VB_HAS_UV1 = (1 << 14),
	VB_HAS_UV2 = (1 << 15),
	VB_HAS_UV3 = (1 << 16),
	VB_HAS_UV4 = (1 << 17),
	VB_HAS_UV5 = (1 << 18),
	VB_HAS_UV6 = (1 << 19),
	VB_HAS_UV7 = (1 << 20),
	VB_HAS_UVALL = (0xff << 13)
};

#ifdef WIN32
#define LOADERDECL __cdecl
#else
#define LOADERDECL
#endif

enum PipelineStateFlags : u8
{
	TPS_NONE = 0,
	TPS_USE_BBOX = 1,
	TPS_SKIP_VERTEX = 2,
};

class TPipelineState : public DataReader, public DataWriter
{
public:
	TPipelineState(u8* source, u8* _end, u8* destination) : DataReader(source, _end), DataWriter(destination)
	{
		Clear();
	};
	TPipelineState() : DataReader(), DataWriter()
	{
		Clear();
	};
	void Initialize(u8* source, u8* _end, u8* destination)
	{
		DataReader::SetReadPosition(source, _end);
		DataWriter::SetWritePosition(destination);
	}
	void Clear()
	{
		stride = 0;
		curposmtx = 0;
		memset(curtexmtx, 0, sizeof(curtexmtx));
		texmtxwrite = 0;
		texmtxread = 0;
		tcIndex = 0;
		colIndex = 0;
		colElements[0] = 0;
		colElements[1] = 0;
		posScale = 0.0f;
		memset(tcScale, 0, sizeof(tcScale));
		count = 0;
		flags = 0;
	}
	float posScale;
	float tcScale[8];
	s32 count;
	s32 skippedVertices;
	s32 stride;
	u8 colIndex;
	u8 tcIndex;
	u8 curposmtx;
	u8 texmtxwrite;
	u8 texmtxread;
	u8 flags;
	u8 colElements[2];
	u8 curtexmtx[8];
};

typedef void (LOADERDECL *TPipelineFunction)();
typedef void (LOADERDECL *TCompiledLoaderFunction)(TPipelineState&);

struct AttributeFormat
{
	EVTXComponentFormat type;
	int components;
	int offset;
	bool enable;
};

struct PortableVertexDeclaration
{
	int stride;

	AttributeFormat position;
	AttributeFormat normals[3];
	AttributeFormat colors[2];
	AttributeFormat texcoords[8];
	AttributeFormat posmtx;
	inline bool operator<(const PortableVertexDeclaration& b) const
	{
		return memcmp(this, &b, sizeof(PortableVertexDeclaration)) < 0;
	}

	inline bool operator==(const PortableVertexDeclaration& b) const
	{
		return memcmp(this, &b, sizeof(PortableVertexDeclaration)) == 0;
	}
};

// The implementation of this class is specific for GL/DX, so NativeVertexFormat.cpp
// is in the respective backend, not here in VideoCommon.

// Note that this class can't just invent arbitrary vertex formats out of its input -
// all the data loading code must always be made compatible.
class NativeVertexFormat : NonCopyable
{
public:
	virtual ~NativeVertexFormat()
	{}

	virtual void SetupVertexPointers() = 0;

	u32 GetVertexStride() const
	{
		return vtx_decl.stride;
	}
	const PortableVertexDeclaration& GetVertexDeclaration() const
	{
		return vtx_decl;
	}
protected:
	// Let subclasses construct.
	NativeVertexFormat()
	{}

	PortableVertexDeclaration vtx_decl;
};

extern TPipelineState g_PipelineState;
