// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>

#include "Common/GL/GLUtil.h"
#include "VideoCommon/BPStructs.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoCommon.h"

namespace OGL
{

class TextureCache : public ::TextureCacheBase
{
public:
	TextureCache();
	~TextureCache();

	static void DisableStage(u32 stage);
	static void SetStage();

private:
	struct TCacheEntry : TextureCacheBase::TCacheEntryBase
	{
		GLuint texture;
		GLuint nrm_texture;
		GLuint framebuffer;
		bool compressed;

		int gl_format;
		int gl_iformat;
		int gl_siformat;
		int gl_type;
		void SetFormat();
		//TexMode0 mode; // current filter and clamp modes that texture is set to
		//TexMode1 mode1; // current filter and clamp modes that texture is set to

		TCacheEntry(const TCacheEntryConfig& config);
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
			return nrm_texture != 0;
		};
		void Bind(u32 stage) override;
		bool Save(const std::string& filename, u32 level) override;
		inline uintptr_t GetInternalObject() override
		{
			return static_cast<uintptr_t>(texture);
		}
	};

	PC_TexFormat GetNativeTextureFormat(const s32 texformat, const TlutFormat tlutfmt, u32 width, u32 height) override;

	TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) override;

	void CopyEFB(u8* dst, const EFBCopyFormat& format, u32 native_width, u32 bytes_per_row,
		u32 num_blocks_y, u32 memory_stride, bool is_depth_copy,
		const EFBRectangle& src_rect, bool scale_by_half) override;
	bool Palettize(TCacheEntryBase* entry, const TCacheEntryBase* base_entry) override;
	void LoadLut(u32 lutFmt, void* addr, u32 size) override;
	bool CompileShaders() override;
	void DeleteShaders() override;
	bool SupportsGPUTextureDecode(TextureFormat format, TlutFormat palette_format) override;
	void* m_last_addr = {};
	u32 m_last_size = {};
	u64 m_last_hash = {};
	u32 m_last_lutFmt = {};
};

bool SaveTexture(const std::string& filename, u32 textarget, u32 tex, int virtual_width, int virtual_height, u32 level, bool compressed = false);

}
