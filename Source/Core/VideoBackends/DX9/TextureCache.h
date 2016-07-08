// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>

#include "VideoBackends/DX9/D3DBase.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoCommon.h"

namespace DX9
{

class TextureCache: public ::TextureCacheBase
{
public:
	TextureCache();
	~TextureCache();
private:
	struct TCacheEntry: TCacheEntryBase
	{
		LPDIRECT3DTEXTURE9 texture;

		D3DFORMAT d3d_fmt;
		bool compressed;
		TCacheEntry(const TCacheEntryConfig& config, LPDIRECT3DTEXTURE9 _tex): TCacheEntryBase(config), texture(_tex), compressed(false)
		{}
		~TCacheEntry();

		void CopyRectangleFromTexture(
			const TCacheEntryBase* source,
			const MathUtil::Rectangle<int> &srcrect,
			const MathUtil::Rectangle<int> &dstrect) override;

		void ReplaceTexture(const u8* src, u32 width, u32 height,
			u32 expanded_width, u32 level, bool swap_r_b);

		void Load(const u8* src, u32 width, u32 height,
			u32 expanded_width, u32 level) override;
		void LoadMaterialMap(const u8* src, u32 width, u32 height, u32 level) override
		{};
		void Load(const u8* src, u32 width, u32 height, u32 expandedWidth,
			u32 expandedHeight, const s32 texformat, const u32 tlutaddr, const TlutFormat tlutfmt, u32 level) override;
		void LoadFromTmem(const u8* ar_src, const u8* gb_src, u32 width, u32 height,
			u32 expanded_width, u32 expanded_Height, u32 level) override;

		void FromRenderTarget(u8* dst, PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
			bool scaleByHalf, unsigned int cbufid, const float *colmat) override;
		bool SupportsMaterialMap() const override
		{
			return false;
		}
		void Bind(u32 stage, u32 last_texture) override;
		bool Save(const std::string& filename, u32 level) override;
	};

	PC_TexFormat GetNativeTextureFormat(const s32 texformat, const TlutFormat tlutfmt, u32 width, u32 height);

	TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config);

	void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
		PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
		bool isIntensity, bool scaleByHalf) override;
	bool Palettize(TCacheEntryBase* entry, const TCacheEntryBase* base_entry) override;
	void LoadLut(u32 lutFmt, void* addr, u32 size);
	void CompileShaders() override
	{}
	void DeleteShaders() override
	{}
};

}