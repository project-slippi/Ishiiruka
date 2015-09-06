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
#include "VideoBackends/DX11/PSTextureEncoder.h"
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

static const char EFB_ENCODE_VS[] = R"HLSL(
// dolphin-emu EFB encoder vertex shader

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

struct Output
{
	float4 Pos : SV_Position;
	float2 Coord : ENCODECOORD;
};

Output main(in float2 Pos : POSITION)
{
	Output result;
	result.Pos = float4(2*Pos.x-1, -2*Pos.y+1, 0.0, 1.0);
	result.Coord = Pos * float2(Params.NumHalfCacheLinesX, Params.NumBlocksY);
	return result;
}
)HLSL";

static const char EFB_ENCODE_PS[] = R"HLSL(
// dolphin-emu EFB encoder pixel shader

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
float4 result = EFBTexture.Sample(EFBSampler, texCoord);
result.a = 1.0;
return result;
}

float4 Fetch_1(float2 coord)
{
float3 texCoord = float3(CalcTexCoord(coord), 0.0);
return EFBTexture.Sample(EFBSampler, texCoord);
}

float4 Fetch_2(float2 coord)
{
float3 texCoord = float3(CalcTexCoord(coord), 0.0);
float4 result = EFBTexture.Sample(EFBSampler, texCoord);
result.a = 1.0;
return result;
}

float4 Fetch_3(float2 coord)
{
float3 texCoord = float3(CalcTexCoord(coord), 0.0);

uint depth24 = 0xFFFFFF - 0xFFFFFF * EFBTexture.Sample(EFBSampler, texCoord).r;
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

// Source format 2
class cFetch_2 : iFetch
{
float4 Fetch(float2 coord)
{ return Fetch_2(coord); }
};

// Source format 3
class cFetch_3 : iFetch
{
float4 Fetch(float2 coord)
{ return Fetch_3(coord); }
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

void main(out uint4 ocol0 : SV_Target, in float4 Pos : SV_Position, in float2 fCacheCoord : ENCODECOORD)
{
float2 cacheCoord = floor(fCacheCoord);
ocol0 = IMP_GENERATOR(cacheCoord);
}
)HLSL";
static const D3D11_INPUT_ELEMENT_DESC QUAD_LAYOUT_DESC[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const struct QuadVertex
{
	float posX;
	float posY;
} QUAD_VERTS[4] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };

void PSTextureEncoder::Init()
{
	m_ready = false;

	HRESULT hr;

	// Create output texture RGBA format

	// This format allows us to generate one cache line in two pixels.
	D3D11_TEXTURE2D_DESC t2dd = CD3D11_TEXTURE2D_DESC(
		DXGI_FORMAT_R32G32B32A32_UINT,
		EFB_WIDTH, EFB_HEIGHT / 4, 1, 1, D3D11_BIND_RENDER_TARGET);
	hr = D3D::device->CreateTexture2D(&t2dd, nullptr, D3D::ToAddr(m_out));
	CHECK(SUCCEEDED(hr), "create efb encode output texture");
	D3D::SetDebugObjectName(m_out.get(), "efb encoder output texture");

	// Create output render target view

	D3D11_RENDER_TARGET_VIEW_DESC rtvd = CD3D11_RENDER_TARGET_VIEW_DESC(m_out.get(),
		D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32G32B32A32_UINT);
	hr = D3D::device->CreateRenderTargetView(m_out.get(), &rtvd, D3D::ToAddr(m_outRTV));
	CHECK(SUCCEEDED(hr), "create efb encode output render target view");
	D3D::SetDebugObjectName(m_outRTV.get(), "efb encoder output rtv");

	// Create output staging buffer

	t2dd.Usage = D3D11_USAGE_STAGING;
	t2dd.BindFlags = 0;
	t2dd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	hr = D3D::device->CreateTexture2D(&t2dd, nullptr, D3D::ToAddr(m_outStage));
	CHECK(SUCCEEDED(hr), "create efb encode output staging buffer");
	D3D::SetDebugObjectName(m_outStage.get(), "efb encoder output staging buffer");

	// Create constant buffer for uploading data to shaders

	D3D11_BUFFER_DESC bd = CD3D11_BUFFER_DESC(sizeof(EFBEncodeParams),
		D3D11_BIND_CONSTANT_BUFFER);
	hr = D3D::device->CreateBuffer(&bd, nullptr, D3D::ToAddr(m_encodeParams));
	CHECK(SUCCEEDED(hr), "create efb encode params buffer");
	D3D::SetDebugObjectName(m_encodeParams.get(), "efb encoder params buffer");

	// Create vertex quad

	bd = CD3D11_BUFFER_DESC(sizeof(QUAD_VERTS), D3D11_BIND_VERTEX_BUFFER,
		D3D11_USAGE_IMMUTABLE);
	D3D11_SUBRESOURCE_DATA srd = { QUAD_VERTS, 0, 0 };

	hr = D3D::device->CreateBuffer(&bd, &srd, D3D::ToAddr(m_quad));
	CHECK(SUCCEEDED(hr), "create efb encode quad vertex buffer");
	D3D::SetDebugObjectName(m_quad.get(), "efb encoder quad vertex buffer");

	// Create vertex shader

	D3DBlob bytecode;
	if (!D3D::CompileShader(D3D::ShaderType::Vertex, EFB_ENCODE_VS, bytecode))
	{
		ERROR_LOG(VIDEO, "EFB encode vertex shader failed to compile");
		return;
	}

	hr = D3D::device->CreateVertexShader(bytecode.Data(), bytecode.Size(), nullptr, D3D::ToAddr(m_vShader));
	CHECK(SUCCEEDED(hr), "create efb encode vertex shader");
	D3D::SetDebugObjectName(m_vShader.get(), "efb encoder vertex shader");

	// Create input layout for vertex quad using bytecode from vertex shader

	hr = D3D::device->CreateInputLayout(QUAD_LAYOUT_DESC,
		sizeof(QUAD_LAYOUT_DESC) / sizeof(D3D11_INPUT_ELEMENT_DESC),
		bytecode.Data(), bytecode.Size(), D3D::ToAddr(m_quadLayout));
	CHECK(SUCCEEDED(hr), "create efb encode quad vertex layout");
	D3D::SetDebugObjectName(m_quadLayout.get(), "efb encoder quad layout");

	// Create pixel shader

#ifdef USE_DYNAMIC_MODE
	if (!InitDynamicMode())
#else
	if (!InitStaticMode())
#endif
		return;

	// Create blend state

	D3D11_BLEND_DESC bld = CD3D11_BLEND_DESC(CD3D11_DEFAULT());
	hr = D3D::device->CreateBlendState(&bld, D3D::ToAddr(m_efbEncodeBlendState));
	CHECK(SUCCEEDED(hr), "create efb encode blend state");
	D3D::SetDebugObjectName(m_efbEncodeBlendState.get(), "efb encoder blend state");

	// Create depth state

	D3D11_DEPTH_STENCIL_DESC dsd = CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT());
	dsd.DepthEnable = FALSE;
	hr = D3D::device->CreateDepthStencilState(&dsd, D3D::ToAddr(m_efbEncodeDepthState));
	CHECK(SUCCEEDED(hr), "create efb encode depth state");
	D3D::SetDebugObjectName(m_efbEncodeDepthState.get(), "efb encoder depth state");

	// Create rasterizer state

	D3D11_RASTERIZER_DESC rd = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
	rd.CullMode = D3D11_CULL_NONE;
	rd.DepthClipEnable = FALSE;
	hr = D3D::device->CreateRasterizerState(&rd, D3D::ToAddr(m_efbEncodeRastState));
	CHECK(SUCCEEDED(hr), "create efb encode rast state");
	D3D::SetDebugObjectName(m_efbEncodeRastState.get(), "efb encoder rast state");

	// Create efb texture sampler

	D3D11_SAMPLER_DESC sd = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	hr = D3D::device->CreateSamplerState(&sd, D3D::ToAddr(m_efbSampler));
	CHECK(SUCCEEDED(hr), "create efb encode texture sampler");
	D3D::SetDebugObjectName(m_efbSampler.get(), "efb encoder texture sampler");

	m_ready = true;
}

void PSTextureEncoder::Shutdown()
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
	m_efbEncodeRastState.reset();
	m_efbEncodeDepthState.reset();
	m_efbEncodeBlendState.reset();
	m_quadLayout.reset();
	m_vShader.reset();
	m_quad.reset();
	m_encodeParams.reset();
	m_outStage.reset();
	m_outRTV.reset();
	m_out.reset();
}

void PSTextureEncoder::Encode(u8* dst, const TextureCache::TCacheEntryBase *texture_entry,
	PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
	bool isIntensity, bool scaleByHalf)
{
	if (!m_ready) // Make sure we initialized OK
		return;

	HRESULT hr;
	u32 numBlocksY = texture_entry->NumBlocksY();
	u32 cacheLinesPerRow = texture_entry->CacheLinesPerRow();
	if ((texture_entry->format & 0xF) == 0x6 && texture_entry->format != GX_TF_RGBA8)
		cacheLinesPerRow *= 2;
	// Reset API
	g_renderer->ResetAPIState();

	// Set up all the state for EFB encoding

#ifdef USE_DYNAMIC_MODE
	if (SetDynamicShader(texture_entry->format, srcFormat, isIntensity, scaleByHalf))
#else
	if (SetStaticShader(texture_entry->format, srcFormat, isIntensity, scaleByHalf))
#endif
	{
		D3D::stateman->SetVertexShader(m_vShader.get());
		D3D::stateman->SetGeometryShader(nullptr);

		D3D::stateman->PushBlendState(m_efbEncodeBlendState.get());
		D3D::stateman->PushDepthState(m_efbEncodeDepthState.get());
		D3D::stateman->PushRasterizerState(m_efbEncodeRastState.get());
			
		D3D::context->OMSetRenderTargets(1, D3D::ToAddr(m_outRTV), nullptr);
		D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, FLOAT(cacheLinesPerRow * 2), FLOAT(numBlocksY));
		D3D::context->RSSetViewports(1, &vp);

		D3D::stateman->SetInputLayout(m_quadLayout.get());
		D3D::stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		UINT stride = sizeof(QuadVertex);
		UINT offset = 0;
		D3D::stateman->SetVertexBuffer(m_quad.get(), stride, offset);

		EFBRectangle fullSrcRect;
		fullSrcRect.left = 0;
		fullSrcRect.top = 0;
		fullSrcRect.right = EFB_WIDTH;
		fullSrcRect.bottom = EFB_HEIGHT;
		TargetRectangle targetRect = g_renderer->ConvertEFBRectangle(fullSrcRect);

		EFBEncodeParams params = { 0 };
		params.NumHalfCacheLinesX = FLOAT(cacheLinesPerRow * 2);
		params.NumBlocksY = FLOAT(numBlocksY);
		params.PosX = FLOAT(srcRect.left);
		params.PosY = FLOAT(srcRect.top);
		params.TexLeft = float(targetRect.left) / g_renderer->GetTargetWidth();
		params.TexTop = float(targetRect.top) / g_renderer->GetTargetHeight();
		params.TexRight = float(targetRect.right) / g_renderer->GetTargetWidth();
		params.TexBottom = float(targetRect.bottom) / g_renderer->GetTargetHeight();
		D3D::context->UpdateSubresource(m_encodeParams.get(), 0, nullptr, &params, 0, 0);

		D3D::stateman->SetVertexConstants(m_encodeParams.get());

		ID3D11ShaderResourceView* pEFB = (srcFormat == PEControl::Z24) ?
			FramebufferManager::GetEFBDepthTexture()->GetSRV() :
			// FIXME: Instead of resolving EFB, it would be better to pick out a
			// single sample from each pixel. The game may break if it isn't
			// expecting the blurred edges around multisampled shapes.
			FramebufferManager::GetResolvedEFBColorTexture()->GetSRV();

		D3D::stateman->SetPixelConstants(m_encodeParams.get());
		D3D::stateman->SetTexture(0, pEFB);
		D3D::stateman->SetSampler(0, m_efbSampler.get());

		// Encode!
		D3D::stateman->Apply();
		D3D::context->Draw(4, 0);

		// Copy to staging buffer

		D3D11_BOX srcBox = CD3D11_BOX(0, 0, 0, cacheLinesPerRow * 2, numBlocksY, 1);
		D3D::context->CopySubresourceRegion(m_outStage.get(), 0, 0, 0, 0, m_out.get(), 0, &srcBox);

		// Clean up state

		D3D::context->OMSetRenderTargets(0, nullptr, nullptr);

		D3D::stateman->SetSampler(0, nullptr);
		D3D::stateman->SetTexture(0, nullptr);
		D3D::stateman->SetPixelConstants(nullptr);
		D3D::stateman->SetVertexConstants(nullptr);

		D3D::stateman->SetPixelShader(nullptr);
		D3D::stateman->SetVertexBuffer(nullptr, 0, 0);

		D3D::stateman->PopRasterizerState();
		D3D::stateman->PopDepthState();
		D3D::stateman->PopBlendState();

		D3D::stateman->Apply();

		// Transfer staging buffer to GameCube/Wii RAM

		D3D11_MAPPED_SUBRESOURCE map = { 0 };
		hr = D3D::context->Map(m_outStage.get(), 0, D3D11_MAP_READ, 0, &map);
		CHECK(SUCCEEDED(hr), "map staging buffer");

		u8* src = (u8*)map.pData;
		for (u32 y = 0; y < numBlocksY; ++y)
		{
			memcpy(dst, src, cacheLinesPerRow * 32);
			dst += texture_entry->memory_stride * 32;
			src += map.RowPitch;
		}

		D3D::context->Unmap(m_outStage.get(), 0);
	}

	// Restore API

	g_renderer->RestoreAPIState();
	D3D::context->OMSetRenderTargets(1,
		&FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());
}

bool PSTextureEncoder::InitStaticMode()
{
	// Nothing to really do.
	return true;
}

static const char* FETCH_FUNC_NAMES[4] = {
	"Fetch_0", "Fetch_1", "Fetch_2", "Fetch_3"
};

static const char* SCALEDFETCH_FUNC_NAMES[2] = {
	"ScaledFetch_0", "ScaledFetch_1"
};

static const char* INTENSITY_FUNC_NAMES[2] = {
	"Intensity_0", "Intensity_1"
};

bool PSTextureEncoder::SetStaticShader(u32 dstFormat, u32 srcFormat,
	bool isIntensity, bool scaleByHalf)
{
	size_t fetchNum = srcFormat;
	size_t scaledFetchNum = scaleByHalf ? 1 : 0;
	size_t intensityNum = isIntensity ? 1 : 0;
	size_t generatorNum = dstFormat & 0xF;

	ComboKey key = MakeComboKey(dstFormat, srcFormat, isIntensity, scaleByHalf);

	ComboMap::iterator it = m_staticShaders.find(key);
	if (it == m_staticShaders.end())
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
			m_staticShaders.emplace(key, D3D::PixelShaderPtr(nullptr));
			return false;
		}

		INFO_LOG(VIDEO, "Compiling efb encoding shader for dstFormat 0x%X, srcFormat %d, isIntensity %d, scaleByHalf %d",
			dstFormat, srcFormat, isIntensity ? 1 : 0, scaleByHalf ? 1 : 0);

		// Shader permutation not found, so compile it
		D3DBlob bytecode;
		D3D_SHADER_MACRO macros[] = {
			{ "IMP_FETCH", FETCH_FUNC_NAMES[fetchNum] },
			{ "IMP_SCALEDFETCH", SCALEDFETCH_FUNC_NAMES[scaledFetchNum] },
			{ "IMP_INTENSITY", INTENSITY_FUNC_NAMES[intensityNum] },
			{ "IMP_GENERATOR", generatorFuncName },
			{ nullptr, nullptr }
		};
		if (!D3D::CompileShader(D3D::ShaderType::Pixel, EFB_ENCODE_PS, bytecode, macros))
		{
			WARN_LOG(VIDEO, "EFB encoder shader for dstFormat 0x%X, srcFormat %d, isIntensity %d, scaleByHalf %d failed to compile",
				dstFormat, srcFormat, isIntensity ? 1 : 0, scaleByHalf ? 1 : 0);
			// Add dummy shader to map to prevent trying to compile over and
			// over again
			m_staticShaders[key].reset(nullptr);
			return false;
		}

		D3D::PixelShaderPtr newShader;
		HRESULT hr = D3D::device->CreatePixelShader(bytecode.Data(), bytecode.Size(), nullptr, D3D::ToAddr(newShader));
		CHECK(SUCCEEDED(hr), "create efb encoder pixel shader");

		it = m_staticShaders.emplace(key, std::move(newShader)).first;
	}

	if (it != m_staticShaders.end())
	{
		if (it->second)
		{
			D3D::context->PSSetShader(it->second.get(), nullptr, 0);
			return true;
		}
		else
			return false;
	}
	else
		return false;
}

bool PSTextureEncoder::InitDynamicMode()
{
	HRESULT hr;

	D3D_SHADER_MACRO macros[] = {
		{ "DYNAMIC_MODE", nullptr },
		{ nullptr, nullptr }
	};

	D3DBlob bytecode;
	if (!D3D::CompileShader(D3D::ShaderType::Pixel, EFB_ENCODE_PS, bytecode, macros))
	{
		ERROR_LOG(VIDEO, "EFB encode pixel shader failed to compile");
		return false;
	}

	hr = D3D::device->CreateClassLinkage(D3D::ToAddr(m_classLinkage));
	CHECK(SUCCEEDED(hr), "create efb encode class linkage");
	D3D::SetDebugObjectName(m_classLinkage.get(), "efb encoder class linkage");

	hr = D3D::device->CreatePixelShader(bytecode.Data(), bytecode.Size(), m_classLinkage.get(), D3D::ToAddr(m_dynamicShader));
	CHECK(SUCCEEDED(hr), "create efb encode pixel shader");
	D3D::SetDebugObjectName(m_dynamicShader.get(), "efb encoder pixel shader");

	// Use D3DReflect

	ID3D11ShaderReflection* reflect = nullptr;
	hr = HLSLCompiler::getInstance().Reflect(bytecode.Data(), bytecode.Size(), IID_ID3D11ShaderReflection, (void**)&reflect);
	CHECK(SUCCEEDED(hr), "reflect on efb encoder shader");

	// Get number of slots and create dynamic linkage array

	UINT numSlots = reflect->GetNumInterfaceSlots();
	m_linkageArray.resize(numSlots, nullptr);

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

	for (size_t i = 0; i < 4; ++i)
		m_fetchClass[i] = nullptr;
	for (size_t i = 0; i < 2; ++i)
		m_scaledFetchClass[i] = nullptr;
	for (size_t i = 0; i < 2; ++i)
		m_intensityClass[i] = nullptr;
	for (size_t i = 0; i < 16; ++i)
		m_generatorClass[i] = nullptr;

	reflect->Release();
	return true;
}

static const char* FETCH_CLASS_NAMES[4] = {
	"cFetch_0", "cFetch_1", "cFetch_2", "cFetch_3"
};

static const char* SCALEDFETCH_CLASS_NAMES[2] = {
	"cScaledFetch_0", "cScaledFetch_1"
};

static const char* INTENSITY_CLASS_NAMES[2] = {
	"cIntensity_0", "cIntensity_1"
};

bool PSTextureEncoder::SetDynamicShader(u32 dstFormat,
	u32 srcFormat, bool isIntensity, bool scaleByHalf)
{
	size_t fetchNum = srcFormat;
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
	if (!m_fetchClass[fetchNum])
	{
		INFO_LOG(VIDEO, "Creating %s class instance for encoder 0x%X",
			FETCH_CLASS_NAMES[fetchNum], dstFormat);
		HRESULT hr = m_classLinkage->CreateClassInstance(
			FETCH_CLASS_NAMES[fetchNum], 0, 0, 0, 0, D3D::ToAddr(m_fetchClass[fetchNum]));
		CHECK(SUCCEEDED(hr), "create fetch class instance");
	}
	if (!m_scaledFetchClass[scaledFetchNum])
	{
		INFO_LOG(VIDEO, "Creating %s class instance for encoder 0x%X",
			SCALEDFETCH_CLASS_NAMES[scaledFetchNum], dstFormat);
		HRESULT hr = m_classLinkage->CreateClassInstance(
			SCALEDFETCH_CLASS_NAMES[scaledFetchNum], 0, 0, 0, 0,
			D3D::ToAddr(m_scaledFetchClass[scaledFetchNum]));
		CHECK(SUCCEEDED(hr), "create scaled fetch class instance");
	}
	if (!m_intensityClass[intensityNum])
	{
		INFO_LOG(VIDEO, "Creating %s class instance for encoder 0x%X",
			INTENSITY_CLASS_NAMES[intensityNum], dstFormat);
		HRESULT hr = m_classLinkage->CreateClassInstance(
			INTENSITY_CLASS_NAMES[intensityNum], 0, 0, 0, 0,
			D3D::ToAddr(m_intensityClass[intensityNum]));
		CHECK(SUCCEEDED(hr), "create intensity class instance");
	}
	if (!m_generatorClass[generatorNum])
	{
		INFO_LOG(VIDEO, "Creating %s class instance for encoder 0x%X",
			generatorName, dstFormat);
		HRESULT hr = m_classLinkage->CreateClassInstance(
			generatorName, 0, 0, 0, 0, D3D::ToAddr(m_generatorClass[generatorNum]));
		CHECK(SUCCEEDED(hr), "create generator class instance");
	}

	// Assemble dynamic linkage array
	if (m_fetchSlot != UINT(-1))
		m_linkageArray[m_fetchSlot] = m_fetchClass[fetchNum].get();
	if (m_scaledFetchSlot != UINT(-1))
		m_linkageArray[m_scaledFetchSlot] = m_scaledFetchClass[scaledFetchNum].get();
	if (m_intensitySlot != UINT(-1))
		m_linkageArray[m_intensitySlot] = m_intensityClass[intensityNum].get();
	if (m_generatorSlot != UINT(-1))
		m_linkageArray[m_generatorSlot] = m_generatorClass[generatorNum].get();

	D3D::stateman->SetPixelShaderDynamic(m_dynamicShader.get(),
		m_linkageArray.empty() ? nullptr : &m_linkageArray[0],
		(UINT)m_linkageArray.size());

	return true;
}

}
