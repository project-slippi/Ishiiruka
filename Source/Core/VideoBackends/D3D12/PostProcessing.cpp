// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Align.h"
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

D3DPostProcessingShader::D3DPostProcessingShader()
{
	m_uniform_buffer = reinterpret_cast<uintptr_t>(new D3DStreamBuffer(PostProcessor::UNIFORM_BUFFER_SIZE * 128, PostProcessor::UNIFORM_BUFFER_SIZE * 1024, nullptr));
}

D3DPostProcessingShader::~D3DPostProcessingShader()
{
	for (RenderPassData& pass : m_passes)
	{
		ReleasePassNativeResources(pass);
	}
	D3DStreamBuffer* buffer = reinterpret_cast<D3DStreamBuffer*>(m_uniform_buffer);
	delete buffer;
	m_uniform_buffer = 0;
}

void D3DPostProcessingShader::ReleasePassNativeResources(RenderPassData& pass)
{
	if (pass.shader)
	{
		RenderPassDx12Data* pixel_shader = reinterpret_cast<RenderPassDx12Data*>(pass.shader);
		SAFE_RELEASE(pixel_shader->m_shader_blob);
		delete pixel_shader;
		pass.shader = 0;
	}
}

void D3DPostProcessingShader::ReleaseBindingSampler(uintptr_t sampler)
{

}

uintptr_t D3DPostProcessingShader::CreateBindingSampler(const PostProcessingShaderConfiguration::RenderPass::Input& input_config)
{
	return static_cast<uintptr_t>(input_config.filter * POST_PROCESSING_ADDRESS_MODE_COUNT + input_config.address_mode + 1);
}

bool D3DPostProcessingShader::RecompileShaders()
{
	static const char *inputs[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8" };
	static D3D_SHADER_MACRO macros[] = {
		{ "API_D3D", "1" },
		{ "HLSL", "1" },
		{ "COLOR_BUFFER_INPUT_INDEX", 0 },
		{ "DEPTH_BUFFER_INPUT_INDEX", 0 },
		{ "PREV_OUTPUT_INPUT_INDEX", 0 },
		{ nullptr, nullptr }
	};
	std::string common_source = PostProcessor::GetCommonFragmentShaderSource(API_D3D11, m_config, 0);
	for (size_t i = 0; i < m_passes.size(); i++)
	{
		RenderPassData& pass = m_passes[i];
		const PostProcessingShaderConfiguration::RenderPass& pass_config = m_config->GetPass(i);

		int color_buffer_index = 0;
		int depth_buffer_index = 0;
		int prev_output_index = 0;

		pass_config.GetInputLocations(color_buffer_index, depth_buffer_index, prev_output_index);

		macros[2].Definition = inputs[color_buffer_index];
		macros[3].Definition = inputs[depth_buffer_index];
		macros[4].Definition = inputs[prev_output_index];

		std::string hlsl_source = PostProcessor::GetPassFragmentShaderSource(API_D3D11, m_config, &pass_config);
		ReleasePassNativeResources(pass);
		RenderPassDx12Data* pixel_shader = new RenderPassDx12Data();
		pass.shader = reinterpret_cast<uintptr_t>(pixel_shader);
		D3D::CompilePixelShader(common_source + hlsl_source, &pixel_shader->m_shader_blob, macros, "passmain");
		if (pixel_shader->m_shader_blob == nullptr)
		{
			ReleasePassNativeResources(pass);
			ERROR_LOG(VIDEO, "Failed to compile post-processing shader %s (pass %s)", m_config->GetShaderName().c_str(), pass_config.entry_point.c_str());
			m_ready = false;
			return false;
		}
		pixel_shader->m_shader_bytecode = { pixel_shader->m_shader_blob->Data(), pixel_shader->m_shader_blob->Size() };

	}
	return true;
}

void D3DPostProcessingShader::MapAndUpdateConfigurationBuffer()
{
	u32 buffer_size;
	D3DStreamBuffer* buffer = reinterpret_cast<D3DStreamBuffer*>(m_uniform_buffer);
	void* buffer_data = m_config->UpdateConfigurationBuffer(&buffer_size, true);
	if (buffer_data)
	{
		buffer->AllocateSpaceInBuffer(buffer_size, 256);
		memcpy(
			buffer->GetCPUAddressOfCurrentAllocation(),
			buffer_data,
			buffer_size);

	}
	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PS_CBV, true);
	D3D::current_command_list->SetGraphicsRootConstantBufferView(
		DESCRIPTOR_TABLE_PS_CBVTWO,
		buffer->GetGPUAddressOfCurrentAllocation()
	);
}

void D3DPostProcessingShader::Draw(PostProcessor* p,
	const TargetRectangle& dst_rect, const TargetSize& dst_size, uintptr_t dst_tex,
	const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_tex,
	uintptr_t src_depth_tex, int src_layer, float gamma)
{
	D3DPostProcessor* parent = reinterpret_cast<D3DPostProcessor*>(p);
	D3DTexture2D* dst_texture = reinterpret_cast<D3DTexture2D*>(dst_tex);
	D3DTexture2D* src_texture = reinterpret_cast<D3DTexture2D*>(src_tex);
	D3DTexture2D* src_depth_texture = reinterpret_cast<D3DTexture2D*>(src_depth_tex);
	_dbg_assert_(VIDEO, m_ready && m_internal_size == src_size);

	// Determine whether we can skip the final copy by writing directly to the output texture, if the last pass is not scaled.
	bool skip_final_copy = !IsLastPassScaled() && (dst_texture != src_texture || !m_last_pass_uses_color_buffer) && !m_prev_frame_enabled;

	// Draw each pass.
	PostProcessor::InputTextureSizeArray input_sizes;
	TargetRectangle output_rect = {};
	TargetSize output_size;

	D3D12_CPU_DESCRIPTOR_HANDLE base_sampler_cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE base_sampler_gpu;
	int required_handles = (int)(POST_PROCESSING_MAX_TEXTURE_INPUTS * m_passes.size());
	DX12::D3D::sampler_descriptor_heap_mgr->AllocateGroup(required_handles, &base_sampler_cpu, &base_sampler_gpu);

	D3D12_CPU_DESCRIPTOR_HANDLE base_texture_cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE base_texture_gpu;
	// On the first texture in the group, we need to allocate the space in the descriptor heap.
	if (!DX12::D3D::gpu_descriptor_heap_mgr->AllocateTemporary(required_handles, &base_texture_cpu, &base_texture_gpu))
	{
		// Kick command buffer before attempting to allocate again. This is slow.
		D3D::command_list_mgr->ExecuteQueuedWork();
		if (!D3D::gpu_descriptor_heap_mgr->AllocateTemporary(required_handles, &base_texture_cpu, &base_texture_gpu))
		{
			PanicAlert("Failed to allocate temporary descriptors.");
			return;
		}
	}
	MapAndUpdateConfigurationBuffer();
	for (size_t pass_index = 0; pass_index < m_passes.size(); pass_index++)
	{
		const RenderPassData& pass = m_passes[pass_index];
		bool is_last_pass = (pass_index == m_last_pass_index);
		if (!pass.enabled)
			continue;		

		D3D12_CPU_DESCRIPTOR_HANDLE sampler_cpu = { base_sampler_cpu.ptr + pass_index * POST_PROCESSING_MAX_TEXTURE_INPUTS * D3D::sampler_descriptor_size};
		D3D12_GPU_DESCRIPTOR_HANDLE sampler_gpu = { base_sampler_gpu.ptr + pass_index * POST_PROCESSING_MAX_TEXTURE_INPUTS * D3D::sampler_descriptor_size };

		D3D12_CPU_DESCRIPTOR_HANDLE texture_cpu = { base_texture_cpu.ptr + pass_index * POST_PROCESSING_MAX_TEXTURE_INPUTS * D3D::resource_descriptor_size };
		D3D12_GPU_DESCRIPTOR_HANDLE texture_gpu = { base_texture_gpu.ptr + pass_index * POST_PROCESSING_MAX_TEXTURE_INPUTS * D3D::resource_descriptor_size };

		// Bind inputs to pipeline
		for (size_t i = 0; i < pass.inputs.size(); i++)
		{
			const InputBinding& input = pass.inputs[i];

			D3D12_CPU_DESCRIPTOR_HANDLE textureDestDescriptor;
			D3DTexture2D* input_texture = nullptr;
			textureDestDescriptor.ptr = texture_cpu.ptr + i * D3D::resource_descriptor_size;

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
			case POST_PROCESSING_INPUT_TYPE_PASS_FRAME_OUTPUT:
				if (m_prev_frame_enabled)
				{
					input_texture = reinterpret_cast<D3DTexture2D*>(GetPrevColorFrame(input.frame_index)->GetInternalObject());
					input_sizes[i] = m_prev_frame_size;
				}
				break;
			case POST_PROCESSING_INPUT_TYPE_PASS_DEPTH_FRAME_OUTPUT:
				if (m_prev_depth_enabled)
				{
					input_texture = reinterpret_cast<D3DTexture2D*>(GetPrevDepthFrame(input.frame_index)->GetInternalObject());
					input_sizes[i] = m_prev_depth_frame_size;
				}
				break;
			default:
				TextureCacheBase::TCacheEntryBase* i_texture = input.texture != nullptr ? input.texture : input.prev_texture;
				if (i_texture != nullptr)
				{
					input_texture = reinterpret_cast<D3DTexture2D*>(i_texture->GetInternalObject());
					input_sizes[i] = input.size;
				}
				else
				{
					input_texture = src_texture;
					input_sizes[i] = src_size;
				}
				break;
			}
			if (input_texture)
			{
				input_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}
			DX12::D3D::device->CopyDescriptorsSimple(
				1,
				textureDestDescriptor,
				input_texture != nullptr ? input_texture->GetSRVCPUShadow() : DX12::D3D::null_srv_cpu_shadow,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
			);

			D3D12_CPU_DESCRIPTOR_HANDLE destinationDescriptor;
			destinationDescriptor.ptr = sampler_cpu.ptr + i * D3D::sampler_descriptor_size;

			DX12::D3D::device->CopyDescriptorsSimple(
				1,
				destinationDescriptor,
				parent->GetSamplerHandle(static_cast<UINT>(input.texture_sampler) - 1),
				D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
			);
		}
		D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV, texture_gpu);
		D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SAMPLER, sampler_gpu);
		D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_SAMPLERS, true);

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
			reinterpret_cast<D3DTexture2D*>(pass.output_texture->GetInternalObject())->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
			D3D::current_command_list->OMSetRenderTargets(1, &reinterpret_cast<D3DTexture2D*>(pass.output_texture->GetInternalObject())->GetRTV(), FALSE, nullptr);
		}

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
			reinterpret_cast<RenderPassDx12Data*>(pass.shader)->m_shader_bytecode, parent->GetVertexShader(), StaticShaderCache::GetSimpleVertexShaderInputLayout(),
			geometry_shader, std::max(src_layer, 0), DXGI_FORMAT_R8G8B8A8_UNORM, false, dst_texture->GetMultisampled());
	}

	// Copy the last pass output to the target if not done already
	IncrementFrame();
	if (m_prev_depth_enabled && src_depth_tex)
	{
		TargetRectangle dst;
		dst.left = 0;
		dst.right = m_prev_depth_frame_size.width;
		dst.top = 0;
		dst.bottom = m_prev_depth_frame_size.height;
		parent->CopyTexture(dst, GetPrevDepthFrame(0)->GetInternalObject(), output_rect, src_depth_tex, src_size, src_layer, true, true);
	}
	if (!skip_final_copy)
	{
		RenderPassData& final_pass = m_passes[m_last_pass_index];
		if (m_prev_frame_enabled)
		{
			TargetRectangle dst;
			dst.left = 0;
			dst.right = m_prev_frame_size.width;
			dst.top = 0;
			dst.bottom = m_prev_frame_size.height;
			parent->CopyTexture(dst, GetPrevColorFrame(0)->GetInternalObject(), output_rect, final_pass.output_texture->GetInternalObject(), final_pass.output_size, src_layer, false, true);
		}
		parent->CopyTexture(dst_rect, dst_tex, output_rect, final_pass.output_texture->GetInternalObject(), final_pass.output_size, src_layer);
	}
}

D3DPostProcessor::~D3DPostProcessor()
{
	SAFE_RELEASE(m_geometry_shader_blob);
	SAFE_RELEASE(m_vertex_shader_blob);
	m_uniform_buffer.reset();
}

bool D3DPostProcessor::Initialize()
{
	D3D12_DESCRIPTOR_HEAP_DESC sampler_descriptor_heap_desc = {};
	sampler_descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	sampler_descriptor_heap_desc.NumDescriptors = POST_PROCESSING_INPUT_FILTER_COUNT * POST_PROCESSING_ADDRESS_MODE_COUNT;
	sampler_descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	HRESULT hr = D3D::device->CreateDescriptorHeap(&sampler_descriptor_heap_desc, IID_PPV_ARGS(m_texture_samplers_descriptor_heap.ReleaseAndGetAddressOf()));

	if (FAILED(hr))
	{
		return false;
	}

	texture_sampler_cpu_handle = m_texture_samplers_descriptor_heap->GetCPUDescriptorHandleForHeapStart();

	// Lookup tables for samplers
	static const D3D12_FILTER d3d_sampler_filters[] = { D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT };
	static const D3D12_TEXTURE_ADDRESS_MODE d3d_address_modes[] = { D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_MIRROR };
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
	for (size_t i = 0; i < POST_PROCESSING_INPUT_FILTER_COUNT; i++)
	{
		for (size_t j = 0; j < POST_PROCESSING_ADDRESS_MODE_COUNT; j++)
		{
			sampler_desc.Filter = d3d_sampler_filters[i];
			sampler_desc.AddressU = d3d_address_modes[j];
			sampler_desc.AddressV = d3d_address_modes[j];
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle{ texture_sampler_cpu_handle.ptr + (i * POST_PROCESSING_ADDRESS_MODE_COUNT + j) * D3D::sampler_descriptor_size };
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
	size_t basesize = Common::AlignUpSizePow2(POST_PROCESSING_CONTANTS_BUFFER_SIZE, 256);
	m_uniform_buffer = std::make_unique<D3DStreamBuffer>(basesize * 128, basesize * 1024, nullptr);
	return true;
}

std::unique_ptr<PostProcessingShader> D3DPostProcessor::CreateShader(PostProcessingShaderConfiguration* config)
{
	std::unique_ptr<PostProcessingShader> shader;
	shader.reset(new D3DPostProcessingShader());
	if (!shader->Initialize(config, FramebufferManager::GetEFBLayers()))
		shader.reset();

	return shader;
}

void D3DPostProcessor::PostProcessEFBToTexture(uintptr_t dst_texture)
{
	// Apply normal post-process process, but to the EFB buffers.
	// Uses the current viewport as the "visible" region to post-process.
	TargetRectangle target_rect = { 0, 0, g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight() };
	TargetSize target_size(g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight());

	// Source and target textures, if MSAA is enabled, this needs to be resolved
	D3DTexture2D* color_texture = FramebufferManager::GetResolvedEFBColorTexture();
	D3DTexture2D* depth_texture = nullptr;
	if (m_requires_depth_buffer)
	{
		depth_texture = FramebufferManager::GetResolvedEFBDepthTexture();
	}
	D3DTexture2D* real_dst_texture = reinterpret_cast<D3DTexture2D*>(dst_texture);

	// Invoke post process process
	PostProcess(nullptr, nullptr, nullptr,
		target_rect, target_size, reinterpret_cast<uintptr_t>(color_texture),
		target_rect, target_size, reinterpret_cast<uintptr_t>(depth_texture), dst_texture);

	g_renderer->RestoreAPIState();
}

void D3DPostProcessor::PostProcessEFB(const TargetRectangle& src_rect, const TargetSize& src_size)
{
	// Apply normal post-process process, but to the EFB buffers.
	// Uses the current viewport as the "visible" region to post-process.
	// In D3D, the viewport rectangle must fit within the render target.
	TargetRectangle target_rect;
	TargetSize target_size(src_size.width, src_size.height);
	target_rect.left = src_rect.left >= 0 ? src_rect.left : 0;
	target_rect.top = src_rect.top >= 0 ? src_rect.top : 0;
	target_rect.right = src_rect.right <= src_size.width ? src_rect.right : src_size.width;
	target_rect.bottom = src_rect.bottom <= src_size.height ? src_rect.bottom : src_size.height;

	// Source and target textures, if MSAA is enabled, this needs to be resolved
	D3DTexture2D* color_texture = FramebufferManager::GetResolvedEFBColorTexture();
	D3DTexture2D* depth_texture = nullptr;
	if (m_requires_depth_buffer)
	{
		depth_texture = FramebufferManager::GetResolvedEFBDepthTexture();
	}

	// Invoke post process process
	PostProcess(nullptr, nullptr, nullptr,
		target_rect, target_size, reinterpret_cast<uintptr_t>(color_texture),
		target_rect, target_size, reinterpret_cast<uintptr_t>(depth_texture));

	// Copy back to EFB buffer when multisampling is enabled
	if (g_ActiveConfig.iMultisamples > 1)
		CopyTexture(target_rect, reinterpret_cast<uintptr_t>(FramebufferManager::GetEFBColorTexture()), target_rect, reinterpret_cast<uintptr_t>(color_texture), target_size, -1, false, true);

	g_renderer->RestoreAPIState();
}

void D3DPostProcessor::MapAndUpdateUniformBuffer(
	const InputTextureSizeArray& input_sizes,
	const TargetRectangle& dst_rect, const TargetSize& dst_size,
	const TargetRectangle& src_rect, const TargetSize& src_size,
	int src_layer, float gamma)
{
	// Skip writing to buffer if there were no changes
	if (UpdateConstantUniformBuffer(input_sizes, dst_rect, dst_size, src_rect, src_size, src_layer, gamma))
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

void D3DPostProcessor::CopyTexture(const TargetRectangle& dst_rect, uintptr_t dst_tex,
	const TargetRectangle& src_rect, uintptr_t src_tex,
	const TargetSize& src_size, int src_layer, bool is_depth_texture,
	bool force_shader_copy)
{
	D3DTexture2D* dst_texture = reinterpret_cast<D3DTexture2D*>(dst_tex);
	D3DTexture2D* src_texture = reinterpret_cast<D3DTexture2D*>(src_tex);
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
			1 };

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
			dst_texture->GetFormat(), false, dst_texture->GetMultisampled());
	}
}

}  // namespace DX12
