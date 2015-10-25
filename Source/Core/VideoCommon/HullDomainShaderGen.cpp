// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>

#include "VideoCommon/HullDomainShaderGen.h"
#include "VideoCommon/LightingShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoConfig.h"
static char text[HULLDOMAINSHADERGEN_BUFFERSIZE];

static const char* s_hlsl_header_str = R"hlsl(
struct HSOutput
{
	float4 pos: BEZIERPOS;
};

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("TConstFunc")]
HSOutput HS_TFO(InputPatch<VS_OUTPUT, 3> patch, uint id : SV_OutputControlPointID,uint patchID : SV_PrimitiveID)
{
HSOutput result = (HSOutput)0;
return result;
}
)hlsl";

static const char* s_hlsl_constant_func_str = R"hlsl(
float GetScreenSize(float3 Origin, float Diameter)
{
    float w = dot()hlsl" I_PROJECTION R"hlsl([3], float4( Origin, 1.0 ));
    return abs(Diameter * )hlsl" I_PROJECTION R"hlsl([1].y / w);
}

float CalcTessFactor(float3 Control0, float3 Control1)
{
    float e0 = distance(Control0,Control1);
    float3 m0 = (Control0 + Control1)/2;
    return max()hlsl" I_TESSPARAMS ".x, " I_TESSPARAMS R"hlsl(.y * GetScreenSize(m0,e0));
}
ConstantOutput TConstFunc(InputPatch<VS_OUTPUT, 3> patch)
{
ConstantOutput result = (ConstantOutput)0;
)hlsl";

static const char* s_hlsl_ds_str = R"hlsl(
float3 PrjToPlane(float3 planeNormal, float3 planePoint, float3 pointToProject)
{
return pointToProject - dot(pointToProject-planePoint, planeNormal) * planeNormal;
}

float2 BInterpolate(float2 v0, float2 v1, float2 v2, float3 barycentric)
{
return barycentric.z * v0 + barycentric.x * v1 + barycentric.y * v2;
}

float2 BInterpolate(float2 v[3], float3 barycentric)
{
return BInterpolate(v[0], v[1], v[2], barycentric);
}

float3 BInterpolate(float3 v0, float3 v1, float3 v2, float3 barycentric)
{
return barycentric.z * v0 + barycentric.x * v1 + barycentric.y * v2;
}

float3 BInterpolate(float3 v[3], float3 barycentric)
{
return BInterpolate(v[0], v[1], v[2], barycentric);
}

float4 BInterpolate(float4 v0, float4 v1, float4 v2, float3 barycentric)
{
return barycentric.z * v0 + barycentric.x * v1 + barycentric.y * v2;
}

float4 BInterpolate(float4 v[3], float3 barycentric)
{
return BInterpolate(v[0], v[1], v[2], barycentric);
}

[domain("tri")]
VS_OUTPUT DS_TFO(ConstantOutput pconstans, const OutputPatch<HSOutput, 3> patch, float3 bCoords : SV_DomainLocation )
{
VS_OUTPUT result = (VS_OUTPUT)0;
)hlsl";

static const char* s_phong_str = R"hlsl(
pos0 = PrjToPlane(norm0, pos0, position);
pos1 = PrjToPlane(norm1, pos1, position);
pos2 = PrjToPlane(norm2, pos2, position);
position = lerp(position, BInterpolate(pos0, pos1, pos2, bCoords),)hlsl" I_TESSPARAMS ".zzz );";

template<class T, API_TYPE ApiType, bool is_writing_shadercode>
static inline void GenerateHullDomainShader(T& out, const XFMemory &xfr, const u32 components)
{
	// Non-uid template parameters will Write to the dummy data (=> gets optimized out)
	HullDomain_shader_uid_data dummy_data;
	bool uidPresent = (&out.template GetUidData<HullDomain_shader_uid_data>() != nullptr);
	HullDomain_shader_uid_data& uid_data = uidPresent ? out.template GetUidData<HullDomain_shader_uid_data>() : dummy_data;

	if (uidPresent)
	{
		out.ClearUID();
	}

	uid_data.numTexGens = xfr.numTexGen.numTexGens;
	bool normalpresent = (components & VB_HAS_NRM0) != 0;
	uid_data.normal = normalpresent;

	char* codebuffer = nullptr;
	if (is_writing_shadercode)
	{
		codebuffer = out.GetBuffer();
		if (codebuffer == nullptr)
		{
			codebuffer = text;
			out.SetBuffer(codebuffer);
		}
		codebuffer[sizeof(text) - 1] = 0x7C;  // canary
	}
	else
	{
		return;
	}

	// uniforms
	if (ApiType == API_OPENGL)
		out.Write("layout(std140%s) uniform GSBlock {\n", g_ActiveConfig.backend_info.bSupportsBindingLayout ? ", binding = 3" : "");
	else
		out.Write("cbuffer GSBlock {\n");
	out.Write(
		"\tfloat4 " I_TESSPARAMS";\n"
		"\tfloat4 " I_DEPTHPARAMS";\n"
		"\tfloat4 " I_PROJECTION"[4];\n"
		"};\n");

	out.Write("struct VS_OUTPUT {\n");
	GenerateVSOutputMembers<T, ApiType>(out, normalpresent, xfr);
	out.Write("};\n");

	if (ApiType == API_OPENGL)
	{

	}
	else // D3D
	{
		out.Write("struct ConstantOutput\n");
		out.Write("{\n");
		out.Write("float EFactor[3] : SV_TessFactor;\n");
		out.Write("float InsideFactor : SV_InsideTessFactor;\n");
		out.Write("float4 pos[3] : TEXCOORD1;\n");
		out.Write("float4 colors_0[3] : TEXCOORD4;\n");
		out.Write("float4 colors_1[3] : TEXCOORD7;\n");

		if (xfr.numTexGen.numTexGens < 7)
		{
			for (unsigned int i = 0; i < xfr.numTexGen.numTexGens; ++i)
				out.Write("float4 tex%d[3] : TEXCOORD%d;\n", i, i * 3 + 10);
			if (normalpresent)
			{
				out.Write("float4 Normal[3]: TEXCOORD%d;\n", xfr.numTexGen.numTexGens * 3 + 10);
			}
		}
		else
		{
			// Store clip position in the w component of first 4 texcoords
			for (int i = 0; i < 8; ++i)
				out.Write("float4 tex%d[3] : TEXCOORD%d;\n", i, i * 3 + 10);
		}
		out.Write("};\n");

		out.Write(s_hlsl_header_str);
		out.Write(s_hlsl_constant_func_str);
		out.Write("[unroll]\n");
		out.Write("for(int i = 0; i < 3; i++)\n{\n");
		if (xfr.numTexGen.numTexGens < 7)
		{
			out.Write("result.pos[i] = float4(patch[i].clipPos.x,patch[i].clipPos.y,patch[i].Normal.w, 1.0);\n");
		}
		else
		{
			out.Write("result.pos[i] = float4(patch[i].tex0.w, patch[i].tex1.w, patch[i].tex7.w, 1.0);\n");
		}

		out.Write("result.colors_0[i] = patch[i].colors_0;\n");
		out.Write("result.colors_1[i] = patch[i].colors_1;\n");
		if (xfr.numTexGen.numTexGens < 7)
		{
			for (unsigned int i = 0; i < xfr.numTexGen.numTexGens; ++i)
				out.Write("result.tex%d[i] = float4(patch[i].tex%d, 1.0);\n", i, i);
			if (normalpresent)
			{
				out.Write("result.Normal[i] = patch[i].Normal;\n");
			}
		}
		else
		{
			// Store clip position in the w component of first 4 texcoords
			for (int i = 0; i < 8; ++i)
				out.Write("result.tex%d[i] = patch[i].tex%d;\n", i, i);
		}
		out.Write("}\n");
		//out.Write("result.facenormal.xyz = normalize(cross(result.pos[2].xyz - result.pos[0].xyz, result.pos[1].xyz - result.pos[2].xyz));\n");
		out.Write("result.EFactor[0] = CalcTessFactor(result.pos[2].xyz, result.pos[0].xyz);\n");
		out.Write("result.EFactor[1] = CalcTessFactor(result.pos[0].xyz, result.pos[1].xyz);\n");
		out.Write("result.EFactor[2] = CalcTessFactor(result.pos[1].xyz, result.pos[2].xyz);\n");
		out.Write("result.InsideFactor = (result.EFactor[0] + result.EFactor[1] + result.EFactor[2]) / 3;\n");
		out.Write("return result;\n};\n");
		out.Write(s_hlsl_ds_str);
		out.Write("float3 pos0 = pconstans.pos[0].xyz;\n");
		out.Write("float3 pos1 = pconstans.pos[1].xyz;\n");
		out.Write("float3 pos2 = pconstans.pos[2].xyz;\n");
		out.Write("float3 position = BInterpolate(pos0, pos1, pos2, bCoords);\n");
		if (normalpresent)
		{
			if (xfr.numTexGen.numTexGens < 7)
			{
				out.Write("float3 norm0 = pconstans.Normal[0].xyz;\n");
				out.Write("float3 norm1 = pconstans.Normal[1].xyz;\n");
				out.Write("float3 norm2 = pconstans.Normal[2].xyz;\n");
			}
			else
			{
				out.Write("float3 norm0 = float3(pconstans.tex4[0].w, pconstans.tex5[0].w, pconstans.tex6[0].w);\n");
				out.Write("float3 norm1 = float3(pconstans.tex4[1].w, pconstans.tex5[1].w, pconstans.tex6[1].w);\n");
				out.Write("float3 norm2 = float3(pconstans.tex4[2].w, pconstans.tex5[2].w, pconstans.tex6[2].w);\n");
			}
			out.Write("float3 normal = BInterpolate(norm0, norm1, norm2, bCoords);\n");
			out.Write(s_phong_str);
		}
		// Transform world position to view-projection
		out.Write("float4 pos = float4(position, 1.0);\n");
		out.Write("result.pos = float4(dot(" I_PROJECTION "[0], pos), dot(" I_PROJECTION "[1], pos), dot(" I_PROJECTION "[2], pos), dot(" I_PROJECTION "[3], pos));\n");
		out.Write("result.pos.z = -((" I_DEPTHPARAMS".x - 1.0) * result.pos.w + result.pos.z * " I_DEPTHPARAMS".y);\n");
		out.Write("result.pos.xy = result.pos.xy + result.pos.w * " I_DEPTHPARAMS".zw;\n");
		out.Write("result.colors_0 = BInterpolate(pconstans.colors_0, bCoords);\n");
		out.Write("result.colors_1 = BInterpolate(pconstans.colors_1, bCoords);\n");
		if (xfr.numTexGen.numTexGens < 7)
		{
			for (unsigned int i = 0; i < xfr.numTexGen.numTexGens; ++i)
				out.Write("result.tex%d = BInterpolate(pconstans.tex%d, bCoords).xyz;\n", i, i, i, i);
			out.Write("result.clipPos = float4(position.xy, result.pos.zw);\n");
			if (normalpresent)
			{
				out.Write("result.Normal = float4(normal.xyz, position.z);\n");
			}
		}
		else
		{
			// Store clip position in the w component of first 4 texcoords
			for (int i = 0; i < 8; ++i)
				out.Write("result.tex%d = BInterpolate(pconstans.tex%d, bCoords);\n", i, i, i, i);
			out.Write("result.tex0.w = position.x;\n");
			out.Write("result.tex1.w = position.y;\n");
			if (normalpresent)
			{
				out.Write("result.tex4.w = normal.x;\n");
				out.Write("result.tex5.w = normal.y;\n");
				out.Write("result.tex6.w = normal.z;\n");
			}

			if (xfr.numTexGen.numTexGens < 8)
				out.Write("result.tex7 = position.xyzz;\n");
			else
				out.Write("result.tex7.w = position.z;\n");
		}
		out.Write("return result;\n}");
	}

	if (is_writing_shadercode)
	{
		if (codebuffer[HULLDOMAINSHADERGEN_BUFFERSIZE - 1] != 0x7C)
			PanicAlert("GeometryShader generator - buffer too small, canary has been eaten!");
	}
}

void GenerateHullDomainShaderCode(ShaderCode& object, API_TYPE ApiType, const XFMemory &xfr, const u32 components)
{
	if (ApiType == API_OPENGL)
	{
		GenerateHullDomainShader<ShaderCode, API_OPENGL, true>(object, xfr, components);
	}
	else
	{
		GenerateHullDomainShader<ShaderCode, API_D3D11, true>(object, xfr, components);
	}
}

void GetHullDomainShaderUid(HullDomainShaderUid& object, API_TYPE ApiType, const XFMemory &xfr, const u32 components)
{
	if (ApiType == API_OPENGL)
	{
		GenerateHullDomainShader<HullDomainShaderUid, API_OPENGL, false>(object, xfr, components);
	}
	else
	{
		GenerateHullDomainShader<HullDomainShaderUid, API_D3D11, false>(object, xfr, components);
	}
}
