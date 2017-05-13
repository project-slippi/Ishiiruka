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

class TextureCache : public ::TextureCacheBase
{
public:
	TextureCache();
	~TextureCache();
private:
	struct TCacheEntry : TCacheEntryBase
	{
		LPDIRECT3DTEXTURE9 texture;

		D3DFORMAT d3d_fmt;
		bool compressed;
		TCacheEntry(const TCacheEntryConfig& config, LPDIRECT3DTEXTURE9 _tex) : TCacheEntryBase(config), texture(_tex), compressed(false)
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
		void FromRenderTarget(bool is_depth_copy, const EFBRectangle& srcRect,
			bool scaleByHalf, unsigned int cbufid, const float *colmat, u32 width, u32 height) override;
		bool SupportsMaterialMap() const override
		{
			return false;
		}
		void Bind(u32 stage) override;
		bool Save(const std::string& filename, u32 level) override;
		inline uintptr_t GetInternalObject() override
		{
			return reinterpret_cast<uintptr_t>(texture);
		}
	};

	PC_TexFormat GetNativeTextureFormat(const s32 texformat, const TlutFormat tlutfmt, u32 width, u32 height) override;

	TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config);

	void CopyEFB(u8* dst, const EFBCopyFormat& format, u32 native_width, u32 bytes_per_row,
		u32 num_blocks_y, u32 memory_stride, bool is_depth_copy,
		const EFBRectangle& src_rect, bool scale_by_half) override;
	bool Palettize(TCacheEntryBase* entry, const TCacheEntryBase* base_entry) override;
	void LoadLut(u32 lutFmt, void* addr, u32 size);
	bool CompileShaders() override
	{
		return true;
	}
	void DeleteShaders() override
	{}
};

}