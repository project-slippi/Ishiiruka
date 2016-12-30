// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/Memmap.h"
#include "VideoCommon/HLSLCompiler.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoBackends/DX11/D3DPtr.h"
#include "VideoBackends/DX11/D3DBase.h"
#include "VideoBackends/DX11/D3DShader.h"
#include "VideoBackends/DX11/FramebufferManager.h"
#include "VideoBackends/DX11/D3DState.h"
#include "VideoBackends/DX11/CSTextureEncoder.h"
#include "VideoBackends/DX11/Render.h"
#include "VideoBackends/DX11/TextureCache.h"

// "Static mode" will compile a new EFB encoder shader for every combination of
// encoding configurations. It's compatible with Shader Model 4.

// "Dynamic mode" will use the dynamic-linking feature of Shader Model 5. Only
// one shader needs to be compiled.

// Unfortunately, the June 2010 DirectX SDK includes a broken HLSL compiler
// which cripples dynamic linking for us.
// See <http://www.gamedev.net/topic/587232-dx11-dynamic-linking-compilation-warnings/>.
// Dynamic mode is disabled for now. To enable it, uncomment the line below.

//#define USE_DYNAMIC_MODE

// FIXME: When Microsoft fixes their HLSL compiler, make Dolphin enable dynamic
// mode on Shader Model 5-compatible cards.

namespace DX11
{

union EFBEncodeParams
{
	struct
	{
		FLOAT NumHalfCacheLinesX;
		FLOAT NumBlocksY;
		FLOAT PosX;
		FLOAT PosY;
		FLOAT TexLeft;
		FLOAT TexTop;
		FLOAT TexRight;
		FLOAT TexBottom;
	};
	// Constant buffers must be a multiple of 16 bytes in size.
	u8 pad[32]; // Pad to the next multiple of 16 bytes
};

static const char EFB_ENCODE_CS[] = R"HLSL(
// dolphin-emu EFB encoder compute shader

// Input
cbuffer cbParams : register(b0)
{
struct // Should match EFBEncodeParams above
{
	float NumHalfCacheLinesX;
	float NumBlocksY;
	float PosX; // Upper-left corner of source
	float PosY;
	float TexLeft; // Rectangle within EFBTexture representing the actual EFB (normalized)
	float TexTop;
	float TexRight;
	float TexBottom;
} Params;
}

Texture2DArray EFBTexture : register(t0);
sampler EFBSampler : register(s0);

// Constants

static const float2 INV_EFB_DIMS = float2(1.0/640.0, 1.0/528.0);

// FIXME: Is this correct?
static const float3 INTENSITY_COEFFS = float3(0.257, 0.504, 0.098);
static const float INTENSITY_ADD = 16.0/255.0;

// Utility functions

uint4 Swap4_32(uint4 v) {
return (((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000));
}

uint4 UINT4_8888_BE(uint4 a, uint4 b, uint4 c, uint4 d) {
return (d << 24) | (c << 16) | (b << 8) | a;
}

uint UINT_44444444_BE(uint a, uint b, uint c, uint d, uint e, uint f, uint g, uint h) {
return (g << 28) | (h << 24) | (e << 20) | (f << 16) | (c << 12) | (d << 8) | (a << 4) | b;
}

uint UINT_1555(uint a, uint b, uint c, uint d) {
return (a << 15) | (b << 10) | (c << 5) | d;
}

uint UINT_3444(uint a, uint b, uint c, uint d) {
return (a << 12) | (b << 8) | (c << 4) | d;
}

uint UINT_565(uint a, uint b, uint c) {
return (a << 11) | (b << 5) | c;
}

uint UINT_1616(uint a, uint b) {
return (a << 16) | b;
}

uint Float8ToUint3(float v) {
return (uint)(v*255.0) >> 5;
}

uint Float8ToUint4(float v) {
return (uint)(v*255.0) >> 4;
}

uint Float8ToUint5(float v) {
return (uint)(v*255.0) >> 3;
}

uint Float8ToUint6(float v) {
return (uint)(v*255.0) >> 2;
}

uint EncodeRGB5A3(float4 pixel) {
if (pixel.a >= 224.0/255.0) {
	// Encode to ARGB1555
	return UINT_1555(1, Float8ToUint5(pixel.r), Float8ToUint5(pixel.g), Float8ToUint5(pixel.b));
} else {
	// Encode to ARGB3444
	return UINT_3444(Float8ToUint3(pixel.a), Float8ToUint4(pixel.r), Float8ToUint4(pixel.g), Float8ToUint4(pixel.b));
}
}

uint EncodeRGB565(float4 pixel) {
return UINT_565(Float8ToUint5(pixel.r), Float8ToUint6(pixel.g), Float8ToUint5(pixel.b));
}

float2 CalcTexCoord(float2 coord)
{
// Add 0.5,0.5 to sample from the center of the EFB pixel
float2 efbCoord = coord + float2(0.5,0.5);
return lerp(float2(Params.TexLeft,Params.TexTop), float2(Params.TexRight,Params.TexBottom), efbCoord * INV_EFB_DIMS);
}

// Interface and classes for different source formats

float4 Fetch_0(float2 coord)
{
float3 texCoord = float3(CalcTexCoord(coord), 0.0);
return EFBTexture.SampleLevel(EFBSampler, texCoord, 0);
}

float4 Fetch_1(float2 coord)
{
float3 texCoord = float3(CalcTexCoord(coord), 0.0);
uint depth24 = 0xFFFFFF - (0xFFFFFF * EFBTexture.SampleLevel(EFBSampler, texCoord, 0).r);
uint4 bytes = uint4(
	(depth24 >> 16) & 0xFF, // r
	(depth24 >> 8) & 0xFF,  // g
	depth24 & 0xFF,         // b
	255);                   // a
return bytes / 255.0;
}

#ifdef DYNAMIC_MODE
interface iFetch
{
float4 Fetch(float2 coord);
};

// Source format 0
class cFetch_0 : iFetch
{
float4 Fetch(float2 coord)
{ return Fetch_0(coord); }
};

// Source format 1
class cFetch_1 : iFetch
{
float4 Fetch(float2 coord)
{ return Fetch_1(coord); }
};

// Declare fetch interface; must be set by application
iFetch g_fetch;
#define IMP_FETCH g_fetch.Fetch

#endif // #ifdef DYNAMIC_MODE

#ifndef IMP_FETCH
#error No Fetch specified
#endif

// Interface and classes for different intensity settings (on or off)

float4 Intensity_0(float4 sample)
{
return sample;
}

float4 Intensity_1(float4 sample)
{
sample.r = dot(INTENSITY_COEFFS, sample.rgb) + INTENSITY_ADD;
// FIXME: Is this correct? What happens if you use one of the non-R
// formats with intensity on?
sample = sample.rrrr;
return sample;
}

#ifdef DYNAMIC_MODE
interface iIntensity
{
float4 Intensity(float4 sample);
};

// Intensity off
class cIntensity_0 : iIntensity
{
float4 Intensity(float4 sample)
{ return Intensity_0(sample); }
};

// Intensity on
class cIntensity_1 : iIntensity
{
float4 Intensity(float4 sample)
{ return Intensity_1(sample); }
};

// Declare intensity interface; must be set by application
iIntensity g_intensity;
#define IMP_INTENSITY g_intensity.Intensity

#endif // #ifdef DYNAMIC_MODE

#ifndef IMP_INTENSITY
#error No Intensity specified
#endif

// Interface and classes for different scale/filter settings (on or off)

float4 ScaledFetch_0(float2 coord)
{
return IMP_FETCH(float2(Params.PosX,Params.PosY) + coord);
}

float4 ScaledFetch_1(float2 coord)
{
float2 ul = float2(Params.PosX,Params.PosY) + 2*coord;
float4 sample0 = IMP_FETCH(ul+float2(0,0));
float4 sample1 = IMP_FETCH(ul+float2(1,0));
float4 sample2 = IMP_FETCH(ul+float2(0,1));
float4 sample3 = IMP_FETCH(ul+float2(1,1));
// Average all four samples together
// FIXME: Is this correct?
return 0.25 * (sample0+sample1+sample2+sample3);
}

#ifdef DYNAMIC_MODE
interface iScaledFetch
{
float4 ScaledFetch(float2 coord);
};

// Scale off
class cScaledFetch_0 : iScaledFetch
{
float4 ScaledFetch(float2 coord)
{ return ScaledFetch_0(coord); }
};

// Scale on
class cScaledFetch_1 : iScaledFetch
{
float4 ScaledFetch(float2 coord)
{ return ScaledFetch_1(coord); }
};

// Declare scaled fetch interface; must be set by application code
iScaledFetch g_scaledFetch;
#define IMP_SCALEDFETCH g_scaledFetch.ScaledFetch

#endif // #ifdef DYNAMIC_MODE

#ifndef IMP_SCALEDFETCH
#error No ScaledFetch specified
#endif

// Main EFB-sampling function: performs all steps of fetching pixels, scaling,
// applying intensity function

float4 SampleEFB(float2 coord)
{
// FIXME: Does intensity happen before or after scaling? Or does
// it matter?
float4 sample = IMP_SCALEDFETCH(coord);
return IMP_INTENSITY(sample);
}

// Interfaces and classes for different destination formats

uint4 Generate_0(float2 cacheCoord) // R4
{
float2 blockCoord = floor(cacheCoord / float2(2,1));

float2 blockUL = blockCoord * float2(8,8);
float2 subBlockUL = blockUL + float2(0, 4*(cacheCoord.x%2));

float4 sample[32];
for (uint y = 0; y < 4; ++y) {
	for (uint x = 0; x < 8; ++x) {
		sample[y*8+x] = SampleEFB(subBlockUL+float2(x,y));
	}
}

uint dw[4];
for (uint i = 0; i < 4; ++i) {
	dw[i] = UINT_44444444_BE(
		Float8ToUint4(sample[8*i+0].r),
		Float8ToUint4(sample[8*i+1].r),
		Float8ToUint4(sample[8*i+2].r),
		Float8ToUint4(sample[8*i+3].r),
		Float8ToUint4(sample[8*i+4].r),
		Float8ToUint4(sample[8*i+5].r),
		Float8ToUint4(sample[8*i+6].r),
		Float8ToUint4(sample[8*i+7].r)
		);
}

return uint4(dw[0], dw[1], dw[2], dw[3]);
}
)HLSL" // Visual do not like string literal of more than 16K and it is legit, that should be an external resource
R"HLSL(
// FIXME: Untested
uint4 Generate_1(float2 cacheCoord) // R8 (FIXME: Duplicate of R8 below?)
{
float2 blockCoord = floor(cacheCoord / float2(2,1));

float2 blockUL = blockCoord * float2(8,4);
float2 subBlockUL = blockUL + float2(0, 2*(cacheCoord.x%2));

float4 sample0 = SampleEFB(subBlockUL+float2(0,0));
float4 sample1 = SampleEFB(subBlockUL+float2(1,0));
float4 sample2 = SampleEFB(subBlockUL+float2(2,0));
float4 sample3 = SampleEFB(subBlockUL+float2(3,0));
float4 sample4 = SampleEFB(subBlockUL+float2(4,0));
float4 sample5 = SampleEFB(subBlockUL+float2(5,0));
float4 sample6 = SampleEFB(subBlockUL+float2(6,0));
float4 sample7 = SampleEFB(subBlockUL+float2(7,0));
float4 sample8 = SampleEFB(subBlockUL+float2(0,1));
float4 sample9 = SampleEFB(subBlockUL+float2(1,1));
float4 sampleA = SampleEFB(subBlockUL+float2(2,1));
float4 sampleB = SampleEFB(subBlockUL+float2(3,1));
float4 sampleC = SampleEFB(subBlockUL+float2(4,1));
float4 sampleD = SampleEFB(subBlockUL+float2(5,1));
float4 sampleE = SampleEFB(subBlockUL+float2(6,1));
float4 sampleF = SampleEFB(subBlockUL+float2(7,1));

uint4 dw4 = UINT4_8888_BE(
	255*float4(sample0.r, sample4.r, sample8.r, sampleC.r),
	255*float4(sample1.r, sample5.r, sample9.r, sampleD.r),
	255*float4(sample2.r, sample6.r, sampleA.r, sampleE.r),
	255*float4(sample3.r, sample7.r, sampleB.r, sampleF.r)
	);

return dw4;
}

// FIXME: Untested
uint4 Generate_2(float2 cacheCoord) // A4 R4
{
float2 blockCoord = floor(cacheCoord / float2(2,1));

float2 blockUL = blockCoord * float2(8,4);
float2 subBlockUL = blockUL + float2(0, 2*(cacheCoord.x%2));

float4 sample0 = SampleEFB(subBlockUL+float2(0,0));
float4 sample1 = SampleEFB(subBlockUL+float2(1,0));
float4 sample2 = SampleEFB(subBlockUL+float2(2,0));
float4 sample3 = SampleEFB(subBlockUL+float2(3,0));
float4 sample4 = SampleEFB(subBlockUL+float2(4,0));
float4 sample5 = SampleEFB(subBlockUL+float2(5,0));
float4 sample6 = SampleEFB(subBlockUL+float2(6,0));
float4 sample7 = SampleEFB(subBlockUL+float2(7,0));
float4 sample8 = SampleEFB(subBlockUL+float2(0,1));
float4 sample9 = SampleEFB(subBlockUL+float2(1,1));
float4 sampleA = SampleEFB(subBlockUL+float2(2,1));
float4 sampleB = SampleEFB(subBlockUL+float2(3,1));
float4 sampleC = SampleEFB(subBlockUL+float2(4,1));
float4 sampleD = SampleEFB(subBlockUL+float2(5,1));
float4 sampleE = SampleEFB(subBlockUL+float2(6,1));
float4 sampleF = SampleEFB(subBlockUL+float2(7,1));

uint dw0 = UINT_44444444_BE(
	Float8ToUint4(sample0.a), Float8ToUint4(sample0.r),
	Float8ToUint4(sample1.a), Float8ToUint4(sample1.r),
	Float8ToUint4(sample2.a), Float8ToUint4(sample2.r),
	Float8ToUint4(sample3.a), Float8ToUint4(sample3.r)
	);
uint dw1 = UINT_44444444_BE(
	Float8ToUint4(sample4.a), Float8ToUint4(sample4.r),
	Float8ToUint4(sample5.a), Float8ToUint4(sample5.r),
	Float8ToUint4(sample6.a), Float8ToUint4(sample6.r),
	Float8ToUint4(sample7.a), Float8ToUint4(sample7.r)
	);
uint dw2 = UINT_44444444_BE(
	Float8ToUint4(sample8.a), Float8ToUint4(sample8.r),
	Float8ToUint4(sample9.a), Float8ToUint4(sample9.r),
	Float8ToUint4(sampleA.a), Float8ToUint4(sampleA.r),
	Float8ToUint4(sampleB.a), Float8ToUint4(sampleB.r)
	);
uint dw3 = UINT_44444444_BE(
	Float8ToUint4(sampleC.a), Float8ToUint4(sampleC.r),
	Float8ToUint4(sampleD.a), Float8ToUint4(sampleD.r),
	Float8ToUint4(sampleE.a), Float8ToUint4(sampleE.r),
	Float8ToUint4(sampleF.a), Float8ToUint4(sampleF.r)
	);

return uint4(dw0, dw1, dw2, dw3);
}

// FIXME: Untested
uint4 Generate_3(float2 cacheCoord) // A8 R8
{
float2 blockCoord = floor(cacheCoord / float2(2,1));

float2 blockUL = blockCoord * float2(4,4);
float2 subBlockUL = blockUL + float2(0, 2*(cacheCoord.x%2));

float4 sample0 = SampleEFB(subBlockUL+float2(0,0));
float4 sample1 = SampleEFB(subBlockUL+float2(1,0));
float4 sample2 = SampleEFB(subBlockUL+float2(2,0));
float4 sample3 = SampleEFB(subBlockUL+float2(3,0));
float4 sample4 = SampleEFB(subBlockUL+float2(0,1));
float4 sample5 = SampleEFB(subBlockUL+float2(1,1));
float4 sample6 = SampleEFB(subBlockUL+float2(2,1));
float4 sample7 = SampleEFB(subBlockUL+float2(3,1));

uint4 dw4 = UINT4_8888_BE(
	255*float4(sample0.a, sample2.a, sample4.a, sample6.a),
	255*float4(sample0.r, sample2.r, sample4.r, sample6.r),
	255*float4(sample1.a, sample3.a, sample5.a, sample7.a),
	255*float4(sample1.r, sample3.r, sample5.r, sample7.r)
	);

return dw4;
}

uint4 Generate_4(float2 cacheCoord) // R5 G6 B5
{
float2 blockCoord = floor(cacheCoord / float2(2,1));

float2 blockUL = blockCoord * float2(4,4);
float2 subBlockUL = blockUL + float2(0, 2*(cacheCoord.x%2));

float4 sample0 = SampleEFB(subBlockUL+float2(0,0));
float4 sample1 = SampleEFB(subBlockUL+float2(1,0));
float4 sample2 = SampleEFB(subBlockUL+float2(2,0));
float4 sample3 = SampleEFB(subBlockUL+float2(3,0));
float4 sample4 = SampleEFB(subBlockUL+float2(0,1));
float4 sample5 = SampleEFB(subBlockUL+float2(1,1));
float4 sample6 = SampleEFB(subBlockUL+float2(2,1));
float4 sample7 = SampleEFB(subBlockUL+float2(3,1));

uint dw0 = UINT_1616(EncodeRGB565(sample0), EncodeRGB565(sample1));
uint dw1 = UINT_1616(EncodeRGB565(sample2), EncodeRGB565(sample3));
uint dw2 = UINT_1616(EncodeRGB565(sample4), EncodeRGB565(sample5));
uint dw3 = UINT_1616(EncodeRGB565(sample6), EncodeRGB565(sample7));

return Swap4_32(uint4(dw0, dw1, dw2, dw3));
}
)HLSL" // Visual do not like string literal of more than 16K and it is legit, that should be an external resource
R"HLSL(
uint4 Generate_5(float2 cacheCoord) // 1 R5 G5 B5 or 0 A3 R4 G4 G4
{
float2 blockCoord = floor(cacheCoord / float2(2,1));

float2 blockUL = blockCoord * float2(4,4);
float2 subBlockUL = blockUL + float2(0, 2*(cacheCoord.x%2));

float4 sample0 = SampleEFB(subBlockUL+float2(0,0));
float4 sample1 = SampleEFB(subBlockUL+float2(1,0));
float4 sample2 = SampleEFB(subBlockUL+float2(2,0));
float4 sample3 = SampleEFB(subBlockUL+float2(3,0));
float4 sample4 = SampleEFB(subBlockUL+float2(0,1));
float4 sample5 = SampleEFB(subBlockUL+float2(1,1));
float4 sample6 = SampleEFB(subBlockUL+float2(2,1));
float4 sample7 = SampleEFB(subBlockUL+float2(3,1));

uint dw0 = UINT_1616(EncodeRGB5A3(sample0), EncodeRGB5A3(sample1));
uint dw1 = UINT_1616(EncodeRGB5A3(sample2), EncodeRGB5A3(sample3));
uint dw2 = UINT_1616(EncodeRGB5A3(sample4), EncodeRGB5A3(sample5));
uint dw3 = UINT_1616(EncodeRGB5A3(sample6), EncodeRGB5A3(sample7));

return Swap4_32(uint4(dw0, dw1, dw2, dw3));
}

uint4 Generate_6(float2 cacheCoord) // A8 R8 A8 R8 | G8 B8 G8 B8
{
float2 blockCoord = floor(cacheCoord / float2(4,1));

float2 blockUL = blockCoord * float2(4,4);
float2 subBlockUL = blockUL + float2(0, 2*(cacheCoord.x%2));

float4 sample0 = SampleEFB(subBlockUL+float2(0,0));
float4 sample1 = SampleEFB(subBlockUL+float2(1,0));
float4 sample2 = SampleEFB(subBlockUL+float2(2,0));
float4 sample3 = SampleEFB(subBlockUL+float2(3,0));
float4 sample4 = SampleEFB(subBlockUL+float2(0,1));
float4 sample5 = SampleEFB(subBlockUL+float2(1,1));
float4 sample6 = SampleEFB(subBlockUL+float2(2,1));
float4 sample7 = SampleEFB(subBlockUL+float2(3,1));

uint4 dw4;
if (cacheCoord.x % 4 < 2)
{
	// First cache line gets AR
	dw4 = UINT4_8888_BE(
		255*float4(sample0.a, sample2.a, sample4.a, sample6.a),
		255*float4(sample0.r, sample2.r, sample4.r, sample6.r),
		255*float4(sample1.a, sample3.a, sample5.a, sample7.a),
		255*float4(sample1.r, sample3.r, sample5.r, sample7.r)
		);
}
else
{
	// Second cache line gets GB
	dw4 = UINT4_8888_BE(
		255*float4(sample0.g, sample2.g, sample4.g, sample6.g),
		255*float4(sample0.b, sample2.b, sample4.b, sample6.b),
		255*float4(sample1.g, sample3.g, sample5.g, sample7.g),
		255*float4(sample1.b, sample3.b, sample5.b, sample7.b)
		);
}

return dw4;
}

uint4 Generate_7(float2 cacheCoord) // A8
{
float2 blockCoord = floor(cacheCoord / float2(2,1));

float2 blockUL = blockCoord * float2(8,4);
float2 subBlockUL = blockUL + float2(0, 2*(cacheCoord.x%2));

float4 sample0 = SampleEFB(subBlockUL+float2(0,0));
float4 sample1 = SampleEFB(subBlockUL+float2(1,0));
float4 sample2 = SampleEFB(subBlockUL+float2(2,0));
float4 sample3 = SampleEFB(subBlockUL+float2(3,0));
float4 sample4 = SampleEFB(subBlockUL+float2(4,0));
float4 sample5 = SampleEFB(subBlockUL+float2(5,0));
float4 sample6 = SampleEFB(subBlockUL+float2(6,0));
float4 sample7 = SampleEFB(subBlockUL+float2(7,0));
float4 sample8 = SampleEFB(subBlockUL+float2(0,1));
float4 sample9 = SampleEFB(subBlockUL+float2(1,1));
float4 sampleA = SampleEFB(subBlockUL+float2(2,1));
float4 sampleB = SampleEFB(subBlockUL+float2(3,1));
float4 sampleC = SampleEFB(subBlockUL+float2(4,1));
float4 sampleD = SampleEFB(subBlockUL+float2(5,1));
float4 sampleE = SampleEFB(subBlockUL+float2(6,1));
float4 sampleF = SampleEFB(subBlockUL+float2(7,1));

uint4 dw4 = UINT4_8888_BE(
	255*float4(sample0.a, sample4.a, sample8.a, sampleC.a),
	255*float4(sample1.a, sample5.a, sample9.a, sampleD.a),
	255*float4(sample2.a, sample6.a, sampleA.a, sampleE.a),
	255*float4(sample3.a, sample7.a, sampleB.a, sampleF.a)
	);

return dw4;
}

uint4 Generate_8(float2 cacheCoord) // R8
{
float2 blockCoord = floor(cacheCoord / float2(2,1));

float2 blockUL = blockCoord * float2(8,4);
float2 subBlockUL = blockUL + float2(0, 2*(cacheCoord.x%2));

float4 sample0 = SampleEFB(subBlockUL+float2(0,0));
float4 sample1 = SampleEFB(subBlockUL+float2(1,0));
float4 sample2 = SampleEFB(subBlockUL+float2(2,0));
float4 sample3 = SampleEFB(subBlockUL+float2(3,0));
float4 sample4 = SampleEFB(subBlockUL+float2(4,0));
float4 sample5 = SampleEFB(subBlockUL+float2(5,0));
float4 sample6 = SampleEFB(subBlockUL+float2(6,0));
float4 sample7 = SampleEFB(subBlockUL+float2(7,0));
float4 sample8 = SampleEFB(subBlockUL+float2(0,1));
float4 sample9 = SampleEFB(subBlockUL+float2(1,1));
float4 sampleA = SampleEFB(subBlockUL+float2(2,1));
float4 sampleB = SampleEFB(subBlockUL+float2(3,1));
float4 sampleC = SampleEFB(subBlockUL+float2(4,1));
float4 sampleD = SampleEFB(subBlockUL+float2(5,1));
float4 sampleE = SampleEFB(subBlockUL+float2(6,1));
float4 sampleF = SampleEFB(subBlockUL+float2(7,1));

uint4 dw4 = UINT4_8888_BE(
	255*float4(sample0.r, sample4.r, sample8.r, sampleC.r),
	255*float4(sample1.r, sample5.r, sample9.r, sampleD.r),
	255*float4(sample2.r, sample6.r, sampleA.r, sampleE.r),
	255*float4(sample3.r, sample7.r, sampleB.r, sampleF.r)
	);

return dw4;
}

// FIXME: Untested
uint4 Generate_9(float2 cacheCoord) // G8
{
float2 blockCoord = floor(cacheCoord / float2(2,1));

float2 blockUL = blockCoord * float2(8,4);
float2 subBlockUL = blockUL + float2(0, 2*(cacheCoord.x%2));

float4 sample0 = SampleEFB(subBlockUL+float2(0,0));
float4 sample1 = SampleEFB(subBlockUL+float2(1,0));
float4 sample2 = SampleEFB(subBlockUL+float2(2,0));
float4 sample3 = SampleEFB(subBlockUL+float2(3,0));
float4 sample4 = SampleEFB(subBlockUL+float2(4,0));
float4 sample5 = SampleEFB(subBlockUL+float2(5,0));
float4 sample6 = SampleEFB(subBlockUL+float2(6,0));
float4 sample7 = SampleEFB(subBlockUL+float2(7,0));
float4 sample8 = SampleEFB(subBlockUL+float2(0,1));
float4 sample9 = SampleEFB(subBlockUL+float2(1,1));
float4 sampleA = SampleEFB(subBlockUL+float2(2,1));
float4 sampleB = SampleEFB(subBlockUL+float2(3,1));
float4 sampleC = SampleEFB(subBlockUL+float2(4,1));
float4 sampleD = SampleEFB(subBlockUL+float2(5,1));
float4 sampleE = SampleEFB(subBlockUL+float2(6,1));
float4 sampleF = SampleEFB(subBlockUL+float2(7,1));

uint4 dw4 = UINT4_8888_BE(
	255*float4(sample0.g, sample4.g, sample8.g, sampleC.g),
	255*float4(sample1.g, sample5.g, sample9.g, sampleD.g),
	255*float4(sample2.g, sample6.g, sampleA.g, sampleE.g),
	255*float4(sample3.g, sample7.g, sampleB.g, sampleF.g)
	);

return dw4;
}

uint4 Generate_A(float2 cacheCoord) // B8
{
float2 blockCoord = floor(cacheCoord / float2(2,1));

float2 blockUL = blockCoord * float2(8,4);
float2 subBlockUL = blockUL + float2(0, 2*(cacheCoord.x%2));

float4 sample0 = SampleEFB(subBlockUL+float2(0,0));
float4 sample1 = SampleEFB(subBlockUL+float2(1,0));
float4 sample2 = SampleEFB(subBlockUL+float2(2,0));
float4 sample3 = SampleEFB(subBlockUL+float2(3,0));
float4 sample4 = SampleEFB(subBlockUL+float2(4,0));
float4 sample5 = SampleEFB(subBlockUL+float2(5,0));
float4 sample6 = SampleEFB(subBlockUL+float2(6,0));
float4 sample7 = SampleEFB(subBlockUL+float2(7,0));
float4 sample8 = SampleEFB(subBlockUL+float2(0,1));
float4 sample9 = SampleEFB(subBlockUL+float2(1,1));
float4 sampleA = SampleEFB(subBlockUL+float2(2,1));
float4 sampleB = SampleEFB(subBlockUL+float2(3,1));
float4 sampleC = SampleEFB(subBlockUL+float2(4,1));
float4 sampleD = SampleEFB(subBlockUL+float2(5,1));
float4 sampleE = SampleEFB(subBlockUL+float2(6,1));
float4 sampleF = SampleEFB(subBlockUL+float2(7,1));

uint4 dw4 = UINT4_8888_BE(
	255*float4(sample0.b, sample4.b, sample8.b, sampleC.b),
	255*float4(sample1.b, sample5.b, sample9.b, sampleD.b),
	255*float4(sample2.b, sample6.b, sampleA.b, sampleE.b),
	255*float4(sample3.b, sample7.b, sampleB.b, sampleF.b)
	);

return dw4;
}

uint4 Generate_B(float2 cacheCoord) // G8 R8
{
float2 blockCoord = floor(cacheCoord / float2(2,1));

float2 blockUL = blockCoord * float2(4,4);
float2 subBlockUL = blockUL + float2(0, 2*(cacheCoord.x%2));

float4 sample0 = SampleEFB(subBlockUL+float2(0,0));
float4 sample1 = SampleEFB(subBlockUL+float2(1,0));
float4 sample2 = SampleEFB(subBlockUL+float2(2,0));
float4 sample3 = SampleEFB(subBlockUL+float2(3,0));
float4 sample4 = SampleEFB(subBlockUL+float2(0,1));
float4 sample5 = SampleEFB(subBlockUL+float2(1,1));
float4 sample6 = SampleEFB(subBlockUL+float2(2,1));
float4 sample7 = SampleEFB(subBlockUL+float2(3,1));

uint4 dw4 = UINT4_8888_BE(
	255*float4(sample0.g, sample2.g, sample4.g, sample6.g),
	255*float4(sample0.r, sample2.r, sample4.r, sample6.r),
	255*float4(sample1.g, sample3.g, sample5.g, sample7.g),
	255*float4(sample1.r, sample3.r, sample5.r, sample7.r)
	);

return dw4;
}

// FIXME: Untested
uint4 Generate_C(float2 cacheCoord) // B8 G8
{
float2 blockCoord = floor(cacheCoord / float2(2,1));

float2 blockUL = blockCoord * float2(4,4);
float2 subBlockUL = blockUL + float2(0, 2*(cacheCoord.x%2));

float4 sample0 = SampleEFB(subBlockUL+float2(0,0));
float4 sample1 = SampleEFB(subBlockUL+float2(1,0));
float4 sample2 = SampleEFB(subBlockUL+float2(2,0));
float4 sample3 = SampleEFB(subBlockUL+float2(3,0));
float4 sample4 = SampleEFB(subBlockUL+float2(0,1));
float4 sample5 = SampleEFB(subBlockUL+float2(1,1));
float4 sample6 = SampleEFB(subBlockUL+float2(2,1));
float4 sample7 = SampleEFB(subBlockUL+float2(3,1));

uint4 dw4 = UINT4_8888_BE(
	255*float4(sample0.b, sample2.b, sample4.b, sample6.b),
	255*float4(sample0.g, sample2.g, sample4.g, sample6.g),
	255*float4(sample1.b, sample3.b, sample5.b, sample7.b),
	255*float4(sample1.g, sample3.g, sample5.g, sample7.g)
	);

return dw4;
}

#ifdef DYNAMIC_MODE
interface iGenerator
{
uint4 Generate(float2 cacheCoord);
};

class cGenerator_4 : iGenerator
{
uint4 Generate(float2 cacheCoord)
{ return Generate_4(cacheCoord); }
};

class cGenerator_5 : iGenerator
{
uint4 Generate(float2 cacheCoord)
{ return Generate_5(cacheCoord); }
};

class cGenerator_6 : iGenerator
{
uint4 Generate(float2 cacheCoord)
{ return Generate_6(cacheCoord); }
};

class cGenerator_8 : iGenerator
{
uint4 Generate(float2 cacheCoord)
{ return Generate_8(cacheCoord); }
};

class cGenerator_B : iGenerator
{
uint4 Generate(float2 cacheCoord)
{ return Generate_B(cacheCoord); }
};

// Declare generator interface; must be set by application
iGenerator g_generator;
#define IMP_GENERATOR g_generator.Generate

#endif

#ifndef IMP_GENERATOR
#error No generator specified
#endif

#ifndef SHADER_MODEL
#error Missing shader model version
#endif

#if SHADER_MODEL >= 5
RWBuffer<uint> outBuf :register(u0);
#else
RWByteAddressBuffer outBuf :register(u0);
#endif
[numthreads(8,8,1)]
void main(in uint3 groupIdx : SV_GroupID, in uint3 subgroup : SV_GroupThreadID)
{
int2 cacheCoord = groupIdx.xy * 8 + subgroup.xy;
if (cacheCoord.x < Params.NumHalfCacheLinesX && cacheCoord.y < Params.NumBlocksY) {
	uint4 ocol0 = IMP_GENERATOR(cacheCoord);

	uint idx = 4 * (Params.NumHalfCacheLinesX*cacheCoord.y + cacheCoord.x);
#if SHADER_MODEL >= 5
	outBuf[idx+0] = ocol0.x;
	outBuf[idx+1] = ocol0.y;
	outBuf[idx+2] = ocol0.z;
	outBuf[idx+3] = ocol0.w;
#else
	idx *= 4;
	outBuf.Store4( idx+4*0, ocol0.x);
	outBuf.Store4( idx+4*1, ocol0.y);
	outBuf.Store4( idx+4*2, ocol0.z);
	outBuf.Store4( idx+4*3, ocol0.w);
#endif
}
}
)HLSL";

void CSTextureEncoder::Init()
{
	m_ready = false;

	HRESULT hr;

	// Create output buffer
	bool bSupportsShaderModel5 = DX11::D3D::GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0;
	auto outBd = CD3D11_BUFFER_DESC((4 * 4)*EFB_WIDTH*EFB_HEIGHT / 4, D3D11_BIND_UNORDERED_ACCESS);
	if (!bSupportsShaderModel5)
	{
		outBd.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	}
	hr = D3D::device->CreateBuffer(&outBd, nullptr, ToAddr(m_out));
	CHECK(SUCCEEDED(hr), "create efb encode output buffer");
	D3D::SetDebugObjectName(m_out.get(), "efb encoder output buffer");

	// And the cpu side version

	outBd.Usage = D3D11_USAGE_STAGING;
	outBd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	outBd.BindFlags = 0;
	outBd.MiscFlags = 0;
	hr = D3D::device->CreateBuffer(&outBd, nullptr, ToAddr(m_outStage));
	CHECK(SUCCEEDED(hr), "create efb encode staging buffer");
	D3D::SetDebugObjectName(m_outStage.get(), "efb encoder staging buffer");

	// UAV to write the shader result

	auto outUavDesc = CD3D11_UNORDERED_ACCESS_VIEW_DESC(m_out.get(), DXGI_FORMAT_R32_UINT, 0, (4)*EFB_WIDTH*EFB_HEIGHT / 4);
	if (!bSupportsShaderModel5)
	{
		outUavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		outUavDesc.Buffer.Flags |= D3D11_BUFFER_UAV_FLAG_RAW;
	}
	hr = D3D::device->CreateUnorderedAccessView(m_out.get(), &outUavDesc, ToAddr(m_outUav));

	D3D11_BUFFER_DESC bd = CD3D11_BUFFER_DESC(sizeof(EFBEncodeParams),
		D3D11_BIND_CONSTANT_BUFFER);
	hr = D3D::device->CreateBuffer(&bd, nullptr, ToAddr(m_encodeParams));
	CHECK(SUCCEEDED(hr), "create efb encode params buffer");
	D3D::SetDebugObjectName(m_encodeParams.get(), "efb encoder params buffer");

	// Create compute shader

#ifdef USE_DYNAMIC_MODE
	if (!InitDynamicMode())
#else
	if (!InitStaticMode())
#endif
		return;

	// Create efb texture sampler

	D3D11_SAMPLER_DESC sd = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	hr = D3D::device->CreateSamplerState(&sd, ToAddr(m_efbSampler));
	CHECK(SUCCEEDED(hr), "create efb encode texture sampler");
	D3D::SetDebugObjectName(m_efbSampler.get(), "efb encoder texture sampler");

	m_ready = true;

	// Warm up with shader cache
	char cache_filename[MAX_PATH];
	sprintf(cache_filename, "%sdx11-ENCODER-cs.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str());
	m_shaderCache.OpenAndRead(cache_filename, ShaderCacheInserter(*this));
}

void CSTextureEncoder::Shutdown()
{
	m_ready = false;

	for (auto &e : m_fetchClass)
		e.reset();
	for (auto &e : m_scaledFetchClass)
		e.reset();
	for (auto &e : m_intensityClass)
		e.reset();
	for (auto &e : m_generatorClass)
		e.reset();

	m_linkageArray.clear();

	m_classLinkage.reset();
	m_dynamicShader.reset();

	m_staticShaders.clear();

	m_efbSampler.reset();
	m_out.reset();
	m_outUav.reset();
	m_outStage.reset();
	m_shaderCache.Close();
}

void CSTextureEncoder::Encode(u8* dest_ptr, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
	bool is_depth_copy, bool bIsIntensityFmt, bool bScaleByHalf, const EFBRectangle& source)
{
	if (!m_ready) // Make sure we initialized OK
		return;

	HRESULT hr;
	u32 cacheLinesPerRow = bytes_per_row / 32;
	// Reset API

	g_renderer->ResetAPIState();

	// Set up all the state for EFB encoding

#ifdef USE_DYNAMIC_MODE
	if (SetDynamicShader(format, is_depth_copy, bIsIntensityFmt, bScaleByHalf))
#else
	if (SetStaticShader(format, is_depth_copy, bIsIntensityFmt, bScaleByHalf))
#endif
	{
		D3D::context->OMSetRenderTargets(0, nullptr, nullptr);

		EFBRectangle fullSrcRect;
		fullSrcRect.left = 0;
		fullSrcRect.top = 0;
		fullSrcRect.right = EFB_WIDTH;
		fullSrcRect.bottom = EFB_HEIGHT;
		TargetRectangle targetRect = g_renderer->ConvertEFBRectangle(fullSrcRect);

		EFBEncodeParams params = { 0 };
		params.NumHalfCacheLinesX = FLOAT(cacheLinesPerRow * 2);
		params.NumBlocksY = FLOAT(num_blocks_y);
		params.PosX = FLOAT(source.left);
		params.PosY = FLOAT(source.top);
		params.TexLeft = float(targetRect.left) / g_renderer->GetTargetWidth();
		params.TexTop = float(targetRect.top) / g_renderer->GetTargetHeight();
		params.TexRight = float(targetRect.right) / g_renderer->GetTargetWidth();
		params.TexBottom = float(targetRect.bottom) / g_renderer->GetTargetHeight();
		D3D::context->UpdateSubresource(m_encodeParams.get(), 0, nullptr, &params, 0, 0);

		D3D::context->CSSetConstantBuffers(0, 1, D3D::ToAddr(m_encodeParams));

		D3D::context->CSSetUnorderedAccessViews(0, 1, D3D::ToAddr(m_outUav), nullptr);

		ID3D11ShaderResourceView* pEFB = is_depth_copy ?
			FramebufferManager::GetResolvedEFBDepthTexture()->GetSRV() :
			// FIXME: Instead of resolving EFB, it would be better to pick out a
			// single sample from each pixel. The game may break if it isn't
			// expecting the blurred edges around multisampled shapes.
			FramebufferManager::GetResolvedEFBColorTexture()->GetSRV();

		D3D::context->CSSetShaderResources(0, 1, &pEFB);

		D3D::context->CSSetSamplers(0, 1, D3D::ToAddr(m_efbSampler));

		// Encode!

		D3D::context->Dispatch((cacheLinesPerRow * 2 + 7) / 8, (num_blocks_y + 7) / 8, 1);

		D3D11_BOX srcBox = CD3D11_BOX(0, 0, 0, (4 * 4)*cacheLinesPerRow * 2 * num_blocks_y, 1, 1);
		D3D::context->CopySubresourceRegion(m_outStage.get(), 0, 0, 0, 0, m_out.get(), 0, &srcBox);

		//
		// Clean up state
		IUnknown* nullDummy = nullptr;
		D3D::context->CSSetUnorderedAccessViews(0, 1, (ID3D11UnorderedAccessView**)&nullDummy, nullptr);
		D3D::context->CSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&nullDummy);

		// Transfer staging buffer to GameCube/Wii RAM
		// nVidia is unable to sync properly with a blocking Map
		// That workaround works and NES games do not flick as hell anymore
		D3D::context->Flush();
		D3D11_MAPPED_SUBRESOURCE map = { 0 };
		while ((hr = D3D::context->Map(m_outStage.get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &map)) != S_OK && hr == DXGI_ERROR_WAS_STILL_DRAWING)
		{
			Common::YieldCPU();
		}

		CHECK(SUCCEEDED(hr), "map staging buffer (0x%x)", hr);
		if (hr == S_OK)
		{
			u8* src = (u8*)map.pData;
			u32 readStride = std::min(bytes_per_row, map.RowPitch);
			for (u32 y = 0; y < num_blocks_y; ++y)
			{
				memcpy(dest_ptr, src, readStride);
				dest_ptr += memory_stride;
				src += readStride;
			}
			D3D::context->Unmap(m_outStage.get(), 0);
		}
	}

	// Restore API
	g_renderer->RestoreAPIState();
	D3D::context->OMSetRenderTargets(1,
		&FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());
}

bool CSTextureEncoder::InitStaticMode()
{
	// Nothing to really do.
	return true;
}

static const char* FETCH_FUNC_NAMES[4] = {
	"Fetch_0", "Fetch_1"
};

static const char* SCALEDFETCH_FUNC_NAMES[2] = {
	"ScaledFetch_0", "ScaledFetch_1"
};

static const char* INTENSITY_FUNC_NAMES[2] = {
	"Intensity_0", "Intensity_1"
};

bool CSTextureEncoder::SetStaticShader(u32 dstFormat, bool is_depth_copy,
	bool isIntensity, bool scaleByHalf)
{
	size_t scaledFetchNum = scaleByHalf ? 1 : 0;
	size_t intensityNum = isIntensity ? 1 : 0;
	size_t generatorNum = dstFormat & 0xF;

	ComboKey key = MakeComboKey(dstFormat, is_depth_copy, isIntensity, scaleByHalf, DX11::D3D::GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0);

	ComboMap::iterator it = m_staticShaders.find(key);

	ID3D11ComputeShader* shader{};
	if (it != m_staticShaders.end())
	{
		shader = it->second.get();
	}
	else
	{
		const char* generatorFuncName = nullptr;
		switch (generatorNum)
		{
		case 0x0: generatorFuncName = "Generate_0"; break;
		case 0x1: generatorFuncName = "Generate_1"; break;
		case 0x2: generatorFuncName = "Generate_2"; break;
		case 0x3: generatorFuncName = "Generate_3"; break;
		case 0x4: generatorFuncName = "Generate_4"; break;
		case 0x5: generatorFuncName = "Generate_5"; break;
		case 0x6: generatorFuncName = "Generate_6"; break;
		case 0x7: generatorFuncName = "Generate_7"; break;
		case 0x8: generatorFuncName = "Generate_8"; break;
		case 0x9: generatorFuncName = "Generate_9"; break;
		case 0xA: generatorFuncName = "Generate_A"; break;
		case 0xB: generatorFuncName = "Generate_B"; break;
		case 0xC: generatorFuncName = "Generate_C"; break;
		default:
			WARN_LOG(VIDEO, "No generator available for dst format 0x%X; aborting", generatorNum);
			m_staticShaders[key] = nullptr;
			return false;
		}

		INFO_LOG(VIDEO, "Compiling efb encoding shader for dstFormat 0x%X, is_depth_copy %d, isIntensity %d, scaleByHalf %d",
			dstFormat, is_depth_copy, isIntensity ? 1 : 0, scaleByHalf ? 1 : 0);

		// Shader permutation not found, so compile it
		D3DBlob bytecode;
		D3D_SHADER_MACRO macros[] = {
				{ "IMP_FETCH", FETCH_FUNC_NAMES[is_depth_copy] },
				{ "IMP_SCALEDFETCH", SCALEDFETCH_FUNC_NAMES[scaledFetchNum] },
				{ "IMP_INTENSITY", INTENSITY_FUNC_NAMES[intensityNum] },
				{ "IMP_GENERATOR", generatorFuncName },
				{ "SHADER_MODEL", DX11::D3D::GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0 ? "5" : "4" },
				{ nullptr, nullptr }
		};
		if (!D3D::CompileShader(D3D::ShaderType::Compute, EFB_ENCODE_CS, bytecode, macros))
		{
			WARN_LOG(VIDEO, "EFB encoder shader for dstFormat 0x%X, is_depth_copy %d, isIntensity %d, scaleByHalf %d failed to compile",
				dstFormat, is_depth_copy, isIntensity ? 1 : 0, scaleByHalf ? 1 : 0);
			// Add dummy shader to map to prevent trying to compile over and
			// over again
			m_staticShaders[key] = nullptr;
			return false;
		}

		m_shaderCache.Append(key, bytecode.Data(), (u32)bytecode.Size());
		shader = InsertShader(key, bytecode.Data(), (u32)bytecode.Size());
	}

	if (shader)
	{
		D3D::context->CSSetShader(shader, nullptr, 0);
		return true;
	}
	return false;
}

ID3D11ComputeShader* CSTextureEncoder::InsertShader(ComboKey const &key, u8 const *data, u32 sz)
{
	ID3D11ComputeShader* newShader;
	HRESULT hr = D3D::device->CreateComputeShader(data, sz, nullptr, &newShader);
	CHECK(SUCCEEDED(hr), "create efb encoder pixel shader");

	m_staticShaders.emplace(key, D3D::UniquePtr<ID3D11ComputeShader>(newShader));
	return newShader;
}

bool CSTextureEncoder::InitDynamicMode()
{

	HRESULT hr;

	D3D_SHADER_MACRO macros[] = {
			{ "DYNAMIC_MODE", nullptr },
			{ nullptr, nullptr }
	};

	D3DBlob bytecode;
	if (!D3D::CompileShader(D3D::ShaderType::Compute, EFB_ENCODE_CS, bytecode, macros))
	{
		ERROR_LOG(VIDEO, "EFB encode pixel shader failed to compile");
		return false;
	}

	hr = D3D::device->CreateClassLinkage(ToAddr(m_classLinkage));
	CHECK(SUCCEEDED(hr), "create efb encode class linkage");
	D3D::SetDebugObjectName(m_classLinkage.get(), "efb encoder class linkage");

	hr = D3D::device->CreateComputeShader(bytecode.Data(), bytecode.Size(), m_classLinkage.get(), ToAddr(m_dynamicShader));
	CHECK(SUCCEEDED(hr), "create efb encode pixel shader");
	D3D::SetDebugObjectName(m_dynamicShader.get(), "efb encoder pixel shader");

	// Use D3DReflect

	D3D::UniquePtr<ID3D11ShaderReflection> reflect;
	hr = HLSLCompiler::getInstance().Reflect(bytecode.Data(), bytecode.Size(), IID_ID3D11ShaderReflection, ToAddr(reflect));
	CHECK(SUCCEEDED(hr), "reflect on efb encoder shader");

	// Get number of slots and create dynamic linkage array

	UINT numSlots = reflect->GetNumInterfaceSlots();
	m_linkageArray.resize(numSlots);

	// Get interface slots

	ID3D11ShaderReflectionVariable* var = reflect->GetVariableByName("g_fetch");
	m_fetchSlot = var->GetInterfaceSlot(0);

	var = reflect->GetVariableByName("g_scaledFetch");
	m_scaledFetchSlot = var->GetInterfaceSlot(0);

	var = reflect->GetVariableByName("g_intensity");
	m_intensitySlot = var->GetInterfaceSlot(0);

	var = reflect->GetVariableByName("g_generator");
	m_generatorSlot = var->GetInterfaceSlot(0);

	INFO_LOG(VIDEO, "Fetch slot %d, scaledFetch slot %d, intensity slot %d, generator slot %d",
		m_fetchSlot, m_scaledFetchSlot, m_intensitySlot, m_generatorSlot);

	// Class instances will be created at the time they are used

	for (auto &e : m_fetchClass)
		e.reset();
	for (auto &e : m_scaledFetchClass)
		e.reset();
	for (auto &e : m_intensityClass)
		e.reset();
	for (auto &e : m_generatorClass)
		e.reset();

	return true;
}

static const char* FETCH_CLASS_NAMES[2] = {
	"cFetch_0", "cFetch_1"
};

static const char* SCALEDFETCH_CLASS_NAMES[2] = {
	"cScaledFetch_0", "cScaledFetch_1"
};

static const char* INTENSITY_CLASS_NAMES[2] = {
	"cIntensity_0", "cIntensity_1"
};

bool CSTextureEncoder::SetDynamicShader(u32 dstFormat,
	bool is_depth_copy, bool isIntensity, bool scaleByHalf)
{

	size_t scaledFetchNum = scaleByHalf ? 1 : 0;
	size_t intensityNum = isIntensity ? 1 : 0;
	size_t generatorNum = dstFormat & 0xF;

	// FIXME: Not all the possible generators are available as classes yet.
	// When dynamic mode is usable, implement them.
	const char* generatorName = nullptr;
	switch (generatorNum)
	{
	case 0x4: generatorName = "cGenerator_4"; break;
	case 0x5: generatorName = "cGenerator_5"; break;
	case 0x6: generatorName = "cGenerator_6"; break;
	case 0x8: generatorName = "cGenerator_8"; break;
	case 0xB: generatorName = "cGenerator_B"; break;
	default:
		WARN_LOG(VIDEO, "No generator available for dst format 0x%X; aborting", generatorNum);
		return false;
	}

	// Make sure class instances are available
	if (!m_fetchClass[is_depth_copy])
	{
		INFO_LOG(VIDEO, "Creating %s class instance for encoder 0x%X",
			FETCH_CLASS_NAMES[is_depth_copy], dstFormat);
		HRESULT hr = m_classLinkage->CreateClassInstance(
			FETCH_CLASS_NAMES[is_depth_copy], 0, 0, 0, 0, ToAddr(m_fetchClass[is_depth_copy]));
		CHECK(SUCCEEDED(hr), "create fetch class instance");
	}
	if (!m_scaledFetchClass[scaledFetchNum])
	{
		INFO_LOG(VIDEO, "Creating %s class instance for encoder 0x%X",
			SCALEDFETCH_CLASS_NAMES[scaledFetchNum], dstFormat);
		HRESULT hr = m_classLinkage->CreateClassInstance(
			SCALEDFETCH_CLASS_NAMES[scaledFetchNum], 0, 0, 0, 0,
			ToAddr(m_scaledFetchClass[scaledFetchNum]));
		CHECK(SUCCEEDED(hr), "create scaled fetch class instance");
	}
	if (!m_intensityClass[intensityNum])
	{
		INFO_LOG(VIDEO, "Creating %s class instance for encoder 0x%X",
			INTENSITY_CLASS_NAMES[intensityNum], dstFormat);
		HRESULT hr = m_classLinkage->CreateClassInstance(
			INTENSITY_CLASS_NAMES[intensityNum], 0, 0, 0, 0,
			ToAddr(m_intensityClass[intensityNum]));
		CHECK(SUCCEEDED(hr), "create intensity class instance");
	}
	if (!m_generatorClass[generatorNum])
	{
		INFO_LOG(VIDEO, "Creating %s class instance for encoder 0x%X",
			generatorName, dstFormat);
		HRESULT hr = m_classLinkage->CreateClassInstance(
			generatorName, 0, 0, 0, 0, ToAddr(m_generatorClass[generatorNum]));
		CHECK(SUCCEEDED(hr), "create generator class instance");
	}

	// Assemble dynamic linkage array
	if (m_fetchSlot != UINT(-1))
		m_linkageArray[m_fetchSlot] = m_fetchClass[is_depth_copy].get();
	if (m_scaledFetchSlot != UINT(-1))
		m_linkageArray[m_scaledFetchSlot] = m_scaledFetchClass[scaledFetchNum].get();
	if (m_intensitySlot != UINT(-1))
		m_linkageArray[m_intensitySlot] = m_intensityClass[intensityNum].get();
	if (m_generatorSlot != UINT(-1))
		m_linkageArray[m_generatorSlot] = m_generatorClass[generatorNum].get();

	D3D::context->CSSetShader(m_dynamicShader.get(),
		m_linkageArray.empty() ? nullptr : m_linkageArray.data(),
		(UINT)m_linkageArray.size());

	return true;
}

}
