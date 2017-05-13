// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <stdarg.h>
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/XFMemory.h"
#include "VideoCommon/VideoCommon.h"
#include "ShaderGenCommon.h"
#include "LightingShaderGen.h"

// TODO should be reordered
#define SHADER_POSITION_ATTRIB  0
#define SHADER_POSMTX_ATTRIB    1
#define SHADER_NORM0_ATTRIB     2
#define SHADER_NORM1_ATTRIB     3
#define SHADER_NORM2_ATTRIB     4
#define SHADER_COLOR0_ATTRIB    5
#define SHADER_COLOR1_ATTRIB    6

#define SHADER_TEXTURE0_ATTRIB  8
#define SHADER_TEXTURE1_ATTRIB  9
#define SHADER_TEXTURE2_ATTRIB  10
#define SHADER_TEXTURE3_ATTRIB  11
#define SHADER_TEXTURE4_ATTRIB  12
#define SHADER_TEXTURE5_ATTRIB  13
#define SHADER_TEXTURE6_ATTRIB  14
#define SHADER_TEXTURE7_ATTRIB  15



// shader variables
#define I_PROJECTION            "cproj"
#define I_DEPTHPARAMS           "cDepth" // farZ, zRange, pixel center x, pixel center y
#define I_VIEWPARAMS            "cViewport" // half viewport width, half viewport height, 2 x PixelsizeX, 2 x PixelsizeY
#define I_MATERIALS             "cmtrl"
#define I_LIGHTS                "clights"
#define I_TEXMATRICES           "ctexmtx"
#define I_TRANSFORMMATRICES     "ctrmtx"
#define I_NORMALMATRICES        "cnmtx"
#define I_POSTTRANSFORMMATRICES "cpostmtx"
#define I_PLOFFSETPARAMS        "cPLOffset" // line/point offset for correct emulation 
#define I_PHONG                 "cphong"

#define C_PROJECTION            0
#define C_DEPTHPARAMS           (C_PROJECTION + 4)
#define C_VIEWPARAMS            (C_DEPTHPARAMS + 1)
#define C_MATERIALS             (C_VIEWPARAMS + 1)
#define C_LIGHTS                (C_MATERIALS + 4)
#define C_PHONG                 (C_LIGHTS + 40)
#define C_TEXMATRICES           (C_PHONG + 2)
#define C_TRANSFORMMATRICES     (C_TEXMATRICES + 24)
#define C_NORMALMATRICES        (C_TRANSFORMMATRICES + 64)
#define C_POSTTRANSFORMMATRICES (C_NORMALMATRICES + 32)
#define C_PLOFFSETPARAMS        (C_POSTTRANSFORMMATRICES + 64)
#define C_VENVCONST_END         (C_PLOFFSETPARAMS + 13)

#pragma pack(1)
struct vertex_shader_uid_data
{
	u32 NumValues() const
	{
		return sizeof(vertex_shader_uid_data);
	}
	u32 StartValue() const
	{
		return 0;
	}

	void ClearUnused(){}

	u32 components : 22;
	u32 numTexGens : 4;
	u32 numColorChans : 2;
	u32 dualTexTrans_enabled : 1;
	u32 pixel_lighting : 1;
	u32 msaa : 1;
	u32 ssaa : 1;

	u32 texMtxInfo_n_projection : 16; // Stored separately to guarantee that the texMtxInfo struct is 8 bits wide
	u32 pad0 : 16;

	struct
	{
		u8 index : 6;
		u8 normalize : 1;
		u8 pad : 1;
	} postMtxInfo[8];

	struct
	{
		u16 inputform : 2;
		u16 texgentype : 3;
		u16 sourcerow : 5;
		u16 embosssourceshift : 3;
		u16 embosslightshift : 3;
	} texMtxInfo[8];

	LightingUidData lighting;
};
#pragma pack()
#define VERTEXSHADERGEN_BUFFERSIZE 32768
#define VERTEXSHADERGEN_UID_VERSION 1
typedef ShaderUid<vertex_shader_uid_data> VertexShaderUid;

void GetVertexShaderUID(VertexShaderUid& object, u32 components, const XFMemory &xfr, const BPMemory &bpm);

void GenerateVertexShaderCodeD3D9(ShaderCode& object, const vertex_shader_uid_data& uid_data);

void GenerateVertexShaderCodeD3D11(ShaderCode& object, const vertex_shader_uid_data& uid_data);

void GenerateVertexShaderCodeGL(ShaderCode& object, const vertex_shader_uid_data& uid_data);

void GenerateVertexShaderCodeVulkan(ShaderCode& object, const vertex_shader_uid_data& uid_data);