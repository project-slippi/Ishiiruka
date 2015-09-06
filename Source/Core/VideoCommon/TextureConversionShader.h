// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include "Common/CommonTypes.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoCommon.h"

namespace TextureConversionShader
{
	inline u16 GetEncodedSampleCount(u32 format)
	{
		switch (format)
		{
		case GX_TF_I4: return 8;
		case GX_TF_I8: return 4;
		case GX_TF_IA4: return 4;
		case GX_TF_IA8: return 2;
		case GX_TF_RGB565: return 2;
		case GX_TF_RGB5A3: return 2;
		case GX_TF_RGBA8: return 1;
		case GX_CTF_R4: return 8;
		case GX_CTF_RA4: return 4;
		case GX_CTF_RA8: return 2;
		case GX_CTF_A8: return 4;
		case GX_CTF_R8: return 4;
		case GX_CTF_G8: return 4;
		case GX_CTF_B8: return 4;
		case GX_CTF_RG8: return 2;
		case GX_CTF_GB8: return 2;
		case GX_CTF_Z8H: return 4;
		case GX_TF_Z8: return 4;
		case GX_CTF_Z16R: return 2;
		case GX_TF_Z16: return 2;
		case GX_TF_Z24X8: return 1;
		case GX_CTF_Z4: return 8;
		case GX_CTF_Z8M: return 4;
		case GX_CTF_Z8L: return 4;
		case GX_CTF_Z16L: return 2;
		default: return 1;
		}
	}
}

namespace TextureConversionShaderLegacy
{
const char *GenerateEncodingShader(u32 format);
void SetShaderParameters(float width, float height, float offsetX, float offsetY, float widthStride, float heightStride, float buffW = 0.0f, float buffH = 0.0f);
}

namespace TextureConversionShader
{
const char *GenerateEncodingShader(u32 format, API_TYPE ApiType = API_OPENGL);
}