// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

#include "Common/Align.h"

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

static std::unique_ptr<D3DStreamBuffer> s_efb_copy_stream_buffer = nullptr;
static u32 s_efb_copy_last_cbuf_id = UINT_MAX;

static ID3D12Resource* s_texture_cache_entry_readback_buffer = nullptr;
static size_t s_texture_cache_entry_readback_buffer_size = 0;

TextureCache::TCacheEntry::~TCacheEntry()
{
	m_texture->Release();
	SAFE_RELEASE(m_nrm_texture);
}

static D3D12_GPU_DESCRIPTOR_HANDLE s_group_base_texture_gpu_handle = { 0 };
static bool s_handle_changed = false;

D3D12_GPU_DESCRIPTOR_HANDLE TextureCache::GetTextureGroupHandle()
{
	D3D12_GPU_DESCRIPTOR_HANDLE Handle = { 0 };
	if (s_handle_changed)
	{
		s_handle_changed = false;
		Handle = s_group_base_texture_gpu_handle;
	}
	return Handle;
}

void TextureCache::TCacheEntry::Bind(unsigned int stage)
{

}

bool TextureCache::TCacheEntry::Save(const std::string& filename, unsigned int level)
{
	u32 level_width = std::max(config.width >> level, 1u);
	u32 level_height = std::max(config.height >> level, 1u);
	size_t level_pitch = level_width;
	size_t num_lines = level_height;
	if (this->compressed)
	{
		level_pitch = (level_pitch + 3) >> 2;
		level_pitch *= 16; // Size of the bc2 block
		num_lines = (num_lines + 3) >> 2;
	}
	else
	{
		level_pitch *= sizeof(u32);
	}
	level_pitch = Common::AlignUpSizePow2(level_pitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	size_t required_readback_buffer_size = level_pitch * num_lines;

	// Check if the current readback buffer is large enough
	if (required_readback_buffer_size > s_texture_cache_entry_readback_buffer_size)
	{
		// Reallocate the buffer with the new size. Safe to immediately release because we're the only user and we block until completion.
		if (s_texture_cache_entry_readback_buffer)
			s_texture_cache_entry_readback_buffer->Release();

		s_texture_cache_entry_readback_buffer_size = required_readback_buffer_size;
		CheckHR(D3D::device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(s_texture_cache_entry_readback_buffer_size),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&s_texture_cache_entry_readback_buffer)));
	}

	m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_TEXTURE_COPY_LOCATION dst_location = {};
	dst_location.pResource = s_texture_cache_entry_readback_buffer;
	dst_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst_location.PlacedFootprint.Offset = 0;
	dst_location.PlacedFootprint.Footprint.Depth = 1;
	dst_location.PlacedFootprint.Footprint.Format = this->DXGI_format;
	dst_location.PlacedFootprint.Footprint.Width = level_width;
	dst_location.PlacedFootprint.Footprint.Height = level_height;
	dst_location.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(level_pitch);

	D3D12_TEXTURE_COPY_LOCATION src_location = CD3DX12_TEXTURE_COPY_LOCATION(m_texture->GetTex(), level);

	D3D::current_command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);

	D3D::command_list_mgr->ExecuteQueuedWork(true);

	// Map readback buffer and save to file.
	void* readback_texture_map;
	D3D12_RANGE read_range = { 0, required_readback_buffer_size };
	CheckHR(s_texture_cache_entry_readback_buffer->Map(0, &read_range, &readback_texture_map));

	bool saved = false;
	if (this->compressed)
	{
		saved = TextureToDDS(
			static_cast<u8*>(readback_texture_map),
			dst_location.PlacedFootprint.Footprint.RowPitch,
			filename,
			dst_location.PlacedFootprint.Footprint.Width,
			dst_location.PlacedFootprint.Footprint.Height
		);
	}
	else
	{
		saved = TextureToPng(
			static_cast<u8*>(readback_texture_map),
			dst_location.PlacedFootprint.Footprint.RowPitch,
			filename,
			dst_location.PlacedFootprint.Footprint.Width,
			dst_location.PlacedFootprint.Footprint.Height
		);
	}
	m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D12_RANGE write_range = {};
	s_texture_cache_entry_readback_buffer->Unmap(0, &write_range);
	return saved;
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
		CD3DX12_BOX src_box(src_rect.left, src_rect.top, 0, src_rect.right, src_rect.bottom, srcentry->config.layers);

		D3D12_TEXTURE_COPY_LOCATION dst = CD3DX12_TEXTURE_COPY_LOCATION(m_texture->GetTex(), 0);
		D3D12_TEXTURE_COPY_LOCATION src = CD3DX12_TEXTURE_COPY_LOCATION(srcentry->m_texture->GetTex(), 0);

		m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_DEST);
		srcentry->m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);

		D3D::current_command_list->CopyTextureRegion(&dst, dst_rect.left, dst_rect.top, 0, &src, &src_box);

		return;
	}
	else if (!config.rendertarget)
	{
		config.rendertarget = true;
		D3DTexture2D* ptexture = D3DTexture2D::Create(config.width, config.height,
			TEXTURE_BIND_FLAG_SHADER_RESOURCE | TEXTURE_BIND_FLAG_RENDER_TARGET,
			DXGI_FORMAT_R8G8B8A8_UNORM, 1, config.layers);
		ptexture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_DEST);
		m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
		D3D::current_command_list->CopyResource(ptexture->GetTex(), m_texture->GetTex());
		m_texture->Release();
		m_texture = ptexture;
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
		StaticShaderCache::GetSimpleVertexShaderInputLayout(), StaticShaderCache::GetCopyGeometryShader(), 0,
		DXGI_FORMAT_R8G8B8A8_UNORM, false, m_texture->GetMultisampled());
	m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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

PC_TexFormat TextureCache::GetNativeTextureFormat(const s32 texformat, const TlutFormat tlutfmt, u32 width, u32 height)
{
	const bool compressed_supported = ((width & 3) == 0) && ((height & 3) == 0);
	PC_TexFormat pcfmt = GetPC_TexFormat(texformat, tlutfmt, compressed_supported);
	pcfmt = !g_ActiveConfig.backend_info.bSupportedFormats[pcfmt] ? PC_TEX_FMT_RGBA32 : pcfmt;
	return pcfmt;
}

TextureCacheBase::TCacheEntryBase* TextureCache::CreateTexture(const TCacheEntryConfig& config)
{
	static const DXGI_FORMAT PC_TexFormat_To_DXGIFORMAT[]
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
		DXGI_FORMAT_R32_FLOAT,//PC_TEX_FMT_DEPTH_FLOAT
		DXGI_FORMAT_R32_FLOAT,//PC_TEX_FMT_R_FLOAT
		DXGI_FORMAT_R16G16B16A16_FLOAT,//PC_TEX_FMT_RGBA16_FLOAT
		DXGI_FORMAT_R32G32B32A32_FLOAT,//PC_TEX_FMT_RGBA_FLOAT
	};
	if (config.rendertarget)
	{
		D3DTexture2D* texture = D3DTexture2D::Create(config.width, config.height,
			TEXTURE_BIND_FLAG_SHADER_RESOURCE | TEXTURE_BIND_FLAG_RENDER_TARGET,
			PC_TexFormat_To_DXGIFORMAT[config.pcformat], 1, config.layers);

		TCacheEntry* entry = new TCacheEntry(config, texture);
		return entry;
	}
	else
	{
		DXGI_FORMAT format = PC_TexFormat_To_DXGIFORMAT[config.pcformat];
		ComPtr<ID3D12Resource> pTexture;

		D3D12_RESOURCE_DESC texdesc12 = CD3DX12_RESOURCE_DESC::Tex2D(format,
			config.width, config.height, 1, config.levels);

		CheckHR(
			D3D::device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC(texdesc12),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				nullptr,
				IID_PPV_ARGS(pTexture.ReleaseAndGetAddressOf())
			)
		);

		D3DTexture2D* texture = new D3DTexture2D(
			pTexture.Get(),
			TEXTURE_BIND_FLAG_SHADER_RESOURCE,
			format,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			false,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		);

		TCacheEntry* const entry = new TCacheEntry(
			config, texture
		);

		entry->DXGI_format = format;
		if (format != DXGI_FORMAT_R8G8B8A8_UNORM)
		{
			entry->compressed = true;
		}
		// EXISTINGD3D11TODO: better debug names
		D3D::SetDebugObjectName12(entry->m_texture->GetTex(), "a texture of the TextureCache");

		if (config.materialmap)
		{
			CheckHR(
				D3D::device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC(texdesc12),
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					nullptr,
					IID_PPV_ARGS(pTexture.ReleaseAndGetAddressOf())
				)
			);
			entry->m_nrm_texture = new D3DTexture2D(
				pTexture.Get(),
				TEXTURE_BIND_FLAG_SHADER_RESOURCE,
				format,
				DXGI_FORMAT_UNKNOWN,
				DXGI_FORMAT_UNKNOWN,
				DXGI_FORMAT_UNKNOWN,
				false,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			);
		}
		return entry;
	}
}

void TextureCache::TCacheEntry::FromRenderTarget(bool is_depth_copy, const EFBRectangle& src_rect,
	bool scale_by_half, u32 cbuf_id, const float* colmat, u32 width, u32 height)
{
	// When copying at half size, in multisampled mode, resolve the color/depth buffer first.
	// This is because multisampled texture reads go through Load, not Sample, and the linear
	// filter is ignored.
	bool multisampled = (g_ActiveConfig.iMultisamples > 1);
	D3DTexture2D* efb_tex = is_depth_copy ?
		FramebufferManager::GetEFBDepthTexture() :
		FramebufferManager::GetEFBColorTexture();
	const TargetRectangle targetSource = g_renderer->ConvertEFBRectangle(src_rect);
	if (multisampled && scale_by_half)
	{
		multisampled = false;
		efb_tex = is_depth_copy ?
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
	D3D::SetViewportAndScissor(0, 0, width, height);
	D3D::current_command_list->SetGraphicsRootConstantBufferView(DESCRIPTOR_TABLE_PS_CBVONE, s_efb_copy_stream_buffer->GetGPUAddressOfCurrentAllocation());
	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PS_CBV, true);


	// TODO: try targetSource.asRECT();
	const D3D12_RECT sourcerect = CD3DX12_RECT(targetSource.left, targetSource.top, targetSource.right, targetSource.bottom);

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
		g_renderer->GetTargetWidth(),
		g_renderer->GetTargetHeight(),
		is_depth_copy ? StaticShaderCache::GetDepthMatrixPixelShader(multisampled)
		: StaticShaderCache::GetColorMatrixPixelShader(multisampled),
		StaticShaderCache::GetSimpleVertexShader(),
		StaticShaderCache::GetSimpleVertexShaderInputLayout(),
		StaticShaderCache::GetCopyGeometryShader(),
		0, DXGI_FORMAT_R8G8B8A8_UNORM, false, m_texture->GetMultisampled()
	);
	m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	g_renderer->RestoreAPIState();
}

void TextureCache::CopyEFB(u8* dst, const EFBCopyFormat& format, u32 native_width,
	u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
	bool is_depth_copy, const EFBRectangle& src_rect, bool scale_by_half)
{
	s_encoder->Encode(dst,
		format,
		native_width,
		bytes_per_row,
		num_blocks_y,
		memory_stride,
		is_depth_copy,
		src_rect,
		scale_by_half);
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

void TextureCache::LoadLut(u32 lutFmt, void* palette, u32 size)
{
	if (m_lut_size > 512)
	{
		return;
	}
	if (lutFmt == m_lut_format && palette == m_addr && size == m_lut_size && m_hash)
	{
		u64 hash = GetHash64(reinterpret_cast<u8*>(palette), size, g_ActiveConfig.iSafeTextureCache_ColorSamples);
		if (hash == m_hash)
		{
			return;
		}
		m_hash = hash;
	}
	else
	{
		m_hash = GetHash64(reinterpret_cast<u8*>(palette), size, g_ActiveConfig.iSafeTextureCache_ColorSamples);
	}
	m_lut_format = (TlutFormat)lutFmt;
	m_lut_size = size;
	m_addr = palette;

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

	// D3D12: Because the second SRV slot is occupied by this buffer, and an arbitrary texture occupies the first SRV slot,
	// we need to allocate temporary space out of our descriptor heap, place the palette SRV in the second slot, then copy the
	// existing texture's descriptor into the first slot.

	// First, allocate the (temporary) space in the descriptor heap.
	D3D12_CPU_DESCRIPTOR_HANDLE srv_group_cpu_handle[2] = {};
	D3D12_GPU_DESCRIPTOR_HANDLE srv_group_gpu_handle[2] = {};
	if (!D3D::gpu_descriptor_heap_mgr->AllocateTemporary(2, srv_group_cpu_handle, srv_group_gpu_handle))
	{
		D3D::command_list_mgr->ExecuteQueuedWork();
		if (!D3D::gpu_descriptor_heap_mgr->AllocateTemporary(2, srv_group_cpu_handle, srv_group_gpu_handle))
		{
			PanicAlert("Failed to allocate temporary descriptors.");
			return false;
		}
	}

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

	D3D::device->CreateShaderResourceView(m_palette_stream_buffer->GetBuffer(), &palette_buffer_srv_desc, srv_group_cpu_handle[1]);

	// Now, copy the existing texture's descriptor into the new temporary location.
	base_entry->m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D::device->CopyDescriptorsSimple(
		1,
		srv_group_cpu_handle[0],
		base_entry->m_texture->GetSRVCPUShadow(),
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	);

	// Finally, bind our temporary location.
	D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV, srv_group_gpu_handle[0]);

	// D3D11EXISTINGTODO: Add support for C14X2 format.  (Different multiplier, more palette entries.)

	// D3D12: See TextureCache::TextureCache() - because there are only two possible buffer contents here,
	// just pre-populate the data in two parts of the same upload heap.
	if ((unconverted->mem_format & 0xf) == GX_TF_I4)
	{
		D3D::current_command_list->SetGraphicsRootConstantBufferView(DESCRIPTOR_TABLE_PS_CBVONE, m_palette_uniform_buffer->GetGPUVirtualAddress());
	}
	else
	{
		D3D::current_command_list->SetGraphicsRootConstantBufferView(DESCRIPTOR_TABLE_PS_CBVONE, m_palette_uniform_buffer->GetGPUVirtualAddress() + 256);
	}

	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PS_CBV, true);

	const D3D12_RECT source_rect = CD3DX12_RECT(0, 0, unconverted->config.width, unconverted->config.height);

	D3D::SetPointCopySampler();

	// Make sure we don't draw with the texture set as both a source and target.
	// (This can happen because we don't unbind textures when we free them.)

	static_cast<TCacheEntry*>(entry)->m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &static_cast<TCacheEntry*>(entry)->m_texture->GetRTV(), FALSE, nullptr);
	// stretch picture with increased internal resolution
	// stretch picture with increased internal resolution
	D3D::SetViewportAndScissor(0, 0, unconverted->config.width, unconverted->config.height);
	// Create texture copy
	D3D::DrawShadedTexQuad(
		base_entry->m_texture,
		&source_rect, unconverted->config.width,
		unconverted->config.height,
		m_palette_pixel_shaders[m_lut_format],
		StaticShaderCache::GetSimpleVertexShader(),
		StaticShaderCache::GetSimpleVertexShaderInputLayout(),
		StaticShaderCache::GetCopyGeometryShader(),
		0,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		true,
		static_cast<TCacheEntry*>(entry)->m_texture->GetMultisampled()
	);
	static_cast<TCacheEntry*>(entry)->m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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

	return{ Blob->Data(), Blob->Size() };
}

TextureCache::TextureCache()
{
	// FIXME: Is it safe here?
	s_encoder = std::make_unique<PSTextureEncoder>();
	s_encoder->Init();
	s_texture_cache_entry_readback_buffer = nullptr;
	s_texture_cache_entry_readback_buffer_size = 0;

	s_efb_copy_stream_buffer = std::make_unique<D3DStreamBuffer>(1024 * 1024, 1024 * 1024, nullptr);
	s_efb_copy_last_cbuf_id = UINT_MAX;

	m_palette_pixel_shaders[GX_TL_IA8] = GetConvertShader("IA8");
	m_palette_pixel_shaders[GX_TL_RGB565] = GetConvertShader("RGB565");
	m_palette_pixel_shaders[GX_TL_RGB5A3] = GetConvertShader("RGB5A3");

	m_palette_stream_buffer = std::make_unique<D3DStreamBuffer>(sizeof(u16) * 256 * 1024, sizeof(u16) * 256 * 1024 * 16, nullptr);

	// Right now, there are only two variants of palette_uniform data. So, we'll just create an upload heap to permanently store both of these.
	CheckHR(
		D3D::device->CreateCommittedResource(
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

	s_efb_copy_stream_buffer.reset();
	m_palette_stream_buffer.reset();

	if (s_texture_cache_entry_readback_buffer)
	{
		// Safe to destroy the readback buffer immediately, as the only time it's used is blocked until completion.
		s_texture_cache_entry_readback_buffer->Release();
		s_texture_cache_entry_readback_buffer = nullptr;
		s_texture_cache_entry_readback_buffer_size = 0;
	}

	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_palette_uniform_buffer);
}

void TextureCache::BindTextures()
{
	const bool use_materials = g_ActiveConfig.HiresMaterialMapsEnabled();
	unsigned int last_texture = 0;
	for (unsigned int i = 0; i < 8; ++i)
	{
		if (bound_textures[i] != nullptr)
		{
			last_texture = i;
		}
	}

	if (last_texture == 0 && bound_textures[0] != nullptr && reinterpret_cast<TCacheEntry*>(bound_textures[0])->m_nrm_texture == nullptr)
	{
		DX12::D3D::current_command_list->SetGraphicsRootDescriptorTable(
			DESCRIPTOR_TABLE_PS_SRV,
			reinterpret_cast<TCacheEntry*>(bound_textures[0])->m_texture->GetSRVGPU());
		return;
	}

	// If more than one texture, allocate space for group.
	D3D12_CPU_DESCRIPTOR_HANDLE s_group_base_texture_cpu_handle;
	D3D12_GPU_DESCRIPTOR_HANDLE s_group_base_texture_gpu_handle;
	const unsigned int num_handles = use_materials ? 16 : 8;
	if (!D3D::gpu_descriptor_heap_mgr->AllocateTemporary(num_handles, &s_group_base_texture_cpu_handle, &s_group_base_texture_gpu_handle))
	{
		// Kick command buffer before attempting to allocate again. This is slow.
		D3D::command_list_mgr->ExecuteQueuedWork();
		if (!D3D::gpu_descriptor_heap_mgr->AllocateTemporary(num_handles, &s_group_base_texture_cpu_handle, &s_group_base_texture_gpu_handle))
		{
			PanicAlert("Failed to allocate temporary descriptors.");
			return;
		}
	}

	for (unsigned int stage = 0; stage < 8; stage++)
	{
		if (bound_textures[stage] != nullptr)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE textureDestDescriptor;
			textureDestDescriptor.ptr =
				s_group_base_texture_cpu_handle.ptr + stage * D3D::resource_descriptor_size;

			DX12::D3D::device->CopyDescriptorsSimple(
				1, textureDestDescriptor, reinterpret_cast<TCacheEntry*>(bound_textures[stage])
				->m_texture->GetSRVCPUShadow(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			if (reinterpret_cast<TCacheEntry*>(bound_textures[stage])->m_nrm_texture && use_materials)
			{
				textureDestDescriptor.ptr = s_group_base_texture_cpu_handle.ptr + ((8 + stage) * D3D::resource_descriptor_size);
				DX12::D3D::device->CopyDescriptorsSimple(
					1,
					textureDestDescriptor,
					reinterpret_cast<TCacheEntry*>(bound_textures[stage])->m_nrm_texture->GetSRVCPUShadow(),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
			}
		}
		else
		{
			D3D12_CPU_DESCRIPTOR_HANDLE nullDestDescriptor;
			nullDestDescriptor.ptr =
				s_group_base_texture_cpu_handle.ptr + stage * D3D::resource_descriptor_size;

			DX12::D3D::device->CopyDescriptorsSimple(1, nullDestDescriptor,
				DX12::D3D::null_srv_cpu_shadow,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			if (use_materials)
			{
				nullDestDescriptor.ptr = s_group_base_texture_cpu_handle.ptr + ((8 + stage) * D3D::resource_descriptor_size);
				DX12::D3D::device->CopyDescriptorsSimple(
					1,
					nullDestDescriptor,
					DX12::D3D::null_srv_cpu_shadow,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
			}
		}
	}

	// Actually bind the textures.
	DX12::D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV,
		s_group_base_texture_gpu_handle);
}

}
