// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/XFMemory.h"

#pragma pack(1)

struct HDstage_hash_data
{
	// Align Everything to 32 bits words to speed up things
	u32 tevorders_enable : 1;
	u32 tevorders_texmap : 3;
	u32 tevorders_texcoord : 3;
	u32 hasindstage : 1;
	u32 pad0 : 3;
	u32 tevind : 21;
};

struct HullDomain_shader_uid_data
{
	u32 NumValues() const { return sizeof(HullDomain_shader_uid_data); }
	u32 StartValue() const { return 0; }

	u32 numTexGens : 4;
	u32 normal : 1;
	u32 genMode_numtevstages : 4;
	u32 genMode_numindstages : 3;
	u32 nIndirectStagesUsed : 4;
	u32 pixel_normals : 1;
	u32 padding : 15;
	

	u32 texMtxInfo_n_projection : 8; // 8x1 bit
	u32 tevindref_bi0 : 3;
	u32 tevindref_bc0 : 3;
	u32 tevindref_bi1 : 3;
	u32 tevindref_bc1 : 3;
	u32 tevindref_bi2 : 3;
	u32 tevindref_bc3 : 3;
	u32 tevindref_bi4 : 3;
	u32 tevindref_bc4 : 3;

	HDstage_hash_data stagehash[16];

	inline void SetTevindrefValues(int index, u32 texcoord, u32 texmap)
	{
		if (index == 0) { tevindref_bc0 = texcoord; tevindref_bi0 = texmap; }
		else if (index == 1) { tevindref_bc1 = texcoord; tevindref_bi1 = texmap; }
		else if (index == 2) { tevindref_bc3 = texcoord; tevindref_bi2 = texmap; }
		else if (index == 3) { tevindref_bc4 = texcoord; tevindref_bi4 = texmap; }
	}
	inline void SetTevindrefTexmap(int index, u32 texmap)
	{
		if (index == 0) { tevindref_bi0 = texmap; }
		else if (index == 1) { tevindref_bi1 = texmap; }
		else if (index == 2) { tevindref_bi2 = texmap; }
		else if (index == 3) { tevindref_bi4 = texmap; }
	}
};

#pragma pack()

#define I_TESSPARAMS  "ctess"
#define I_PROJECTION  "cproj"
#define I_DEPTHPARAMS "cDepth" // farZ, zRange, scaled viewport width, scaled viewport height
#define I_TEXDIMS     "texdim"
#define I_INDTEXSCALE "cindscale"
#define I_INDTEXMTX   "cindmtx"
#define I_FLAGS       "cflags"

#define HULLDOMAINSHADERGEN_BUFFERSIZE 32768
typedef ShaderUid<HullDomain_shader_uid_data> HullDomainShaderUid;

void GenerateHullDomainShaderCode(ShaderCode& object, API_TYPE ApiType, const XFMemory &xfr, const BPMemory &bpm, const u32 components);
void GetHullDomainShaderUid(HullDomainShaderUid& object, API_TYPE ApiType, const XFMemory &xfr, const BPMemory &bpm, const u32 components);
