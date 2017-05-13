// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/D3D12/TextureEncoder.h"

#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureConversionShader.h"

namespace DX12
{

class PSTextureEncoder final : public TextureEncoder
{
public:
	PSTextureEncoder();

	void Init();
	void Shutdown();
	void Encode(u8* dst, const EFBCopyFormat& format, u32 native_width, u32 bytes_per_row,
		u32 num_blocks_y, u32 memory_stride, bool is_depth_copy, const EFBRectangle& src_rect,
		bool scale_by_half);

private:
	D3D12_SHADER_BYTECODE GetEncodingPixelShader(const EFBCopyFormat& format);
	bool m_ready = false;

	ComPtr<ID3D12Resource> m_out;
	D3D12_CPU_DESCRIPTOR_HANDLE m_out_rtv_cpu = {};

	ComPtr<ID3D12Resource> m_out_readback_buffer;

	ComPtr<ID3D12Resource> m_encode_params_buffer;
	void* m_encode_params_buffer_data = nullptr;

	std::map<EFBCopyFormat, D3D12_SHADER_BYTECODE> m_encoding_shaders;
	std::vector<D3DBlob*> m_shader_blobs;

	void InitializeRTV();

};

}
