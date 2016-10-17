// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/LightingShaderGen.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/VideoCommon.h"

#define I_COLORS      "color"
#define I_KCOLORS     "k"
#define I_ALPHA       "alphaRef"
#define I_TEXDIMS     "texdim"
#define I_ZBIAS       "czbias"
#define I_INDTEXSCALE "cindscale"
#define I_INDTEXMTX   "cindmtx"
#define I_FOGCOLOR    "cfogcolor"
#define I_FOGI        "cfogi"
#define I_FOGF        "cfogf"
#define I_ZSLOPE      "czslope"
#define I_FLAGS       "cflags"
#define I_EFBSCALE    "cefbscale"
#define I_PPHONG      "cphong"

#define C_COLORMATRIX   0                   // 0
#define C_COLORS        0                   // 0
#define C_KCOLORS       (C_COLORS + 4)      // 4
#define C_ALPHA         (C_KCOLORS + 4)     // 8
#define C_TEXDIMS       (C_ALPHA + 1)       // 9
#define C_ZBIAS         (C_TEXDIMS + 8)     //17
#define C_INDTEXSCALE   (C_ZBIAS + 2)       //19
#define C_INDTEXMTX     (C_INDTEXSCALE + 2) //21
#define C_FOGCOLOR      (C_INDTEXMTX + 6)   //27
#define C_FOGI          (C_FOGCOLOR + 1)    //28
#define C_FOGF          (C_FOGI + 1)        //29
#define C_ZSLOPE        (C_FOGF + 2)        //31
#define C_FLAGS         (C_ZSLOPE + 1)      //32
#define C_EFBSCALE      (C_FLAGS + 1)       //33

#define C_PMATERIALS    (C_EFBSCALE + 1)
#define C_PLIGHTS       (C_PMATERIALS + 4)
#define C_PPHONG        (C_PLIGHTS + 40)
#define C_PENVCONST_END (C_PPHONG + 2)
#define C_PCONST_END    (C_PMATERIALS)
// Different ways to achieve rendering with destination alpha
enum PIXEL_SHADER_RENDER_MODE : unsigned int
{
	PSRM_DEFAULT = 0, // Render normally, without destination alpha
	PSRM_ALPHA_PASS = 1, // Render normally first, then render again for alpha
	PSRM_DUAL_SOURCE_BLEND = 2, // Use dual-source blending,
	PSRM_DEPTH_ONLY = 3, // Depth only Rendering
};

#pragma pack(1)
union stage_hash_data
{
	struct
	{
		// Align Everything to 32 bits words to speed up things
		u32 cc : 24;
		u32 tevorders_colorchan : 3;
		u32 pad0 : 5;
		u32 ac : 24;
		u32 pad1 : 8;

		u32 tevorders_enable : 1;
		u32 tevorders_texmap : 3;
		u32 tevorders_texcoord : 3;
		u32 hasindstage : 1;
		u32 pad2 : 3;
		u32 tevind : 21;

		// TODO: Clean up the swapXY mess
		u32 tevksel_swap1a : 2;
		u32 tevksel_swap2a : 2;
		u32 tevksel_swap1b : 2;
		u32 tevksel_swap2b : 2;
		u32 tevksel_swap1c : 2;
		u32 tevksel_swap2c : 2;
		u32 tevksel_swap1d : 2;
		u32 tevksel_swap2d : 2;
		u32 tevksel_kc : 8; // 3 bits padded to aling data
		u32 tevksel_ka : 8; // 3 bits padded to aling data
	};
	u32 hex[4];
};

struct pixel_shader_uid_data
{
	u32 NumValues() const
	{
		return sizeof(pixel_shader_uid_data) - (sizeof(stage_hash_data) * (16 - (genMode_numtevstages + 1)));
	}
	u32 StartValue() const
	{
		return pixel_lighting ? 0 : sizeof(LightingUidData);
	}

	void ClearUnused()
	{
		texMtxInfo_n_projection = 0;
	}

	// TODO: Optimize field order for easy access!
	LightingUidData lighting;

	u32 components : 2;
	u32 zfreeze : 1;
	u32 pixel_lighting : 2;
	u32 pixel_normals : 1;
	u32 stereo : 1;
	u32 msaa : 1;
	u32 ssaa : 1;
	u32 numColorChans : 2;
	u32 late_ztest : 1;
	u32 rgba6_format : 1;
	u32 dither : 1;
	u32 pad0 : 10;

	u32 render_mode : 2;
	u32 Pretest : 2;
	u32 nIndirectStagesUsed : 4;

	u32 genMode_numtexgens : 4;
	u32 genMode_numtevstages : 4;

	u32 genMode_numindstages : 3;
	u32 alpha_test_logic : 2;
	u32 alpha_test_comp0 : 3;

	u32 alpha_test_comp1 : 3;
	u32 alpha_test_use_zcomploc_hack : 1;
	u32 fog_proj : 1;
	u32 fog_fsel : 3;

	u32 fog_RangeBaseEnabled : 1;
	u32 ztex_op : 2;
	u32 fast_depth_calc : 1;
	u32 per_pixel_depth : 1;
	u32 forced_early_z : 1;
	u32 early_ztest : 1;
	u32 bounding_box : 1;

	u32 texMtxInfo_n_projection : 8; // 8x1 bit
	u32 tevindref_bi0 : 3;
	u32 tevindref_bc0 : 3;
	u32 tevindref_bi1 : 3;
	u32 tevindref_bc1 : 3;
	u32 tevindref_bi2 : 3;
	u32 tevindref_bc3 : 3;
	u32 tevindref_bi4 : 3;
	u32 tevindref_bc4 : 3;
	stage_hash_data stagehash[16];

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
#define PIXELSHADERGEN_BUFFERSIZE 32768
#define PIXELSHADERGEN_UID_VERSION 1
typedef ShaderUid<pixel_shader_uid_data> PixelShaderUid;

void GetPixelShaderUID(PixelShaderUid& object, PIXEL_SHADER_RENDER_MODE render_mode, u32 components, const XFMemory &xfr, const BPMemory &bpm);

void GeneratePixelShaderCodeD3D9(ShaderCode& object, const pixel_shader_uid_data& uid_data);

void GeneratePixelShaderCodeD3D9SM2(ShaderCode& object, const pixel_shader_uid_data& uid_data);

void GeneratePixelShaderCodeD3D11(ShaderCode& object, const pixel_shader_uid_data& uid_data);

void GeneratePixelShaderCodeGL(ShaderCode& object, const pixel_shader_uid_data& uid_data);

void GeneratePixelShaderCodeVulkan(ShaderCode& object, const pixel_shader_uid_data& uid_data);
