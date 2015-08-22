// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "VideoBackends/DX11/D3DTexture.h"
#include "VideoBackends/DX11/D3DState.h"
#include "VideoBackends/DX11/FramebufferManager.h"
#include "VideoBackends/DX11/PostProcessing.h"

#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{
struct paramsStruct
{
	// Time
	u32 Time;
	u32 ScalingFilter;
	// Layer
	int Layer;
	// Gamma
	float native_gamma;	
	float resolution[4];
	float targetscale[4];
	float dstscale[4];
};

struct STVertex { float x, y, z, u0, v0; };

static const char* vertex_shader_code = R"hlsl(
cbuffer ParamBuffer : register(b0) 
{
	uint Time;
	uint ScalingFilter;
	int Layer;
	float native_gamma;
	
	float4 resolution;
	float4 targetscale;
	float4 dstscale;
}

struct VSOUTPUT
{
	float4 vPosition : SV_Position;
	float2 vTexCoord : TEXCOORD0;
	float4 vTexCoord1 : TEXCOORD1;
	float4 vTexCoord2 : TEXCOORD2;
};
VSOUTPUT main(float4 inPosition : POSITION,float2 inTEX0 : TEXCOORD0)
{
	VSOUTPUT OUT;
	OUT.vPosition = inPosition;
	OUT.vTexCoord = inTEX0;
	OUT.vTexCoord1 = inTEX0.xyyx + (float4(-0.375f,-0.125f,-0.375f, 0.125f) * dstscale.zwwz);
	OUT.vTexCoord2 = inTEX0.xyyx + (float4( 0.375f, 0.125f, 0.375f,-0.125f) * dstscale.zwwz);
	return OUT;
})hlsl";

DX11PostProcessing::DX11PostProcessing()
	: m_initialized(false), m_vertexbuffer(0x4000), m_prev_dst_height(0), m_prev_dst_width(0), m_prev_src_height(0), m_prev_src_width(0), m_prev_samples(0)
{
	const D3D11_INPUT_ELEMENT_DESC simpleelems[3] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	D3DBlob blob;
	D3D::CompileShader(D3D::ShaderType::Vertex, vertex_shader_code, blob);
	D3D::device->CreateInputLayout(simpleelems, 2, blob.Data(), blob.Size(), D3D::ToAddr(m_layout));
	m_vshader = D3D::CreateVertexShaderFromByteCode(blob);
	if (m_layout == nullptr ||
		m_vshader == nullptr)
		PanicAlert("Failed to create post processing vertex shader or input layout at %s %d\n", __FILE__, __LINE__);

	D3D::SetDebugObjectName(m_layout.get(), "post processing input layout");
	D3D::SetDebugObjectName(m_vshader.get(), "post processing vertex shader");

	unsigned int cbsize = (sizeof(paramsStruct) + 15) & (~15); // must be always multiple of 16
	D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(cbsize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	HRESULT hr = D3D::device->CreateBuffer(&cbdesc, nullptr, D3D::ToAddr(m_params));
	CHECK(hr == S_OK, "post processing constant buffer (size=%u)", cbsize);
	D3D::SetDebugObjectName(m_params.get(), "Post processing constant buffer");
	ID3D11SamplerState* samplers[] = { 
		D3D::GetLinearCopySampler(), 
		D3D::GetPointCopySampler()
	};
	D3D::context->PSSetSamplers(9, 2, samplers);
	m_vertex_buffer_observer = true;
	m_vertexbuffer.AddWrapObserver(&m_vertex_buffer_observer);
}

DX11PostProcessing::~DX11PostProcessing()
{
	for (size_t i = 0; i < m_stageOutput.size(); i++)
	{
		m_stageOutput[i]->Release();
	}
	for (auto& shader : m_pshader)
	{
		shader.reset();
	}
	m_pshader.clear();
	m_vshader.reset();
	m_layout.reset();
	m_params.reset();
	m_options.reset();
}

void DX11PostProcessing::UpdateConfiguration()
{
	D3D11_MAPPED_SUBRESOURCE map;
	D3D::context->Map(m_options.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
	u8* dstdata = (u8*)map.pData;
	u32 buffer_size = 0;
	for (auto& it : m_config.GetOptions())
	{
		if (it.second.m_resolve_at_compilation)
		{
			continue;
		}
		u32 needed_size = 0;
		void* source = nullptr;
		switch (it.second.m_type)
		{
		case PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_BOOL:
			needed_size = sizeof(int);
			break;
		case PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_INTEGER:
			source = it.second.m_integer_values.data();
			needed_size = (u32)(it.second.m_integer_values.size() * sizeof(int));
			break;
		case PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_FLOAT:
			source = it.second.m_float_values.data();
			needed_size = (u32)(it.second.m_float_values.size() * sizeof(float));
			break;
		}
		u32 remaining = ((buffer_size + 15) & (~15)) - buffer_size;
		if (remaining < needed_size && remaining > 0)
		{
			// Padding needed to compensate contant buffer padding to 16 bytes
			buffer_size += remaining;
			dstdata += remaining;
		}
		if (source != nullptr)
		{
			memcpy(dstdata, source, needed_size);
		}
		else
		{
			*((int*)dstdata) = it.second.m_bool_value ? 1 : 0;
		}
		buffer_size += needed_size;
		dstdata += needed_size;
		it.second.m_dirty = false;
	}
	D3D::context->Unmap(m_options.get(), 0);
	m_config.SetDirty(false);
}

void DX11PostProcessing::BlitFromTexture(const TargetRectangle &src, const TargetRectangle &dst,
	void* src_texture_ptr, void* src_depth_texture_ptr, int src_width, int src_height, int layer, float gamma)
{
	D3DTexture2D* src_texture = ((D3DTexture2D*)src_texture_ptr);
	D3DTexture2D* src_texture_depth = ((D3DTexture2D*)src_depth_texture_ptr);
	ApplyShader();
	
	if (m_config.HasOptions() && m_config.IsDirty())
	{
		UpdateConfiguration();
	}
	paramsStruct params;
	params.native_gamma = 1.0f / gamma;
	params.Layer = layer;
	params.Time = (u32)m_timer.GetTimeElapsed();
	params.ScalingFilter = (src.GetWidth() > dst.GetWidth() && src.GetHeight() > dst.GetHeight() && g_ActiveConfig.bUseScalingFilter) ? 1u : 0;
	float sw = 1.0f / (float)src_width;
	float sh = 1.0f / (float)src_height;
	float u0 = ((float)src.left) * sw;
	float u1 = ((float)src.right) * sw;
	float v0 = ((float)src.top) * sh;
	float v1 = ((float)src.bottom) * sh;


	params.resolution[0] = (float)src_width;
	params.resolution[1] = (float)src_height;
	params.resolution[2] = sw;
	params.resolution[3] = sh;
	params.targetscale[0] = u0;
	params.targetscale[1] = v0;
	params.targetscale[2] = 1.0f / (u1-u0);
	params.targetscale[3] = 1.0f / (v1-v0);
	params.dstscale[0] = float(dst.GetWidth());
	params.dstscale[1] = float(dst.GetHeight());
	params.dstscale[2] = 1.0f / params.dstscale[0];
	params.dstscale[3] = 1.0f / params.dstscale[1];

	D3D11_MAPPED_SUBRESOURCE map;
	D3D::context->Map(m_params.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
	memcpy(map.pData, &params, sizeof(params));
	D3D::context->Unmap(m_params.get(), 0);

	STVertex coords[4] = {
		{ -1.0f, 1.0f, 0.0f, u0, v0 },
		{ 1.0f, 1.0f, 0.0f, u1, v0 },
		{ -1.0f, -1.0f, 0.0f, u0, v1 },
		{ 1.0f, -1.0f, 0.0f, u1, v1 },
	};
	if (m_vertex_buffer_observer || pu0 != u0 || pu1 != u1 || pv0 != v0 || pv1 != v1)
	{
		m_vertex_buffer_offset = m_vertexbuffer.AppendData(coords, sizeof(coords), sizeof(STVertex));
		m_vertex_buffer_observer = false;
		pu0 = u0;
		pu1 = u1;
		pv0 = v0;
		pv1 = v1;
	}
	D3D::stateman->SetVertexConstants(m_params.get());
	D3D::stateman->SetPixelConstants(m_params.get(), m_options.get());
	D3D::stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	D3D::stateman->SetInputLayout(m_layout.get());
	D3D::stateman->SetVertexBuffer(m_vertexbuffer.GetBuffer(), sizeof(STVertex), 0);

	ID3D11ShaderResourceView* views[6] = {
		src_texture->GetSRV(),
		nullptr,
		src_texture->GetSRV(),
		nullptr,
		nullptr,
		nullptr
	};

	if (src_texture_depth != nullptr)
	{
		views[1] = src_texture_depth->GetSRV();
	}
	D3D::stateman->SetVertexShader(m_vshader.get());
	D3D::stateman->SetGeometryShader(nullptr);
	D3D::context->PSSetShaderResources(9, 3, views);
	ID3D11RenderTargetView* OutRTV = nullptr;
	const auto& stages = m_config.GetStages();
	size_t finalstage = stages.size() - 1;
	if (finalstage > 0)
	{
		D3D::context->OMGetRenderTargets(1, &OutRTV, nullptr);
		if (finalstage > 0 &&
			(m_prev_dst_width != dst.GetWidth()
			|| m_prev_dst_height != dst.GetHeight()
			|| m_prev_src_width != src.GetWidth()
			|| m_prev_src_height != src.GetHeight()
			|| m_stageOutput.size() != finalstage))
		{
			m_prev_dst_width = dst.GetWidth();
			m_prev_dst_height = dst.GetHeight();
			m_prev_src_width = src.GetWidth();
			m_prev_src_height = src.GetHeight();
			for (size_t i = 0; i < m_stageOutput.size(); i++)
			{
				m_stageOutput[i]->Release();
				m_stageOutput[i] = nullptr;
			}
			m_stageOutput.resize(finalstage);
			for (size_t i = 0; i < finalstage; i++)
			{
				const auto& stage = stages[i];
				u32 stage_width = stage.m_use_source_resolution ? m_prev_src_width : m_prev_dst_width;
				u32 stage_height = stage.m_use_source_resolution ? m_prev_src_height : m_prev_dst_height;
				stage_width = (u32)(stage_width * stage.m_outputScale);
				stage_height = (u32)(stage_height * stage.m_outputScale);
				int flags = ((int)D3D11_BIND_RENDER_TARGET | (int)D3D11_BIND_SHADER_RESOURCE);
				m_stageOutput[i] = D3DTexture2D::Create(
					stage_width,
					stage_height,
					(D3D11_BIND_FLAG)flags,
					D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM);
			}
		}
	}
	for (size_t i = 0; i < stages.size(); i++)
	{
		const auto& stage = stages[i];
		if (!stage.m_isEnabled)
		{
			continue;
		}
		D3D11_VIEWPORT vp;
		if (i == m_config.GetLastActiveStage())
		{
			if (OutRTV != nullptr)
			{
				D3D::context->OMSetRenderTargets(1, &OutRTV, nullptr);
				OutRTV->Release();
			}
			vp = CD3D11_VIEWPORT((float)dst.left, (float)dst.top, (float)dst.GetWidth(), (float)dst.GetHeight());
		}
		else
		{
			D3D::context->OMSetRenderTargets(1, &m_stageOutput[i]->GetRTV(), nullptr);
			float stage_width = (float)(stage.m_use_source_resolution ? m_prev_src_width : m_prev_dst_width);
			float stage_height = (float)(stage.m_use_source_resolution ? m_prev_src_height : m_prev_dst_height);
			stage_width = stage_width * stage.m_outputScale;
			stage_height = stage_height * stage.m_outputScale;
			vp = CD3D11_VIEWPORT(0.0f, 0.0f, stage_width, stage_height);
		}
		D3D::context->RSSetViewports(1, &vp);
		D3D::stateman->SetPixelShader(m_pshader[i].get());
		bool prev_stage_output_required = i > 0 && finalstage > 0;
		if (prev_stage_output_required)
		{
			for (size_t stageidx = 0; stageidx < stage.m_inputs.size(); stageidx++)
			{
				u32 originalidx = stage.m_inputs[stageidx];
				while (!stages[originalidx].m_isEnabled && originalidx > 0)
				{
					originalidx--;
				}
				views[2 + stageidx] = m_stageOutput[originalidx]->GetSRV();
			}
			D3D::context->PSSetShaderResources(11, UINT(stage.m_inputs.size()), &views[2]);
		}
		D3D::stateman->Apply();
		D3D::context->Draw(4, m_vertex_buffer_offset);
		if (prev_stage_output_required)
		{
			for (size_t stageidx = 0; stageidx < stage.m_inputs.size(); stageidx++)
			{
				views[2 + stageidx] = nullptr;
			}
			D3D::context->PSSetShaderResources(11, UINT(stage.m_inputs.size()), &views[2]);
		}
	}
	views[0] = nullptr;
	views[1] = nullptr;
	views[2] = nullptr;
	D3D::context->PSSetShaderResources(9, 3, views);
}

static const std::string s_hlsl_entry = "(\n"
"out float4 ocol0 : SV_Target,\n"
"in float4 frag_pos : SV_Position,\n"
"in float2 _uv0 : TEXCOORD0,\n"
"in float4 _uv1 : TEXCOORD1,\n"
"in float4 _uv2 : TEXCOORD2)\n"
"{\n"
"fragment_pos = frag_pos;\n"
"uv0 = _uv0;\n"
"uv1 = _uv1;\n"
"uv2 = _uv2;\n";

std::string DX11PostProcessing::InitStages(const std::string &code)
{
	std::string result = code;
	const auto& stages = m_config.GetStages();
	m_pshader.resize(stages.size());
	for (auto& stage : stages)
	{
		std::string glslentry = "void ";
		glslentry += stage.m_stage_entry_point;
		size_t entryPointStart = result.find(glslentry);
		if (entryPointStart == std::string::npos)
		{
			result = "";
			break;
		}
		size_t entryPointEnd = result.find("{", entryPointStart);
		if (entryPointEnd == std::string::npos)
		{
			result = "";
			break;
		}
		result = result.substr(0, entryPointStart + glslentry.size()) + s_hlsl_entry + result.substr(entryPointEnd + 1);
	}
	return result;
}

void DX11PostProcessing::ApplyShader()
{
	u32 new_samples = g_ActiveConfig.bUseXFB ? 1 : D3D::GetAAMode(g_ActiveConfig.iMultisampleMode).Count;
	// shader didn't changed
	if (m_initialized 
		&& m_config.GetShader() == g_ActiveConfig.sPostProcessingShader 
		&& m_prev_samples == new_samples
		&& !m_config.NeedRecompile())
		return;
	if (m_config.NeedRecompile())
	{
		m_config.SaveOptionsConfiguration();
		m_config.SetRecompile(false);
	}
	m_prev_samples = new_samples;
	for (auto& stageoutput : m_stageOutput)
	{
		stageoutput->Release();
		stageoutput = nullptr;
	}
	m_stageOutput.resize(0);
	for (auto& shader : m_pshader)
	{
		shader.reset();
	}
	
	// load shader code
	std::string code = m_config.LoadShader();
	code = InitStages(code);
	code = LoadShaderOptions(code);
	
	m_initialized = true;
	const auto& stages = m_config.GetStages();
	// and compile it
	for (size_t i = 0; i < stages.size(); i++)
	{
		m_pshader[i] = D3D::CompileAndCreatePixelShader(code, nullptr, stages[i].m_stage_entry_point.c_str());
		if (!m_pshader[i])
		{
			ERROR_LOG(VIDEO, "Failed to compile post-processing shader %s", m_config.GetShader().c_str());
			m_initialized = false;
			break;
		}
	}
	if (!m_initialized)
	{
		// Erro COmpilling so fallback to default
		g_Config.sPostProcessingShader.clear();
		g_ActiveConfig.sPostProcessingShader.clear();
		for (auto& shader : m_pshader)
		{
			shader.reset();
		}
		m_pshader.resize(1);
		code = LoadShaderOptions(InitStages(m_config.LoadShader()));
		m_pshader[0] = D3D::CompileAndCreatePixelShader(code);
		m_initialized = true;
	}
}

static const std::string s_hlsl_header = R"hlsl(
// Required variables
// Shouldn't be accessed directly by the PP shader
// Texture sampler
sampler samp8 : register(s8);
sampler samp9 : register(s9);
sampler samp10 : register(s10);
Texture2D Tex8 : register(t8);
Texture2DArray Tex9 : register(t9);
Texture2DArray Tex10 : register(t10);
Texture2DArray Tex11[4] : register(t11);

cbuffer ParamBuffer : register(b0) 
{
	uint Time;
	uint ScalingFilter;
	int layer;
	float native_gamma;
	float4 resolution;
	float4 targetscale;
	float4 dstscale;
}

// Globals
static float2 uv0;
static float4 uv1, uv2, fragment_pos;
// Interfacing functions
float2 GetFragmentCoord()
{
	return fragment_pos.xy;
}
float4 Sample(float2 location, int l)
{
	return Tex9.Sample(samp9, float3(location, l));
}
float4 SampleLocationOffset(float2 location, int2 offset)
{
	return Tex9.Sample(samp9, float3(location, layer), offset);
}
float2 FromSRCCoords(float2 location)
{
	return (location - targetscale.xy) * targetscale.zw;
}
float2 ToSRCCoords(float2 location)
{
	return location / targetscale.zw + targetscale.xy;
}
float4 SamplePrev(int idx, float2 location)
{
	return Tex11[idx].Sample(samp9, float3(FromSRCCoords(location), 0));
}
float4 SamplePrevLocationOffset(int idx, float2 location, int2 offset)
{
	return Tex11[idx].Sample(samp9, float3(FromSRCCoords(location), 0), offset);
}
float SampleDepth(float2 location, int l)
{
	/*float Znear = 0.001;
	float Zfar = 1.0;
	float A  = (1 - ( Zfar / Znear ))/2;
	float B = (1 + ( Zfar / Znear ))/2;*/
	float A = -499.5;
	float B =  500.5;
	float depth = 1.0 - Tex10.Sample(samp10, float3(location, l)).x;
	depth = 1.0 / (A * depth + B);
	return depth;
}
float SampleDepthLoacationOffset(float2 location, int2 offset)
{
	float A = -499.5;
	float B =  500.5;
	float depth = 1.0 - Tex10.Sample(samp10, float3(location, layer), offset).x;
	depth = 1.0 / (A * depth + B);
	return depth;
}
)hlsl";

static const std::string s_hlsl_header_MSAA = R"hlsl(
// Required variables
// Shouldn't be accessed directly by the PP shader
// Texture sampler
sampler samp8 : register(s8);
sampler samp9 : register(s9);
sampler samp10 : register(s10);
Texture2D Tex8 : register(t8);
Texture2DArray Tex9 : register(t9);
Texture2DMSArray<float4, %d> Tex10 : register(t10);
Texture2DArray Tex11[4] : register(t11);

cbuffer ParamBuffer : register(b0) 
{
	uint Time;
	uint ScalingFilter;
	int layer;
	float native_gamma;
	float4 resolution;
	float4 targetscale;
	float4 dstscale;
}

// Globals
static float2 uv0;
static float4 uv1, uv2, fragment_pos;
// Interfacing functions
float2 GetFragmentCoord()
{
	return fragment_pos.xy;
}
float4 Sample(float2 location, int l)
{
	return Tex9.Sample(samp9, float3(location, l));
}
float4 SampleLocationOffset(float2 location, int2 offset)
{
	return Tex9.Sample(samp9, float3(location, layer), offset);
}
float2 FromSRCCoords(float2 location)
{
	return (location - targetscale.xy) * targetscale.zw;
}
float2 ToSRCCoords(float2 location)
{
	return location / targetscale.zw + targetscale.xy;
}
float4 SamplePrev(int idx, float2 location)
{
	return Tex11[idx].Sample(samp9, float3(FromSRCCoords(location), 0));
}
float4 SamplePrevLocationOffset(int idx, float2 location, int2 offset)
{
	return Tex11[idx].Sample(samp9, float3(FromSRCCoords(location), 0), offset);
}
float SampleDepth(float2 location, int l)
{
	/*float Znear = 0.001;
	float Zfar = 1.0;
	float A  = (1 - ( Zfar / Znear ))/2;
	float B = (1 + ( Zfar / Znear ))/2;*/
	float A = -499.5;
	float B =  500.5;
	float depth = 1.0 - Tex10.Load(int3(int2(resolution.xy * location), l), 0).x;
	depth = 1.0 / (A * depth + B);
	return depth;
}
float SampleDepthLoacationOffset(float2 location, int2 offset)
{
	float A = -499.5;
	float B =  500.5;
	const int samples = %d;
	float depth = 1.0 - Tex10.Load(int3(int2(resolution.xy * location), layer), 0, offset).x;
	depth = 1.0 / (A * depth + B);
	return depth;
}
)hlsl";

static const std::string s_hlsl_interface = R"hlsl(
float4 Sample() 
{ 
	float4 outputcolor = Sample(uv0, layer);
	if (ScalingFilter != 0)
	{
		outputcolor += Sample(uv1.xy, layer);
		outputcolor += Sample(uv1.wz, layer);
		outputcolor += Sample(uv2.xy, layer);
		outputcolor += Sample(uv2.wz, layer);
		outputcolor *= 0.2;
	}
	return outputcolor;
}
float4 SampleOffset(int2 offset) { return SampleLocationOffset(uv0, offset); }
float4 SamplePrev() { return SamplePrev(0, uv0); }
float4 SamplePrev(int idx) { return SamplePrev(idx, uv0); }
float4 SamplePrevOffset(int2 offset) { return SamplePrevLocationOffset(0, uv0, offset); }
float4 SamplePrevOffset(int idx, int2 offset) { return SamplePrevLocationOffset(idx, uv0, offset); }
float SampleDepth() { return SampleDepth(uv0, layer); }
float SampleDepthOffset(int2 offset) { return SampleDepthLoacationOffset(uv0, offset); }
float4 SampleLocation(float2 location) { return Sample(location, layer); }
float SampleDepthLocation(float2 location) { return SampleDepth(location, layer); }
float4 SamplePrevLocation(float2 location) { return SamplePrev(0, location); }
float4 SamplePrevLocation(int idx, float2 location) { return SamplePrev(idx, location); }
float4 SampleLayer(int l) { return Sample(uv0, l); }
float SampleDepthLayer(int l) { return SampleDepth(uv0, l); }
float4 SampleFontLocation(float2 location) { return Tex8.Sample(samp8, location); }

float4 ApplyGCGamma(float4 col)
{
	return pow(col, native_gamma);
}
float2 GetResolution()
{
	return resolution.xy;
}
float2 GetInvResolution()
{
	return resolution.zw;
}
float2 GetCoordinates()
{
	return uv0;
}
uint GetTime()
{
	return Time;
}

#define SetOutput(color) ocol0 = color
#define GetOption(x) (option_##x)
#define OptionEnabled(x) (option_##x != 0)

//Random
static float global_rnd_state;

float RandomSeedfloat(float2 seed)
{
	float noise = frac(sin(dot(seed, float2(12.9898, 78.233)*2.0)) * 43758.5453);
	return noise;
}

void rnd_advance()
{
    global_rnd_state = RandomSeedfloat(uv0 + global_rnd_state);
}

uint RandomSeeduint(float2 seed)
{
	float noise = RandomSeedfloat(seed);
	return uint(noise * 0xFFFFFF);
}

void Randomize()
{
	global_rnd_state = frac(float(GetTime())*0.0001);
}

uint Rndint()
{
	rnd_advance();
	return uint(global_rnd_state * 0xFFFFFF);
}

float Rndfloat()
{
	rnd_advance();
	return global_rnd_state;
}

float2 Rndfloat2()
{
	float2 val;
	rnd_advance();
	val.x = global_rnd_state;
	rnd_advance();
	val.y = global_rnd_state;
	return val;
}

float3 Rndfloat3()
{
	float3 val;
	rnd_advance();
	val.x = global_rnd_state;
	rnd_advance();
	val.y = global_rnd_state;
	rnd_advance();
	val.z = global_rnd_state;
	return val;
}

float4 Rndfloat4()
{
	float4 val;
	rnd_advance();
	val.x = global_rnd_state;
	rnd_advance();
	val.y = global_rnd_state;
	rnd_advance();
	val.z = global_rnd_state;
	rnd_advance();
	val.w = global_rnd_state;
	return val;
}

)hlsl";

std::string DX11PostProcessing::LoadShaderOptions(const std::string& code)
{
	m_options.reset();
	std::string hlsl_options = "";
	if (m_config.HasOptions())
	{
		hlsl_options = "cbuffer OptionBuffer : register(b1) {\n";
		u32 buffer_size = 0;
		u32 paddingcount = 0;
		for (const auto& it : m_config.GetOptions())
		{
			if (it.second.m_resolve_at_compilation)
			{
				continue;
			}
			u32 needed_size = 0;
			u32 count = 1;
			u32 typeindex = 0;
			const char* dtype[] = { "int", "float" };
			if (it.second.m_type == PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_INTEGER)
			{
				count = static_cast<u32>(it.second.m_integer_values.size());
				needed_size = sizeof(int) * count;
			}
			else if (it.second.m_type == PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_FLOAT)
			{
				count = static_cast<u32>(it.second.m_float_values.size());
				typeindex = 1;
				needed_size = sizeof(float) * count;
			}
			else
			{
				// PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_BOOL
				needed_size = sizeof(int);
			}
			u32 remaining = ((buffer_size + 15) & (~15)) - buffer_size;
			if (remaining < needed_size && remaining > 0)
			{
				u32 padelements = remaining / 4;
				if (padelements < 2)
					hlsl_options += StringFromFormat("int	padding_%d;\n", paddingcount);
				else
					hlsl_options += StringFromFormat("int%d	padding_%d;\n", padelements, paddingcount);
				// Padding needed to compensate contant buffer padding to 16 bytes
				buffer_size += remaining;
				paddingcount++;
			}
			if (count < 2)
				hlsl_options += StringFromFormat("%s	option_%s;\n", dtype[typeindex], it.first.c_str());
			else
				hlsl_options += StringFromFormat("%s%d	option_%s;\n", dtype[typeindex], count, it.first.c_str());
			buffer_size += needed_size;
		}
		hlsl_options += "}\n";
		m_config.PrintCompilationTimeOptions(hlsl_options);
		if (buffer_size > 0)
		{
			buffer_size = (buffer_size + 15) & (~15);
			D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(buffer_size, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
			HRESULT hr = D3D::device->CreateBuffer(&cbdesc, nullptr, D3D::ToAddr(m_options));
			CHECK(hr == S_OK, "post processing options constant buffer (size=%u)", buffer_size);
			D3D::SetDebugObjectName(m_options.get(), "Post processing options constant buffer");
		}
		else
		{
			hlsl_options = "";
		}
	}
	int s = m_prev_samples;
	std::string header;
	if (s == 1)
	{
		header = s_hlsl_header;
	}
	else
	{
		header = StringFromFormat(s_hlsl_header_MSAA.c_str(), s);
	}
	return header + s_hlsl_interface + hlsl_options + code;
}

}  // namespace OGL