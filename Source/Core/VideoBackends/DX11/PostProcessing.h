// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>

#include <string>
#include <unordered_map>

#include <wrl/client.h>
#include "VideoBackends/DX11/D3DPtr.h"
#include "VideoBackends/DX11/D3DTexture.h"
#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VideoCommon.h"

namespace DX11
{

// Forward declaration needed for PostProcessingShader::Draw()
class D3DPostProcessor;

// PostProcessingShader comprises of all the resources needed to render a shader, including
// temporary buffers, external images, shader programs, and configurations.
class PostProcessingShader final
{
public:
	PostProcessingShader() = default;
	~PostProcessingShader();

	D3DTexture2D* GetLastPassOutputTexture();

	bool IsReady() const { return m_ready; }

	bool Initialize(const PostProcessingShaderConfiguration* config, int target_layers);
	bool ResizeIntermediateBuffers(int target_width, int target_height);

	bool UpdateOptions(bool force = false);
	bool RecompileShaders();

	void Draw(D3DPostProcessor* parent,
		const TargetRectangle& target_rect, D3DTexture2D* target_texture,
		const TargetRectangle& src_rect, int src_width, int src_height,
		D3DTexture2D* src_texture, D3DTexture2D* src_depth_texture,
		int src_layer, float gamma = 1.0f);

private:
	struct InputBinding final
	{
		PostProcessingInputType type;
		u32 texture_unit;
		int width;
		int height;

		D3DTexture2D* texture;	// only set for external images
		ID3D11ShaderResourceView* texture_srv;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> texture_sampler;
	};

	struct RenderPassData final
	{
		D3D::PixelShaderPtr pixel_shader;

		std::vector<InputBinding> inputs;

		D3DTexture2D* output_texture;
		int output_width;
		int output_height;
		float output_scale;

		bool enabled;
	};

	const PostProcessingShaderConfiguration* m_config;

	int m_internal_width = 0;
	int m_internal_height = 0;
	int m_internal_layers = 0;

	std::vector<RenderPassData> m_passes;
	size_t m_last_pass_index = 0;
	bool m_last_pass_uses_color_buffer = false;
	bool m_ready = false;
};

class D3DPostProcessor final : public PostProcessor
{
public:
	D3DPostProcessor() = default;
	~D3DPostProcessor();

	bool Initialize() override;

	void ReloadShaders() override;

	void PostProcessEFB() override;

	void BlitToFramebuffer(const TargetRectangle& dst, uintptr_t dst_texture,
		const TargetRectangle& src, uintptr_t src_texture, uintptr_t src_depth_texture,
		int src_width, int src_height, int src_layer, float gamma) override;

	void PostProcess(const TargetRectangle& visible_rect, int tex_width, int tex_height, int tex_layers,
		uintptr_t texture, uintptr_t depth_texture) override;

	void MapAndUpdateUniformBuffer(const PostProcessingShaderConfiguration* config,
		int input_resolutions[POST_PROCESSING_MAX_TEXTURE_INPUTS][2],
		const TargetRectangle& src_rect, const TargetRectangle& dst_rect, int src_width, int src_height, int src_layer, float gamma);

	// NOTE: Can change current render target and viewport
	// If src_layer <0, copy all layers, otherwise, copy src_layer to layer 0.
	static void CopyTexture(const TargetRectangle& dst_rect, D3DTexture2D* dst_texture,
		const TargetRectangle& src_rect, D3DTexture2D* src_texture,
		int src_width, int src_height, int src_layer,
		bool force_shader_copy = false);

protected:
	bool ResizeCopyBuffers(int width, int height, int layers);

	D3D::BufferPtr m_uniform_buffer;

	std::unique_ptr<PostProcessingShader> m_blit_shader;
	std::vector<std::unique_ptr<PostProcessingShader>> m_post_processing_shaders;

	int m_copy_width = 0;
	int m_copy_height = 0;
	int m_copy_layers = 0;
	D3DTexture2D* m_color_copy_texture = nullptr;
	D3DTexture2D* m_depth_copy_texture = nullptr;
};

}  // namespace
