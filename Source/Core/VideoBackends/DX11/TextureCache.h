// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include "VideoBackends/DX11/D3DTexture.h"
#include "VideoBackends/DX11/TextureEncoder.h"
#include "VideoCommon/TextureCacheBase.h"

namespace DX11
{

class TextureCache : public ::TextureCacheBase
{
public:
	TextureCache();
	~TextureCache();

private:
	struct TCacheEntry : TCacheEntryBase
	{
		D3DTexture2D *texture;
		D3DTexture2D *nrm_texture;
		DXGI_FORMAT DXGI_format;
		D3D11_USAGE usage;
		bool swap_rg;
		bool convertrgb565;
		bool compressed;

		TCacheEntry(const TCacheEntryConfig& config, D3DTexture2D *_tex) : TCacheEntryBase(config),
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
		void FromRenderTarget(bool is_depth_copy, const EFBRectangle& srcRect,
			bool scaleByHalf, unsigned int cbufid, const float *colmat, u32 width, u32 height) override;
		bool DecodeTextureOnGPU(u32 dst_level, const u8* data,
			u32 data_size, TextureFormat format, u32 width, u32 height,
			u32 aligned_width, u32 aligned_height, u32 row_stride,
			const u8* palette, TlutFormat palette_format) override;
		bool SupportsMaterialMap() const override
		{
			return nrm_texture != nullptr;
		};
		void Bind(u32 stage) override;
		bool Save(const std::string& filename, u32 level) override;
		inline uintptr_t GetInternalObject() override
		{
			return reinterpret_cast<uintptr_t>(texture);
		};
	};

	PC_TexFormat GetNativeTextureFormat(const s32 texformat, const TlutFormat tlutfmt, u32 width, u32 height) override;

	TextureCache::TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config);

	void CopyEFB(u8* dst, const EFBCopyFormat& format, u32 native_width, u32 bytes_per_row,
		u32 num_blocks_y, u32 memory_stride, bool is_depth_copy,
		const EFBRectangle& src_rect, bool scale_by_half) override;
	bool Palettize(TCacheEntryBase* entry, const TCacheEntryBase* base_entry) override;
	void LoadLut(u32 lutFmt, void* addr, u32 size) override;
	bool SupportsGPUTextureDecode(TextureFormat format, TlutFormat palette_format) override;
	bool CompileShaders() override
	{
		return true;
	}
	void DeleteShaders() override
	{}
};

}
