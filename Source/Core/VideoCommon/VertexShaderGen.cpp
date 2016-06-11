// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>
#include <locale.h>
#ifdef __APPLE__
#include <xlocale.h>
#endif

#include "VideoCommon/NativeVertexFormat.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/LightingShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoConfig.h"

static char text[VERTEXSHADERGEN_BUFFERSIZE];
static const char *texOffsetMemberSelector[] = { "x", "y", "z", "w" };

void GetVertexShaderUID(VertexShaderUid& out, u32 components, const XFMemory &xfr, const BPMemory &bpm)
{
	out.ClearUID();
	vertex_shader_uid_data& uid_data = out.GetUidData<vertex_shader_uid_data>();
	uid_data.numTexGens = xfr.numTexGen.numTexGens;
	uid_data.components = components;
	bool lightingEnabled = xfr.numChan.numColorChans > 0;
	bool forced_lighting_enabled = g_ActiveConfig.TessellationEnabled() && xfr.projection.type == GX_PERSPECTIVE && g_ActiveConfig.bForcedLighting;
	bool enable_pl = g_ActiveConfig.PixelLightingEnabled(xfr, components) || forced_lighting_enabled;
	bool needLightShader = lightingEnabled && !enable_pl;
	for (unsigned int i = 0; i < uid_data.numTexGens; ++i)
	{
		const TexMtxInfo& texinfo = xfr.texMtxInfo[i];
		needLightShader = needLightShader || texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC0 || texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC1;
	}
	uid_data.pixel_lighting = enable_pl;
	uid_data.numColorChans = xfr.numChan.numColorChans;
	if ((g_ActiveConfig.backend_info.APIType & API_D3D9) == 0)
	{
		uid_data.msaa = g_ActiveConfig.iMultisamples > 1;
		uid_data.ssaa = g_ActiveConfig.iMultisamples > 1 && g_ActiveConfig.bSSAA;
	}
	if (needLightShader)
		GetLightingShaderUid(uid_data.lighting, xfr);
	// transform texcoords
	for (unsigned int i = 0; i < uid_data.numTexGens; ++i)
	{
		auto& texinfo = uid_data.texMtxInfo[i];

		texinfo.sourcerow = xfr.texMtxInfo[i].sourcerow;
		texinfo.inputform = xfr.texMtxInfo[i].inputform;
		texinfo.texgentype = xfr.texMtxInfo[i].texgentype;
		// first transformation
		switch (texinfo.texgentype)
		{
		case XF_TEXGEN_EMBOSS_MAP: // calculate tex coords into bump map
			if (uid_data.components & (VB_HAS_NRM1 | VB_HAS_NRM2))
			{
				// transform the light dir into tangent space
				texinfo.embosslightshift = xfr.texMtxInfo[i].embosslightshift;
				texinfo.embosssourceshift = xfr.texMtxInfo[i].embosssourceshift;
			}
			break;
		case XF_TEXGEN_COLOR_STRGBC0:
			break;
		case XF_TEXGEN_COLOR_STRGBC1:
			break;
		case XF_TEXGEN_REGULAR:
		default:
			uid_data.texMtxInfo_n_projection |= xfr.texMtxInfo[i].projection << i;
			break;
		}

		uid_data.dualTexTrans_enabled = xfr.dualTexTrans.enabled;
		// CHECKME: does this only work for regular tex gen types?
		if (uid_data.dualTexTrans_enabled && texinfo.texgentype == XF_TEXGEN_REGULAR)
		{
			auto& postInfo = uid_data.postMtxInfo[i];
			postInfo.index = xfr.postMtxInfo[i].index;
			postInfo.normalize = xfr.postMtxInfo[i].normalize;
		}
	}
	out.CalculateUIDHash();
}

template<API_TYPE api_type>
inline void GenerateVertexShader(ShaderCode& out, const vertex_shader_uid_data& uid_data, bool use_integer_math)
{
	char * buffer = nullptr;
	buffer = out.GetBuffer();
	if (buffer == nullptr)
	{
		buffer = text;
		out.SetBuffer(text);
	}
	const u32 components = uid_data.components;
	bool lightingEnabled = uid_data.numColorChans > 0;
	bool needLightShader = lightingEnabled && !uid_data.pixel_lighting;
	for (unsigned int i = 0; i < uid_data.numTexGens; ++i)
	{
		const auto& texinfo = uid_data.texMtxInfo[i];
		needLightShader = needLightShader || texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC0 || texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC1;
	}
	buffer[VERTEXSHADERGEN_BUFFERSIZE - 1] = 0x7C;  // canary
																	// uniforms
	if (api_type == API_OPENGL)
		out.Write("layout(std140%s) uniform VSBlock {\n", g_ActiveConfig.backend_info.bSupportsBindingLayout ? ", binding = 2" : "");
	else if (api_type == API_D3D11)
		out.Write("cbuffer VSBlock : register(b0) {\n");

	DeclareUniform<api_type>(out, C_PROJECTION, "float4", I_PROJECTION"[4]");
	DeclareUniform<api_type>(out, C_DEPTHPARAMS, "float4", I_DEPTHPARAMS);
	DeclareUniform<api_type>(out, C_MATERIALS, "float4", I_MATERIALS"[4]");
	DeclareUniform<api_type>(out, C_LIGHTS, "float4", I_LIGHTS"[40]");
	DeclareUniform<api_type>(out, C_PHONG, "float4", I_PHONG"[2]");
	DeclareUniform<api_type>(out, C_TEXMATRICES, "float4", I_TEXMATRICES"[24]");
	DeclareUniform<api_type>(out, C_TRANSFORMMATRICES, "float4", I_TRANSFORMMATRICES"[64]");
	DeclareUniform<api_type>(out, C_NORMALMATRICES, "float4", I_NORMALMATRICES"[32]");
	DeclareUniform<api_type>(out, C_POSTTRANSFORMMATRICES, "float4", I_POSTTRANSFORMMATRICES"[64]");
	DeclareUniform<api_type>(out, C_PLOFFSETPARAMS, "float4", I_PLOFFSETPARAMS"[13]");

	if (api_type == API_OPENGL || api_type == API_D3D11)
		out.Write("};\n");

	out.Write("struct VS_OUTPUT {\n");
	GenerateVSOutputMembers<api_type>(out, uid_data.pixel_lighting, uid_data.numTexGens);
	out.Write("};\n");

	if (api_type == API_OPENGL)
	{
		out.Write("in float4 rawpos; // ATTR%d,\n", SHADER_POSITION_ATTRIB);
		out.Write("in float fposmtx; // ATTR%d,\n", SHADER_POSMTX_ATTRIB);
		if (components & VB_HAS_NRM0)
			out.Write("in float3 rawnorm0; // ATTR%d,\n", SHADER_NORM0_ATTRIB);
		if (components & VB_HAS_NRM1)
			out.Write("in float3 rawnorm1; // ATTR%d,\n", SHADER_NORM1_ATTRIB);
		if (components & VB_HAS_NRM2)
			out.Write("in float3 rawnorm2; // ATTR%d,\n", SHADER_NORM2_ATTRIB);

		if (components & VB_HAS_COL0)
			out.Write("in float4 color0; // ATTR%d,\n", SHADER_COLOR0_ATTRIB);
		if (components & VB_HAS_COL1)
			out.Write("in float4 color1; // ATTR%d,\n", SHADER_COLOR1_ATTRIB);

		for (int i = 0; i < 8; ++i)
		{
			u32 hastexmtx = (components & (VB_HAS_TEXMTXIDX0 << i));
			if ((components & (VB_HAS_UV0 << i)) || hastexmtx)
				out.Write("in float%d tex%d; // ATTR%d,\n", hastexmtx ? 3 : 2, i, SHADER_TEXTURE0_ATTRIB + i);
		}

		if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
		{
			out.Write("out VertexData {\n");
			GenerateVSOutputMembers<api_type>(out, uid_data.pixel_lighting, uid_data.numTexGens, GetInterpolationQualifier(api_type, uid_data.msaa, uid_data.ssaa, false, true));
			out.Write("} vs;\n");
		}
		else
		{
			const char* optCentroid = GetInterpolationQualifier(api_type, uid_data.msaa, uid_data.ssaa);

			// Let's set up attributes
			if (uid_data.numTexGens < 7)
			{
				for (int i = 0; i < 8; ++i)
					out.Write("%s out float3 uv%d_2;\n", optCentroid, i);
				out.Write("%s out float4 clipPos_2;\n", optCentroid);
				if (uid_data.pixel_lighting)
					out.Write("%s out float4 Normal_2;\n", optCentroid);
			}
			else
			{
				// wpos is in w of first 4 texcoords
				if (uid_data.pixel_lighting)
				{
					for (int i = 0; i < 8; ++i)
						out.Write("%s out float4 uv%d_2;\n", optCentroid, i);
				}
				else
				{
					for (unsigned int i = 0; i < uid_data.numTexGens; ++i)
						out.Write("%s out float%d uv%d_2;\n", optCentroid, i < 4 ? 4 : 3, i);
				}
			}
			out.Write("%s out float4 colors_0;\n", optCentroid);
			out.Write("%s out float4 colors_1;\n", optCentroid);
		}

		out.Write("void main()\n{\n");
	}
	else
	{
		out.Write("VS_OUTPUT main(\n");

		// inputs
		if (components & VB_HAS_NRM0)
			out.Write("  float3 rawnorm0 : NORMAL0,\n");
		if (components & VB_HAS_NRM1)
			out.Write("  float3 rawnorm1 : NORMAL1,\n");
		if (components & VB_HAS_NRM2)
			out.Write("  float3 rawnorm2 : NORMAL2,\n");
		if (components & VB_HAS_COL0)
			out.Write("  float4 color0 : COLOR0,\n");
		if (components & VB_HAS_COL1)
			out.Write("  float4 color1 : COLOR1,\n");
		for (int i = 0; i < 8; ++i)
		{
			u32 hastexmtx = (components & (VB_HAS_TEXMTXIDX0 << i));
			if ((components & (VB_HAS_UV0 << i)) || hastexmtx)
				out.Write("  float%d tex%d : TEXCOORD%d,\n", hastexmtx ? 3 : 2, i, i);
		}
		out.Write("  float4 blend_indices : BLENDINDICES,\n");

		out.Write("  float4 rawpos : POSITION) {\n");
	}
	out.Write("VS_OUTPUT o;\n");
	// transforms
	if (api_type & API_D3D9)
	{
		out.Write("int4 indices = D3DCOLORtoUBYTE4(blend_indices);\n");
		out.Write("int posmtx = indices.x;\n");
	}
	else if (api_type == API_D3D11)
	{
		out.Write("int posmtx = blend_indices.x * 255.0;\n");
	}
	else
	{
		out.Write("int posmtx = int(fposmtx);\n");
	}

	out.Write("float4 pos = float4(dot(" I_TRANSFORMMATRICES"[posmtx], rawpos), dot(" I_TRANSFORMMATRICES"[posmtx+1], rawpos), dot(" I_TRANSFORMMATRICES"[posmtx+2], rawpos), 1);\n");

	if (components & VB_HAS_NRMALL)
	{
		out.Write("int normidx = posmtx >= 32 ? (posmtx-32) : posmtx;\n");
		out.Write("float3 N0 = " I_NORMALMATRICES"[normidx].xyz, N1 = " I_NORMALMATRICES"[normidx+1].xyz, N2 = " I_NORMALMATRICES"[normidx+2].xyz;\n");
		
		if (components & VB_HAS_NRM0)
			out.Write("float3 _norm0 = normalize(float3(dot(N0, rawnorm0), dot(N1, rawnorm0), dot(N2, rawnorm0)));\n");
		if (components & VB_HAS_NRM1)
			out.Write("float3 _norm1 = float3(dot(N0, rawnorm1), dot(N1, rawnorm1), dot(N2, rawnorm1));\n");
		if (components & VB_HAS_NRM2)
			out.Write("float3 _norm2 = float3(dot(N0, rawnorm2), dot(N1, rawnorm2), dot(N2, rawnorm2));\n");
	}

	if (!(components & VB_HAS_NRM0))
		out.Write("float3 _norm0 = float3(0.0, 0.0, 0.0);\n");

	out.Write("o.pos = float4(dot(" I_PROJECTION"[0], pos), dot(" I_PROJECTION"[1], pos), dot(" I_PROJECTION"[2], pos), dot(" I_PROJECTION"[3], pos));\n");
	if (api_type & API_D3D9)
	{
		//Write Pos offset for Point/Line Rendering
		out.Write("o.pos.xy = o.pos.xy + " I_PLOFFSETPARAMS"[indices.z].xy * o.pos.w;\n");
	}
	if (needLightShader)
	{
		out.Write("float4 mat, lacc;\n"
			"float3 ldir, h;\n"
			"float dist, dist2, attn;\n");
		if (use_integer_math)
		{
			out.Write("int4 ilacc;\n");
		}
	}
	if (!lightingEnabled)
	{
		if (components & VB_HAS_COL0)
			out.Write("o.colors_0 = color0;\n");
		else
			out.Write("o.colors_0 = float4(1.0, 1.0, 1.0, 1.0);\n");

		if (components & VB_HAS_COL1)
			out.Write("o.colors_1 = color1;\n");
		else
			out.Write("o.colors_1 = o.colors_0;\n");
	}
	if (needLightShader)
		GenerateLightingShaderCode(out, uid_data.numColorChans, uid_data.lighting, components, I_MATERIALS, I_LIGHTS, "color", "o.colors_", use_integer_math);

	if (uid_data.numColorChans < 2 && needLightShader)
	{
		if (components & VB_HAS_COL1)
			out.Write("o.colors_1 = color1;\n");
		else
			out.Write("o.colors_1 = o.colors_0;\n");
	}
	// transform texcoords
	out.Write("float4 coord = float4(0.0, 0.0, 1.0, 1.0);\n");


	for (unsigned int i = 0; i < uid_data.numTexGens; ++i)
	{
		const auto& texinfo = uid_data.texMtxInfo[i];
		out.Write("{\n");
		out.Write("coord = float4(0.0, 0.0, 1.0, 1.0);\n");
		switch (texinfo.sourcerow)
		{
		case XF_SRCGEOM_INROW:
			out.Write("coord.xyz = rawpos.xyz;\n"); // pos.w is 1
			break;
		case XF_SRCNORMAL_INROW:
			if (components & VB_HAS_NRM0)
			{
				out.Write("coord.xyz = rawnorm0.xyz;\n");
			}
			break;
		case XF_SRCCOLORS_INROW:
			_dbg_assert_log_(VIDEO, texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC0 || texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC1, "texgentype missmatch: %u", texinfo.texgentype);
			break;
		case XF_SRCBINORMAL_T_INROW:
			if (components & VB_HAS_NRM1)
			{
				out.Write("coord.xyz = rawnorm1.xyz;\n");
			}
			break;
		case XF_SRCBINORMAL_B_INROW:
			if (components & VB_HAS_NRM2)
			{
				out.Write("coord.xyz = rawnorm2.xyz;\n");
			}
			break;
		default:
			_dbg_assert_log_(VIDEO, texinfo.sourcerow <= XF_SRCTEX7_INROW, "sourcerow missmatch: %u", texinfo.sourcerow);
			if (components & (VB_HAS_UV0 << (texinfo.sourcerow - XF_SRCTEX0_INROW)))
				out.Write("coord.xy = tex%d.xy;\n", texinfo.sourcerow - XF_SRCTEX0_INROW);
			break;
		}
		// An input form other than ABC1 or AB11 doesn't exist
		// But the hardware has it as a two bit field
		if (texinfo.inputform == XF_TEXINPUT_AB11)
			out.Write("coord.z = 1.0;\n");

		// first transformation
		switch (texinfo.texgentype)
		{
		case XF_TEXGEN_EMBOSS_MAP: // calculate tex coords into bump map

			if (components & (VB_HAS_NRM1 | VB_HAS_NRM2))
			{
				// transform the light dir into tangent space
				out.Write("float3 eldir = normalize(" LIGHT_POS".xyz - pos.xyz);\n", LIGHT_POS_PARAMS(I_LIGHTS, texinfo.embosslightshift));
				out.Write("o.tex%d.xyz = o.tex%d.xyz + float3(dot(eldir, _norm1), dot(eldir, _norm2), 0.0);\n", i, texinfo.embosssourceshift);
			}
			else
			{
				// Even if inputform ABC1 is set, it only uses AB11
				out.Write("o.tex%d.xyz = float3(coord.xy, 1.0);\n", i);
			}

			break;
		case XF_TEXGEN_COLOR_STRGBC0:
		{
			out.Write("o.tex%d.xyz = float3(o.colors_0.x, o.colors_0.y, 1);\n", i);
		}
		break;
		case XF_TEXGEN_COLOR_STRGBC1:
		{
			out.Write("o.tex%d.xyz = float3(o.colors_1.x, o.colors_1.y, 1);\n", i);
		}
		break;
		case XF_TEXGEN_REGULAR:
		default:
		{
			if (components & (VB_HAS_TEXMTXIDX0 << i))
			{
				out.Write("int tmp = int(tex%d.z);\n", i);
				if (((uid_data.texMtxInfo_n_projection >> i) & 1) == XF_TEXPROJ_STQ)
					out.Write("o.tex%d.xyz = float3(dot(coord, " I_TRANSFORMMATRICES"[tmp]), dot(coord, " I_TRANSFORMMATRICES"[tmp+1]), dot(coord, " I_TRANSFORMMATRICES"[tmp+2]));\n", i);
				else
					out.Write("o.tex%d.xyz = float3(dot(coord, " I_TRANSFORMMATRICES"[tmp]), dot(coord, " I_TRANSFORMMATRICES"[tmp+1]), 1);\n", i);
			}
			else
			{
				if (((uid_data.texMtxInfo_n_projection >> i) & 1) == XF_TEXPROJ_STQ)
					out.Write("o.tex%d.xyz = float3(dot(coord, " I_TEXMATRICES"[%d]), dot(coord, " I_TEXMATRICES"[%d]), dot(coord, " I_TEXMATRICES"[%d]));\n", i, 3 * i, 3 * i + 1, 3 * i + 2);
				else
					out.Write("o.tex%d.xyz = float3(dot(coord, " I_TEXMATRICES"[%d]), dot(coord, " I_TEXMATRICES"[%d]), 1);\n", i, 3 * i, 3 * i + 1);
			}
		}
		break;
		}

		if (texinfo.texgentype == XF_TEXGEN_REGULAR)
		{
			// CHECKME: does this only work for regular tex gen types?
			if (uid_data.dualTexTrans_enabled)
			{
				const auto& postInfo = uid_data.postMtxInfo[i];

				int postidx = postInfo.index;
				out.Write("float4 P0 = " I_POSTTRANSFORMMATRICES"[%d];\n"
					"float4 P1 = " I_POSTTRANSFORMMATRICES"[%d];\n"
					"float4 P2 = " I_POSTTRANSFORMMATRICES"[%d];\n",
					postidx & 0x3f, (postidx + 1) & 0x3f, (postidx + 2) & 0x3f);

				if (postInfo.normalize)
					out.Write("o.tex%d.xyz = normalize(o.tex%d.xyz);\n", i, i);

				// multiply by postmatrix
				out.Write("o.tex%d.xyz = float3(dot(P0.xyz, o.tex%d.xyz) + P0.w, dot(P1.xyz, o.tex%d.xyz) + P1.w, dot(P2.xyz, o.tex%d.xyz) + P2.w);\n", i, i, i, i);
			}

			out.Write("if(o.tex%d.z == 0.0f)\n", i);
			out.Write("\to.tex%d.xy = clamp(o.tex%d.xy / 2.0f, float2(-1.0f, -1.0f), float2(1.0f, 1.0f));\n", i, i);
		}
		out.Write("}\n");
	}
	// clipPos/w needs to be done in pixel shader, not here
	if (uid_data.numTexGens < 7)
	{
		out.Write("o.clipPos%s = float4(pos.x,pos.y,o.pos.z,o.pos.w);\n", (api_type == API_OPENGL) ? "_2" : "");
	}
	else
	{
		out.Write("o.tex0.w = pos.x;\n");
		out.Write("o.tex1.w = pos.y;\n");
		out.Write("o.tex2.w = o.pos.z;\n");
		out.Write("o.tex3.w = o.pos.w;\n");
	}

	if (uid_data.pixel_lighting)
	{
		if (uid_data.numTexGens < 7)
		{
			out.Write("o.Normal%s = float4(_norm0.x,_norm0.y,_norm0.z,pos.z);\n", (api_type == API_OPENGL) ? "_2" : "");
		}
		else
		{
			out.Write("o.tex4.w = _norm0.x;\n");
			out.Write("o.tex5.w = _norm0.y;\n");
			out.Write("o.tex6.w = _norm0.z;\n");
			if (uid_data.numTexGens < 8)
				out.Write("o.tex7 = pos.xyzz;\n");
			else
				out.Write("o.tex7.w = pos.z;\n");
		}

		if (components & VB_HAS_COL0)
			out.Write("o.colors_0 = color0;\n");
		else
			out.Write("o.colors_0 = float4(1.0, 1.0, 1.0, 1.0);\n");

		if (components & VB_HAS_COL1)
			out.Write("o.colors_1 = color1;\n");
		else
			out.Write("o.colors_1 = o.colors_0;\n");
	}

	//write the true depth value, if the game uses depth textures pixel shaders will override with the correct values
	//if not early z culling will improve speed
	if (g_ActiveConfig.backend_info.bSupportsClipControl)
	{
		out.Write("o.pos.z = -o.pos.z;\n");
	}
	else if (api_type & API_D3D9 || api_type == API_D3D11)
	{
		out.Write("o.pos.z = -((" I_DEPTHPARAMS".x - 1.0) * o.pos.w + o.pos.z * " I_DEPTHPARAMS".y);\n");
	}
	else
	{
		// this results in a scale from -1..0 to -1..1 after perspective
		// divide
		out.Write("o.pos.z = o.pos.z * -2.0 - o.pos.w;\n");

		// the next steps of the OGL pipeline are:
		// (x_c,y_c,z_c,w_c) = o.pos  //switch to OGL spec terminology
		// clipping to -w_c <= (x_c,y_c,z_c) <= w_c
		// (x_d,y_d,z_d) = (x_c,y_c,z_c)/w_c//perspective divide
		// z_w = (f-n)/2*z_d + (n+f)/2
		// z_w now contains the value to go to the 0..1 depth buffer

		//trying to get the correct semantic while not using glDepthRange
		//seems to get rather complicated
	}

	// The console GPU places the pixel center at 7/12 in screen space unless
	// antialiasing is enabled, while D3D11 and OpenGL place it at 0.5, and D3D9 at 0. This results
	// in some primitives being placed one pixel too far to the bottom-right,
	// which in turn can be critical if it happens for clear quads.
	// Hence, we compensate for this pixel center difference so that primitives
	// get rasterized correctly.
	out.Write("o.pos.xy = o.pos.xy + o.pos.w * " I_DEPTHPARAMS".zw;\n");

	if (api_type & API_D3D9)
	{
		// Write Texture Offsets for Point/Line Rendering
		for (unsigned int i = 0; i < uid_data.numTexGens; ++i)
		{
			out.Write("o.tex%d.xy = o.tex%d.xy + (" I_PLOFFSETPARAMS"[indices.w].zw * " I_PLOFFSETPARAMS"[indices.y + %d].%s );\n", i, i, ((i / 4) + 1), texOffsetMemberSelector[i % 4]);
		}
	}

	if (api_type == API_OPENGL)
	{
		if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
		{
			AssignVSOutputMembers<api_type>(out, "vs", "o", uid_data.pixel_lighting, uid_data.numTexGens);
		}
		else
		{
			if (uid_data.numTexGens < 7)
			{
				for (unsigned int i = 0; i < 8; ++i)
				{
					if (i < uid_data.numTexGens)
						out.Write(" uv%d_2.xyz =  o.tex%d;\n", i, i);
					else
						out.Write(" uv%d_2.xyz =  float3(0.0, 0.0, 0.0);\n", i);
				}
				out.Write("  clipPos_2 = o.clipPos;\n");
				if (uid_data.pixel_lighting)
					out.Write("  Normal_2 = o.Normal;\n");
			}
			else
			{
				// clip position is in w of first 4 texcoords
				if (uid_data.pixel_lighting)
				{
					for (int i = 0; i < 8; ++i)
						out.Write(" uv%d_2 = o.tex%d;\n", i, i);
				}
				else
				{
					for (unsigned int i = 0; i < uid_data.numTexGens; ++i)
						out.Write("  uv%d_2%s = o.tex%d;\n", i, i < 4 ? ".xyzw" : ".xyz", i);
				}
			}
			out.Write("colors_0 = o.colors_0;\n");
			out.Write("colors_1 = o.colors_1;\n");
		}
		out.Write("gl_Position = o.pos;\n");
		out.Write("}\n");
	}
	else
	{
		out.Write("return o;\n}\n");
	}

	if (buffer[VERTEXSHADERGEN_BUFFERSIZE - 1] != 0x7C)
		PanicAlert("VertexShader generator - buffer too small, canary has been eaten!");
}

void GenerateVertexShaderCodeD3D9(ShaderCode& object, const vertex_shader_uid_data& uid_data)
{
	GenerateVertexShader<API_D3D9>(object, uid_data, false);
}

void GenerateVertexShaderCodeD3D11(ShaderCode& object, const vertex_shader_uid_data& uid_data)
{
	GenerateVertexShader<API_D3D11>(object, uid_data, true);
}

void GenerateVertexShaderCodeGL(ShaderCode& object, const vertex_shader_uid_data& uid_data)
{
	GenerateVertexShader<API_OPENGL>(object, uid_data, true);
}
