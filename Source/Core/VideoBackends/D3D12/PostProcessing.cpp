// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DBlob.h"
#include "VideoBackends/D3D12/D3DShader.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/ShaderCache.h"
#include "VideoBackends/D3D12/StaticShaderCache.h"
#include "VideoBackends/D3D12/PostProcessing.h"
#include "VideoBackends/D3D12/Render.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/Statistics.h"

namespace DX12
{

static const u32 FIRST_INPUT_BINDING_SLOT = 9;

static const char* s_shader_common = R"(

struct VS_INPUT
{
float4 position		: POSITION;
float3 texCoord		: TEXCOORD0;
};

struct VS_OUTPUT
{
float4 position		: SV_Position;
float2 srcTexCoord	: TEXCOORD0;
float2 dstTexCoord	: TEXCOORD1;
float layer			: TEXCOORD2;
};

struct GS_OUTPUT
{
float4 position		: SV_Position;
float2 srcTexCoord	: TEXCOORD0;
float2 dstTexCoord	: TEXCOORD1;
float layer			: TEXCOORD2;
uint slice			: SV_RenderTargetArrayIndex;
};

)";

static const char* s_vertex_shader = R"(

void main(in VS_INPUT input, out VS_OUTPUT output)
{
output.position = input.position;
output.srcTexCoord = input.texCoord.xy;
output.dstTexCoord = float2(input.position.x * 0.5f + 0.5f, 1.0f - (input.position.y * 0.5f + 0.5f));
output.layer = input.texCoord.z;
}

)";

static const char* s_geometry_shader = R"(
[maxvertexcount(6)]
void main(triangle VS_OUTPUT input[3], inout TriangleStream<GS_OUTPUT> output)
{
for (int layer = 0; layer < 2; layer++)
{
for (int i = 0; i < 3; i++)
{
	GS_OUTPUT vert;
	vert.position = input[i].position;
	vert.srcTexCoord = input[i].srcTexCoord;
	vert.dstTexCoord = input[i].dstTexCoord;
	vert.layer = float(layer);
	vert.slice = layer;
	output.Append(vert);
}
output.RestartStrip();
}
}
)";

PostProcessingShader::PostProcessingShader()
{
	m_uniform_buffer.reset();
	m_uniform_buffer = std::make_unique<D3DStreamBuffer>(PostProcessor::UNIFORM_BUFFER_SIZE * 128, PostProcessor::UNIFORM_BUFFER_SIZE * 1024, nullptr);
}

PostProcessingShader::~PostProcessingShader()
{
	m_uniform_buffer.reset();
	for (RenderPassData& pass : m_passes)
	{
		for (InputBinding& input : pass.inputs)
		{
			// External textures
			 SAFE_RELEASE(input.texture);
		}
		SAFE_RELEASE(pass.output_texture);
		SAFE_RELEASE(pass.m_shader_blob);
		pass.m_shader_bytecode.pShaderBytecode = nullptr;
	}
}

D3DTexture2D* PostProcessingShader::GetLastPassOutputTexture() const
{
	return m_passes[m_last_pass_index].output_texture;
}

bool PostProcessingShader::IsLastPassScaled() const
{
	return (m_passes[m_last_pass_index].output_size != m_internal_size);
}

bool PostProcessingShader::Initialize(PostProcessingShaderConfiguration* config, int target_layers)
{
	m_internal_layers = target_layers;
	m_config = config;
	m_ready = false;

	if (!CreatePasses())
		return false;

	// Set size to zero, that way it'll always be reconfigured on first use
	m_internal_size.Set(0, 0);
	m_ready = RecompileShaders();
	return m_ready;
}

bool PostProcessingShader::Reconfigure(const TargetSize& new_size)
{
	m_ready = true;

	const bool size_changed = (m_internal_size != new_size);
	if (size_changed)
		m_ready = ResizeOutputTextures(new_size);

	// Re-link on size change due to the input pointer changes
	if (m_ready && (m_config->IsDirty() || size_changed))
		LinkPassOutputs();

	// Recompile shaders if compile-time constants have changed
	if (m_ready && m_config->IsCompileTimeConstantsDirty())
		m_ready = RecompileShaders();

	return m_ready;
}

bool PostProcessingShader::CreatePasses()
{
	m_passes.reserve(m_config->GetPasses().size());
	for (const auto& pass_config : m_config->GetPasses())
	{
		RenderPassData pass;
		pass.output_texture = nullptr;
		pass.output_scale = pass_config.output_scale;
		pass.enabled = true;
		pass.inputs.reserve(pass_config.inputs.size());

		for (const auto& input_config : pass_config.inputs)
		{
			InputBinding input;
			input.type = input_config.type;
			input.texture_unit = input_config.texture_unit;

			if (input.type == POST_PROCESSING_INPUT_TYPE_IMAGE)
			{
				// Copy the external image across all layers
				ComPtr<ID3D12Resource> pTexture;

				D3D12_RESOURCE_DESC texdesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM,
					input_config.external_image_size.width, input_config.external_image_size.height, 1, 1);

				HRESULT hr = D3D::device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC(texdesc),
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					nullptr,
					IID_PPV_ARGS(pTexture.ReleaseAndGetAddressOf())
					);

				if (FAILED(hr))
				{
					ERROR_LOG(VIDEO, "CreateTexture2D failed, hr=0x%X", hr);
					return false;
				}

				input.texture = new D3DTexture2D(
					pTexture.Get(),
					TEXTURE_BIND_FLAG_SHADER_RESOURCE,
					DXGI_FORMAT_UNKNOWN,
					DXGI_FORMAT_UNKNOWN,
					DXGI_FORMAT_UNKNOWN,
					false,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
					);

				D3D::ReplaceTexture2D(input.texture->GetTex(), input_config.external_image_data.get(), DXGI_FORMAT_R8G8B8A8_UNORM,
					input_config.external_image_size.width, input_config.external_image_size.height, input_config.external_image_size.width, 0, input.texture->GetResourceUsageState());
				input.size = input_config.external_image_size;
			}

			// If set to previous pass, but we are the first pass, use the color buffer instead.
			if (input.type == POST_PROCESSING_INPUT_TYPE_PREVIOUS_PASS_OUTPUT && m_passes.empty())
				input.type = POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER;

			

			input.sampler_index = input_config.filter * 3 + input_config.address_mode;		

			pass.inputs.push_back(std::move(input));
		}

		m_passes.push_back(std::move(pass));
	}

	return true;
}

bool PostProcessingShader::RecompileShaders()
{
	for (size_t i = 0; i < m_passes.size(); i++)
	{
		RenderPassData& pass = m_passes[i];
		const PostProcessingShaderConfiguration::RenderPass& pass_config = m_config->GetPass(i);
		std::string hlsl_source = PostProcessor::GetPassFragmentShaderSource(API_D3D11, m_config, &pass_config);
		SAFE_RELEASE(pass.m_shader_blob);
		D3D::CompilePixelShader(hlsl_source, &pass.m_shader_blob);
		if (pass.m_shader_blob == nullptr)
		{
			ERROR_LOG(VIDEO, "Failed to compile post-processing shader %s (pass %s)", m_config->GetShaderName().c_str(), pass_config.entry_point.c_str());
			m_ready = false;
			return false;
		}
		pass.m_shader_bytecode = { pass.m_shader_blob->Data(), pass.m_shader_blob->Size() };
	}
	return true;
}

bool PostProcessingShader::ResizeOutputTextures(const TargetSize& new_size)
{
	for (size_t pass_index = 0; pass_index < m_passes.size(); pass_index++)
	{
		RenderPassData& pass = m_passes[pass_index];
		const PostProcessingShaderConfiguration::RenderPass& pass_config = m_config->GetPass(pass_index);
		pass.output_size = PostProcessor::ScaleTargetSize(new_size, pass_config.output_scale);
		
		SAFE_RELEASE(pass.output_texture);
		
		ComPtr<ID3D12Resource> color_texture;
		D3D12_RESOURCE_DESC color_texture_desc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R8G8B8A8_UNORM,
			pass.output_size.width,
			pass.output_size.height,
			m_internal_layers, 1, 1 , 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
		
		HRESULT hr = D3D::device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC(color_texture_desc),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(color_texture.ReleaseAndGetAddressOf())
			);
		if (FAILED(hr))
		{
			ERROR_LOG(VIDEO, "CreateTexture2D failed, hr=0x%X", hr);
			return false;
		}
		pass.output_texture = new D3DTexture2D(
			color_texture.Get(),
			TEXTURE_BIND_FLAG_SHADER_RESOURCE | TEXTURE_BIND_FLAG_RENDER_TARGET,
			color_texture_desc.Format,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			false,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	m_internal_size = new_size;
	return true;
}

void PostProcessingShader::LinkPassOutputs()
{
	m_last_pass_index = 0;
	m_last_pass_uses_color_buffer = false;

	// Update dependant options (enable/disable passes)
	for (size_t pass_index = 0; pass_index < m_passes.size(); pass_index++)
	{
		const PostProcessingShaderConfiguration::RenderPass& pass_config = m_config->GetPass(pass_index);
		RenderPassData& pass = m_passes[pass_index];
		pass.enabled = pass_config.CheckEnabled();
		if (!pass.enabled)
			continue;
		size_t previous_pass_index = m_last_pass_index;
		m_last_pass_index = pass_index;
		m_last_pass_uses_color_buffer = false;
		for (size_t input_index = 0; input_index < pass_config.inputs.size(); input_index++)
		{
			InputBinding& input_binding = pass.inputs[input_index];
			switch (input_binding.type)
			{
			case POST_PROCESSING_INPUT_TYPE_PASS_OUTPUT:
			{
				u32 pass_output_index = pass_config.inputs[input_index].pass_output_index;
				input_binding.ext_texture = m_passes[pass_output_index].output_texture;
				input_binding.size = m_passes[pass_output_index].output_size;
			}
			break;

			case POST_PROCESSING_INPUT_TYPE_PREVIOUS_PASS_OUTPUT:
			{
				input_binding.ext_texture = m_passes[previous_pass_index].output_texture;
				input_binding.size = m_passes[previous_pass_index].output_size;
			}
			break;

			case POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER:
				m_last_pass_uses_color_buffer = true;
				break;

			default:
				break;
			}
		}
	}
}

void PostProcessingShader::MapAndUpdateConfigurationBuffer()
{
	u32 buffer_size;
	void* buffer_data = m_config->UpdateConfigurationBuffer(&buffer_size);
	if (buffer_data)
	{
		m_uniform_buffer->AllocateSpaceInBuffer(buffer_size, 256);
		memcpy(
			m_uniform_buffer->GetCPUAddressOfCurrentAllocation(),
			buffer_data,
			buffer_size);

	}
	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PS_CBV, true);
	D3D::current_command_list->SetGraphicsRootConstantBufferView(
		DESCRIPTOR_TABLE_PS_CBVTWO,
		m_uniform_buffer->GetGPUAddressOfCurrentAllocation()
		);
}

void PostProcessingShader::Draw(D3DPostProcessor* parent,
	const TargetRectangle& dst_rect, const TargetSize& dst_size, D3DTexture2D* dst_texture,
	const TargetRectangle& src_rect, const TargetSize& src_size, D3DTexture2D* src_texture,
	D3DTexture2D* src_depth_texture, int src_layer, float gamma)
{
	_dbg_assert_(VIDEO, m_ready && m_internal_size == src_size);

	// Determine whether we can skip the final copy by writing directly to the output texture, if the last pass is not scaled.
	bool skip_final_copy = !IsLastPassScaled() && (dst_texture != src_texture || !m_last_pass_uses_color_buffer);

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
		if (is_last_pass && skip_final_copy)
		{
			// The target rect may differ from the source.
			output_rect = dst_rect;
			output_size = dst_size;
			dst_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
			D3D::current_command_list->OMSetRenderTargets(1, &dst_texture->GetRTV(), FALSE, nullptr);
		}
		else
		{
			output_rect = PostProcessor::ScaleTargetRectangle(API_D3D11, src_rect, pass.output_scale);
			output_size = pass.output_size;
			pass.output_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
			D3D::current_command_list->OMSetRenderTargets(1, &pass.output_texture->GetRTV(), FALSE, nullptr);
		}

		D3D12_CPU_DESCRIPTOR_HANDLE base_sampler_cpu_handle;
		D3D12_GPU_DESCRIPTOR_HANDLE base_sampler_gpu_handle;

		DX12::D3D::sampler_descriptor_heap_mgr->AllocateGroup(&base_sampler_cpu_handle, 16, &base_sampler_gpu_handle, nullptr, true);

		D3D12_CPU_DESCRIPTOR_HANDLE group_base_texture_cpu_handle;
		D3D12_GPU_DESCRIPTOR_HANDLE group_base_texture_gpu_handle;
		// On the first texture in the group, we need to allocate the space in the descriptor heap.
		DX12::D3D::gpu_descriptor_heap_mgr->AllocateGroup(&group_base_texture_cpu_handle, 16, &group_base_texture_gpu_handle, nullptr, true);		

		// Bind inputs to pipeline
		for (size_t i = 0; i < pass.inputs.size(); i++)
		{
			const InputBinding& input = pass.inputs[i];

			D3D12_CPU_DESCRIPTOR_HANDLE textureDestDescriptor;
			D3DTexture2D* input_texture = nullptr;;
			textureDestDescriptor.ptr = group_base_texture_cpu_handle.ptr + (FIRST_INPUT_BINDING_SLOT + i) * D3D::resource_descriptor_size;

			switch (input.type)
			{
			case POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER:
				input_texture = src_texture;
				input_sizes[i] = src_size;
				break;

			case POST_PROCESSING_INPUT_TYPE_DEPTH_BUFFER:
				input_texture = src_depth_texture;				
				input_sizes[i] = src_size;
				break;

			default:
				input_texture = input.texture != nullptr ? input.texture : input.ext_texture;
				input_sizes[i] = input.size;
				break;
			}
			if (input_texture)
			{
				input_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}
			DX12::D3D::device->CopyDescriptorsSimple(
				1,
				textureDestDescriptor,
				input_texture != nullptr ? input_texture->GetSRVGPUCPUShadow() : DX12::D3D::null_srv_cpu_shadow,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);

			D3D12_CPU_DESCRIPTOR_HANDLE destinationDescriptor;
			destinationDescriptor.ptr = base_sampler_cpu_handle.ptr + (FIRST_INPUT_BINDING_SLOT + i) * D3D::sampler_descriptor_size;

			DX12::D3D::device->CopyDescriptorsSimple(
				1,
				destinationDescriptor,
				parent->GetSamplerHandle(input.sampler_index),
				D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
				);
		}		
		D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV, group_base_texture_gpu_handle);
		D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SAMPLER, base_sampler_gpu_handle);
		D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_SAMPLERS, true);
		// Set viewport based on target rect
		D3D::SetViewportAndScissor(output_rect.left, output_rect.top,
			output_rect.GetWidth(), output_rect.GetHeight());

		parent->MapAndUpdateUniformBuffer(input_sizes, output_rect, output_size, src_rect, src_size, src_layer, gamma);		
		
		// Select geometry shader based on layers
		D3D12_SHADER_BYTECODE geometry_shader = {};
		if (src_layer < 0 && m_internal_layers > 1)
			geometry_shader = parent->GetGeometryShader();

		// Draw pass
		D3D::DrawShadedTexQuad(nullptr, src_rect.AsRECT(), src_size.width, src_size.height,
			pass.m_shader_bytecode, parent->GetVertexShader(), StaticShaderCache::GetSimpleVertexShaderInputLayout(),
			geometry_shader, std::max(src_layer, 0), DXGI_FORMAT_R8G8B8A8_UNORM, false, dst_texture->GetMultisampled());
	}

	// Copy the last pass output to the target if not done already
	if (!skip_final_copy)
	{
		RenderPassData& final_pass = m_passes[m_last_pass_index];
		D3DPostProcessor::CopyTexture(dst_rect, dst_texture, output_rect, final_pass.output_texture, final_pass.output_size, src_layer, DXGI_FORMAT_R8G8B8A8_UNORM);
	}
}

D3DPostProcessor::~D3DPostProcessor()
{
	SAFE_RELEASE(m_geometry_shader_blob);
	SAFE_RELEASE(m_vertex_shader_blob);
	SAFE_RELEASE(m_color_copy_texture);
	SAFE_RELEASE(m_depth_copy_texture);
	SAFE_RELEASE(m_stereo_buffer_texture);
	m_uniform_buffer.reset();
}

bool D3DPostProcessor::Initialize()
{
	D3D::sampler_descriptor_heap_mgr->AllocateGroup(&texture_sampler_cpu_handle, 16, &texture_sampler_gpu_handle, &texture_sampler_gpu_handle_cpu_shadow);
	// Lookup tables for samplers
	static const D3D12_FILTER d3d_sampler_filters[] = { D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT };
	static const D3D12_TEXTURE_ADDRESS_MODE d3d_address_modes[] = { D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_BORDER };
	// Create sampler objects to match posible configuration values
	D3D12_SAMPLER_DESC sampler_desc =
	{
		d3d_sampler_filters[0],
		d3d_address_modes[0],
		d3d_address_modes[0],
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		0.0f, 1, D3D12_COMPARISON_FUNC_ALWAYS,
		{ 0.0f, 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f
	};
	for (size_t i = 0; i < 2; i++)
	{
		for (size_t j = 0; j < 3; j++)
		{
			sampler_desc.Filter = d3d_sampler_filters[i];
			sampler_desc.AddressU = d3d_address_modes[j];
			sampler_desc.AddressV = d3d_address_modes[j];
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle{ texture_sampler_cpu_handle.ptr + (i * 3 + j) * D3D::sampler_descriptor_size };
			D3D::device->CreateSampler(&sampler_desc, cpu_handle);
		}
	}
	// Create VS/GS
	if (!CreateCommonShaders())
		return false;

	// Uniform buffer
	if (!CreateUniformBuffer())
		return false;

	// Load the currently-configured shader (this may fail, and that's okay)
	ReloadShaders();
	return true;
}

void D3DPostProcessor::ReloadShaders()
{
	Constant empty = {};
	m_current_constants.fill(empty);
	m_reload_flag.Clear();
	m_post_processing_shaders.clear();
	m_scaling_shader.reset();
	m_stereo_shader.reset();
	m_active = false;
	
	ReloadShaderConfigs();
	D3D::command_list_mgr->ExecuteQueuedWork(true);

	if (g_ActiveConfig.bPostProcessingEnable)
		CreatePostProcessingShaders();

	CreateScalingShader();

	if (m_stereo_config)
		CreateStereoShader();

	// Set initial sizes to 0,0 to force texture creation on next draw
	m_copy_size.Set(0, 0);
	m_stereo_buffer_size.Set(0, 0);
}

bool D3DPostProcessor::CreateCommonShaders()
{
	SAFE_RELEASE(m_vertex_shader_blob);
	D3D::CompileVertexShader(std::string(s_shader_common) + std::string(s_vertex_shader), &m_vertex_shader_blob);
	
	
	SAFE_RELEASE(m_geometry_shader_blob);
	D3D::CompileGeometryShader(std::string(s_shader_common) + std::string(s_geometry_shader), &m_geometry_shader_blob);

	if (!m_vertex_shader_blob || !m_geometry_shader_blob)
	{
		SAFE_RELEASE(m_vertex_shader_blob);
		SAFE_RELEASE(m_geometry_shader_blob);
		return false;
	}
	m_vertex_shader = { m_vertex_shader_blob->Data(), m_vertex_shader_blob->Size() };
	m_geometry_shader = { m_geometry_shader_blob->Data(), m_geometry_shader_blob->Size() };
	return true;
}

bool D3DPostProcessor::CreateUniformBuffer()
{
	m_uniform_buffer.reset();
	size_t basesize = ROUND_UP(POST_PROCESSING_CONTANTS_BUFFER_SIZE, 256);
	m_uniform_buffer = std::make_unique<D3DStreamBuffer>(basesize * 128, basesize * 1024, nullptr);
	return true;
}

std::unique_ptr<PostProcessingShader> D3DPostProcessor::CreateShader(PostProcessingShaderConfiguration* config)
{
	std::unique_ptr<PostProcessingShader> shader = std::make_unique<PostProcessingShader>();
	if (!shader->Initialize(config, FramebufferManager::GetEFBLayers()))
		shader.reset();

	return shader;
}

void D3DPostProcessor::CreatePostProcessingShaders()
{
	for (const std::string& shader_name : m_shader_names)
	{
		const auto& it = m_shader_configs.find(shader_name);
		if (it == m_shader_configs.end())
			continue;

		std::unique_ptr<PostProcessingShader> shader = CreateShader(it->second.get());
		if (!shader)
		{
			ERROR_LOG(VIDEO, "Failed to initialize postprocessing shader ('%s'). This shader will be ignored.", shader_name.c_str());
			OSD::AddMessage(StringFromFormat("Failed to initialize postprocessing shader ('%s'). This shader will be ignored.", shader_name.c_str()));
			continue;
		}

		m_post_processing_shaders.push_back(std::move(shader));
	}

	// If no shaders initialized successfully, disable post processing
	m_active = !m_post_processing_shaders.empty();
	if (m_active)
	{
		DEBUG_LOG(VIDEO, "Postprocessing is enabled with %u shaders in sequence.", (u32)m_post_processing_shaders.size());
		OSD::AddMessage(StringFromFormat("Postprocessing is enabled with %u shaders in sequence.", (u32)m_post_processing_shaders.size()));
	}
}

void D3DPostProcessor::CreateScalingShader()
{
	if (!m_scaling_config)
		return;

	m_scaling_shader = std::make_unique<PostProcessingShader>();
	if (!m_scaling_shader->Initialize(m_scaling_config.get(), FramebufferManager::GetEFBLayers()))
	{
		ERROR_LOG(VIDEO, "Failed to initialize scaling shader ('%s'). Falling back to copy shader.", m_scaling_config->GetShaderName().c_str());
		OSD::AddMessage(StringFromFormat("Failed to initialize scaling shader ('%s'). Falling back to copy shader.", m_scaling_config->GetShaderName().c_str()));
		m_scaling_shader.reset();
	}
}

void D3DPostProcessor::CreateStereoShader()
{
	if (!m_stereo_config)
		return;

	m_stereo_shader = std::make_unique<PostProcessingShader>();
	if (!m_stereo_shader->Initialize(m_stereo_config.get(), FramebufferManager::GetEFBLayers()))
	{
		ERROR_LOG(VIDEO, "Failed to initialize stereoscopy shader ('%s'). Falling back to blit.", m_scaling_config->GetShaderName().c_str());
		OSD::AddMessage(StringFromFormat("Failed to initialize stereoscopy shader ('%s'). Falling back to blit.", m_scaling_config->GetShaderName().c_str()));
		m_stereo_shader.reset();
	}
}

void D3DPostProcessor::PostProcessEFBToTexture(uintptr_t dst_texture)
{
	// Apply normal post-process process, but to the EFB buffers.
	// Uses the current viewport as the "visible" region to post-process.
	TargetRectangle target_rect = { 0, 0, g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight() };
	TargetSize target_size(g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight());
	D3D12_RESOURCE_STATES prev_color_state, prev_depth_state, prev_dst_state;
	// Source and target textures, if MSAA is enabled, this needs to be resolved
	D3DTexture2D* color_texture = FramebufferManager::GetResolvedEFBColorTexture();
	prev_color_state = color_texture->GetResourceUsageState();
	D3DTexture2D* depth_texture = nullptr;
	if (m_requires_depth_buffer)
	{
		depth_texture = FramebufferManager::GetResolvedEFBDepthTexture();
		prev_depth_state = depth_texture->GetResourceUsageState();
	}
	D3DTexture2D* real_dst_texture = reinterpret_cast<D3DTexture2D*>(dst_texture);
	prev_dst_state = real_dst_texture->GetResourceUsageState();

	// Invoke post process process
	PostProcess(nullptr, nullptr, nullptr,
		target_rect, target_size, reinterpret_cast<uintptr_t>(color_texture),
		target_rect, target_size, reinterpret_cast<uintptr_t>(depth_texture), dst_texture);
	
	color_texture->TransitionToResourceState(D3D::current_command_list, prev_color_state);
	real_dst_texture->TransitionToResourceState(D3D::current_command_list, prev_dst_state);
	if (m_requires_depth_buffer)
		depth_texture->TransitionToResourceState(D3D::current_command_list, prev_depth_state);

	g_renderer->RestoreAPIState();
}

void D3DPostProcessor::PostProcessEFB(const TargetRectangle* src_rect)
{
	// Apply normal post-process process, but to the EFB buffers.
	// Uses the current viewport as the "visible" region to post-process.
	// Copied from Renderer::SetViewport
	int scissorXOff = bpmem.scissorOffset.x * 2;
	int scissorYOff = bpmem.scissorOffset.y * 2;

	float X = Renderer::EFBToScaledXf(xfmem.viewport.xOrig - xfmem.viewport.wd - scissorXOff);
	float Y = Renderer::EFBToScaledYf(xfmem.viewport.yOrig + xfmem.viewport.ht - scissorYOff);
	float Wd = Renderer::EFBToScaledXf(2.0f * xfmem.viewport.wd);
	float Ht = Renderer::EFBToScaledYf(-2.0f * xfmem.viewport.ht);
	if (Wd < 0.0f)
	{
		X += Wd;
		Wd = -Wd;
	}
	if (Ht < 0.0f)
	{
		Y += Ht;
		Ht = -Ht;
	}

	// In D3D, the viewport rectangle must fit within the render target.
	TargetRectangle target_rect;
	TargetSize target_size(g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight());
	target_rect.left = static_cast<int>((X >= 0.f) ? X : 0.f);
	target_rect.top = static_cast<int>((Y >= 0.f) ? Y : 0.f);
	target_rect.right = target_rect.left + static_cast<int>((X + Wd <= g_renderer->GetTargetWidth()) ? Wd : (g_renderer->GetTargetWidth() - X));
	target_rect.bottom = target_rect.bottom + static_cast<int>((Y + Ht <= g_renderer->GetTargetHeight()) ? Ht : (g_renderer->GetTargetHeight() - Y));
	// If efb copy target is larger than the active vieport enlarge the post proccesing area
	if (src_rect != nullptr)
	{
		target_rect.Merge(*src_rect);
	}
	
	D3D12_RESOURCE_STATES prev_color_state, prev_depth_state;
	// Source and target textures, if MSAA is enabled, this needs to be resolved
	D3DTexture2D* color_texture = FramebufferManager::GetResolvedEFBColorTexture();
	prev_color_state = color_texture->GetResourceUsageState();
	D3DTexture2D* depth_texture = nullptr;
	if (m_requires_depth_buffer)
	{
		depth_texture = FramebufferManager::GetResolvedEFBDepthTexture();
		prev_depth_state = depth_texture->GetResourceUsageState();
	}

	// Invoke post process process
	PostProcess(nullptr, nullptr, nullptr,
		target_rect, target_size, reinterpret_cast<uintptr_t>(color_texture),
		target_rect, target_size, reinterpret_cast<uintptr_t>(depth_texture));

	// Copy back to EFB buffer when multisampling is enabled
	if (g_ActiveConfig.iMultisamples > 1)
		CopyTexture(target_rect, FramebufferManager::GetEFBColorTexture(), target_rect, color_texture, target_size, -1, DXGI_FORMAT_R8G8B8A8_UNORM, true);

	color_texture->TransitionToResourceState(D3D::current_command_list, prev_color_state);
	if (m_requires_depth_buffer)
		depth_texture->TransitionToResourceState(D3D::current_command_list, prev_depth_state);

	g_renderer->RestoreAPIState();
}

void D3DPostProcessor::BlitScreen(const TargetRectangle& dst_rect, const TargetSize& dst_size, uintptr_t dst_texture,
	const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_texture, uintptr_t src_depth_texture,
	int src_layer, float gamma)
{
	const bool triguer_after_blit = ShouldTriggerAfterBlit();
	D3D12_RESOURCE_STATES prev_src_state, prev_depth_state, prev_dst_state;
	D3DTexture2D* real_dst_texture = reinterpret_cast<D3DTexture2D*>(dst_texture);
	prev_dst_state = real_dst_texture->GetResourceUsageState();
	D3DTexture2D* real_src_texture = reinterpret_cast<D3DTexture2D*>(src_texture);
	prev_src_state = real_src_texture->GetResourceUsageState();
	D3DTexture2D* real_src_depth_texture = reinterpret_cast<D3DTexture2D*>(src_depth_texture);
	if (real_src_depth_texture != nullptr)
	{
		prev_depth_state = real_src_depth_texture->GetResourceUsageState();
	}
	_dbg_assert_msg_(VIDEO, src_layer >= 0, "BlitToFramebuffer should always be called with a single source layer");

	ReconfigureScalingShader(src_size);
	ReconfigureStereoShader(dst_size);
	if (triguer_after_blit)
	{
		TargetSize buffer_size(dst_rect.GetWidth(), dst_rect.GetHeight());
		if (!ResizeCopyBuffers(buffer_size, FramebufferManager::GetEFBLayers()) ||
			!ReconfigurePostProcessingShaders(buffer_size))
		{
			ERROR_LOG(VIDEO, "Failed to update post-processor state. Disabling post processor.");
			DisablePostProcessor();
			return;
		}
	}

	// Use stereo shader if enabled, otherwise invoke scaling shader, if that is invalid, fall back to blit.
	if (m_stereo_shader)
		DrawStereoBuffers(dst_rect, dst_size, real_dst_texture, src_rect, src_size, real_src_texture, real_src_depth_texture, gamma);
	else if (triguer_after_blit)
	{
		TargetSize buffer_size(dst_rect.GetWidth(), dst_rect.GetHeight());
		TargetRectangle buffer_rect(0, 0, buffer_size.width, buffer_size.height);
		if (m_scaling_shader)
		{
			m_scaling_shader->Draw(this, buffer_rect, buffer_size, m_color_copy_texture, src_rect, src_size, real_src_texture, real_src_depth_texture, src_layer, gamma);
		}
		else
		{
			CopyTexture(buffer_rect, m_color_copy_texture, src_rect, real_src_texture, src_size, -1, DXGI_FORMAT_R8G8B8A8_UNORM, false);
		}
		PostProcess(nullptr, nullptr, nullptr, buffer_rect, buffer_size, reinterpret_cast<uintptr_t>(m_color_copy_texture), src_rect, src_size, reinterpret_cast<uintptr_t>(real_src_depth_texture), dst_texture, &dst_rect, &dst_size);
	}
	else if (m_scaling_shader)
		m_scaling_shader->Draw(this, dst_rect, dst_size, real_dst_texture, src_rect, src_size, real_src_texture, real_src_depth_texture, src_layer, gamma);
	else
		CopyTexture(dst_rect, real_dst_texture, src_rect, real_src_texture, src_size, src_layer, DXGI_FORMAT_R8G8B8A8_UNORM);

	real_src_texture->TransitionToResourceState(D3D::current_command_list, prev_src_state);
	real_dst_texture->TransitionToResourceState(D3D::current_command_list, prev_dst_state);
	if (real_src_depth_texture != nullptr)
		real_src_depth_texture->TransitionToResourceState(D3D::current_command_list, prev_depth_state);
}

void D3DPostProcessor::PostProcess(TargetRectangle* output_rect, TargetSize* output_size, uintptr_t* output_texture,
	const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_texture,
	const TargetRectangle& src_depth_rect, const TargetSize& src_depth_size, uintptr_t src_depth_texture,
	uintptr_t dst_texture, const TargetRectangle* dst_rect, const TargetSize* dst_size)
{
	if (!m_active)
		return;
	D3D12_RESOURCE_STATES prev_src_state, prev_depth_state, prev_dst_state;
	D3DTexture2D* real_src_texture = reinterpret_cast<D3DTexture2D*>(src_texture);
	D3DTexture2D* real_src_depth_texture = reinterpret_cast<D3DTexture2D*>(src_depth_texture);
	D3DTexture2D* real_dst_texture = reinterpret_cast<D3DTexture2D*>(dst_texture);
	real_dst_texture = real_dst_texture == nullptr ? real_src_texture : real_dst_texture;
	prev_dst_state = real_dst_texture->GetResourceUsageState();
	prev_src_state = real_src_texture->GetResourceUsageState();
	if (real_src_depth_texture != nullptr)
	{
		prev_depth_state = real_src_depth_texture->GetResourceUsageState();
	}
	// Setup copy buffers first, and update compile-time constants.
	TargetSize buffer_size(src_rect.GetWidth(), src_rect.GetHeight());
	if (!ResizeCopyBuffers(buffer_size, FramebufferManager::GetEFBLayers()) ||
		!ReconfigurePostProcessingShaders(buffer_size))
	{
		ERROR_LOG(VIDEO, "Failed to update post-processor state. Disabling post processor.");

		// We need to fill in an output texture if we're bailing out, so set it to the source.
		if (output_texture)
		{
			*output_rect = src_rect;
			*output_size = src_size;
			*output_texture = src_texture;
		}

		DisablePostProcessor();
		return;
	}

	// Copy only the visible region to our buffers.
	TargetRectangle buffer_rect(0, 0, buffer_size.width, buffer_size.height);
	D3DTexture2D* input_color_texture = m_color_copy_texture;
	D3DTexture2D* input_depth_texture = m_depth_copy_texture;
	// Only copy if the size is different
	if (src_size != buffer_size)
	{
		CopyTexture(buffer_rect, m_color_copy_texture, src_rect, real_src_texture, src_size, -1, DXGI_FORMAT_R8G8B8A8_UNORM, false);
	}
	else
	{
		input_color_texture = real_src_texture;
	}
	if (real_src_depth_texture != nullptr && src_depth_size != buffer_size)
	{
		// Depth buffers can only be completely CopySubresourced, so use a shader copy instead.
		CopyTexture(buffer_rect, m_depth_copy_texture, src_depth_rect, real_src_depth_texture, src_depth_size, -1, DXGI_FORMAT_R32_FLOAT, true);
	}
	else
	{
		input_depth_texture = real_src_depth_texture;
	}

	// Loop through the shader list, applying each of them in sequence.
	for (size_t shader_index = 0; shader_index < m_post_processing_shaders.size(); shader_index++)
	{
		PostProcessingShader* shader = m_post_processing_shaders[shader_index].get();

		// To save a copy, we use the output of one shader as the input to the next.
		// This works except when the last pass is scaled, as the next shader expects a full-size input, so re-use the copy buffer for this case.
		D3DTexture2D* output_color_texture = (shader->IsLastPassScaled()) ? m_color_copy_texture : shader->GetLastPassOutputTexture();

		// Last shader in the sequence? If so, write to the output texture.
		if (shader_index == (m_post_processing_shaders.size() - 1))
		{
			// Are we returning our temporary texture, or storing back to the original buffer?
			if (output_texture && !dst_texture)
			{
				// Use the same texture as if it was a previous pass, and return it.
				shader->Draw(this, buffer_rect, buffer_size, output_color_texture, buffer_rect, buffer_size, input_color_texture, input_depth_texture, -1, 1.0f);
				*output_rect = buffer_rect;
				*output_size = buffer_size;
				*output_texture = reinterpret_cast<uintptr_t>(output_color_texture);
			}
			else
			{
				// Write to The output texturre directly.
				shader->Draw(this, dst_rect != nullptr ? *dst_rect : src_rect, dst_size != nullptr ? *dst_size : src_size, real_dst_texture, buffer_rect, buffer_size, input_color_texture, input_depth_texture, -1, 1.0f);
				if (output_texture)
				{
					*output_rect = buffer_rect;
					*output_size = buffer_size;
					*output_texture = reinterpret_cast<uintptr_t>(real_dst_texture);
				}
			}
		}
		else
		{
			shader->Draw(this, buffer_rect, buffer_size, output_color_texture, buffer_rect, buffer_size, input_color_texture, input_depth_texture, -1, 1.0f);
			input_color_texture = output_color_texture;
		}
	}
	real_src_texture->TransitionToResourceState(D3D::current_command_list, prev_src_state);
	real_dst_texture->TransitionToResourceState(D3D::current_command_list, prev_dst_state);
	if (real_src_depth_texture != nullptr)
		real_src_depth_texture->TransitionToResourceState(D3D::current_command_list, prev_depth_state);
}

bool D3DPostProcessor::ResizeCopyBuffers(const TargetSize& size, int layers)
{
	HRESULT hr;
	if (m_copy_size == size && m_copy_layers == layers)
		return true;

	// reset before creating, in case it fails
	SAFE_RELEASE(m_color_copy_texture);
	SAFE_RELEASE(m_depth_copy_texture);
	m_copy_size.Set(0, 0);
	m_copy_layers = 0;

	// Copy the external image across all layers
	ComPtr<ID3D12Resource> texture;

	D3D12_RESOURCE_DESC texdesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM,
		size.width, size.height, layers, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

	hr = D3D::device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC(texdesc),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(texture.ReleaseAndGetAddressOf())
		);

	if (FAILED(hr))
	{
		ERROR_LOG(VIDEO, "CreateTexture2D failed, hr=0x%X", hr);
		return false;
	}

	m_color_copy_texture = new D3DTexture2D(
		texture.Get(),
		TEXTURE_BIND_FLAG_SHADER_RESOURCE | TEXTURE_BIND_FLAG_RENDER_TARGET,
		texdesc.Format,
		DXGI_FORMAT_UNKNOWN,
		DXGI_FORMAT_UNKNOWN,
		false,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE 
		);

	texdesc.Format = DXGI_FORMAT_R32_FLOAT;

	hr = D3D::device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC(texdesc),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(texture.ReleaseAndGetAddressOf())
		);
	
	if (FAILED(hr))
	{
		ERROR_LOG(VIDEO, "CreateTexture2D failed, hr=0x%X", hr);
		return false;
	}

	m_depth_copy_texture = new D3DTexture2D(
		texture.Get(),
		TEXTURE_BIND_FLAG_SHADER_RESOURCE | TEXTURE_BIND_FLAG_RENDER_TARGET,
		texdesc.Format,
		DXGI_FORMAT_UNKNOWN,
		DXGI_FORMAT_UNKNOWN,
		false,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		);

	m_copy_size = size;
	m_copy_layers = layers;
	return true;
}

bool D3DPostProcessor::ResizeStereoBuffer(const TargetSize& size)
{
	if (m_stereo_buffer_size == size)
		return true;

	SAFE_RELEASE(m_stereo_buffer_texture);
	m_stereo_buffer_size.Set(0, 0);

	ComPtr<ID3D12Resource> stereo_buffer_texture;
	D3D12_RESOURCE_DESC texdesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM,
		size.width, size.height, 2, 1, 1, 0 , D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

	HRESULT hr = D3D::device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC(texdesc),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(stereo_buffer_texture.ReleaseAndGetAddressOf())
		);
	
	if (FAILED(hr))
	{
		ERROR_LOG(VIDEO, "CreateTexture2D failed, hr=0x%X", hr);
		return false;
	}

	m_stereo_buffer_size = size;
	m_stereo_buffer_texture = new D3DTexture2D(
		stereo_buffer_texture.Get(),
		TEXTURE_BIND_FLAG_SHADER_RESOURCE | TEXTURE_BIND_FLAG_RENDER_TARGET,
		texdesc.Format,
		DXGI_FORMAT_UNKNOWN,
		DXGI_FORMAT_UNKNOWN,
		false,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		);
	return true;
}

bool D3DPostProcessor::ReconfigurePostProcessingShaders(const TargetSize& size)
{
	for (const auto& shader : m_post_processing_shaders)
	{
		if (!shader->IsReady() || !shader->Reconfigure(size))
			return false;
	}

	// Remove dirty flags afterwards. This is because we can have the same shader twice.
	for (auto& it : m_shader_configs)
		it.second->ClearDirty();

	return true;
}

bool D3DPostProcessor::ReconfigureScalingShader(const TargetSize& size)
{
	if (m_scaling_shader)
	{
		if (!m_scaling_shader->IsReady() ||
			!m_scaling_shader->Reconfigure(size))
		{
			m_scaling_shader.reset();
			return false;
		}

		m_scaling_config->ClearDirty();
	}

	return true;
}

bool D3DPostProcessor::ReconfigureStereoShader(const TargetSize& size)
{
	if (m_stereo_shader)
	{
		if (!m_stereo_shader->IsReady() ||
			!m_stereo_shader->Reconfigure(size) ||
			!ResizeStereoBuffer(size))
		{
			m_stereo_shader.reset();
			return false;
		}

		m_stereo_config->ClearDirty();
	}

	return true;
}

void D3DPostProcessor::DrawStereoBuffers(const TargetRectangle& dst_rect, const TargetSize& dst_size, D3DTexture2D* dst_texture,
	const TargetRectangle& src_rect, const TargetSize& src_size, D3DTexture2D* src_texture, D3DTexture2D* src_depth_texture, float gamma)
{
	const bool triguer_after_blit = ShouldTriggerAfterBlit();
	D3DTexture2D* stereo_buffer = (m_scaling_shader || triguer_after_blit) ? m_stereo_buffer_texture : src_texture;
	TargetRectangle stereo_buffer_rect(src_rect);
	TargetSize stereo_buffer_size(src_size);

	// Apply scaling shader if enabled, otherwise just use the source buffers
	if (triguer_after_blit)
	{
		stereo_buffer_rect = TargetRectangle(0, 0, dst_size.width, dst_size.height);
		stereo_buffer_size = dst_size;
		TargetSize buffer_size(dst_rect.GetWidth(), dst_rect.GetHeight());
		TargetRectangle buffer_rect(0, 0, buffer_size.width, buffer_size.height);
		if (m_scaling_shader)
		{
			m_scaling_shader->Draw(this, buffer_rect, buffer_size, m_color_copy_texture, src_rect, src_size, src_texture, src_depth_texture, -1, gamma);
		}
		else
		{
			CopyTexture(buffer_rect, m_color_copy_texture, src_rect, src_texture, src_size, -1, DXGI_FORMAT_R8G8B8A8_UNORM, false);
		}
		PostProcess(nullptr, nullptr, nullptr, buffer_rect, buffer_size, reinterpret_cast<uintptr_t>(m_color_copy_texture), src_rect, src_size, reinterpret_cast<uintptr_t>(src_depth_texture), reinterpret_cast<uintptr_t>(stereo_buffer), &stereo_buffer_rect, &stereo_buffer_size);
	}
	else if (m_scaling_shader)
	{
		stereo_buffer_rect = TargetRectangle(0, 0, dst_size.width, dst_size.height);
		stereo_buffer_size = dst_size;
		m_scaling_shader->Draw(this, stereo_buffer_rect, stereo_buffer_size, stereo_buffer, src_rect, src_size, src_texture, src_depth_texture, -1, gamma);
	}

	m_stereo_shader->Draw(this, dst_rect, dst_size, dst_texture, stereo_buffer_rect, stereo_buffer_size, stereo_buffer, nullptr, 0, 1.0f);
}

void D3DPostProcessor::DisablePostProcessor()
{
	m_post_processing_shaders.clear();
	m_active = false;
}

void D3DPostProcessor::MapAndUpdateUniformBuffer(
	const InputTextureSizeArray& input_sizes,
	const TargetRectangle& dst_rect, const TargetSize& dst_size,
	const TargetRectangle& src_rect, const TargetSize& src_size,
	int src_layer, float gamma)
{
	// Skip writing to buffer if there were no changes
	if (UpdateConstantUniformBuffer(API_D3D11, input_sizes, dst_rect, dst_size, src_rect, src_size, src_layer, gamma))
	{
		m_uniform_buffer->AllocateSpaceInBuffer(POST_PROCESSING_CONTANTS_BUFFER_SIZE, 256);
		memcpy(
			m_uniform_buffer->GetCPUAddressOfCurrentAllocation(),
			m_current_constants.data(),
			POST_PROCESSING_CONTANTS_BUFFER_SIZE);

	}
	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PS_CBV, true);
	D3D::current_command_list->SetGraphicsRootConstantBufferView(
		DESCRIPTOR_TABLE_PS_CBVONE,
		m_uniform_buffer->GetGPUAddressOfCurrentAllocation()
		);
}

void D3DPostProcessor::CopyTexture(const TargetRectangle& dst_rect, D3DTexture2D* dst_texture,
	const TargetRectangle& src_rect, D3DTexture2D* src_texture,
	const TargetSize& src_size, int src_layer, DXGI_FORMAT fmt,
	bool force_shader_copy)
{
	D3D12_RESOURCE_STATES src_state = src_texture->GetResourceUsageState();
	D3D12_RESOURCE_STATES dst_state = dst_texture->GetResourceUsageState();
	// If the dimensions are the same, we can copy instead of using a shader.
	bool scaling = (dst_rect.GetWidth() != src_rect.GetWidth() || dst_rect.GetHeight() != src_rect.GetHeight());
	if (!scaling && !force_shader_copy)
	{
		D3D12_BOX srcbox = {
			static_cast<UINT>(src_rect.left),
			static_cast<UINT>(src_rect.top),
			0,
			static_cast<UINT>(src_rect.right),
			static_cast<UINT>(src_rect.bottom),
			1};
		
		D3D12_TEXTURE_COPY_LOCATION dst = CD3DX12_TEXTURE_COPY_LOCATION(dst_texture->GetTex(), 0);
		D3D12_TEXTURE_COPY_LOCATION src = CD3DX12_TEXTURE_COPY_LOCATION(src_texture->GetTex(), 0);

		dst_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_DEST);
		src_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);

		if (src_layer < 0)
		{
			// Copy all layers
			for (unsigned int layer = 0; layer < FramebufferManager::GetEFBLayers(); layer++)
			{
				src.SubresourceIndex = D3D12CalcSubresource(0, layer, 0, 1, FramebufferManager::GetEFBLayers());
				dst.SubresourceIndex = src.SubresourceIndex;
				D3D::current_command_list->CopyTextureRegion(&dst, dst_rect.left, dst_rect.top, 0, &src, &srcbox);
			}
		}
		else
		{
			// Copy single layer to layer 0
			D3D::current_command_list->CopyTextureRegion(&dst, dst_rect.left, dst_rect.top, 0, &src, &srcbox);
		}
	}
	else
	{
		D3D::SetViewportAndScissor(dst_rect.left, dst_rect.top, dst_rect.GetWidth(), dst_rect.GetHeight());
		dst_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
		D3D::current_command_list->OMSetRenderTargets(1, &dst_texture->GetRTV(), FALSE, nullptr);
		
		if (scaling)
			D3D::SetLinearCopySampler();
		else
			D3D::SetPointCopySampler();

		D3D12_SHADER_BYTECODE bytecode = {};

		D3D::DrawShadedTexQuad(src_texture, src_rect.AsRECT(), src_size.width, src_size.height,
			StaticShaderCache::GetColorCopyPixelShader(false),
			StaticShaderCache::GetSimpleVertexShader(),
			StaticShaderCache::GetSimpleVertexShaderInputLayout(), 
			(src_layer < 0) ? StaticShaderCache::GetCopyGeometryShader() : bytecode, 0,
			fmt, false, dst_texture->GetMultisampled());
	}
	dst_texture->TransitionToResourceState(D3D::current_command_list, dst_state);
	src_texture->TransitionToResourceState(D3D::current_command_list, src_state);
}

}  // namespace DX12
