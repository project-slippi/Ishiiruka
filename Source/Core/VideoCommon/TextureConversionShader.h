// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <utility>

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
const char *GenerateEncodingShader(const EFBCopyFormat& format, API_TYPE ApiType = API_OPENGL);

// View format of the input data to the texture decoding shader.
enum BufferFormat
{
	BUFFER_FORMAT_R8_UINT,
	BUFFER_FORMAT_R16_UINT,
	BUFFER_FORMAT_R32G32_UINT,
	BUFFER_FORMAT_COUNT
};

// Information required to compile and dispatch a texture decoding shader.
struct DecodingShaderInfo
{
	BufferFormat buffer_format;
	u32 palette_size;
	u32 group_size_x;
	u32 group_size_y;
	bool group_flatten;
	const char* shader_body;
};

// Obtain shader information for the specified texture format.
// If this format does not have a shader written for it, returns nullptr.
const DecodingShaderInfo* GetDecodingShaderInfo(TextureFormat format);

// Determine how many bytes there are in each element of the texel buffer.
// Needed for alignment and stride calculations.
u32 GetBytesPerBufferElement(BufferFormat buffer_format);

// Determine how many thread groups should be dispatched for an image of the specified width/height.
// First is the number of X groups, second is the number of Y groups, Z is always one.
std::pair<u32, u32> GetDispatchCount(const DecodingShaderInfo* info, u32 width, u32 height);

// Returns the GLSL string containing the texture decoding shader for the specified format.
std::string GenerateDecodingShader(TextureFormat format, TlutFormat palette_format, API_TYPE ApiType = API_OPENGL);
}

namespace TextureConversionShaderLegacy
{
const char *GenerateEncodingShader(const EFBCopyFormat& format);
void SetShaderParameters(float width, float height, float offsetX, float offsetY, float widthStride, float heightStride, float buffW = 0.0f, float buffH = 0.0f);
}