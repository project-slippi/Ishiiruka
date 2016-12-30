// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

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
	void Encode(u8* dest_ptr, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
		bool is_depth_copy, bool bIsIntensityFmt, bool bScaleByHalf, const EFBRectangle& source) override;
private:

	bool m_ready{};

	D3D::BufferPtr m_out;
	D3D::BufferPtr m_outStage;
	D3D::UavPtr m_outUav;

	D3D::BufferPtr m_encodeParams;
	D3D::SamplerStatePtr m_efbSampler;

	// Stuff only used in static-linking mode (SM4.0-compatible)

	bool InitStaticMode();
	bool SetStaticShader(u32 dstFormat,
		bool is_depth_copy, bool isIntensity, bool scaleByHalf);

	typedef u32 ComboKey; // Key for a shader combination

	ID3D11ComputeShader* InsertShader(ComboKey const &key, u8 const *data, u32 sz);

	ComboKey MakeComboKey(u32 dstFormat,
		bool is_depth_copy, bool isIntensity, bool scaleByHalf, bool model5)
	{
		return (model5 ? (1 << 24) : 0) | (dstFormat << 4) | (is_depth_copy << 2) | (isIntensity ? (1 << 1) : 0)
			| (scaleByHalf ? (1 << 0) : 0);
	}

	typedef std::map<ComboKey, D3D::ComputeShaderPtr> ComboMap;

	ComboMap m_staticShaders;

	class ShaderCacheInserter : public LinearDiskCacheReader<ComboKey, u8>
	{
	public:
		void Read(const ComboKey &key, const u8 *value, u32 value_size)
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

	// Stuff only used for dynamic-linking mode (SM5.0+, available as soon as
	// Microsoft fixes their bloody HLSL compiler)

	bool InitDynamicMode();
	bool SetDynamicShader(u32 dstFormat,
		bool is_depth_copy, bool isIntensity, bool scaleByHalf);

	D3D::ComputeShaderPtr m_dynamicShader;
	D3D::ClkPtr m_classLinkage;

	// Interface slots
	UINT m_fetchSlot;
	UINT m_scaledFetchSlot;
	UINT m_intensitySlot;
	UINT m_generatorSlot;

	// Class instances
	// Fetch: 0 is RGB, 1 is Z
	D3D::CiPtr m_fetchClass[2];
	// ScaledFetch: 0 is off, 1 is on
	D3D::CiPtr m_scaledFetchClass[2];
	// Intensity: 0 is off, 1 is on
	D3D::CiPtr m_intensityClass[2];
	// Generator: one for each dst format, 16 total
	D3D::CiPtr m_generatorClass[16];

	std::vector<ID3D11ClassInstance*> m_linkageArray;

};

}
