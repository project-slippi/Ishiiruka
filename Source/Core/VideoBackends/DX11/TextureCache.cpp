// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/Memmap.h"
#include "VideoBackends/DX11/D3DBase.h"
#include "VideoBackends/DX11/D3DState.h"
#include "VideoBackends/DX11/D3DUtil.h"
#include "VideoBackends/DX11/FramebufferManager.h"
#include "VideoBackends/DX11/PixelShaderCache.h"
#include "VideoBackends/DX11/CSTextureDecoder.h"
#include "VideoBackends/DX11/CSTextureEncoder.h"
#include "VideoBackends/DX11/PSTextureEncoder.h"
#include "VideoBackends/DX11/TextureCache.h"
#include "VideoBackends/DX11/TextureEncoder.h"
#include "VideoBackends/DX11/VertexShaderCache.h"
#include "VideoBackends/DX11/GeometryShaderCache.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/TextureScalerCommon.h"

namespace DX11
{

static std::unique_ptr<TextureEncoder> s_encoder;
static std::unique_ptr<TextureDecoder> s_decoder;
static std::unique_ptr<TextureScaler> s_scaler;
const size_t MAX_COPY_BUFFERS = 33;
D3D::BufferPtr efbcopycbuf[MAX_COPY_BUFFERS];

TextureCache::TCacheEntry::~TCacheEntry()
{
	texture->Release();
	if (nrm_texture)
	{
		nrm_texture->Release();
	}
}

void TextureCache::TCacheEntry::Bind(u32 stage, u32 last_texture)
{
	D3D::stateman->SetTexture(stage, texture->GetSRV());
	if (nrm_texture && g_ActiveConfig.HiresMaterialMapsEnabled())
	{
		D3D::stateman->SetTexture(stage + 8, nrm_texture->GetSRV());
	}
}

bool TextureCache::TCacheEntry::Save(const std::string& filename, u32 level)
{
	// Create a staging/readback texture with the dimensions of the specified mip level.
	u32 mip_width = std::max(config.width >> level, 1u);
	u32 mip_height = std::max(config.height >> level, 1u);
	CD3D11_TEXTURE2D_DESC staging_texture_desc(
		this->DXGI_format,
		mip_width, mip_height, 1, 1, 0,
		D3D11_USAGE_STAGING,
		D3D11_CPU_ACCESS_READ);

	ID3D11Texture2D* staging_texture;
	HRESULT hr = D3D::device->CreateTexture2D(&staging_texture_desc, nullptr, &staging_texture);
	if (FAILED(hr))
	{
		WARN_LOG(VIDEO, "Failed to create texture dumping readback texture: %X", static_cast<u32>(hr));
		return false;
	}

	// Copy the selected mip level to the staging texture.
	CD3D11_BOX src_box(0, 0, 0, mip_width, mip_height, 1);
	D3D::context->CopySubresourceRegion(staging_texture, 0, 0, 0, 0,
		texture->GetTex(), D3D11CalcSubresource(level, 0, config.levels), &src_box);

	// Map the staging texture to client memory, and encode it as a .png image.
	D3D11_MAPPED_SUBRESOURCE map;
	hr = D3D::context->Map(staging_texture, 0, D3D11_MAP_READ, 0, &map);
	if (FAILED(hr))
	{
		WARN_LOG(VIDEO, "Failed to map texture dumping readback texture: %X", static_cast<u32>(hr));
		staging_texture->Release();
		return false;
	}

	bool encode_result = false;
	if (this->compressed)
	{
		encode_result = TextureToDDS(reinterpret_cast<u8*>(map.pData), map.RowPitch, filename, mip_width, mip_height);
	}
	else
	{
		encode_result = TextureToPng(reinterpret_cast<u8*>(map.pData), map.RowPitch, filename, mip_width, mip_height);
	}
	D3D::context->Unmap(staging_texture, 0);
	staging_texture->Release();

	return encode_result;
}

void TextureCache::LoadLut(u32 lutFmt, void* addr, u32 size)
{
	s_decoder->LoadLut(lutFmt, addr, size);
}

void TextureCache::TCacheEntry::CopyRectangleFromTexture(
	const TCacheEntryBase* source,
	const MathUtil::Rectangle<int> &srcrect,
	const MathUtil::Rectangle<int> &dstrect)
{
	TCacheEntry* srcentry = (TCacheEntry*)source;
	if (srcrect.GetWidth() == dstrect.GetWidth()
		&& srcrect.GetHeight() == dstrect.GetHeight()
		&& static_cast<UINT>(dstrect.GetWidth()) <= config.width
		&& static_cast<UINT>(dstrect.GetHeight()) <= config.height
		&& static_cast<UINT>(dstrect.GetWidth()) <= srcentry->config.width
		&& static_cast<UINT>(dstrect.GetHeight()) <= srcentry->config.height)
	{
		CD3D11_BOX src_box(srcrect.left, srcrect.top, 0, srcrect.right, srcrect.bottom, srcentry->config.layers);
		D3D::context->CopySubresourceRegion(
			texture->GetTex(),
			0,
			dstrect.left,
			dstrect.top,
			0,
			srcentry->texture->GetTex(),
			0,
			&src_box);
		return;
	}
	else if (!config.rendertarget)
	{
		config.rendertarget = true;
		int flags = ((int)D3D11_BIND_RENDER_TARGET | (int)D3D11_BIND_SHADER_RESOURCE);
		if (D3D::GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0)
		{
			flags |= D3D11_BIND_UNORDERED_ACCESS;
		}
		D3DTexture2D* ptexture = D3DTexture2D::Create(config.width, config.height,
			(D3D11_BIND_FLAG)flags,
			D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM, 1, config.layers);
		D3D::context->CopyResource(
			ptexture->GetTex(),
			texture->GetTex());

		texture->Release();
		texture = ptexture;
	}
	g_renderer->ResetAPIState(); // reset any game specific settings

	const D3D11_VIEWPORT vp = CD3D11_VIEWPORT(
		float(dstrect.left),
		float(dstrect.top),
		float(dstrect.GetWidth()),
		float(dstrect.GetHeight()));
	u64 texture_mask = D3D::stateman->UnsetTexture(texture->GetSRV());
	D3D::stateman->Apply();
	D3D::context->OMSetRenderTargets(1, &texture->GetRTV(), nullptr);
	D3D::context->RSSetViewports(1, &vp);
	D3D::SetLinearCopySampler();
	D3D11_RECT srcRC;
	srcRC.left = srcrect.left;
	srcRC.right = srcrect.right;
	srcRC.top = srcrect.top;
	srcRC.bottom = srcrect.bottom;
	D3D::drawShadedTexQuad(srcentry->texture->GetSRV(), &srcRC,
		srcentry->config.width, srcentry->config.height,
		PixelShaderCache::GetColorCopyProgram(false),
		VertexShaderCache::GetSimpleVertexShader(),
		VertexShaderCache::GetSimpleInputLayout(), GeometryShaderCache::GetCopyGeometryShader(), 1.0, 0);

	D3D::context->OMSetRenderTargets(1,
		&FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());

	g_renderer->RestoreAPIState();
	D3D::stateman->SetTextureByMask(texture_mask, texture->GetSRV());
}

void TextureCache::TCacheEntry::Load(const u8* src, u32 width, u32 height,
	u32 expanded_width, u32 level)
{
	D3D::ReplaceTexture2D(
		texture->GetTex(),
		src,
		width,
		height,
		expanded_width,
		level,
		usage,
		DXGI_format,
		swap_rg,
		convertrgb565);
}
void TextureCache::TCacheEntry::LoadMaterialMap(const u8* src, u32 width, u32 height, u32 level)
{
	D3D::ReplaceTexture2D(
		nrm_texture->GetTex(),
		src,
		width,
		height,
		width,
		level,
		usage,
		DXGI_format,
		swap_rg,
		convertrgb565);
}
void TextureCache::TCacheEntry::Load(const u8* src, u32 width, u32 height, u32 expandedWidth,
	u32 expandedHeight, const s32 texformat, const u32 tlutaddr, const TlutFormat tlutfmt, u32 level)
{
	bool need_cpu_decode = is_scaled;
	if (!need_cpu_decode)
	{
		need_cpu_decode = !s_decoder->Decode(
			src,
			TexDecoder_GetTextureSizeInBytes(expandedWidth, expandedHeight, texformat),
			texformat,
			width,
			height,
			level,
			*texture);
	}
	if (need_cpu_decode)
	{
		TexDecoder_Decode(
			TextureCache::temp,
			src,
			expandedWidth,
			expandedHeight,
			texformat,
			tlutaddr,
			tlutfmt,
			PC_TEX_FMT_RGBA32 == config.pcformat,
			compressed);
		u8* data = TextureCache::temp;
		if (is_scaled)
		{
			data = (u8*)s_scaler->Scale((u32*)data, expandedWidth, height);
			width *= g_ActiveConfig.iTexScalingFactor;
			height *= g_ActiveConfig.iTexScalingFactor;
			expandedWidth *= g_ActiveConfig.iTexScalingFactor;
		}
		D3D::ReplaceTexture2D(
			texture->GetTex(),
			data,
			width,
			height,
			expandedWidth,
			level,
			usage,
			DXGI_format,
			swap_rg,
			convertrgb565);
	}
}
void TextureCache::TCacheEntry::LoadFromTmem(const u8* ar_src, const u8* gb_src, u32 width, u32 height,
	u32 expanded_width, u32 expanded_Height, u32 level)
{
	bool need_cpu_decode = is_scaled;
	if (!need_cpu_decode)
	{
		need_cpu_decode = !s_decoder->DecodeRGBAFromTMEM(ar_src, gb_src, width, height, *texture);
	}
	if (need_cpu_decode)
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
		D3D::ReplaceTexture2D(
			texture->GetTex(),
			data,
			width,
			height,
			expanded_width,
			level,
			usage,
			DXGI_format,
			swap_rg,
			convertrgb565);
	}
}

PC_TexFormat TextureCache::GetNativeTextureFormat(const s32 texformat, const TlutFormat tlutfmt, u32 width, u32 height)
{
	if (s_decoder->FormatSupported(texformat))
	{
		return PC_TEX_FMT_RGBA32;
	}
	const bool compressed_supported = ((width & 3) == 0) && ((height & 3) == 0);
	PC_TexFormat pcfmt = GetPC_TexFormat(texformat, tlutfmt, compressed_supported);
	pcfmt = !g_ActiveConfig.backend_info.bSupportedFormats[pcfmt] ? PC_TEX_FMT_RGBA32 : pcfmt;
	return pcfmt;
}

TextureCache::TCacheEntryBase* TextureCache::CreateTexture(const TCacheEntryConfig& config)
{
	static const DXGI_FORMAT PC_TexFormat_To_DXGIFORMAT[12]
	{
		DXGI_FORMAT_UNKNOWN,//PC_TEX_FMT_NONE
		DXGI_FORMAT_B8G8R8A8_UNORM,//PC_TEX_FMT_BGRA32
		DXGI_FORMAT_R8G8B8A8_UNORM,//PC_TEX_FMT_RGBA32
		DXGI_FORMAT_R8_UNORM,//PC_TEX_FMT_I4_AS_I8
		DXGI_FORMAT_R8G8_UNORM,//PC_TEX_FMT_IA4_AS_IA8
		DXGI_FORMAT_R8_UNORM,//PC_TEX_FMT_I8
		DXGI_FORMAT_R8G8_UNORM,//PC_TEX_FMT_IA8
		DXGI_FORMAT_B5G6R5_UNORM,//PC_TEX_FMT_RGB565
		DXGI_FORMAT_BC1_UNORM,//PC_TEX_FMT_DXT1
		DXGI_FORMAT_BC2_UNORM,//PC_TEX_FMT_DXT3
		DXGI_FORMAT_BC3_UNORM,//PC_TEX_FMT_DXT5
		DXGI_FORMAT_R32_FLOAT,//PC_TEX_FMT_R32
	};
	if (config.rendertarget)
	{
		int flags = ((int)D3D11_BIND_RENDER_TARGET | (int)D3D11_BIND_SHADER_RESOURCE);
		if (D3D::GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0)
		{
			flags |= D3D11_BIND_UNORDERED_ACCESS;
		}
		return new TCacheEntry(config, D3DTexture2D::Create(config.width, config.height,
			(D3D11_BIND_FLAG)flags,
			D3D11_USAGE_DEFAULT, PC_TexFormat_To_DXGIFORMAT[config.pcformat], 1, config.layers));
	}
	bool swaprg = false;
	bool convertrgb565 = false;

	DXGI_FORMAT format = PC_TexFormat_To_DXGIFORMAT[config.pcformat];
	bool bgrasupported = D3D::BGRATexturesSupported();
	if (format == DXGI_FORMAT_B8G8R8A8_UNORM && !bgrasupported)
	{
		swaprg = true;
		format = DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	if (format == DXGI_FORMAT_B5G6R5_UNORM && !D3D::BGRA565TexturesSupported())
	{
		convertrgb565 = true;
		format = bgrasupported ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	bool compressed = format == DXGI_FORMAT_BC1_UNORM
		|| format == DXGI_FORMAT_BC2_UNORM
		|| format == DXGI_FORMAT_BC3_UNORM;
	D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
	D3D11_CPU_ACCESS_FLAG cpu_access = (D3D11_CPU_ACCESS_FLAG)0;

	if ((config.levels == 1 || format == DXGI_FORMAT_B5G6R5_UNORM) && !compressed)
	{
		usage = D3D11_USAGE_DYNAMIC;
		cpu_access = D3D11_CPU_ACCESS_WRITE;
	}
	const D3D11_TEXTURE2D_DESC texdesc = CD3D11_TEXTURE2D_DESC(format,
		config.width, config.height, 1, config.levels, D3D11_BIND_SHADER_RESOURCE, usage, cpu_access);

	ID3D11Texture2D *pTexture;
	const HRESULT hr = D3D::device->CreateTexture2D(&texdesc, NULL, &pTexture);
	CHECK(SUCCEEDED(hr), "Create texture of the TextureCache");

	TCacheEntry* const entry = new TCacheEntry(config, new D3DTexture2D(pTexture, D3D11_BIND_SHADER_RESOURCE));
	entry->usage = usage;
	entry->DXGI_format = format;
	entry->swap_rg = swaprg;
	entry->convertrgb565 = convertrgb565;
	entry->compressed = compressed;
	// TODO: better debug names
	D3D::SetDebugObjectName(entry->texture->GetTex(), "a texture of the TextureCache");
	D3D::SetDebugObjectName(entry->texture->GetSRV(), "shader resource view of a texture of the TextureCache");
	SAFE_RELEASE(pTexture);
	if (config.materialmap)
	{
		const D3D11_TEXTURE2D_DESC texdesc = CD3D11_TEXTURE2D_DESC(format,
			config.width, config.height, config.layers, config.levels, D3D11_BIND_SHADER_RESOURCE, usage, cpu_access);

		const HRESULT hr = D3D::device->CreateTexture2D(&texdesc, NULL, &pTexture);
		CHECK(SUCCEEDED(hr), "Create material texture of the TextureCache");
		entry->nrm_texture = new D3DTexture2D(pTexture, D3D11_BIND_SHADER_RESOURCE);
		// TODO: better debug names
		D3D::SetDebugObjectName(entry->nrm_texture->GetTex(), "a material texture of the TextureCache");
		D3D::SetDebugObjectName(entry->nrm_texture->GetSRV(), "shader resource view of a material texture of the TextureCache");
		SAFE_RELEASE(pTexture);
	}
	return entry;
}

void TextureCache::TCacheEntry::FromRenderTarget(u8* dst, PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
	bool scaleByHalf, unsigned int cbufid, const float *colmat)
{
	// When copying at half size, in multisampled mode, resolve the color/depth buffer first.
	// This is because multisampled texture reads go through Load, not Sample, and the linear
	// filter is ignored.
	bool multisampled = (g_ActiveConfig.iMultisamples > 1);
	ID3D11ShaderResourceView* efb_texture_srv;

	if (multisampled && scaleByHalf)
	{
		multisampled = false;
		efb_texture_srv = (srcFormat == PEControl::Z24) ?
			FramebufferManager::GetResolvedEFBDepthTexture()->GetSRV() :
			FramebufferManager::GetResolvedEFBColorTexture()->GetSRV();
	}
	else
	{
		efb_texture_srv = (srcFormat == PEControl::Z24) ?
			FramebufferManager::GetEFBDepthTexture()->GetSRV() :
			FramebufferManager::GetEFBColorTexture()->GetSRV();
	}
	const TargetRectangle targetSource = g_renderer->ConvertEFBRectangle(srcRect);
	g_renderer->ResetAPIState();
	// stretch picture with increased internal resolution
	const D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)targetSource.GetWidth() * (scaleByHalf ? 0.5f : 1.0f), (float)targetSource.GetHeight() * (scaleByHalf ? 0.5f : 1.0f));
	D3D::context->RSSetViewports(1, &vp);

	// set transformation
	if (nullptr == efbcopycbuf[cbufid].get())
	{
		const D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(28 * sizeof(float), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DEFAULT);
		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = colmat;
		HRESULT hr = D3D::device->CreateBuffer(&cbdesc, &data, D3D::ToAddr(efbcopycbuf[cbufid]));
		CHECK(SUCCEEDED(hr), "Create efb copy constant buffer %d", cbufid);
		D3D::SetDebugObjectName(efbcopycbuf[cbufid].get(), "a constant buffer used in TextureCache::CopyRenderTargetToTexture");
	}
	D3D::stateman->SetPixelConstants(efbcopycbuf[cbufid].get());

	
	// TODO: try targetSource.asRECT();
	const D3D11_RECT sourcerect = CD3D11_RECT(targetSource.left, targetSource.top, targetSource.right, targetSource.bottom);

	// Use linear filtering if (bScaleByHalf), use point filtering otherwise
	if (scaleByHalf)
		D3D::SetLinearCopySampler();
	else
		D3D::SetPointCopySampler();

	// if texture is currently in use, it needs to be temporarily unset
	u64 texture_mask = D3D::stateman->UnsetTexture(texture->GetSRV());
	D3D::stateman->Apply();
	D3D::context->OMSetRenderTargets(1, &texture->GetRTV(), nullptr);
	// Create texture copy
	D3D::drawShadedTexQuad(
		efb_texture_srv,
		&sourcerect, Renderer::GetTargetWidth(), Renderer::GetTargetHeight(),
		(srcFormat == PEControl::Z24) ? PixelShaderCache::GetDepthMatrixProgram(multisampled) : PixelShaderCache::GetColorMatrixProgram(multisampled),
		VertexShaderCache::GetSimpleVertexShader(),
		VertexShaderCache::GetSimpleInputLayout(),
		(g_Config.iStereoMode > 0) ? GeometryShaderCache::GetCopyGeometryShader() : nullptr);

	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());

	g_renderer->RestoreAPIState();
	D3D::stateman->SetTextureByMask(texture_mask, texture->GetSRV());
}

void TextureCache::CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
	PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
	bool isIntensity, bool scaleByHalf)
{
	s_encoder->Encode(
		dst,
		format,
		native_width,
		bytes_per_row,
		num_blocks_y,
		memory_stride,
		srcFormat,
		isIntensity,
		scaleByHalf,
		srcRect);
}

bool TextureCache::Palettize(TCacheEntryBase* entry, const TCacheEntryBase* base_entry)
{
	DX11::D3DTexture2D* texture = ((TextureCache::TCacheEntry*)entry)->texture;
	u32 texformat = entry->format & 0xf;
	BaseType baseType = Unorm4;
	if (texformat == GX_TF_C4 || texformat == GX_TF_I4)
		baseType = Unorm4;
	else if (texformat == GX_TF_C8 || texformat == GX_TF_I8)
		baseType = Unorm8;
	else
		return false;
	// if texture is currently in use, it needs to be temporarily unset
	u64 texture_mask = D3D::stateman->UnsetTexture(texture->GetSRV());
	D3D::stateman->Apply();
	bool result = s_decoder->Depalettize(*texture, *((TextureCache::TCacheEntry*)base_entry)->texture, baseType, base_entry->config.width, base_entry->config.height);
	D3D::stateman->SetTextureByMask(texture_mask, texture->GetSRV());
	return result;
}

TextureCache::TextureCache()
{
	if (g_ActiveConfig.backend_info.bSupportsComputeTextureEncoding
		&& g_ActiveConfig.bEnableComputeTextureEncoding)
	{
		s_encoder = std::make_unique<CSTextureEncoder>();
	}
	else
	{
		s_encoder = std::make_unique<PSTextureEncoder>();
	}
	s_encoder->Init();
	s_decoder = std::make_unique<CSTextureDecoder>();
	s_decoder->Init();
	s_scaler = std::make_unique<TextureScaler>();
}

TextureCache::~TextureCache()
{
	for (u32 k = 0; k < MAX_COPY_BUFFERS; ++k)
		efbcopycbuf[k].reset();

	s_encoder->Shutdown();
	s_encoder.reset();

	s_decoder->Shutdown();
	s_decoder.reset();
	s_scaler.reset();
}

}
