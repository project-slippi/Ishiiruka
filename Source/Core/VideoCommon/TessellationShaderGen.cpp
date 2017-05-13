// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>

#include "VideoCommon/TessellationShaderGen.h"
#include "VideoCommon/LightingShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/VideoConfig.h"
static char text[TESSELLATIONSHADERGEN_BUFFERSIZE];

static const char* headerUtilI = R"hlsl(
int4 CHK_O_U8(int4 x)
{
	return x & 255;
}
#define BOR(x, n) ((x) | (n))
#define BSHR(x, n) ((x) >> (n))
int2 BSH(int2 x, int n)
{
	if(n >= 0)
	{
		return x >> n;
	}
	else
	{
		return x << (-n);
	}
}
int remainder(int x, int y)
{
	return x %% y;
}
// dot product for integer vectors
int idot(int3 x, int3 y)
{
	int3 tmp = x * y;
	return tmp.x + tmp.y + tmp.z;
}
int idot(int4 x, int4 y)
{
	int4 tmp = x * y;
	return tmp.x + tmp.y + tmp.z + tmp.w;
}
// rounding + casting to integer at once in a single function
int  wuround(float  x) { return int(round(x)); }
int2 wuround(float2 x) { return int2(round(x)); }
int3 wuround(float3 x) { return int3(round(x)); }
int4 wuround(float4 x) { return int4(round(x)); }
)hlsl";

static const char* s_hlsl_hull_header_str = R"hlsl(
struct HSOutput
{
	float4 pos: BEZIERPOS;
	float4 colors_0: COLOR0;
	float4 colors_1: COLOR1;
	float2 clipDist: TANGENT;
};

[domain("tri")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("TConstFunc")]
HSOutput HS_TFO(InputPatch<VS_OUTPUT, 3> patch, uint id : SV_OutputControlPointID,uint patchID : SV_PrimitiveID)
{
HSOutput result = (HSOutput)0;
)hlsl";
static const char* s_hlsl_constant_header_str = R"hlsl(

float GetScreenSize(float3 Origin, float Diameter)
{
    float w = dot()hlsl" I_PROJECTION R"hlsl([3], float4( Origin, 1.0 ));
    return abs(Diameter * )hlsl" I_PROJECTION R"hlsl([1].y / w);
}

float CalcTessFactor(float3 Origin, float Diameter)
{
	float distance = 1.0 - saturate(length(Origin) * )hlsl" I_TESSPARAMS R"hlsl(.x);
	distance = distance * distance;
	return round(max(4.0,)hlsl" I_TESSPARAMS R"hlsl(.y * GetScreenSize(Origin,Diameter) * distance));
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

float BInterpolate(float v0, float v1, float v2, float3 barycentric)
{
return barycentric.z * v0 + barycentric.x * v1 + barycentric.y * v2;
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



template<API_TYPE ApiType>
void SampleTextureRAW(ShaderCode& out, const char *texcoords, const char *texswap, const char *layer, int texmap, int tcoord)
{
	if (ApiType == API_D3D11)
	{
		out.Write("Tex[%d].SampleLevel(samp[%d], float3(%s.xy * " I_TEXDIMS"[%d].xy, %s), round(log2(1.0 / " I_TEXDIMS "[%d].x)) * uv[%d].w).%s;\n", 8 + texmap, texmap, texcoords, texmap, layer, texmap, tcoord, texswap);
	}
	else
	{
		out.Write("texture(samp[%d],float3(%s.xy * " I_TEXDIMS"[%d].xy, %s)).%s;\n", 8 + texmap, texcoords, texmap, layer, texswap);
	}
}

template<API_TYPE ApiType>
void SampleTexture(ShaderCode& out, const char *texcoords, const char *texswap, int texmap, bool stereo)
{
	if (ApiType == API_D3D11)
	{
		out.Write("wuround((Tex[%d].SampleLevel(samp[%d], float3(%s.xy * " I_TEXDIMS"[%d].xy, %s), 0.0)).%s * 255.0);\n", texmap, texmap, texcoords, texmap, stereo ? "layer" : "0.0", texswap);
	}
	else if (ApiType == API_OPENGL)
	{
		out.Write("wuround(texture(samp[%d],float3(%s.xy * " I_TEXDIMS"[%d].xy, %s)).%s * 255.0);\n", texmap, texcoords, texmap, stereo ? "layer" : "0.0", texswap);
	}
	else
	{
		out.Write("wuround(tex2D(samp[%d],%s.xy * " I_TEXDIMS"[%d].xy).%s * 255.0);\n", texmap, texcoords, texmap, texswap);
	}
}

static inline void WriteStageUID(Tessellation_shader_uid_data& uid_data, int n, const BPMemory &bpm)
{
	int texcoord = bpm.tevorders[n / 2].getTexCoord(n & 1);
	bool bHasTexCoord = (u32)texcoord < bpm.genMode.numtexgens.Value();
	bool bHasIndStage = bpm.tevind[n].bt < bpm.genMode.numindstages.Value();
	const TevStageCombiner::ColorCombiner &cc = bpm.combiners[n].colorC;
	const TevStageCombiner::AlphaCombiner &ac = bpm.combiners[n].alphaC;
	// HACK to handle cases where the tex gen is not enabled
	if (!bHasTexCoord)
		texcoord = bpm.genMode.numtexgens;
	uid_data.stagehash[n].hasindstage = bHasIndStage;
	uid_data.stagehash[n].tevorders_texcoord = texcoord;
	if (bHasIndStage)
	{
		uid_data.stagehash[n].tevind = bpm.tevind[n].hex;
	}
	uid_data.stagehash[n].tevorders_enable = bpm.tevorders[n / 2].getEnable(n & 1)
		&& (cc.UsedAsInput(TEVCOLORARG_TEXC) || cc.UsedAsInput(TEVCOLORARG_TEXA) || ac.UsedAsInput(TEVALPHAARG_TEXA));
	if (bpm.tevorders[n / 2].getEnable(n & 1))
	{
		int texmap = bpm.tevorders[n / 2].getTexMap(n & 1);
		uid_data.stagehash[n].tevorders_texmap = texmap;
	}
}

void GetTessellationShaderUID(TessellationShaderUid& out, const XFMemory& xfr, const BPMemory& bpm, const u32 components)
{
	Tessellation_shader_uid_data& uid_data = out.GetUidData<Tessellation_shader_uid_data>();
	out.ClearUID();
	u32 numStages = bpm.genMode.numtevstages.Value() + 1;
	u32 numTexgen = xfr.numTexGen.numTexGens;
	u32 numindStages = bpm.genMode.numindstages.Value();
	bool normalpresent = (components & VB_HAS_NRM0) != 0;
	uid_data.numTexGens = numTexgen;
	uid_data.normal = normalpresent;
	uid_data.genMode_numtevstages = bpm.genMode.numtevstages;
	uid_data.genMode_numindstages = numindStages;
	uid_data.msaa = g_ActiveConfig.iMultisamples > 1;
	uid_data.ssaa = g_ActiveConfig.iMultisamples > 1 && g_ActiveConfig.bSSAA;
	int nIndirectStagesUsed = 0;
	if (numindStages > 0)
	{
		for (u32 i = 0; i < numStages; ++i)
		{
			if (bpm.tevind[i].IsActive() && bpm.tevind[i].bt < numindStages)
				nIndirectStagesUsed |= 1 << bpm.tevind[i].bt;
		}
	}
	uid_data.nIndirectStagesUsed = nIndirectStagesUsed;
	uid_data.stereo = g_ActiveConfig.iStereoMode > 0;
	bool enable_pl = g_ActiveConfig.PixelLightingEnabled(xfr, components) || g_ActiveConfig.bForcedLighting;
	uid_data.pixel_lighting = enable_pl;
	bool enablenormalmaps = g_ActiveConfig.HiresMaterialMapsEnabled();
	if (enablenormalmaps)
	{
		enablenormalmaps = false;
		for (u32 i = 0; i < numStages; ++i)
		{
			if (bpm.tevorders[i / 2].getEnable(i & 1))
			{
				enablenormalmaps = true;
				break;
			}
		}
	}
	uid_data.pixel_normals = enablenormalmaps ? 1 : 0;
	if (enablenormalmaps)
	{
		for (u32 i = 0; i < numindStages; ++i)
		{
			if (nIndirectStagesUsed & (1 << i))
			{
				u32 texcoord = bpm.tevindref.getTexCoord(i);
				u32 texmap = bpm.tevindref.getTexMap(i);
				uid_data.SetTevindrefValues(i, texcoord, texmap);
			}
		}
		for (u32 i = 0; i < numStages; ++i)
		{
			WriteStageUID(uid_data, i, bpm); // Fetch Texture data
		}
	}
	out.CalculateUIDHash();
}

template<API_TYPE ApiType>
inline void WriteFetchDisplacement(ShaderCode& out, int n, const Tessellation_shader_uid_data &uid_data)
{
	const auto& stage = uid_data.stagehash[n];
	u32 texcoord = stage.tevorders_texcoord;
	bool bHasTexCoord = texcoord < uid_data.numTexGens;
	bool bHasIndStage = stage.hasindstage;

	TevStageIndirect tevind;
	tevind.hex = stage.tevind;
	int texmap = stage.tevorders_texmap;
	out.Write("\n{\n");
	if (bHasIndStage)
	{
		out.Write("// indirect op\n");
		// perform the indirect op on the incoming regular coordinates using indtex%d as the offset coords
		if (tevind.mid != 0)
		{
			static const char *tevIndFmtMask[] = { "255", "31", "15", "7" };
			out.Write("int3 indtevcrd%d = indtex%d & %s;\n", n, tevind.bt, tevIndFmtMask[tevind.fmt]);

			static const char *tevIndBiasField[] = { "", "x", "y", "xy", "z", "xz", "yz", "xyz" }; // indexed by bias
			static const char *tevIndBiasAdd[] = { "int(-128)", "int(1)", "int(1)", "int(1)" }; // indexed by fmt
																															// bias
			if (tevind.bias == ITB_S || tevind.bias == ITB_T || tevind.bias == ITB_U)
				out.Write("indtevcrd%d.%s += %s;\n", n, tevIndBiasField[tevind.bias], tevIndBiasAdd[tevind.fmt]);
			else if (tevind.bias == ITB_ST || tevind.bias == ITB_SU || tevind.bias == ITB_TU)
				out.Write("indtevcrd%d.%s += int2(%s, %s);\n", n, tevIndBiasField[tevind.bias], tevIndBiasAdd[tevind.fmt], tevIndBiasAdd[tevind.fmt]);
			else if (tevind.bias == ITB_STU)
				out.Write("indtevcrd%d.%s += int3(%s, %s, %s);\n", n, tevIndBiasField[tevind.bias], tevIndBiasAdd[tevind.fmt], tevIndBiasAdd[tevind.fmt], tevIndBiasAdd[tevind.fmt]);

			// multiply by offset matrix and scale
			if (tevind.mid <= 3)
			{
				int mtxidx = 2 * (tevind.mid - 1);
				out.Write("int2 indtevtrans%d = int2(idot(" I_INDTEXMTX "[%d].xyz, indtevcrd%d), idot(" I_INDTEXMTX "[%d].xyz, indtevcrd%d));\n",
					n, mtxidx, n, mtxidx + 1, n);

				out.Write("indtevtrans%d = BSHR(indtevtrans%d, int(3));\n", n, n);
				out.Write("indtevtrans%d = BSH(indtevtrans%d, " I_INDTEXMTX "[%d].w);\n", n, n, mtxidx);
			}
			else if (tevind.mid <= 7 && bHasTexCoord)
			{ // s matrix
				_assert_(tevind.mid >= 5);
				int mtxidx = 2 * (tevind.mid - 5);
				out.Write("int2 indtevtrans%d = int2(uv[%d].xy * indtevcrd%d.xx);\n", n, texcoord, n);
				out.Write("indtevtrans%d = BSHR(indtevtrans%d, int(8));\n", n, n);
				out.Write("indtevtrans%d = BSH(indtevtrans%d, " I_INDTEXMTX "[%d].w);\n", n, n, mtxidx);
			}
			else if (tevind.mid <= 11 && bHasTexCoord)
			{ // t matrix
				_assert_(tevind.mid >= 9);
				int mtxidx = 2 * (tevind.mid - 9);
				out.Write("int2 indtevtrans%d = int2(uv[%d].xy * indtevcrd%d.yy);\n", n, texcoord, n);
				out.Write("indtevtrans%d = BSHR(indtevtrans%d, int(8));\n", n, n);
				out.Write("indtevtrans%d = BSH(indtevtrans%d, " I_INDTEXMTX "[%d].w);\n", n, n, mtxidx);
			}
			else
			{
				out.Write("int2 indtevtrans%d = int2(0,0);\n", n);
			}
		}
		else
		{
			out.Write("int2 indtevtrans%d = int2(0,0);\n", n);
		}
		// ---------
		// Wrapping
		// ---------
		static const char *tevIndWrapStart[] = { "int(0)", "int(256*128)", "int(128*128)", "int(64*128)", "int(32*128)", "int(16*128)", "int(1)", "int(1)" };
		// wrap S
		if (tevind.sw == ITW_OFF)
			out.Write("wrappedcoord.x = int(uv[%d].x);\n", texcoord);
		else if (tevind.sw == ITW_0)
			out.Write("wrappedcoord.x = int(0);\n");
		else
			out.Write("wrappedcoord.x = remainder(int(uv[%d].x), %s);\n", texcoord, tevIndWrapStart[tevind.sw]);

		// wrap T
		if (tevind.tw == ITW_OFF)
			out.Write("wrappedcoord.y = int(uv[%d].y);\n", texcoord);
		else if (tevind.tw == ITW_0)
			out.Write("wrappedcoord.y = int(0);\n");
		else
			out.Write("wrappedcoord.y = remainder(int(uv[%d].y), %s);\n", texcoord, tevIndWrapStart[tevind.tw]);

		if (tevind.fb_addprev) // add previous tevcoord
			out.Write("tevcoord.xy += wrappedcoord + indtevtrans%d;\n", n);
		else
			out.Write("tevcoord.xy = wrappedcoord + indtevtrans%d;\n", n);

		// Emulate s24 overflows
		out.Write("tevcoord.xy = (tevcoord.xy << 8) >> 8;\n");
	}
	if (stage.tevorders_enable)
	{
		if (!bHasIndStage)
		{
			for (int i = 0; i < n; i++)
			{
				if (stage.hex == uid_data.stagehash[i].hex)
				{
					out.Write("}\n");
					return;
				}
			}
			// calc tevcord
			if (bHasTexCoord)
				out.Write("tevcoord.xy = int2(uv[%d].xy);\n", texcoord);
			else
				out.Write("tevcoord.xy = int2(0,0);\n");
		}
		out.Write("if((" I_FLAGS ".x & %i) != 0)\n{\n", 1 << texmap);
		out.Write("float2 stagecoord = float2(tevcoord.xy);\n");
		out.Write("float bump = ");
		SampleTextureRAW<ApiType>(out, "(stagecoord)", "b", "0.0", texmap, texcoord);
		out.Write("bump = (bump * 255.0/127.0 - 128.0/127.0) * uv[%d].z;\n", texcoord);
		out.Write("displacement = displacement * displacementcount + bump;\n");
		// finalize Running average
		out.Write("displacementcount+=1.0;");
		out.Write("displacement = displacement / displacementcount;\n");
		out.Write("}\n");
	}
	out.Write("}\n");
}

template<API_TYPE ApiType>
inline void GenerateTessellationShader(ShaderCode& out, const Tessellation_shader_uid_data& uid_data)
{
	// Non-uid template parameters will Write to the dummy data (=> gets optimized out)
	u32 numStages = uid_data.genMode_numtevstages + 1;
	u32 numTexgen = uid_data.numTexGens;
	u32 numindStages = uid_data.genMode_numindstages;
	bool normalpresent = uid_data.normal;
	bool enablenormalmaps = uid_data.pixel_normals;
	int nIndirectStagesUsed = uid_data.nIndirectStagesUsed;
	char* codebuffer = nullptr;
	codebuffer = out.GetBuffer();
	if (codebuffer == nullptr)
	{
		codebuffer = text;
		out.SetBuffer(codebuffer);
	}
	codebuffer[sizeof(text) - 1] = 0x7C;  // canary
	if (enablenormalmaps)
	{
		if (ApiType == API_OPENGL)
		{
			// Declare samplers
			out.Write("SAMPLER_BINDING(0) uniform sampler2DArray samp[16];\n");
		}
		else
		{
			out.Write("SamplerState samp[8] : register(s0);\n");
			out.Write("Texture2DArray Tex[16] : register(t0);\n");
		}
		out.Write(headerUtilI);
	}
	// uniforms
	if (ApiType == API_OPENGL)
		out.Write("layout(std140%s) uniform TSBlock {\n", g_ActiveConfig.backend_info.bSupportsBindingLayout ? ", binding = 3" : "");
	else
		out.Write("cbuffer TSBlock : register(b0) {\n");
	out.Write(
		"\tfloat4 " I_TESSPARAMS";\n"
		"\tint4 " I_CULLPARAMS";\n"
		"};\n");

	if (ApiType == API_OPENGL)
		out.Write("layout(std140%s) uniform VSBlock {\n", g_ActiveConfig.backend_info.bSupportsBindingLayout ? ", binding = 2" : "");
	else
		out.Write("cbuffer VSBlock : register(b1) {\n");
	out.Write(
		"\tfloat4 " I_PROJECTION "[4];\n"
		"\tfloat4 " I_DEPTHPARAMS ";\n"
		"\tfloat4 " I_VIEWPARAMS ";\n"
		"};\n");

	if (ApiType == API_OPENGL)
		out.Write("layout(std140%s) uniform PSBlock {\n", g_ActiveConfig.backend_info.bSupportsBindingLayout ? ", binding = 1" : "");
	else
		out.Write("cbuffer PSBlock : register(b2) {\n");
	out.Write(
		"\tint4 " I_COLORS "[4];\n"
		"\tint4 " I_KCOLORS "[4];\n"
		"\tint4 " I_ALPHA ";\n"
		"\tfloat4 " I_TEXDIMS "[8];\n"
		"\tint4 " I_ZBIAS "[2];\n"
		"\tint4 " I_INDTEXSCALE "[2];\n"
		"\tint4 " I_INDTEXMTX "[6];\n"
		"\tint4 " I_FOGCOLOR ";\n"
		"\tint4 " I_FOGI ";\n"
		"\tfloat4 " I_FOGF "[2];\n"
		"\tfloat4 " I_ZSLOPE ";\n"
		"\tint4 "  I_FLAGS ";\n"
		"\tfloat4 " I_EFBSCALE ";\n"
		"};\n");

	out.Write("struct VS_OUTPUT {\n");
	GenerateVSOutputMembers<ApiType>(out, true, uid_data.numTexGens);
	out.Write("};\n");

	if (ApiType == API_OPENGL)
	{

	}
	else // D3D
	{
		out.Write("struct ConstantOutput\n"
			"{\n"
			"float EFactor[3] : SV_TessFactor;\n"
			"float InsideFactor : SV_InsideTessFactor;\n"
			"float4 edgesize : TEXCOORD0;\n");
		u32 texcount = uid_data.numTexGens < 7 ? uid_data.numTexGens : 8;
		for (unsigned int i = 0; i < texcount; ++i)
			out.Write("float4 tex%d[3] : TEXCOORD%d;\n", i, i * 3 + 1);
		out.Write("float4 Normal[3]: TEXCOORD%d;\n", texcount * 3 + 1);

		out.Write("};\n");
		out.Write(s_hlsl_hull_header_str);
		if (uid_data.numTexGens < 7)
		{
			out.Write("result.pos = float4(patch[id].clipPos.x,patch[id].clipPos.y,patch[id].Normal.w, 1.0);\n");
		}
		else
		{
			out.Write("result.pos = float4(patch[id].tex0.w, patch[id].tex1.w, patch[id].tex7.w, 1.0);\n");
		}
		out.Write("result.colors_0 = patch[id].colors_0;\n"
			"result.colors_1 = patch[id].colors_1;\n");
		if (g_ActiveConfig.backend_info.bSupportsDepthClamp)
			out.Write("result.clipDist = patch[id].clipDist;\n");
		out.Write("return result;\n}\n");
		out.Write(s_hlsl_constant_header_str);
		out.Write(
			"if (" I_CULLPARAMS ".y != 0) {\n"
			"float3 spos0 = patch[0].pos.xyz / patch[0].pos.w;\n"
			"float3 spos1 = patch[1].pos.xyz / patch[1].pos.w;\n"
			"float3 spos2 = patch[2].pos.xyz / patch[2].pos.w;\n"
			"float3 posmax = max(max(spos0, spos1), spos2);\n"
			"float3 posmin = min(min(spos0, spos1), spos2);\n"
			"if (\n"
			"(posmin.x > 1.5 || posmax.x < -1.5 || posmin.y > 1.5 || posmax.y < -1.5 || posmin.z > 1.5 || posmax.z < -0.5)"
			")\n"
			"{\n"
			"result.EFactor[0] = 0;\n"
			"result.EFactor[1] = 0;\n"
			"result.EFactor[2] = 0;\n"
			"result.InsideFactor = 0;\n"
			"return result; // culled, so no further processing\n"
			"}\n}\n");
		out.Write("float4 pos[3];\n");
		out.Write("[unroll]\n"
			"for(int i = 0; i < 3; i++)\n{\n");
		if (uid_data.numTexGens < 7)
		{
			out.Write("pos[i] = float4(patch[i].clipPos.x,patch[i].clipPos.y,patch[i].Normal.w, 1.0);\n");
		}
		else
		{
			out.Write("pos[i] = float4(patch[i].tex0.w, patch[i].tex1.w, patch[i].tex7.w, 1.0);\n");
		}
		for (u32 i = 0; i < texcount; ++i)
		{
			out.Write("result.tex%d[i].xyz = patch[i].tex%d.xyz;\n", i, i);
			out.Write("{\n");
			out.Write("float2 t0 = patch[i].tex%d.xy;", i);
			out.Write("t0 = t0 / ((patch[i].tex%d.z == 0.0) ? 2.0 : patch[i].tex%d.z);", i, i);
			out.Write("float2 t1 = patch[(i + 1) %% 3].tex%d.xy;", i);
			out.Write("if (patch[(i + 1) %% 3].tex%d.z != 0.0) t0 = t0 /patch[(i + 1) %% 3].tex%d.z;", i, i);
			out.Write("result.tex%d[i].w = distance(t0, t1);\n", i);
			out.Write("}\n");
		}
		if (normalpresent)
		{
			if (uid_data.numTexGens < 7)
			{
				out.Write("result.Normal[i] = patch[i].Normal;\n");
			}
			else
			{
				out.Write("result.Normal[i] = float4(patch[i].tex4.w, patch[i].tex5.w, patch[i].tex6.w, 1.0f);\n");
			}
		}
		out.Write("}\n");
		out.Write("float3 edge0 = pos[1].xyz - pos[0].xyz;\n"
			"float3 edge2 = pos[2].xyz - pos[0].xyz;\n"
			"float3 faceNormal = normalize(cross(edge2, edge0));\n");
		if (!normalpresent)
		{
			out.Write("result.Normal[0] = float4(faceNormal, 1.0f);\n");
			out.Write("result.Normal[1] = result.Normal[0];\n");
			out.Write("result.Normal[2] = result.Normal[0];\n");
		}
		out.Write(
			"if (" I_CULLPARAMS ".x != 0) {\n"
			"float3 view = normalize(-pos[0].xyz);\n"
			"float visibility = dot(view, faceNormal);\n"
			"bool notvisible = " I_CULLPARAMS ".x < 0 ? (visibility < -0.25) : (visibility > 0.25);\n"
			"if (notvisible) {\n"
			"result.EFactor[0] = 0;\n"
			"result.EFactor[1] = 0;\n"
			"result.EFactor[2] = 0;\n"
			"result.InsideFactor = 0;\n"
			"return result; // culled, so no further processing\n"
			"}\n}\n");
		out.Write(
			"float l0 = distance(pos[1].xyz,pos[2].xyz);\n"
			"float l1 = distance(pos[2].xyz,pos[0].xyz);\n"
			"float l2 = distance(pos[0].xyz,pos[1].xyz);\n"
			"result.edgesize = float4(l0, l1, l2, 1.0);\n"
			"result.EFactor[0] = CalcTessFactor((pos[1].xyz+pos[2].xyz) * 0.5, l0);\n"
			"result.EFactor[1] = CalcTessFactor((pos[2].xyz+pos[0].xyz) * 0.5, l1);\n"
			"result.EFactor[2] = CalcTessFactor((pos[0].xyz+pos[1].xyz) * 0.5, l2);\n"
			"result.InsideFactor = (result.EFactor[0] + result.EFactor[1] + result.EFactor[2]) / 3;\n"
			"return result;\n};\n"
		);
		out.Write(s_hlsl_ds_str);

		for (u32 i = 0; i < texcount; ++i)
			out.Write("result.tex%d.xyz = BInterpolate(pconstans.tex%d, bCoords).xyz;\n", i, i, i, i);

		out.Write("float displacement = 0.0, displacementcount = 0.0, borderdistance = bCoords.x * bCoords.y * bCoords.z;\n");
		out.Write("int3 tevcoord=int3(0,0,0);\n");
		out.Write("int2 wrappedcoord = int2(0, 0);\n");
		if (enablenormalmaps)
		{
			out.Write("if(" I_FLAGS ".x != 0)\n{\n");
			out.Write("float4 uv[%d];\n", numTexgen);
			out.Write("int2 t_coord;\n");
			for (u32 i = 0; i < numTexgen; ++i)
			{
				if (((uid_data.texMtxInfo_n_projection >> i) & 1) == XF_TEXPROJ_STQ)
				{
					out.Write("\tuv[%d].xy = result.tex%d.xy / ((result.tex%d.z == 0.0) ? 2.0 : result.tex%d.z);\n", i, i, i, i);
				}
				else
				{
					out.Write("uv[%d].xy = result.tex%d.xy;\n", i, i);
				}
				out.Write("uv[%d].xy = trunc(uv[%d].xy * " I_TEXDIMS"[%d].zw);\n", i, i, i);
				out.Write("uv[%d].z = dot(pconstans.edgesize.zxy, bCoords)/dot(float3(pconstans.tex%d[0].w, pconstans.tex%d[1].w, pconstans.tex%d[2].w), bCoords);\n", i, i, i, i);
				out.Write("uv[%d].w = dot(log2(8.0*float3(pconstans.tex%d[0].w,pconstans.tex%d[1].w,pconstans.tex%d[2].w) / float3(pconstans.EFactor[2], pconstans.EFactor[0], pconstans.EFactor[1])),bCoords);\n", i, i, i, i);
				out.Write("uv[%d].z = borderdistance * 2.0 * uv[%d].z;\n", i, i);
				out.Write("uv[%d].w = ceil(saturate(uv[%d].w) * 8.0) * 0.125;\n", i, i);
			}
			for (u32 i = 0; i < numindStages; ++i)
			{
				if (nIndirectStagesUsed & (1 << i))
				{
					u32 texcoord = uid_data.GetTevindirefCoord(i);
					u32 texmap = uid_data.GetTevindirefMap(i);
					if (texcoord < numTexgen)
					{
						out.Write("t_coord = BSHR(int2(uv[%d].xy) , " I_INDTEXSCALE"[%d].%s);\n", texcoord, i / 2, (i & 1) ? "zw" : "xy");
					}
					else
					{
						out.Write("t_coord = int2(0,0);\n");
					}
					out.Write("int3 indtex%d = ", i);
					SampleTexture<ApiType>(out, "float2(t_coord)", "abg", texmap, uid_data.stereo);
				}
			}
			for (u32 i = 0; i < numStages; ++i)
			{
				WriteFetchDisplacement<ApiType>(out, i, uid_data); // Fetch Texture data
			}
			out.Write("}\n");
		}

		out.Write("float3 pos0 = patch[0].pos.xyz;\n"
			"float3 pos1 = patch[1].pos.xyz;\n"
			"float3 pos2 = patch[2].pos.xyz;\n"
			"float3 position = BInterpolate(pos0, pos1, pos2, bCoords);\n");

		out.Write(
			"float3 norm0 = pconstans.Normal[0].xyz;\n"
			"float3 norm1 = pconstans.Normal[1].xyz;\n"
			"float3 norm2 = pconstans.Normal[2].xyz;\n");

		out.Write("float3 normal = normalize(BInterpolate(norm0, norm1, norm2, bCoords));\n"
			"pos0 = PrjToPlane(norm0, pos0, position);\n"
			"pos1 = PrjToPlane(norm1, pos1, position);\n"
			"pos2 = PrjToPlane(norm2, pos2, position);\n"
			"position = lerp(position, BInterpolate(pos0, pos1, pos2, bCoords),saturate(" I_TESSPARAMS ".zzz * borderdistance * 16.0));\n"
			"position += displacement * normal * " I_TESSPARAMS ".w;\n");
		// Transform world position to view-projection
		out.Write("float4 pos = float4(position, 1.0);\n"
			"result.pos = float4(dot(" I_PROJECTION "[0], pos), dot(" I_PROJECTION "[1], pos), dot(" I_PROJECTION "[2], pos), dot(" I_PROJECTION "[3], pos));\n"
			"result.pos.xy = result.pos.xy + result.pos.w * " I_DEPTHPARAMS".zw;\n"
			"result.colors_0 = BInterpolate(patch[0].colors_0, patch[1].colors_0, patch[2].colors_0, bCoords);\n"
			"result.colors_1 = BInterpolate(patch[0].colors_1, patch[1].colors_1, patch[2].colors_1, bCoords);\n");
		
			if (g_ActiveConfig.backend_info.bSupportsDepthClamp)
			out.Write("result.clipDist = BInterpolate(patch[0].clipDist, patch[1].clipDist, patch[2].clipDist, bCoords);\n");

		if (uid_data.numTexGens < 7)
		{
			out.Write("result.clipPos = float4(position.xy, result.pos.zw);\n");
			out.Write("result.Normal = float4(normal.xyz, position.z);\n");
		}
		else
		{
			// Store clip position in the w component of first 4 texcoords
			out.Write(
				"result.tex0.w = position.x;\n"
				"result.tex1.w = position.y;\n"
				"result.tex2.w = result.pos.z;\n"
				"result.tex3.w = result.pos.w;\n");
			out.Write("result.tex4.w = normal.x;\n"
				"result.tex5.w = normal.y;\n"
				"result.tex6.w = normal.z;\n");

			if (uid_data.numTexGens < 8)
				out.Write("result.tex7 = position.xyzz;\n");
			else
				out.Write("result.tex7.w = position.z;\n");
		}
		out.Write("result.pos.z = result.pos.w * " I_DEPTHPARAMS ".x - result.pos.z * " I_DEPTHPARAMS ".y;\n");
		out.Write("return result;\n}");
	}

	if (codebuffer[TESSELLATIONSHADERGEN_BUFFERSIZE - 1] != 0x7C)
		PanicAlert("GeometryShader generator - buffer too small, canary has been eaten!");
}

void GenerateTessellationShaderCode(ShaderCode& object, API_TYPE ApiType, const Tessellation_shader_uid_data& uid_data)
{
	if (ApiType == API_OPENGL)
	{
		GenerateTessellationShader<API_OPENGL>(object, uid_data);
	}
	else
	{
		GenerateTessellationShader<API_D3D11>(object, uid_data);
	}
}
