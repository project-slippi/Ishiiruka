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

// Forward declaration needed for PostProcessingShader::Draw()
class D3DPostProcessor;
class D3DBlob;

// PostProcessingShader comprises of all the resources needed to render a shader, including
// temporary buffers, external images, shader programs, and configurations.
class PostProcessingShader final
{
public:
	PostProcessingShader();
	~PostProcessingShader();

	D3DTexture2D* GetLastPassOutputTexture() const;
	bool IsLastPassScaled() const;

	bool IsReady() const { return m_ready; }

	bool Initialize(PostProcessingShaderConfiguration* config, int target_layers);
	bool Reconfigure(const TargetSize& new_size);
	void MapAndUpdateConfigurationBuffer();
	void Draw(D3DPostProcessor* parent,
		const TargetRectangle& dst_rect, const TargetSize& dst_size, D3DTexture2D* dst_texture,
		const TargetRectangle& src_rect, const TargetSize& src_size, D3DTexture2D* src_texture,
		D3DTexture2D* src_depth_texture, int src_layer, float gamma);

private:
	struct InputBinding final
	{
		PostProcessingInputType type{};
		u32 texture_unit{};
		TargetSize size{};

		D3DTexture2D* texture{};
		D3DTexture2D* ext_texture{};

		UINT sampler_index{};
	};

	struct RenderPassData final
	{
		D3DBlob* m_shader_blob{};
		D3D12_SHADER_BYTECODE m_shader_bytecode{};
		std::vector<InputBinding> inputs;

		D3DTexture2D* output_texture{};
		TargetSize output_size{};
		float output_scale{};

		bool enabled{};
	};

	bool CreatePasses();
	bool RecompileShaders();
	bool ResizeOutputTextures(const TargetSize& new_size);
	void LinkPassOutputs();

	PostProcessingShaderConfiguration* m_config;
	std::unique_ptr<D3DStreamBuffer> m_uniform_buffer;

	TargetSize m_internal_size;
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

	void PostProcessEFB(const TargetRectangle* src_rect) override;

	void PostProcessEFBToTexture(uintptr_t dst_texture) override;

	void BlitScreen(const TargetRectangle& dst_rect, const TargetSize& dst_size, uintptr_t dst_texture,
		const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_texture, uintptr_t src_depth_texture,
		int src_layer, float gamma) override;

	void PostProcess(TargetRectangle* output_rect, TargetSize* output_size, uintptr_t* output_texture,
		const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_texture,
		const TargetRectangle& src_depth_rect, const TargetSize& src_depth_size, uintptr_t src_depth_texture,
		uintptr_t dst_texture = 0, const TargetRectangle* dst_rect = 0, const TargetSize* dst_size = 0) override;

	void MapAndUpdateUniformBuffer(
		const InputTextureSizeArray& input_sizes,
		const TargetRectangle& dst_rect, const TargetSize& dst_size,
		const TargetRectangle& src_rect, const TargetSize& src_size,
		int src_layer, float gamma);

	// NOTE: Can change current render target and viewport.
	// If src_layer <0, copy all layers, otherwise, copy src_layer to layer 0.
	static void CopyTexture(const TargetRectangle& dst_rect, D3DTexture2D* dst_texture,
		const TargetRectangle& src_rect, D3DTexture2D* src_texture,
		const TargetSize& src_size, int src_layer, DXGI_FORMAT fmt,
		bool force_shader_copy = false);

	// Shadered shader stages
	const D3D12_SHADER_BYTECODE& GetVertexShader() const { return m_vertex_shader; }
	const D3D12_SHADER_BYTECODE& GetGeometryShader() const { return m_geometry_shader; }
	const D3D12_CPU_DESCRIPTOR_HANDLE GetSamplerHandle(UINT idx) const 
	{ 
		D3D12_CPU_DESCRIPTOR_HANDLE handle;
		handle.ptr = texture_sampler_gpu_handle_cpu_shadow.ptr + idx * D3D::sampler_descriptor_size;
		return handle;
	}
protected:
	bool CreateCommonShaders();
	bool CreateUniformBuffer();

	std::unique_ptr<PostProcessingShader> CreateShader(PostProcessingShaderConfiguration* config);
	void CreatePostProcessingShaders();
	void CreateScalingShader();
	void CreateStereoShader();

	bool ResizeCopyBuffers(const TargetSize& size, int layers);
	bool ResizeStereoBuffer(const TargetSize& size);
	bool ReconfigurePostProcessingShaders(const TargetSize& size);
	bool ReconfigureScalingShader(const TargetSize& size);
	bool ReconfigureStereoShader(const TargetSize& size);

	void DrawStereoBuffers(const TargetRectangle& dst_rect, const TargetSize& dst_size, D3DTexture2D* dst_texture,
		const TargetRectangle& src_rect, const TargetSize& src_size, D3DTexture2D* src_texture, D3DTexture2D* src_depth_texture, float gamma);

	void DisablePostProcessor();

	D3DBlob*  m_vertex_shader_blob{};
	D3D12_SHADER_BYTECODE m_vertex_shader{};
	D3DBlob*  m_geometry_shader_blob{};
	D3D12_SHADER_BYTECODE m_geometry_shader{};
	std::unique_ptr<D3DStreamBuffer> m_uniform_buffer;

	std::unique_ptr<PostProcessingShader> m_scaling_shader;
	std::unique_ptr<PostProcessingShader> m_stereo_shader;
	std::vector<std::unique_ptr<PostProcessingShader>> m_post_processing_shaders;

	TargetSize m_copy_size{};
	int m_copy_layers{};
	D3DTexture2D* m_color_copy_texture{};
	D3DTexture2D* m_depth_copy_texture{};

	TargetSize m_stereo_buffer_size{};
	D3DTexture2D* m_stereo_buffer_texture{};
	
	D3D12_CPU_DESCRIPTOR_HANDLE texture_sampler_cpu_handle{};
	D3D12_GPU_DESCRIPTOR_HANDLE texture_sampler_gpu_handle{};
	D3D12_CPU_DESCRIPTOR_HANDLE texture_sampler_gpu_handle_cpu_shadow{};
};

}  // namespace
