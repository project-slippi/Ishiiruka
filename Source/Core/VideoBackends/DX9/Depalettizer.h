// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/VideoCommon.h"

#include "VideoBackends/DX9/D3DUtil.h"

namespace DX9
{

// "Depalletize" means to remove stuff from a wooden pallet.
// "Depalettize" means to convert from a color-indexed image to a direct-color
// image.
class Depalettizer
{
public:
	enum InternalPaletteFormat
	{
		IA = 0,
		RGB565,
		RGBA8
	};
	enum BaseType
	{
		Unorm4 = 0,
		Unorm8
	};

	Depalettizer();
	~Depalettizer();

	bool Depalettize(LPDIRECT3DTEXTURE9 dstTex, LPDIRECT3DTEXTURE9 baseTex,
		BaseType baseType);
	void UploadPalette(u32 tlutFmt, void* addr, u32 size);

private:
	LPDIRECT3DPIXELSHADER9 GetShader(BaseType type);

	InternalPaletteFormat m_PalleteFormat;

	LPDIRECT3DTEXTURE9 m_palette_texture[3][2];
	LPDIRECT3DTEXTURE9 m_staging_texture[3][2];

	// Depalettizing shader for 4-bit indices as normalized float
	LPDIRECT3DPIXELSHADER9 m_unorm4Shader;
	// Depalettizing shader for 8-bit indices as normalized float
	LPDIRECT3DPIXELSHADER9 m_unorm8Shader;
	// Depalettizing shader for 16-bit indices as normalized float
	LPDIRECT3DPIXELSHADER9 m_unorm16Shader;
	u32 m_last_tlutFmt = {};
	void* m_last_addr = {};
	u32 m_last_size = {};
	u64 m_last_hash = {};
};

}