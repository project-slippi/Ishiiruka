// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/FileUtil.h"
#include "Common/LinearDiskCache.h"

#include "Core/ConfigManager.h"
#include "Core/Host.h"
#include "VideoBackends/DX11/D3DBase.h"
#include "VideoBackends/DX11/D3DShader.h"
#include "VideoBackends/DX11/D3DUtil.h"
#include "VideoBackends/DX11/PixelShaderCache.h"
#include "VideoBackends/DX11/VertexShaderCache.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/HLSLCompiler.h"
#include "VideoCommon/PixelShaderManager.h"



namespace DX11
{

PixelShaderCache::PSCache* PixelShaderCache::s_pixel_shaders;
const PixelShaderCache::PSCacheEntry* PixelShaderCache::s_last_entry;
PixelShaderUid PixelShaderCache::s_last_uid;
PixelShaderUid PixelShaderCache::s_external_last_uid;
static HLSLAsyncCompiler *s_compiler;
static Common::SpinLock<true> s_pixel_shaders_lock;
static bool s_previous_per_pixel_lighting = false;
LinearDiskCache<PixelShaderUid, u8> g_ps_disk_cache;

D3D::PixelShaderPtr s_ColorMatrixProgram[2];
D3D::PixelShaderPtr s_ColorCopyProgram[3];
D3D::PixelShaderPtr s_DepthMatrixProgram[2];
D3D::PixelShaderPtr s_DepthResolveProgram;
D3D::PixelShaderPtr s_ClearProgram;
D3D::PixelShaderPtr s_rgba6_to_rgb8[2];
D3D::PixelShaderPtr s_rgb8_to_rgba6[2];

D3D::ConstantStreamBuffer* pscbuf;

const char* clear_program_code = R"hlsl(
	void main(
	out float4 ocol0 : SV_Target,
	in float4 pos : SV_Position,
	in float4 incol0 : COLOR0){
	ocol0 = incol0;
	}
)hlsl";

// TODO: Find some way to avoid having separate shaders for non-MSAA and MSAA...
const char* color_copy_program_code = R"hlsl(
sampler samp0 : register(s0);
Texture2DArray Tex0 : register(t0);
void main(
	out float4 ocol0 : SV_Target,
	in float4 pos : SV_Position,
	in float3 uv0 : TEXCOORD0,
	in float  uv1 : TEXCOORD1,
	in float4 uv2 : TEXCOORD2,
	in float4 uv3 : TEXCOORD3)
{
	ocol0 = pow(Tex0.Sample(samp0,uv0), uv1);
}
)hlsl";

const char* color_copy_program_code_ssaa = R"hlsl(
sampler samp0 : register(s0);
Texture2DArray Tex0 : register(t0);
void main(
	out float4 ocol0 : SV_Target,
	in float4 pos : SV_Position,
	in float3 uv0 : TEXCOORD0,
	in float  uv1 : TEXCOORD1,
	in float4 uv2 : TEXCOORD2,
	in float4 uv3 : TEXCOORD3)
{
	ocol0 =  Tex0.Sample(samp0,float3(uv2.xy, uv0.z));
	ocol0 += Tex0.Sample(samp0,float3(uv2.wz, uv0.z));
	ocol0 += Tex0.Sample(samp0,float3(uv3.xy, uv0.z));
	ocol0 += Tex0.Sample(samp0,float3(uv3.wz, uv0.z));
	ocol0 =  pow((ocol0 * 0.25), uv1);
}
)hlsl";

// TODO: Improve sampling algorithm!
const char* color_copy_program_code_msaa = R"hlsl(
sampler samp0 : register(s0);
Texture2DMSArray<float4, %d> Tex0 : register(t0);
void main(
	out float4 ocol0 : SV_Target,
	in float4 pos : SV_Position,
	in float3 uv0 : TEXCOORD0,
	in float  uv1 : TEXCOORD1,
	in float4 uv2 : TEXCOORD2,
	in float4 uv3 : TEXCOORD3)
{
	int width, height, slices, samples;
	Tex0.GetDimensions(width, height, slices, samples);
	ocol0 = 0;
	for(int i = 0; i < samples; ++i)
		ocol0 += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);
	ocol0 /= samples;
	ocol0 = pow(ocol0, uv1);
}
)hlsl";

const char* color_matrix_program_code = R"hlsl(
sampler samp0 : register(s0);
Texture2DArray Tex0 : register(t0);
uniform float4 cColMatrix[7] : register(c0);
void main( 
	out float4 ocol0 : SV_Target,
	in float4 pos : SV_Position,
	in float3 uv0 : TEXCOORD0,
	in float  uv1 : TEXCOORD1,
	in float4 uv2 : TEXCOORD2,
	in float4 uv3 : TEXCOORD3)
{
	float4 texcol = Tex0.Sample(samp0,uv0);
	texcol = floor(texcol * cColMatrix[5])*cColMatrix[6];
	ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];
}
)hlsl";

const char* color_matrix_program_code_msaa = R"hlsl(
sampler samp0 : register(s0);
Texture2DMSArray<float4, %d> Tex0 : register(t0);
uniform float4 cColMatrix[7] : register(c0);
void main( 
	out float4 ocol0 : SV_Target,
	in float4 pos : SV_Position,
	in float3 uv0 : TEXCOORD0,
	in float  uv1 : TEXCOORD1,
	in float4 uv2 : TEXCOORD2,
	in float4 uv3 : TEXCOORD3)
{
	int width, height, slices, samples;
	Tex0.GetDimensions(width, height, slices, samples);
	float4 texcol = 0;
	for(int i = 0; i < samples; ++i)
		texcol += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);
	texcol /= samples;
	texcol = floor(texcol * cColMatrix[5])*cColMatrix[6];
	ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];
}
)hlsl";

const char* depth_matrix_program_code = R"hlsl(
sampler samp0 : register(s0);
Texture2DArray Tex0 : register(t0);
uniform float4 cColMatrix[7] : register(c0);
void main(
	out float4 ocol0 : SV_Target,
	in float4 pos : SV_Position,
	in float3 uv0 : TEXCOORD0,
	in float  uv1 : TEXCOORD1,
	in float4 uv2 : TEXCOORD2,
	in float4 uv3 : TEXCOORD3)
{
	float4 texcol = Tex0.Sample(samp0,uv0);
	int depth = int((1.0 - texcol.x) * 16777216.0);
	texcol.z = float(depth & 255);   // z component
	depth = depth >> 8;
	texcol.y = float(depth & 255);   // y component
	depth = depth >> 8;
	texcol.x = float(depth & 255);   // x component
	texcol.w = float(depth & 240);    // w component
	texcol = texcol / 255.0;             // normalize components to [0.0..1.0]
	ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];
}
)hlsl";

const char* depth_matrix_program_msaa = R"hlsl(
sampler samp0 : register(s0);
Texture2DMSArray<float4, %d> Tex0 : register(t0);
uniform float4 cColMatrix[7] : register(c0);
void main(
	out float4 ocol0 : SV_Target,
	in float4 pos : SV_Position,
	in float3 uv0 : TEXCOORD0,
	in float  uv1 : TEXCOORD1,
	in float4 uv2 : TEXCOORD2,
	in float4 uv3 : TEXCOORD3)
{
	int width, height, slices, samples;
	Tex0.GetDimensions(width, height, slices, samples);
	float tdepth = 0.0f;
	for(int i = 0; i < samples; ++i)
		tdepth += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i).x;
	tdepth /= samples;
	float4 texcol = float4(tdepth,tdepth,tdepth,tdepth);
	int depth = int((1.0 - texcol.x) * 16777216.0);
	texcol.z = float(depth & 255);   // z component
	depth = depth >> 8;
	texcol.y = float(depth & 255);   // y component
	depth = depth >> 8;
	texcol.x = float(depth & 255);   // y component
	texcol.w = float(depth & 240);    // w component
	texcol = texcol / 255.0;             // normalize components to [0.0..1.0]
	ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];
}
)hlsl";

const char* reint_rgba6_to_rgb8_code = R"hlsl(
sampler samp0 : register(s0);
Texture2DArray Tex0 : register(t0);
void main(
	out float4 ocol0 : SV_Target,
	in float4 pos : SV_Position,
	in float3 uv0 : TEXCOORD0,
	in float  uv1 : TEXCOORD1,
	in float4 uv2 : TEXCOORD2,
	in float4 uv3 : TEXCOORD3)
{	
	int4 src6 = round(Tex0.Sample(samp0,uv0) * 63.f);
	int4 dst8;
	dst8.r = (src6.r << 2) | (src6.g >> 4);
	dst8.g = ((src6.g & 0xF) << 4) | (src6.b >> 2);
	dst8.b = ((src6.b & 0x3) << 6) | src6.a;
	dst8.a = 255;
	ocol0 = (float4)dst8 / 255.f;
}
)hlsl";

const char* reint_rgba6_to_rgb8_msaa = R"hlsl(
sampler samp0 : register(s0);
Texture2DMSArray<float4, %d> Tex0 : register(t0);
void main(
	out float4 ocol0 : SV_Target,
	in float4 pos : SV_Position,
	in float3 uv0 : TEXCOORD0,
	in float  uv1 : TEXCOORD1,
	in float4 uv2 : TEXCOORD2,
	in float4 uv3 : TEXCOORD3)
{
	int width, height, slices, samples;
	Tex0.GetDimensions(width, height, slices, samples);
	float4 texcol = 0;
	for(int i = 0; i < samples; ++i)
		texcol += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);
	texcol /= samples;
	int4 src6 = round(texcol * 63.f);
	int4 dst8;
	dst8.r = (src6.r << 2) | (src6.g >> 4);
	dst8.g = ((src6.g & 0xF) << 4) | (src6.b >> 2);
	dst8.b = ((src6.b & 0x3) << 6) | src6.a;
	dst8.a = 255;
	ocol0 = (float4)dst8 / 255.f;
}
)hlsl";

const char* reint_rgb8_to_rgba6_code = R"hlsl(
sampler samp0 : register(s0);
Texture2DArray Tex0 : register(t0);
void main(
	out float4 ocol0 : SV_Target,
	in float4 pos : SV_Position,
	in float3 uv0 : TEXCOORD0,
	in float  uv1 : TEXCOORD1,
	in float4 uv2 : TEXCOORD2,
	in float4 uv3 : TEXCOORD3)
{
	int4 src8 = round(Tex0.Sample(samp0,uv0) * 255.f);
	int4 dst6;
	dst6.r = src8.r >> 2;
	dst6.g = ((src8.r & 0x3) << 4) | (src8.g >> 4);
	dst6.b = ((src8.g & 0xF) << 2) | (src8.b >> 6);
	dst6.a = src8.b & 0x3F;
	ocol0 = (float4)dst6 / 63.f;
}
)hlsl";

const char* reint_rgb8_to_rgba6_msaa_code = R"hlsl(
sampler samp0 : register(s0);
Texture2DMSArray<float4, %d> Tex0 : register(t0);
void main(
	out float4 ocol0 : SV_Target,
	in float4 pos : SV_Position,
	in float3 uv0 : TEXCOORD0,
	in float  uv1 : TEXCOORD1,
	in float4 uv2 : TEXCOORD2,
	in float4 uv3 : TEXCOORD3)
{
	int width, height, slices, samples;
	Tex0.GetDimensions(width, height, slices, samples);
	float4 texcol = 0;
	for(int i = 0; i < samples; ++i)
		texcol += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);
	texcol /= samples;
	int4 src8 = round(texcol * 255.f);
	int4 dst6;
	dst6.r = src8.r >> 2;
	dst6.g = ((src8.r & 0x3) << 4) | (src8.g >> 4);
	dst6.b = ((src8.g & 0xF) << 2) | (src8.b >> 6);
	dst6.a = src8.b & 0x3F;
	ocol0 = (float4)dst6 / 63.f;
}
)hlsl";

const char* depth_resolve_program = R"hlsl(
	Texture2DMSArray<float4, %d> Tex0 : register(t0);
	void main(
		 out float ocol0 : SV_Target,
	    in float4 pos : SV_Position,
	    in float3 uv0 : TEXCOORD0)
	{
		int width, height, slices, samples;
		Tex0.GetDimensions(width, height, slices, samples);
		ocol0  = Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), 0).x;
		for(int i = 1; i < samples; ++i)
			ocol0  = min(ocol0, Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i).x);
	}
)hlsl";

ID3D11PixelShader* PixelShaderCache::GetDepthResolveProgram()
{
	if (s_DepthResolveProgram)
		return s_DepthResolveProgram.get();

	// create MSAA shader for current AA mode
	std::string buf = StringFromFormat(depth_resolve_program, g_ActiveConfig.iMultisamples);
	s_DepthResolveProgram = D3D::CompileAndCreatePixelShader(buf);
	CHECK(s_DepthResolveProgram != nullptr, "Create depth matrix MSAA pixel shader");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)s_DepthResolveProgram.get(), "depth resolve pixel shader");
	return s_DepthResolveProgram.get();
}

ID3D11PixelShader* PixelShaderCache::ReinterpRGBA6ToRGB8(bool multisampled)
{
	if (!multisampled || g_ActiveConfig.iMultisamples <= 1)
	{
		if (!s_rgba6_to_rgb8[0])
		{
			s_rgba6_to_rgb8[0] = D3D::CompileAndCreatePixelShader(reint_rgba6_to_rgb8_code);
			CHECK(s_rgba6_to_rgb8[0], "Create RGBA6 to RGB8 pixel shader");
			D3D::SetDebugObjectName(s_rgba6_to_rgb8[0].get(), "RGBA6 to RGB8 pixel shader");
		}
		return s_rgba6_to_rgb8[0].get();
	}
	else if (!s_rgba6_to_rgb8[1])
	{
		// create MSAA shader for current AA mode
		std::string buf = StringFromFormat(reint_rgba6_to_rgb8_msaa, g_ActiveConfig.iMultisamples);
		s_rgba6_to_rgb8[1] = D3D::CompileAndCreatePixelShader(buf);

		CHECK(s_rgba6_to_rgb8[1], "Create RGBA6 to RGB8 MSAA pixel shader");
		D3D::SetDebugObjectName(s_rgba6_to_rgb8[1].get(), "RGBA6 to RGB8 MSAA pixel shader");
	}
	return s_rgba6_to_rgb8[1].get();
}

ID3D11PixelShader* PixelShaderCache::ReinterpRGB8ToRGBA6(bool multisampled)
{
	if (!multisampled || g_ActiveConfig.iMultisamples <= 1)
	{
		if (!s_rgb8_to_rgba6[0])
		{
			s_rgb8_to_rgba6[0] = D3D::CompileAndCreatePixelShader(reint_rgb8_to_rgba6_code);
			CHECK(s_rgb8_to_rgba6[0], "Create RGB8 to RGBA6 pixel shader");
			D3D::SetDebugObjectName(s_rgb8_to_rgba6[0].get(), "RGB8 to RGBA6 pixel shader");
		}
		return s_rgb8_to_rgba6[0].get();
	}
	else if (!s_rgb8_to_rgba6[1])
	{
		// create MSAA shader for current AA mode
		std::string buf = StringFromFormat(reint_rgb8_to_rgba6_msaa_code, g_ActiveConfig.iMultisamples);
		s_rgb8_to_rgba6[1] = D3D::CompileAndCreatePixelShader(buf);

		CHECK(s_rgb8_to_rgba6[1], "Create RGB8 to RGBA6 MSAA pixel shader");
		D3D::SetDebugObjectName(s_rgb8_to_rgba6[1].get(), "RGB8 to RGBA6 MSAA pixel shader");
	}
	return s_rgb8_to_rgba6[1].get();
}

ID3D11PixelShader* PixelShaderCache::GetColorCopyProgram(bool multisampled, bool ssaa)
{
	if (!multisampled || g_ActiveConfig.iMultisamples <= 1)
	{
		return ssaa ? s_ColorCopyProgram[2].get() : s_ColorCopyProgram[0].get();
	}
	else if (s_ColorCopyProgram[1])
	{
		return s_ColorCopyProgram[1].get();
	}
	else
	{
		// create MSAA shader for current AA mode
		std::string buf = StringFromFormat(color_copy_program_code_msaa, g_ActiveConfig.iMultisamples);
		s_ColorCopyProgram[1] = D3D::CompileAndCreatePixelShader(buf);
		CHECK(s_ColorCopyProgram[1] != nullptr, "Create color copy MSAA pixel shader");
		D3D::SetDebugObjectName(s_ColorCopyProgram[1].get(), "color copy MSAA pixel shader");
		return s_ColorCopyProgram[1].get();
	}
}

ID3D11PixelShader* PixelShaderCache::GetColorMatrixProgram(bool multisampled)
{
	if (!multisampled || g_ActiveConfig.iMultisamples <= 1) return s_ColorMatrixProgram[0].get();
	else if (s_ColorMatrixProgram[1]) return s_ColorMatrixProgram[1].get();
	else
	{
		// create MSAA shader for current AA mode
		std::string buf = StringFromFormat(color_matrix_program_code_msaa, g_ActiveConfig.iMultisamples);
		s_ColorMatrixProgram[1] = D3D::CompileAndCreatePixelShader(buf);
		CHECK(s_ColorMatrixProgram[1] != nullptr, "Create color matrix MSAA pixel shader");
		D3D::SetDebugObjectName(s_ColorMatrixProgram[1].get(), "color matrix MSAA pixel shader");
		return s_ColorMatrixProgram[1].get();
	}
}

ID3D11PixelShader* PixelShaderCache::GetDepthMatrixProgram(bool multisampled)
{
	if (!multisampled || g_ActiveConfig.iMultisamples <= 1) return s_DepthMatrixProgram[0].get();
	else if (s_DepthMatrixProgram[1]) return s_DepthMatrixProgram[1].get();
	else
	{
		// create MSAA shader for current AA mode
		std::string buf = StringFromFormat(depth_matrix_program_msaa, g_ActiveConfig.iMultisamples);
		s_DepthMatrixProgram[1] = D3D::CompileAndCreatePixelShader(buf);
		CHECK(s_DepthMatrixProgram[1] != nullptr, "Create depth matrix MSAA pixel shader");
		D3D::SetDebugObjectName(s_DepthMatrixProgram[1].get(), "depth matrix MSAA pixel shader");
		return s_DepthMatrixProgram[1].get();
	}
}

ID3D11PixelShader* PixelShaderCache::GetClearProgram()
{
	return s_ClearProgram.get();
}

D3D::BufferDescriptor PixelShaderCache::GetConstantBuffer()
{
	if (PixelShaderManager::IsDirty())
	{
		const int sz = C_PCONST_END * 4 * sizeof(float);
		// TODO: divide the global variables of the generated shaders into about 5 constant buffers to speed this up
		pscbuf->AppendData((void*)PixelShaderManager::GetBuffer(), sz);
		PixelShaderManager::Clear();
		ADDSTAT(stats.thisFrame.bytesUniformStreamed, sz);
	}
	return pscbuf->GetDescriptor();
}

// this class will load the precompiled shaders into our cache
class PixelShaderCacheInserter : public LinearDiskCacheReader<PixelShaderUid, u8>
{
public:
	void Read(const PixelShaderUid &key, const u8 *value, u32 value_size)
	{
		PixelShaderCache::InsertByteCode(key, value, value_size);
	}
};

void PixelShaderCache::Init()
{
	s_compiler = &HLSLAsyncCompiler::getInstance();
	s_pixel_shaders_lock.unlock();
	bool use_partial_buffer_update = D3D::SupportPartialContantBufferUpdate();
	u32 cbsize = C_PCONST_END * 4 * sizeof(float) * (use_partial_buffer_update ? 1024 : 1); // is always a multiple of 16	
	pscbuf = new D3D::ConstantStreamBuffer(cbsize);
	ID3D11Buffer* buf = pscbuf->GetBuffer();
	CHECK(buf != nullptr, "Create pixel shader constant buffer");
	D3D::SetDebugObjectName(buf, "pixel shader constant buffer used to emulate the GX pipeline");

	// used when drawing clear quads
	s_ClearProgram = D3D::CompileAndCreatePixelShader(clear_program_code);
	CHECK(s_ClearProgram != nullptr, "Create clear pixel shader");
	D3D::SetDebugObjectName(s_ClearProgram.get(), "clear pixel shader");

	// used when copying/resolving the color buffer
	s_ColorCopyProgram[0] = D3D::CompileAndCreatePixelShader(color_copy_program_code);
	CHECK(s_ColorCopyProgram[0] != nullptr, "Create color copy pixel shader");
	D3D::SetDebugObjectName(s_ColorCopyProgram[0].get(), "color copy pixel shader");

	s_ColorCopyProgram[2] = D3D::CompileAndCreatePixelShader(color_copy_program_code_ssaa);
	CHECK(s_ColorCopyProgram[2] != nullptr, "Create color copy pixel shader SSAA");
	D3D::SetDebugObjectName(s_ColorCopyProgram[2].get(), "color copy pixel shader SSAA");

	// used for color conversion
	s_ColorMatrixProgram[0] = D3D::CompileAndCreatePixelShader(color_matrix_program_code);
	CHECK(s_ColorMatrixProgram[0] != nullptr, "Create color matrix pixel shader");
	D3D::SetDebugObjectName(s_ColorMatrixProgram[0].get(), "color matrix pixel shader");

	// used for depth copy
	s_DepthMatrixProgram[0] = D3D::CompileAndCreatePixelShader(depth_matrix_program_code);
	CHECK(s_DepthMatrixProgram[0] != nullptr, "Create depth matrix pixel shader");
	D3D::SetDebugObjectName(s_DepthMatrixProgram[0].get(), "depth matrix pixel shader");

	Clear();

	if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX).c_str());

	SETSTAT(stats.numPixelShadersCreated, 0);
	SETSTAT(stats.numPixelShadersAlive, 0);

	pKey_t gameid = (pKey_t)GetMurmurHash3(reinterpret_cast<const u8*>(SConfig::GetInstance().m_strGameID.data()), (u32)SConfig::GetInstance().m_strGameID.size(), 0);
	s_pixel_shaders = PSCache::Create(
		gameid,
		PIXELSHADERGEN_UID_VERSION,
		"Ishiiruka.ps",
		StringFromFormat("%s.ps", SConfig::GetInstance().m_strGameID.c_str())
	);

	std::string cache_filename = StringFromFormat("%sIDX11-%s-ps.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
		SConfig::GetInstance().m_strGameID.c_str());

	PixelShaderCacheInserter inserter;
	g_ps_disk_cache.OpenAndRead(cache_filename, inserter);
	if (g_ActiveConfig.bCompileShaderOnStartup)
	{
		size_t shader_count = 0;
		s_pixel_shaders->ForEachMostUsedByCategory(gameid,
			[&](const PixelShaderUid& it, size_t total)
		{
			PixelShaderUid item = it;
			item.ClearHASH();
			item.CalculateUIDHash();
			CompilePShader(item, true);
			shader_count++;
			if ((shader_count & 7) == 0)
			{
				Host_UpdateTitle(StringFromFormat("Compiling Pixel Shaders %i %% (%i/%i)", (shader_count * 100) / total, shader_count, total));
				s_compiler->WaitForFinish();
			}
		},
			[](PSCacheEntry& entry)
		{
			return !entry.shader;
		}
		, true);
		s_compiler->WaitForFinish();
	}
	s_last_entry = nullptr;
}

// ONLY to be used during shutdown.
void PixelShaderCache::Clear()
{
	if (s_pixel_shaders)
	{
		s_pixel_shaders_lock.lock();
		s_pixel_shaders->Persist();
		s_pixel_shaders->Clear([](PSCacheEntry& item)
		{
			item.Destroy();
		});
		s_pixel_shaders_lock.unlock();
	}
	s_last_entry = nullptr;
}

// Used in Swap() when AA mode has changed
void PixelShaderCache::InvalidateMSAAShaders()
{
	s_ColorCopyProgram[1].reset();
	s_ColorMatrixProgram[1].reset();
	s_DepthMatrixProgram[1].reset();
	s_rgb8_to_rgba6[1].reset();
	s_rgba6_to_rgb8[1].reset();
	s_DepthResolveProgram.reset();
}

void PixelShaderCache::Shutdown()
{
	if (s_compiler)
	{
		s_compiler->WaitForFinish();
	}
	if (pscbuf != nullptr)
	{
		delete pscbuf;
	}
	pscbuf = nullptr;
	s_ClearProgram.reset();
	s_DepthResolveProgram.reset();
	for (auto & p : s_ColorCopyProgram)
		p.reset();
	for (auto & p : s_ColorMatrixProgram)
		p.reset();
	for (auto & p : s_DepthMatrixProgram)
		p.reset();
	for (auto & p : s_rgba6_to_rgb8)
		p.reset();
	for (auto & p : s_rgb8_to_rgba6)
		p.reset();
	Clear();
	delete s_pixel_shaders;
	s_pixel_shaders = nullptr;
	g_ps_disk_cache.Sync();
	g_ps_disk_cache.Close();
}

void PixelShaderCache::CompilePShader(const PixelShaderUid& uid, bool ongputhread)
{
	s_pixel_shaders_lock.lock();
	PSCacheEntry* entry = &s_pixel_shaders->GetOrAdd(uid);
	s_pixel_shaders_lock.unlock();
	if (ongputhread)
	{
		s_last_entry = entry;
	}
	// Compile only when we have a new instance
	if (entry->initialized.test_and_set())
	{
		return;
	}
	// Need to compile a new shader

	ShaderCompilerWorkUnit *wunit = s_compiler->NewUnit(PIXELSHADERGEN_BUFFERSIZE);
	wunit->GenerateCodeHandler = [uid](ShaderCompilerWorkUnit* wunit)
	{
		ShaderCode code;
		code.SetBuffer(wunit->code.data());
		GeneratePixelShaderCodeD3D11(code, uid.GetUidData());
		wunit->codesize = (u32)code.BufferSize();
	};

	wunit->entrypoint = "main";
#if defined(_DEBUG) || defined(DEBUGFAST)
	wunit->flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	wunit->flags = D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
	wunit->target = D3D::PixelShaderVersionString();
	wunit->ResultHandler = [uid, entry](ShaderCompilerWorkUnit* wunit)
	{
		if (SUCCEEDED(wunit->cresult))
		{
			ID3DBlob* shaderBuffer = wunit->shaderbytecode;
			const u8* bytecode = (const u8*)shaderBuffer->GetBufferPointer();
			u32 bytecodelen = (u32)shaderBuffer->GetBufferSize();
			g_ps_disk_cache.Append(uid, bytecode, bytecodelen);
			PushByteCode(bytecode, bytecodelen, entry);
		}
		else
		{
			static int num_failures = 0;
			std::string filename = StringFromFormat("%sbad_ps_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
			std::ofstream file;
			OpenFStream(file, filename, std::ios_base::out);
			file << ((const char *)wunit->code.data());
			file << ((const char *)wunit->error->GetBufferPointer());
			file.close();

			PanicAlert("Failed to compile pixel shader!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%s):\n%s",
				filename,
				D3D::PixelShaderVersionString(),
				(char*)wunit->error->GetBufferPointer());
		}
	};
	s_compiler->CompileShaderAsync(wunit);
}

void PixelShaderCache::PrepareShader(PIXEL_SHADER_RENDER_MODE render_mode,
	u32 components,
	const XFMemory &xfr,
	const BPMemory &bpm, bool ongputhread)
{
	PixelShaderUid uid;
	GetPixelShaderUID(uid, render_mode, components, xfr, bpm);
	if (ongputhread)
	{
		s_compiler->ProcCompilationResults();
		// Check if the shader is already set
		if (s_last_entry)
		{
			if (uid == s_last_uid)
			{
				return;
			}
		}
		s_last_uid = uid;
		GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
	}
	else
	{
		if (s_external_last_uid == uid)
		{
			return;
		}
		s_external_last_uid = uid;
	}
	CompilePShader(uid, ongputhread);
}

bool PixelShaderCache::TestShader()
{
	int count = 0;
	while (!s_last_entry->compiled)
	{
		s_compiler->ProcCompilationResults();
		if (g_ActiveConfig.bFullAsyncShaderCompilation)
		{
			break;
		}
		Common::cYield(count++);
	}
	return s_last_entry->shader != nullptr && s_last_entry->compiled;
}

void PixelShaderCache::PushByteCode(const void* bytecode, u32 bytecodelen, PixelShaderCache::PSCacheEntry* entry)
{
	entry->shader = std::move(D3D::CreatePixelShaderFromByteCode(bytecode, bytecodelen));
	entry->compiled = true;
	if (entry->shader != nullptr)
	{
		// TODO: Somehow make the debug name a bit more specific
		D3D::SetDebugObjectName(entry->shader.get(), "a pixel shader of PixelShaderCache");
		INCSTAT(stats.numPixelShadersCreated);
		SETSTAT(stats.numPixelShadersAlive, static_cast<int>(s_pixel_shaders->size()));
	}
}

void PixelShaderCache::InsertByteCode(const PixelShaderUid &uid, const void* bytecode, u32 bytecodelen)
{
	PSCacheEntry* entry = &s_pixel_shaders->GetOrAdd(uid);
	entry->initialized.test_and_set();
	PushByteCode(bytecode, bytecodelen, entry);
}

}  // DX11
