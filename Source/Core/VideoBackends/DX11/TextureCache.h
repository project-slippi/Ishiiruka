// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include "VideoBackends/DX11/D3DTexture.h"
#include "VideoBackends/DX11/TextureEncoder.h"
#include "VideoCommon/TextureCacheBase.h"

namespace DX11
{

class TextureCache: public ::TextureCacheBase
{
public:
	TextureCache();
	~TextureCache();

private:
	struct TCacheEntry: TCacheEntryBase
	{
		D3DTexture2D *texture;
		D3DTexture2D *nrm_texture;
		DXGI_FORMAT DXGI_format;
		D3D11_USAGE usage;
		bool swap_rg;
		bool convertrgb565;
		bool compressed;

		TCacheEntry(const TCacheEntryConfig& config, D3DTexture2D *_tex): TCacheEntryBase(config),
			texture(_tex), nrm_texture(nullptr), swap_rg(false), convertrgb565(false), compressed(false)
		{}
		~TCacheEntry();

		void CopyRectangleFromTexture(
			const TCacheEntryBase* source,
			const MathUtil::Rectangle<int> &srcrect,
			const MathUtil::Rectangle<int> &dstrect) override;

		void Load(const u8* src, u32 width, u32 height,
			u32 expanded_width, u32 level) override;
		void LoadMaterialMap(const u8* src, u32 width, u32 height, u32 level) override;
		void Load(const u8* src, u32 width, u32 height, u32 expandedWidth,
			u32 expandedHeight, const s32 texformat, const u32 tlutaddr, const TlutFormat tlutfmt, u32 level) override;
		void LoadFromTmem(const u8* ar_src, const u8* gb_src, u32 width, u32 height,
			u32 expanded_width, u32 expanded_Height, u32 level) override;

		void FromRenderTarget(u8* dst, PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
			bool scaleByHalf, unsigned int cbufid, const float *colmat) override;
		bool SupportsMaterialMap() const override
		{
			return nrm_texture != nullptr;
		};
		void Bind(u32 stage, u32 last_texture) override;
		bool Save(const std::string& filename, u32 level) override;
	};

	PC_TexFormat GetNativeTextureFormat(const s32 texformat, const TlutFormat tlutfmt, u32 width, u32 height);

	TextureCache::TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config);

	void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
		PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
		bool isIntensity, bool scaleByHalf) override;
	bool Palettize(TCacheEntryBase* entry, const TCacheEntryBase* base_entry) override;
	void LoadLut(u32 lutFmt, void* addr, u32 size) override;
	void CompileShaders() override
	{}
	void DeleteShaders() override
	{}
};

}
