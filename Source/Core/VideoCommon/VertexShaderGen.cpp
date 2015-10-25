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

template<class T, bool Write_Code, API_TYPE api_type>
inline void GenerateVertexShader(T& out, u32 components, const XFMemory &xfr, const BPMemory &bpm, bool use_integer_math)
{
	// Non-uid template parameters will write to the dummy data (=> gets optimized out)
	bool uidPresent = (&out.template GetUidData<vertex_shader_uid_data>() != NULL);
	vertex_shader_uid_data dummy_data;
	vertex_shader_uid_data& uid_data = uidPresent ? out.template GetUidData<vertex_shader_uid_data>() : dummy_data;
	if (uidPresent)
	{
		out.ClearUID();
	}
	_assert_(bpm.genMode.numtexgens == xfr.numTexGen.numTexGens);
	_assert_(bpm.genMode.numcolchans == xfr.numChan.numColorChans);
	uid_data.numTexGens = xfr.numTexGen.numTexGens;
	uid_data.components = components;
	bool lightingEnabled = xfr.numChan.numColorChans > 0;
	bool enable_pl = g_ActiveConfig.PixelLightingEnabled(xfr, components);
	bool needLightShader = lightingEnabled && !enable_pl;
	for (unsigned int i = 0; i < xfr.numTexGen.numTexGens; ++i)
	{
		const TexMtxInfo& texinfo = xfr.texMtxInfo[i];
		needLightShader = needLightShader || texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC0 || texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC1;
	}
	uid_data.pixel_lighting = enable_pl;
	uid_data.numColorChans = xfr.numChan.numColorChans;
	if (!(api_type & API_D3D9))
	{
		uid_data.msaa = g_ActiveConfig.iMultisampleMode > 0;
		uid_data.ssaa = g_ActiveConfig.iMultisampleMode > 0 && g_ActiveConfig.bSSAA;
	}
	char * buffer = nullptr;
	if (Write_Code)
	{
		buffer = out.GetBuffer();
		if (buffer == nullptr)
		{
			buffer = text;
			out.SetBuffer(text);
		}

		buffer[VERTEXSHADERGEN_BUFFERSIZE - 1] = 0x7C;  // canary
		// uniforms
		if (api_type == API_OPENGL)
			out.Write("layout(std140%s) uniform VSBlock {\n", g_ActiveConfig.backend_info.bSupportsBindingLayout ? ", binding = 2" : "");

		DeclareUniform<T, api_type>(out, C_PROJECTION, "float4", I_PROJECTION"[4]");
		DeclareUniform<T, api_type>(out, C_MATERIALS, "float4", I_MATERIALS"[4]");
		DeclareUniform<T, api_type>(out, C_LIGHTS, "float4", I_LIGHTS"[40]");
		DeclareUniform<T, api_type>(out, C_TEXMATRICES, "float4", I_TEXMATRICES"[24]");
		DeclareUniform<T, api_type>(out, C_TRANSFORMMATRICES, "float4", I_TRANSFORMMATRICES"[64]");
		DeclareUniform<T, api_type>(out, C_NORMALMATRICES, "float4", I_NORMALMATRICES"[32]");
		DeclareUniform<T, api_type>(out, C_POSTTRANSFORMMATRICES, "float4", I_POSTTRANSFORMMATRICES"[64]");
		DeclareUniform<T, api_type>(out, C_DEPTHPARAMS, "float4", I_DEPTHPARAMS);
		DeclareUniform<T, api_type>(out, C_PLOFFSETPARAMS, "float4", I_PLOFFSETPARAMS"[13]");

		if (api_type == API_OPENGL)
			out.Write("};\n");

		out.Write("struct VS_OUTPUT {\n");
		GenerateVSOutputMembers<T, api_type>(out, enable_pl, xfr);
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
				GenerateVSOutputMembers<T, api_type>(out, enable_pl, xfr, GetInterpolationQualifier(api_type, false, true));
				out.Write("} vs;\n");
			}
			else
			{
				const char* optCentroid = GetInterpolationQualifier(api_type);

				// Let's set up attributes
				if (xfr.numTexGen.numTexGens < 7)
				{
					for (int i = 0; i < 8; ++i)
						out.Write("%s out float3 uv%d_2;\n", optCentroid, i);
					out.Write("%s out float4 clipPos_2;\n", optCentroid);
					if (enable_pl)
						out.Write("%s out float4 Normal_2;\n", optCentroid);
				}
				else
				{
					// wpos is in w of first 4 texcoords
					if (enable_pl)
					{
						for (int i = 0; i < 8; ++i)
							out.Write("%s out float4 uv%d_2;\n", optCentroid, i);
					}
					else
					{
						for (unsigned int i = 0; i < xfr.numTexGen.numTexGens; ++i)
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
		if (api_type & API_D3D9)
		{
			out.Write("int4 indices = D3DCOLORtoUBYTE4(blend_indices);\n");
		}
		// transforms
		if (api_type & API_D3D9)
		{
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

		if (components & VB_HAS_NRMALL) {
			out.Write("int normidx = posmtx >= 32 ? (posmtx-32) : posmtx;\n");
			out.Write("float3 N0 = " I_NORMALMATRICES"[normidx].xyz, N1 = " I_NORMALMATRICES"[normidx+1].xyz, N2 = " I_NORMALMATRICES"[normidx+2].xyz;\n");
		}

		if (components & VB_HAS_NRM0)
			out.Write("float3 _norm0 = normalize(float3(dot(N0, rawnorm0), dot(N1, rawnorm0), dot(N2, rawnorm0)));\n");
		if (components & VB_HAS_NRM1)
			out.Write("float3 _norm1 = float3(dot(N0, rawnorm1), dot(N1, rawnorm1), dot(N2, rawnorm1));\n");
		if (components & VB_HAS_NRM2)
			out.Write("float3 _norm2 = float3(dot(N0, rawnorm2), dot(N1, rawnorm2), dot(N2, rawnorm2));\n");

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
	}
	if (needLightShader)
		GenerateLightingShader<T, Write_Code>(out, uid_data.lighting, components, I_MATERIALS, I_LIGHTS, "color", "o.colors_", xfr, use_integer_math);

	// special case if only pos and tex coord 0 and tex coord input is AB11
	// donko - this has caused problems in some games. removed for now.
	bool texGenSpecialCase = false;
	/*bool texGenSpecialCase =
	((g_main_cp_state.vtx_desc.Hex & 0x60600L) == g_main_cp_state.vtx_desc.Hex) && // only pos and tex coord 0
	(g_main_cp_state.vtx_desc.Tex0Coord != NOT_PRESENT) &&
	(xfr.texcoords[0].texmtxinfo.inputform == XF_TEXINPUT_AB11);
	*/
	if (Write_Code)
	{
		if (xfr.numChan.numColorChans < 2 && needLightShader)
		{
			if (components & VB_HAS_COL1)
				out.Write("o.colors_1 = color1;\n");
			else
				out.Write("o.colors_1 = o.colors_0;\n");
		}
		// transform texcoords
		out.Write("float4 coord = float4(0.0, 0.0, 1.0, 1.0);\n");
	}


	for (unsigned int i = 0; i < xfr.numTexGen.numTexGens; ++i)
	{
		const TexMtxInfo& texinfo = xfr.texMtxInfo[i];
		uid_data.texMtxInfo[i].sourcerow = xfr.texMtxInfo[i].sourcerow;
		if (Write_Code)
		{
			out.Write("{\n");
			out.Write("coord = float4(0.0, 0.0, 1.0, 1.0);\n");
			switch (texinfo.sourcerow)
			{
			case XF_SRCGEOM_INROW:
				// The following assert was triggered in Super Smash Bros. Project M 3.6.
				//_assert_(texinfo.inputform == XF_TEXINPUT_ABC1);
				out.Write("coord = rawpos;\n"); // pos.w is 1
				break;
			case XF_SRCNORMAL_INROW:
				if (components & VB_HAS_NRM0)
				{
					_assert_(texinfo.inputform == XF_TEXINPUT_ABC1);
					out.Write("coord = float4(rawnorm0.xyz, 1.0);\n");
				}
				break;
			case XF_SRCCOLORS_INROW:
				_assert_(texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC0 || texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC1);
				break;
			case XF_SRCBINORMAL_T_INROW:
				if (components & VB_HAS_NRM1)
				{
					_assert_(texinfo.inputform == XF_TEXINPUT_ABC1);
					out.Write("coord = float4(rawnorm1.xyz, 1.0);\n");
				}
				break;
			case XF_SRCBINORMAL_B_INROW:
				if (components & VB_HAS_NRM2)
				{
					_assert_(texinfo.inputform == XF_TEXINPUT_ABC1);
					out.Write("coord = float4(rawnorm2.xyz, 1.0);\n");
				}
				break;
			default:
				_assert_(texinfo.sourcerow <= XF_SRCTEX7_INROW);
				if (components & (VB_HAS_UV0 << (texinfo.sourcerow - XF_SRCTEX0_INROW)))
					out.Write("coord = float4(tex%d.x, tex%d.y, 1.0, 1.0);\n", texinfo.sourcerow - XF_SRCTEX0_INROW, texinfo.sourcerow - XF_SRCTEX0_INROW);
				break;
			}
		}

		// first transformation
		uid_data.texMtxInfo[i].texgentype = xfr.texMtxInfo[i].texgentype;
		switch (texinfo.texgentype)
		{
		case XF_TEXGEN_EMBOSS_MAP: // calculate tex coords into bump map

			if (components & (VB_HAS_NRM1 | VB_HAS_NRM2))
			{
				// transform the light dir into tangent space
				uid_data.texMtxInfo[i].embosslightshift = xfr.texMtxInfo[i].embosslightshift;
				uid_data.texMtxInfo[i].embosssourceshift = xfr.texMtxInfo[i].embosssourceshift;
				if (Write_Code)
				{
					out.Write("float3 eldir = normalize(" LIGHT_POS".xyz - pos.xyz);\n", LIGHT_POS_PARAMS(I_LIGHTS, texinfo.embosslightshift));
					out.Write("o.tex%d.xyz = o.tex%d.xyz + float3(dot(eldir, _norm1), dot(eldir, _norm2), 0.0);\n", i, texinfo.embosssourceshift);
				}
			}
			else
			{
				// The following assert was triggered in House of the Dead Overkill and Star Wars Rogue Squadron 2
				//_assert_(0); // should have normals
				uid_data.texMtxInfo[i].embosssourceshift = xfr.texMtxInfo[i].embosssourceshift;
				if (Write_Code)
				{
					out.Write("o.tex%d.xyz = o.tex%d.xyz;\n", i, texinfo.embosssourceshift);
				}
			}

			break;
		case XF_TEXGEN_COLOR_STRGBC0:
			_assert_(texinfo.sourcerow == XF_SRCCOLORS_INROW);
			if (Write_Code)
				out.Write("o.tex%d.xyz = float3(o.colors_0.x, o.colors_0.y, 1);\n", i);
			break;
		case XF_TEXGEN_COLOR_STRGBC1:
			_assert_(texinfo.sourcerow == XF_SRCCOLORS_INROW);
			if (Write_Code)
				out.Write("o.tex%d.xyz = float3(o.colors_1.x, o.colors_1.y, 1);\n", i);
			break;
		case XF_TEXGEN_REGULAR:
		default:
			uid_data.texMtxInfo_n_projection |= xfr.texMtxInfo[i].projection << i;
			if (Write_Code)
			{
				if (components & (VB_HAS_TEXMTXIDX0 << i))
				{
					out.Write("int tmp = int(tex%d.z);\n", i);
					if (texinfo.projection == XF_TEXPROJ_STQ)
						out.Write("o.tex%d.xyz = float3(dot(coord, " I_TRANSFORMMATRICES"[tmp]), dot(coord, " I_TRANSFORMMATRICES"[tmp+1]), dot(coord, " I_TRANSFORMMATRICES"[tmp+2]));\n", i);
					else
						out.Write("o.tex%d.xyz = float3(dot(coord, " I_TRANSFORMMATRICES"[tmp]), dot(coord, " I_TRANSFORMMATRICES"[tmp+1]), 1);\n", i);
				}
				else
				{
					if (texinfo.projection == XF_TEXPROJ_STQ)
						out.Write("o.tex%d.xyz = float3(dot(coord, " I_TEXMATRICES"[%d]), dot(coord, " I_TEXMATRICES"[%d]), dot(coord, " I_TEXMATRICES"[%d]));\n", i, 3 * i, 3 * i + 1, 3 * i + 2);
					else
						out.Write("o.tex%d.xyz = float3(dot(coord, " I_TEXMATRICES"[%d]), dot(coord, " I_TEXMATRICES"[%d]), 1);\n", i, 3 * i, 3 * i + 1);
				}
			}
			break;
		}

		uid_data.dualTexTrans_enabled = xfr.dualTexTrans.enabled;
		// CHECKME: does this only work for regular tex gen types?
		if (xfr.dualTexTrans.enabled && texinfo.texgentype == XF_TEXGEN_REGULAR)
		{
			const PostMtxInfo& postInfo = xfr.postMtxInfo[i];

			uid_data.postMtxInfo[i].index = xfr.postMtxInfo[i].index;
			int postidx = postInfo.index;
			if (Write_Code)
			{
				out.Write("float4 P0 = " I_POSTTRANSFORMMATRICES"[%d];\n"
					"float4 P1 = " I_POSTTRANSFORMMATRICES"[%d];\n"
					"float4 P2 = " I_POSTTRANSFORMMATRICES"[%d];\n",
					postidx & 0x3f, (postidx + 1) & 0x3f, (postidx + 2) & 0x3f);
			}
			if (texGenSpecialCase)
			{
				// no normalization
				// q of input is 1
				// q of output is unknown

				// multiply by postmatrix
				if (Write_Code)
					out.Write("o.tex%d.xyz = float3(dot(P0.xy, o.tex%d.xy) + P0.z + P0.w, dot(P1.xy, o.tex%d.xy) + P1.z + P1.w, 0.0);\n", i, i, i);
			}
			else
			{
				uid_data.postMtxInfo[i].normalize = xfr.postMtxInfo[i].normalize;
				if (Write_Code)
				{
					if (postInfo.normalize)
						out.Write("o.tex%d.xyz = normalize(o.tex%d.xyz);\n", i, i);

					// multiply by postmatrix
					out.Write("o.tex%d.xyz = float3(dot(P0.xyz, o.tex%d.xyz) + P0.w, dot(P1.xyz, o.tex%d.xyz) + P1.w, dot(P2.xyz, o.tex%d.xyz) + P2.w);\n", i, i, i, i);
				}
			}
		}
		if (Write_Code)
			out.Write("}\n");
	}
	if (Write_Code)
	{
		// clipPos/w needs to be done in pixel shader, not here
		if (xfr.numTexGen.numTexGens < 7)
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

		if (enable_pl)
		{
			if (xfr.numTexGen.numTexGens < 7)
			{
				out.Write("o.Normal%s = float4(_norm0.x,_norm0.y,_norm0.z,pos.z);\n", (api_type == API_OPENGL) ? "_2" : "");
			}
			else
			{
				out.Write("o.tex4.w = _norm0.x;\n");
				out.Write("o.tex5.w = _norm0.y;\n");
				out.Write("o.tex6.w = _norm0.z;\n");
				if (xfr.numTexGen.numTexGens < 8)
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
			for (unsigned int i = 0; i < xfr.numTexGen.numTexGens; ++i)
			{
				out.Write("o.tex%d.xy = o.tex%d.xy + (" I_PLOFFSETPARAMS"[indices.w].zw * " I_PLOFFSETPARAMS"[indices.y + %d].%s );\n", i, i, ((i / 4) + 1), texOffsetMemberSelector[i % 4]);
			}
		}

		if (api_type == API_OPENGL)
		{
			if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
			{
				AssignVSOutputMembers<T, api_type>(out, "vs", "o", enable_pl, xfr);
			}
			else
			{

				if (xfr.numTexGen.numTexGens < 7)
				{
					for (unsigned int i = 0; i < 8; ++i)
					{
						if (i < xfr.numTexGen.numTexGens)
							out.Write(" uv%d_2.xyz =  o.tex%d;\n", i, i);
						else
							out.Write(" uv%d_2.xyz =  float3(0.0, 0.0, 0.0);\n", i);
					}
					out.Write("  clipPos_2 = o.clipPos;\n");
					if (enable_pl)
						out.Write("  Normal_2 = o.Normal;\n");
				}
				else
				{
					// clip position is in w of first 4 texcoords
					if (enable_pl)
					{
						for (int i = 0; i < 8; ++i)
							out.Write(" uv%d_2 = o.tex%d;\n", i, i);
					}
					else
					{
						for (unsigned int i = 0; i < xfr.numTexGen.numTexGens; ++i)
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
	if (uidPresent)
	{
		out.CalculateUIDHash();
	}
}

void GetVertexShaderUidD3D9(VertexShaderUid& object, u32 components, const XFMemory &xfr, const BPMemory &bpm)
{
	GenerateVertexShader<VertexShaderUid, false, API_D3D9>(object, components, xfr, bpm, false);
}

void GenerateVertexShaderCodeD3D9(ShaderCode& object, u32 components, const XFMemory &xfr, const BPMemory &bpm)
{
	GenerateVertexShader<ShaderCode, true, API_D3D9>(object, components, xfr, bpm, false);
}

void GetVertexShaderUidD3D11(VertexShaderUid& object, u32 components, const XFMemory &xfr, const BPMemory &bpm)
{
	GenerateVertexShader<VertexShaderUid, false, API_D3D11>(object, components, xfr, bpm, true);
}

void GenerateVertexShaderCodeD3D11(ShaderCode& object, u32 components, const XFMemory &xfr, const BPMemory &bpm)
{
	GenerateVertexShader<ShaderCode, true, API_D3D11>(object, components, xfr, bpm, true);
}

void GetVertexShaderUidGL(VertexShaderUid& object, u32 components, const XFMemory &xfr, const BPMemory &bpm)
{
	GenerateVertexShader<VertexShaderUid, false, API_OPENGL>(object, components, xfr, bpm, true);
}

void GenerateVertexShaderCodeGL(ShaderCode& object, u32 components, const XFMemory &xfr, const BPMemory &bpm)
{
	GenerateVertexShader<ShaderCode, true, API_OPENGL>(object, components, xfr, bpm, true);
}
