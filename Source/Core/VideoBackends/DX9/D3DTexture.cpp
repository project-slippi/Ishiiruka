// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#include "Common/MsgHandler.h"

#include "VideoBackends/DX9/D3DBase.h"
#include "VideoBackends/DX9/D3DTexture.h"
#include "VideoCommon/TextureUtil.h"

namespace DX9
{

namespace D3D
{

inline void LoadDataToRect(LPDIRECT3DTEXTURE9 pTexture, const u8* buffer, const int width, const int height, const int pitch, D3DFORMAT fmt, const bool swap_r_b, int level)
{
	D3DLOCKED_RECT Lock;
	pTexture->LockRect(level, &Lock, NULL, 0);
	s32 pixelsize = 0;
	switch (fmt)
	{
	case D3DFMT_L8:
	case D3DFMT_A8:
	case D3DFMT_A4L4:
		pixelsize = 1;
		break;
	case D3DFMT_R5G6B5:
		pixelsize = 2;
		break;
	case D3DFMT_A8P8:
		TextureUtil::ExpandI8Data((u8*)Lock.pBits, buffer, width, height, pitch, Lock.Pitch);
		break;
	case D3DFMT_A8L8:
		pixelsize = 2;
		break;
	case D3DFMT_A8R8G8B8:
		if (!swap_r_b)
		{
			pixelsize = 4;
		}
		else
		{
			TextureUtil::ConvertRGBA_BGRA((u32 *)Lock.pBits, Lock.Pitch, (u32*)buffer, width, height, pitch);
		}
		break;
	case D3DFMT_DXT1:
	case D3DFMT_DXT3:
	case D3DFMT_DXT5:
		TextureUtil::CopyCompressedTextureData((u8*)Lock.pBits, buffer, width, height, pitch, fmt == D3DFMT_DXT1 ? 8 : 16, Lock.Pitch);
		break;
	default:
		PanicAlert("D3D: Invalid texture format %i", fmt);
	}
	if (pixelsize > 0)
	{
		TextureUtil::CopyTextureData((u8*)Lock.pBits, buffer, width, height, pitch * pixelsize, Lock.Pitch, pixelsize);
	}
	pTexture->UnlockRect(level);
}

LPDIRECT3DTEXTURE9 CreateTexture2D(const s32 width, const s32 height, D3DFORMAT fmt, s32 levels, D3DPOOL pool)
{
	LPDIRECT3DTEXTURE9 pTexture;
	D3DFORMAT cfmt = fmt;
	if (fmt == D3DFMT_A8P8)
	{
		cfmt = D3DFMT_A8L8;
	}
	HRESULT hr = dev->CreateTexture(width, height, levels, 0, cfmt, pool, &pTexture, NULL);
	if (FAILED(hr))
	{
		PanicAlert("Failed to create texture at %s, line %d: hr=%#x\n", __FILE__, __LINE__, hr);
		return 0;
	}
	return pTexture;
}

void ReplaceTexture2D(LPDIRECT3DTEXTURE9 pTexture, const u8* buffer, const s32 width, const s32 height, const s32 pitch, D3DFORMAT fmt, bool swap_r_b, s32 level)
{
	LoadDataToRect(pTexture, buffer, width, height, pitch, fmt, swap_r_b, level);
}

}  // namespace

}  // namespace DX9