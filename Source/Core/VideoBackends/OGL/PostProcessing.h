// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <unordered_map>

#include "Common/GL/GLUtil.h"

#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/StreamBuffer.h"

#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VideoCommon.h"

namespace OGL
{

// Forward declaration needed for PostProcessingShader::Draw()
class OGLPostProcessor;

// PostProcessingShader comprises of all the resources needed to render a shader, including
// temporary buffers, external images, shader programs, and configurations.
class PostProcessingShader final
{
public:
	PostProcessingShader() = default;
	~PostProcessingShader();

	bool IsReady() const { return m_ready; }

	bool Initialize(const PostProcessingShaderConfiguration* config, int target_layers);
	bool ResizeIntermediateBuffers(int target_width, int target_height);
	
	bool RecompileShaders();
	void UpdateEnabledPasses();

	void Draw(OGLPostProcessor* parent,
		const TargetRectangle& target_rect, GLuint target_texture,
		const TargetRectangle& src_rect, int src_width, int src_height,
		GLuint src_texture, GLuint src_depth_texture,
		int src_layer, float gamma = 1.0);

private:
	struct InputBinding final
	{
		PostProcessingInputType type;
		GLuint texture_unit;
		GLuint texture_id;
		GLuint sampler_id;
		int width;
		int height;
		bool owned;
	};

	struct RenderPassData final
	{
		std::unique_ptr<SHADER> program;
		std::unique_ptr<SHADER> gs_program;

		std::vector<InputBinding> inputs;

		GLuint output_texture_id;
		int output_width;
		int output_height;
		float output_scale;

		bool enabled;
	};

	const PostProcessingShaderConfiguration* m_config;

	int m_internal_width = 0;
	int m_internal_height = 0;
	int m_internal_layers = 0;

	GLuint m_framebuffer = 0;

	std::vector<RenderPassData> m_passes;
	size_t m_last_pass_index = 0;
	bool m_last_pass_uses_color_buffer = false;
	bool m_ready = false;
};

class OGLPostProcessor final : public PostProcessor
{
public:
	OGLPostProcessor() = default;
	~OGLPostProcessor();

	bool Initialize() override;

	void ReloadShaders() override;

	void PostProcessEFB() override;

	void BlitToFramebuffer(const TargetRectangle& dst, uintptr_t dst_texture,
		const TargetRectangle& src, uintptr_t src_texture,
		int src_width, int src_height, int src_layer, float gamma) override;

	void PostProcess(const TargetRectangle& visible_rect, int tex_width, int tex_height, int tex_layers,
		uintptr_t texture, uintptr_t depth_texture) override;

	void MapAndUpdateUniformBuffer(const PostProcessingShaderConfiguration* config,
		int input_resolutions[POST_PROCESSING_MAX_TEXTURE_INPUTS][2],
		const TargetRectangle& src_rect, const TargetRectangle& dst_rect, int src_width, int src_height, int src_layer, float gamma);

	// NOTE: Can modify the bindings of draw_framebuffer/read_framebuffer.
	// If src_layer <0, copy all layers, otherwise, copy src_layer to layer 0.
	void CopyTexture(const TargetRectangle& dst_rect, GLuint dst_texture,
		const TargetRectangle& src_rect, GLuint src_texture,
		int src_layer, bool is_depth_texture,
		bool force_blit = false);

protected:
	bool ResizeCopyBuffers(int width, int height, int layers);

	GLuint m_draw_framebuffer = 0;
	GLuint m_read_framebuffer = 0;

	int m_copy_width = 0;
	int m_copy_height = 0;
	int m_copy_layers = 0;
	GLuint m_color_copy_texture = 0;
	GLuint m_depth_copy_texture = 0;

	std::unique_ptr<StreamBuffer> m_uniform_buffer;

	std::unique_ptr<PostProcessingShader> m_post_processing_shader;
	std::unique_ptr<PostProcessingShader> m_blit_shader;
};

}  // namespace
