// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "VideoBackends/OGL/FramebufferManager.h"
#include "Common/GL/GLUtil.h"
#include "VideoBackends/OGL/PostProcessing.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/SamplerCache.h"

#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{

static const char s_vertex_shader[] =
	"out vec2 uv0;\n"
	"out vec4 uv1;\n"
	"out vec4 uv2;\n"
	"uniform vec4 src_rect;\n"
	"uniform vec4 dst_scale;\n"
	"void main(void) {\n"
	"	vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);\n"
	"	gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);\n"
	"	uv0 = rawpos * src_rect.zw + src_rect.xy;\n"
	"	uv1 = uv0.xyyx + (vec4(-0.375f, -0.125f, -0.375f, 0.125f) * dst_scale.zwwz);\n"
	"	uv2 = uv0.xyyx + (vec4(0.375f, 0.125f, 0.375f, -0.125f) * dst_scale.zwwz);\n"
	"}\n";

OpenGLPostProcessing::OpenGLPostProcessing() :
	m_prev_dst_width(0),
	m_prev_dst_height(0),
	m_prev_src_width(0),
	m_prev_src_height(0),
	m_initialized(false)  
{
	CreateHeader();
}

void OpenGLPostProcessing::DestroyStageOutput()
{
	for (auto& tex : m_stageOutput)
	{
		if (tex.first)
		{
			glDeleteTextures(1, &tex.first);
			tex.first = 0;
		}

		if (tex.second)
		{
			glDeleteFramebuffers(1, &tex.second);
			tex.second = 0;
		}
	}	
}

OpenGLPostProcessing::~OpenGLPostProcessing()
{
	DestroyStageOutput();
	for (auto& shader : m_shaders)
	{
		shader.shader.Destroy();
	}
}

void OpenGLPostProcessing::BlitFromTexture(const TargetRectangle &src, const TargetRectangle &dst,
	void* src_texture_ptr, void* src_depth_texture_ptr, int src_width, int src_height, int layer, float gamma)
{
	int src_texture = *((int*)src_texture_ptr);
	int src_texture_depth = *((int*)src_depth_texture_ptr);
	ApplyShader();

	OpenGL_BindAttributelessVAO();		

	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D_ARRAY, src_texture);
	g_sampler_cache->BindLinearSampler(9);
	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_2D_ARRAY, src_texture_depth);
	g_sampler_cache->BindNearestSampler(10);
	glActiveTexture(GL_TEXTURE11);
	glBindTexture(GL_TEXTURE_2D_ARRAY, src_texture); 
	g_sampler_cache->BindLinearSampler(11);
	const auto& stages = m_config.GetStages();
	size_t finalstage = stages.size() - 1;
	if (finalstage > 0 &&
		(m_prev_dst_width != u32(dst.GetWidth())
		|| m_prev_dst_height != u32(dst.GetHeight())
		|| m_prev_src_width != u32(src.GetWidth())
		|| m_prev_src_height != u32(src.GetHeight())
		|| m_stageOutput.size() != finalstage))
	{
		m_prev_dst_width = dst.GetWidth();
		m_prev_dst_height = dst.GetHeight();
		m_prev_src_width = src.GetWidth();
		m_prev_src_height = src.GetHeight();
		DestroyStageOutput();
		m_stageOutput.resize(finalstage);
		for (size_t i = 0; i < finalstage; i++)
		{
			u32 stage_width = stages[i].m_use_source_resolution ? m_prev_src_width : m_prev_dst_width;
			u32 stage_height = stages[i].m_use_source_resolution ? m_prev_src_height : m_prev_dst_height;
			stage_width = (u32)(stage_width * stages[i].m_outputScale);
			stage_height = (u32)(stage_height * stages[i].m_outputScale);
			auto &stage_output = m_stageOutput[i];
			glGenTextures(1, &stage_output.first);
			glActiveTexture(GL_TEXTURE11);
			glBindTexture(GL_TEXTURE_2D_ARRAY, stage_output.first);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
			glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, stage_width, stage_height, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			glGenFramebuffers(1, &stage_output.second);
			FramebufferManager::SetFramebuffer(stage_output.second);
			FramebufferManager::FramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_ARRAY, stage_output.first, 0);
			glDrawBuffer(GL_COLOR_ATTACHMENT0);
		}
	}
	for (size_t i = 0; i < stages.size(); i++)
	{
		const auto& stage = stages[i];
		if (!stage.m_isEnabled)
		{
			continue;
		}
		if (i == m_config.GetLastActiveStage())
		{
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glViewport(dst.left, dst.bottom, dst.GetWidth(), dst.GetHeight());
		}
		else
		{
			FramebufferManager::SetFramebuffer(m_stageOutput[i].second);
			u32 stage_width = stage.m_use_source_resolution ? src.GetWidth() : dst.GetWidth();
			u32 stage_height = stage.m_use_source_resolution ? src.GetHeight() : dst.GetHeight();
			stage_width = (u32)(stage_width * stage.m_outputScale);
			stage_height = (u32)(stage_height * stage.m_outputScale);
			glViewport(0, 0, (GLsizei)(stage_width), (GLsizei)(stage_height));
		}
		ShaderInstance& currentshader = m_shaders[i];
		currentshader.shader.Bind();
		glUniform4f(currentshader.m_uniform_resolution, (float)src_width, (float)src_height, 1.0f / (float)src_width, 1.0f / (float)src_height);
		glUniform4f(currentshader.m_uniform_src_rect, src.left / (float)src_width, src.bottom / (float)src_height,
			src.GetWidth() / (float)src_width, src.GetHeight() / (float)src_height);
		glUniform1ui(currentshader.m_uniform_time, (GLuint)m_timer.GetTimeElapsed());
		glUniform1i(currentshader.m_uniform_layer, layer);
		glUniform1f(currentshader.m_uniform_gamma, gamma);
		glUniform4f(currentshader.m_uniform_dstscale, 
			float(dst.GetWidth()), 
			float(dst.GetHeight()), 
			1.0f / float(dst.GetWidth()), 
			1.0f / float(dst.GetHeight()));
		glUniform1i(currentshader.m_uniform_ScalingFilter, 
			(src.GetWidth() > dst.GetWidth() && src.GetHeight() > dst.GetHeight() && g_ActiveConfig.bUseScalingFilter) ? 1u : 0);
		if (m_config.IsDirty())
		{
			for (auto& it : m_config.GetOptions())
			{
				if (it.second.m_resolve_at_compilation)
				{
					continue;
				}
				if (it.second.m_dirty)
				{
					switch (it.second.m_type)
					{
					case PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_BOOL:
						glUniform1i(currentshader.m_uniform_bindings[it.first], it.second.m_bool_value);
						break;
					case PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_INTEGER:
						switch (it.second.m_integer_values.size())
						{
						case 1:
							glUniform1i(currentshader.m_uniform_bindings[it.first], it.second.m_integer_values[0]);
							break;
						case 2:
							glUniform2i(currentshader.m_uniform_bindings[it.first],
								it.second.m_integer_values[0],
								it.second.m_integer_values[1]);
							break;
						case 3:
							glUniform3i(currentshader.m_uniform_bindings[it.first],
								it.second.m_integer_values[0],
								it.second.m_integer_values[1],
								it.second.m_integer_values[2]);
							break;
						case 4:
							glUniform4i(currentshader.m_uniform_bindings[it.first],
								it.second.m_integer_values[0],
								it.second.m_integer_values[1],
								it.second.m_integer_values[2],
								it.second.m_integer_values[3]);
							break;
						}
						break;
					case PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_FLOAT:
						switch (it.second.m_float_values.size())
						{
						case 1:
							glUniform1f(currentshader.m_uniform_bindings[it.first], it.second.m_float_values[0]);
							break;
						case 2:
							glUniform2f(currentshader.m_uniform_bindings[it.first],
								it.second.m_float_values[0],
								it.second.m_float_values[1]);
							break;
						case 3:
							glUniform3f(currentshader.m_uniform_bindings[it.first],
								it.second.m_float_values[0],
								it.second.m_float_values[1],
								it.second.m_float_values[2]);
							break;
						case 4:
							glUniform4f(currentshader.m_uniform_bindings[it.first],
								it.second.m_float_values[0],
								it.second.m_float_values[1],
								it.second.m_float_values[2],
								it.second.m_float_values[3]);
							break;
						}
						break;
					}					
				}
			}			
		}		
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
				glActiveTexture(GL_TEXTURE11 + GLenum(stageidx));
				glBindTexture(GL_TEXTURE_2D_ARRAY, m_stageOutput[originalidx].first);
			}
		}
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		if (prev_stage_output_required)
		{
			for (size_t stageidx = 0; stageidx < stage.m_inputs.size(); stageidx++)
			{
				glActiveTexture(GL_TEXTURE11 + GLenum(stageidx));
				glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
			}
		}
	}
	if (m_config.IsDirty())
	{
		for (auto& it : m_config.GetOptions())
		{
			it.second.m_dirty = false;
		}
		m_config.SetDirty(false);
	}
	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	glActiveTexture(GL_TEXTURE11);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void OpenGLPostProcessing::ApplyShader()
{
	// shader didn't changed
	if (m_initialized 
		&& m_config.GetShader() == g_ActiveConfig.sPostProcessingShader
		&& !m_config.NeedRecompile())
		return;
	if (m_config.NeedRecompile())
	{
		m_config.SaveOptionsConfiguration();
		m_config.SetRecompile(false);
	}
	DestroyStageOutput();
	m_stageOutput.resize(0);
	for (auto& shader : m_shaders)
	{
		shader.shader.Destroy();
	}
	// load shader code
	std::string code = m_config.LoadShader();
	code = LoadShaderOptions(code);

	const char* vertex_shader = s_vertex_shader;

	m_initialized = true;
	const auto& stages = m_config.GetStages();
	m_shaders.resize(stages.size());
	// and compile it
	const char* macros[1];
	for (size_t i = 0; i < stages.size(); i++)
	{
		// and compile it
		std::string entry_point = "#define ";
		entry_point += stages[i].m_stage_entry_point;
		entry_point += " main\n";
		macros[0] = entry_point.c_str();
		if (!ProgramShaderCache::CompileShader(m_shaders[i].shader, vertex_shader, code.c_str(), nullptr, macros, 1))
		{
			ERROR_LOG(VIDEO, "Failed to compile post-processing shader %s", m_config.GetShader().c_str());
			m_initialized = false;
			break;
		}
	}

	// and compile it
	if (!m_initialized)
	{
		g_Config.sPostProcessingShader.clear();
		g_ActiveConfig.sPostProcessingShader.clear();
		for (auto& shader : m_shaders)
		{
			shader.shader.Destroy();
		}
		m_shaders.resize(1);
		code = m_config.LoadShader();
		code = LoadShaderOptions(code);
		ProgramShaderCache::CompileShader(m_shaders[0].shader, vertex_shader, code.c_str());
	}

	for (auto& shader : m_shaders)
	{
		shader.m_uniform_resolution = glGetUniformLocation(shader.shader.glprogid, "resolution");
		shader.m_uniform_gamma = glGetUniformLocation(shader.shader.glprogid, "native_gamma");
		shader.m_uniform_time = glGetUniformLocation(shader.shader.glprogid, "time");
		shader.m_uniform_src_rect = glGetUniformLocation(shader.shader.glprogid, "src_rect");
		shader.m_uniform_layer = glGetUniformLocation(shader.shader.glprogid, "layer");
		shader.m_uniform_dstscale = glGetUniformLocation(shader.shader.glprogid, "dst_scale");
		shader.m_uniform_ScalingFilter = glGetUniformLocation(shader.shader.glprogid, "scaling_filter");
		shader.m_uniform_bindings.clear();
		for (const auto& it : m_config.GetOptions())
		{
			std::string glsl_name = "option_" + it.first;
			shader.m_uniform_bindings[it.first] = glGetUniformLocation(shader.shader.glprogid, glsl_name.c_str());
		}
	}
	
	m_initialized = true;
}

void OpenGLPostProcessing::CreateHeader()
{
	m_glsl_header = R"GLSL(
// Required variables
// Shouldn't be accessed directly by the PP shader
// Texture sampler
SAMPLER_BINDING(8) uniform sampler2D samp8;
SAMPLER_BINDING(9) uniform sampler2DArray samp9;
SAMPLER_BINDING(10) uniform sampler2DArray samp10;
SAMPLER_BINDING(11) uniform sampler2DArray samp11;
SAMPLER_BINDING(12) uniform sampler2DArray samp12;
SAMPLER_BINDING(13) uniform sampler2DArray samp13;
SAMPLER_BINDING(14) uniform sampler2DArray samp14;

// Output variable
out float4 ocol0;
// Input coordinates
in float2 uv0;
in float4 uv1;
in float4 uv2;
// Resolution
uniform float4 resolution;
// Time
uniform uint time;
// Layer
uniform int layer;
// Gamma
uniform float native_gamma;
// Source Rect
uniform vec4 src_rect;

uniform int scaling_filter;

// Interfacing functions		
float2 GetFragmentCoord()
{
	return gl_FragCoord.xy;
}

float4 Sample(float2 location, int l)
{
	return texture(samp9, float3(location, l));
}

float4 SampleLocationOffset(float2 location, int2 offset)
{ 
	return texture(samp9, float3(location + offset * resolution.zw, layer));
}
float2 FromSRCCoords(float2 location)
{
	return (location - src_rect.xy) / src_rect.zw;
}
float2 ToSRCCoords(float2 location)
{
	return location * src_rect.zw + src_rect.xy;
}
float4 SamplePrev(int idx, float2 location)
{
	float2 newlocation = FromSRCCoords(location);
	if (idx == 0)
	{
		return texture(samp11, float3(newlocation, 0));
	}
	else if (idx == 1)
	{
		return texture(samp12, float3(newlocation, 0));
	}
	else if (idx == 2)
	{
		return texture(samp13, float3(newlocation, 0));
	}
	else
	{
		return texture(samp14, float3(newlocation, 0));
	}
}

float4 SamplePrevLocationOffset(int idx, float2 location, int2 offset)
{
	float2 newlocation = FromSRCCoords(location);
	newlocation += offset / (src_rect.zw * resolution.xy);
	if (idx == 0)
	{
		return texture(samp11, float3(newlocation, 0));
	}
	else if (idx == 1)
	{
		return texture(samp12, float3(newlocation, 0));
	}
	else if (idx == 2)
	{
		return texture(samp13, float3(newlocation, 0));
	}
	else
	{
		return texture(samp14, float3(newlocation, 0));
	}
	
}

float SampleDepth(float2 location, int l)
{
	/*float Znear = 0.001;
	float Zfar = 1.0;
	float A  = (1 - ( Zfar / Znear ))/2;
	float B = (1 + ( Zfar / Znear ))/2;*/
	float A = -499.5;
	float B =  500.5;
	float depth = texture(samp10, float3(location, l)).x;
	depth = 1.0 / (A * depth + B);
	return depth;
}

float SampleDepthRaw(float2 location, int l)
{
	float depth = texture(samp10, float3(location, l)).x;
	return depth;
}

float SampleDepthLocationOffset(float2 location, int2 offset)
{
	float A = -499.5;
	float B =  500.5;
	float depth = texture(samp10, float3(location + offset * resolution.zw, layer)).x;
	depth = 1.0 / (A * depth + B);
	return depth;	
}

float SampleDepthLocationOffsetRaw(float2 location, int2 offset)
{
	float depth = texture(samp10, float3(location + offset * resolution.zw, layer)).x;
	return depth;	
}

float4 SampleOffset(int2 offset)
{
	return texture(samp9, float3(uv0 + offset * resolution.zw, layer));
}

float4 SamplePrevOffset(int2 offset)
{
	return SamplePrevLocationOffset(0, uv0, offset);
}

float4 SamplePrevOffset(int idx, int2 offset)
{
	return SamplePrevLocationOffset(idx, uv0, offset);
}

float SampleDepthOffset(int2 offset)
{
	return SampleDepthLocationOffset(uv0, offset);
}

float SampleDepthOffsetRaw(int2 offset)
{
	return SampleDepthLocationOffsetRaw(uv0, offset);
}

float4 Sample()
{ 
	float4 outputcolor = Sample(uv0, layer);
	if (scaling_filter != 0)
	{
		outputcolor += Sample(uv1.xy, layer);
		outputcolor += Sample(uv1.wz, layer);
		outputcolor += Sample(uv2.xy, layer);
		outputcolor += Sample(uv2.wz, layer);
		outputcolor *= 0.2;
	}
	return outputcolor;
}
float4 SamplePrev(){ return SamplePrev(0, uv0); }
float4 SamplePrev(int idx){ return SamplePrev(idx, uv0); }
float SampleDepth() { return SampleDepth(uv0, layer); }
float SampleDepthRaw() { return SampleDepthRaw(uv0, layer); }
float4 SampleLocation(float2 location) { return Sample(location, layer); }
float4 SamplePrevLocation(float2 location) { return SamplePrev(0, location); }
float4 SamplePrevLocation(int idx, float2 location) { return SamplePrev(idx, location); }
float SampleDepthLocation(float2 location) { return SampleDepth(location, layer); }
float SampleDepthLocationRaw(float2 location) { return SampleDepthRaw(location, layer); }
float4 SampleLayer(int l) { return Sample(uv0, l); }
float SampleDepthLayer(int l) { return SampleDepth(uv0, l); }
float SampleDepthLayerRaw(int l) { return SampleDepthRaw(uv0, l); }
float4 SampleFontLocation(float2 location) { return texture(samp8, location); }

float4 ApplyGCGamma(float4 col)
{
	return pow(col, float4(native_gamma, native_gamma, native_gamma, native_gamma));
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
	return time;
}
void SetOutput(float4 color)
{
	ocol0 = color;
}
#define GetOption(x) (option_##x)
#define OptionEnabled(x) (option_##x != 0)
//Random
float global_rnd_state;
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

float4 GetBicubicSampleLocation(float2 location, out float4 scalingFactor, float resolutionmultiplier)
{
	float2 textureDimensions    = GetResolution() * resolutionmultiplier;
	float2 invTextureDimensions = 1.f / textureDimensions;

		location *= textureDimensions;

			float2 texelCenter   = floor( location - 0.5f ) + 0.5f;
	float2 fracOffset    = location - texelCenter;
	float2 fracOffset_x2 = fracOffset * fracOffset;
	float2 fracOffset_x3 = fracOffset * fracOffset_x2;
	float2 weight0 = fracOffset_x2 - 0.5f * ( fracOffset_x3 + fracOffset );
	float2 weight1 = 1.5f * fracOffset_x3 - 2.5f * fracOffset_x2 + 1.f;
	float2 weight3 = 0.5f * ( fracOffset_x3 - fracOffset_x2 );
	float2 weight2 = 1.f - weight0 - weight1 - weight3;

		scalingFactor = float4(weight0 + weight1,  weight2 + weight3);	
	scalingFactor = scalingFactor.xzxz * scalingFactor.yyww;
	float2 f0 = weight1 / ( weight0 + weight1 );
	float2 f1 = weight3 / ( weight2 + weight3 );

		return float4(texelCenter - 1.f + f0,texelCenter + 1.f + f1) * invTextureDimensions.xyxy;
}

float4 SampleBicubic(float2 location)
{
	float4 scalingFactor;
	float4 texCoord = GetBicubicSampleLocation(location, scalingFactor, 1.0);
	return
		SampleLocation(texCoord.xy) * scalingFactor.x +
		SampleLocation(texCoord.zy) * scalingFactor.y +
		SampleLocation(texCoord.xw) * scalingFactor.z +
		SampleLocation(texCoord.zw) * scalingFactor.w;
}

float4 SampleBicubic(float2 location, float resolutionmultiplier)
{
	float4 scalingFactor;
	float4 texCoord = GetBicubicSampleLocation(location, scalingFactor, resolutionmultiplier);
	return
		SampleLocation(texCoord.xy) * scalingFactor.x +
		SampleLocation(texCoord.zy) * scalingFactor.y +
		SampleLocation(texCoord.xw) * scalingFactor.z +
		SampleLocation(texCoord.zw) * scalingFactor.w;
}

float4 SamplePrevBicubic(float2 location)
{
	float4 scalingFactor;
	float4 texCoord = GetBicubicSampleLocation(location, scalingFactor, 1.0);
	return
		SamplePrevLocation(0, texCoord.xy) * scalingFactor.x +
		SamplePrevLocation(0, texCoord.zy) * scalingFactor.y +
		SamplePrevLocation(0, texCoord.xw) * scalingFactor.z +
		SamplePrevLocation(0, texCoord.zw) * scalingFactor.w;
}

float4 SamplePrevBicubic(float2 location, int idx, float resolutionmultiplier)
{
	float4 scalingFactor;
	float4 texCoord = GetBicubicSampleLocation(location, scalingFactor, resolutionmultiplier);
	return
		SamplePrevLocation(idx, texCoord.xy) * scalingFactor.x +
		SamplePrevLocation(idx, texCoord.zy) * scalingFactor.y +
		SamplePrevLocation(idx, texCoord.xw) * scalingFactor.z +
		SamplePrevLocation(idx, texCoord.zw) * scalingFactor.w;
}

float4 SampleBicubic() 
{ 
	float4 outputcolor = SampleBicubic(uv0);
	if (scaling_filter != 0)
	{
		outputcolor += SampleBicubic(uv1.xy);
		outputcolor += SampleBicubic(uv1.wz);
		outputcolor += SampleBicubic(uv2.xy);
		outputcolor += SampleBicubic(uv2.wz);
		outputcolor *= 0.2;
	}
	return outputcolor;
}

)GLSL";
}

std::string OpenGLPostProcessing::LoadShaderOptions(const std::string& code)
{
	std::string glsl_options = "";

	for (const auto& it : m_config.GetOptions())
	{
		if (it.second.m_resolve_at_compilation)
		{
			continue;
		}
		if (it.second.m_type == PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_BOOL)
		{
			glsl_options += StringFromFormat("uniform int     option_%s;\n", it.first.c_str());
		}
		else if (it.second.m_type == PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_INTEGER)
		{
			u32 count = static_cast<u32>(it.second.m_integer_values.size());
			if (count < 2)
				glsl_options += StringFromFormat("uniform int     option_%s;\n", it.first.c_str());
			else
				glsl_options += StringFromFormat("uniform int%d   option_%s;\n", count, it.first.c_str());
		}
		else if (it.second.m_type == PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_FLOAT)
		{
			u32 count = static_cast<u32>(it.second.m_float_values.size());
			if (count < 2)
				glsl_options += StringFromFormat("uniform float   option_%s;\n", it.first.c_str());
			else
				glsl_options += StringFromFormat("uniform float%d option_%s;\n", count, it.first.c_str());
		}
	}
	m_config.PrintCompilationTimeOptions(glsl_options);
	return m_glsl_header + glsl_options + code;
}

}  // namespace OGL
