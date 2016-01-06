// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

namespace DX11
{
static const u32 FIRST_INPUT_BINDING_SLOT = 9;

PostProcessingShader::PostProcessingShader()
{

}

PostProcessingShader::~PostProcessingShader()
{
	// Delete texture objects that we own
	for (RenderPassData& pass : m_passes)
	{
		for (InputBinding& input : pass.inputs)
		{
			// External textures
			if (input.texture != nullptr)
				input.texture->Release();
		}

		if (pass.output_texture != nullptr)
			pass.output_texture->Release();

		pass.pixel_shader.reset();
	}
}

bool PostProcessingShader::Initialize(const PostProcessingShaderConfiguration* config, int target_layers)
{
	m_internal_layers = target_layers;
	m_config = config;
	m_ready = false;

	m_passes.reserve(m_config->GetPasses().size());
	for (const auto& pass_config : m_config->GetPasses())
	{
		// texture is allocated in CreateIntermediateBuffers
		RenderPassData pass;
		pass.output_texture = nullptr;
		pass.output_width = 0;
		pass.output_height = 0;
		pass.output_scale = pass_config.output_scale;
		pass.enabled = true;

		// set up inputs for external images and upload them
		pass.inputs.reserve(pass_config.inputs.size());
		for (const auto& input_config : pass_config.inputs)
		{
			// Non-external textures will be bound at run-time.
			InputBinding input;
			input.type = input_config.type;
			input.width = 1;
			input.height = 1;
			input.texture_unit = input_config.texture_unit;
			input.texture = nullptr;
			input.texture_srv = nullptr;
			input.texture_sampler = nullptr;

			// Only external images have to be set up here
			if (input.type == POST_PROCESSING_INPUT_TYPE_IMAGE)
			{
				_dbg_assert_(VIDEO, input_config.external_image_width > 0 && input_config.external_image_height > 0);

				// Copy the external image across all layers
				Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
				CD3D11_TEXTURE2D_DESC texture_desc(DXGI_FORMAT_R8G8B8A8_UNORM,
					input_config.external_image_width, input_config.external_image_height, 1, 1,
					D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, 1, 0, 0);

				D3D11_SUBRESOURCE_DATA initial_data = { input_config.external_image_data.get(),
					input_config.external_image_width * 4, 0 };

				std::unique_ptr<D3D11_SUBRESOURCE_DATA[]> initial_data_layers = std::make_unique<D3D11_SUBRESOURCE_DATA[]>(m_internal_layers);
				for (int i = 0; i < m_internal_layers; i++)
					memcpy(&initial_data_layers[i], &initial_data, sizeof(initial_data));

				HRESULT hr = D3D::device->CreateTexture2D(&texture_desc, initial_data_layers.get(), &texture);
				if (FAILED(hr))
				{
					ERROR_LOG(VIDEO, "Failed to upload post-processing external image for shader %s (pass %s)", m_config->GetShader().c_str(), pass_config.entry_point.c_str());
					return false;
				}

				input.texture = new D3DTexture2D(texture.Get(), (D3D11_BIND_FLAG)texture_desc.BindFlags, texture_desc.Format);
				input.width = input_config.external_image_width;
				input.height = input_config.external_image_height;
			}

			// Lookup tables for samplers, simple due to no mipmaps
			static const D3D11_FILTER d3d_sampler_filters[] = { D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT };
			static const D3D11_TEXTURE_ADDRESS_MODE d3d_address_modes[] = { D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_BORDER };
			static const float d3d_border_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

			// Create sampler object matching the values from config
			CD3D11_SAMPLER_DESC sampler_desc(d3d_sampler_filters[input_config.filter],
				d3d_address_modes[input_config.address_mode],
				d3d_address_modes[input_config.address_mode],
				D3D11_TEXTURE_ADDRESS_CLAMP,
				0.0f, 1, D3D11_COMPARISON_ALWAYS,
				d3d_border_color, 0.0f, 0.0f);

			HRESULT hr = D3D::device->CreateSamplerState(&sampler_desc, input.texture_sampler.ReleaseAndGetAddressOf());
			if (FAILED(hr))
			{
				ERROR_LOG(VIDEO, "Failed to create post-processing sampler for shader %s (pass %s)", m_config->GetShader().c_str(), pass_config.entry_point.c_str());
				return false;
			}

			pass.inputs.push_back(std::move(input));
		}

		m_passes.push_back(std::move(pass));
	}

	// Compile programs
	if (!RecompileShaders())
		return false;

	// Determine which passes to execute
	UpdateEnabledPasses();
	m_ready = true;
	return true;
}

bool PostProcessingShader::ResizeIntermediateBuffers(int target_width, int target_height)
{
	_dbg_assert_(VIDEO, target_width > 0 && target_height > 0);
	if (m_internal_width == target_width && m_internal_height == target_height)
		return true;

	m_ready = false;

	size_t previous_pass = 0;
	for (size_t pass_index = 0; pass_index < m_passes.size(); pass_index++)
	{
		RenderPassData& pass = m_passes[pass_index];
		const PostProcessingShaderConfiguration::RenderPass& pass_config = m_config->GetPass(pass_index);
		PostProcessor::ScaleTargetSize(&pass.output_width, &pass.output_height, target_width, target_height, pass_config.output_scale);

		if (pass.output_texture != nullptr)
		{
			pass.output_texture->Release();
			pass.output_texture = nullptr;
		}

		Microsoft::WRL::ComPtr<ID3D11Texture2D> color_texture;
		CD3D11_TEXTURE2D_DESC color_texture_desc(DXGI_FORMAT_R8G8B8A8_UNORM, pass.output_width, pass.output_height, m_internal_layers, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
		HRESULT hr = D3D::device->CreateTexture2D(&color_texture_desc, nullptr, &color_texture);
		if (FAILED(hr))
		{
			ERROR_LOG(VIDEO, "Failed to create post-processing intermediate buffers (dimensions: %ux%u)", target_width, target_height);
			return false;
		}

		pass.output_texture = new D3DTexture2D(color_texture.Get(), (D3D11_BIND_FLAG)color_texture_desc.BindFlags, color_texture_desc.Format);

		// Hook up any inputs that are other passes
		for (size_t input_index = 0; input_index < pass_config.inputs.size(); input_index++)
		{
			const PostProcessingShaderConfiguration::RenderPass::Input& input_config = pass_config.inputs[input_index];
			InputBinding& input_binding = pass.inputs[input_index];
			if (input_config.type == POST_PROCESSING_INPUT_TYPE_PASS_OUTPUT)
			{
				_dbg_assert_(VIDEO, input_config.pass_output_index < pass_index);
				input_binding.texture_srv = m_passes[input_config.pass_output_index].output_texture->GetSRV();
				input_binding.width = m_passes[input_config.pass_output_index].output_width;
				input_binding.height = m_passes[input_config.pass_output_index].output_height;
			}
			else if (input_config.type == POST_PROCESSING_INPUT_TYPE_PREVIOUS_PASS_OUTPUT)
			{
				_dbg_assert_(VIDEO, previous_pass < pass_index);
				input_binding.texture_srv = m_passes[previous_pass].output_texture->GetSRV();
				input_binding.width = m_passes[previous_pass].output_width;
				input_binding.height = m_passes[previous_pass].output_height;
			}
		}

		if (pass.enabled)
			previous_pass = pass_index;
	}

	m_internal_width = target_width;
	m_internal_height = target_height;
	m_ready = true;
	return true;
}

void PostProcessingShader::Draw(D3DPostProcessor* parent,
	const TargetRectangle& target_rect, D3DTexture2D* target_texture,
	const TargetRectangle& src_rect, int src_width, int src_height,
	D3DTexture2D* src_texture, D3DTexture2D* src_depth_texture,
	int src_layer, float gamma)
{
	_dbg_assert_(VIDEO, m_ready);
	_dbg_assert_(VIDEO, src_width == m_internal_width && src_height == m_internal_height);

	// Determine whether we can skip the final copy by writing directly to the output texture.
	bool skip_final_copy = (target_texture != src_texture || !m_last_pass_uses_color_buffer);

	// If the last pass is not at full scale, we can't skip the copy.
	if (m_passes[m_last_pass_index].output_width != src_width || m_passes[m_last_pass_index].output_height != src_height)
		skip_final_copy = false;

	// Draw each pass.
	TargetRectangle output_rect = {};
	int input_resolutions[POST_PROCESSING_MAX_TEXTURE_INPUTS][2] = {};
	for (size_t pass_index = 0; pass_index < m_passes.size(); pass_index++)
	{
		const RenderPassData& pass = m_passes[pass_index];
		bool is_last_pass = (pass_index == m_last_pass_index);

		// If this is the last pass and we can skip the final copy, write directly to output texture.
		if (is_last_pass && skip_final_copy)
		{
			// The target rect may differ from the source.
			output_rect = target_rect;
			D3D::context->OMSetRenderTargets(1, &target_texture->GetRTV(), nullptr);
		}
		else
		{
			output_rect = PostProcessor::ScaleTargetRectangle(API_D3D11, src_rect, pass.output_scale);
			D3D::context->OMSetRenderTargets(1, &pass.output_texture->GetRTV(), nullptr);
		}

		// Bind inputs to pipeline
		for (size_t i = 0; i < pass.inputs.size(); i++)
		{
			const InputBinding& input = pass.inputs[i];
			ID3D11ShaderResourceView* input_srv;

			switch (input.type)
			{
			case POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER:
				input_srv = src_texture->GetSRV();
				input_resolutions[i][0] = src_width;
				input_resolutions[i][1] = src_height;
				break;

			case POST_PROCESSING_INPUT_TYPE_DEPTH_BUFFER:
				input_srv = (src_depth_texture != nullptr) ? src_depth_texture->GetSRV() : nullptr;
				input_resolutions[i][0] = src_width;
				input_resolutions[i][1] = src_height;
				break;

			default:
				input_srv = input.texture_srv;
				input_resolutions[i][0] = input.width;
				input_resolutions[i][1] = input.height;
				break;
			}

			D3D::stateman->SetTexture(FIRST_INPUT_BINDING_SLOT + input.texture_unit, input_srv);
			D3D::stateman->SetSampler(FIRST_INPUT_BINDING_SLOT + input.texture_unit, input.texture_sampler.Get());
		}

		// Set viewport based on target rect
		CD3D11_VIEWPORT output_viewport((float)output_rect.left, (float)output_rect.top,
			(float)output_rect.GetWidth(), (float)output_rect.GetHeight());

		D3D::context->RSSetViewports(1, &output_viewport);

		parent->MapAndUpdateUniformBuffer(m_config, input_resolutions, src_rect, target_rect, src_width, src_height, src_layer, gamma);

		// Select geometry shader based on layers
		ID3D11GeometryShader* geometry_shader = nullptr;
		if (src_layer < 0 && m_internal_layers > 1)
			geometry_shader = GeometryShaderCache::GetCopyGeometryShader();

		// Draw pass
		D3D::drawShadedTexQuad(nullptr, src_rect.AsRECT(), src_width, src_height,
			pass.pixel_shader.get(), VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(),
			geometry_shader, 1.0f, std::max(src_layer, 0));

		// Unbind textures after pass, if they're used in the future passes
		for (u32 i = 0; i < POST_PROCESSING_MAX_TEXTURE_INPUTS; i++)
			D3D::stateman->SetTexture(FIRST_INPUT_BINDING_SLOT + i, nullptr);
		D3D::stateman->Apply();
	}

	// Copy the last pass output to the target if not done already
	if (!skip_final_copy)
	{
		D3DTexture2D* final_texture = (target_texture != nullptr) ? target_texture : D3D::GetBackBuffer();
		RenderPassData& final_pass = m_passes[m_last_pass_index];
		D3DPostProcessor::CopyTexture(target_rect, final_texture, output_rect, final_pass.output_texture,
			final_pass.output_width, final_pass.output_height, src_layer);
	}
}

bool PostProcessingShader::RecompileShaders()
{
	for (size_t i = 0; i < m_passes.size(); i++)
	{
		RenderPassData& pass = m_passes[i];
		const PostProcessingShaderConfiguration::RenderPass& pass_config = m_config->GetPass(i);

		std::string hlsl_source = PostProcessor::GetPassFragmentShaderSource(API_D3D11, m_config, &pass_config);
		pass.pixel_shader = D3D::CompileAndCreatePixelShader(hlsl_source);
		if (pass.pixel_shader == nullptr)
		{
			ERROR_LOG(VIDEO, "Failed to compile post-processing shader %s (pass %s)", m_config->GetShader().c_str(), pass_config.entry_point.c_str());
			m_ready = false;
			return false;
		}
	}

	return true;
}

void PostProcessingShader::UpdateEnabledPasses()
{
	m_last_pass_index = 0;
	m_last_pass_uses_color_buffer = false;

	size_t previous_pass_index = 0;
	for (size_t pass_index = 0; pass_index < m_passes.size(); pass_index++)
	{
		const PostProcessingShaderConfiguration::RenderPass& pass_config = m_config->GetPass(pass_index);
		RenderPassData& pass = m_passes[pass_index];
		pass.enabled = pass_config.CheckEnabled();

		// Check for color buffer reads, for copy optimization
		if (pass.enabled)
		{
			m_last_pass_index = pass_index;
			m_last_pass_uses_color_buffer = false;
			for (size_t input_index = 0; input_index < pass.inputs.size(); input_index++)
			{
				InputBinding& input = pass.inputs[input_index];
				if (input.type == POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER)
				{
					m_last_pass_uses_color_buffer = true;
					break;
				}
			}
		}
	}
}

D3DPostProcessor::~D3DPostProcessor()
{
	if (m_color_copy_texture != nullptr)
		m_color_copy_texture->Release();
	if (m_depth_copy_texture != nullptr)
		m_depth_copy_texture->Release();
}

bool D3DPostProcessor::Initialize()
{
	// Uniform buffer
	CD3D11_BUFFER_DESC buffer_desc(UNIFORM_BUFFER_SIZE, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE, 0, 0);
	m_uniform_buffer.reset();
	HRESULT hr = D3D::device->CreateBuffer(&buffer_desc, nullptr, D3D::ToAddr(m_uniform_buffer));
	if (FAILED(hr))
	{
		ERROR_LOG(VIDEO, "Failed to create post-processing uniform buffer (hr=%X)", hr);
		return false;
	}

	// Load the currently-configured shader (this may fail, and that's okay)
	ReloadShaders();
	return true;
}

void D3DPostProcessor::ReloadShaders()
{
	m_reload_flag.Clear();
	m_post_processing_shader.reset();
	m_blit_shader.reset();
	m_active = false;
	if (g_ActiveConfig.bPostProcessingEnable)
	{
		const std::string& post_shader_name = g_ActiveConfig.sPostProcessingShader;
		m_post_processing_shader = std::make_unique<PostProcessingShader>();
		if (m_config.LoadShader("", post_shader_name) && m_post_processing_shader->Initialize(&m_config, FramebufferManager::GetEFBLayers()))
		{
			if (!post_shader_name.empty())
			{
				DEBUG_LOG(VIDEO, "Postprocessing shader loaded: '%s'", post_shader_name.c_str());
				OSD::AddMessage(StringFromFormat("Postprocessing shader loaded: '%s'", post_shader_name.c_str()));
			}

			m_config.ClearDirty();
			m_active = true;
		}
		else
		{
			ERROR_LOG(VIDEO, "Failed to load postprocessing shader ('%s'). Disabling post processor.", post_shader_name.c_str());
			OSD::AddMessage(StringFromFormat("Failed to load postprocessing shader ('%s'). Disabling post processor.", post_shader_name.c_str()));

			m_post_processing_shader.reset();
		}
	}

	const std::string& blit_shader_subdir = (g_ActiveConfig.iStereoMode == STEREO_ANAGLYPH) ? ANAGLYPH_DIR : "";
	const std::string& blit_shader_name = (g_ActiveConfig.iStereoMode == STEREO_ANAGLYPH) ? g_ActiveConfig.sAnaglyphShader : g_ActiveConfig.sBlitShader;
	m_blit_shader = std::make_unique<PostProcessingShader>();
	if (m_blit_config.LoadShader(blit_shader_subdir, blit_shader_name) && m_blit_shader->Initialize(&m_blit_config, FramebufferManager::GetEFBLayers()))
	{
		if (!blit_shader_name.empty())
		{
			DEBUG_LOG(VIDEO, "Blit shader loaded: '%s'", blit_shader_name.c_str());
			OSD::AddMessage(StringFromFormat("Blit shader loaded: '%s'", blit_shader_name.c_str()));
		}

		m_blit_config.ClearDirty();
	}
	else
	{
		ERROR_LOG(VIDEO, "Failed to load blit shader ('%s'). Falling back to copy shader.", blit_shader_name.c_str());
		OSD::AddMessage(StringFromFormat("Failed to load blit shader ('%s'). Falling back to copy shader.", blit_shader_name.c_str()));

		m_blit_shader.reset();
	}
}

void D3DPostProcessor::PostProcessEFB()
{
	// Uses the current viewport as the "visible" region to post-process.
	g_renderer->ResetAPIState();

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
	target_rect.left = static_cast<int>((X >= 0.f) ? X : 0.f);
	target_rect.top = static_cast<int>((Y >= 0.f) ? Y : 0.f);
	target_rect.right = target_rect.left + static_cast<int>((X + Wd <= g_renderer->GetTargetWidth()) ? Wd : (g_renderer->GetTargetWidth() - X));
	target_rect.bottom = target_rect.bottom + static_cast<int>((Y + Ht <= g_renderer->GetTargetHeight()) ? Ht : (g_renderer->GetTargetHeight() - Y));

	// Source and target textures, if MSAA is enabled, this needs to be resolved
	D3DTexture2D* color_texture = FramebufferManager::GetResolvedEFBColorTexture();
	D3DTexture2D* depth_texture = nullptr;
	if (m_config.RequiresDepthBuffer())
		depth_texture = FramebufferManager::GetResolvedEFBDepthTexture();

	// Invoke post process process
	PostProcess(target_rect, g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight(), FramebufferManager::GetEFBLayers(),
		reinterpret_cast<uintptr_t>(color_texture), reinterpret_cast<uintptr_t>(depth_texture));

	// Copy back to EFB buffer when multisampling is enabled
	if (g_ActiveConfig.iMultisamples > 1)
	{
		CopyTexture(target_rect, FramebufferManager::GetEFBColorTexture(), target_rect, color_texture,
			g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight(), -1, true);
	}

	g_renderer->RestoreAPIState();

	// Restore EFB target
	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());
}

void D3DPostProcessor::BlitToFramebuffer(const TargetRectangle& dst, uintptr_t dst_texture,
	const TargetRectangle& src, uintptr_t src_texture,
	int src_width, int src_height, int src_layer, float gamma)
{
	D3DTexture2D* real_dst_texture = reinterpret_cast<D3DTexture2D*>(dst_texture);
	D3DTexture2D* real_src_texture = reinterpret_cast<D3DTexture2D*>(src_texture);
	_dbg_assert_msg_(VIDEO, src_layer >= 0, "BlitToFramebuffer should always be called with a single source layer");

	// Options changed?
	if (m_blit_shader != nullptr && m_blit_shader->IsReady())
	{
		if (m_blit_config.IsDirty())
		{
			if (m_blit_config.IsCompileTimeConstantsDirty())
				m_blit_shader->RecompileShaders();

			m_blit_shader->UpdateEnabledPasses();
			m_blit_config.ClearDirty();
		}
		m_blit_shader->ResizeIntermediateBuffers(src_width, src_height);
	}
	// Use blit shader if one is set-up. Should only be a single pass in almost all cases.
	if (m_blit_shader != nullptr && m_blit_shader->IsReady())
	{
		D3D::stateman->SetPixelConstants(m_uniform_buffer.get());
		m_blit_shader->Draw(this, dst, real_dst_texture, src, src_width, src_height, real_src_texture, nullptr, src_layer);
	}
	else
	{
		CopyTexture(dst, real_dst_texture, src, real_src_texture, src_width, src_height, src_layer);
	}
}

void D3DPostProcessor::PostProcess(const TargetRectangle& visible_rect, int tex_width, int tex_height, int tex_layers,
	uintptr_t texture, uintptr_t depth_texture)
{
	D3DTexture2D* real_texture = reinterpret_cast<D3DTexture2D*>(texture);
	D3DTexture2D* real_depth_texture = reinterpret_cast<D3DTexture2D*>(depth_texture);
	_dbg_assert_(VIDEO, !m_active || m_post_processing_shader != nullptr);
	if (!(m_active && m_post_processing_shader))
		return;

	int visible_width = visible_rect.GetWidth();
	int visible_height = visible_rect.GetHeight();
	if (!m_post_processing_shader->IsReady() ||
		!m_post_processing_shader->ResizeIntermediateBuffers(visible_width, visible_height) ||
		!ResizeCopyBuffers(visible_width, visible_height, tex_layers) ||
		(m_config.IsCompileTimeConstantsDirty() && !m_post_processing_shader->RecompileShaders()))
	{
		ERROR_LOG(VIDEO, "Failed to update post-processor state. Disabling post processor.");
		m_post_processing_shader.reset();
		m_active = false;
		return;
	}

	if (m_config.IsDirty())
	{
		m_post_processing_shader->UpdateEnabledPasses();
		m_config.ClearDirty();
	}

	// Copy only the visible region to our buffers.
	TargetRectangle buffer_rect(0, 0, visible_width, visible_height);
	CopyTexture(buffer_rect, m_color_copy_texture, visible_rect, real_texture, tex_width, tex_height, -1, false);
	if (real_depth_texture != nullptr)
	{
		// Depth buffers can only be completely CopySubresourced, so use a shader copy instead.
		CopyTexture(buffer_rect, m_depth_copy_texture, visible_rect, real_depth_texture, tex_width, tex_height, -1, true);
	}

	// Post-process, and output back to the original texture.
	D3D::stateman->SetPixelConstants(m_uniform_buffer.get());
	m_post_processing_shader->Draw(this, visible_rect, real_texture, buffer_rect, visible_width, visible_height, m_color_copy_texture, m_depth_copy_texture, -1);
}

void D3DPostProcessor::MapAndUpdateUniformBuffer(const PostProcessingShaderConfiguration* config,
	int input_resolutions[POST_PROCESSING_MAX_TEXTURE_INPUTS][2],
	const TargetRectangle& src_rect, const TargetRectangle& dst_rect, int src_width, int src_height, int src_layer, float gamma)
{
	D3D11_MAPPED_SUBRESOURCE mapped_cbuf;
	HRESULT hr = D3D::context->Map(m_uniform_buffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_cbuf);
	CHECK(SUCCEEDED(hr), "Map post processing constant buffer failed, hr=%X", hr);
	UpdateUniformBuffer(API_D3D11, config, mapped_cbuf.pData, input_resolutions, src_rect, dst_rect, src_width, src_height, src_layer, gamma);
	D3D::context->Unmap(m_uniform_buffer.get(), 0);
}

bool D3DPostProcessor::ResizeCopyBuffers(int width, int height, int layers)
{
	HRESULT hr;
	if (m_copy_width == width && m_copy_height == height && m_copy_layers == layers)
		return true;

	if (m_color_copy_texture != nullptr)
	{
		m_color_copy_texture->Release();
		m_color_copy_texture = nullptr;
	}
	if (m_depth_copy_texture != nullptr)
	{
		m_depth_copy_texture->Release();
		m_depth_copy_texture = nullptr;
	}

	// reset before creating, in case it fails
	m_copy_width = 0;
	m_copy_height = 0;
	m_copy_layers = 0;

	// allocate new textures
	Microsoft::WRL::ComPtr<ID3D11Texture2D> color_texture;
	CD3D11_TEXTURE2D_DESC color_desc(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, layers, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, 1, 0, 0);
	if (FAILED(hr = D3D::device->CreateTexture2D(&color_desc, nullptr, &color_texture)))
	{
		ERROR_LOG(VIDEO, "CreateTexture2D failed, hr=0x%X", hr);
		return false;
	}

	Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_texture;
	CD3D11_TEXTURE2D_DESC depth_desc(DXGI_FORMAT_R32_FLOAT, width, height, layers, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, 1, 0, 0);
	if (FAILED(hr = D3D::device->CreateTexture2D(&depth_desc, nullptr, &depth_texture)))
	{
		ERROR_LOG(VIDEO, "CreateTexture2D failed, hr=0x%X", hr);
		color_texture->Release();
		return false;
	}

	m_copy_width = width;
	m_copy_height = height;
	m_copy_layers = layers;
	m_color_copy_texture = new D3DTexture2D(color_texture.Get(), (D3D11_BIND_FLAG)color_desc.BindFlags, color_desc.Format);
	m_depth_copy_texture = new D3DTexture2D(depth_texture.Get(), (D3D11_BIND_FLAG)depth_desc.BindFlags, depth_desc.Format);
	return true;

}

void D3DPostProcessor::CopyTexture(const TargetRectangle& dst_rect, D3DTexture2D* dst_texture,
	const TargetRectangle& src_rect, D3DTexture2D* src_texture,
	int src_width, int src_height, int src_layer,
	bool force_shader_copy)
{
	// If the dimensions are the same, we can copy instead of using a shader.
	bool scaling = (dst_rect.GetWidth() != src_rect.GetWidth() || dst_rect.GetHeight() != src_rect.GetHeight());
	if (!scaling && !force_shader_copy)
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

		D3D::drawShadedTexQuad(src_texture->GetSRV(), src_rect.AsRECT(), src_width, src_height,
			PixelShaderCache::GetColorCopyProgram(false), VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(),
			(src_layer < 0) ? GeometryShaderCache::GetCopyGeometryShader() : nullptr, 1.0f, std::max(src_layer, 0));
	}
}

}  // namespace DX11
