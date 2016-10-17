// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/XFMemory.h"


#define LIGHT_COL "%s[5*%d].%s"
#define LIGHT_COL_PARAMS(lightsName, index, swizzle) (lightsName), (index), (swizzle)

#define LIGHT_COSATT "%s[5*%d+1]"
#define LIGHT_COSATT_PARAMS(lightsName, index) (lightsName), (index)

#define LIGHT_DISTATT "%s[5*%d+2]"
#define LIGHT_DISTATT_PARAMS(lightsName, index) (lightsName), (index)

#define LIGHT_POS "%s[5*%d+3]"
#define LIGHT_POS_PARAMS(lightsName, index) (lightsName), (index)

#define LIGHT_DIR "%s[5*%d+4]"
#define LIGHT_DIR_PARAMS(lightsName, index) (lightsName), (index)

/**
* Common uid data used for shader generators that use lighting calculations.
*/
#pragma pack(1)
struct LightingUidData
{
	u32 matsource : 4; // 4x1 bit
	u32 enablelighting : 4; // 4x1 bit
	u32 ambsource : 4; // 4x1 bit
	u32 pad0 : 4;
	u32 diffusefunc : 8; // 4x2 bits
	u32 attnfunc : 8; // 4x2 bits	
	u32 light_mask : 32; // 4x8 bits
};
#pragma pack()

inline void GenerateLightShader(ShaderCode& object,
	const LightingUidData& uid_data,
	int index,
	int litchan_index,
	const char* lightsName,
	bool alpha,
	bool forcephong = false)
{
	const char* swizzle = alpha ? "a" : "rgb";

	object.Write("ldir = " LIGHT_POS".xyz - pos.xyz;\n",
		LIGHT_POS_PARAMS(lightsName, index));

	int attnfunc = (uid_data.attnfunc >> (2 * litchan_index)) & 0x3;
	int diffusefunc = (uid_data.diffusefunc >> (2 * litchan_index)) & 0x3;

	switch (attnfunc)
	{
	case LIGHTATTN_NONE:
	case LIGHTATTN_DIR:
		object.Write("ldir = normalize(ldir);\n");
		object.Write("attn = 1.0f;\n");
		object.Write("if (length(ldir) == 0.0)\n\t ldir = _norm0;\n");
		break;
	case LIGHTATTN_SPEC:
		object.Write("ldir = normalize(ldir);\n");
		object.Write("attn = (dot(_norm0, ldir) >= 0.0) ? max(0.0, dot(_norm0, " LIGHT_DIR".xyz)) : 0.0;\n", LIGHT_DIR_PARAMS(lightsName, index));
		object.Write("attn = max(0.0f, dot(" LIGHT_COSATT".xyz, float3(1.0, attn, attn*attn))) / dot(%s(" LIGHT_DISTATT".xyz), float3(1.0, attn, attn*attn));\n",
			LIGHT_COSATT_PARAMS(lightsName, index),
			(diffusefunc == LIGHTDIF_NONE) ? "" : "normalize",
			LIGHT_DISTATT_PARAMS(lightsName, index));
		break;
	case LIGHTATTN_SPOT:
		object.Write("dist2 = dot(ldir, ldir);\n"
			"dist = sqrt(dist2);\n"
			"ldir = ldir / dist;\n"
			"attn = max(0.0, dot(ldir, " LIGHT_DIR".xyz));\n", LIGHT_DIR_PARAMS(lightsName, index));
		// attn*attn may overflow
		object.Write("attn = max(0.0, dot(" LIGHT_COSATT".xyz, float3(1.0, attn, attn*attn))) / dot(" LIGHT_DISTATT".xyz, float3(1.0,dist,dist2));\n",
			LIGHT_COSATT_PARAMS(lightsName, index), LIGHT_DISTATT_PARAMS(lightsName, index));
		break;
	}

	switch (diffusefunc)
	{
	case LIGHTDIF_NONE:
		object.Write("lacc.%s += round(attn * " LIGHT_COL");\n", swizzle, LIGHT_COL_PARAMS(lightsName, index, swizzle));
		break;
	case LIGHTDIF_SIGN:
	case LIGHTDIF_CLAMP:
		object.Write("lacc.%s += round(attn * %sdot(ldir, _norm0)) * " LIGHT_COL");\n",
			swizzle,
			diffusefunc != LIGHTDIF_SIGN ? "max(0.0," : "(",
			LIGHT_COL_PARAMS(lightsName, index, swizzle));
		if (forcephong)
		{
			object.Write("spec.%s += attn * pow(saturate(dot(normalize(ldir + View),_norm0)), 4.0 + 60.0 * normalmap.w) * " LIGHT_COL";\n",
				swizzle,
				LIGHT_COL_PARAMS(lightsName, index, swizzle));
		}
		break;
	default: _assert_(0);
	}
	object.Write("\n");
}

// vertex shader
// lights/colors
// materials name is I_MATERIALS in vs and I_PMATERIALS in ps
// inColorName is color in vs and colors_ in ps
// dest is o.colors_ in vs and colors_ in ps
inline void GenerateLightingShaderCode(ShaderCode& object, u32 numColorChans, const LightingUidData& uid_data, int components, const char* materialsName, const char* lightsName, const char* inColorName, const char* dest, bool use_integer_math, bool forcephong = false)
{
	for (unsigned int j = 0; j < numColorChans; j++)
	{
		bool colormatsource = !!(uid_data.matsource & (1 << j));
		object.Write("{\n");
		if (colormatsource) // from vertex
		{
			if (components & (VB_HAS_COL0 << j))
				object.Write("mat = round(%s%d * 255.0);\n", inColorName, j);
			else if (components & VB_HAS_COL0)
				object.Write("mat = round(%s0 * 255.0);\n", inColorName);
			else
				object.Write("mat = float4(255.0,255.0,255.0,255.0);\n");
		}
		else // from color
		{
			object.Write("mat = %s[%d];\n", materialsName, j + 2);
		}
		if (uid_data.enablelighting & (1 << j))
		{
			if (uid_data.ambsource & (1 << j)) // from vertex
			{
				if (components & (VB_HAS_COL0 << j))
					object.Write("lacc = round(%s%d * 255.0);\n", inColorName, j);
				else if (components & VB_HAS_COL0)
					object.Write("lacc = round(%s0 * 255.0);\n", inColorName);
				else
					// TODO: this isn't verified. Here we want to read the ambient from the vertex,
					// but the vertex itself has no color. So we don't know which value to read.
					// Returing 0.0 is Required in SMS to make Debug boxes invisible in the Bonus level
					object.Write("lacc = float4(255.0,255.0,255.0,255.0);\n");
			}
			else // from color
			{
				object.Write("lacc = %s[%d];\n", materialsName, j);
			}
		}
		else
		{
			object.Write("lacc = float4(255.0,255.0,255.0,255.0);\n");
		}

		// check if alpha is different
		bool alphamatsource = !!(uid_data.matsource & (1 << (j + 2)));
		if (alphamatsource != colormatsource)
		{
			if (alphamatsource) // from vertex
			{
				if (components & (VB_HAS_COL0 << j))
					object.Write("mat.w = round(%s%d.w * 255.0);\n", inColorName, j);
				else if (components & VB_HAS_COL0)
					object.Write("mat.w = round(%s0.w * 255.0);\n", inColorName);
				else object.Write("mat.w = 255.0;\n");
			}
			else // from color
			{
				object.Write("mat.w = %s[%d].w;\n", materialsName, j + 2);
			}
		}


		if (uid_data.enablelighting & (1 << (j + 2)))
		{
			if (uid_data.ambsource & (1 << (j + 2))) // from vertex
			{
				if (components & (VB_HAS_COL0 << j))
					object.Write("lacc.w = round(%s%d.w * 255.0);\n", inColorName, j);
				else if (components & VB_HAS_COL0)
					object.Write("lacc.w = round(%s0.w * 255.0);\n", inColorName);
				else
					// TODO: The same for alpha: We want to read from vertex, but the vertex has no color
					object.Write("lacc.w = 255.0;\n");
			}
			else // from color
			{
				object.Write("lacc.w = %s[%d].w;\n", materialsName, j);
			}
		}
		else
		{
			object.Write("lacc.w = 255.0;\n");
		}
		if ((uid_data.enablelighting & (1 << j)))
		{
			for (int i = 0; i < 8; ++i)
				if (uid_data.light_mask & (1 << (i + 8 * j)))
					GenerateLightShader(object, uid_data, i, j, lightsName, false, forcephong);
		}
		if (uid_data.enablelighting & (1 << (j + 2)))
		{
			for (int i = 0; i < 8; ++i)
				if (uid_data.light_mask & (1 << (i + 8 * (j + 2))))
					GenerateLightShader(object, uid_data, i, j + 2, lightsName, true, forcephong);
		}
		if (use_integer_math && !forcephong)
		{
			object.Write("ilacc = int4(lacc);\n");
			object.Write("ilacc = clamp(ilacc, 0, 255);\n");
			object.Write("ilacc += ilacc >> 7;\n");
			object.Write("%s%d = float4((int4(mat) * ilacc) >> 8) / 255.0;\n", dest, j);
		}
		else
		{
			object.Write("lacc = clamp(lacc, 0.0, 255.0);\n");
			object.Write("lacc = lacc + floor(lacc / 128.0);\n");
			object.Write("%s%d = floor((mat * lacc)/256.0)/255.0;\n", dest, j);
		}
		object.Write("}\n");
	}
}

inline void GetLightingShaderUid(LightingUidData& uid_data, const XFMemory &xfr)
{
	for (unsigned int j = 0; j < xfr.numChan.numColorChans; j++)
	{
		const LitChannel& color = xfr.color[j];
		const LitChannel& alpha = xfr.alpha[j];
		uid_data.matsource |= color.matsource << j;
		uid_data.matsource |= alpha.matsource << (j + 2);
		uid_data.enablelighting |= color.enablelighting << j;
		uid_data.enablelighting |= alpha.enablelighting << (j + 2);

		if (uid_data.enablelighting & (1 << j)) // Color lights
		{
			uid_data.ambsource |= color.ambsource << j;
			uid_data.attnfunc |= color.attnfunc << (2 * j);
			uid_data.diffusefunc |= color.diffusefunc << (2 * j);
			uid_data.light_mask |= color.GetFullLightMask() << (8 * j);
		}
		if (uid_data.enablelighting & (1 << (j + 2))) // Alpha lights
		{
			uid_data.ambsource |= alpha.ambsource << (j + 2);
			uid_data.attnfunc |= alpha.attnfunc << (2 * (j + 2));
			uid_data.diffusefunc |= alpha.diffusefunc << (2 * (j + 2));
			uid_data.light_mask |= alpha.GetFullLightMask() << (8 * (j + 2));
		}
	}
}
