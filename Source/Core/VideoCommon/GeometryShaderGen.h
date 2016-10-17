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

struct geometry_shader_uid_data
{
	u32 NumValues() const
	{
		return sizeof(geometry_shader_uid_data);
	}
	u32 StartValue() const
	{
		return 0;
	}
	bool IsPassthrough() const
	{
		return primitive_type == PRIMITIVE_TRIANGLES && !stereo && !wireframe;
	}

	void ClearUnused(){}

	u32 stereo : 1;
	u32 numTexGens : 4;
	u32 pixel_lighting : 1;
	u32 primitive_type : 2;
	u32 wireframe : 1;
	u32 msaa : 1;
	u32 ssaa : 1;
	u32 padding : 21;
};

#pragma pack()

#define I_STEREOPARAMS  "cstereo"
#define I_LINEPTPARAMS  "clinept"
#define I_TEXOFFSET     "ctexoffset"

#define GEOMETRYSHADERGEN_BUFFERSIZE 32768
#define GEOMETRYSHADERGEN_UID_VERSION 1
typedef ShaderUid<geometry_shader_uid_data> GeometryShaderUid;

void GenerateGeometryShaderCode(ShaderCode& object, const geometry_shader_uid_data& uid_data, API_TYPE ApiType);
void GetGeometryShaderUid(GeometryShaderUid& object, u32 primitive_type, const XFMemory &xfr, const u32 components);
