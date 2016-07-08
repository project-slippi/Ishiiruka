// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/DX9/D3DBase.h"

namespace DX9
{

namespace D3D
{
LPDIRECT3DTEXTURE9 CreateTexture2D(const s32 width, const s32 height, D3DFORMAT fmt = D3DFMT_A8R8G8B8, s32 levels = 1, D3DPOOL pool = D3DPOOL_DEFAULT);
void ReplaceTexture2D(LPDIRECT3DTEXTURE9 pTexture, const u8* buffer, const s32 width, const s32 height, const s32 pitch, D3DFORMAT fmt, bool swap_r_b, s32 level = 0);
}

}  //  namespace DX9
