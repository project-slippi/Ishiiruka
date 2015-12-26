// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/D3D12/TextureEncoder.h"

#include "VideoCommon/TextureCacheBase.h"

namespace DX12
{

class PSTextureEncoder : public TextureEncoder
{
public:
	PSTextureEncoder();

	void Init();
	void Shutdown();
	void Encode(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
	              PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
	              bool isIntensity, bool scaleByHalf);

private:
	bool m_ready;

	ID3D12Resource* m_out12;
	D3D12_CPU_DESCRIPTOR_HANDLE m_outRTV12cpu;

	ID3D12Resource* m_outStage12;
	void* m_outStage12data;

	ID3D12Resource* m_encodeParams12;
	void* m_encodeParams12data;

	D3D12_SHADER_BYTECODE SetStaticShader12(unsigned int dstFormat,
		PEControl::PixelFormat srcFormat, bool isIntensity, bool scaleByHalf);

	typedef unsigned int ComboKey; // Key for a shader combination

	ComboKey MakeComboKey(unsigned int dstFormat,
		PEControl::PixelFormat srcFormat, bool isIntensity, bool scaleByHalf)
	{
		return (dstFormat << 4) | (static_cast<int>(srcFormat) << 2) | (isIntensity ? (1<<1) : 0)
			| (scaleByHalf ? (1<<0) : 0);
	}

	typedef std::map<ComboKey, D3D12_SHADER_BYTECODE> ComboMap12;
	ComboMap12 m_staticShaders12;
	std::vector<D3DBlob*> m_staticShaders12blobs;
};

}
