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

union Tessellationstage_hash_data
{
	// Align Everything to 32 bits words to speed up things
	struct
	{
		u32 tevorders_enable : 1;
		u32 tevorders_texmap : 3;
		u32 tevorders_texcoord : 3;
		u32 hasindstage : 1;
		u32 pad0 : 3;
		u32 tevind : 21;
	};
	u32 hex;
};

struct Tessellation_shader_uid_data
{
	u32 NumValues() const
	{
		return sizeof(Tessellation_shader_uid_data);
	}
	u32 StartValue() const
	{
		return 0;
	}

	void ClearUnused()
	{
		texMtxInfo_n_projection = 0;
	}

	u32 numTexGens : 4;
	u32 normal : 1;
	u32 genMode_numtevstages : 4;
	u32 genMode_numindstages : 3;
	u32 nIndirectStagesUsed : 4;
	u32 pixel_normals : 1;
	u32 stereo : 1;
	u32 pixel_lighting : 1;
	u32 msaa : 1;
	u32 ssaa : 1;
	u32 padding : 11;


	u32 texMtxInfo_n_projection : 8; // 8x1 bit
	u32 tevindref_bi0 : 3;
	u32 tevindref_bc0 : 3;
	u32 tevindref_bi1 : 3;
	u32 tevindref_bc1 : 3;
	u32 tevindref_bi2 : 3;
	u32 tevindref_bc3 : 3;
	u32 tevindref_bi4 : 3;
	u32 tevindref_bc4 : 3;

	Tessellationstage_hash_data stagehash[16];

	inline void SetTevindrefValues(int index, u32 texcoord, u32 texmap)
	{
		if (index == 0)
		{
			tevindref_bc0 = texcoord; tevindref_bi0 = texmap;
		}
		else if (index == 1)
		{
			tevindref_bc1 = texcoord; tevindref_bi1 = texmap;
		}
		else if (index == 2)
		{
			tevindref_bc3 = texcoord; tevindref_bi2 = texmap;
		}
		else if (index == 3)
		{
			tevindref_bc4 = texcoord; tevindref_bi4 = texmap;
		}
	}

	inline u32 GetTevindirefCoord(int index) const
	{
		if (index == 0)
		{
			return tevindref_bc0;
		}
		else if (index == 1)
		{
			return tevindref_bc1;
		}
		else if (index == 2)
		{
			return tevindref_bc3;
		}
		else if (index == 3)
		{
			return tevindref_bc4;
		}
		return 0;
	}

	inline u32 GetTevindirefMap(int index) const
	{
		if (index == 0)
		{
			return tevindref_bi0;
		}
		else if (index == 1)
		{
			return tevindref_bi1;
		}
		else if (index == 2)
		{
			return tevindref_bi2;
		}
		else if (index == 3)
		{
			return tevindref_bi4;
		}
		return 0;
	}
};

#pragma pack()

#define I_TESSPARAMS  "ctess"
#define I_CULLPARAMS  "ccullp"

#define TESSELLATIONSHADERGEN_BUFFERSIZE 32768
#define TESSELLATIONSHADERGEN_UID_VERSION 1
typedef ShaderUid<Tessellation_shader_uid_data> TessellationShaderUid;

void GenerateTessellationShaderCode(ShaderCode& object, API_TYPE ApiType, const Tessellation_shader_uid_data& uid_data);
void GetTessellationShaderUID(TessellationShaderUid& out, const XFMemory& xfr, const BPMemory& bpm, const u32 components);
