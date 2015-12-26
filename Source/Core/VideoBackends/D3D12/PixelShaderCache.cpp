// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/FileUtil.h"
#include "Common/LinearDiskCache.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"

#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DShader.h"
#include "VideoBackends/D3D12/Globals.h"
#include "VideoBackends/D3D12/PixelShaderCache.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/HLSLCompiler.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{

PixelShaderCache::PSCache PixelShaderCache::s_pixel_shaders;
const PixelShaderCache::PSCacheEntry* PixelShaderCache::s_last_entry;
PixelShaderUid PixelShaderCache::s_last_uid;
PixelShaderUid PixelShaderCache::s_external_last_uid;
UidChecker<PixelShaderUid, ShaderCode> PixelShaderCache::s_pixel_uid_checker;
static HLSLAsyncCompiler *s_compiler;
static Common::SpinLock<true> s_pixel_shaders_lock;

LinearDiskCache<PixelShaderUid, u8> g_ps_disk_cache;

D3DBlob* s_ColorMatrixProgramBlob[2] = {};
D3DBlob* s_ColorCopyProgramBlob[2] = {};
D3DBlob* s_DepthMatrixProgramBlob[2] = {};
D3DBlob* s_DepthCopyProgramBlob[2] = {};
D3DBlob* s_ClearProgramBlob = {};
D3DBlob* s_AnaglyphProgramBlob = {};
D3DBlob* s_rgba6_to_rgb8Blob[2] = {};
D3DBlob* s_rgb8_to_rgba6Blob[2] = {};

ID3D12Resource* pscbuf12 = nullptr;
D3D12_GPU_VIRTUAL_ADDRESS pscbuf12GPUVA = {};
void* pscbuf12data = nullptr;
const UINT pscbuf12paddedSize = ((C_PCONST_END * 4 * sizeof(float)) + 0xff) & ~0xff;

#define pscbuf12Slots 10000
unsigned int currentPscbuf12 = 0; // 0 - pscbuf12Slots;

const char clear_program_code[] = {
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	"in float4 pos : SV_Position,\n"
	"in float4 incol0 : COLOR0){\n"
	"ocol0 = incol0;\n"
	"}\n"
};

// TODO: Find some way to avoid having separate shaders for non-MSAA and MSAA...
const char color_copy_program_code[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2DArray Tex0 : register(t0);\n"
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	"in float4 pos : SV_Position,\n"
	"in float3 uv0 : TEXCOORD0){\n"
	"ocol0 = Tex0.Sample(samp0,uv0);\n"
	"}\n"
};

const char depth_copy_program_code[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2DArray Tex0 : register(t0);\n"
	"void main(\n"
	"out float odepth : SV_Depth,\n"
	"in float4 pos : SV_Position,\n"
	"in float3 uv0 : TEXCOORD0){\n"
	"odepth = Tex0.Sample(samp0,uv0);\n"
	"}\n"
};

// Anaglyph Red-Cyan shader based on Dubois algorithm
// Constants taken from the paper:
// "Conversion of a Stereo Pair to Anaglyph with
// the Least-Squares Projection Method"
// Eric Dubois, March 2009
const char anaglyph_program_code[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2DArray Tex0 : register(t0);\n"
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	"in float4 pos : SV_Position,\n"
	"in float3 uv0 : TEXCOORD0){\n"
	"float4 c0 = Tex0.Sample(samp0, float3(uv0.xy, 0.0));\n"
	"float4 c1 = Tex0.Sample(samp0, float3(uv0.xy, 1.0));\n"
	"float3x3 l = float3x3( 0.437, 0.449, 0.164,\n"
	"                      -0.062,-0.062,-0.024,\n"
	"                      -0.048,-0.050,-0.017);\n"
	"float3x3 r = float3x3(-0.011,-0.032,-0.007,\n"
	"                       0.377, 0.761, 0.009,\n"
	"                      -0.026,-0.093, 1.234);\n"
	"ocol0 = float4(mul(l, c0.rgb) + mul(r, c1.rgb), c0.a);\n"
	"}\n"
};

// TODO: Improve sampling algorithm!
const char color_copy_program_code_msaa[] = {
	"#define SAMPLES %d\n"
	"sampler samp0 : register(s0);\n"
	"Texture2DMSArray<float4, SAMPLES> Tex0 : register(t0);\n"
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	"in float4 pos : SV_Position,\n"
	"in float3 uv0 : TEXCOORD0){\n"
	"int width, height, slices, samples;\n"
	"Tex0.GetDimensions(width, height, slices, samples);\n"
	"ocol0 = 0;\n"
	"for(int i = 0; i < SAMPLES; ++i)\n"
	"	ocol0 += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);\n"
	"ocol0 /= SAMPLES;\n"
	"}\n"
};

const char depth_copy_program_code_msaa[] = {
	"#define SAMPLES %d\n"
	"sampler samp0 : register(s0);\n"
	"Texture2DMSArray<float, SAMPLES> Tex0 : register(t0);\n"
	"void main(\n"
	"out float odepth : SV_Depth,\n"
	"in float4 pos : SV_Position,\n"
	"in float3 uv0 : TEXCOORD0){\n"
	"int width, height, slices, samples;\n"
	"Tex0.GetDimensions(width, height, slices, samples);\n"
	"odepth = 0;\n"
	"for(int i = 0; i < SAMPLES; ++i)\n"
	"	odepth += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);\n"
	"odepth /= SAMPLES;\n"
	"}\n"
};

const char color_matrix_program_code[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2DArray Tex0 : register(t0);\n"
	"uniform float4 cColMatrix[7] : register(c0);\n"
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	"in float4 pos : SV_Position,\n"
	"in float3 uv0 : TEXCOORD0){\n"
	"float4 texcol = Tex0.Sample(samp0,uv0);\n"
	"texcol = round(texcol * cColMatrix[5])*cColMatrix[6];\n"
	"ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"
	"}\n"
};

const char color_matrix_program_code_msaa[] = {
	"#define SAMPLES %d\n"
	"sampler samp0 : register(s0);\n"
	"Texture2DMSArray<float4, SAMPLES> Tex0 : register(t0);\n"
	"uniform float4 cColMatrix[7] : register(c0);\n"
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	"in float4 pos : SV_Position,\n"
	"in float3 uv0 : TEXCOORD0){\n"
	"int width, height, slices, samples;\n"
	"Tex0.GetDimensions(width, height, slices, samples);\n"
	"float4 texcol = 0;\n"
	"for(int i = 0; i < SAMPLES; ++i)\n"
	"	texcol += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);\n"
	"texcol /= SAMPLES;\n"
	"texcol = round(texcol * cColMatrix[5])*cColMatrix[6];\n"
	"ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"
	"}\n"
};

const char depth_matrix_program[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2DArray Tex0 : register(t0);\n"
	"uniform float4 cColMatrix[7] : register(c0);\n"
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	" in float4 pos : SV_Position,\n"
	" in float3 uv0 : TEXCOORD0){\n"
	"	float4 texcol = Tex0.Sample(samp0,uv0);\n"
	"	int depth = int((1.0 - texcol.x) * 16777216.0);\n"

	// Convert to Z24 format
	"	int4 workspace;\n"
	"	workspace.r = (depth >> 16) & 255;\n"
	"	workspace.g = (depth >> 8) & 255;\n"
	"	workspace.b = depth & 255;\n"

	// Convert to Z4 format
	"	workspace.a = (depth >> 16) & 0xF0;\n"

	// Normalize components to [0.0..1.0]
	"	texcol = float4(workspace) / 255.0;\n"

	// Apply color matrix
	"	ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"
	"}\n"
};

const char depth_matrix_program_msaa[] = {
	"#define SAMPLES %d\n"
	"sampler samp0 : register(s0);\n"
	"Texture2DMSArray<float4, SAMPLES> Tex0 : register(t0);\n"
	"uniform float4 cColMatrix[7] : register(c0);\n"
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	" in float4 pos : SV_Position,\n"
	" in float3 uv0 : TEXCOORD0){\n"
	"	int width, height, slices, samples;\n"
	"	Tex0.GetDimensions(width, height, slices, samples);\n"
	"	float4 texcol = 0;\n"
	"	for(int i = 0; i < SAMPLES; ++i)\n"
	"		texcol += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);\n"
	"	texcol /= SAMPLES;\n"
	"	int depth = int((1.0 - texcol.x) * 16777216.0);\n"

	// Convert to Z24 format
	"	int4 workspace;\n"
	"	workspace.r = (depth >> 16) & 255;\n"
	"	workspace.g = (depth >> 8) & 255;\n"
	"	workspace.b = depth & 255;\n"

	// Convert to Z4 format
	"	workspace.a = (depth >> 16) & 0xF0;\n"

	// Normalize components to [0.0..1.0]
	"	texcol = float4(workspace) / 255.0;\n"

	// Apply color matrix
	"	ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"
	"}\n"
};

const char reint_rgba6_to_rgb8[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2DArray Tex0 : register(t0);\n"
	"void main(\n"
	"	out float4 ocol0 : SV_Target,\n"
	"	in float4 pos : SV_Position,\n"
	"	in float3 uv0 : TEXCOORD0)\n"
	"{\n"
	"	int4 src6 = round(Tex0.Sample(samp0,uv0) * 63.f);\n"
	"	int4 dst8;\n"
	"	dst8.r = (src6.r << 2) | (src6.g >> 4);\n"
	"	dst8.g = ((src6.g & 0xF) << 4) | (src6.b >> 2);\n"
	"	dst8.b = ((src6.b & 0x3) << 6) | src6.a;\n"
	"	dst8.a = 255;\n"
	"	ocol0 = (float4)dst8 / 255.f;\n"
	"}"
};

const char reint_rgba6_to_rgb8_msaa[] = {
	"#define SAMPLES %d\n"
	"sampler samp0 : register(s0);\n"
	"Texture2DMSArray<float4, SAMPLES> Tex0 : register(t0);\n"
	"void main(\n"
	"	out float4 ocol0 : SV_Target,\n"
	"	in float4 pos : SV_Position,\n"
	"	in float3 uv0 : TEXCOORD0)\n"
	"{\n"
	"	int width, height, slices, samples;\n"
	"	Tex0.GetDimensions(width, height, slices, samples);\n"
	"	float4 texcol = 0;\n"
	"	for (int i = 0; i < SAMPLES; ++i)\n"
	"		texcol += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);\n"
	"	texcol /= SAMPLES;\n"
	"	int4 src6 = round(texcol * 63.f);\n"
	"	int4 dst8;\n"
	"	dst8.r = (src6.r << 2) | (src6.g >> 4);\n"
	"	dst8.g = ((src6.g & 0xF) << 4) | (src6.b >> 2);\n"
	"	dst8.b = ((src6.b & 0x3) << 6) | src6.a;\n"
	"	dst8.a = 255;\n"
	"	ocol0 = (float4)dst8 / 255.f;\n"
	"}"
};

const char reint_rgb8_to_rgba6[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2DArray Tex0 : register(t0);\n"
	"void main(\n"
	"	out float4 ocol0 : SV_Target,\n"
	"	in float4 pos : SV_Position,\n"
	"	in float3 uv0 : TEXCOORD0)\n"
	"{\n"
	"	int4 src8 = round(Tex0.Sample(samp0,uv0) * 255.f);\n"
	"	int4 dst6;\n"
	"	dst6.r = src8.r >> 2;\n"
	"	dst6.g = ((src8.r & 0x3) << 4) | (src8.g >> 4);\n"
	"	dst6.b = ((src8.g & 0xF) << 2) | (src8.b >> 6);\n"
	"	dst6.a = src8.b & 0x3F;\n"
	"	ocol0 = (float4)dst6 / 63.f;\n"
	"}\n"
};

const char reint_rgb8_to_rgba6_msaa[] = {
	"#define SAMPLES %d\n"
	"sampler samp0 : register(s0);\n"
	"Texture2DMSArray<float4, SAMPLES> Tex0 : register(t0);\n"
	"void main(\n"
	"	out float4 ocol0 : SV_Target,\n"
	"	in float4 pos : SV_Position,\n"
	"	in float3 uv0 : TEXCOORD0)\n"
	"{\n"
	"	int width, height, slices, samples;\n"
	"	Tex0.GetDimensions(width, height, slices, samples);\n"
	"	float4 texcol = 0;\n"
	"	for (int i = 0; i < SAMPLES; ++i)\n"
	"		texcol += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);\n"
	"	texcol /= SAMPLES;\n"
	"	int4 src8 = round(texcol * 255.f);\n"
	"	int4 dst6;\n"
	"	dst6.r = src8.r >> 2;\n"
	"	dst6.g = ((src8.r & 0x3) << 4) | (src8.g >> 4);\n"
	"	dst6.b = ((src8.g & 0xF) << 2) | (src8.b >> 6);\n"
	"	dst6.a = src8.b & 0x3F;\n"
	"	ocol0 = (float4)dst6 / 63.f;\n"
	"}\n"
};

D3D12_SHADER_BYTECODE PixelShaderCache::ReinterpRGBA6ToRGB812(bool multisampled)
{
	D3D12_SHADER_BYTECODE bytecode = {};

	if (!multisampled || g_ActiveConfig.iMultisamples == 1)
	{
		if (!s_rgba6_to_rgb8Blob[0])
		{
			D3D::CompilePixelShader(reint_rgba6_to_rgb8, &s_rgba6_to_rgb8Blob[0]);
		}

		bytecode = { s_rgba6_to_rgb8Blob[0]->Data(), s_rgba6_to_rgb8Blob[0]->Size() };
		return bytecode;
	}
	else if (!s_rgba6_to_rgb8Blob[1])
	{
		// create MSAA shader for current AA mode
		std::string buf = StringFromFormat(reint_rgba6_to_rgb8_msaa, g_ActiveConfig.iMultisamples);

		D3D::CompilePixelShader(buf, &s_rgba6_to_rgb8Blob[1]);
		bytecode = { s_rgba6_to_rgb8Blob[1]->Data(), s_rgba6_to_rgb8Blob[1]->Size() };
	}
	return bytecode;
}

D3D12_SHADER_BYTECODE PixelShaderCache::ReinterpRGB8ToRGBA612(bool multisampled)
{
	D3D12_SHADER_BYTECODE bytecode = {};

	if (!multisampled || g_ActiveConfig.iMultisamples == 1)
	{
		if (!s_rgb8_to_rgba6Blob[0])
		{
			D3D::CompilePixelShader(reint_rgb8_to_rgba6, &s_rgb8_to_rgba6Blob[0]);
		}

		bytecode = { s_rgb8_to_rgba6Blob[0]->Data(), s_rgb8_to_rgba6Blob[0]->Size() };
		return bytecode;
	}
	else if (!s_rgb8_to_rgba6Blob[1])
	{
		// create MSAA shader for current AA mode
		std::string buf = StringFromFormat(reint_rgb8_to_rgba6_msaa, g_ActiveConfig.iMultisamples);

		D3D::CompilePixelShader(buf, &s_rgb8_to_rgba6Blob[1]);
		bytecode = { s_rgb8_to_rgba6Blob[1]->Data(), s_rgb8_to_rgba6Blob[1]->Size() };
	}

	return bytecode;
}

D3D12_SHADER_BYTECODE PixelShaderCache::GetColorCopyProgram12(bool multisampled)
{
	D3D12_SHADER_BYTECODE bytecode = {};

	if (!multisampled || g_ActiveConfig.iMultisamples == 1)
	{
		bytecode = { s_ColorCopyProgramBlob[0]->Data(), s_ColorCopyProgramBlob[0]->Size() };
	}
	else if (s_ColorCopyProgramBlob[1])
	{
		bytecode = { s_ColorCopyProgramBlob[1]->Data(), s_ColorCopyProgramBlob[1]->Size() };
	}
	else
	{
		// create MSAA shader for current AA mode
		std::string buf = StringFromFormat(color_copy_program_code_msaa, g_ActiveConfig.iMultisamples);

		D3D::CompilePixelShader(buf, &s_ColorCopyProgramBlob[1]);
		bytecode = { s_ColorCopyProgramBlob[1]->Data(), s_ColorCopyProgramBlob[1]->Size() };
	}

	return bytecode;
}

D3D12_SHADER_BYTECODE PixelShaderCache::GetColorMatrixProgram12(bool multisampled)
{
	D3D12_SHADER_BYTECODE bytecode = {};

	if (!multisampled || g_ActiveConfig.iMultisamples == 1)
	{
		bytecode = { s_ColorMatrixProgramBlob[0]->Data(), s_ColorMatrixProgramBlob[0]->Size() };
	}
	else if (s_ColorMatrixProgramBlob[1])
	{
		bytecode = { s_ColorMatrixProgramBlob[1]->Data(), s_ColorMatrixProgramBlob[1]->Size() };
	}
	else
	{
		// create MSAA shader for current AA mode
		std::string buf = StringFromFormat(color_matrix_program_code_msaa, g_ActiveConfig.iMultisamples);

		D3D::CompilePixelShader(buf, &s_ColorMatrixProgramBlob[1]);
		bytecode = { s_ColorMatrixProgramBlob[1]->Data(), s_ColorMatrixProgramBlob[1]->Size() };
	}

	return bytecode;
}

D3D12_SHADER_BYTECODE PixelShaderCache::GetDepthMatrixProgram12(bool multisampled)
{
	D3D12_SHADER_BYTECODE bytecode = {};

	if (!multisampled || g_ActiveConfig.iMultisamples == 1)
	{
		bytecode = { s_DepthMatrixProgramBlob[0]->Data(), s_DepthMatrixProgramBlob[0]->Size() };
	}
	else if (s_DepthMatrixProgramBlob[1])
	{
		bytecode = { s_DepthMatrixProgramBlob[1]->Data(), s_DepthMatrixProgramBlob[1]->Size() };
	}
	else
	{
		// create MSAA shader for current AA mode
		std::string buf = StringFromFormat(depth_matrix_program_msaa, g_ActiveConfig.iMultisamples);

		D3D::CompilePixelShader(buf, &s_DepthMatrixProgramBlob[1]);

		bytecode = { s_DepthMatrixProgramBlob[1]->Data(), s_DepthMatrixProgramBlob[1]->Size() };
	}

	return bytecode;
}


D3D12_SHADER_BYTECODE PixelShaderCache::GetDepthCopyProgram12(bool multisampled)
{
	D3D12_SHADER_BYTECODE bytecode = {};

	if (!multisampled || g_ActiveConfig.iMultisamples == 1)
	{
		bytecode = { s_DepthCopyProgramBlob[0]->Data(), s_DepthCopyProgramBlob[0]->Size() };
	}
	else if (s_DepthCopyProgramBlob[1])
	{
		bytecode = { s_DepthCopyProgramBlob[1]->Data(), s_DepthCopyProgramBlob[1]->Size() };
	}
	else
	{
		// create MSAA shader for current AA mode
		std::string buf = StringFromFormat(depth_copy_program_code_msaa, g_ActiveConfig.iMultisamples);

		D3D::CompilePixelShader(buf, &s_DepthCopyProgramBlob[1]);
		bytecode = { s_DepthCopyProgramBlob[1]->Data(), s_DepthCopyProgramBlob[1]->Size() };
	}

	return bytecode;
}

D3D12_SHADER_BYTECODE PixelShaderCache::GetClearProgram12()
{
	D3D12_SHADER_BYTECODE shader = {};
	shader.BytecodeLength = s_ClearProgramBlob->Size();
	shader.pShaderBytecode = s_ClearProgramBlob->Data();

	return shader;
}

D3D12_SHADER_BYTECODE PixelShaderCache::GetAnaglyphProgram12()
{
	D3D12_SHADER_BYTECODE shader = {};
	shader.BytecodeLength = s_AnaglyphProgramBlob->Size();
	shader.pShaderBytecode = s_AnaglyphProgramBlob->Data();

	return shader;
}

void PixelShaderCache::GetConstantBuffer12()
{
	if (PixelShaderManager::IsDirty())
	{
		currentPscbuf12 = (currentPscbuf12 + 1) % pscbuf12Slots;
		const int sz = C_PCONST_END * 4 * sizeof(float);
		memcpy((u8*)pscbuf12data + currentPscbuf12 * pscbuf12paddedSize, PixelShaderManager::GetBuffer(), sz);

		PixelShaderManager::Clear();

		ADDSTAT(stats.thisFrame.bytesUniformStreamed, sz);

		D3D::command_list_mgr->m_dirty_ps_cbv = true;
	}

	if (D3D::command_list_mgr->m_dirty_ps_cbv)
	{
		D3D::current_command_list->SetGraphicsRootConstantBufferView(
			DESCRIPTOR_TABLE_PS_CBVONE,
			pscbuf12GPUVA + currentPscbuf12 * pscbuf12paddedSize
			);

		D3D::command_list_mgr->m_dirty_ps_cbv = false;
	}
}

// this class will load the precompiled shaders into our cache
class PixelShaderCacheInserter : public LinearDiskCacheReader<PixelShaderUid, u8>
{
public:
	void Read(const PixelShaderUid &key, const u8* value, u32 value_size)
	{
		PixelShaderCache::InsertByteCode(key, value, value_size);
	}
};

void PixelShaderCache::Init()
{
	s_compiler = &HLSLAsyncCompiler::getInstance();
	s_pixel_shaders_lock.unlock();
	unsigned int pscbuf12sizeInBytes = pscbuf12paddedSize * pscbuf12Slots;
	CheckHR(D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(pscbuf12sizeInBytes), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&pscbuf12)));
	D3D::SetDebugObjectName12(pscbuf12, "pixel shader constant buffer used to emulate the GX pipeline");

	// Obtain persistent CPU pointer to PS Constant Buffer
	CheckHR(pscbuf12->Map(0, nullptr, &pscbuf12data));

	// Obtain GPU VA for buffer, used at binding time.
	pscbuf12GPUVA = pscbuf12->GetGPUVirtualAddress();

	// used when drawing clear quads
	D3D::CompilePixelShader(clear_program_code, &s_ClearProgramBlob);

	// used for anaglyph stereoscopy
	D3D::CompilePixelShader(anaglyph_program_code, &s_AnaglyphProgramBlob);

	// used when copying/resolving the color buffer
	D3D::CompilePixelShader(color_copy_program_code, &s_ColorCopyProgramBlob[0]);

	// used for color conversion
	D3D::CompilePixelShader(color_matrix_program_code, &s_ColorMatrixProgramBlob[0]);

	// used for depth copy
	D3D::CompilePixelShader(depth_matrix_program, &s_DepthMatrixProgramBlob[0]);

	Clear();

	if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

	SETSTAT(stats.numPixelShadersCreated, 0);
	SETSTAT(stats.numPixelShadersAlive, 0);

	// Intentionally share the same cache as DX11, as the shaders are identical. Reduces recompilation when switching APIs.
	std::string cache_filename = StringFromFormat("%sIDX11-%s-ps.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
		SConfig::GetInstance().m_strUniqueID.c_str());
	PixelShaderCacheInserter inserter;
	g_ps_disk_cache.OpenAndRead(cache_filename, inserter);

	if (g_Config.bEnableShaderDebugging)
		Clear();

	s_last_entry = nullptr;
	s_last_uid = {};
	s_external_last_uid = {};
	PixelShaderManager::DisableDirtyRegions();
}

// ONLY to be used during shutdown.
void PixelShaderCache::Clear()
{
	for (auto& iter : s_pixel_shaders)
	{
		iter.second.Destroy();
		delete iter.second.shaderDesc.pShaderBytecode;
	}

	s_pixel_shaders.clear();
	s_pixel_shaders_lock.unlock();
	s_pixel_uid_checker.Invalidate();

	s_last_entry = nullptr;
}

// Used in Swap() when AA mode has changed
void PixelShaderCache::InvalidateMSAAShaders()
{
	SAFE_RELEASE(s_ColorCopyProgramBlob[1]);
	SAFE_RELEASE(s_ColorMatrixProgramBlob[1]);
	SAFE_RELEASE(s_DepthMatrixProgramBlob[1]);
	SAFE_RELEASE(s_rgb8_to_rgba6Blob[1]);
	SAFE_RELEASE(s_rgba6_to_rgb8Blob[1]);
}

void PixelShaderCache::Shutdown()
{
	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(pscbuf12);

	SAFE_RELEASE(s_ClearProgramBlob);
	SAFE_RELEASE(s_AnaglyphProgramBlob);

	for (int i = 0; i < 2; ++i)
	{
		SAFE_RELEASE(s_ColorCopyProgramBlob[i]);
		SAFE_RELEASE(s_ColorMatrixProgramBlob[i]);
		SAFE_RELEASE(s_DepthMatrixProgramBlob[i]);
		SAFE_RELEASE(s_rgba6_to_rgb8Blob[i]);
		SAFE_RELEASE(s_rgb8_to_rgba6Blob[i]);
	}

	Clear();
	g_ps_disk_cache.Sync();
	g_ps_disk_cache.Close();
}

void PixelShaderCache::PrepareShader(DSTALPHA_MODE dstAlphaMode,
	u32 components,
	const XFMemory &xfr,
	const BPMemory &bpm, bool ongputhread)
{
	PixelShaderUid uid;
	GetPixelShaderUidD3D11(uid, dstAlphaMode, components, xfr, bpm);
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
		D3D::command_list_mgr->m_dirty_pso = true;
#if defined(_DEBUG) || defined(DEBUGFAST)
		if (g_ActiveConfig.bEnableShaderDebugging)
		{
			ShaderCode code;
			GeneratePixelShaderCodeD3D11(code, dstAlphaMode, components, xfr, bpm);
			s_pixel_uid_checker.AddToIndexAndCheck(code, uid, "Pixel", "p");
		}
#endif
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
	s_pixel_shaders_lock.lock();
	PSCacheEntry* entry = &s_pixel_shaders[uid];
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
	ShaderCode code;
	ShaderCompilerWorkUnit *wunit = s_compiler->NewUnit(PIXELSHADERGEN_BUFFERSIZE);
	code.SetBuffer(wunit->code.data());
	GeneratePixelShaderCodeD3D11(code, dstAlphaMode, components, xfr, bpm);
	wunit->codesize = (u32)code.BufferSize();
	wunit->entrypoint = "main";
	wunit->flags = D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_OPTIMIZATION_LEVEL3;
	wunit->target = D3D::PixelShaderVersionString();
	wunit->ResultHandler = [uid, entry](ShaderCompilerWorkUnit* wunit)
	{
		if (SUCCEEDED(wunit->cresult))
		{
			ID3DBlob* shaderBuffer = wunit->shaderbytecode;
			const u8* bytecode = (const u8*)shaderBuffer->GetBufferPointer();
			u32 bytecodelen = (u32)shaderBuffer->GetBufferSize();
			g_ps_disk_cache.Append(uid, bytecode, bytecodelen);
			PushByteCode(entry, bytecode, bytecodelen);			
#if defined(_DEBUG) || defined(DEBUGFAST)
			if (g_ActiveConfig.bEnableShaderDebugging)
			{
				entry->code = wunit->code.data();
			}
#endif
		}
		else
		{
			static int num_failures = 0;
			char szTemp[MAX_PATH];
			sprintf(szTemp, "%sbad_ps_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
			std::ofstream file;
			OpenFStream(file, szTemp, std::ios_base::out);
			file << ((const char *)wunit->code.data());
			file << ((const char *)wunit->error->GetBufferPointer());
			file.close();

			PanicAlert("Failed to compile pixel shader!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%s):\n%s",
				szTemp,
				D3D::PixelShaderVersionString(),
				(char*)wunit->error->GetBufferPointer());
		}
	};
	s_compiler->CompileShaderAsync(wunit);
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
	return s_last_entry->shaderDesc.pShaderBytecode != nullptr && s_last_entry->compiled;
}

void PixelShaderCache::PushByteCode(PixelShaderCache::PSCacheEntry* entry, const void* bytecode, unsigned int bytecodelen)
{

	// In D3D12, shader bytecode is needed at Pipeline State creation time, so make a copy (as LinearDiskCache frees original after load).
	entry->shaderDesc.BytecodeLength = bytecodelen;
	entry->shaderDesc.pShaderBytecode = new u8[bytecodelen];
	memcpy(const_cast<void*>(entry->shaderDesc.pShaderBytecode), bytecode, bytecodelen);
	entry->compiled = true;

	INCSTAT(stats.numPixelShadersCreated);
	SETSTAT(stats.numPixelShadersAlive, s_pixel_shaders.size());
}


bool PixelShaderCache::InsertByteCode(const PixelShaderUid &uid, const void* bytecode, unsigned int bytecodelen)
{
	// Make an entry in the table
	PSCacheEntry* newentry = &s_pixel_shaders[uid];

	// In D3D12, shader bytecode is needed at Pipeline State creation time, so make a copy (as LinearDiskCache frees original after load).
	newentry->shaderDesc.BytecodeLength = bytecodelen;
	newentry->shaderDesc.pShaderBytecode = new u8[bytecodelen];
	memcpy(const_cast<void*>(newentry->shaderDesc.pShaderBytecode), bytecode, bytecodelen);
	newentry->initialized.test_and_set();
	newentry->compiled = true;
	if (!bytecode)
	{
		// INCSTAT(stats.numPixelShadersFailed);
		return false;
	}
	SETSTAT(stats.numPixelShadersAlive, s_pixel_shaders.size());
	return true;
}

}  // DX12
