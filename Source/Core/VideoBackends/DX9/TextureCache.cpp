// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <d3dx9.h>
#include <memory>

#include "Common/CommonPaths.h"
#include "Common/MemoryUtil.h"
#include "Common/Hash.h"
#include "Common/FileUtil.h"

#include "Core/HW/Memmap.h"

#include "VideoBackends/DX9/D3DBase.h"
#include "VideoBackends/DX9/D3DTexture.h"
#include "VideoBackends/DX9/D3DUtil.h"
#include "VideoBackends/DX9/FramebufferManager.h"
#include "VideoBackends/DX9/PixelShaderCache.h"
#include "VideoBackends/DX9/Render.h"
#include "VideoBackends/DX9/TextureCache.h"
#include "VideoBackends/DX9/TextureConverter.h"
#include "VideoBackends/DX9/VertexShaderCache.h"
#include "VideoBackends/DX9/Depalettizer.h"

#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/HiresTextures.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/TextureScalerCommon.h"
#include "VideoCommon/VertexShaderManager.h"


extern s32 frameCount;

namespace DX9
{

static const D3DFORMAT PC_TexFormat_To_D3DFORMAT[]
{
	D3DFMT_UNKNOWN,//PC_TEX_FMT_NONE
	D3DFMT_A8R8G8B8,//PC_TEX_FMT_BGRA32
	D3DFMT_A8R8G8B8,//PC_TEX_FMT_RGBA32
	D3DFMT_A8P8,//PC_TEX_FMT_I4_AS_I8 A hack which means the format is a packed 8-bit intensity texture. It is unpacked to A8L8 in D3DTexture.cpp
	D3DFMT_A8L8,//PC_TEX_FMT_IA4_AS_IA8
	D3DFMT_A8P8,//PC_TEX_FMT_I8
	D3DFMT_A8L8,//PC_TEX_FMT_IA8
	D3DFMT_R5G6B5,//PC_TEX_FMT_RGB565
	D3DFMT_DXT1,//PC_TEX_FMT_DXT1
	D3DFMT_DXT3,//PC_TEX_FMT_DXT3
	D3DFMT_DXT5,//PC_TEX_FMT_DXT5
	D3DFMT_D32F_LOCKABLE,//PC_TEX_FMT_DEPTH_FLOAT
	D3DFMT_R32F,//PC_TEX_FMT_R_FLOAT
	D3DFMT_A16B16G16R16F, //PC_TEX_FMT_RGBA16_FLOAT
	D3DFMT_A32B32G32R32F,//PC_TEX_FMT_RGBA_FLOAT
};

static const u32 PC_TexFormat_To_Buffer_Index[]
{
	0,//PC_TEX_FMT_NONE
	0,//PC_TEX_FMT_BGRA32
	0,//PC_TEX_FMT_RGBA32
	1,//PC_TEX_FMT_I4_AS_I8 A hack which means the format is a packed 8-bit intensity texture. It is unpacked to A8L8 in D3DTexture.cpp
	1,//PC_TEX_FMT_IA4_AS_IA8
	1,//PC_TEX_FMT_I8
	1,//PC_TEX_FMT_IA8
	2,//PC_TEX_FMT_RGB565
	3,//PC_TEX_FMT_DXT1
	4,//PC_TEX_FMT_DXT3
	5,//PC_TEX_FMT_DXT5
	6,//PC_TEX_FMT_DEPTH_FLOAT
	7,//PC_TEX_FMT_R_FLOAT
	8,//PC_TEX_FMT_RGBA16_FLOAT
	9,//PC_TEX_FMT_RGBA_FLOAT
};

#define MEM_TEXTURE_POOL_SIZE 10
static LPDIRECT3DTEXTURE9 s_memPoolTexture[MEM_TEXTURE_POOL_SIZE];
static u32 s_memPoolTextureW[MEM_TEXTURE_POOL_SIZE];
static u32 s_memPoolTextureH[MEM_TEXTURE_POOL_SIZE];

static std::unique_ptr<Depalettizer> s_depaletizer;

TextureCache::TCacheEntry::~TCacheEntry()
{
	texture->Release();
}

void TextureCache::TCacheEntry::Bind(u32 stage)
{
	D3D::SetTexture(stage, texture);
}

bool TextureCache::TCacheEntry::Save(const std::string& filename, u32 level)
{
	IDirect3DSurface9* surface;
	HRESULT hr = texture->GetSurfaceLevel(level, &surface);
	if (FAILED(hr))
		return false;

	hr = PD3DXSaveSurfaceToFileA(filename.c_str(), this->compressed ? D3DXIFF_DDS : D3DXIFF_PNG, surface, NULL, NULL);
	surface->Release();

	return SUCCEEDED(hr);
}

void TextureCache::TCacheEntry::CopyRectangleFromTexture(
	const TextureCache::TCacheEntryBase* source,
	const MathUtil::Rectangle<int> &srcrect,
	const MathUtil::Rectangle<int> &dstrect)
{
	TCacheEntry* entry = (TCacheEntry*)source;
	if (entry->d3d_fmt != d3d_fmt || d3d_fmt != D3DFMT_A8R8G8B8)
	{
		return;
	}
	LPDIRECT3DSURFACE9 srcsurf = nullptr;
	LPDIRECT3DSURFACE9 dstsurf = nullptr;
	if (!config.rendertarget)
	{
		config.rendertarget = true;
		LPDIRECT3DTEXTURE9 text;
		D3D::dev->CreateTexture(config.width, config.height, 1, D3DUSAGE_RENDERTARGET,
			D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &text, 0);
		texture->GetSurfaceLevel(0, &srcsurf);
		text->GetSurfaceLevel(0, &dstsurf);
		HRESULT hr = D3D::dev->StretchRect(srcsurf, nullptr, dstsurf, nullptr, D3DTEXF_LINEAR);
		_assert_msg_(VIDEO, SUCCEEDED(hr), "Failed updating texture");
		srcsurf->Release();
		texture->Release();
		texture = text;
		srcsurf = nullptr;
	}
	else
	{
		texture->GetSurfaceLevel(0, &dstsurf);
	}
	entry->texture->GetSurfaceLevel(0, &srcsurf);
	if (srcsurf != nullptr && dstsurf != nullptr)
	{
		HRESULT hr = D3D::dev->StretchRect(srcsurf, (RECT*)&srcrect, dstsurf, (RECT*)&dstrect, D3DTEXF_POINT);
		_assert_msg_(VIDEO, SUCCEEDED(hr), "Failed updating texture");
	}
	if (srcsurf != nullptr)
	{
		srcsurf->Release();
	}
	if (dstsurf != nullptr)
	{
		dstsurf->Release();
	}
}

void TextureCache::TCacheEntry::ReplaceTexture(const u8* src, u32 width, u32 height,
	u32 expanded_width, u32 level, bool swap_r_b)
{
	u32 pcformatidx = PC_TexFormat_To_Buffer_Index[config.pcformat];
	if (s_memPoolTexture[pcformatidx] == nullptr || width > s_memPoolTextureW[pcformatidx] || height > s_memPoolTextureH[pcformatidx])
	{
		if (s_memPoolTexture[pcformatidx] != nullptr)
		{
			s_memPoolTexture[pcformatidx]->Release();
			s_memPoolTexture[pcformatidx] = nullptr;
		}
		u32 max = std::max(width, height);
		u32 nextsize = s_memPoolTextureW[pcformatidx];
		while (nextsize < max)
		{
			nextsize *= 2;
		}
		s_memPoolTextureW[pcformatidx] = nextsize;
		s_memPoolTextureH[pcformatidx] = nextsize;
		s_memPoolTexture[pcformatidx] = D3D::CreateTexture2D(s_memPoolTextureW[pcformatidx], s_memPoolTextureH[pcformatidx], d3d_fmt, 1, D3DPOOL_SYSTEMMEM);
	}
	D3D::ReplaceTexture2D(s_memPoolTexture[pcformatidx], src, width, height, expanded_width, d3d_fmt, swap_r_b);
	PDIRECT3DSURFACE9 srcsurf;
	s_memPoolTexture[pcformatidx]->GetSurfaceLevel(0, &srcsurf);
	PDIRECT3DSURFACE9 dstsurface;
	texture->GetSurfaceLevel(level, &dstsurface);
	RECT srcr{ 0, 0, LONG(width), LONG(height) };
	POINT dstp{ 0, 0 };
	D3D::dev->UpdateSurface(srcsurf, &srcr, dstsurface, &dstp);
	srcsurf->Release();
	dstsurface->Release();
}

void TextureCache::TCacheEntry::Load(const u8* src, u32 width, u32 height,
	u32 expanded_width, u32 level)
{
	bool swap_r_b = PC_TEX_FMT_RGBA32 == config.pcformat;
	ReplaceTexture(src, width, height, expanded_width, level, swap_r_b);
}

void TextureCache::TCacheEntry::FromRenderTarget(bool is_depth_copy, const EFBRectangle& srcRect,
	bool scaleByHalf, unsigned int cbufid, const float *colmat, u32 width, u32 height)
{
	g_renderer->ResetAPIState(); // reset any game specific settings

	const LPDIRECT3DTEXTURE9 read_texture = is_depth_copy?
		FramebufferManager::GetEFBDepthTexture() :
		FramebufferManager::GetEFBColorTexture();

	LPDIRECT3DSURFACE9 Rendersurf = NULL;
	texture->GetSurfaceLevel(0, &Rendersurf);
	D3D::dev->SetDepthStencilSurface(NULL);
	D3D::dev->SetRenderTarget(0, Rendersurf);

	D3DVIEWPORT9 vp;
	TargetRectangle targetSource = g_renderer->ConvertEFBRectangle(srcRect);
	// Stretch picture with increased internal resolution
	vp.X = 0;
	vp.Y = 0;
	vp.Width = width;
	vp.Height = height;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);
	RECT destrect;
	destrect.bottom = vp.Height;
	destrect.left = 0;
	destrect.right = vp.Width;
	destrect.top = 0;

	PixelShaderManager::SetColorMatrix(colmat); // set transformation
	D3D::dev->SetPixelShaderConstantF(C_COLORMATRIX, PixelShaderManager::GetBuffer(), 7);
	
	RECT sourcerect;
	sourcerect.bottom = targetSource.bottom;
	sourcerect.left = targetSource.left;
	sourcerect.right = targetSource.right;
	sourcerect.top = targetSource.top;

	if (is_depth_copy)
	{
		if (scaleByHalf || g_ActiveConfig.iMultisamples > 1)
		{
			D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
			D3D::ChangeSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		}
		else
		{
			D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
			D3D::ChangeSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		}
	}
	else
	{
		D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		D3D::ChangeSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	}

	D3DFORMAT bformat = FramebufferManager::GetEFBDepthRTSurfaceFormat();
	s32 SSAAMode = g_ActiveConfig.iMultisamples - 1;

	D3D::drawShadedTexQuad(read_texture, &sourcerect,
		g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight(),
		config.width, config.height,
		PixelShaderCache::GetDepthMatrixProgram(SSAAMode, is_depth_copy && bformat != FOURCC_RAWZ),
		VertexShaderCache::GetSimpleVertexShader(SSAAMode));

	Rendersurf->Release();

	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);
	D3D::RefreshSamplerState(0, D3DSAMP_MAGFILTER);
	D3D::SetTexture(0, NULL);
	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());

	g_renderer->RestoreAPIState();
}

void TextureCache::CopyEFB(u8* dst, const EFBCopyFormat& format, u32 native_width, u32 bytes_per_row,
	u32 num_blocks_y, u32 memory_stride, bool is_depth_copy,
	const EFBRectangle& src_rect, bool scale_by_half)
{
	TextureConverter::EncodeToRamFromTexture(dst, format, native_width, bytes_per_row, num_blocks_y,
		memory_stride, is_depth_copy, src_rect, scale_by_half);
}

bool TextureCache::Palettize(TCacheEntryBase* entry, const TCacheEntryBase* base_entry)
{
	LPDIRECT3DTEXTURE9 texture = ((TCacheEntry*)entry)->texture;
	u32 texformat = entry->format & 0xf;
	Depalettizer::BaseType baseType = Depalettizer::Unorm8;
	if (texformat == GX_TF_C4 || texformat == GX_TF_I4)
		baseType = Depalettizer::Unorm4;
	else if (texformat == GX_TF_C8 || texformat == GX_TF_I8)
		baseType = Depalettizer::Unorm8;
	else
		return false;
	return s_depaletizer->Depalettize(texture, ((TextureCache::TCacheEntry*)base_entry)->texture, baseType);
}

PC_TexFormat TextureCache::GetNativeTextureFormat(const s32 texformat, const TlutFormat tlutfmt, u32 width, u32 height)
{
	const bool compressed_supported = ((width & 3) == 0) && ((height & 3) == 0);
	PC_TexFormat pcfmt = GetPC_TexFormat(texformat, tlutfmt, compressed_supported);
	pcfmt = !g_ActiveConfig.backend_info.bSupportedFormats[pcfmt] ? PC_TEX_FMT_RGBA32 : pcfmt;
	return pcfmt;
}

TextureCache::TCacheEntryBase* TextureCache::CreateTexture(const TCacheEntryConfig& config)
{
	if (config.rendertarget)
	{
		LPDIRECT3DTEXTURE9 texture;
		D3D::dev->CreateTexture(config.width, config.height, 1, D3DUSAGE_RENDERTARGET,
			PC_TexFormat_To_D3DFORMAT[config.pcformat], D3DPOOL_DEFAULT, &texture, 0);
		TCacheEntry* entry = new TCacheEntry(config, texture);
		entry->d3d_fmt = PC_TexFormat_To_D3DFORMAT[config.pcformat];
		return entry;
	}
	// if no rgba support so swap is needed
	D3DFORMAT d3d_fmt = PC_TexFormat_To_D3DFORMAT[config.pcformat];
	TCacheEntry* entry = new TCacheEntry(config, D3D::CreateTexture2D(config.width, config.height, d3d_fmt, config.levels));
	entry->d3d_fmt = d3d_fmt;
	entry->compressed = d3d_fmt == D3DFMT_DXT1
		|| d3d_fmt == D3DFMT_DXT3
		|| d3d_fmt == D3DFMT_DXT5;
	return entry;
}

void TextureCache::LoadLut(u32 lutFmt, void* addr, u32 size)
{
	s_depaletizer->UploadPalette(lutFmt, addr, size);
}

TextureCache::TextureCache()
{
	for (size_t i = 0; i < MEM_TEXTURE_POOL_SIZE; i++)
	{
		s_memPoolTexture[i] = nullptr;
		s_memPoolTextureW[i] = 1024u;
		s_memPoolTextureH[i] = 1024u;
	}
	s_depaletizer = std::make_unique<Depalettizer>();
}

TextureCache::~TextureCache()
{
	for (size_t i = 0; i < MEM_TEXTURE_POOL_SIZE; i++)
	{
		if (s_memPoolTexture[i])
		{
			s_memPoolTexture[i]->Release();
			s_memPoolTexture[i] = nullptr;
		}
	}
	s_depaletizer.reset();
}

}
