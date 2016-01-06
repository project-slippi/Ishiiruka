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
	// TODO: Somehow implement this (D3DX11 doesn't support dumping individual LODs)
	static bool warn_once = true;
	if (level && warn_once)
	{
		WARN_LOG(VIDEO, "Dumping individual LOD not supported by D3D11 backend!");
		warn_once = false;
		return false;
	}

	ID3D11Texture2D* pNewTexture = nullptr;
	ID3D11Texture2D* pSurface = texture->GetTex();
	D3D11_TEXTURE2D_DESC desc;
	pSurface->GetDesc(&desc);
	if (desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM)
	{
		// Do not support compressed texture dump right now
		return false;
	}
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	desc.Usage = D3D11_USAGE_STAGING;

	HRESULT hr = D3D::device->CreateTexture2D(&desc, nullptr, &pNewTexture);

	bool saved_png = false;

	if (SUCCEEDED(hr) && pNewTexture)
	{
		D3D::context->CopyResource(pNewTexture, pSurface);

		D3D11_MAPPED_SUBRESOURCE map;
		hr = D3D::context->Map(pNewTexture, 0, D3D11_MAP_READ_WRITE, 0, &map);
		if (SUCCEEDED(hr))
		{
			saved_png = TextureToPng((u8*)map.pData, map.RowPitch, filename, desc.Width, desc.Height);
			D3D::context->Unmap(pNewTexture, 0);
		}
		SAFE_RELEASE(pNewTexture);
	}

	return saved_png;
}

void TextureCache::LoadLut(u32 lutFmt, void* addr, u32 size) {
	s_decoder->LoadLut(lutFmt, addr, size);
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
		const D3D11_BOX *psrcbox = nullptr;
		D3D11_BOX srcbox;
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
		D3D::context->CopySubresourceRegion(
			texture->GetTex(),
			0,
			dstrect.left,
			dstrect.top,
			0,
			srcentry->texture->GetTex(),
			0,
			psrcbox);
		return;
	}
	else if (!config.rendertarget)
	{
		config.rendertarget = true;
		texture->Release();
		int flags = ((int)D3D11_BIND_RENDER_TARGET | (int)D3D11_BIND_SHADER_RESOURCE);
		if (D3D::GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0)
		{
			flags |= D3D11_BIND_UNORDERED_ACCESS;
		}
		texture = D3DTexture2D::Create(config.width, config.height,
			(D3D11_BIND_FLAG)flags,
			D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM, 1, config.layers);
	}
	g_renderer->ResetAPIState(); // reset any game specific settings

	const D3D11_VIEWPORT vp = CD3D11_VIEWPORT(
		float(dstrect.left),
		float(dstrect.top),
		float(dstrect.GetWidth()),
		float(dstrect.GetHeight()));

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
		VertexShaderCache::GetSimpleInputLayout(), nullptr, 1.0, 0);

	D3D::context->OMSetRenderTargets(1,
		&FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());

	g_renderer->RestoreAPIState();
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
	if (config.rendertarget)
	{
		int flags = ((int)D3D11_BIND_RENDER_TARGET | (int)D3D11_BIND_SHADER_RESOURCE);
		if (D3D::GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0)
		{
			flags |= D3D11_BIND_UNORDERED_ACCESS;
		}
		return new TCacheEntry(config, D3DTexture2D::Create(config.width, config.height,
			(D3D11_BIND_FLAG)flags,
			D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM, 1, config.layers));
	}
	bool swaprg = false;
	bool convertrgb565 = false;
	static const DXGI_FORMAT PC_TexFormat_To_DXGIFORMAT[11]
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
	};
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

	g_renderer->ResetAPIState();
	// stretch picture with increased internal resolution
	const D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)config.width, (float)config.height);
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

	const TargetRectangle targetSource = g_renderer->ConvertEFBRectangle(srcRect);
	// TODO: try targetSource.asRECT();
	const D3D11_RECT sourcerect = CD3D11_RECT(targetSource.left, targetSource.top, targetSource.right, targetSource.bottom);

	// Use linear filtering if (bScaleByHalf), use point filtering otherwise
	if (scaleByHalf)
		D3D::SetLinearCopySampler();
	else
		D3D::SetPointCopySampler();

	// if texture is currently in use, it needs to be temporarily unset
	u64 textureSlotMask = D3D::stateman->UnsetTexture(texture->GetSRV());
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
	D3D::stateman->SetTextureByMask(textureSlotMask, texture->GetSRV());
	if (!g_ActiveConfig.bSkipEFBCopyToRam)
	{
		
	}
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
	if (texformat == GX_TF_C4)
		baseType = Unorm4;
	else if (texformat == GX_TF_C8)
		baseType = Unorm8;
	else
		return false;
	// if texture is currently in use, it needs to be temporarily unset
	u64 textureSlotMask = D3D::stateman->UnsetTexture(texture->GetSRV());
	bool result = s_decoder->Depalettize(*texture, *((TextureCache::TCacheEntry*)base_entry)->texture, baseType, base_entry->config.width, base_entry->config.height);
	D3D::stateman->SetTextureByMask(textureSlotMask, texture->GetSRV());
	return result;
}

TextureCache::TextureCache()
{
	if (D3D::GetFeatureLevel() < D3D_FEATURE_LEVEL_11_0)
	{
		s_encoder = std::make_unique<PSTextureEncoder>();
	}
	else
	{
		s_encoder = std::make_unique<CSTextureEncoder>();
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
