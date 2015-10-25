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
struct HullDomain_shader_uid_data
{
	u32 NumValues() const { return sizeof(HullDomain_shader_uid_data); }
	u32 StartValue() const { return 0; }

	u32 numTexGens : 4;
	u32 normal : 1;
	u32 padding : 28;
};

#pragma pack()

#define I_TESSPARAMS    "ctess"
#define I_PROJECTION    "cproj"
#define I_DEPTHPARAMS   "cDepth" // farZ, zRange, scaled viewport width, scaled viewport height

#define HULLDOMAINSHADERGEN_BUFFERSIZE 32768
typedef ShaderUid<HullDomain_shader_uid_data> HullDomainShaderUid;

void GenerateHullDomainShaderCode(ShaderCode& object, API_TYPE ApiType, const XFMemory &xfr, const u32 components);
void GetHullDomainShaderUid(HullDomainShaderUid& object, API_TYPE ApiType, const XFMemory &xfr, const u32 components);
