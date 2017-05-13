// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/LinearDiskCache.h"
#include "VideoCommon/TextureConversionShader.h"
#include "VideoBackends/DX11/D3DPtr.h"
#include "VideoBackends/DX11/TextureEncoder.h"

namespace DX11
{

class PSTextureEncoder : public TextureEncoder
{
public:
	PSTextureEncoder();

	void Init();
	void Shutdown();
	void Encode(u8* dst, const EFBCopyFormat& format, u32 native_width,
		u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
		bool is_depth_copy, const EFBRectangle& src_rect, bool scale_by_half);

private:
	ID3D11PixelShader* GetEncodingPixelShader(const EFBCopyFormat& format);
	ID3D11PixelShader* InsertShader(const EFBCopyFormat &key, u8 const *data, u32 sz);
	bool m_ready;

	D3D::Texture2dPtr m_out;
	D3D::RtvPtr m_outRTV;
	D3D::Texture2dPtr m_outStage;
	D3D::BufferPtr m_encodeParams;

	std::map<EFBCopyFormat, D3D::PixelShaderPtr> m_encoding_shaders;

	class ShaderCacheInserter : public LinearDiskCacheReader<EFBCopyFormat, u8>
	{
	public:
		void Read(const EFBCopyFormat &key, const u8 *value, u32 value_size)
		{
			encoder_.InsertShader(key, value, value_size);
		}
		ShaderCacheInserter(PSTextureEncoder &encoder) : encoder_(encoder)
		{}
	private:
		PSTextureEncoder& encoder_;
	};
	friend ShaderCacheInserter;

	LinearDiskCache<EFBCopyFormat, u8> m_shaderCache;

};

}