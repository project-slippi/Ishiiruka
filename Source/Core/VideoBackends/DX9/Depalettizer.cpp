// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Hash.h"

#include "VideoBackends/DX9/Depalettizer.h"

#include "VideoBackends/DX9/D3DShader.h"
#include "VideoBackends/DX9/D3DTexture.h"
#include "VideoBackends/DX9/FramebufferManager.h"
#include "VideoBackends/DX9/Render.h"
#include "VideoBackends/DX9/VertexShaderCache.h"

#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/TextureDecoder.h"


namespace DX9
{

#define SAFE_RELEASE(p) if (p) { (p)->Release(); (p) = nullptr; }

static const char DEPALETTIZE_1dSHADER[] = R"HLSL(
//
#ifndef NUM_COLORS
#error NUM_COLORS was not defined
#endif

uniform sampler s_Base : register(s0);
uniform sampler s_Palette : register(s1);

void main(out float4 ocol0 : COLOR0, in float2 uv0 : TEXCOORD0)
{
	float sample = tex2D(s_Base, uv0);
	float index = round(sample * (NUM_COLORS-1));
	ocol0 = tex1D(s_Palette, (index + 0.5) / NUM_COLORS);
}
//
)HLSL";

static D3DFORMAT s_internal_formats[] = {
	D3DFMT_A8L8,
	D3DFMT_R5G6B5,
	D3DFMT_A8R8G8B8
};

static u32 s_palette_size_w[] = {
	16,
	256,
	256,
};

static u32 s_palette_size_h[] = {
	1,
	1,
	256,
};

Depalettizer::Depalettizer()
	: m_unorm4Shader(nullptr), m_unorm8Shader(nullptr), m_PalleteFormat(IA)
{
	for (size_t i = 0; i < 3; i++)
	{
		for (size_t j = 0; j < 2; j++)
		{
			m_palette_texture[i][j] = nullptr;
			m_staging_texture[i][j] = nullptr;
		}
	}
}

Depalettizer::~Depalettizer()
{
	SAFE_RELEASE(m_unorm4Shader);
	SAFE_RELEASE(m_unorm8Shader);
	for (size_t i = 0; i < 3; i++)
	{
		for (size_t j = 0; j < 2; j++)
		{
			SAFE_RELEASE(m_palette_texture[i][j]);
		}
	}
}

bool Depalettizer::Depalettize(LPDIRECT3DTEXTURE9 dstTex,
	LPDIRECT3DTEXTURE9 baseTex, BaseType baseType)
{
	LPDIRECT3DPIXELSHADER9 shader = GetShader(baseType);
	if (!shader)
		return false;

	D3DSURFACE_DESC dstDesc;
	dstTex->GetLevelDesc(0, &dstDesc);

	g_renderer->ResetAPIState();

	D3D::dev->SetDepthStencilSurface(nullptr);
	LPDIRECT3DSURFACE9 renderSurface = nullptr;
	dstTex->GetSurfaceLevel(0, &renderSurface);
	D3D::dev->SetRenderTarget(0, renderSurface);

	D3DVIEWPORT9 vp = { 0, 0, dstDesc.Width, dstDesc.Height, 0.f, 1.f };
	D3D::dev->SetViewport(&vp);

	if (m_palette_texture[m_PalleteFormat][baseType] == nullptr)
	{
		PanicAlert("Palette not uploaded.");
	}

	// Set shader inputs
	// Texture 0 will be set to baseTex by drawShadedTexQuad
	D3D::SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	D3D::SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	D3D::SetTexture(1, m_palette_texture[m_PalleteFormat][baseType]);
	D3D::SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	D3D::SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_POINT);

	// Depalettize!
	RECT rectSrc = { 0, 0, 1, 1 };
	D3D::drawShadedTexQuad(baseTex,
		&rectSrc,
		1, 1,
		dstDesc.Width, dstDesc.Height,
		shader,
		VertexShaderCache::GetSimpleVertexShader(0)
	);

	SAFE_RELEASE(renderSurface);
	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
	// Clean up
	D3D::SetTexture(1, nullptr);
	g_renderer->RestoreAPIState();
	return true;
}

// Decode to D3DFMT_A8L8
inline void DecodeIA8Palette(u16* dst, const u16* src, unsigned int numColors)
{
	for (unsigned int i = 0; i < numColors; ++i)
	{
		// FIXME: Do we need swap16?
		dst[i] = src[i];
	}
}

// Decode to D3DFMT_R5G6B5
inline void DecodeRGB565Palette(u16* dst, const u16* src, unsigned int numColors)
{
	for (unsigned int i = 0; i < numColors; ++i)
		dst[i] = Common::swap16(src[i]);
}

inline u32 decode5A3(u16 val)
{
	s32 r, g, b, a;
	if ((val & 0x8000))
	{
		a = 0xFF;
		r = Convert5To8((val >> 10) & 0x1F);
		g = Convert5To8((val >> 5) & 0x1F);
		b = Convert5To8(val & 0x1F);
	}
	else
	{
		a = Convert3To8((val >> 12) & 0x7);
		r = Convert4To8((val >> 8) & 0xF);
		g = Convert4To8((val >> 4) & 0xF);
		b = Convert4To8(val & 0xF);
	}
	return (a << 24) | (r << 16) | (g << 8) | b;
}

// Decode to D3DFMT_A8R8G8B8
inline void DecodeRGB5A3Palette(u32* dst, const u16* src, unsigned int numColors)
{
	for (unsigned int i = 0; i < numColors; ++i)
	{
		dst[i] = decode5A3(Common::swap16(src[i]));
	}
}

void Depalettizer::UploadPalette(u32 tlutFmt, void* addr, u32 size)
{
	if (tlutFmt == m_last_tlutFmt && addr == m_last_addr && size == m_last_size && m_last_hash)
	{
		u64 hash = GetHash64(reinterpret_cast<u8*>(addr), size, g_ActiveConfig.iSafeTextureCache_ColorSamples);
		if (hash == m_last_hash)
		{
			return;
		}
		m_last_hash = hash;
	}
	else
	{
		m_last_hash = GetHash64(reinterpret_cast<u8*>(addr), size, g_ActiveConfig.iSafeTextureCache_ColorSamples);
	}
	m_last_tlutFmt = tlutFmt;
	m_last_addr = addr;
	m_last_size = size;
	HRESULT hr;
	InternalPaletteFormat format = IA;
	BaseType source_type = BaseType::Unorm4;
	UINT numColors = size / sizeof(u16);
	if (numColors <= 16)
	{
		source_type = BaseType::Unorm4;
	}
	else if (numColors <= 256)
	{
		source_type = BaseType::Unorm8;
	}
	else
	{
		return;
	}
	switch (tlutFmt)
	{
	case GX_TL_IA8: format = IA; break;
	case GX_TL_RGB565: format = RGB565; break;
	case GX_TL_RGB5A3: format = RGBA8; break;
	default:
		ERROR_LOG(VIDEO, "Invalid TLUT format");
		return;
	}
	const u16* lut = (const u16*)addr;
	LPDIRECT3DTEXTURE9 staging_texture;
	LPDIRECT3DTEXTURE9 palette_texture;
	D3DLOCKED_RECT lock;
	if (m_palette_texture[format][source_type] == nullptr)
	{
		m_palette_texture[format][source_type] = D3D::CreateTexture2D(
			s_palette_size_w[source_type],
			s_palette_size_h[source_type],
			s_internal_formats[format], 1);
		m_staging_texture[format][source_type] = D3D::CreateTexture2D(
			s_palette_size_w[source_type],
			s_palette_size_h[source_type],
			s_internal_formats[format], 1, D3DPOOL_SYSTEMMEM);
	}
	staging_texture = m_staging_texture[format][source_type];
	palette_texture = m_palette_texture[format][source_type];
	hr = staging_texture->LockRect(0, &lock, NULL, D3DLOCK_DISCARD);
	if (FAILED(hr))
	{
		ERROR_LOG(VIDEO, "Failed to lock palette texture");
		return;
	}

	switch (format)
	{
	case IA: DecodeIA8Palette((u16*)lock.pBits, lut, numColors); break;
	case RGB565: DecodeRGB565Palette((u16*)lock.pBits, lut, numColors); break;
	case RGBA8: DecodeRGB5A3Palette((u32*)lock.pBits, lut, numColors); break;
	}

	staging_texture->UnlockRect(0);
	PDIRECT3DSURFACE9 srcsurf;
	staging_texture->GetSurfaceLevel(0, &srcsurf);
	PDIRECT3DSURFACE9 dstsurface;
	palette_texture->GetSurfaceLevel(0, &dstsurface);
	D3D::dev->UpdateSurface(srcsurf, nullptr, dstsurface, nullptr);
	srcsurf->Release();
	dstsurface->Release();
	m_PalleteFormat = format;
}

LPDIRECT3DPIXELSHADER9 Depalettizer::GetShader(BaseType type)
{
	switch (type)
	{
	case Unorm4:
		if (!m_unorm4Shader)
		{
			D3D_SHADER_MACRO macros[] = {
				{ "NUM_COLORS", "16" },
				{ nullptr, nullptr }
			};
			m_unorm4Shader = D3D::CompileAndCreatePixelShader(DEPALETTIZE_1dSHADER,
				sizeof(DEPALETTIZE_1dSHADER), macros);
		}
		return m_unorm4Shader;
	case Unorm8:
		if (!m_unorm8Shader)
		{
			D3D_SHADER_MACRO macros[] = {
				{ "NUM_COLORS", "256" },
				{ nullptr, nullptr }
			};
			m_unorm8Shader = D3D::CompileAndCreatePixelShader(DEPALETTIZE_1dSHADER,
				sizeof(DEPALETTIZE_1dSHADER), macros);
		}
		return m_unorm8Shader;
	default:
		return nullptr;
	}
}

}
