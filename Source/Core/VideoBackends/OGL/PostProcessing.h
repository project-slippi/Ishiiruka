// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include <unordered_map>

#include "Common/GL/GLUtil.h"

#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/StreamBuffer.h"

#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VideoCommon.h"

namespace OGL
{
// PostProcessingShader comprises of all the resources needed to render a shader, including
// temporary buffers, external images, shader programs, and configurations.
class OGLPostProcessingShader final : public PostProcessingShader
{
public:
	OGLPostProcessingShader();
	~OGLPostProcessingShader();

	void MapAndUpdateConfigurationBuffer() override;
	void Draw(PostProcessor* parent,
		const TargetRectangle& dst_rect, const TargetSize& dst_size, uintptr_t dst_texture,
		const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_texture,
		uintptr_t src_depth_texture, int src_layer, float gamma) override;

private:

	struct OGLRenderPassData final
	{
		std::unique_ptr<SHADER> program;
		std::unique_ptr<SHADER> gs_program;
	};

	struct OGLBufferWrapper final
	{
		std::unique_ptr<StreamBuffer> m_uniform_buffer;
	};

	void ReleaseBindingSampler(uintptr_t binding) override;
	uintptr_t CreateBindingSampler(const PostProcessingShaderConfiguration::RenderPass::Input& input_config) override;
	void ReleasePassNativeResources(RenderPassData& pass) override;
	bool RecompileShaders() override;
};

class OGLPostProcessor final : public PostProcessor
{
public:
	OGLPostProcessor() : PostProcessor(API_OPENGL){}
	~OGLPostProcessor();

	bool Initialize() override;

	void PostProcessEFB(const TargetRectangle& target_rect, const TargetSize& target_size) override;

	void PostProcessEFBToTexture(uintptr_t dst_texture) override;

	void MapAndUpdateUniformBuffer(
		const InputTextureSizeArray& input_sizes,
		const TargetRectangle& dst_rect, const TargetSize& dst_size,
		const TargetRectangle& src_rect, const TargetSize& src_size,
		int src_layer, float gamma);

	// NOTE: Can change current render target and viewport.
	// If src_layer <0, copy all layers, otherwise, copy src_layer to layer 0.
	void CopyTexture(const TargetRectangle& dst_rect, uintptr_t dst_texture,
		const TargetRectangle& src_rect, uintptr_t src_texture,
		const TargetSize& src_size, int src_layer, bool is_depth_texture = false,
		bool force_shader_copy = false) override;

	// Shared FBOs
	GLuint GetDrawFramebuffer() const
	{
		return m_draw_framebuffer;
	}
	GLuint GetReadFramebuffer() const
	{
		return m_read_framebuffer;
	}

protected:
	std::unique_ptr<PostProcessingShader> CreateShader(PostProcessingShaderConfiguration* config) override;
	GLuint m_draw_framebuffer = 0;
	GLuint m_read_framebuffer = 0;

	std::unique_ptr<StreamBuffer> m_uniform_buffer;
};

}  // namespace
