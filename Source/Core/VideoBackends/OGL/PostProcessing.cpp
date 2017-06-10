// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Align.h"
#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Common/GL/GLUtil.h"

#include "VideoBackends/OGL/FramebufferManager.h"
#include "VideoBackends/OGL/PostProcessing.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/SamplerCache.h"
#include "VideoBackends/OGL/TextureCache.h"

#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{

static const u32 FIRST_INPUT_TEXTURE_UNIT = 9;
static const u32 UNIFORM_BUFFER_BIND_POINT = 4;

static const char* s_vertex_shader = R"(
out vec2 v_source_uv;
out vec2 v_target_uv;
flat out float v_layer;
void main(void)
{
vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);
gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);
v_source_uv = rawpos * u_source_rect.zw + u_source_rect.xy;
v_target_uv = rawpos;
v_layer = u_src_layer;
}
)";

static const char* s_layered_vertex_shader = R"(
out vec2 i_source_uv;
out vec2 i_target_uv;
void main(void)
{
vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);
gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);
i_source_uv = rawpos * u_source_rect.zw + u_source_rect.xy;
i_target_uv = rawpos;
}
)";

static const char* s_geometry_shader = R"(

	layout(triangles) in;
layout(triangle_strip, max_vertices = %d) out;

in vec2 i_source_uv[3];
in vec2 i_target_uv[3];
out vec2 v_source_uv;
out vec2 v_target_uv;
flat out float v_layer;

void main()
{
for (int i = 0; i < %d; i++)
{
	for (int j = 0; j < 3; j++)
	{
		gl_Position = gl_in[j].gl_Position;
		v_source_uv = i_source_uv[j];
		v_target_uv = i_target_uv[j];
		v_layer = float(i);
		gl_Layer = i;
		EmitVertex();
	}

			EndPrimitive();
}
}

)";

OGLPostProcessingShader::OGLPostProcessingShader()
{
	OGLBufferWrapper* wrapper = new	 OGLBufferWrapper();
	wrapper->m_uniform_buffer = StreamBuffer::Create(GL_UNIFORM_BUFFER, static_cast<u32>(PostProcessor::UNIFORM_BUFFER_SIZE * 1024));
	m_uniform_buffer = reinterpret_cast<uintptr_t>(wrapper);
}

OGLPostProcessingShader::~OGLPostProcessingShader()
{
	for (RenderPassData& pass : m_passes)
	{
		for (InputBinding& input : pass.inputs)
		{
			ReleaseBindingSampler(input.texture_sampler);
		}
		ReleasePassNativeResources(pass);
	}
	OGLBufferWrapper* buffer = reinterpret_cast<OGLBufferWrapper*>(m_uniform_buffer);
	buffer->m_uniform_buffer.release();
	delete buffer;
	m_uniform_buffer = 0;
}

void OGLPostProcessingShader::ReleasePassNativeResources(RenderPassData& pass)
{
	if (pass.shader)
	{
		OGLRenderPassData* shader = reinterpret_cast<OGLRenderPassData*>(pass.shader);
		if (shader->program != nullptr)
		{
			shader->program->Destroy();
			shader->program.reset();
		}

		if (shader->gs_program != nullptr)
		{
			shader->gs_program->Destroy();
			shader->gs_program.reset();
		}
		delete shader;
		pass.shader = 0;
	}
}

void OGLPostProcessingShader::ReleaseBindingSampler(uintptr_t sampler)
{
	GLuint samp = static_cast<GLuint>(sampler);
	glDeleteSamplers(1, &samp);
}

uintptr_t OGLPostProcessingShader::CreateBindingSampler(const PostProcessingShaderConfiguration::RenderPass::Input& input_config)
{
	// Lookup tables for samplers, simple due to no mipmaps
	static const GLenum gl_sampler_filters[] = { GL_NEAREST, GL_LINEAR };
	static const GLenum gl_sampler_modes[] = { GL_CLAMP_TO_EDGE, GL_REPEAT,  GL_CLAMP_TO_BORDER, GL_MIRRORED_REPEAT };
	static const float gl_border_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	GLuint sampler = 0;
	// Create sampler object matching the values from config
	glGenSamplers(1, &sampler);
	glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, gl_sampler_filters[input_config.filter]);
	glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, gl_sampler_filters[input_config.filter]);
	glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, gl_sampler_modes[input_config.address_mode]);
	glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, gl_sampler_modes[input_config.address_mode]);
	glSamplerParameterfv(sampler, GL_TEXTURE_BORDER_COLOR, gl_border_color);
	return static_cast<uintptr_t>(sampler);
}

bool OGLPostProcessingShader::RecompileShaders()
{
	std::string common_source = PostProcessor::GetCommonFragmentShaderSource(API_OPENGL, m_config);
	for (size_t i = 0; i < m_passes.size(); i++)
	{
		RenderPassData& pass = m_passes[i];
		const PostProcessingShaderConfiguration::RenderPass& pass_config = m_config->GetPass(i);

		int color_buffer_index = 0;
		int depth_buffer_index = 0;
		int prev_output_index = 0;

		pass_config.GetInputLocations(color_buffer_index, depth_buffer_index, prev_output_index);

		std::string header_shader_source = StringFromFormat("#define COLOR_BUFFER_INPUT_INDEX %d\n", color_buffer_index);
		header_shader_source += StringFromFormat("#define DEPTH_BUFFER_INPUT_INDEX %d\n", depth_buffer_index);
		header_shader_source += StringFromFormat("#define PREV_OUTPUT_INPUT_INDEX %d\n", prev_output_index);
		header_shader_source += "#define API_OPENGL 1\n";
		header_shader_source += "#define GLSL 1\n";
		ReleasePassNativeResources(pass);
		OGLRenderPassData* shader = new OGLRenderPassData();
		pass.shader = reinterpret_cast<uintptr_t>(shader);
		// Compile shader for this pass
		shader->program = std::make_unique<SHADER>();
		std::string vertex_shader_source;
		PostProcessor::GetUniformBufferShaderSource(API_OPENGL, m_config, vertex_shader_source);
		vertex_shader_source += s_vertex_shader;
		std::string fragment_shader_source = header_shader_source + common_source;
		fragment_shader_source += PostProcessor::GetPassFragmentShaderSource(API_OPENGL, m_config, &pass_config);
		if (!ProgramShaderCache::CompileShader(*shader->program, vertex_shader_source.c_str(), fragment_shader_source.c_str()))
		{
			ReleasePassNativeResources(pass);
			ERROR_LOG(VIDEO, "Failed to compile post-processing shader %s (pass %s)", m_config->GetShaderName().c_str(), pass_config.entry_point.c_str());
			m_ready = false;
			return false;
		}

		// Bind our uniform block
		GLuint block_index = glGetUniformBlockIndex(shader->program->glprogid, "PostProcessingConstants");
		if (block_index != GL_INVALID_INDEX)
			glUniformBlockBinding(shader->program->glprogid, block_index, UNIFORM_BUFFER_BIND_POINT);

		block_index = glGetUniformBlockIndex(shader->program->glprogid, "ConfigurationConstants");
		if (block_index != GL_INVALID_INDEX)
			glUniformBlockBinding(shader->program->glprogid, block_index, UNIFORM_BUFFER_BIND_POINT + 1);

		// Only generate a GS-expanding program if needed
		std::unique_ptr<SHADER> gs_program;
		if (m_internal_layers > 1)
		{
			shader->gs_program = std::make_unique<SHADER>();
			vertex_shader_source.clear();
			PostProcessor::GetUniformBufferShaderSource(API_OPENGL, m_config, vertex_shader_source);
			vertex_shader_source += s_layered_vertex_shader;
			std::string geometry_shader_source = StringFromFormat(s_geometry_shader, m_internal_layers * 3, m_internal_layers).c_str();

			if (!ProgramShaderCache::CompileShader(*shader->gs_program, vertex_shader_source.c_str(), fragment_shader_source.c_str(), geometry_shader_source.c_str()))
			{
				ReleasePassNativeResources(pass);
				ERROR_LOG(VIDEO, "Failed to compile GS post-processing shader %s (pass %s)", m_config->GetShaderName().c_str(), pass_config.entry_point.c_str());
				m_ready = false;
				return false;
			}

			block_index = glGetUniformBlockIndex(shader->gs_program->glprogid, "PostProcessingConstants");
			if (block_index != GL_INVALID_INDEX)
				glUniformBlockBinding(shader->gs_program->glprogid, block_index, UNIFORM_BUFFER_BIND_POINT);
		}
	}

	return true;
}

void OGLPostProcessingShader::MapAndUpdateConfigurationBuffer()
{
	// Skip writing to buffer if there were no changes
	u32 buffer_size;
	OGLBufferWrapper* buffer = reinterpret_cast<OGLBufferWrapper*>(m_uniform_buffer);
	void* buffer_data = m_config->UpdateConfigurationBuffer(&buffer_size);
	if (buffer_data)
	{
		// Annoyingly, due to latched state, we have to bind our private uniform buffer here, then restore the
		// ProgramShaderCache uniform buffer afterwards, otherwise we'll end up flushing the wrong buffer.
		glBindBuffer(GL_UNIFORM_BUFFER, buffer->m_uniform_buffer->m_buffer);

		auto mapped_buffer = buffer->m_uniform_buffer->Stream(buffer_size, ProgramShaderCache::GetUniformBufferAlignment(), buffer_data);

		glBindBufferRange(GL_UNIFORM_BUFFER, UNIFORM_BUFFER_BIND_POINT + 1, buffer->m_uniform_buffer->m_buffer, mapped_buffer, buffer_size);

		ADDSTAT(stats.thisFrame.bytesUniformStreamed, buffer_size);
	}
}

void OGLPostProcessingShader::Draw(PostProcessor* p,
	const TargetRectangle& dst_rect, const TargetSize& dst_size, uintptr_t dst_tex,
	const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_tex,
	uintptr_t src_depth_tex, int src_layer, float gamma)
{
	OGLPostProcessor* parent = reinterpret_cast<OGLPostProcessor*>(p);
	GLuint dst_texture = static_cast<GLuint>(dst_tex);
	GLuint src_texture = static_cast<GLuint>(src_tex);
	GLuint src_depth_texture = static_cast<GLuint>(src_depth_tex);

	_dbg_assert_(VIDEO, m_ready && m_internal_size == src_size);
	OpenGL_BindAttributelessVAO();

	// Determine whether we can skip the final copy by writing directly to the output texture, if the last pass is not scaled.
	bool skip_final_copy = !IsLastPassScaled() && (dst_texture != src_texture || !m_last_pass_uses_color_buffer) && !m_prev_frame_enabled;

	// If the last pass is not at full scale, we can't skip the copy.
	if (m_passes[m_last_pass_index].output_size != src_size)
		skip_final_copy = false;

	MapAndUpdateConfigurationBuffer();

	// Draw each pass.
	PostProcessor::InputTextureSizeArray input_sizes;
	TargetRectangle output_rect = {};
	TargetSize output_size;
	for (size_t pass_index = 0; pass_index < m_passes.size(); pass_index++)
	{
		const RenderPassData& pass = m_passes[pass_index];
		bool is_last_pass = (pass_index == m_last_pass_index);
		if (!pass.enabled)
			continue;
		// If this is the last pass and we can skip the final copy, write directly to output texture.
		GLuint output_texture;
		if (is_last_pass && skip_final_copy)
		{
			output_rect = dst_rect;
			output_size = dst_size;
			output_texture = dst_texture;
		}
		else
		{
			output_rect = PostProcessor::ScaleTargetRectangle(API_OPENGL, src_rect, pass.output_scale);
			output_size = pass.output_size;
			output_texture = static_cast<GLuint>(pass.output_texture->GetInternalObject());
		}

		// Setup framebuffer
		if (output_texture != 0)
		{
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, parent->GetDrawFramebuffer());
			if (src_layer < 0 && m_internal_layers > 1)
				glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, output_texture, 0);
			else if (src_layer >= 0)
				glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, output_texture, 0, src_layer);
			else
				glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, output_texture, 0, 0);
		}
		else
		{
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		}

		OGLRenderPassData* shader = reinterpret_cast<OGLRenderPassData*>(pass.shader);
		// Bind program and texture units here
		if (src_layer < 0 && m_internal_layers > 1)
			shader->gs_program->Bind();
		else
			shader->program->Bind();

		for (size_t i = 0; i < pass.inputs.size(); i++)
		{
			const InputBinding& input = pass.inputs[i];
			u32 stage = g_ActiveConfig.backend_info.bSupportsBindingLayout ? FIRST_INPUT_TEXTURE_UNIT : 0;
			stage += input.texture_unit;
			glActiveTexture(GL_TEXTURE0 + stage);

			switch (input.type)
			{
			case POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER:
				glBindTexture(GL_TEXTURE_2D_ARRAY, src_texture);
				input_sizes[i] = src_size;
				break;

			case POST_PROCESSING_INPUT_TYPE_DEPTH_BUFFER:
				glBindTexture(GL_TEXTURE_2D_ARRAY, src_depth_texture);
				input_sizes[i] = src_size;
				break;
			case POST_PROCESSING_INPUT_TYPE_PASS_FRAME_OUTPUT:
				if (m_prev_frame_enabled)
				{
					glBindTexture(GL_TEXTURE_2D_ARRAY, static_cast<GLuint>(GetPrevColorFrame(input.frame_index)->GetInternalObject()));
					input_sizes[i] = m_prev_frame_size;
				}
				break;
			case POST_PROCESSING_INPUT_TYPE_PASS_DEPTH_FRAME_OUTPUT:
				if (m_prev_depth_enabled)
				{
					glBindTexture(GL_TEXTURE_2D_ARRAY, static_cast<GLuint>(GetPrevDepthFrame(input.frame_index)->GetInternalObject()));
					input_sizes[i] = m_prev_depth_frame_size;
				}
				break;
			default:
				TextureCacheBase::TCacheEntryBase* input_texture = input.texture != nullptr ? input.texture : input.prev_texture;
				if (input_texture != nullptr)
				{
					glBindTexture(GL_TEXTURE_2D_ARRAY, static_cast<GLuint>(input_texture->GetInternalObject()));
					input_sizes[i] = input.size;
				}
				else
				{
					glBindTexture(GL_TEXTURE_2D_ARRAY, src_texture);
					input_sizes[i] = src_size;
				}
				break;
			}
			g_sampler_cache->BindExternalSampler(stage, static_cast<GLuint>(input.texture_sampler));
		}

		parent->MapAndUpdateUniformBuffer(input_sizes, output_rect, output_size, src_rect, src_size, src_layer, gamma);
		glViewport(output_rect.left, output_rect.bottom, output_rect.GetWidth(), output_rect.GetHeight());
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}

	// Unbind input textures after rendering, so that they can safely be used as outputs again.
	for (u32 i = 0; i < POST_PROCESSING_MAX_TEXTURE_INPUTS; i++)
	{
		glActiveTexture(GL_TEXTURE0 + FIRST_INPUT_TEXTURE_UNIT + i);
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	// Copy the last pass output to the target if not done already
	IncrementFrame();
	if (m_prev_depth_enabled && src_depth_tex)
	{
		TargetRectangle dst = {0, m_prev_depth_frame_size.height, m_prev_depth_frame_size.width, 0};
		parent->CopyTexture(dst, GetPrevDepthFrame(0)->GetInternalObject(), output_rect, src_depth_tex, src_size, src_layer, true, true);
	}
	if (!skip_final_copy)
	{
		RenderPassData& final_pass = m_passes[m_last_pass_index];
		if (m_prev_frame_enabled)
		{
			TargetRectangle dst = {0, m_prev_frame_size.height, m_prev_frame_size.width, 0 };
			parent->CopyTexture(dst, GetPrevColorFrame(0)->GetInternalObject(), output_rect, final_pass.output_texture->GetInternalObject(), final_pass.output_size, src_layer, false, true);
		}
		parent->CopyTexture(dst_rect, dst_texture, output_rect, final_pass.output_texture->GetInternalObject(), final_pass.output_size, src_layer);
	}
}

OGLPostProcessor::~OGLPostProcessor()
{
	if (m_read_framebuffer != 0)
		glDeleteFramebuffers(1, &m_read_framebuffer);
	if (m_draw_framebuffer != 0)
		glDeleteFramebuffers(1, &m_draw_framebuffer);

	// Need to change latched buffer before freeing uniform buffer, see MapAndUpdateUniformBuffer for why.
	if (m_uniform_buffer)
	{
		glBindBuffer(GL_UNIFORM_BUFFER, m_uniform_buffer->m_buffer);
		m_uniform_buffer.reset();
	}
}

bool OGLPostProcessor::Initialize()
{
	// Create our framebuffer objects, since these are needed regardless of whether we're enabled.
	glGenFramebuffers(1, &m_draw_framebuffer);
	glGenFramebuffers(1, &m_read_framebuffer);
	if (!(m_draw_framebuffer && m_read_framebuffer))
	{
		ERROR_LOG(VIDEO, "Failed to create postprocessing framebuffer objects.");
		return false;
	}

	m_uniform_buffer = StreamBuffer::Create(GL_UNIFORM_BUFFER, static_cast<u32>(Common::AlignUpSizePow2(POST_PROCESSING_CONTANTS_BUFFER_SIZE, 256) * 1024));
	if (m_uniform_buffer == nullptr)
	{
		ERROR_LOG(VIDEO, "Failed to create postprocessing uniform buffer.");
		return false;
	}

	// Load the currently-configured shader (this may fail, and that's okay)
	ReloadShaders();
	return true;
}

std::unique_ptr<PostProcessingShader> OGLPostProcessor::CreateShader(PostProcessingShaderConfiguration* config)
{
	std::unique_ptr<PostProcessingShader> shader;
	shader.reset(new OGLPostProcessingShader());
	if (!shader->Initialize(config, FramebufferManager::GetEFBLayers()))
		shader.reset();

	return shader;
}

void OGLPostProcessor::PostProcessEFBToTexture(uintptr_t dst_texture)
{
	// Apply normal post-process process, but to the EFB buffers.
	// Uses the current viewport as the "visible" region to post-process.
	g_renderer->ResetAPIState();

	EFBRectangle efb_rect(0, EFB_HEIGHT, EFB_WIDTH, 0);
	TargetSize target_size(g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight());
	TargetRectangle target_rect = { 0, target_size.height, target_size.width,  0};
	

	// Source and target textures, if MSAA is enabled, this needs to be resolved
	GLuint efb_color_texture = FramebufferManager::GetEFBColorTexture(efb_rect);
	GLuint efb_depth_texture = 0;
	if (m_requires_depth_buffer)
		efb_depth_texture = FramebufferManager::GetEFBDepthTexture(efb_rect);

	// Invoke post-process process, this will write back to efb_color_texture
	PostProcess(nullptr, nullptr, nullptr, target_rect, target_size, efb_color_texture, target_rect, target_size, efb_depth_texture, dst_texture);

	// Restore EFB framebuffer
	FramebufferManager::SetFramebuffer(0);

	g_renderer->RestoreAPIState();
}

void OGLPostProcessor::PostProcessEFB(const TargetRectangle& target_rect, const TargetSize& target_size)
{
	// Apply normal post-process process, but to the EFB buffers.
	// Uses the current viewport as the "visible" region to post-process.
	g_renderer->ResetAPIState();

	EFBRectangle efb_rect(0, EFB_HEIGHT, EFB_WIDTH, 0);
	
	// Source and target textures, if MSAA is enabled, this needs to be resolved
	GLuint efb_color_texture = FramebufferManager::GetEFBColorTexture(efb_rect);
	GLuint efb_depth_texture = 0;
	if (m_requires_depth_buffer)
		efb_depth_texture = FramebufferManager::GetEFBDepthTexture(efb_rect);

	// Invoke post-process process, this will write back to efb_color_texture
	PostProcess(nullptr, nullptr, nullptr, target_rect, target_size, efb_color_texture, target_rect, target_size, efb_depth_texture);

	// Restore EFB framebuffer
	FramebufferManager::SetFramebuffer(0);

	// In msaa mode, we need to blit back to the original framebuffer.
	// An accessor for the texture name means we could use CopyTexture here.
	if (g_ActiveConfig.iMultisamples > 1)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_read_framebuffer);
		FramebufferManager::FramebufferTexture(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D_ARRAY, efb_color_texture, 0);

		glBlitFramebuffer(target_rect.left, target_rect.bottom, target_rect.right, target_rect.top,
			target_rect.left, target_rect.bottom, target_rect.right, target_rect.top,
			GL_COLOR_BUFFER_BIT, GL_NEAREST);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}

	g_renderer->RestoreAPIState();
}

void OGLPostProcessor::CopyTexture(const TargetRectangle& dst_rect, uintptr_t dst_tex,
	const TargetRectangle& src_rect, uintptr_t src_tex,
	const TargetSize& src_size, int src_layer, bool is_depth_texture,
	bool force_shader_copy)
{
	GLuint dst_texture = static_cast<GLuint>(dst_tex);
	GLuint src_texture = static_cast<GLuint>(src_tex);
	// Can we copy the image?
	bool scaling = (dst_rect.GetWidth() != src_rect.GetWidth() || dst_rect.GetHeight() != src_rect.GetHeight());
	int layers_to_copy = (src_layer < 0) ? FramebufferManager::GetEFBLayers() : 1;

	// Copy each layer individually.
	for (int i = 0; i < layers_to_copy; i++)
	{
		int layer = (src_layer < 0) ? i : src_layer;
		if (g_ogl_config.bSupportsCopySubImage && dst_texture != 0 && !(force_shader_copy || scaling || is_depth_texture))
		{
			// use (ARB|NV)_copy_image, but only for non-window-framebuffer cases
			glCopyImageSubData(src_texture, GL_TEXTURE_2D_ARRAY, 0, src_rect.left, src_rect.bottom, layer,
				dst_texture, GL_TEXTURE_2D_ARRAY, 0, dst_rect.left, dst_rect.bottom, layer,
				src_rect.GetWidth(), src_rect.GetHeight(), 1);
		}
		else
		{
			glBindFramebuffer(GL_READ_FRAMEBUFFER, m_read_framebuffer);
			glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, is_depth_texture ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0, src_texture, 0, layer);

			// fallback to glBlitFramebuffer path
			GLenum filter = (scaling && !is_depth_texture) ? GL_LINEAR : GL_NEAREST;
			GLbitfield bits = (is_depth_texture) ? GL_DEPTH_BUFFER_BIT : GL_COLOR_BUFFER_BIT;
			if (dst_texture != 0)
			{
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_draw_framebuffer);
				glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, is_depth_texture ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0, dst_texture, 0, layer);
			}
			else
			{
				// window framebuffer
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			}
			glBlitFramebuffer(src_rect.left, src_rect.bottom, src_rect.right, src_rect.top,
				dst_rect.left, dst_rect.bottom, dst_rect.right, dst_rect.top,
				bits, filter);
		}
	}
}

void OGLPostProcessor::MapAndUpdateUniformBuffer(
	const InputTextureSizeArray& input_sizes,
	const TargetRectangle& dst_rect, const TargetSize& dst_size,
	const TargetRectangle& src_rect, const TargetSize& src_size,
	int src_layer, float gamma)
{
	// Skip writing to buffer if there were no changes
	if (UpdateConstantUniformBuffer(input_sizes, dst_rect, dst_size, src_rect, src_size, src_layer, gamma))
	{
		// Annoyingly, due to latched state, we have to bind our private uniform buffer here, then restore the
		// ProgramShaderCache uniform buffer afterwards, otherwise we'll end up flushing the wrong buffer.
		glBindBuffer(GL_UNIFORM_BUFFER, m_uniform_buffer->m_buffer);
		glBindBufferRange(
			GL_UNIFORM_BUFFER,
			UNIFORM_BUFFER_BIND_POINT,
			m_uniform_buffer->m_buffer,
			m_uniform_buffer->Stream(
				POST_PROCESSING_CONTANTS_BUFFER_SIZE,
				ProgramShaderCache::GetUniformBufferAlignment(),
				m_current_constants.data()),
			POST_PROCESSING_CONTANTS_BUFFER_SIZE);
		ADDSTAT(stats.thisFrame.bytesUniformStreamed, POST_PROCESSING_CONTANTS_BUFFER_SIZE);
	}
}

}  // namespace OGL
