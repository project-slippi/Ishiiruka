// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/DX9/D3DBase.h"
#include "VideoBackends/DX9/D3DShader.h"
#include "VideoBackends/DX9/D3DTexture.h"
#include "VideoBackends/DX9/D3DUtil.h"

#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/TextureCacheBase.h"

namespace DX9
{

// Converts textures between formats using shaders
// TODO: support multiple texture formats
namespace TextureConverter
{

void Init();
void Shutdown();

void EncodeToRamYUYV(LPDIRECT3DTEXTURE9 srcTexture, const TargetRectangle& sourceRc, u8* destAddr, u32 dstwidth, u32 dstStride, u32 dstHeight, float Gamma);

void DecodeToTexture(u32 xfbAddr, int srcWidth, int srcHeight, LPDIRECT3DTEXTURE9 destTexture);

// returns size of the encoded data (in bytes)
void EncodeToRamFromTexture(u8* dest_ptr, const EFBCopyFormat& format, u32 native_width,
	u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
	bool is_depth_copy, const EFBRectangle& src_rect, bool scale_by_half);


}

}  // namespace DX9