// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/TextureCacheBase.h"

namespace Null
{
class TextureCache : public TextureCacheBase
{
public:
  TextureCache() {}
  ~TextureCache() {}
  bool CompileShaders() override { return true; }
  void DeleteShaders() override {}

  void CopyEFB(u8* dst, const EFBCopyFormat& format, u32 native_width, u32 bytes_per_row,
		           u32 num_blocks_y, u32 memory_stride,
		           bool is_depth_copy, const EFBRectangle& src_rect, bool scale_by_half) override
  {
  }

  PC_TexFormat GetNativeTextureFormat(const s32 texformat,
		const TlutFormat tlutfmt, u32 width, u32 height) override
	{return PC_TexFormat::PC_TEX_FMT_NONE;}

  bool Palettize(TCacheEntryBase* entry, const TCacheEntryBase* base_entry) override {return false;}
  void LoadLut(u32 lutFmt, void* addr, u32 size) override {}
private:
  struct TCacheEntry : TCacheEntryBase
  {
    TCacheEntry(const TCacheEntryConfig& _config) : TCacheEntryBase(_config) {}
    ~TCacheEntry() {}
    uintptr_t GetInternalObject() {return 0;}
    void Load(const u8* buffer, u32 width, u32 height, u32 expanded_width, u32 level) override {}
    void FromRenderTarget(bool is_depth_copy, const EFBRectangle& src_rect, bool scale_by_half,
                          u32 cbufid, const float* colmat, u32 width, u32 height) override
    {
    }

    bool SupportsMaterialMap() const {return false;}

    void CopyRectangleFromTexture(const TCacheEntryBase* source,
                                  const MathUtil::Rectangle<int>& srcrect,
                                  const MathUtil::Rectangle<int>& dstrect) override
    {
    }

    void Bind(unsigned int stage) override {}
    bool Save(const std::string& filename, unsigned int level) override { return false; }
  };

  TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) override
  {
    return new TCacheEntry(config);
  }
};

}  // Null name space
