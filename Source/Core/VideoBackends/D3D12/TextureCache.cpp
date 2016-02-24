// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DBlob.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DShader.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DStreamBuffer.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/PSTextureEncoder.h"
#include "VideoBackends/D3D12/StaticShaderCache.h"
#include "VideoBackends/D3D12/TextureCache.h"
#include "VideoBackends/D3D12/TextureEncoder.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/TextureScalerCommon.h"

namespace DX12
{

static std::unique_ptr<TextureEncoder> s_encoder;
static std::unique_ptr<TextureScaler> s_scaler;

static std::unique_ptr<D3DStreamBuffer> s_efb_copy_stream_buffer = nullptr;
static u32 s_efb_copy_last_cbuf_id = UINT_MAX;

static ID3D12Resource* s_texture_cache_entry_readback_buffer = nullptr;
static void* s_texture_cache_entry_readback_buffer_data = nullptr;
static UINT s_texture_cache_entry_readback_buffer_size = 0;

TextureCache::TCacheEntry::~TCacheEntry()
{
	m_texture->Release();
	SAFE_RELEASE(m_nrm_texture);
}

void TextureCache::TCacheEntry::Bind(unsigned int stage, unsigned int last_Texture)
{
	static bool s_first_texture_in_group = true;
	static D3D12_CPU_DESCRIPTOR_HANDLE s_group_base_texture_cpu_handle;
	static D3D12_GPU_DESCRIPTOR_HANDLE s_group_base_texture_gpu_handle;

	const bool use_materials = g_ActiveConfig.HiresMaterialMapsEnabled();
	if (last_Texture == 0 && !use_materials)
	{
		DX12::D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV, this->m_texture_srv_gpu_handle);
		if (g_ActiveConfig.TessellationEnabled())
		{
			DX12::D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_DS_SRV, this->m_texture_srv_gpu_handle);
		}
		return;
	}
	
	if (s_first_texture_in_group)
	{
		const unsigned int num_handles = use_materials ? 16 : 8;
		// On the first texture in the group, we need to allocate the space in the descriptor heap.
		DX12::D3D::gpu_descriptor_heap_mgr->AllocateGroup(&s_group_base_texture_cpu_handle, num_handles, &s_group_base_texture_gpu_handle, nullptr, true);

		// Pave over space with null textures.
		for (unsigned int i = 0; i < (8 + last_Texture); i++)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE nullDestDescriptor;
			nullDestDescriptor.ptr = s_group_base_texture_cpu_handle.ptr + i * D3D::resource_descriptor_size;

			DX12::D3D::device12->CopyDescriptorsSimple(
				1,
				nullDestDescriptor,
				DX12::D3D::null_srv_cpu_shadow,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
		}

		// Future binding calls will not be the first texture in the group.. until stage == count, below.
		s_first_texture_in_group = false;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE textureDestDescriptor;
	textureDestDescriptor.ptr = s_group_base_texture_cpu_handle.ptr + stage * D3D::resource_descriptor_size;
	DX12::D3D::device12->CopyDescriptorsSimple(
		1,
		textureDestDescriptor,
		this->m_texture_srv_gpu_handle_cpu_shadow,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);

	if (m_nrm_texture && use_materials)
	{
		textureDestDescriptor.ptr = s_group_base_texture_cpu_handle.ptr + ((8 + stage) * D3D::resource_descriptor_size);
		DX12::D3D::device12->CopyDescriptorsSimple(
			1,
			textureDestDescriptor,
			this->m_nrm_texture_srv_gpu_handle_cpu_shadow,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
			);
	}

	// Stage is zero-based, count is one-based
	if (stage == last_Texture)
	{
		// On the last texture, we need to actually bind the table.
		DX12::D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV, s_group_base_texture_gpu_handle);
		if (g_ActiveConfig.TessellationEnabled())
		{
			DX12::D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_DS_SRV, s_group_base_texture_gpu_handle);
		}
		// Then mark that the next binding call will be the first texture in a group.
		s_first_texture_in_group = true;
	}
}

bool TextureCache::TCacheEntry::Save(const std::string& filename, unsigned int level)
{
	// EXISTINGD3D11TODO: Somehow implement this (D3DX11 doesn't support dumping individual LODs)
	static bool warn_once = true;
	if (level && warn_once)
	{
		WARN_LOG(VIDEO, "Dumping individual LOD not supported by D3D11 backend!");
		warn_once = false;
		return false;
	}

	D3D12_RESOURCE_DESC texture_desc = m_texture->GetTex()->GetDesc();

	const unsigned int required_readback_buffer_size = AlignValue(static_cast<unsigned int>(texture_desc.Width) * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	if (s_texture_cache_entry_readback_buffer_size < required_readback_buffer_size)
	{
		s_texture_cache_entry_readback_buffer_size = required_readback_buffer_size;

		// We know the readback buffer won't be in use right now, since we wait on this thread 
		// for the GPU to finish execution right after copying to it.

		SAFE_RELEASE(s_texture_cache_entry_readback_buffer);
	}

	if (!s_texture_cache_entry_readback_buffer_size)
	{
		CheckHR(
			D3D::device12->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(s_texture_cache_entry_readback_buffer_size),
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&s_texture_cache_entry_readback_buffer)
				)
			);

		CheckHR(s_texture_cache_entry_readback_buffer->Map(0, nullptr, &s_texture_cache_entry_readback_buffer_data));
	}

	bool saved_png = false;

	m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
	
	D3D12_TEXTURE_COPY_LOCATION dst_location = {};
	dst_location.pResource = s_texture_cache_entry_readback_buffer;
	dst_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst_location.PlacedFootprint.Offset = 0;
	dst_location.PlacedFootprint.Footprint.Depth = 1;
	dst_location.PlacedFootprint.Footprint.Format = texture_desc.Format;
	dst_location.PlacedFootprint.Footprint.Width = static_cast<UINT>(texture_desc.Width);
	dst_location.PlacedFootprint.Footprint.Height = texture_desc.Height;
	dst_location.PlacedFootprint.Footprint.RowPitch = AlignValue(dst_location.PlacedFootprint.Footprint.Width * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	D3D12_TEXTURE_COPY_LOCATION src_location = CD3DX12_TEXTURE_COPY_LOCATION(m_texture->GetTex(), 0);

	D3D::current_command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);

	D3D::command_list_mgr->ExecuteQueuedWork(true);

	saved_png = TextureToPng(
		static_cast<u8*>(s_texture_cache_entry_readback_buffer_data),
		dst_location.PlacedFootprint.Footprint.RowPitch,
		filename,
		dst_location.PlacedFootprint.Footprint.Width,
		dst_location.PlacedFootprint.Footprint.Height
		);

	return saved_png;
}

void TextureCache::TCacheEntry::CopyRectangleFromTexture(
	const TCacheEntryBase* source,
	const MathUtil::Rectangle<int>& src_rect,
	const MathUtil::Rectangle<int>& dst_rect)
{
	const TCacheEntry* srcentry = reinterpret_cast<const TCacheEntry*>(source);
	if (src_rect.GetWidth() == dst_rect.GetWidth()
		&& src_rect.GetHeight() == dst_rect.GetHeight())
	{
		const D3D12_BOX *psrcbox = nullptr;
		D3D12_BOX srcbox;
		if (src_rect.left != 0 
			|| src_rect.top != 0 
			|| src_rect.GetWidth() != srcentry->config.width 
			|| src_rect.GetHeight() != srcentry->config.height)
		{
			srcbox.left = src_rect.left;
			srcbox.top = src_rect.top;
			srcbox.right = src_rect.right;
			srcbox.bottom = src_rect.bottom;
			srcbox.front = 0;
			srcbox.back = 1;
			psrcbox = &srcbox;
		}
		
		D3D12_TEXTURE_COPY_LOCATION dst = CD3DX12_TEXTURE_COPY_LOCATION(m_texture->GetTex(), 0);
		D3D12_TEXTURE_COPY_LOCATION src = CD3DX12_TEXTURE_COPY_LOCATION(srcentry->m_texture->GetTex(), 0);

		m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_DEST);
		srcentry->m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);

		D3D::current_command_list->CopyTextureRegion(&dst, dst_rect.left, dst_rect.top, 0, &src, psrcbox);

		return;
	}
	else if (!config.rendertarget)
	{
		return;
	}

	D3D::SetViewportAndScissor(dst_rect.left, dst_rect.top, dst_rect.GetWidth(), dst_rect.GetHeight());

	m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &m_texture->GetRTV(), FALSE, nullptr);

	D3D::SetLinearCopySampler();
	D3D12_RECT srcRC;
	srcRC.left = src_rect.left;
	srcRC.right = src_rect.right;
	srcRC.top = src_rect.top;
	srcRC.bottom = src_rect.bottom;
	D3D::DrawShadedTexQuad(srcentry->m_texture, &srcRC,
		srcentry->config.width, srcentry->config.height,
		StaticShaderCache::GetColorCopyPixelShader(false),
		StaticShaderCache::GetSimpleVertexShader(),
		StaticShaderCache::GetSimpleVertexShaderInputLayout(), D3D12_SHADER_BYTECODE(), 1.0, 0,
		DXGI_FORMAT_R8G8B8A8_UNORM, false, m_texture->GetMultisampled());

	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV());

	g_renderer->RestoreAPIState();
}

void TextureCache::TCacheEntry::Load(const u8* src, u32 width, u32 height,
	u32 expanded_width, u32 level)
{
	D3D::ReplaceTexture2D(m_texture->GetTex(), src, DXGI_format, width, height, expanded_width, level, m_texture->GetResourceUsageState());
}

void TextureCache::TCacheEntry::LoadMaterialMap(const u8* src, u32 width, u32 height, u32 level)
{
	D3D::ReplaceTexture2D(m_nrm_texture->GetTex(), src, DXGI_format, width, height, width, level, m_nrm_texture->GetResourceUsageState());
}

void TextureCache::TCacheEntry::Load(const u8* src, u32 width, u32 height, u32 expandedWidth,
	u32 expandedHeight, const s32 texformat, const u32 tlutaddr, const TlutFormat tlutfmt, u32 level)
{
	TexDecoder_Decode(
		TextureCache::temp,
		src,
		expandedWidth,
		expandedHeight,
		texformat,
		tlutaddr,
		tlutfmt,
		DXGI_format == DXGI_FORMAT_R8G8B8A8_UNORM,
		compressed);
	u8* data = TextureCache::temp;
	if (is_scaled)
	{
		data = reinterpret_cast<u8*>(s_scaler->Scale(reinterpret_cast<u32*>(data), expandedWidth, height));
		width *= g_ActiveConfig.iTexScalingFactor;
		height *= g_ActiveConfig.iTexScalingFactor;
		expandedWidth *= g_ActiveConfig.iTexScalingFactor;
	}
	D3D::ReplaceTexture2D(m_texture->GetTex(), data, DXGI_format, width, height, expandedWidth, level, m_texture->GetResourceUsageState());
}
void TextureCache::TCacheEntry::LoadFromTmem(const u8* ar_src, const u8* gb_src, u32 width, u32 height,
	u32 expanded_width, u32 expanded_Height, u32 level)
{
	TexDecoder_DecodeRGBA8FromTmem(
		reinterpret_cast<u32*>(TextureCache::temp),
		ar_src,
		gb_src,
		expanded_width,
		expanded_Height);
	u8* data = TextureCache::temp;
	if (is_scaled)
	{
		data = reinterpret_cast<u8*>(s_scaler->Scale(reinterpret_cast<u32*>(data), expanded_width, height));
		width *= g_ActiveConfig.iTexScalingFactor;
		height *= g_ActiveConfig.iTexScalingFactor;
		expanded_width *= g_ActiveConfig.iTexScalingFactor;
	}
	D3D::ReplaceTexture2D(m_texture->GetTex(), data, DXGI_format, width, height, expanded_width, level, m_texture->GetResourceUsageState());
}

PC_TexFormat TextureCache::GetNativeTextureFormat(const s32 texformat, const TlutFormat tlutfmt, u32 width, u32 height)
{
	const bool compressed_supported = ((width & 3) == 0) && ((height & 3) == 0);
	PC_TexFormat pcfmt = GetPC_TexFormat(texformat, tlutfmt, compressed_supported);
	pcfmt = !g_ActiveConfig.backend_info.bSupportedFormats[pcfmt] ? PC_TEX_FMT_RGBA32 : pcfmt;
	return pcfmt;
}

TextureCacheBase::TCacheEntryBase* TextureCache::CreateTexture(const TCacheEntryConfig& config)
{
	if (config.rendertarget)
	{
		D3DTexture2D* texture = D3DTexture2D::Create(config.width, config.height,
			static_cast<D3D11_BIND_FLAG>((static_cast<int>(D3D11_BIND_RENDER_TARGET) | static_cast<int>(D3D11_BIND_SHADER_RESOURCE))),
			D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM, 1, config.layers);

		TCacheEntry* entry = new TCacheEntry(config, texture);

		entry->m_texture_srv_cpu_handle = texture->GetSRVCPU();
		entry->m_texture_srv_gpu_handle = texture->GetSRVGPU();
		entry->m_texture_srv_gpu_handle_cpu_shadow = texture->GetSRVGPUCPUShadow();

		return entry;
	}
	else
	{
		static const DXGI_FORMAT PC_TexFormat_To_DXGIFORMAT[11]
		{
			DXGI_FORMAT_UNKNOWN,//PC_TEX_FMT_NONE
			DXGI_FORMAT_R8G8B8A8_UNORM,//PC_TEX_FMT_BGRA32
			DXGI_FORMAT_R8G8B8A8_UNORM,//PC_TEX_FMT_RGBA32
			DXGI_FORMAT_R8G8B8A8_UNORM,//PC_TEX_FMT_I4_AS_I8
			DXGI_FORMAT_R8G8B8A8_UNORM,//PC_TEX_FMT_IA4_AS_IA8
			DXGI_FORMAT_R8G8B8A8_UNORM,//PC_TEX_FMT_I8
			DXGI_FORMAT_R8G8B8A8_UNORM,//PC_TEX_FMT_IA8
			DXGI_FORMAT_R8G8B8A8_UNORM,//PC_TEX_FMT_RGB565
			DXGI_FORMAT_BC1_UNORM,//PC_TEX_FMT_DXT1
			DXGI_FORMAT_BC2_UNORM,//PC_TEX_FMT_DXT3
			DXGI_FORMAT_BC3_UNORM,//PC_TEX_FMT_DXT5
		};
		DXGI_FORMAT format = PC_TexFormat_To_DXGIFORMAT[config.pcformat];
		ID3D12Resource* pTexture12 = nullptr;

		D3D12_RESOURCE_DESC texdesc12 = CD3DX12_RESOURCE_DESC::Tex2D(format,
			config.width, config.height, 1, config.levels);

		CheckHR(
			D3D::device12->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC(texdesc12),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(&pTexture12)
			)
			);

		D3DTexture2D* texture = new D3DTexture2D(
			pTexture12,
			D3D11_BIND_SHADER_RESOURCE,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			false,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			);

		TCacheEntry* const entry = new TCacheEntry(
			config, texture
			);

		entry->m_texture_srv_cpu_handle = texture->GetSRVCPU();
		entry->m_texture_srv_gpu_handle = texture->GetSRVGPU();
		entry->m_texture_srv_gpu_handle_cpu_shadow = texture->GetSRVGPUCPUShadow();
		entry->DXGI_format = format;
		if (format != DXGI_FORMAT_R8G8B8A8_UNORM)
		{
			entry->compressed = true;
		}
		// EXISTINGD3D11TODO: better debug names
		D3D::SetDebugObjectName12(entry->m_texture->GetTex(), "a texture of the TextureCache");

		SAFE_RELEASE(pTexture12);
		if (config.materialmap)
		{
			pTexture12 = nullptr;
			CheckHR(
				D3D::device12->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC(texdesc12),
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					nullptr,
					IID_PPV_ARGS(&pTexture12)
					)
				);
			entry->m_nrm_texture = new D3DTexture2D(
				pTexture12,
				D3D11_BIND_SHADER_RESOURCE,
				DXGI_FORMAT_UNKNOWN,
				DXGI_FORMAT_UNKNOWN,
				DXGI_FORMAT_UNKNOWN,
				false,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
				);
			entry->m_nrm_texture_srv_cpu_handle = entry->m_nrm_texture->GetSRVCPU();
			entry->m_nrm_texture_srv_gpu_handle = entry->m_nrm_texture->GetSRVGPU();
			entry->m_nrm_texture_srv_gpu_handle_cpu_shadow = entry->m_nrm_texture->GetSRVGPUCPUShadow();
			SAFE_RELEASE(pTexture12);
		}
		return entry;
	}
}

void TextureCache::TCacheEntry::FromRenderTarget(u8* dst, PEControl::PixelFormat src_format, const EFBRectangle& src_rect,
	bool scale_by_half, u32 cbuf_id, const float* colmat)
{
	// When copying at half size, in multisampled mode, resolve the color/depth buffer first.
	// This is because multisampled texture reads go through Load, not Sample, and the linear
	// filter is ignored.
	bool multisampled = (g_ActiveConfig.iMultisamples > 1);
	D3DTexture2D* efb_tex = (src_format == PEControl::Z24) ?
		FramebufferManager::GetEFBDepthTexture() :
		FramebufferManager::GetEFBColorTexture();
	if (multisampled && scale_by_half)
	{
		multisampled = false;
		efb_tex = (src_format == PEControl::Z24) ?
			FramebufferManager::GetResolvedEFBDepthTexture() :
			FramebufferManager::GetResolvedEFBColorTexture();
	}

	// set transformation
	if (s_efb_copy_last_cbuf_id != cbuf_id)
	{
		s_efb_copy_stream_buffer->AllocateSpaceInBuffer(28 * sizeof(float), 256);
		memcpy(s_efb_copy_stream_buffer->GetCPUAddressOfCurrentAllocation(), colmat, 28 * sizeof(float));
		s_efb_copy_last_cbuf_id = cbuf_id;
	}
	// stretch picture with increased internal resolution
	D3D::SetViewportAndScissor(0, 0, config.width, config.height);
	D3D::current_command_list->SetGraphicsRootConstantBufferView(DESCRIPTOR_TABLE_PS_CBVONE, s_efb_copy_stream_buffer->GetGPUAddressOfCurrentAllocation());
	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PS_CBV, true);

	const TargetRectangle targetSource = g_renderer->ConvertEFBRectangle(src_rect);
	// TODO: try targetSource.asRECT();
	const D3D11_RECT sourcerect = CD3D11_RECT(targetSource.left, targetSource.top, targetSource.right, targetSource.bottom);

	// Use linear filtering if (bScaleByHalf), use point filtering otherwise
	if (scale_by_half)
		D3D::SetLinearCopySampler();
	else
		D3D::SetPointCopySampler();

	// Make sure we don't draw with the texture set as both a source and target.
	// (This can happen because we don't unbind textures when we free them.)

	m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &m_texture->GetRTV(), FALSE, nullptr);

	// Create texture copy
	D3D::DrawShadedTexQuad(
		efb_tex,
		&sourcerect,
		Renderer::GetTargetWidth(),
		Renderer::GetTargetHeight(),
		(src_format == PEControl::Z24) ? StaticShaderCache::GetDepthMatrixPixelShader(multisampled) : StaticShaderCache::GetColorMatrixPixelShader(multisampled),
		StaticShaderCache::GetSimpleVertexShader(),
		StaticShaderCache::GetSimpleVertexShaderInputLayout(),
		StaticShaderCache::GetCopyGeometryShader(),
		1.0f, 0, DXGI_FORMAT_R8G8B8A8_UNORM, false, m_texture->GetMultisampled()
		);
	m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	
	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV());

	g_renderer->RestoreAPIState();
}

void TextureCache::CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
	PEControl::PixelFormat src_format, const EFBRectangle& src_rect,
	bool is_intensity, bool scale_by_half)
{
	s_encoder->Encode(dst, format, native_width, bytes_per_row, num_blocks_y, memory_stride, src_format, src_rect, is_intensity, scale_by_half);
}

static const constexpr char s_palette_shader_hlsl[] =
R"HLSL(
sampler samp0 : register(s0);
Texture2DArray Tex0 : register(t0);
Buffer<uint> Tex1 : register(t1);
uniform float Multiply;

uint Convert3To8(uint v)
{
	// Swizzle bits: 00000123 -> 12312312
	return (v << 5) | (v << 2) | (v >> 1);
}

uint Convert4To8(uint v)
{
	// Swizzle bits: 00001234 -> 12341234
	return (v << 4) | v;
}

uint Convert5To8(uint v)
{
	// Swizzle bits: 00012345 -> 12345123
	return (v << 3) | (v >> 2);
}

uint Convert6To8(uint v)
{
	// Swizzle bits: 00123456 -> 12345612
	return (v << 2) | (v >> 4);
}

float4 DecodePixel_RGB5A3(uint val)
{
	int r,g,b,a;
	if ((val&0x8000))
	{
		r=Convert5To8((val>>10) & 0x1f);
		g=Convert5To8((val>>5 ) & 0x1f);
		b=Convert5To8((val    ) & 0x1f);
		a=0xFF;
	}
	else
	{
		a=Convert3To8((val>>12) & 0x7);
		r=Convert4To8((val>>8 ) & 0xf);
		g=Convert4To8((val>>4 ) & 0xf);
		b=Convert4To8((val    ) & 0xf);
	}
	return float4(r, g, b, a) / 255;
}

float4 DecodePixel_RGB565(uint val)
{
	int r, g, b, a;
	r = Convert5To8((val >> 11) & 0x1f);
	g = Convert6To8((val >> 5) & 0x3f);
	b = Convert5To8((val) & 0x1f);
	a = 0xFF;
	return float4(r, g, b, a) / 255;
}

float4 DecodePixel_IA8(uint val)
{
	int i = val & 0xFF;
	int a = val >> 8;
	return float4(i, i, i, a) / 255;
}

void main(
	out float4 ocol0 : SV_Target,
	in float4 pos : SV_Position,
	in float3 uv0 : TEXCOORD0)
{
	uint src = round(Tex0.Sample(samp0,uv0) * Multiply).r;
	src = Tex1.Load(src);
	src = ((src << 8) & 0xFF00) | (src >> 8);
	ocol0 = DECODE(src);
}
)HLSL";

void TextureCache::LoadLut(u32 lutFmt, void* palette, u32 size) {
	
	m_lut_format = (TlutFormat)lutFmt;
	m_lut_size = size;
	if (m_lut_size > 512)
	{
		return;
	}
	// D3D12: Copy the palette into a free place in the palette_buf12 upload heap.
	// Only 1024 palette buffers are supported in flight at once (arbitrary, this should be plenty).
	const unsigned int palette_buffer_allocation_size = 512;
	m_palette_stream_buffer->AllocateSpaceInBuffer(palette_buffer_allocation_size, 256);
	memcpy(m_palette_stream_buffer->GetCPUAddressOfCurrentAllocation(), palette, palette_buffer_allocation_size);
}

bool TextureCache::Palettize(TCacheEntryBase* entry, const TCacheEntryBase* unconverted)
{
	if (m_lut_size > 512)
	{
		return false;
	}
	const TCacheEntry* base_entry = static_cast<const TCacheEntry*>(unconverted);
	// stretch picture with increased internal resolution
	// stretch picture with increased internal resolution
	D3D::SetViewportAndScissor(0, 0, unconverted->config.width, unconverted->config.height);

	// D3D12: Because the second SRV slot is occupied by this buffer, and an arbitrary texture occupies the first SRV slot,
	// we need to allocate temporary space out of our descriptor heap, place the palette SRV in the second slot, then copy the
	// existing texture's descriptor into the first slot.

	// First, allocate the (temporary) space in the descriptor heap.
	D3D12_CPU_DESCRIPTOR_HANDLE srv_group_cpu_handle[2] = {};
	D3D12_GPU_DESCRIPTOR_HANDLE srv_group_gpu_handle[2] = {};
	D3D::gpu_descriptor_heap_mgr->AllocateGroup(srv_group_cpu_handle, 2, srv_group_gpu_handle, nullptr, true);

	srv_group_cpu_handle[1].ptr = srv_group_cpu_handle[0].ptr + D3D::resource_descriptor_size;

	// Now, create the palette SRV at the appropriate offset.
	D3D12_SHADER_RESOURCE_VIEW_DESC palette_buffer_srv_desc = {
		DXGI_FORMAT_R16_UINT,                    // DXGI_FORMAT Format;
		D3D12_SRV_DIMENSION_BUFFER,              // D3D12_SRV_DIMENSION ViewDimension;
		D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING // UINT Shader4ComponentMapping;
	};
	// Each 'element' is two bytes since format is R16.
	palette_buffer_srv_desc.Buffer.FirstElement = m_palette_stream_buffer->GetOffsetOfCurrentAllocation() / sizeof(u16);
	palette_buffer_srv_desc.Buffer.NumElements = 256;

	D3D::device12->CreateShaderResourceView(m_palette_stream_buffer->GetBuffer(), &palette_buffer_srv_desc, srv_group_cpu_handle[1]);

	// Now, copy the existing texture's descriptor into the new temporary location.
	base_entry->m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D::device12->CopyDescriptorsSimple(
		1,
		srv_group_cpu_handle[0],
		base_entry->m_texture->GetSRVGPUCPUShadow(),
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);

	// Finally, bind our temporary location.
	D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV, srv_group_gpu_handle[0]);

	// D3D11EXISTINGTODO: Add support for C14X2 format.  (Different multiplier, more palette entries.)

	// D3D12: See TextureCache::TextureCache() - because there are only two possible buffer contents here,
	// just pre-populate the data in two parts of the same upload heap.
	if ((unconverted->format & 0xf) == GX_TF_I4)
	{
		D3D::current_command_list->SetGraphicsRootConstantBufferView(DESCRIPTOR_TABLE_PS_CBVONE, m_palette_uniform_buffer->GetGPUVirtualAddress());
	}
	else
	{
		D3D::current_command_list->SetGraphicsRootConstantBufferView(DESCRIPTOR_TABLE_PS_CBVONE, m_palette_uniform_buffer->GetGPUVirtualAddress() + 256);
	}

	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PS_CBV, true);

	const D3D11_RECT source_rect = CD3D11_RECT(0, 0, unconverted->config.width, unconverted->config.height);

	D3D::SetPointCopySampler();

	// Make sure we don't draw with the texture set as both a source and target.
	// (This can happen because we don't unbind textures when we free them.)

	static_cast<TCacheEntry*>(entry)->m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &static_cast<TCacheEntry*>(entry)->m_texture->GetRTV(), FALSE, nullptr);

	// Create texture copy
	D3D::DrawShadedTexQuad(
		base_entry->m_texture,
		&source_rect, unconverted->config.width,
		unconverted->config.height,
		m_palette_pixel_shaders[m_lut_format],
		StaticShaderCache::GetSimpleVertexShader(),
		StaticShaderCache::GetSimpleVertexShaderInputLayout(),
		StaticShaderCache::GetCopyGeometryShader(),
		1.0f,
		0,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		true,
		static_cast<TCacheEntry*>(entry)->m_texture->GetMultisampled()
		);

	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE );
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV());

	g_renderer->RestoreAPIState();
	return true;
}

D3D12_SHADER_BYTECODE GetConvertShader(const std::string& type)
{
	std::string shader = "#define DECODE DecodePixel_";
	shader.append(type);
	shader.append("\n");
	shader.append(s_palette_shader_hlsl);

	D3DBlob* Blob = nullptr;
	D3D::CompilePixelShader(shader, &Blob);

	return { Blob->Data(), Blob->Size() };
}

TextureCache::TextureCache()
{
	// FIXME: Is it safe here?
	s_encoder = std::make_unique<PSTextureEncoder>();
	s_encoder->Init();
	s_scaler = std::make_unique<TextureScaler>();	
	s_texture_cache_entry_readback_buffer = nullptr;
	s_texture_cache_entry_readback_buffer_data = nullptr;
	s_texture_cache_entry_readback_buffer_size = 0;

	s_efb_copy_stream_buffer = std::make_unique<D3DStreamBuffer>(1024 * 1024, 1024 * 1024, nullptr);
	s_efb_copy_last_cbuf_id = UINT_MAX;

	m_palette_pixel_shaders[GX_TL_IA8] = GetConvertShader("IA8");
	m_palette_pixel_shaders[GX_TL_RGB565] = GetConvertShader("RGB565");
	m_palette_pixel_shaders[GX_TL_RGB5A3] = GetConvertShader("RGB5A3");

	m_palette_stream_buffer = std::make_unique<D3DStreamBuffer>(sizeof(u16) * 256 * 1024, sizeof(u16) * 256 * 1024 * 16, nullptr);

	// Right now, there are only two variants of palette_uniform data. So, we'll just create an upload heap to permanently store both of these.
	CheckHR(
		D3D::device12->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(((16 + 255) & ~255) * 2), // Constant Buffers have to be 256b aligned. "* 2" to create for two sets of data.
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_palette_uniform_buffer)
		)
		);

	D3D::SetDebugObjectName12(m_palette_uniform_buffer, "a constant buffer used in TextureCache::ConvertTexture");

	// Temporarily repurpose m_palette_stream_buffer as a copy source to populate initial data here.
	m_palette_stream_buffer->AllocateSpaceInBuffer(256 * 2, 256);
	u8* upload_heap_data_location = reinterpret_cast<u8*>(m_palette_stream_buffer->GetCPUAddressOfCurrentAllocation());
	memset(upload_heap_data_location, 0, 256 * 2);

	float paramsFormatZero[4] = { 15.f };
	float paramsFormatNonzero[4] = { 255.f };

	memcpy(upload_heap_data_location, paramsFormatZero, sizeof(paramsFormatZero));
	memcpy(upload_heap_data_location + 256, paramsFormatNonzero, sizeof(paramsFormatNonzero));
	D3D::current_command_list->CopyBufferRegion(m_palette_uniform_buffer, 0, m_palette_stream_buffer->GetBuffer(), m_palette_stream_buffer->GetOffsetOfCurrentAllocation(), 256 * 2);
	DX12::D3D::ResourceBarrier(D3D::current_command_list, m_palette_uniform_buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, 0);
}

TextureCache::~TextureCache()
{
	s_encoder->Shutdown();
	s_encoder.reset();
	s_scaler.reset();

	s_efb_copy_stream_buffer.reset();
	m_palette_stream_buffer.reset();

	if (s_texture_cache_entry_readback_buffer)
	{
		D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(s_texture_cache_entry_readback_buffer);
		s_texture_cache_entry_readback_buffer = nullptr;
	}

	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_palette_uniform_buffer);
}

}
