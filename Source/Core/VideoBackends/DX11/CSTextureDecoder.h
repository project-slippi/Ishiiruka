// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include "Common/LinearDiskCache.h"
#include "VideoBackends/DX11/D3DPtr.h"
#include "VideoBackends/DX11/TextureCache.h"


namespace DX11
{

class CSTextureDecoder : public TextureDecoder
{

public:

	CSTextureDecoder() = default;

	void Init() override;
	void Shutdown() override;
	bool FormatSupported(u32 srcFmt);
	bool Decode(const u8* src, u32 srcsize, u32 srcFmt, u32 w, u32 h, u32 expandedw, u32 expandedh, u32 levels, D3DTexture2D& dstTexture) override;
	bool Depalettize(D3DTexture2D& dstTexture, D3DTexture2D& srcTexture, BaseType baseType, u32 width, u32 height) override;
	void LoadLut(u32 lutFmt, void* addr, u32 size) override;
private:

	bool m_ready{};
	struct PoolValue
	{
		D3D::Texture2dPtr m_rsc;
		D3D::UavPtr m_uav;
		PoolValue() = default;
		PoolValue(PoolValue && o) : m_rsc{ std::move(o.m_rsc) }, m_uav{ std::move(o.m_uav) }
		{}
	};

	using TexturePool = std::vector<PoolValue>;
	TexturePool m_pool;
#define MAX_POOL_SIZE 16
	u32 m_pool_idx;
	u32 m_Pool_size;

	//D3D::UniquePtr<ID3D11Buffer> m_encodeParams;

	// Stuff only used in static-linking mode (SM4.0-compatible)

	bool InitStaticMode();
	bool SetStaticShader(u32 srcFmt, u32 lutFmt);
	bool SetDepalettizeShader(BaseType srcFmt, u32 lutFmt);

	typedef unsigned int ComboKey; // Key for a shader combination

	ID3D11ComputeShader* InsertShader(ComboKey const &key, u8 const *data, u32 sz);

	ComboKey MakeComboKey(u32 srcFmt, u32 lutFmt)
	{
		return srcFmt | ((lutFmt & 0xF) << 16);
	}

	typedef std::map<ComboKey, D3D::ComputeShaderPtr> ComboMap;

	D3D::BufferPtr m_rawDataRsc;
	D3D::SrvPtr m_rawDataSrv;

	D3D::BufferPtr m_lutRsc;
	D3D::SrvPtr m_lutSrv;

	D3D::BufferPtr m_params;

	u32 m_lutFmt{};

	ComboMap m_staticShaders;

	class ShaderCacheInserter : public LinearDiskCacheReader<ComboKey, u8>
	{
	public:
		void Read(const ComboKey &key, const u8 *value, u32 value_size)
		{
			m_encoder.InsertShader(key, value, value_size);
		}
		ShaderCacheInserter(CSTextureDecoder &encoder) : m_encoder(encoder)
		{}
	private:
		CSTextureDecoder& m_encoder;
	};
	friend ShaderCacheInserter;

	LinearDiskCache<ComboKey, u8> m_shaderCache;
	D3D::PixelShaderPtr m_depalettize_shaders[3][2];
	ID3D11PixelShader* GetDepalettizerPShader(BaseType baseType, u32 lutfmt);
	void* m_last_addr = {};
	u32 m_last_size = {};
	u64 m_last_hash = {};
};

}
