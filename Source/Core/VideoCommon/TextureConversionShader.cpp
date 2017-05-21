// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.


#include <stdio.h>
#include <math.h>
#include <locale.h>
#ifdef __APPLE__
#include <xlocale.h>
#endif

#include "TextureConversionShader.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"

#define WRITE p+=sprintf

static char text[16384];
static bool IntensityConstantAdded = false;

namespace TextureConversionShaderLegacy
{

const char* WriteRegister(const char *prefix, const u32 num)
{
	static char result[64];
	sprintf(result, " : register(%s%d)", prefix, num);
	return result;
}

static bool EFBFormatHasAlpha(u32 format)
{
	return format == PEControl::RGBA6_Z24;
}

// block dimensions : widthStride, heightStride 
// texture dims : width, height, x offset, y offset
void WriteSwizzler(char*& p, u32 format)
{
	// [0] left, top, right, bottom of source rectangle within source texture
	// [1] width and height of destination texture in pixels
	// Two were merged for GLSL
	WRITE(p, "uniform float4 " I_COLORS"[2] %s;\n", WriteRegister("c", C_COLORS));

	u32 blkW = TexDecoder_GetBlockWidthInTexels(format);
	u32 blkH = TexDecoder_GetBlockHeightInTexels(format);
	u32 samples = TextureConversionShader::GetEncodedSampleCount(format);

	WRITE(p, "uniform sampler samp0 : register(s0);\n");
	WRITE(p, "void main(\n");
	WRITE(p, "  out float4 ocol0 : SV_Target,\n");
	WRITE(p, "  in float2 uv0 : TEXCOORD0)\n");

	WRITE(p, "{\n"
		"  float2 sampleUv;\n"
		"  float2 uv1 = floor(uv0);\n");

	WRITE(p, "  uv1.x = uv1.x * %u.0;\n", samples);

	WRITE(p, "  float xl =  floor(uv1.x / %u.0);\n", blkW);
	WRITE(p, "  float xib = uv1.x - (xl * %u.0);\n", blkW);
	WRITE(p, "  float yl = floor(uv1.y / %u.0);\n", blkH);
	WRITE(p, "  float yb = yl * %u.0;\n", blkH);
	WRITE(p, "  float yoff = uv1.y - yb;\n");
	WRITE(p, "  float xp = uv1.x + (yoff * " I_COLORS"[1].x);\n");
	WRITE(p, "  float xel = floor(xp / %u.0);\n", blkW);
	WRITE(p, "  float xb = floor(xel / %u.0);\n", blkH);
	WRITE(p, "  float xoff = xel - (xb * %u.0);\n", blkH);

	WRITE(p, "  sampleUv.x = xib + (xb * %u.0);\n", blkW);
	WRITE(p, "  sampleUv.y = yb + xoff;\n");

	WRITE(p, "  sampleUv = sampleUv * " I_COLORS"[0].xy;\n");

	WRITE(p, "  sampleUv = sampleUv + " I_COLORS"[1].zw;\n");

	WRITE(p, "  sampleUv = sampleUv + float2(0.0f,1.0f);\n");// still to determine the reason for this
	WRITE(p, "  sampleUv = sampleUv / " I_COLORS"[0].zw;\n");
}

// block dimensions : widthStride, heightStride 
// texture dims : width, height, x offset, y offset
void Write32BitSwizzler(char*& p, u32 format)
{
	// [0] left, top, right, bottom of source rectangle within source texture
	// [1] width and height of destination texture in pixels
	// Two were merged for GLSL
	WRITE(p, "uniform float4 " I_COLORS"[2] %s;\n", WriteRegister("c", C_COLORS));

	u32 blkW = TexDecoder_GetBlockWidthInTexels(format);
	u32 blkH = TexDecoder_GetBlockHeightInTexels(format);

	// 32 bit textures (RGBA8 and Z24) are store in 2 cache line increments
	WRITE(p, "uniform sampler samp0 : register(s0);\n");

	WRITE(p, "void main(\n");
	WRITE(p, "  out float4 ocol0 : COLOR0,\n");
	WRITE(p, "  in float2 uv0 : TEXCOORD0)\n");
	WRITE(p, "{\n"
		"  float2 sampleUv;\n"
		"  float2 uv1 = floor(uv0);\n");

	WRITE(p, "  float yl = floor(uv1.y / %u.0);\n", blkH);
	WRITE(p, "  float yb = yl * %u.0;\n", blkH);
	WRITE(p, "  float yoff = uv1.y - yb;\n");
	WRITE(p, "  float xp = uv1.x + (yoff * " I_COLORS"[1].x);\n");
	WRITE(p, "  float xel = floor(xp / 2.0f);\n");
	WRITE(p, "  float xb = floor(xel / %u.0);\n", blkH);
	WRITE(p, "  float xoff = xel - (xb * %u.0);\n", blkH);

	WRITE(p, "  float x2 = uv1.x * 2.0f;\n");
	WRITE(p, "  float xl = floor(x2 / %u.0);\n", blkW);
	WRITE(p, "  float xib = x2 - (xl * %u.0);\n", blkW);
	WRITE(p, "  float halfxb = floor(xb / 2.0f);\n");

	WRITE(p, "  sampleUv.x = xib + (halfxb * %u.0);\n", blkW);
	WRITE(p, "  sampleUv.y = yb + xoff;\n");
	WRITE(p, "  sampleUv = sampleUv * " I_COLORS"[0].xy;\n");

	WRITE(p, "  sampleUv = sampleUv + " I_COLORS"[1].zw;\n");

	WRITE(p, "  sampleUv = sampleUv + float2(0.0f,1.0f);\n");// still to determine the reason for this
	WRITE(p, "  sampleUv = sampleUv / " I_COLORS"[0].zw;\n");
}

void WriteSampleColor(char*& p, const char* colorComp, const char* dest, int xoffset, const EFBCopyFormat& format, bool depth = false)
{
	WRITE(p, "  %s = tex2D(samp0, sampleUv + float2(%d.0f * (" I_COLORS "[0].x / " I_COLORS "[0].z), 0.0f)).%s;\n",
		dest, xoffset, colorComp);
	if (!depth)
	{
		// Truncate 8-bits to 5/6-bits per channel.
		switch (format.efb_format)
		{
		case PEControl::RGBA6_Z24:
			WRITE(p, "  %s = floor(%s * 63.0) / 63.0;\n", dest, dest);
			break;

		case PEControl::RGB565_Z16:
			WRITE(
				p,
				"  %s = floor(%s * float4(31.0, 63.0, 31.0, 1.0).%s) / float4(31.0, 63.0, 31.0, 1.0).%s;\n",
				dest, dest, colorComp, colorComp);
			break;
		}

		// Alpha channel is set to 1 in the copy if the EFB does not have an alpha channel.
		if (std::strchr(colorComp, 'a') && !EFBFormatHasAlpha(format.efb_format))
			WRITE(p, "  %s.a = 1.0;\n", dest);
	}
	
}

void WriteColorToIntensity(char*& p, const char* src, const char* dest)
{
	if (!IntensityConstantAdded)
	{
		WRITE(p, "  float4 IntensityConst = float4(0.257f,0.504f,0.098f,0.0625f);\n");
		IntensityConstantAdded = true;
	}
	WRITE(p, "  %s = dot(IntensityConst.rgb, %s.rgb);\n", dest, src);
	// don't add IntensityConst.a yet, because doing it later is faster and uses less instructions, due to vectorization
}

void WriteToBitDepth(char*& p, u8 depth, const char* src, const char* dest)
{
	WRITE(p, "  %s = floor(%s * 255.0 / exp2(8.0 - %d.0));\n", dest, src, depth);
}

void WriteEncoderEnd(char* p)
{
	WRITE(p, "}\n");
	IntensityConstantAdded = false;
}

void WriteI8Encoder(char* p, const EFBCopyFormat& format)
{
	WriteSwizzler(p, GX_TF_I8);
	WRITE(p, "  float3 texSample;\n");

	WriteSampleColor(p, "rgb", "texSample", 0, format);
	WriteColorToIntensity(p, "texSample", "ocol0.b");

	WriteSampleColor(p, "rgb", "texSample", 1, format);
	WriteColorToIntensity(p, "texSample", "ocol0.g");

	WriteSampleColor(p, "rgb", "texSample", 2, format);
	WriteColorToIntensity(p, "texSample", "ocol0.r");

	WriteSampleColor(p, "rgb", "texSample", 3, format);
	WriteColorToIntensity(p, "texSample", "ocol0.a");

	WRITE(p, "  ocol0.rgba += IntensityConst.aaaa;\n"); // see WriteColorToIntensity

	WriteEncoderEnd(p);
}

void WriteI4Encoder(char* p, const EFBCopyFormat& format)
{
	WriteSwizzler(p, GX_TF_I4);
	WRITE(p, "  float3 texSample;\n");
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, "rgb", "texSample", 0, format);
	WriteColorToIntensity(p, "texSample", "color0.b");

	WriteSampleColor(p, "rgb", "texSample", 1, format);
	WriteColorToIntensity(p, "texSample", "color1.b");

	WriteSampleColor(p, "rgb", "texSample", 2, format);
	WriteColorToIntensity(p, "texSample", "color0.g");

	WriteSampleColor(p, "rgb", "texSample", 3, format);
	WriteColorToIntensity(p, "texSample", "color1.g");

	WriteSampleColor(p, "rgb", "texSample", 4, format);
	WriteColorToIntensity(p, "texSample", "color0.r");

	WriteSampleColor(p, "rgb", "texSample", 5, format);
	WriteColorToIntensity(p, "texSample", "color1.r");

	WriteSampleColor(p, "rgb", "texSample", 6, format);
	WriteColorToIntensity(p, "texSample", "color0.a");

	WriteSampleColor(p, "rgb", "texSample", 7, format);
	WriteColorToIntensity(p, "texSample", "color1.a");

	WRITE(p, "  color0.rgba += IntensityConst.aaaa;\n");
	WRITE(p, "  color1.rgba += IntensityConst.aaaa;\n");

	WriteToBitDepth(p, 4, "color0", "color0");
	WriteToBitDepth(p, 4, "color1", "color1");

	WRITE(p, "  ocol0 = (color0 * 16.0f + color1) / 255.0f;\n");
	WriteEncoderEnd(p);
}

void WriteIA8Encoder(char* p, const EFBCopyFormat& format)
{
	WriteSwizzler(p, GX_TF_IA8);
	WRITE(p, "  float4 texSample;\n");

	WriteSampleColor(p, "rgba", "texSample", 0, format);
	WRITE(p, "  ocol0.b = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "ocol0.g");

	WriteSampleColor(p, "rgba", "texSample", 1, format);
	WRITE(p, "  ocol0.r = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "ocol0.a");

	WRITE(p, "  ocol0.ga += IntensityConst.aa;\n");

	WriteEncoderEnd(p);
}

void WriteIA4Encoder(char* p, const EFBCopyFormat& format)
{
	WriteSwizzler(p, GX_TF_IA4);
	WRITE(p, "  float4 texSample;\n");
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, "rgba", "texSample", 0, format);
	WRITE(p, "  color0.b = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "color1.b");

	WriteSampleColor(p, "rgba", "texSample", 1, format);
	WRITE(p, "  color0.g = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "color1.g");

	WriteSampleColor(p, "rgba", "texSample", 2, format);
	WRITE(p, "  color0.r = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "color1.r");

	WriteSampleColor(p, "rgba", "texSample", 3, format);
	WRITE(p, "  color0.a = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "color1.a");

	WRITE(p, "  color1.rgba += IntensityConst.aaaa;\n");

	WriteToBitDepth(p, 4, "color0", "color0");
	WriteToBitDepth(p, 4, "color1", "color1");

	WRITE(p, "  ocol0 = (color0 * 16.0f + color1) / 255.0f;\n");
	WriteEncoderEnd(p);
}

void WriteRGB565Encoder(char* p, const EFBCopyFormat& format)
{
	WriteSwizzler(p, GX_TF_RGB565);
	WRITE(p, "  float3 texSample0;\n");
	WRITE(p, "  float3 texSample1;\n");
	WriteSampleColor(p, "rgb", "texSample0", 0, format);
	WriteSampleColor(p, "rgb", "texSample1", 1, format);
	WRITE(p, "  float2 texRs = float2(texSample0.r, texSample1.r);\n");
	WRITE(p, "  float2 texGs = float2(texSample0.g, texSample1.g);\n");
	WRITE(p, "  float2 texBs = float2(texSample0.b, texSample1.b);\n");

	WriteToBitDepth(p, 6, "texGs", "float2 gInt");
	WRITE(p, "  float2 gUpper = floor(gInt / 8.0f);\n");
	WRITE(p, "  float2 gLower = gInt - gUpper * 8.0f;\n");

	WriteToBitDepth(p, 5, "texRs", "ocol0.br");
	WRITE(p, "  ocol0.br = ocol0.br * 8.0f + gUpper;\n");
	WriteToBitDepth(p, 5, "texBs", "ocol0.ga");
	WRITE(p, "  ocol0.ga = ocol0.ga + gLower * 32.0f;\n");

	WRITE(p, "  ocol0 = ocol0 / 255.0f;\n");
	WriteEncoderEnd(p);
}

void WriteRGB5A3Encoder(char* p, const EFBCopyFormat& format)
{
	WriteSwizzler(p, GX_TF_RGB5A3);

	WRITE(p, "  float4 texSample;\n");
	WRITE(p, "  float color0;\n");
	WRITE(p, "  float gUpper;\n");
	WRITE(p, "  float gLower;\n");

	WriteSampleColor(p, "rgba", "texSample", 0, format);

	// 0.8784 = 224 / 255 which is the maximum alpha value that can be represented in 3 bits
	WRITE(p, "if(texSample.a > 0.878f) {\n");

	WriteToBitDepth(p, 5, "texSample.g", "color0");
	WRITE(p, "  gUpper = floor(color0 / 8.0f);\n");
	WRITE(p, "  gLower = color0 - gUpper * 8.0f;\n");

	WriteToBitDepth(p, 5, "texSample.r", "ocol0.b");
	WRITE(p, "  ocol0.b = ocol0.b * 4.0f + gUpper + 128.0f;\n");
	WriteToBitDepth(p, 5, "texSample.b", "ocol0.g");
	WRITE(p, "  ocol0.g = ocol0.g + gLower * 32.0f;\n");

	WRITE(p, "} else {\n");

	WriteToBitDepth(p, 4, "texSample.r", "ocol0.b");
	WriteToBitDepth(p, 4, "texSample.b", "ocol0.g");

	WriteToBitDepth(p, 3, "texSample.a", "color0");
	WRITE(p, "ocol0.b = ocol0.b + color0 * 16.0f;\n");
	WriteToBitDepth(p, 4, "texSample.g", "color0");
	WRITE(p, "ocol0.g = ocol0.g + color0 * 16.0f;\n");

	WRITE(p, "}\n");


	WriteSampleColor(p, "rgba", "texSample", 1, format);

	WRITE(p, "if(texSample.a > 0.878f) {\n");

	WriteToBitDepth(p, 5, "texSample.g", "color0");
	WRITE(p, "  gUpper = floor(color0 / 8.0f);\n");
	WRITE(p, "  gLower = color0 - gUpper * 8.0f;\n");

	WriteToBitDepth(p, 5, "texSample.r", "ocol0.r");
	WRITE(p, "  ocol0.r = ocol0.r * 4.0f + gUpper + 128.0f;\n");
	WriteToBitDepth(p, 5, "texSample.b", "ocol0.a");
	WRITE(p, "  ocol0.a = ocol0.a + gLower * 32.0f;\n");

	WRITE(p, "} else {\n");

	WriteToBitDepth(p, 4, "texSample.r", "ocol0.r");
	WriteToBitDepth(p, 4, "texSample.b", "ocol0.a");

	WriteToBitDepth(p, 3, "texSample.a", "color0");
	WRITE(p, "ocol0.r = ocol0.r + color0 * 16.0f;\n");
	WriteToBitDepth(p, 4, "texSample.g", "color0");
	WRITE(p, "ocol0.a = ocol0.a + color0 * 16.0f;\n");

	WRITE(p, "}\n");

	WRITE(p, "  ocol0 = ocol0 / 255.0f;\n");
	WriteEncoderEnd(p);
}

void WriteRGBA8Encoder(char* p, const EFBCopyFormat& format)
{
	Write32BitSwizzler(p, GX_TF_RGBA8);

	WRITE(p, "  float cl1 = xb - (halfxb * 2.0f);\n");
	WRITE(p, "  float cl0 = 1.0f - cl1;\n");

	WRITE(p, "  float4 texSample;\n");
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, "rgba", "texSample", 0, format);
	WRITE(p, "  color0.b = texSample.a;\n");
	WRITE(p, "  color0.g = texSample.r;\n");
	WRITE(p, "  color1.b = texSample.g;\n");
	WRITE(p, "  color1.g = texSample.b;\n");

	WriteSampleColor(p, "rgba", "texSample", 1, format);
	WRITE(p, "  color0.r = texSample.a;\n");
	WRITE(p, "  color0.a = texSample.r;\n");
	WRITE(p, "  color1.r = texSample.g;\n");
	WRITE(p, "  color1.a = texSample.b;\n");

	WRITE(p, "  ocol0 = (cl0 * color0) + (cl1 * color1);\n");

	WriteEncoderEnd(p);
}

void WriteC4Encoder(char* p, const char* comp, const EFBCopyFormat& format)
{
	WriteSwizzler(p, GX_CTF_R4);
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, comp, "color0.b", 0, format);
	WriteSampleColor(p, comp, "color1.b", 1, format);
	WriteSampleColor(p, comp, "color0.g", 2, format);
	WriteSampleColor(p, comp, "color1.g", 3, format);
	WriteSampleColor(p, comp, "color0.r", 4, format);
	WriteSampleColor(p, comp, "color1.r", 5, format);
	WriteSampleColor(p, comp, "color0.a", 6, format);
	WriteSampleColor(p, comp, "color1.a", 7, format);

	WriteToBitDepth(p, 4, "color0", "color0");
	WriteToBitDepth(p, 4, "color1", "color1");

	WRITE(p, "  ocol0 = (color0 * 16.0f + color1) / 255.0f;\n");
	WriteEncoderEnd(p);
}

void WriteC8Encoder(char* p, const char* comp, const EFBCopyFormat& format)
{
	WriteSwizzler(p, GX_CTF_R8);

	WriteSampleColor(p, comp, "ocol0.b", 0, format);
	WriteSampleColor(p, comp, "ocol0.g", 1, format);
	WriteSampleColor(p, comp, "ocol0.r", 2, format);
	WriteSampleColor(p, comp, "ocol0.a", 3, format);

	WriteEncoderEnd(p);
}

void WriteCC4Encoder(char* p, const char* comp, const EFBCopyFormat& format)
{
	WriteSwizzler(p, GX_CTF_RA4);
	WRITE(p, "  float2 texSample;\n");
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, comp, "texSample", 0, format);
	WRITE(p, "  color0.b = texSample.x;\n");
	WRITE(p, "  color1.b = texSample.y;\n");

	WriteSampleColor(p, comp, "texSample", 1, format);
	WRITE(p, "  color0.g = texSample.x;\n");
	WRITE(p, "  color1.g = texSample.y;\n");

	WriteSampleColor(p, comp, "texSample", 2, format);
	WRITE(p, "  color0.r = texSample.x;\n");
	WRITE(p, "  color1.r = texSample.y;\n");

	WriteSampleColor(p, comp, "texSample", 3, format);
	WRITE(p, "  color0.a = texSample.x;\n");
	WRITE(p, "  color1.a = texSample.y;\n");

	WriteToBitDepth(p, 4, "color0", "color0");
	WriteToBitDepth(p, 4, "color1", "color1");

	WRITE(p, "  ocol0 = (color0 * 16.0f + color1) / 255.0f;\n");
	WriteEncoderEnd(p);
}

void WriteCC8Encoder(char* p, const char* comp, const EFBCopyFormat& format)
{
	WriteSwizzler(p, GX_CTF_RA8);

	WriteSampleColor(p, comp, "ocol0.bg", 0, format);
	WriteSampleColor(p, comp, "ocol0.ra", 1, format);

	WriteEncoderEnd(p);
}

void WriteZ8Encoder(char* p, const char* multiplier,const EFBCopyFormat& format)
{
	WriteSwizzler(p, GX_CTF_Z8M);

	WRITE(p, " float depth;\n");

	WriteSampleColor(p, "b", "depth", 0, format, true);
	WRITE(p, " depth = 1.0f - depth;\n");
	WRITE(p, "ocol0.b = frac(depth * %s);\n", multiplier);

	WriteSampleColor(p, "b", "depth", 1, format, true);
	WRITE(p, " depth = 1.0f - depth;\n");
	WRITE(p, "ocol0.g = frac(depth * %s);\n", multiplier);

	WriteSampleColor(p, "b", "depth", 2, format, true);
	WRITE(p, " depth = 1.0f - depth;\n");
	WRITE(p, "ocol0.r = frac(depth * %s);\n", multiplier);

	WriteSampleColor(p, "b", "depth", 3, format, true);
	WRITE(p, " depth = 1.0f - depth;\n");
	WRITE(p, "ocol0.a = frac(depth * %s);\n", multiplier);

	WriteEncoderEnd(p);
}

void WriteZ16Encoder(char* p, const EFBCopyFormat& format)
{
	WriteSwizzler(p, GX_TF_Z16);

	WRITE(p, "  float depth;\n");
	WRITE(p, "  float3 expanded;\n");

	// byte order is reversed

	WriteSampleColor(p, "b", "depth", 0, format, true);
	WRITE(p, " depth = 1.0f - depth;\n");
	WRITE(p, "  depth *= 16777215.0f;\n");
	WRITE(p, "  expanded.r = floor(depth / (256.0f * 256.0f));\n");
	WRITE(p, "  depth -= expanded.r * 256.0f * 256.0f;\n");
	WRITE(p, "  expanded.g = floor(depth / 256.0f);\n");

	WRITE(p, "  ocol0.b = expanded.g / 255.0f;\n");
	WRITE(p, "  ocol0.g = expanded.r / 255.0f;\n");

	WriteSampleColor(p, "b", "depth", 1, format, true);
	WRITE(p, " depth = 1.0f - depth;\n");
	WRITE(p, "  depth *= 16777215.0f;\n");
	WRITE(p, "  expanded.r = floor(depth / (256.0f * 256.0f));\n");
	WRITE(p, "  depth -= expanded.r * 256.0f * 256.0f;\n");
	WRITE(p, "  expanded.g = floor(depth / 256.0f);\n");

	WRITE(p, "  ocol0.r = expanded.g / 255.0f;\n");
	WRITE(p, "  ocol0.a = expanded.r / 255.0f;\n");

	WriteEncoderEnd(p);
}

void WriteZ16LEncoder(char* p, const EFBCopyFormat& format)
{
	WriteSwizzler(p, GX_CTF_Z16L);

	WRITE(p, "  float depth;\n");
	WRITE(p, "  float3 expanded;\n");

	// byte order is reversed

	WriteSampleColor(p, "b", "depth", 0, format, true);
	WRITE(p, " depth = 1.0f - depth;\n");
	WRITE(p, "  depth *= 16777215.0f;\n");
	WRITE(p, "  expanded.r = floor(depth / (256.0f * 256.0f));\n");
	WRITE(p, "  depth -= expanded.r * 256.0f * 256.0f;\n");
	WRITE(p, "  expanded.g = floor(depth / 256.0f);\n");
	WRITE(p, "  depth -= expanded.g * 256.0f;\n");
	WRITE(p, "  expanded.b = depth;\n");

	WRITE(p, "  ocol0.b = expanded.b / 255.0f;\n");
	WRITE(p, "  ocol0.g = expanded.g / 255.0f;\n");

	WriteSampleColor(p, "b", "depth", 1, format, true);
	WRITE(p, " depth = 1.0f - depth;\n");
	WRITE(p, "  depth *= 16777215.0f;\n");
	WRITE(p, "  expanded.r = floor(depth / (256.0f * 256.0f));\n");
	WRITE(p, "  depth -= expanded.r * 256.0f * 256.0f;\n");
	WRITE(p, "  expanded.g = floor(depth / 256.0f);\n");
	WRITE(p, "  depth -= expanded.g * 256.0f;\n");
	WRITE(p, "  expanded.b = depth;\n");

	WRITE(p, "  ocol0.r = expanded.b / 255.0f;\n");
	WRITE(p, "  ocol0.a = expanded.g / 255.0f;\n");

	WriteEncoderEnd(p);
}

void WriteZ24Encoder(char* p, const EFBCopyFormat& format)
{
	Write32BitSwizzler(p, GX_TF_Z24X8);

	WRITE(p, "  float cl = xb - (halfxb * 2.0f);\n");

	WRITE(p, "  float depth0;\n");
	WRITE(p, "  float depth1;\n");
	WRITE(p, "  float3 expanded0;\n");
	WRITE(p, "  float3 expanded1;\n");

	WriteSampleColor(p, "b", "depth0", 0, format, true);
	WRITE(p, " depth0 = 1.0f - depth0;\n");
	WriteSampleColor(p, "b", "depth1", 1, format, true);
	WRITE(p, " depth1 = 1.0f - depth1;\n");

	for (int i = 0; i < 2; i++)
	{
		WRITE(p, "  depth%i *= 16777215.0f;\n", i);

		WRITE(p, "  expanded%i.r = floor(depth%i / (256.0f * 256.0f));\n", i, i);
		WRITE(p, "  depth%i -= expanded%i.r * 256.0f * 256.0f;\n", i, i);
		WRITE(p, "  expanded%i.g = floor(depth%i / 256.0f);\n", i, i);
		WRITE(p, "  depth%i -= expanded%i.g * 256.0f;\n", i, i);
		WRITE(p, "  expanded%i.b = depth%i;\n", i, i);
	}

	WRITE(p, "  if(cl > 0.5f) {\n");
	// upper 16
	WRITE(p, "     ocol0.b = expanded0.g / 255.0f;\n");
	WRITE(p, "     ocol0.g = expanded0.b / 255.0f;\n");
	WRITE(p, "     ocol0.r = expanded1.g / 255.0f;\n");
	WRITE(p, "     ocol0.a = expanded1.b / 255.0f;\n");
	WRITE(p, "  } else {\n");
	// lower 8
	WRITE(p, "     ocol0.b = 1.0f;\n");
	WRITE(p, "     ocol0.g = expanded0.r / 255.0f;\n");
	WRITE(p, "     ocol0.r = 1.0f;\n");
	WRITE(p, "     ocol0.a = expanded1.r / 255.0f;\n");
	WRITE(p, "  }\n");

	WriteEncoderEnd(p);
}

const char *GenerateEncodingShader(const EFBCopyFormat& format)
{
	text[sizeof(text) - 1] = 0x7C;  // canary

	char *p = text;

	switch (format.copy_format)
	{
	case GX_TF_I4:
		WriteI4Encoder(p, format);
		break;
	case GX_TF_I8:
		WriteI8Encoder(p, format);
		break;
	case GX_TF_IA4:
		WriteIA4Encoder(p, format);
		break;
	case GX_TF_IA8:
		WriteIA8Encoder(p, format);
		break;
	case GX_TF_RGB565:
		WriteRGB565Encoder(p, format);
		break;
	case GX_TF_RGB5A3:
		WriteRGB5A3Encoder(p, format);
		break;
	case GX_TF_RGBA8:
		WriteRGBA8Encoder(p, format);
		break;
	case GX_CTF_R4:
		WriteC4Encoder(p, "r", format);
		break;
	case GX_CTF_RA4:
		WriteCC4Encoder(p, "ar", format);
		break;
	case GX_CTF_RA8:
		WriteCC8Encoder(p, "ar", format);
		break;
	case GX_CTF_A8:
		WriteC8Encoder(p, "a", format);
		break;
	case GX_CTF_R8:
		WriteC8Encoder(p, "r", format);
		break;
	case GX_CTF_G8:
		WriteC8Encoder(p, "g", format);
		break;
	case GX_CTF_B8:
		WriteC8Encoder(p, "b", format);
		break;
	case GX_CTF_RG8:
		WriteCC8Encoder(p, "rg", format);
		break;
	case GX_CTF_GB8:
		WriteCC8Encoder(p, "gb", format);
		break;
	case GX_CTF_Z8H:
	case GX_TF_Z8:
		WriteC8Encoder(p, "b", format);
		break;
	case GX_CTF_Z16R:
	case GX_TF_Z16:
		WriteZ16Encoder(p, format);
		break;
	case GX_TF_Z24X8:
		WriteZ24Encoder(p, format);
		break;
	case GX_CTF_Z4:
		WriteC4Encoder(p, "b", format);
		break;
	case GX_CTF_Z8M:
		WriteZ8Encoder(p, "256.0f", format);
		break;
	case GX_CTF_Z8L:
		WriteZ8Encoder(p, "65536.0f", format);
		break;
	case GX_CTF_Z16L:
		WriteZ16LEncoder(p, format);
		break;
	default:
		PanicAlert("Unknown texture copy format: 0x%x\n", format);
		break;
	}

	if (text[sizeof(text) - 1] != 0x7C)
		PanicAlert("TextureConversionShader generator - buffer too small, canary has been eaten!");
	return text;
}

void SetShaderParameters(float width, float height, float offsetX, float offsetY, float widthStride, float heightStride, float buffW, float buffH)
{
	float* cbuff = PixelShaderManager::GetBufferToUpdate(C_COLORMATRIX, 2);
	cbuff[0] = widthStride;
	cbuff[1] = heightStride;
	cbuff[2] = buffW;
	cbuff[3] = buffH;
	cbuff[4] = width;
	cbuff[5] = height - 1;
	cbuff[6] = offsetX;
	cbuff[7] = offsetY;
}

}  // namespace
