// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DShader.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/GeometryShaderCache.h"
#include "VideoBackends/D3D12/PixelShaderCache.h"
#include "VideoBackends/D3D12/PSTextureEncoder.h"
#include "VideoBackends/D3D12/TextureCache.h"
#include "VideoBackends/D3D12/TextureEncoder.h"
#include "VideoBackends/D3D12/VertexShaderCache.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/TextureScalerCommon.h"

namespace DX12
{

static std::unique_ptr<TextureEncoder> s_encoder;
static std::unique_ptr<TextureScaler> s_scaler;

const size_t MAX_COPY_BUFFERS = 32;
ID3D12Resource* efbcopycbuf12[MAX_COPY_BUFFERS] = { 0 };

ID3D12Resource* pTCacheEntrySaveReadbackBuffer = nullptr;
void* pTCacheEntrySaveReadbackBufferData = nullptr;
UINT pTCacheEntrySaveReadbackBufferSize = 0;

TextureCache::TCacheEntry::~TCacheEntry()
{
	texture->Release();
	SAFE_RELEASE(nrm_texture);
}

bool firstTextureInGroup = true;
D3D12_CPU_DESCRIPTOR_HANDLE groupBaseTextureCpuHandle;
D3D12_GPU_DESCRIPTOR_HANDLE groupBaseTextureGpuHandle;

void TextureCache::TCacheEntry::Bind(unsigned int stage, unsigned int lastTexture)
{
	const bool use_materials = g_ActiveConfig.HiresMaterialMapsEnabled();
	if (lastTexture == 0 && !use_materials)
	{
		DX12::D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV, this->srvGpuHandle);
		return;
	}
	
	if (firstTextureInGroup)
	{
		const unsigned int num_handles = use_materials ? 16 : 8;
		// On the first texture in the group, we need to allocate the space in the descriptor heap.
		DX12::D3D::gpu_descriptor_heap_mgr->AllocateGroup(&groupBaseTextureCpuHandle, num_handles, &groupBaseTextureGpuHandle, nullptr, true);

		// Pave over space with null textures.
		for (unsigned int i = 0; i < (8 + lastTexture); i++)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE nullDestDescriptor;
			nullDestDescriptor.ptr = groupBaseTextureCpuHandle.ptr + i * D3D::resource_descriptor_size;

			DX12::D3D::device12->CopyDescriptorsSimple(
				1,
				nullDestDescriptor,
				DX12::D3D::null_srv_cpu_shadow,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
		}

		// Future binding calls will not be the first texture in the group.. until stage == count, below.
		firstTextureInGroup = false;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE textureDestDescriptor;
	textureDestDescriptor.ptr = groupBaseTextureCpuHandle.ptr + stage * D3D::resource_descriptor_size;
	DX12::D3D::device12->CopyDescriptorsSimple(
		1,
		textureDestDescriptor,
		this->srvGpuHandleCpuShadow,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);

	if (nrm_texture && use_materials)
	{
		textureDestDescriptor.ptr = groupBaseTextureCpuHandle.ptr + ((8 + stage) * D3D::resource_descriptor_size);
		DX12::D3D::device12->CopyDescriptorsSimple(
			1,
			textureDestDescriptor,
			this->nrm_srvGpuHandleCpuShadow,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
			);
	}

	// Stage is zero-based, count is one-based
	if (stage == lastTexture)
	{
		// On the last texture, we need to actually bind the table.
		DX12::D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV, groupBaseTextureGpuHandle);

		// Then mark that the next binding call will be the first texture in a group.
		firstTextureInGroup = true;
	}
}

bool TextureCache::TCacheEntry::Save(const std::string& filename, unsigned int level)
{
	// TODO: Somehow implement this (D3DX12 doesn't support dumping individual LODs)
	static bool warn_once = true;
	if (level && warn_once)
	{
		WARN_LOG(VIDEO, "Dumping individual LOD not supported by D3D11 backend!");
		warn_once = false;
		return false;
	}

	D3D12_RESOURCE_DESC textureDesc = texture->GetTex12()->GetDesc();

	UINT requiredReadbackBufferSize = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT + ((textureDesc.Width * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)) * textureDesc.Height;

	if (pTCacheEntrySaveReadbackBufferSize < requiredReadbackBufferSize)
	{
		pTCacheEntrySaveReadbackBufferSize = requiredReadbackBufferSize;

		// We know the readback buffer won't be in use right now, since we wait on this thread 
		// for the GPU to finish execution right after copying to it.

		SAFE_RELEASE(pTCacheEntrySaveReadbackBuffer);
	}

	if (!pTCacheEntrySaveReadbackBuffer)
	{
		CheckHR(
			D3D::device12->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(pTCacheEntrySaveReadbackBufferSize),
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&pTCacheEntrySaveReadbackBuffer)
				)
			);

		CheckHR(pTCacheEntrySaveReadbackBuffer->Map(0, nullptr, &pTCacheEntrySaveReadbackBufferData));
	}

	bool saved_png = false;

	texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
	
	D3D12_TEXTURE_COPY_LOCATION dst = {};
	dst.pResource = pTCacheEntrySaveReadbackBuffer;
	dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst.PlacedFootprint.Offset = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
	dst.PlacedFootprint.Footprint.Depth = 1;
	dst.PlacedFootprint.Footprint.Format = textureDesc.Format;
	dst.PlacedFootprint.Footprint.Width = static_cast<UINT>(textureDesc.Width);
	dst.PlacedFootprint.Footprint.Height = textureDesc.Height;
	dst.PlacedFootprint.Footprint.RowPitch = ((textureDesc.Width * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1));
	
	D3D12_TEXTURE_COPY_LOCATION src = CD3DX12_TEXTURE_COPY_LOCATION(texture->GetTex12(), 0);
	
	D3D::current_command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

	D3D::command_list_mgr->ExecuteQueuedWork(true);

	saved_png = TextureToPng(
		(u8*)pTCacheEntrySaveReadbackBufferData + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT,
		dst.PlacedFootprint.Footprint.RowPitch,
		filename,
		dst.PlacedFootprint.Footprint.Width,
		dst.PlacedFootprint.Footprint.Height
		);

	return saved_png;
}

void TextureCache::TCacheEntry::CopyRectangleFromTexture(
	const TCacheEntryBase* source,
	const MathUtil::Rectangle<int> &srcrect,
	const MathUtil::Rectangle<int> &dstrect)
{
	TCacheEntry* srcentry = (TCacheEntry*)source;
	if (srcrect.GetWidth() == dstrect.GetWidth()
		&& srcrect.GetHeight() == dstrect.GetHeight())
	{
		const D3D12_BOX *psrcbox = nullptr;
		D3D12_BOX srcbox;
		if (srcrect.left != 0 
			|| srcrect.top != 0 
			|| srcrect.GetWidth() != srcentry->config.width 
			|| srcrect.GetHeight() != srcentry->config.height)
		{
			srcbox.left = srcrect.left;
			srcbox.top = srcrect.top;
			srcbox.right = srcrect.right;
			srcbox.bottom = srcrect.bottom;
			srcbox.front = 0;
			srcbox.back = 1;
			psrcbox = &srcbox;
		}
		
		D3D12_TEXTURE_COPY_LOCATION dst = CD3DX12_TEXTURE_COPY_LOCATION(texture->GetTex12(), 0);
		D3D12_TEXTURE_COPY_LOCATION src = CD3DX12_TEXTURE_COPY_LOCATION(srcentry->texture->GetTex12(), 0);

		texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_DEST);
		srcentry->texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);

		D3D::current_command_list->CopyTextureRegion(&dst, dstrect.left, dstrect.top, 0, &src, psrcbox);

		return;
	}
	else if (!config.rendertarget)
	{
		return;
	}

	const D3D12_VIEWPORT vp12 = {
		float(dstrect.left),
		float(dstrect.top),
		float(dstrect.GetWidth()),
		float(dstrect.GetHeight()),
		D3D12_MIN_DEPTH,
		D3D12_MAX_DEPTH
	};
	D3D::current_command_list->RSSetViewports(1, &vp12);

	texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &texture->GetRTV12(), FALSE, nullptr);

	D3D::SetLinearCopySampler();
	D3D12_RECT srcRC;
	srcRC.left = srcrect.left;
	srcRC.right = srcrect.right;
	srcRC.top = srcrect.top;
	srcRC.bottom = srcrect.bottom;
	D3D::DrawShadedTexQuad(srcentry->texture, &srcRC,
		srcentry->config.width, srcentry->config.height,
		PixelShaderCache::GetColorCopyProgram12(false),
		VertexShaderCache::GetSimpleVertexShader12(),
		VertexShaderCache::GetSimpleInputLayout12(), D3D12_SHADER_BYTECODE(), 1.0, 0,
		DXGI_FORMAT_R8G8B8A8_UNORM, false, texture->GetMultisampled());

	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());

	g_renderer->RestoreAPIState();
}

void TextureCache::TCacheEntry::Load(const u8* src, u32 width, u32 height,
	u32 expanded_width, u32 level)
{
	D3D::ReplaceTexture2D(texture->GetTex12(), src, DXGI_format, width, height, expanded_width, level, texture->GetResourceUsageState());
}

void TextureCache::TCacheEntry::LoadMaterialMap(const u8* src, u32 width, u32 height, u32 level)
{
	D3D::ReplaceTexture2D(nrm_texture->GetTex12(), src, DXGI_format, width, height, width, level, nrm_texture->GetResourceUsageState());
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
		data = (u8*)s_scaler->Scale((u32*)data, expandedWidth, height);
		width *= g_ActiveConfig.iTexScalingFactor;
		height *= g_ActiveConfig.iTexScalingFactor;
		expandedWidth *= g_ActiveConfig.iTexScalingFactor;
	}
	D3D::ReplaceTexture2D(texture->GetTex12(), data, DXGI_format, width, height, expandedWidth, level, texture->GetResourceUsageState());
}
void TextureCache::TCacheEntry::LoadFromTmem(const u8* ar_src, const u8* gb_src, u32 width, u32 height,
	u32 expanded_width, u32 expanded_Height, u32 level)
{
	TexDecoder_DecodeRGBA8FromTmem(
		(u32*)TextureCache::temp,
		ar_src,
		gb_src,
		expanded_width,
		expanded_Height);
	u8* data = TextureCache::temp;
	if (is_scaled)
	{
		data = (u8*)s_scaler->Scale((u32*)data, expanded_width, height);
		width *= g_ActiveConfig.iTexScalingFactor;
		height *= g_ActiveConfig.iTexScalingFactor;
		expanded_width *= g_ActiveConfig.iTexScalingFactor;
	}
	D3D::ReplaceTexture2D(texture->GetTex12(), data, DXGI_format, width, height, expanded_width, level, texture->GetResourceUsageState());
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
			(D3D11_BIND_FLAG)((int)D3D11_BIND_RENDER_TARGET | (int)D3D11_BIND_SHADER_RESOURCE),
			D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM, 1, config.layers);

		TCacheEntry* entry = new TCacheEntry(config, texture);

		entry->srvCpuHandle = texture->GetSRV12CPU();
		entry->srvGpuHandle = texture->GetSRV12GPU();
		entry->srvGpuHandleCpuShadow = texture->GetSRV12GPUCPUShadow();

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

		entry->srvCpuHandle = texture->GetSRV12CPU();
		entry->srvGpuHandle = texture->GetSRV12GPU();
		entry->srvGpuHandleCpuShadow = texture->GetSRV12GPUCPUShadow();
		entry->DXGI_format = format;
		if (format != DXGI_FORMAT_R8G8B8A8_UNORM)
		{
			entry->compressed = true;
		}
		// TODO: better debug names
		D3D::SetDebugObjectName12(entry->texture->GetTex12(), "a texture of the TextureCache");

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
			entry->nrm_texture = new D3DTexture2D(
				pTexture12,
				D3D11_BIND_SHADER_RESOURCE,
				DXGI_FORMAT_UNKNOWN,
				DXGI_FORMAT_UNKNOWN,
				DXGI_FORMAT_UNKNOWN,
				false,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
				);
			entry->nrm_srvCpuHandle = entry->nrm_texture->GetSRV12CPU();
			entry->nrm_srvGpuHandle = entry->nrm_texture->GetSRV12GPU();
			entry->nrm_srvGpuHandleCpuShadow = entry->nrm_texture->GetSRV12GPUCPUShadow();
			SAFE_RELEASE(pTexture12);
		}
		return entry;
	}
}

void TextureCache::TCacheEntry::FromRenderTarget(u8* dst, PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
	bool scaleByHalf, unsigned int cbufid, const float *colmat)
{
	// stretch picture with increased internal resolution
	const D3D12_VIEWPORT vp12 = { 0.f, 0.f, (float)config.width, (float)config.height, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D::current_command_list->RSSetViewports(1, &vp12);

	// set transformation
	if (nullptr == efbcopycbuf12[cbufid])
	{
		CheckHR(
			D3D::device12->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(28 * sizeof(float)),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&efbcopycbuf12[cbufid])
				)
			);

		void* pData = nullptr;
		CheckHR(efbcopycbuf12[cbufid]->Map(0, nullptr, &pData));
		memcpy(pData, colmat, 28 * sizeof(float));
	}

	D3D::current_command_list->SetGraphicsRootConstantBufferView(DESCRIPTOR_TABLE_PS_CBVONE, efbcopycbuf12[cbufid]->GetGPUVirtualAddress());
	D3D::command_list_mgr->m_dirty_ps_cbv = true;

	const TargetRectangle targetSource = g_renderer->ConvertEFBRectangle(srcRect);
	// TODO: try targetSource.asRECT();
	const D3D11_RECT sourcerect = CD3D11_RECT(targetSource.left, targetSource.top, targetSource.right, targetSource.bottom);

	// Use linear filtering if (bScaleByHalf), use point filtering otherwise
	if (scaleByHalf)
		D3D::SetLinearCopySampler();
	else
		D3D::SetPointCopySampler();

	// Make sure we don't draw with the texture set as both a source and target.
	// (This can happen because we don't unbind textures when we free them.)

	texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &texture->GetRTV12(), FALSE, nullptr);

	// Create texture copy
	D3D::DrawShadedTexQuad(
		(srcFormat == PEControl::Z24) ? FramebufferManager::GetEFBDepthTexture() : FramebufferManager::GetEFBColorTexture(),
		&sourcerect,
		Renderer::GetTargetWidth(),
		Renderer::GetTargetHeight(),
		(srcFormat == PEControl::Z24) ? PixelShaderCache::GetDepthMatrixProgram12(true) : PixelShaderCache::GetColorMatrixProgram12(true),
		VertexShaderCache::GetSimpleVertexShader12(),
		VertexShaderCache::GetSimpleInputLayout12(),
		GeometryShaderCache::GetCopyGeometryShader12(),
		1.0f, 0, DXGI_FORMAT_R8G8B8A8_UNORM, false, texture->GetMultisampled()
		);

	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());

	g_renderer->RestoreAPIState();
}

void TextureCache::CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
	PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
	bool isIntensity, bool scaleByHalf)
{
	s_encoder->Encode(dst, format, native_width, bytes_per_row, num_blocks_y, memory_stride, srcFormat, srcRect, isIntensity, scaleByHalf);
}

const char palette_shader[] =
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
	palette_buf12index = (palette_buf12index + 1) % 1024;
	memcpy((u8*)palette_buf12data + palette_buf12index * 512, palette, std::min(size, 512u));
}

bool TextureCache::Palettize(TCacheEntryBase* entry, const TCacheEntryBase* unconverted)
{
	if (m_lut_size > 512)
	{
		return false;
	}
	const TCacheEntry* base_entry = static_cast<const TCacheEntry*>(unconverted);
	// stretch picture with increased internal resolution
	const D3D12_VIEWPORT vp12 = { 0.f, 0.f, (float)unconverted->config.width, (float)unconverted->config.height, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D::current_command_list->RSSetViewports(1, &vp12);

	// D3D12: Because the second SRV slot is occupied by this buffer, and an arbitrary texture occupies the first SRV slot,
	// we need to allocate temporary space out of our descriptor heap, place the palette SRV in the second slot, then copy the
	// existing texture's descriptor into the first slot.

	// First, allocate the (temporary) space in the descriptor heap.
	D3D12_CPU_DESCRIPTOR_HANDLE srvGroupCpu[2] = {};
	D3D12_GPU_DESCRIPTOR_HANDLE srvGroupGpu[2] = {};
	D3D::gpu_descriptor_heap_mgr->AllocateGroup(srvGroupCpu, 2, srvGroupGpu, nullptr, true);

	srvGroupCpu[1].ptr = srvGroupCpu[0].ptr + D3D::resource_descriptor_size;

	// Now, create the palette SRV at the appropriate offset.
	D3D12_SHADER_RESOURCE_VIEW_DESC palette_buf12srvDesc = {
		DXGI_FORMAT_R16_UINT,                    // DXGI_FORMAT Format;
		D3D12_SRV_DIMENSION_BUFFER,              // D3D12_SRV_DIMENSION ViewDimension;
		D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING // UINT Shader4ComponentMapping;
	};
	palette_buf12srvDesc.Buffer.FirstElement = palette_buf12index * 256;
	palette_buf12srvDesc.Buffer.NumElements = 256;

	D3D::device12->CreateShaderResourceView(palette_buf12, &palette_buf12srvDesc, srvGroupCpu[1]);

	// Now, copy the existing texture's descriptor into the new temporary location.
	base_entry->texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D::device12->CopyDescriptorsSimple(
		1,
		srvGroupCpu[0],
		base_entry->texture->GetSRV12GPUCPUShadow(),
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);

	// Finally, bind our temporary location.
	D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV, srvGroupGpu[0]);

	// TODO: Add support for C14X2 format.  (Different multiplier, more palette entries.)

	// D3D12: See TextureCache::TextureCache() - because there are only two possible buffer contents here,
	// just pre-populate the data in two parts of the same upload heap.
	if (unconverted->format == 0)
	{
		D3D::current_command_list->SetGraphicsRootConstantBufferView(DESCRIPTOR_TABLE_PS_CBVONE, palette_uniform12->GetGPUVirtualAddress());
	}
	else
	{
		D3D::current_command_list->SetGraphicsRootConstantBufferView(DESCRIPTOR_TABLE_PS_CBVONE, palette_uniform12->GetGPUVirtualAddress() + 256);
	}

	D3D::command_list_mgr->m_dirty_ps_cbv = true;

	const D3D11_RECT sourcerect = CD3D11_RECT(0, 0, unconverted->config.width, unconverted->config.height);

	D3D::SetPointCopySampler();

	// Make sure we don't draw with the texture set as both a source and target.
	// (This can happen because we don't unbind textures when we free them.)

	static_cast<TCacheEntry*>(entry)->texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &static_cast<TCacheEntry*>(entry)->texture->GetRTV12(), FALSE, nullptr);

	// Create texture copy
	D3D::DrawShadedTexQuad(
		base_entry->texture,
		&sourcerect, unconverted->config.width,
		unconverted->config.height,
		palette_pixel_shader12[m_lut_format],
		VertexShaderCache::GetSimpleVertexShader12(),
		VertexShaderCache::GetSimpleInputLayout12(),
		GeometryShaderCache::GetCopyGeometryShader12(),
		1.0f,
		0,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		true,
		static_cast<TCacheEntry*>(entry)->texture->GetMultisampled()
		);

	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE );
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());

	g_renderer->RestoreAPIState();
	return true;
}

D3D12_SHADER_BYTECODE GetConvertShader12(const char* Type)
{
	std::string shader = "#define DECODE DecodePixel_";
	shader.append(Type);
	shader.append("\n");
	shader.append(palette_shader);

	D3DBlob* pBlob = nullptr;
	D3D::CompilePixelShader(shader, &pBlob);

	return { pBlob->Data(), pBlob->Size() };
}

TextureCache::TextureCache()
{
	// FIXME: Is it safe here?
	s_encoder = std::make_unique<PSTextureEncoder>();
	s_encoder->Init();
	s_scaler = std::make_unique<TextureScaler>();	
	pTCacheEntrySaveReadbackBuffer = nullptr;
	pTCacheEntrySaveReadbackBufferData = nullptr;
	pTCacheEntrySaveReadbackBufferSize = 0;

	palette_buf12 = nullptr;
	palette_buf12index = 0;
	palette_buf12data = nullptr;
	ZeroMemory(palette_buf12cpu, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * 1024);
	ZeroMemory(palette_buf12gpu, sizeof(D3D12_GPU_DESCRIPTOR_HANDLE) * 1024);
	palette_pixel_shader12[GX_TL_IA8] = GetConvertShader12("IA8");
	palette_pixel_shader12[GX_TL_RGB565] = GetConvertShader12("RGB565");
	palette_pixel_shader12[GX_TL_RGB5A3] = GetConvertShader12("RGB5A3");

	CheckHR(
		D3D::device12->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(sizeof(u16) * 256 * 1024),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&palette_buf12)
			)
		);

	D3D::SetDebugObjectName12(palette_buf12, "texture decoder lut buffer");

	CheckHR(palette_buf12->Map(0, nullptr, &palette_buf12data));

	// Right now, there are only two variants of palette_uniform data. So, we'll just create an upload heap to permanently store both of these.
	CheckHR(
		D3D::device12->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(((16 + 255) & ~255) * 2), // Constant Buffers have to be 256b aligned. "* 2" to create for two sets of data.
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&palette_uniform12)
		)
		);

	D3D::SetDebugObjectName12(palette_uniform12, "a constant buffer used in TextureCache::ConvertTexture");

	void* palette_uniform12data = nullptr;
	CheckHR(palette_uniform12->Map(0, nullptr, &palette_uniform12data));

	float paramsFormatZero[4] = { 15.f };
	float paramsFormatNonzero[4] = { 255.f };

	memcpy(palette_uniform12data, paramsFormatZero, sizeof(paramsFormatZero));
	memcpy((u8*)palette_uniform12data + 256, paramsFormatNonzero, sizeof(paramsFormatNonzero));
}

TextureCache::~TextureCache()
{
	for (unsigned int k = 0; k < MAX_COPY_BUFFERS; ++k)
	{
		if (efbcopycbuf12[k])
		{
			D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(efbcopycbuf12[k]);
			efbcopycbuf12[k] = nullptr;
		}
	}

	s_encoder->Shutdown();
	s_encoder.reset();
	s_scaler.reset();
	if (pTCacheEntrySaveReadbackBuffer)
	{
		D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(pTCacheEntrySaveReadbackBuffer);
	}

	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(palette_buf12);
	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(palette_uniform12);
}

}
