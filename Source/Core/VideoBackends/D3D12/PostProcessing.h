// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <wrl/client.h>
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DStreamBuffer.h"
#include "VideoBackends/D3D12/D3DTexture.h"
#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VideoCommon.h"

namespace DX12
{
class D3DBlob;

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
	struct RenderPassDx12Data final
	{
		D3DBlob* m_shader_blob{};
		D3D12_SHADER_BYTECODE m_shader_bytecode{};
	};
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
	const D3D12_SHADER_BYTECODE& GetVertexShader() const
	{
		return m_vertex_shader;
	}
	const D3D12_SHADER_BYTECODE& GetGeometryShader() const
	{
		return m_geometry_shader;
	}
	const D3D12_CPU_DESCRIPTOR_HANDLE GetSamplerHandle(UINT idx) const
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle;
		handle.ptr = texture_sampler_cpu_handle.ptr + idx * D3D::sampler_descriptor_size;
		return handle;
	}
protected:
	bool CreateCommonShaders();
	bool CreateUniformBuffer();

	std::unique_ptr<PostProcessingShader> CreateShader(PostProcessingShaderConfiguration* config) override;

	D3DBlob*  m_vertex_shader_blob{};
	D3D12_SHADER_BYTECODE m_vertex_shader{};
	D3DBlob*  m_geometry_shader_blob{};
	D3D12_SHADER_BYTECODE m_geometry_shader{};
	std::unique_ptr<D3DStreamBuffer> m_uniform_buffer;

	ComPtr<ID3D12DescriptorHeap> m_texture_samplers_descriptor_heap;
	D3D12_CPU_DESCRIPTOR_HANDLE texture_sampler_cpu_handle{};
};

}  // namespace
