// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <d3d11.h>
#include <string>
#include <unordered_map>
#include <wrl/client.h>

#include "VideoBackends/DX11/D3DTexture.h"
#include "VideoBackends/DX11/D3DPtr.h"
#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VideoCommon.h"

namespace DX11
{

// PostProcessingShader comprises of all the resources needed to render a shader, including
// temporary buffers, external images, shader programs, and configurations.
class D3DPostProcessingShader final : public PostProcessingShader
{
public:
	D3DPostProcessingShader();
	~D3DPostProcessingShader();
	void MapAndUpdateConfigurationBuffer() override;
	void Draw(PostProcessor* parent,
		const TargetRectangle& dst_rect, const TargetSize& dst_size, uintptr_t dst_texture,
		const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_texture,
		uintptr_t src_depth_texture, int src_layer, float gamma) override;
private:
	void ReleaseBindingSampler(uintptr_t binding) override;
	uintptr_t CreateBindingSampler(const PostProcessingShaderConfiguration::RenderPass::Input& input_config) override;
	void ReleasePassNativeResources(RenderPassData& pass) override;
	bool RecompileShaders() override;
};

class D3DPostProcessor final : public PostProcessor
{
public:
	D3DPostProcessor() : PostProcessor(API_D3D11){};
	~D3DPostProcessor();

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

	// Shadered shader stages
	ID3D11VertexShader* GetVertexShader() const
	{
		return m_vertex_shader.get();
	}
	ID3D11GeometryShader* GetGeometryShader() const
	{
		return m_geometry_shader.get();
	}

protected:
	bool CreateCommonShaders();
	bool CreateUniformBuffer();

	std::unique_ptr<PostProcessingShader> CreateShader(PostProcessingShaderConfiguration* config) override;

	D3D::VertexShaderPtr m_vertex_shader;
	D3D::GeometryShaderPtr m_geometry_shader;
	std::unique_ptr<D3D::ConstantStreamBuffer> m_uniform_buffer;

};

}  // namespace
