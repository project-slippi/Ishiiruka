// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/LinearDiskCache.h"
#include "VideoBackends/DX11/D3DPtr.h"
#include "VideoBackends/DX11/TextureEncoder.h"

namespace DX11
{

class PSTextureEncoder: public TextureEncoder
{
public:
	PSTextureEncoder();

	void Init();
	void Shutdown();
	void Encode(u8* dest_ptr, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
		PEControl::PixelFormat srcFormat, bool isIntensity, bool scaleByHalf, const EFBRectangle& srcRect);

private:
	bool m_ready;

	D3D::Texture2dPtr m_out;
	D3D::RtvPtr m_outRTV;
	D3D::Texture2dPtr m_outStage;
	D3D::BufferPtr m_encodeParams;

	ID3D11PixelShader* SetStaticShader(unsigned int dstFormat,
		PEControl::PixelFormat srcFormat, bool isIntensity, bool scaleByHalf);

	typedef unsigned int ComboKey; // Key for a shader combination

	ComboKey MakeComboKey(unsigned int dstFormat,
		PEControl::PixelFormat srcFormat, bool isIntensity, bool scaleByHalf)
	{
		return (dstFormat << 4) | (static_cast<int>(srcFormat) << 2) | (isIntensity ? (1 << 1) : 0)
			| (scaleByHalf ? (1 << 0) : 0);
	}

	typedef std::map<ComboKey, D3D::PixelShaderPtr> ComboMap;

	ComboMap m_staticShaders;
};

}