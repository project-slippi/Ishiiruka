// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <stdio.h>
#include <cmath>
#include <assert.h>
#include <locale.h>
#ifdef __APPLE__
#include <xlocale.h>
#endif
#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/XFMemory.h"  // for texture projection mode
#include "VideoCommon/VideoConfig.h"



//   old tev->pixelshader notes
//
//   color for this stage (alpha, color) is given by bpmem.tevorders[0].colorchan0
//   konstant for this stage (alpha, color) is given by bpmem.tevksel
//   inputs are given by bpmem.combiners[0].colorC.a/b/c/d     << could be current channel color
//   according to GXTevColorArg table above
//   output is given by .outreg
//   tevtemp is set according to swapmodetables and

// Tev Source States
enum tevSources
{
	CPREV = 0,
	APREV = 1,
	C0 = 2,
	A0 = 3,
	C1 = 4,
	A1 = 5,
	C2 = 6,
	A2 = 7,
	TEXC = 8,
	TEXA = 9,
	RASC = 10,
	RASA = 11,
	ONE = 12,
	HALF = 13,
	KONST = 14,
	ZERO = 15,
	SOURCECOUNT = 16
};

static bool TevOverflowState[tevSources::SOURCECOUNT];
static bool tevRascolor0_Expanded = false;
static bool tevRascolor1_Expanded = false;
inline void InitializeRegisterState()
{
	TevOverflowState[tevSources::CPREV] = true;
	TevOverflowState[tevSources::APREV] = true;
	TevOverflowState[tevSources::C0] = true;
	TevOverflowState[tevSources::A0] = true;
	TevOverflowState[tevSources::C1] = true;
	TevOverflowState[tevSources::A1] = true;
	TevOverflowState[tevSources::C2] = true;
	TevOverflowState[tevSources::A2] = true;
	TevOverflowState[tevSources::TEXC] = false;
	TevOverflowState[tevSources::TEXA] = false;
	TevOverflowState[tevSources::RASC] = true;
	TevOverflowState[tevSources::RASA] = true;
	TevOverflowState[tevSources::ONE] = false;
	TevOverflowState[tevSources::HALF] = false;
	TevOverflowState[tevSources::KONST] = true;
	TevOverflowState[tevSources::ZERO] = false;
	tevRascolor0_Expanded = false;
	tevRascolor1_Expanded = false;
}

static const char *tevKSelTableC[] = // KCSEL
{
	"255,255,255",	// 1   = 0x00
	"223,223,223",	// 7_8 = 0x01
	"191,191,191",	// 3_4 = 0x02
	"159,159,159",	// 5_8 = 0x03
	"128,128,128",	// 1_2 = 0x04
	"96,96,96",		// 3_8 = 0x05
	"64,64,64",		// 1_4 = 0x06
	"32,32,32",		// 1_8 = 0x07
	"0,0,0", // 0x08
	"0,0,0", // 0x09
	"0,0,0", // 0x0a
	"0,0,0", // 0x0b
	I_KCOLORS "[0].rgb", // K0 = 0x0C
	I_KCOLORS "[1].rgb", // K1 = 0x0D
	I_KCOLORS "[2].rgb", // K2 = 0x0E
	I_KCOLORS "[3].rgb", // K3 = 0x0F
	I_KCOLORS "[0].rrr", // K0_R = 0x10
	I_KCOLORS "[1].rrr", // K1_R = 0x11
	I_KCOLORS "[2].rrr", // K2_R = 0x12
	I_KCOLORS "[3].rrr", // K3_R = 0x13
	I_KCOLORS "[0].ggg", // K0_G = 0x14
	I_KCOLORS "[1].ggg", // K1_G = 0x15
	I_KCOLORS "[2].ggg", // K2_G = 0x16
	I_KCOLORS "[3].ggg", // K3_G = 0x17
	I_KCOLORS "[0].bbb", // K0_B = 0x18
	I_KCOLORS "[1].bbb", // K1_B = 0x19
	I_KCOLORS "[2].bbb", // K2_B = 0x1A
	I_KCOLORS "[3].bbb", // K3_B = 0x1B
	I_KCOLORS "[0].aaa", // K0_A = 0x1C
	I_KCOLORS "[1].aaa", // K1_A = 0x1D
	I_KCOLORS "[2].aaa", // K2_A = 0x1E
	I_KCOLORS "[3].aaa", // K3_A = 0x1F
};

static const char *tevKSelTableA[] = // KASEL
{
	"255",	// 1   = 0x00
	"223",	// 7_8 = 0x01
	"191",	// 3_4 = 0x02
	"159",	// 5_8 = 0x03
	"128",	// 1_2 = 0x04
	"96",		// 3_8 = 0x05
	"64",		// 1_4 = 0x06
	"32",		// 1_8 = 0x07
	"0", // 0x08
	"0", // 0x09
	"0", // 0x0a
	"0", // 0x0b
	"0", // 0x0c
	"0", // 0x0d
	"0", // 0x0e
	"0", // 0x0f
	I_KCOLORS "[0].r", // K0_R = 0x10
	I_KCOLORS "[1].r", // K1_R = 0x11
	I_KCOLORS "[2].r", // K2_R = 0x12
	I_KCOLORS "[3].r", // K3_R = 0x13
	I_KCOLORS "[0].g", // K0_G = 0x14
	I_KCOLORS "[1].g", // K1_G = 0x15
	I_KCOLORS "[2].g", // K2_G = 0x16
	I_KCOLORS "[3].g", // K3_G = 0x17
	I_KCOLORS "[0].b", // K0_B = 0x18
	I_KCOLORS "[1].b", // K1_B = 0x19
	I_KCOLORS "[2].b", // K2_B = 0x1A
	I_KCOLORS "[3].b", // K3_B = 0x1B
	I_KCOLORS "[0].a", // K0_A = 0x1C
	I_KCOLORS "[1].a", // K1_A = 0x1D
	I_KCOLORS "[2].a", // K2_A = 0x1E
	I_KCOLORS "[3].a", // K3_A = 0x1F
};

static const char *tevCInputTable[] = // CC
{
	"prev.rgb",         // CPREV,
	"prev.aaa",         // APREV,
	"c0.rgb",           // C0,
	"c0.aaa",           // A0,
	"c1.rgb",           // C1,
	"c1.aaa",		// A1,
	"c2.rgb",		// C2,
	"c2.aaa",		// A2,
	"tex_t.rgb",	// TEXC,
	"tex_t.aaa",	// TEXA,
	"ras_t.rgb",	// RASC,
	"ras_t.aaa",	// RASA,
	"255,255,255",	// ONE
	"128,128,128",	// HALF
	"konst_t.rgb",	// KONST
	"0,0,0"	// ZERO
};

static const char *tevAInputTable[] = // CA
{
	"prev.a",	// APREV,
	"c0.a",		// A0,
	"c1.a",		// A1,
	"c2.a",		// A2,
	"tex_t.a",	// TEXA,
	"ras_t.a",	// RASA,
	"konst_t.a",// KONST,  (hw1 had quarter)
	"0"		// ZERO
};

static const tevSources AInputSourceMap[] =
{
	tevSources::APREV,	// APREV,
	tevSources::A0,		// A0,
	tevSources::A1,		// A1,
	tevSources::A2,		// A2,
	tevSources::TEXA,	// TEXA,
	tevSources::RASA,	// RASA,
	tevSources::KONST,	// KONST,  (hw1 had quarter)
	tevSources::ZERO	// ZERO
};

static const char *tevRasTable[] =
{
	"wu4(col0)",
	"wu4(col1)",
	"wu4(0,0,0,0)", //2
	"wu4(0,0,0,0)", //3
	"wu4(0,0,0,0)", //4
	"wu4(a_bump,a_bump,a_bump,a_bump)", // use bump alpha
	"(wu4(1,1,1,1)*BOR(a_bump, BSHR(a_bump, 5)))", //normalized
	"wu4(0,0,0,0)", // zero
};

//static const char *tevTexFunc[] = { "tex2D", "texRECT" };

static const char *tevCOutputTable[] = { "prev.rgb", "c0.rgb", "c1.rgb", "c2.rgb" };
static const tevSources tevCOutputSourceMap[] = { tevSources::CPREV, tevSources::C0, tevSources::C1, tevSources::C2 };
static const char *tevAOutputTable[] = { "prev.a", "c0.a", "c1.a", "c2.a" };
static const tevSources tevAOutputSourceMap[] = { tevSources::APREV, tevSources::A0, tevSources::A1, tevSources::A2 };

static const char* headerUtil = R"hlsl(
float4 CHK_O_U8(float4 x)
{ 
	return frac(((x) + 1024.0) * (1.0/256.0)) * 256.0;
}
#define BOR(x, n) ((x) + (n))
#define BSHR(x, n) floor((x) * exp2(-(n)))
float2 BSH(float2 x, float n)
{
	float z = exp2(-n);
	if (z < 1.0)
	{
		float y = (1.0 / z);
		if (x.x < 0.0)
			x.x = 1.0 + x.x - y;
		if (x.y < 0.0)
			x.y = 1.0 + x.y - y;
	}
	return trunc(x * z);
}
// remainder implementation with the restriction that "y" must be always greater than 0
float remainder(float x, float y)
{
	y = (x < 0.0) ? (-y) : y;
	return frac(x/y)*y;
}
// rounding + casting to integer at once in a single function
wu  wuround(float  x) { return wu(round(x)); }
wu2 wuround(float2 x) { return wu2(round(x)); }
wu3 wuround(float3 x) { return wu3(round(x)); }
wu4 wuround(float4 x) { return wu4(round(x)); }
)hlsl";

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
wu  wuround(float  x) { return wu(round(x)); }
wu2 wuround(float2 x) { return wu2(round(x)); }
wu3 wuround(float3 x) { return wu3(round(x)); }
wu4 wuround(float4 x) { return wu4(round(x)); }
)hlsl";

static const char* headerLightUtil = R"hlsl(
float3x3 cotangent_frame( float3 N, float3 p, float2 uv )
{
    // get edge vectors of the pixel triangle
    float3 dp1 = ddx( p );
    float3 dp2 = -ddy( p );
    float2 duv1 = ddx( uv );
    float2 duv2 = -ddy( uv );
 
    // solve the linear system
    float3 dp2perp = cross( dp2, N );
    float3 dp1perp = cross( N, dp1 );
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;
 
    // construct a scale-invariant frame 
    float invmax = rsqrt( max( dot(T,T), dot(B,B) ) );
	return float3x3( T * invmax, B * invmax, N );
}

float3 perturb_normal( float3 N, float3 P, float2 texcoord , float3 map)
{
	// assume N, the interpolated vertex normal and 
	// P, Position	
	float3x3 TBN = cotangent_frame( N, P, texcoord );
	return normalize( mul(map, TBN) );
}
)hlsl";

static char text[PIXELSHADERGEN_BUFFERSIZE];
template<class T, API_TYPE ApiType, bool use_integer_math>
static inline void WriteFetchStageTexture(T& out, int n, const bool LoadMaterial, const char swapModeTable[4][5], const BPMemory &bpm);
template<class T, bool Write_Code, API_TYPE ApiType, bool use_integer_math>
inline void WriteStage(T& out, pixel_shader_uid_data& uid_data, int n, const char swapModeTable[4][5], const BPMemory &bpm);
template<class T, API_TYPE ApiType>
inline void SampleTexture(T& out, const char *texcoords, const char *texswap, int texmap);
template<class T, API_TYPE ApiType>
inline void SampleTextureRAW(T& out, const char *texcoords, const char *texswap, const char *layer, int texmap, int texdims);
template<class T, bool Write_Code, API_TYPE ApiType, bool use_integer_math>
inline void WriteAlphaTest(T& out, pixel_shader_uid_data& uid_data, DSTALPHA_MODE dstAlphaMode, bool per_pixel_depth, const BPMemory &bpm);
template<class T, bool Write_Code, bool use_integer_math>
inline void WriteFog(T& out, pixel_shader_uid_data& uid_data, const BPMemory &bpm);
template<class T, bool use_integer_math, API_TYPE ApiType>
inline void WritePerPixelDepth(T& out, const BPMemory &bpm);
template<int TEVARG_ZERO, int TEVARG_ONE, class T>
static inline void WriteTevRegular(T& out, const char* components, int bias, int op, int clamp, int shift, int a, int b, int c, int d);
template<int TEVARG_ZERO, int TEVARG_ONE, class T>
static inline void WriteTevRegularI(T& out, const char* components, int bias, int op, int clamp, int shift, int a, int b, int c, int d);

template<class T, bool Write_Code, API_TYPE ApiType, bool Use_integer_math = false>
inline void GeneratePixelShader(T& out, DSTALPHA_MODE dstAlphaMode, u32 components, const XFMemory &xfr, const BPMemory &bpm)
{
	// Non-uid template parameters will write to the dummy data (=> gets optimized out)
	pixel_shader_uid_data dummy_data;
	bool uidPresent = (&out.template GetUidData<pixel_shader_uid_data>() != NULL);
	pixel_shader_uid_data& uid_data = uidPresent ? out.template GetUidData<pixel_shader_uid_data>() : dummy_data;

	char* codebuffer = nullptr;
	if (Write_Code)
	{
		codebuffer = out.GetBuffer();
		if (codebuffer == nullptr)
		{
			codebuffer = text;
			out.SetBuffer(codebuffer);
		}
		codebuffer[PIXELSHADERGEN_BUFFERSIZE - 1] = 0x7C;  // canary
	}

	if (uidPresent)
	{
		out.ClearUID();
	}

	u32 numStages = bpm.genMode.numtevstages + 1;
	u32 numTexgen = bpm.genMode.numtexgens;
	u32 numindStages = bpm.genMode.numindstages;
	const AlphaTest::TEST_RESULT Pretest = bpm.alpha_test.TestResult();
	const bool forced_early_z = g_ActiveConfig.backend_info.bSupportsEarlyZ
		&& bpm.UseEarlyDepthTest()
		&& (g_ActiveConfig.bFastDepthCalc || Pretest == AlphaTest::UNDETERMINED)
		&& !(bpm.zmode.testenable && bpm.genMode.zfreeze);
	const bool per_pixel_depth = bpm.zmode.testenable
		&& ((bpm.ztex2.op != ZTEXTURE_DISABLE && bpm.UseLateDepthTest())
			|| (!g_ActiveConfig.bFastDepthCalc && !forced_early_z)
			|| bpm.genMode.zfreeze);
	bool lightingEnabled = xfr.numChan.numColorChans > 0;
	bool enable_pl = g_ActiveConfig.bEnablePixelLighting
		&& g_ActiveConfig.backend_info.bSupportsPixelLighting
		&& lightingEnabled;
	bool enablenormalmaps = enable_pl && g_ActiveConfig.HiresMaterialMapsEnabled();
	uid_data.bounding_box = g_ActiveConfig.backend_info.bSupportsBBox && BoundingBox::active && g_ActiveConfig.iBBoxMode == BBoxGPU;
	uid_data.per_pixel_depth = per_pixel_depth;
	uid_data.dstAlphaMode = dstAlphaMode;
	uid_data.pixel_lighting = enable_pl;
	uid_data.genMode_numtexgens = numTexgen;
	uid_data.zfreeze = bpm.genMode.zfreeze;
	uid_data.stereo = g_ActiveConfig.iStereoMode > 0;
	if (!(ApiType & API_D3D9))
	{
		uid_data.msaa = g_ActiveConfig.iMultisampleMode > 0;
		uid_data.ssaa = g_ActiveConfig.iMultisampleMode > 0 && g_ActiveConfig.bSSAA;
	}
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
	if (Write_Code)
	{
		InitializeRegisterState();
		out.Write("//Pixel Shader for TEV stages\n");
		if (enablenormalmaps)
		{
			out.Write(headerLightUtil);
		}
		if (Use_integer_math)
		{
			out.Write("#define wu int\n");
			if (ApiType == API_OPENGL)
			{
				out.Write("#define wu2 ivec2\n");
				out.Write("#define wu3 ivec3\n");
				out.Write("#define wu4 ivec4\n");
			}
			else
			{
				out.Write("#define wu2 int2\n");
				out.Write("#define wu3 int3\n");
				out.Write("#define wu4 int4\n");
			}
			out.Write(headerUtilI);
		}
		else
		{

			out.Write("#define wu float\n");
			if (ApiType == API_OPENGL)
			{
				out.Write("#define wu2 vec2\n");
				out.Write("#define wu3 vec3\n");
				out.Write("#define wu4 vec4\n");
			}
			else
			{
				out.Write("#define wu2 float2\n");
				out.Write("#define wu3 float3\n");
				out.Write("#define wu4 float4\n");
			}
			out.Write(headerUtil);
		}

		u32 samplercount = enablenormalmaps ? 16 : 8;

		if (ApiType == API_OPENGL)
		{
			// Declare samplers
			for (u32 i = 0; i < samplercount; ++i)
				out.Write("SAMPLER_BINDING(%d) uniform sampler2DArray samp%d;\n", i, i);
		}
		else
		{
			// Declare samplers
			for (u32 i = 0; i < 8; ++i)
				out.Write("%s samp%d : register(s%d);\n", (ApiType == API_D3D11) ? "sampler" : "uniform sampler2D", i, i);

			if (ApiType == API_D3D11)
			{
				out.Write("\n");
				for (u32 i = 0; i < samplercount; ++i)
				{
					out.Write("Texture2DArray Tex%d : register(t%d);\n", i, i);
				}
			}
		}
		out.Write("\n");

		if (ApiType == API_OPENGL)
			out.Write("layout(std140%s) uniform PSBlock {\n", g_ActiveConfig.backend_info.bSupportsBindingLayout ? ", binding = 1" : "");

		DeclareUniform<T, ApiType>(out, C_COLORS, "wu4", I_COLORS "[4]");
		DeclareUniform<T, ApiType>(out, C_KCOLORS, "wu4", I_KCOLORS "[4]");
		DeclareUniform<T, ApiType>(out, C_ALPHA, "wu4", I_ALPHA);
		DeclareUniform<T, ApiType>(out, C_TEXDIMS, "float4", I_TEXDIMS "[8]");
		DeclareUniform<T, ApiType>(out, C_ZBIAS, "wu4", I_ZBIAS "[2]");
		DeclareUniform<T, ApiType>(out, C_INDTEXSCALE, "wu4", I_INDTEXSCALE "[2]");
		DeclareUniform<T, ApiType>(out, C_INDTEXMTX, "wu4", I_INDTEXMTX "[6]");
		DeclareUniform<T, ApiType>(out, C_FOGCOLOR, "wu4", I_FOGCOLOR);
		DeclareUniform<T, ApiType>(out, C_FOGI, "wu4", I_FOGI);
		DeclareUniform<T, ApiType>(out, C_FOGF, "float4", I_FOGF "[2]");
		DeclareUniform<T, ApiType>(out, C_ZSLOPE, "float4", I_ZSLOPE);
		DeclareUniform<T, ApiType>(out, C_FLAGS, "wu4", I_FLAGS);
		DeclareUniform<T, ApiType>(out, C_EFBSCALE, "float4", I_EFBSCALE);

		if (enable_pl)
		{
			DeclareUniform<T, ApiType>(out, C_PLIGHTS, "float4", I_PLIGHTS "[40]");
			DeclareUniform<T, ApiType>(out, C_PMATERIALS, "float4", I_PMATERIALS "[4]");
		}

		if (ApiType == API_OPENGL)
		{
			out.Write("};\n");
			if (uid_data.bounding_box)
			{
				out.Write(
					"layout(std140, binding = 3) buffer BBox {\n"
					"\tint4 bbox_data;\n"
					"};\n"
					);
			}
			out.Write("out vec4 ocol0;\n");
			if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
				out.Write("out vec4 ocol1;\n");

			if (per_pixel_depth)
				out.Write("#define depth gl_FragDepth\n");

			if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
			{
				out.Write("in VertexData {\n");
				GenerateVSOutputMembers<T, ApiType>(out, enable_pl, xfr, GetInterpolationQualifier(ApiType, true, true));

				if (g_ActiveConfig.iStereoMode > 0)
					out.Write("\tflat int layer;\n");

				out.Write("};\n");
			}
			else
			{
				// "centroid" attribute is only supported by D3D11
				const char* optCentroid = GetInterpolationQualifier(ApiType);

				out.Write("%s in float4 colors_0;\n", optCentroid);
				out.Write("%s in float4 colors_1;\n", optCentroid);

				// compute window position if needed because binding semantic WPOS is not widely supported
				// Let's set up attributes
				if (numTexgen < 7)
				{
					for (u32 i = 0; i < numTexgen; ++i)
					{
						out.Write("%s in float3 uv%d_2;\n", optCentroid, i);
					}
					out.Write("%s in float4 clipPos_2;\n", optCentroid);
					if (enable_pl)
					{
						out.Write("%s in float4 Normal_2;\n", optCentroid);
					}
				}
				else
				{
					// wpos is in w of first 4 texcoords
					if (enable_pl)
					{
						for (u32 i = 0; i < 8; ++i)
						{
							out.Write("%s in float4 uv%d_2;\n", optCentroid, i);
						}
					}
					else
					{
						for (u32 i = 0; i < numTexgen; ++i)
						{
							out.Write("%s in float%d uv%d_2;\n", optCentroid, i < 4 ? 4 : 3, i);
						}
					}
				}
			}
			if (forced_early_z)
			{
				// HACK: This doesn't force the driver to write to depth buffer if alpha test fails.
				// It just allows it, but it seems that all drivers do.
				out.Write("layout(early_fragment_tests) in;\n");
			}
			out.Write("void main()\n{\n");
			if (numTexgen >= 7)
			{
				out.Write("float4 clipPos;\n");
			}
			out.Write("\tfloat4 rawpos = gl_FragCoord;\n");
		}
		else
		{
			if (ApiType == API_D3D11)
			{
				if (uid_data.bounding_box)
				{
					out.Write(
						"globallycoherent RWBuffer<int> bbox_data : register(u2);\n"
						);
				}
			}
			if (forced_early_z)
			{
				out.Write("[earlydepthstencil]\n");
			}
			out.Write("void main(\n");
			if (ApiType != API_D3D11)
			{
				out.Write("  out float4 ocol0 : COLOR0,%s%s\n  in float4 rawpos : %s,\n",
					dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND ? "\n  out float4 ocol1 : COLOR1," : "",
					per_pixel_depth ? "\n  out float depth : DEPTH," : "",
					ApiType == API_D3D9_SM20 ? "POSITION" : "VPOS");
			}
			else
			{
				out.Write("  out float4 ocol0 : SV_Target0,%s%s\n  in float4 rawpos : SV_Position,\n",
					dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND ? "\n  out float4 ocol1 : SV_Target1," : "",
					per_pixel_depth ? "\n  out float depth : SV_Depth," : "");
			}

			// "centroid" attribute is only supported by D3D11
			const char* optCentroid = GetInterpolationQualifier(ApiType);

			out.Write("  in %s float4 colors_0 : COLOR0,\n", optCentroid);
			out.Write("  in %s float4 colors_1 : COLOR1", optCentroid);

			// compute window position if needed because binding semantic WPOS is not widely supported
			if (numTexgen < 7)
			{
				for (u32 i = 0; i < numTexgen; ++i)
					out.Write(",\n  in %s float3 uv%d : TEXCOORD%d", optCentroid, i, i);
				out.Write(",\n  in %s float4 clipPos : TEXCOORD%d", optCentroid, numTexgen);
				if (enable_pl)
					out.Write(",\n  in %s float4 Normal : TEXCOORD%d", optCentroid, numTexgen + 1);
				if (g_ActiveConfig.iStereoMode > 0)
					out.Write(",\n  in uint layer : SV_RenderTargetArrayIndex\n");
				out.Write("        ) {\n");
			}
			else
			{
				// wpos is in w of first 4 texcoords
				if (enable_pl)
				{
					for (u32 i = 0; i < 8; ++i)
						out.Write(",\n  in %s float4 uv%d : TEXCOORD%d", optCentroid, i, i);
				}
				else
				{
					for (u32 i = 0; i < numTexgen; ++i)
						out.Write(",\n in %s float%d uv%d : TEXCOORD%d", optCentroid, i < 4 ? 4 : 3, i, i);
				}
				if (g_ActiveConfig.iStereoMode > 0)
					out.Write(",\n  in uint layer : SV_RenderTargetArrayIndex\n");
				out.Write("        ) {\n");
				out.Write("float4 clipPos = float4(0.0,0.0,0.0,0.0);");
			}
		}
		if (dstAlphaMode == DSTALPHA_NULL)
		{
			if (per_pixel_depth)
			{
				WritePerPixelDepth<T, Write_Code, ApiType>(out, bpm);
			}
			out.Write("ocol0 = float4(0.0,0.0,0.0,0.0);\n");
			out.Write("}\n");
			if (codebuffer[PIXELSHADERGEN_BUFFERSIZE - 1] != 0x7C)
				PanicAlert("PixelShader generator - buffer too small, canary has been eaten!");
			return;
		}

		out.Write("wu4 c0 = " I_COLORS "[1], c1 = " I_COLORS "[2], c2 = " I_COLORS "[3], prev = " I_COLORS "[0];\n"
			"wu4 tex_ta[%i], tex_t = wu4(0,0,0,0), ras_t = wu4(0,0,0,0), konst_t = wu4(0,0,0,0);\n"
			"wu3 c16 = wu3(1,256,0), c24 = wu3(1,256,256*256);\n"
			"wu a_bump=0;\n"
			"wu2 wrappedcoord=wu2(0,0), t_coord=wu2(0,0),ittmpexp=wu2(0,0);\n"
			"wu4 tin_a = wu4(0,0,0,0), tin_b = wu4(0,0,0,0), tin_c = wu4(0,0,0,0), tin_d = wu4(0,0,0,0);\n\n", numStages);

		if (ApiType == API_OPENGL)
		{
			// compute window position if needed because binding semantic WPOS is not widely supported
			// Let's set up attributes
			if (numTexgen < 7)
			{
				if (numTexgen)
				{
					for (u32 i = 0; i < numTexgen; ++i)
					{
						if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
						{
							out.Write("float3 uv%d = tex%d;\n", i, i);
						}
						else
						{
							out.Write("float3 uv%d = uv%d_2;\n", i, i);
						}
					}
				}
				out.Write("float4 clipPos = clipPos_2;\n");
				if (enable_pl)
				{
					out.Write("float4 Normal = Normal_2;\n");
				}
			}
			else
			{
				// wpos is in w of first 4 texcoords
				if (enable_pl)
				{
					for (u32 i = 0; i < 8; ++i)
					{
						if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
						{
							out.Write("float4 uv%d = tex%d;\n", i, i);
						}
						else
						{
							out.Write("float4 uv%d = uv%d_2;\n", i, i);
						}
					}
				}
				else
				{
					for (u32 i = 0; i < numTexgen; ++i)
					{
						if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
						{
							out.Write("float%d uv%d = tex%d;\n", i < 4 ? 4 : 3, i, i);
						}
						else
						{
							out.Write("float%d uv%d = uv%d_2;\n", i < 4 ? 4 : 3, i, i);
						}
					}
				}
			}
		}
	}

	// HACK to handle cases where the tex gen is not enabled
	if (numTexgen == 0)
	{
		if (Write_Code)
			out.Write("float3 uv0 = float3(0.0,0.0,0.0);\n");
	}
	else
	{
		for (u32 i = 0; i < numTexgen; ++i)
		{
			// optional perspective divides
			uid_data.texMtxInfo_n_projection |= xfr.texMtxInfo[i].projection << i;
			if (Write_Code)
			{
				if (xfr.texMtxInfo[i].projection == XF_TEXPROJ_STQ)
				{
					out.Write("if (uv%d.z != 0.0)", i);
					out.Write("\tuv%d.xy = uv%d.xy / uv%d.z;\n", i, i, i);
				}
				out.Write("uv%d.xy = trunc(128.0 * uv%d.xy * " I_TEXDIMS"[%d].zw);\n", i, i, i);
			}
		}
	}

	uid_data.genMode_numtevstages = bpm.genMode.numtevstages;
	uid_data.genMode_numindstages = numindStages;
	out.Write("//%i TEV stages, %i texgens, %i IND stages\n",
		numStages, numTexgen, numindStages);
	// indirect texture map lookup
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
	for (u32 i = 0; i < numindStages; ++i)
	{
		if (nIndirectStagesUsed & (1 << i))
		{
			u32 texcoord = bpm.tevindref.getTexCoord(i);
			u32 texmap = bpm.tevindref.getTexMap(i);

			uid_data.SetTevindrefValues(i, texcoord, texmap);
			if (Write_Code)
			{
				if (texcoord < numTexgen)
				{
					out.Write("t_coord = BSHR(wu2(uv%d.xy) , " I_INDTEXSCALE"[%d].%s);\n", texcoord, i / 2, (i & 1) ? "zw" : "xy");
				}
				else
				{
					out.Write("t_coord = wu2(0,0);\n");
				}
				out.Write("wu3 indtex%d = ", i);
				SampleTexture<T, ApiType>(out, "(float2(t_coord)/128.0)", "abg", texmap);
			}
		}
	}
	// Uid fields for BuildSwapModeTable are set in WriteStage
	char swapModeTable[4][5];
	if (Write_Code)
	{
		if (enablenormalmaps)
		{
			out.Write("\tfloat2 mapcoord = float2(0.0,0.0);\n");
			out.Write("\tfloat3 normalmap = float3(0.0,0.0,1.0);\n");
		}
		const char* swapColors = "rgba";
		for (int i = 0; i < 4; i++)
		{
			swapModeTable[i][0] = swapColors[bpm.tevksel[i * 2].swap1];
			swapModeTable[i][1] = swapColors[bpm.tevksel[i * 2].swap2];
			swapModeTable[i][2] = swapColors[bpm.tevksel[i * 2 + 1].swap1];
			swapModeTable[i][3] = swapColors[bpm.tevksel[i * 2 + 1].swap2];
			swapModeTable[i][4] = '\0';
		}
		for (u32 i = 0; i < numStages; ++i)
		{
			WriteFetchStageTexture<T, ApiType, Use_integer_math>(out, i, enablenormalmaps, swapModeTable, bpm); // Fetch Texture data
		}
	}

	if (enable_pl)
	{
		if (Write_Code)
		{
			if (numTexgen < 7)
			{
				out.Write("float3 _norm0 = normalize(Normal.xyz);\n\n");
				out.Write("float3 pos = float3(clipPos.x,clipPos.y,Normal.w);\n");
			}
			else
			{
				out.Write("float3 _norm0 = normalize(float3(uv4.w,uv5.w,uv6.w));\n\n");
				out.Write("float3 pos = float3(uv0.w,uv1.w,uv7.w);\n");
			}

			out.Write("float4 mat, lacc;\n"
				"float3 ldir, h;\n"
				"float dist, dist2, attn;\n");
			if (Use_integer_math)
			{
				out.Write("int4 ilacc;\n");
			}
			// On GLSL, input variables must not be assigned to.
			// This is why we declare these variables locally instead.
			out.Write("\tfloat4 col0 = float4(0.0,0.0,0.0,0.0), col1 = float4(0.0,0.0,0.0,0.0);\n");
			out.Write("\tfloat3 rawnormal = _norm0;\n");
			if (enablenormalmaps)
			{
				out.Write("if(" I_FLAGS ".x != 0)\n{\n");
				out.Write("_norm0 = perturb_normal(_norm0, pos, mapcoord, normalmap);\n");
				out.Write("}\n");
			}
		}
		// Only col0 and col1 are needed so discard the remaining components
		uid_data.components = (components >> 11) & 3;
		GenerateLightingShader<T, Write_Code>(out, uid_data.lighting, components, I_PMATERIALS, I_PLIGHTS, "colors_", "col", xfr, Use_integer_math);
	}
	else
	{
		if (Write_Code)
		{
			out.Write("\tfloat4 col0 = colors_0;\n");
			out.Write("\tfloat4 col1 = colors_1;\n");
		}
	}
	if (Write_Code)
	{
		if (numTexgen < 7)
			out.Write("clipPos = float4(rawpos.x, rawpos.y, clipPos.z, clipPos.w);\n");
		else
			out.Write("clipPos = float4(rawpos.x, rawpos.y, uv2.w, uv3.w);\n");
	}
	for (u32 i = 0; i < numStages; i++)
		WriteStage<T, Write_Code, ApiType, Use_integer_math>(out, uid_data, i, swapModeTable, bpm); // build the equation for this stage

	if (Write_Code)
	{
		u32 colorCdest = bpm.combiners[numStages - 1].colorC.dest;
		u32 alphaCdest = bpm.combiners[numStages - 1].alphaC.dest;
		if (numStages)
		{
			// The results of the last texenv stage are put onto the screen,
			// regardless of the used destination register			
			if (colorCdest != 0)
			{
				out.Write("prev.rgb = %s;\n", tevCOutputTable[colorCdest]);
			}
			if (alphaCdest != 0)
			{
				out.Write("prev.a = %s;\n", tevAOutputTable[alphaCdest]);
			}
		}
		// emulation of unsigned 8 overflow
		if (TevOverflowState[tevCOutputSourceMap[colorCdest]] || TevOverflowState[tevAOutputSourceMap[alphaCdest]])
			out.Write("prev = CHK_O_U8(prev);\n");
	}

	uid_data.Pretest = Pretest;
	// depth texture can safely be ignored if the result won't be written to the depth buffer (early_ztest) and isn't used for fog either
	const bool skip_ztexture = !per_pixel_depth && !bpm.fog.c_proj_fsel.fsel;

	uid_data.ztex_op = bpm.ztex2.op;
	uid_data.forced_early_z = forced_early_z;
	uid_data.fast_depth_calc = g_ActiveConfig.bFastDepthCalc;
	uid_data.early_ztest = bpm.UseEarlyDepthTest();
	uid_data.fog_fsel = bpm.fog.c_proj_fsel.fsel;

	// NOTE: Fragment may not be discarded if alpha test always fails and early depth test is enabled 
	// (in this case we need to write a depth value if depth test passes regardless of the alpha testing result)
	if (Pretest != AlphaTest::PASS)
		WriteAlphaTest<T, Write_Code, ApiType, Use_integer_math>(out, uid_data, dstAlphaMode, per_pixel_depth, bpm);

	if (Write_Code)
	{
		// D3D9 doesn't support readback of depth in pixel shader, so we always have to calculate it again.
		// This shouldn't be a performance issue as the written depth is usually still from perspective division
		// but this isn't true for z-textures, so there will be depth issues between enabled and disabled z-textures fragments
		if (bpm.genMode.zfreeze)
		{
			out.Write("\tfloat2 screenpos = rawpos.xy * " I_EFBSCALE".xy;\n");

			// Opengl has reversed vertical screenspace coordiantes
			if (ApiType == API_OPENGL)
			{
				out.Write("\tscreenpos.y = %i - screenpos.y;\n", EFB_HEIGHT);
			}
			if (Use_integer_math)
			{
				out.Write("wu zCoord = wu(" I_ZSLOPE".z + " I_ZSLOPE".x * screenpos.x + " I_ZSLOPE".y * screenpos.y);\n");
			}
			else
			{
				out.Write("wu zCoord = (" I_ZSLOPE".z + " I_ZSLOPE".x * screenpos.x + " I_ZSLOPE".y * screenpos.y) / 16777216.0;\n");
			}
		}
		else if ((ApiType == API_OPENGL || ApiType == API_D3D11) && g_ActiveConfig.bFastDepthCalc)
		{
			if (Use_integer_math)
			{
				out.Write("wu zCoord = wuround((%srawpos.z) * 16777216.0);\n", ApiType == API_OPENGL ? "" : "1.0 - ");
			}
			else
				out.Write("wu zCoord = wu(rawpos.z);\n");
		}
		else
		{
			// the screen space depth value = far z + (clip z / clip w) * z range
			if (Use_integer_math)
				out.Write("wu zCoord = " I_ZBIAS"[1].x + wuround((clipPos.z / clipPos.w) * float(" I_ZBIAS"[1].y));\n");
			else
				out.Write("wu zCoord = round(" I_ZBIAS "[1].x + ((clipPos.z / clipPos.w) * " I_ZBIAS "[1].y)) / 16777216.0;\n");
		}
		if (Use_integer_math)
		{
			out.Write("zCoord = clamp(zCoord, 0, 0xFFFFFF);\n");
		}
		else
		{
			out.Write("zCoord = clamp(zCoord, 0.0, 1.0);\n");
		}
		// Note: z-textures are not written to depth buffer if early depth test is used
		if (per_pixel_depth && bpm.UseEarlyDepthTest())
		{
			if (Use_integer_math)
				out.Write("\tdepth = %s(float(zCoord) / 16777216.0);\n", ApiType == API_OPENGL ? "" : "1.0 - ");
			else
				out.Write("\tdepth = %s zCoord;\n", ApiType == API_OPENGL ? "" : "1.0 - ");
		}
		// Note: depth texture output is only written to depth buffer if late depth test is used
		// theoretical final depth value is used for fog calculation, though, so we have to emulate ztextures anyway
		if (bpm.ztex2.op != ZTEXTURE_DISABLE && !skip_ztexture)
		{
			if (Use_integer_math)
			{
				out.Write("zCoord = idot(" I_ZBIAS"[0].xyzw, tex_t.xyzw) + " I_ZBIAS"[1].w %s;\n",
					(bpmem.ztex2.op == ZTEXTURE_ADD) ? "+ zCoord" : "");
				out.Write("zCoord = zCoord & 0xFFFFFF;\n");
			}
			else
			{
				// use the texture input of the last texture stage (tex_t), hopefully this has been read and is in correct format...
				out.Write("zCoord = dot(" I_ZBIAS"[0].xyzw, tex_t.xyzw * (1.0/255.0)) + " I_ZBIAS "[1].w %s;\n",
					(bpm.ztex2.op == ZTEXTURE_ADD) ? "+ zCoord" : "");
				// U24 overflow emulation : disabled find out why this make nvidia compiler crasy
				// out.Write("zCoord = zCoord > 1.0f ? (zCoord - 1.0f) : zCoord;\n");
			}
		}
		if (per_pixel_depth && bpm.UseLateDepthTest())
		{
			if (Use_integer_math)
				out.Write("\tdepth = %s(float(zCoord) / 16777216.0);\n", ApiType == API_OPENGL ? "" : "1.0 - ");
			else
				out.Write("\tdepth = %s zCoord;\n", ApiType == API_OPENGL ? "" : "1.0 - ");
		}
	}
	if (dstAlphaMode == DSTALPHA_ALPHA_PASS)
	{
		if (Write_Code)
			out.Write("ocol0 = float4(prev.rgb," I_ALPHA ".a) * (1.0/255.0);\n");
	}
	else
	{
		WriteFog<T, Write_Code, Use_integer_math>(out, uid_data, bpm);
		if (Write_Code)
			out.Write("ocol0 = float4(prev) * (1.0/255.0);\n");
	}
	if (Write_Code)
	{
		// Use dual-source color blending to perform dst alpha in a single pass
		if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
		{
			if (ApiType & API_D3D9)
			{
				// alpha component must be 0 or the shader will not compile (Direct3D 9Ex restriction)
				// Colors will be blended against the color from ocol1 in D3D 9...
				out.Write("ocol1 = float4(prev.a, prev.a, prev.a, 0.0) * (1.0/255.0);\n");
			}
			else
			{
				// Colors will be blended against the alpha from ocol1...
				out.Write("ocol1 = float4(prev) * (1.0/255.0);\n");
			}
			// ...and the alpha from ocol0 will be written to the framebuffer.
			out.Write("ocol0.a = " I_ALPHA ".a*(1.0/255.0);\n");
		}
	}
	if (uid_data.bounding_box)
	{

		if (Write_Code)
		{
			const char* atomic_op = ApiType == API_OPENGL ? "atomic" : "Interlocked";
			out.Write(
				"\tif(bbox_data[0] > int(rawpos.x)) %sMin(bbox_data[0], int(rawpos.x));\n"
				"\tif(bbox_data[1] < int(rawpos.x)) %sMax(bbox_data[1], int(rawpos.x));\n"
				"\tif(bbox_data[2] > int(rawpos.y)) %sMin(bbox_data[2], int(rawpos.y));\n"
				"\tif(bbox_data[3] < int(rawpos.y)) %sMax(bbox_data[3], int(rawpos.y));\n",
				atomic_op, atomic_op, atomic_op, atomic_op);
		}
	}
	if (Write_Code)
	{
		out.Write("}\n");
		if (codebuffer[PIXELSHADERGEN_BUFFERSIZE - 1] != 0x7C)
			PanicAlert("PixelShader generator - buffer too small, canary has been eaten!");
	}

	if (uidPresent)
	{
		out.CalculateUIDHash();
	}
}

template<class T, API_TYPE ApiType, bool use_integer_math>
static inline void WriteFetchStageTexture(T& out, int n, const bool LoadMaterial, const char swapModeTable[4][5], const BPMemory &bpm)
{
	int texcoord = bpm.tevorders[n / 2].getTexCoord(n & 1);
	bool bHasTexCoord = (u32)texcoord < bpm.genMode.numtexgens;
	bool bHasIndStage = bpm.tevind[n].bt < bpm.genMode.numindstages;
	// HACK to handle cases where the tex gen is not enabled
	if (!bHasTexCoord)
		texcoord = 0;
	out.Write("\n{\n");
	out.Write("wu3 tevcoord=wu3(0,0,0);\n");
	if (bHasIndStage)
	{
		out.Write("// indirect op\n");
		// perform the indirect op on the incoming regular coordinates using indtex%d as the offset coords
		if (bpm.tevind[n].bs != ITBA_OFF)
		{
			if (use_integer_math)
			{
				static const char *tevIndAlphaSel[] = { "", "x", "y", "z" };
				static const char *tevIndAlphaMask[] = { "248", "224", "240", "248" }; // 0b11111000, 0b11100000, 0b11110000, 0b11111000
				out.Write("a_bump = indtex%d.%s & %s;\n",
					bpm.tevind[n].bt,
					tevIndAlphaSel[bpm.tevind[n].bs],
					tevIndAlphaMask[bpm.tevind[n].fmt]);
			}
			else
			{
				// lest explain a little what is done here
				// a_bump is taked from the upper bits that are not taked for indirect texturing
				// if all the bits are used for indirect texturing then only the upper 5 bits are used for a_bump
				// so to do this bit masking is used for example 
				// a_bump = x & 0xE0; for 3 bits
				// a_bump = x & 0xF0; for 4 bits
				// a_bump = x & 0xF8; for 5 bits
				// there is no support for bitmasking in older hardware
				// and in newer hardware is slower than using pure float operations
				// so we have to emulate it
				// the exact formula for float masking emulation of the upper bits of a number is: 
				// having x as the number to mask stored in a float
				// nb as the number of bits to mask
				// and n the (wordlen - nb) in this case (8 - nb)
				// then result = trunc(x * /(2^n)) * (2^n)
				// so for nb = 3 bit this will be n = 5  result = floor(x * (255.0/32.0)) * (32.0/255.0f);
				// to optimize a litle al the coeficient are precalculated to avoid slowing thigs more than needed

				static const char *tevIndAlphaSel[] = { "", "x", "y", "z" };
				static const char *tevIndAlphaScale[] = { "(1.0/8.0)", "(1.0/32.0)", "(1.0/16.0)", "(1.0/8.0)" };
				static const char *tevIndAlphaNormFactor[] =
				{
					"8.0",	// 5 bits
					"32.0",	// 3 bits
					"16.0",	// 4 bits
					"8.0"	// 5 bits
				};

				out.Write("a_bump = trunc(indtex%d.%s * %s) * %s;\n",
					bpm.tevind[n].bt,
					tevIndAlphaSel[bpm.tevind[n].bs],
					tevIndAlphaScale[bpm.tevind[n].fmt],
					tevIndAlphaNormFactor[bpm.tevind[n].fmt]);
			}
		}

		if (bpm.tevind[n].mid != 0)
		{
			if (use_integer_math)
			{
				static const char *tevIndFmtMask[] = { "255", "31", "15", "7" };
				out.Write("wu3 indtevcrd%d = indtex%d & %s;\n", n, bpmem.tevind[n].bt, tevIndFmtMask[bpmem.tevind[n].fmt]);
			}
			else
			{
				static const char *tevIndFmtScale[] =
				{
					"(1.0/256.0)",	// 8 bits (& 0xFF)
					"(1.0/32.0)",	// 5 bits (& 0x1F)
					"(1.0/16.0)",	// 4 bits (& 0x0F)
					"(1.0/8.0)"		// 3 bits (& 0x07)
				};

				static const char *tevIndFmtNormFactor[] =
				{
					"256.0",// 8 bits
					"32.0",	// 5 bits
					"16.0",	// 4 bits
					"8.0"	// 3 bits
				};

				// format
				// to mask the lower bits the formula is:
				// having x as number to mask stored in a float
				// nb as the number of bits to mask			
				// then result = frac(x * 255.0 / (2^nb) * (2^nb)
				// for 3 bits result = frac(x * 255.0 / 8.0) * 8.0
				if (bpm.tevind[n].fmt > 0)
				{
					out.Write("wu3 indtevcrd%d = round(frac(indtex%d * %s) * %s);\n",
						n,
						bpm.tevind[n].bt,
						tevIndFmtScale[bpm.tevind[n].fmt],
						tevIndFmtNormFactor[bpm.tevind[n].fmt]);
				}
				else
				{
					out.Write("wu3 indtevcrd%d = indtex%d;\n",
						n,
						bpm.tevind[n].bt);
				}
			}



			static const char *tevIndBiasField[] = { "", "x", "y", "xy", "z", "xz", "yz", "xyz" }; // indexed by bias
			static const char *tevIndBiasAdd[] = { "wu(-128)", "wu(1)", "wu(1)", "wu(1)" }; // indexed by fmt
																							// bias
			if (bpm.tevind[n].bias == ITB_S || bpm.tevind[n].bias == ITB_T || bpm.tevind[n].bias == ITB_U)
				out.Write("indtevcrd%d.%s += %s;\n", n, tevIndBiasField[bpm.tevind[n].bias], tevIndBiasAdd[bpm.tevind[n].fmt]);
			else if (bpm.tevind[n].bias == ITB_ST || bpm.tevind[n].bias == ITB_SU || bpm.tevind[n].bias == ITB_TU)
				out.Write("indtevcrd%d.%s += wu2(%s, %s);\n", n, tevIndBiasField[bpm.tevind[n].bias], tevIndBiasAdd[bpm.tevind[n].fmt], tevIndBiasAdd[bpm.tevind[n].fmt]);
			else if (bpm.tevind[n].bias == ITB_STU)
				out.Write("indtevcrd%d.%s += wu3(%s, %s, %s);\n", n, tevIndBiasField[bpm.tevind[n].bias], tevIndBiasAdd[bpm.tevind[n].fmt], tevIndBiasAdd[bpm.tevind[n].fmt], tevIndBiasAdd[bpm.tevind[n].fmt]);

			// multiply by offset matrix and scale
			if (bpm.tevind[n].mid <= 3)
			{
				int mtxidx = 2 * (bpm.tevind[n].mid - 1);
				if (use_integer_math)
				{
					out.Write("wu2 indtevtrans%d = wu2(idot(" I_INDTEXMTX "[%d].xyz, indtevcrd%d), idot(" I_INDTEXMTX "[%d].xyz, indtevcrd%d));\n",
						n, mtxidx, n, mtxidx + 1, n);
				}
				else
				{
					out.Write("wu2 indtevtrans%d = round(wu2(dot(" I_INDTEXMTX "[%d].xyz, indtevcrd%d), dot(" I_INDTEXMTX "[%d].xyz, indtevcrd%d)));\n",
						n, mtxidx, n, mtxidx + 1, n);
				}

				out.Write("indtevtrans%d = BSHR(indtevtrans%d, wu(3));\n", n, n);
				out.Write("indtevtrans%d = BSH(indtevtrans%d, " I_INDTEXMTX "[%d].w);\n", n, n, mtxidx);
			}
			else if (bpm.tevind[n].mid <= 7 && bHasTexCoord)
			{ // s matrix
				_assert_(bpm.tevind[n].mid >= 5);
				int mtxidx = 2 * (bpm.tevind[n].mid - 5);
				out.Write("wu2 indtevtrans%d = wu2(uv%d.xy * indtevcrd%d.xx);\n", n, texcoord, n);
				out.Write("indtevtrans%d = BSHR(indtevtrans%d, wu(8));\n", n, n);
				out.Write("indtevtrans%d = BSH(indtevtrans%d, " I_INDTEXMTX "[%d].w);\n", n, n, mtxidx);
			}
			else if (bpm.tevind[n].mid <= 11 && bHasTexCoord)
			{ // t matrix
				_assert_(bpm.tevind[n].mid >= 9);
				int mtxidx = 2 * (bpm.tevind[n].mid - 9);
				out.Write("wu2 indtevtrans%d = wu2(uv%d.xy * indtevcrd%d.yy);\n", n, texcoord, n);
				out.Write("indtevtrans%d = BSHR(indtevtrans%d, wu(8));\n", n, n);
				out.Write("indtevtrans%d = BSH(indtevtrans%d, " I_INDTEXMTX "[%d].w);\n", n, n, mtxidx);
			}
			else
			{
				out.Write("wu2 indtevtrans%d = wu2(0,0);\n", n);
			}
		}
		else
		{
			out.Write("wu2 indtevtrans%d = wu2(0,0);\n", n);
		}
		// ---------
		// Wrapping
		// ---------
		static const char *tevIndWrapStart[] = { "wu(0)", "wu(256*128)", "wu(128*128)", "wu(64*128)", "wu(32*128)", "wu(16*128)", "wu(1)" };
		// wrap S
		if (bpm.tevind[n].sw == ITW_OFF)
			out.Write("wrappedcoord.x = wu(uv%d.x);\n", texcoord);
		else if (bpm.tevind[n].sw == ITW_0)
			out.Write("wrappedcoord.x = wu(0);\n");
		else
			out.Write("wrappedcoord.x = remainder(wu(uv%d.x), %s);\n", texcoord, tevIndWrapStart[bpm.tevind[n].sw]);

		// wrap T
		if (bpm.tevind[n].tw == ITW_OFF)
			out.Write("wrappedcoord.y = wu(uv%d.y);\n", texcoord);
		else if (bpm.tevind[n].tw == ITW_0)
			out.Write("wrappedcoord.y = wu(0);\n");
		else
			out.Write("wrappedcoord.y = remainder(wu(uv%d.y), %s);\n", texcoord, tevIndWrapStart[bpm.tevind[n].tw]);

		if (bpm.tevind[n].fb_addprev) // add previous tevcoord
			out.Write("tevcoord.xy += wrappedcoord + indtevtrans%d;\n", n);
		else
			out.Write("tevcoord.xy = wrappedcoord + indtevtrans%d;\n", n);

		if (use_integer_math)
		{
			// Emulate s24 overflows
			out.Write("tevcoord.xy = (tevcoord.xy << 8) >> 8;\n");
		}
	}
	if (bpm.tevorders[n / 2].getEnable(n & 1))
	{
		if (!bHasIndStage)
		{
			// calc tevcord
			if (bHasTexCoord)
				out.Write("tevcoord.xy = wu2(uv%d.xy);\n", texcoord);
			else
				out.Write("tevcoord.xy = wu2(0,0);\n");
		}
		const char *texswap = swapModeTable[bpm.combiners[n].alphaC.tswap];
		int texmap = bpm.tevorders[n / 2].getTexMap(n & 1);
		out.Write("float2 stagecoord = float2(tevcoord.xy) * (1.0/128.0);\n");
		out.Write("tex_ta[%i] = ", n);
		SampleTexture<T, ApiType>(out, "stagecoord", texswap, texmap);
		if (LoadMaterial)
		{
			out.Write("if(" I_FLAGS ".x & %i)\n{\n", 1 << texmap);
			out.Write("mapcoord = stagecoord;");
			out.Write("float3 nrmap = ");
			SampleTextureRAW<T, ApiType>(out, "(stagecoord)", "rgb", "0.0", texmap);
			out.Write("nrmap = normalize(nrmap * 255.0/127.0 - 128.0/127.0);\n");
			// Extact z value from x and y
			// Disabled until specular light is implemented
			//out.Write("nrmap.z = sqrt(1.0 - dot(nrmap.xy, nrmap.xy));\n");
			out.Write("normalmap = normalize(float3(nrmap.xy + normalmap.xy, nrmap.z * normalmap.z));\n}\n");
		}
	}
	else
	{
		out.Write("tex_ta[%i] = wu4(255,255,255,255);\n", n);
	}
	out.Write("\n}\n");
}

template<class T, bool Write_Code, API_TYPE ApiType, bool use_integer_math>
static inline void WriteStage(T& out, pixel_shader_uid_data& uid_data, int n, const char swapModeTable[4][5], const BPMemory &bpm)
{
	int texcoord = bpm.tevorders[n / 2].getTexCoord(n & 1);
	bool bHasTexCoord = (u32)texcoord < bpm.genMode.numtexgens;
	bool bHasIndStage = bpm.tevind[n].bt < bpm.genMode.numindstages;
	// HACK to handle cases where the tex gen is not enabled
	if (!bHasTexCoord)
		texcoord = 0;
	if (Write_Code)
	{
		out.Write("// TEV stage %d\n", n);
	}

	uid_data.stagehash[n].hasindstage = bHasIndStage;
	uid_data.stagehash[n].tevorders_texcoord = texcoord;
	if (bHasIndStage)
	{
		uid_data.stagehash[n].tevind = bpm.tevind[n].hex & 0x7FFFFF;
	}

	const TevStageCombiner::ColorCombiner &cc = bpm.combiners[n].colorC;
	const TevStageCombiner::AlphaCombiner &ac = bpm.combiners[n].alphaC;

	uid_data.stagehash[n].cc = cc.hex & 0xFFFFFF;
	uid_data.stagehash[n].ac = ac.hex & 0xFFFFF0; // Storing rswap and tswap later

	if (cc.UsedAsInput(TEVCOLORARG_RASA) || cc.UsedAsInput(TEVCOLORARG_RASC) || ac.UsedAsInput(TEVALPHAARG_RASA))
	{
		const int i = bpm.combiners[n].alphaC.rswap;
		uid_data.stagehash[n].ac |= bpm.combiners[n].alphaC.rswap;
		uid_data.stagehash[n].tevksel_swap1a = bpm.tevksel[i * 2].swap1;
		uid_data.stagehash[n].tevksel_swap2a = bpm.tevksel[i * 2].swap2;
		uid_data.stagehash[n].tevksel_swap1b = bpm.tevksel[i * 2 + 1].swap1;
		uid_data.stagehash[n].tevksel_swap2b = bpm.tevksel[i * 2 + 1].swap2;
		uid_data.stagehash[n].tevorders_colorchan = bpm.tevorders[n / 2].getColorChan(n & 1);

		const char *rasswap = swapModeTable[bpm.combiners[n].alphaC.rswap];
		if (Write_Code)
		{
			int rasindex = bpm.tevorders[n / 2].getColorChan(n & 1);
			if (rasindex == 0 && !tevRascolor0_Expanded)
			{
				out.Write("col0 = round(col0 * 255.0);\n");
				tevRascolor0_Expanded = true;
			}
			if (rasindex == 1 && !tevRascolor1_Expanded)
			{
				out.Write("col1 = round(col1 * 255.0);\n");
				tevRascolor1_Expanded = true;
			}
			out.Write("ras_t = %s.%s;\n", tevRasTable[rasindex], rasswap);
			TevOverflowState[tevSources::RASC] = rasindex < 2;
			TevOverflowState[tevSources::RASA] = rasindex < 2;
		}
	}

	uid_data.stagehash[n].tevorders_enable = bpm.tevorders[n / 2].getEnable(n & 1);
	if (bpm.tevorders[n / 2].getEnable(n & 1))
	{
		const int i = bpm.combiners[n].alphaC.tswap;
		uid_data.stagehash[n].ac |= bpm.combiners[n].alphaC.tswap << 2;
		uid_data.stagehash[n].tevksel_swap1c = bpm.tevksel[i * 2].swap1;
		uid_data.stagehash[n].tevksel_swap2c = bpm.tevksel[i * 2].swap2;
		uid_data.stagehash[n].tevksel_swap1d = bpm.tevksel[i * 2 + 1].swap1;
		uid_data.stagehash[n].tevksel_swap2d = bpm.tevksel[i * 2 + 1].swap2;

		uid_data.stagehash[n].tevorders_texmap = bpm.tevorders[n / 2].getTexMap(n & 1);

		const char *texswap = swapModeTable[bpm.combiners[n].alphaC.tswap];
		int texmap = bpm.tevorders[n / 2].getTexMap(n & 1);
		uid_data.SetTevindrefTexmap(i, texmap);

	}
	if (Write_Code)
		out.Write("tex_t = tex_ta[%i];", n);


	if (cc.UsedAsInput(TEVCOLORARG_KONST) || ac.UsedAsInput(TEVALPHAARG_KONST))
	{
		int kc = bpm.tevksel[n / 2].getKC(n & 1);
		int ka = bpm.tevksel[n / 2].getKA(n & 1);
		uid_data.stagehash[n].tevksel_kc = kc;
		uid_data.stagehash[n].tevksel_ka = ka;
		if (Write_Code)
		{
			out.Write("konst_t = wu4(%s,%s);\n", tevKSelTableC[kc], tevKSelTableA[ka]);
			TevOverflowState[tevSources::KONST] = kc > 11 || ka > 15;
		}
	}
	if (Write_Code)
	{
		out.Write("tin_a = %s(wu4(%s,%s));\n", TevOverflowState[cc.a] || TevOverflowState[AInputSourceMap[ac.a]] ? "CHK_O_U8" : "", tevCInputTable[cc.a], tevAInputTable[ac.a]);
		out.Write("tin_b = %s(wu4(%s,%s));\n", TevOverflowState[cc.b] || TevOverflowState[AInputSourceMap[ac.b]] ? "CHK_O_U8" : "", tevCInputTable[cc.b], tevAInputTable[ac.b]);
		out.Write("tin_c = %s(wu4(%s,%s));\n", TevOverflowState[cc.c] || TevOverflowState[AInputSourceMap[ac.c]] ? "CHK_O_U8" : "", tevCInputTable[cc.c], tevAInputTable[ac.c]);

		bool normalize_c_rgb = cc.c != TEVCOLORARG_ZERO &&  cc.bias != TEVBIAS_COMPARE;
		bool normalize_c_a = ac.c != TEVALPHAARG_ZERO &&  ac.bias != TEVBIAS_COMPARE;
		if (normalize_c_rgb || normalize_c_a)
		{
			const char* cswisle = normalize_c_rgb && normalize_c_a ? "" : (normalize_c_rgb ? ".rgb" : ".a");
			out.Write("tin_c%s = BOR(tin_c%s, BSHR(tin_c%s, 7));\n", cswisle, cswisle, cswisle);
		}
		out.Write("tin_d = wu4(%s,%s);\n", tevCInputTable[cc.d], tevAInputTable[ac.d]);

		TevOverflowState[tevCOutputSourceMap[cc.dest]] = !cc.clamp;
		TevOverflowState[tevAOutputSourceMap[ac.dest]] = !ac.clamp;

		out.Write("// color combine\n");
		out.Write("%s = clamp(", tevCOutputTable[cc.dest]);
		// combine the color channel
		if (cc.bias != TEVBIAS_COMPARE) // if not compare
		{
			//normal color combiner goes here
			if (use_integer_math)
				WriteTevRegularI<TEVCOLORARG_ZERO, TEVCOLORARG_ONE>(out, ".rgb", cc.bias, cc.op, cc.clamp, cc.shift, cc.a, cc.b, cc.c, cc.d);
			else
				WriteTevRegular<TEVCOLORARG_ZERO, TEVCOLORARG_ONE>(out, ".rgb", cc.bias, cc.op, cc.clamp, cc.shift, cc.a, cc.b, cc.c, cc.d);

		}
		else
		{
			//compare color combiner goes here
			int cmp = (cc.shift << 1) | cc.op; // comparemode stored here
			if (use_integer_math)
				WriteTevCompareI(out, 0, cmp);
			else
				WriteTevCompare(out, 0, cmp);
		}
		if (cc.clamp)
		{
			out.Write(",wu(0),wu(255));\n");
		}
		else
		{
			out.Write(",wu(-1024),wu(1023));\n");
		}

		out.Write("// alpha combine\n");
		out.Write("%s = clamp(", tevAOutputTable[ac.dest]);
		if (ac.bias != TEVBIAS_COMPARE) // if not compare
		{
			// 8 is used because alpha stage don't have ONE input so a number outside range is used
			if (use_integer_math)
				WriteTevRegularI<TEVALPHAARG_ZERO, 8>(out, ".a", ac.bias, ac.op, ac.clamp, ac.shift, ac.a, ac.b, ac.c, ac.d);
			else
				WriteTevRegular<TEVALPHAARG_ZERO, 8>(out, ".a", ac.bias, ac.op, ac.clamp, ac.shift, ac.a, ac.b, ac.c, ac.d);
		}
		else
		{
			//compare alpha combiner goes here			
			int cmp = (ac.shift << 1) | ac.op; // comparemode stored here
			if (use_integer_math)
				WriteTevCompareI(out, 1, cmp);
			else
				WriteTevCompare(out, 1, cmp);
		}
		if (ac.clamp)
		{
			out.Write(",wu(0),wu(255));\n\n");
		}
		else
		{
			out.Write(",wu(-1024),wu(1023));\n\n");
		}
		out.Write("// TEV done\n");
	}
}

template<int TEVARG_ZERO, int TEVARG_ONE, class T>
static inline void WriteTevRegularI(T& out, const char* components, int bias, int op, int clamp, int shift, int a, int b, int c, int d)
{
	const char *tevScaleTableLeft[] =
	{
		"",       // SCALE_1
		" << 1",  // SCALE_2
		" << 2",  // SCALE_4
		"",       // DIVIDE_2
	};

	const char *tevScaleTableRight[] =
	{
		"",       // SCALE_1
		"",       // SCALE_2
		"",       // SCALE_4
		" >> 1",  // DIVIDE_2
	};

	const char *tevLerpBias[] = // indexed by 2*op+(shift==3)
	{
		"",
		" + 128",
		"",
		" + 127",
	};

	const char *tevBiasTable[] =
	{
		"",        // ZERO,
		" + 128",  // ADDHALF,
		" - 128",  // SUBHALF,
		"",
	};

	const char *tevOpTable[] = {
		"+",      // TEVOP_ADD = 0,
		"-",      // TEVOP_SUB = 1,
	};

	// Regular TEV stage: (d + bias + lerp(a,b,c)) * scale
	// The GC/Wii GPU uses a very sophisticated algorithm for scale-lerping:
	// - c is scaled from 0..255 to 0..256, which allows dividing the result by 256 instead of 255
	// - if scale is bigger than one, it is moved inside the lerp calculation for increased accuracy
	// - a rounding bias is added before dividing by 256
	out.Write("((((tin_d%s%s)%s)", components, tevBiasTable[bias], tevScaleTableLeft[shift]);
	out.Write(" %s ", tevOpTable[op]);
	if (a == b || c == TEVARG_ZERO)
	{
		out.Write("((((tin_a%s << 8)%s)%s) >> 8)",
			components, tevScaleTableLeft[shift], tevLerpBias[2 * op + (shift != 3)]);
	}
	else if (c == TEVARG_ONE)
	{
		out.Write("((((tin_b%s << 8)%s)%s) >> 8)",
			components, tevScaleTableLeft[shift], tevLerpBias[2 * op + (shift != 3)]);
	}
	else if (a == TEVARG_ZERO)
	{
		out.Write("((((tin_b%s*tin_c%s)%s)%s) >> 8)",
			components, components,
			tevScaleTableLeft[shift], tevLerpBias[2 * op + (shift != 3)]);
	}
	else if (b == TEVARG_ZERO)
	{
		out.Write("((((tin_a%s*(256 - tin_c%s))%s)%s) >> 8)",
			components, components,
			tevScaleTableLeft[shift], tevLerpBias[2 * op + (shift != 3)]);
	}
	else
	{
		out.Write("((((tin_a%s*256 + (tin_b%s - tin_a%s) * tin_c%s)%s)%s) >> 8)",
			components, components, components, components,
			tevScaleTableLeft[shift], tevLerpBias[2 * op + (shift != 3)]);
	}
	out.Write(")%s)", tevScaleTableRight[shift]);
}

template<int TEVARG_ZERO, int TEVARG_ONE, class T>
static inline void WriteTevRegular(T& out, const char* components, int bias, int op, int clamp, int shift, int a, int b, int c, int d)
{
	const char *tevScaleTableLeft[] =
	{
		"",       // SCALE_1
		" * 2.0",  // SCALE_2
		" * 4.0",  // SCALE_4
		"",       // DIVIDE_2
	};

	const char *tevScaleTableRight[] =
	{
		"",       // SCALE_1
		"",       // SCALE_2
		"",       // SCALE_4
		" * 0.5",  // DIVIDE_2
	};

	const char *tevLerpBias[] = // indexed by 2*op+(shift==3)
	{
		"",
		" + 128.0",
		"",
		" + 127.0",
	};

	const char *tevBiasTable[] =
	{
		"",        // ZERO,
		" + 128.0",  // ADDHALF,
		" - 128.0",  // SUBHALF,
		"",
	};

	const char *tevOpTable[] = {
		"+",      // TEVOP_ADD = 0,
		"-",      // TEVOP_SUB = 1,
	};

	// Regular TEV stage: (d + bias + lerp(a,b,c)) * scale
	// The GC/Wii GPU uses a very sophisticated algorithm for scale-lerping:
	// - c is scaled from 0..255 to 0..256, which allows dividing the result by 256 instead of 255
	// - if scale is bigger than one, it is moved inside the lerp calculation for increased accuracy
	// - a rounding bias is added before dividing by 256
	out.Write("%s(", shift == 3 ? "trunc" : "");
	out.Write("(((tin_d%s%s)%s)", components, tevBiasTable[bias], tevScaleTableLeft[shift]);
	out.Write(" %s ", tevOpTable[op]);
	if (a == b || c == TEVARG_ZERO)
	{
		out.Write("trunc((((tin_a%s*256.0)%s)%s)*(1.0/256.0))",
			components, tevScaleTableLeft[shift], tevLerpBias[2 * op + (shift != 3)]);
	}
	else if (c == TEVARG_ONE)
	{
		out.Write("trunc((((tin_b%s*256.0)%s)%s)*(1.0/256.0))",
			components, tevScaleTableLeft[shift], tevLerpBias[2 * op + (shift != 3)]);
	}
	else if (a == TEVARG_ZERO)
	{
		out.Write("trunc((((tin_b%s*tin_c%s)%s)%s)*(1.0/256.0))",
			components, components,
			tevScaleTableLeft[shift], tevLerpBias[2 * op + (shift != 3)]);
	}
	else if (b == TEVARG_ZERO)
	{
		out.Write("trunc((((tin_a%s*(256.0 - tin_c%s))%s)%s)*(1.0/256.0))",
			components, components,
			tevScaleTableLeft[shift], tevLerpBias[2 * op + (shift != 3)]);
	}
	else
	{
		out.Write("trunc((((tin_a%s*256.0 + (tin_b%s-tin_a%s)*tin_c%s)%s)%s)*(1.0/256.0))",
			components, components, components, components,
			tevScaleTableLeft[shift], tevLerpBias[2 * op + (shift != 3)]);
	}
	out.Write(")%s)", tevScaleTableRight[shift]);
}

template<class T>
static inline void WriteTevCompareI(T& out, int components, int cmp)
{
	static const char *TEVCMPComponents[] =
	{
		".rgb",
		".a"
	};

	static const char *TEVCMPZero[] =
	{
		"int3(0,0,0)",
		"0"
	};

	static const char *TEVCMPOPTable[] =
	{
		"((tin_a.r > tin_b.r) ? tin_c%s : %s)", // TEVCMP_R8_GT
		"((tin_a.r == tin_b.r) ? tin_c%s : %s)", // TEVCMP_R8_EQ
		"((idot(tin_a.rgb, c16) >  idot(tin_b.rgb, c16)) ? tin_c%s : %s)", // TEVCMP_GR16_GT
		"((idot(tin_a.rgb, c16) == idot(tin_b.rgb, c16)) ? tin_c%s : %s)", // TEVCMP_GR16_EQ
		"((idot(tin_a.rgb, c24) >  idot(tin_b.rgb, c24)) ? tin_c%s : %s)", // TEVCMP_BGR24_GT
		"((idot(tin_a.rgb, c24) == idot(tin_b.rgb, c24)) ? tin_c%s : %s)", // TEVCMP_BGR24_EQ
		// Only for RGB
		"(max(int3(sign(tin_a.rgb - tin_b.rgb - 0.5)), int3(0,0,0)) * tin_c.rgb)", // TEVCMP_RGB8_GT
		"((int3(1,1,1) - sign(abs(tin_a.rgb - tin_b.rgb))) * tin_c.rgb)", // TEVCMP_RGB8_EQ
		// Only for ALPHA
		"((tin_a.a  > tin_b.a) ? tin_c.a : 0)", // TEVCMP_A8_GT
		"((tin_a.a == tin_b.a) ? tin_c.a : 0)", // TEVCMP_A8_EQ
	};
	out.Write("tin_d%s+", TEVCMPComponents[components]);
	if (cmp < 6)
	{
		// same function for alpha and rgb
		out.Write(TEVCMPOPTable[cmp], TEVCMPComponents[components], TEVCMPZero[components]);
	}
	else
	{
		// if rgb just use the regular cmp
		// for alpha use the last 2 functions
		out.Write(TEVCMPOPTable[cmp + (components << 1)]);
	}
}

template<class T>
static inline void WriteTevCompare(T& out, int components, int cmp)
{
	static const char *TEVCMPComponents[] =
	{
		".rgb",
		".a"
	};

	static const char *TEVCMPZero[] =
	{
		"float3(0.0,0.0,0.0)",
		"0.0"
	};

	static const char *TEVCMPOPTable[] =
	{
		"((tin_a.r >= (tin_b.r + 0.5)) ? tin_c%s : %s)", // TEVCMP_R8_GT
		"((abs(tin_a.r - tin_b.r) < 0.5) ? tin_c%s : %s)", // TEVCMP_R8_EQ
		"((dot(tin_a.rgb, c16) >=  (dot(tin_b.rgb, c16) + 0.5)) ? tin_c%s : %s)", // TEVCMP_GR16_GT
		"((abs(dot(tin_a.rgb, c16) - dot(tin_b.rgb, c16)) < 0.5) ? tin_c%s : %s)", // TEVCMP_GR16_EQ
		"((dot(tin_a.rgb, c24) >=  (dot(tin_b.rgb, c24) + 0.5)) ? tin_c%s : %s)", // TEVCMP_BGR24_GT
		"((abs(dot(tin_a.rgb, c24) - dot(tin_b.rgb, c24)) < 0.5) ? tin_c%s : %s)", // TEVCMP_BGR24_EQ
		// Only for RGB
		"(max(sign(tin_a.rgb - tin_b.rgb - 0.5), float3(0.0,0.0,0.0)) * tin_c.rgb)", // TEVCMP_RGB8_GT
		"((float3(1.0,1.0,1.0) - max(sign(abs(tin_a.rgb - tin_b.rgb) - 0.5),float3(0.0,0.0,0.0))) * tin_c.rgb)", // TEVCMP_RGB8_EQ
		// Only for ALPHA
		"((tin_a.a >= (tin_b.a + 0.5)) ? tin_c.a : 0.0)", // TEVCMP_A8_GT
		"((abs(tin_a.a - tin_b.a) < 0.5) ? tin_c.a : 0.0)", // TEVCMP_A8_EQ
	};
	out.Write("tin_d%s+", TEVCMPComponents[components]);
	if (cmp < 6)
	{
		// same function for alpha and rgb
		out.Write(TEVCMPOPTable[cmp], TEVCMPComponents[components], TEVCMPZero[components]);
	}
	else
	{
		// if rgb just use the regular cmp
		// for alpha use the last 2 functions
		out.Write(TEVCMPOPTable[cmp + (components << 1)]);
	}
}

template<class T, API_TYPE ApiType>
void SampleTextureRAW(T& out, const char *texcoords, const char *texswap, const char *layer, int texmap)
{
	if (ApiType == API_D3D11)
	{
		out.Write("Tex%d.Sample(samp%d, float3(%s.xy * " I_TEXDIMS"[%d].xy, %s)).%s;\n", 8 + texmap, texmap, texcoords, texmap, layer, texswap);
	}
	else
	{
		out.Write("texture(samp%d,float3(%s.xy * " I_TEXDIMS"[%d].xy, %s)).%s;\n", 8 + texmap, texcoords, texmap, layer, texswap);
	}
}

template<class T, API_TYPE ApiType>
void SampleTexture(T& out, const char *texcoords, const char *texswap, int texmap)
{
	if (ApiType == API_D3D11)
	{
		out.Write("wuround((Tex%d.Sample(samp%d, float3(%s.xy * " I_TEXDIMS"[%d].xy, %s))).%s * 255.0);\n", texmap, texmap, texcoords, texmap, g_ActiveConfig.iStereoMode > 0 ? "layer" : "0.0", texswap);
	}
	else if (ApiType == API_OPENGL)
	{
		out.Write("wuround(texture(samp%d,float3(%s.xy * " I_TEXDIMS"[%d].xy, %s)).%s * 255.0);\n", texmap, texcoords, texmap, g_ActiveConfig.iStereoMode > 0 ? "layer" : "0.0", texswap);
	}
	else
	{
		out.Write("wuround(tex2D(samp%d,%s.xy * " I_TEXDIMS"[%d].xy).%s * 255.0);\n", texmap, texcoords, texmap, texswap);
	}
}

static const char *tevAlphaFuncsTable[] =
{
	"(false)",						// NEVER
	"(prev.a <= (%s - 0.25))",		// LESS
	"(abs( prev.a - %s ) < 0.5)",	// EQUAL
	"(prev.a < (%s + 0.25))",		// LEQUAL
	"(prev.a >= (%s + 0.25))",		// GREATER
	"(abs( prev.a - %s ) >= 0.5)",	// NEQUAL
	"(prev.a > (%s - 0.25))",		// GEQUAL
	"(true)"						// ALWAYS
};

static const char *tevAlphaFuncsTableI[] =
{
	"(false)",					// NEVER
	"(prev.a <  %s)",			// LESS
	"(prev.a == %s)",			// EQUAL
	"(prev.a <= %s)",			// LEQUAL
	"(prev.a >  %s)",			// GREATER
	"(prev.a != %s)",			// NEQUAL
	"(prev.a >= %s)",			// GEQUAL
	"(true)"					// ALWAYS
};

static const char *tevAlphaFunclogicTable[] =
{
	" && ", // and
	" || ", // or
	" != ", // xor
	" == "  // xnor
};

template<class T, bool Write_Code, API_TYPE ApiType, bool use_integer_math>
static inline void WriteAlphaTest(T& out, pixel_shader_uid_data& uid_data, DSTALPHA_MODE dstAlphaMode, bool per_pixel_depth, const BPMemory &bpm)
{
	static const char *alphaRef[2] =
	{
		I_ALPHA".r",
		I_ALPHA".g"
	};

	uid_data.alpha_test_comp0 = bpm.alpha_test.comp0;
	uid_data.alpha_test_comp1 = bpm.alpha_test.comp1;
	uid_data.alpha_test_logic = bpm.alpha_test.logic;
	uid_data.alpha_test_use_zcomploc_hack = bpm.UseEarlyDepthTest()
		&& bpm.zmode.updateenable
		&& !g_ActiveConfig.backend_info.bSupportsEarlyZ
		&& !bpm.genMode.zfreeze;
	if (Write_Code)
	{
		// using discard then return works the same in cg and dx9 but not in dx11
		out.Write("if(!( ");

		// Lookup the first component from the alpha function table
		int compindex = bpm.alpha_test.comp0;
		if (use_integer_math)
			out.Write(tevAlphaFuncsTableI[compindex], alphaRef[0]);
		else
			out.Write(tevAlphaFuncsTable[compindex], alphaRef[0]);

		out.Write("%s", tevAlphaFunclogicTable[bpm.alpha_test.logic]);//lookup the logic op

		// Lookup the second component from the alpha function table
		compindex = bpm.alpha_test.comp1;
		if (use_integer_math)
			out.Write(tevAlphaFuncsTableI[compindex], alphaRef[1]);
		else
			out.Write(tevAlphaFuncsTable[compindex], alphaRef[1]);
		out.Write(")) {\n");

		out.Write("ocol0 = float4(0.0,0.0,0.0,0.0);\n");
		if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
			out.Write("ocol1 = float4(0.0,0.0,0.0,0.0);\n");
		if (per_pixel_depth)
			out.Write("depth = %s;\n", (ApiType == API_OPENGL) ? "1.0" : "0.0");

		// HAXX: zcomploc (aka early_ztest) is a way to control whether depth test is done before
		// or after texturing and alpha test. PC graphics APIs have no way to support this
		// feature properly as of 2012: Depth buffer and depth test are not
		// programmable and the depth test is always done after texturing.
		// Most importantly, they do not allow writing to the z-buffer without
		// writing a color value (unless color writing is disabled altogether).
		// We implement "depth test before texturing" by disabling alpha test when early-z is in use.
		// It seems to be less buggy than not to update the depth buffer if alpha test fails,
		// but both ways wouldn't be accurate.

		// OpenGL 4.2 has a flag which allows the driver to still update the depth buffer 
		// if alpha test fails. The driver doesn't have to, but I assume they all do because
		// it's the much faster code path for the GPU.		
		if (!uid_data.alpha_test_use_zcomploc_hack)
		{
			out.Write("discard;\n");
			if (ApiType != API_D3D11)
				out.Write("return;\n");
		}
		out.Write("}\n");
	}
}

static const char *tevFogFuncsTable[] =
{
	"",																// No Fog
	"",																// ?
	"",																// Linear
	"",																// ?
	"fog = 1.0 - exp2(-8.0 * fog);\n",						// exp
	"fog = 1.0 - exp2(-8.0 * fog * fog);\n",				// exp2
	"fog = exp2(-8.0 * (1.0 - fog));\n",					// backward exp
	"fog = 1.0 - fog;\n   fog = exp2(-8.0 * fog * fog);\n"	// backward exp2
};

template<class T, bool Write_Code, bool use_integer_math>
static inline void WriteFog(T& out, pixel_shader_uid_data& uid_data, const BPMemory &bpm)
{
	uid_data.fog_fsel = bpm.fog.c_proj_fsel.fsel;
	if (bpm.fog.c_proj_fsel.fsel == 0)
		return; // no Fog

	uid_data.fog_proj = bpm.fog.c_proj_fsel.proj;
	uid_data.fog_RangeBaseEnabled = bpm.fogRange.Base.Enabled;

	if (Write_Code)
	{
		if (bpm.fog.c_proj_fsel.proj == 0)
		{
			// perspective
			// ze = A/(B - (Zs >> B_SHF)
			if (use_integer_math)
				out.Write("float ze = (" I_FOGF "[1].x * 16777216.0) / float(" I_FOGI ".y - (zCoord >> " I_FOGI ".w));\n");
			else
				out.Write("float ze = " I_FOGF "[1].x / (" I_FOGI ".y - (zCoord * " I_FOGI ".w));\n");
		}
		else
		{
			// orthographic
			// ze = a*Zs	(here, no B_SHF)
			if (use_integer_math)
				out.Write("float ze = " I_FOGF "[1].x * (float(zCoord) / 16777216.0);\n");
			else
				out.Write("float ze = " I_FOGF "[1].x * zCoord;\n");
		}

		// x_adjust = sqrt((x-center)^2 + k^2)/k
		// ze *= x_adjust
		// this is completely theoretical as the real hardware seems to use a table intead of calculating the values.

		if (bpm.fogRange.Base.Enabled)
		{
			out.Write("float x_adjust = (2.0 * (clipPos.x / " I_FOGF "[0].y)) - 1.0 - " I_FOGF "[0].x;\n");
			out.Write("x_adjust = sqrt(x_adjust * x_adjust + " I_FOGF "[0].z * " I_FOGF "[0].z) / " I_FOGF "[0].z;\n");
			out.Write("ze *= x_adjust;\n");
		}

		out.Write("float fog = clamp(ze - " I_FOGF "[1].z, 0.0, 1.0);\n");

		if (bpm.fog.c_proj_fsel.fsel > 3)
		{
			out.Write("%s", tevFogFuncsTable[bpm.fog.c_proj_fsel.fsel]);
		}
		else
		{
			if (bpm.fog.c_proj_fsel.fsel != 2 && out.GetBuffer() != NULL)
				WARN_LOG(VIDEO, "Unknown Fog Type! %08x", bpm.fog.c_proj_fsel.fsel);
		}
		out.Write("wu ifog = wu(round(fog * 256.0));\n");
		out.Write("prev.rgb = BSHR(prev.rgb * (wu(256) - ifog) + " I_FOGCOLOR".rgb * ifog, wu(8));\n");
	}

}

template<class T, bool use_integer_math, API_TYPE ApiType>
inline void WritePerPixelDepth(T& out, const BPMemory &bpm)
{
	if (bpm.genMode.zfreeze)
	{
		out.Write("\tfloat2 screenpos = rawpos.xy * " I_EFBSCALE".xy;\n");

		// Opengl has reversed vertical screenspace coordiantes
		if (ApiType == API_OPENGL)
		{
			out.Write("\tscreenpos.y = %i - screenpos.y - 1;\n", EFB_HEIGHT);
			out.Write("\tdepth = (" I_ZSLOPE".z + " I_ZSLOPE".x * screenpos.x + " I_ZSLOPE".y * screenpos.y) / 16777216.0;\n");
		}
		else
		{
			out.Write("\tdepth = 1.0 - ((" I_ZSLOPE".z + " I_ZSLOPE".x * screenpos.x + " I_ZSLOPE".y * screenpos.y) / 16777216.0);\n");
		}
	}
	else
	{
		if (use_integer_math)
			out.Write("\tdepth = %s(float(zCoord) / 16777216.0);\n", ApiType == API_OPENGL ? "" : "1.0 - ");
		else
			out.Write("\tdepth = %s zCoord;\n", ApiType == API_OPENGL ? "" : "1.0 - ");
	}
}

void GetPixelShaderUidD3D9(PixelShaderUid& object, DSTALPHA_MODE dstAlphaMode, u32 components, const XFMemory &xfr, const BPMemory &bpm)
{
	GeneratePixelShader<PixelShaderUid, false, API_D3D9>(object, dstAlphaMode, components, xfr, bpm);
}

void GeneratePixelShaderCodeD3D9(ShaderCode& object, DSTALPHA_MODE dstAlphaMode, u32 components, const XFMemory &xfr, const BPMemory &bpm)
{
	GeneratePixelShader<ShaderCode, true, API_D3D9>(object, dstAlphaMode, components, xfr, bpm);
}

void GeneratePixelShaderCodeD3D9SM2(ShaderCode& object, DSTALPHA_MODE dstAlphaMode, u32 components, const XFMemory &xfr, const BPMemory &bpm)
{
	GeneratePixelShader<ShaderCode, true, API_D3D9_SM20>(object, dstAlphaMode, components, xfr, bpm);
}

void GetPixelShaderUidD3D11(PixelShaderUid& object, DSTALPHA_MODE dstAlphaMode, u32 components, const XFMemory &xfr, const BPMemory &bpm)
{
	GeneratePixelShader<PixelShaderUid, false, API_D3D11, true>(object, dstAlphaMode, components, xfr, bpm);
}

void GeneratePixelShaderCodeD3D11(ShaderCode& object, DSTALPHA_MODE dstAlphaMode, u32 components, const XFMemory &xfr, const BPMemory &bpm)
{
	GeneratePixelShader<ShaderCode, true, API_D3D11, true>(object, dstAlphaMode, components, xfr, bpm);
}

void GetPixelShaderUidGL(PixelShaderUid& object, DSTALPHA_MODE dstAlphaMode, u32 components, const XFMemory &xfr, const BPMemory &bpm)
{
	GeneratePixelShader<PixelShaderUid, false, API_OPENGL, true>(object, dstAlphaMode, components, xfr, bpm);
}

void GeneratePixelShaderCodeGL(ShaderCode& object, DSTALPHA_MODE dstAlphaMode, u32 components, const XFMemory &xfr, const BPMemory &bpm)
{
	GeneratePixelShader<ShaderCode, true, API_OPENGL, true>(object, dstAlphaMode, components, xfr, bpm);
}

