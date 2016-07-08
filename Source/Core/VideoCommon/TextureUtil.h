// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include "Common/Common.h"
#include "VideoCommon/TextureDecoder.h"
namespace TextureUtil
{
#ifdef _WIN32
void ConvertRGBA_BGRA(u32 *dst, const s32 dstPitch, u32 *pIn, const s32 width, const s32 height, const s32 pitch);
void ConvertRGBA565_BGRA(u32 *dst, const s32 dstPitch, u16 *pIn, const s32 width, const s32 height, const s32 pitch);
void ConvertRGBA565_RGBA(u32 *dst, const s32 dstPitch, u16 *pIn, const s32 width, const s32 height, const s32 pitch);
#endif
void CopyTextureData(u8 *pDst, const u8 *pSrc, const s32 width, const s32 height, const s32 srcpitch, const s32 dstpitch, const s32 pixelsize);
void ExpandI8Data(u8 *pDst, const u8 *pSrc, const s32 width, const s32 height, const s32 srcpitch, const s32 dstpitch);
void CopyCompressedTextureData(u8 *pDst, const u8 *pSrc, const s32 width, const s32 height, const s32 dstPitch, s32 numBytesPerBlock, const s32 dstpitch);
s32 GetTextureSizeInBytes(u32 width, u32 height, PC_TexFormat fmt);
u32 CalculateLevelSize(u32 level_0_size, u32 level);
}
