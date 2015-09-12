// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>

#include "VideoBackends/OGL/GLUtil.h"
#include "VideoCommon/BPStructs.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoCommon.h"

namespace OGL
{

class TextureCache : public ::TextureCache
{
public:
	TextureCache();
	static void DisableStage(u32 stage);
	static void SetStage();

private:
	struct TCacheEntry : TCacheEntryBase
	{
		GLuint texture;
		GLuint nrm_texture;
		GLuint framebuffer;
		bool compressed;

		int gl_format;
		int gl_iformat;
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
		void Load(const u8* src, u32 width, u32 height, u32 expandedWidth,
			u32 expandedHeight, const s32 texformat, const u32 tlutaddr, const TlutFormat tlutfmt, u32 level) override;
		void LoadFromTmem(const u8* ar_src, const u8* gb_src, u32 width, u32 height,
			u32 expanded_width, u32 expanded_Height, u32 level) override;

		void FromRenderTarget(u8 *dst, unsigned int dstFormat, u32 dstStride,
			PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
			bool isIntensity, bool scaleByHalf, u32 cbufid,
			const float *colmat) override;
		bool SupportsMaterialMap() const override { return nrm_texture != 0; };
		bool PalettizeFromBase(const TCacheEntryBase* base_entry) override;
		void Bind(u32 stage) override;
		bool Save(const std::string& filename, u32 level) override;
	};

	~TextureCache();

	PC_TexFormat GetNativeTextureFormat(const s32 texformat, const TlutFormat tlutfmt, u32 width, u32 height);

	TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) override;

	void LoadLut(u32 lutFmt, void* addr, u32 size);
	void CompileShaders() override;
	void DeleteShaders() override;
};

bool SaveTexture(const std::string& filename, u32 textarget, u32 tex, int virtual_width, int virtual_height, u32 level);

}
