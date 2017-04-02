// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Align.h"
#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "VideoBackends/DX11/D3DShader.h"
#include "VideoBackends/DX11/D3DState.h"
#include "VideoBackends/DX11/D3DUtil.h"
#include "VideoBackends/DX11/FramebufferManager.h"
#include "VideoBackends/DX11/GeometryShaderCache.h"
#include "VideoBackends/DX11/PixelShaderCache.h"
#include "VideoBackends/DX11/PostProcessing.h"
#include "VideoBackends/DX11/Render.h"
#include "VideoBackends/DX11/VertexShaderCache.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/Statistics.h"

namespace DX11
{

static const u32 FIRST_INPUT_BINDING_SLOT = 9;

static const char* s_shader_common = R"(

struct VS_INPUT
{
float4 position		: POSITION;
float3 texCoord		: TEXCOORD0;
float3 texCoord1		: TEXCOORD1;
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
	bool use_partial_buffer_update = D3D::SupportPartialContantBufferUpdate();
	m_uniform_buffer = reinterpret_cast<uintptr_t>(new D3D::ConstantStreamBuffer(static_cast<int>(PostProcessor::UNIFORM_BUFFER_SIZE * (use_partial_buffer_update ? 1024 : 1))));
}

D3DPostProcessingShader::~D3DPostProcessingShader()
{
	for (RenderPassData& pass : m_passes)
	{
		for (InputBinding& input : pass.inputs)
		{
			ReleaseBindingSampler(input.texture_sampler);
		}
		ReleasePassNativeResources(pass);
	}
	D3D::ConstantStreamBuffer* buffer = reinterpret_cast<D3D::ConstantStreamBuffer*>(m_uniform_buffer);
	delete buffer;
	m_uniform_buffer = 0;
}

void D3DPostProcessingShader::ReleasePassNativeResources(RenderPassData& pass)
{
	if (pass.shader)
	{
		ID3D11PixelShader* pixel_shader = reinterpret_cast<ID3D11PixelShader*>(pass.shader);
		SAFE_RELEASE(pixel_shader);
		pass.shader = 0;
	}
}

void D3DPostProcessingShader::ReleaseBindingSampler(uintptr_t sampler)
{
	ID3D11SamplerState* d3dsampler = reinterpret_cast<ID3D11SamplerState*>(sampler);
	SAFE_RELEASE(d3dsampler);
}

uintptr_t D3DPostProcessingShader::CreateBindingSampler(const PostProcessingShaderConfiguration::RenderPass::Input& input_config)
{
	// Lookup tables for samplers
	static const D3D11_FILTER d3d_sampler_filters[] = { D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT };
	static const D3D11_TEXTURE_ADDRESS_MODE d3d_address_modes[] = { D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_MIRROR };
	static const float d3d_border_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	// Create sampler object matching the values from config
	CD3D11_SAMPLER_DESC sampler_desc(d3d_sampler_filters[input_config.filter],
		d3d_address_modes[input_config.address_mode],
		d3d_address_modes[input_config.address_mode],
		D3D11_TEXTURE_ADDRESS_CLAMP,
		0.0f, 1, D3D11_COMPARISON_ALWAYS,
		d3d_border_color, 0.0f, 0.0f);
	ID3D11SamplerState* sampler = nullptr;
	HRESULT hr = D3D::device->CreateSamplerState(&sampler_desc, &sampler);
	if (FAILED(hr))
	{
		sampler = nullptr;
	}
	return reinterpret_cast<uintptr_t>(sampler);
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
	std::string common_source = PostProcessor::GetCommonFragmentShaderSource(API_D3D11, m_config);
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
		ID3D11PixelShader* ptr = D3D::CompileAndCreatePixelShaderPtr(common_source + hlsl_source, macros, "passmain");
		pass.shader = reinterpret_cast<uintptr_t>(ptr);
		if (!pass.shader)
		{
			ReleasePassNativeResources(pass);
			ERROR_LOG(VIDEO, "Failed to compile post-processing shader %s (pass %s)", m_config->GetShaderName().c_str(), pass_config.entry_point.c_str());
			m_ready = false;
			return false;
		}
	}
	return true;
}

void D3DPostProcessingShader::MapAndUpdateConfigurationBuffer()
{
	u32 buffer_size;
	void* buffer_data = m_config->UpdateConfigurationBuffer(&buffer_size, true);
	D3D::ConstantStreamBuffer* buffer = reinterpret_cast<D3D::ConstantStreamBuffer*>(m_uniform_buffer);
	if (buffer_data)
	{
		buffer->AppendData(buffer_data, buffer_size);
	}
	D3D::stateman->SetPixelConstants(1, buffer->GetDescriptor());
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
			D3D::context->OMSetRenderTargets(1, &dst_texture->GetRTV(), nullptr);
		}
		else
		{
			output_rect = PostProcessor::ScaleTargetRectangle(API_D3D11, src_rect, pass.output_scale);
			output_size = pass.output_size;
			D3D::context->OMSetRenderTargets(1, &reinterpret_cast<D3DTexture2D*>(pass.output_texture->GetInternalObject())->GetRTV(), nullptr);
		}

		// Bind inputs to pipeline
		for (size_t i = 0; i < pass.inputs.size(); i++)
		{
			const InputBinding& input = pass.inputs[i];
			ID3D11ShaderResourceView* input_srv = nullptr;

			switch (input.type)
			{
			case POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER:
				input_srv = src_texture->GetSRV();
				input_sizes[i] = src_size;
				break;

			case POST_PROCESSING_INPUT_TYPE_DEPTH_BUFFER:
				input_srv = (src_depth_texture != nullptr) ? src_depth_texture->GetSRV() : nullptr;
				input_sizes[i] = src_size;
				break;
			case POST_PROCESSING_INPUT_TYPE_PASS_FRAME_OUTPUT:
				if (m_prev_frame_enabled)
				{
					input_srv = reinterpret_cast<D3DTexture2D*>(GetPrevColorFrame(input.frame_index)->GetInternalObject())->GetSRV();
					input_sizes[i] = m_prev_frame_size;
				}
				break;
			case POST_PROCESSING_INPUT_TYPE_PASS_DEPTH_FRAME_OUTPUT:
				if (m_prev_depth_enabled)
				{
					input_srv = reinterpret_cast<D3DTexture2D*>(GetPrevDepthFrame(input.frame_index)->GetInternalObject())->GetSRV();
					input_sizes[i] = m_prev_depth_frame_size;
				}
				break;
			default:
				TextureCacheBase::TCacheEntryBase* input_texture = input.texture != nullptr ? input.texture : input.prev_texture;
				if (input_texture != nullptr)
				{
					input_srv = reinterpret_cast<D3DTexture2D*>(input_texture->GetInternalObject())->GetSRV();
					input_sizes[i] = input.size;
				}
				else
				{
					input_srv = src_texture->GetSRV();
					input_sizes[i] = src_size;
				}
				break;
			}

			D3D::stateman->SetTexture(FIRST_INPUT_BINDING_SLOT + input.texture_unit, input_srv);
			D3D::stateman->SetSampler(FIRST_INPUT_BINDING_SLOT + input.texture_unit, reinterpret_cast<ID3D11SamplerState*>(input.texture_sampler));
		}

		// Set viewport based on target rect
		CD3D11_VIEWPORT output_viewport((float)output_rect.left, (float)output_rect.top,
			(float)output_rect.GetWidth(), (float)output_rect.GetHeight());

		D3D::context->RSSetViewports(1, &output_viewport);

		parent->MapAndUpdateUniformBuffer(input_sizes, output_rect, output_size, src_rect, src_size, src_layer, gamma);
		// Select geometry shader based on layers
		ID3D11GeometryShader* geometry_shader = nullptr;
		if (src_layer < 0 && m_internal_layers > 1)
			geometry_shader = parent->GetGeometryShader();

		// Draw pass
		D3D::drawShadedTexQuad(nullptr, src_rect.AsRECT(), src_size.width, src_size.height,
			reinterpret_cast<ID3D11PixelShader*>(pass.shader), parent->GetVertexShader(), VertexShaderCache::GetSimpleInputLayout(),
			geometry_shader, 1.0f, std::max(src_layer, 0));
	}

	// Unbind input textures after rendering, so that they can safely be used as outputs again.
	for (u32 i = 0; i < POST_PROCESSING_MAX_TEXTURE_INPUTS; i++)
		D3D::stateman->SetTexture(FIRST_INPUT_BINDING_SLOT + i, nullptr);
	D3D::stateman->Apply();

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
}

bool D3DPostProcessor::Initialize()
{
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
	m_vertex_shader = D3D::CompileAndCreateVertexShader(std::string(s_shader_common) + std::string(s_vertex_shader));
	m_geometry_shader = D3D::CompileAndCreateGeometryShader(std::string(s_shader_common) + std::string(s_geometry_shader));
	if (!m_vertex_shader || !m_geometry_shader)
	{
		m_vertex_shader.reset();
		m_geometry_shader.reset();
		return false;
	}
	return true;
}

bool D3DPostProcessor::CreateUniformBuffer()
{
	m_uniform_buffer.reset();
	bool use_partial_buffer_update = D3D::SupportPartialContantBufferUpdate();
	m_uniform_buffer = std::make_unique<D3D::ConstantStreamBuffer>(static_cast<int>(Common::AlignUpSizePow2(POST_PROCESSING_CONTANTS_BUFFER_SIZE, 256) * (use_partial_buffer_update ? 1024 : 1)));
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
	g_renderer->ResetAPIState();

	TargetRectangle target_rect = { 0, 0, g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight() };
	TargetSize target_size(g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight());

	// Source and target textures, if MSAA is enabled, this needs to be resolved
	D3DTexture2D* color_texture = FramebufferManager::GetResolvedEFBColorTexture();
	D3DTexture2D* depth_texture = nullptr;
	if (m_requires_depth_buffer)
		depth_texture = FramebufferManager::GetResolvedEFBDepthTexture();

	// Invoke post process process
	PostProcess(nullptr, nullptr, nullptr,
		target_rect, target_size, reinterpret_cast<uintptr_t>(color_texture),
		target_rect, target_size, reinterpret_cast<uintptr_t>(depth_texture), dst_texture);

	g_renderer->RestoreAPIState();

	// Restore EFB target
	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());
}

void D3DPostProcessor::PostProcessEFB(const TargetRectangle& src_rect, const TargetSize& src_size)
{
	// Apply normal post-process process, but to the EFB buffers.
	// Uses the current viewport as the "visible" region to post-process.
	g_renderer->ResetAPIState();

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
		depth_texture = FramebufferManager::GetResolvedEFBDepthTexture();

	// Invoke post process process
	PostProcess(nullptr, nullptr, nullptr,
		target_rect, target_size, reinterpret_cast<uintptr_t>(color_texture),
		target_rect, target_size, reinterpret_cast<uintptr_t>(depth_texture));

	// Copy back to EFB buffer when multisampling is enabled
	if (g_ActiveConfig.iMultisamples > 1)
		CopyTexture(target_rect, reinterpret_cast<uintptr_t>(FramebufferManager::GetEFBColorTexture()), target_rect, reinterpret_cast<uintptr_t>(color_texture), target_size, -1, false, true);

	g_renderer->RestoreAPIState();

	// Restore EFB target
	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());
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
		m_uniform_buffer->AppendData(m_current_constants.data(), POST_PROCESSING_CONTANTS_BUFFER_SIZE);
		ADDSTAT(stats.thisFrame.bytesUniformStreamed, POST_PROCESSING_CONTANTS_BUFFER_SIZE);
	}
	D3D::stateman->SetPixelConstants(0, m_uniform_buffer->GetDescriptor());

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
	if (!scaling && !force_shader_copy && !is_depth_texture)
	{
		CD3D11_BOX copy_box(src_rect.left, src_rect.top, 0, src_rect.right, src_rect.bottom, 1);
		if (src_layer < 0)
		{
			// Copy all layers
			for (unsigned int layer = 0; layer < FramebufferManager::GetEFBLayers(); layer++)
				D3D::context->CopySubresourceRegion(dst_texture->GetTex(), layer, dst_rect.left, dst_rect.top, 0, src_texture->GetTex(), layer, &copy_box);
		}
		else
		{
			// Copy single layer to layer 0
			D3D::context->CopySubresourceRegion(dst_texture->GetTex(), 0, dst_rect.left, dst_rect.top, 0, src_texture->GetTex(), src_layer, &copy_box);
		}
	}
	else
	{
		CD3D11_VIEWPORT target_viewport((float)dst_rect.left, (float)dst_rect.top, (float)dst_rect.GetWidth(), (float)dst_rect.GetHeight());

		D3D::context->RSSetViewports(1, &target_viewport);
		D3D::context->OMSetRenderTargets(1, &dst_texture->GetRTV(), nullptr);
		if (scaling)
			D3D::SetLinearCopySampler();
		else
			D3D::SetLinearCopySampler();

		D3D::drawShadedTexQuad(src_texture->GetSRV(), src_rect.AsRECT(), src_size.width, src_size.height,
			PixelShaderCache::GetColorCopyProgram(false), VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(),
			(src_layer < 0) ? GeometryShaderCache::GetCopyGeometryShader() : nullptr, 1.0f, std::max(src_layer, 0));
	}
}

}  // namespace DX11
