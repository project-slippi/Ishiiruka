// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <tuple>

#include "Common/LinearDiskCache.h"
#include "VideoBackends/DX11/D3DPtr.h"
#include "VideoBackends/DX11/TextureEncoder.h"

namespace DX11
{

class CSTextureEncoder : public TextureEncoder
{

public:

	CSTextureEncoder() = default;

	void Init();
	void Shutdown();
	void Encode(u8* dst, const EFBCopyFormat& format, u32 native_width, u32 bytes_per_row,
		u32 num_blocks_y, u32 memory_stride, bool is_depth_copy, const EFBRectangle& src_rect,
		bool scale_by_half);
private:
	struct  ComboKey{
		EFBCopyFormat format;
		bool scaled;
		bool operator<(const ComboKey& rhs) const
		{
			return std::tie(format, scaled) < std::tie(rhs.format, rhs.scaled);
		}
		bool operator == (const ComboKey& rhs) const
		{
			return format == rhs.format && scaled == rhs.scaled;
		}
	};

	ID3D11ComputeShader* GetEncodingComputeShader(const ComboKey& key);
	ID3D11ComputeShader* InsertShader(const ComboKey& key, u8 const *data, u32 sz);
	bool m_ready{};

	D3D::BufferPtr m_out;
	D3D::BufferPtr m_outStage;
	D3D::UavPtr m_outUav;

	D3D::BufferPtr m_encodeParams;
	D3D::SamplerStatePtr m_efbSampler;

	

	std::map<ComboKey, D3D::ComputeShaderPtr> m_encoding_shaders;
	
	class ShaderCacheInserter : public LinearDiskCacheReader<ComboKey, u8>
	{
	public:
		void Read(const ComboKey& key, const u8 *value, u32 value_size)
		{
			encoder_.InsertShader(key, value, value_size);
		}
		ShaderCacheInserter(CSTextureEncoder &encoder) : encoder_(encoder)
		{}
	private:
		CSTextureEncoder& encoder_;
	};
	friend ShaderCacheInserter;

	LinearDiskCache<ComboKey, u8> m_shaderCache;

};

}
