// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/LinearDiskCache.h"
#include "VideoBackends/DX11/D3DPtr.h"
#include "VideoBackends/DX11/TextureEncoder.h"

namespace DX11
{

	class PSTextureEncoder : public TextureEncoder
	{

	public:

		PSTextureEncoder() = default;

		void Init();
		void Shutdown();
		void Encode(u8* dst, const TextureCache::TCacheEntryBase *texture_entry,
			PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
			bool isIntensity, bool scaleByHalf) override;

	private:

		bool m_ready{};

		D3D::Texture2dPtr m_out;
		D3D::RtvPtr m_outRTV;
		D3D::Texture2dPtr m_outStage;
		D3D::BufferPtr m_encodeParams;
		D3D::BufferPtr m_quad;
		D3D::VertexShaderPtr m_vShader;
		D3D::InputLayoutPtr m_quadLayout;
		D3D::BlendStatePtr m_efbEncodeBlendState;
		D3D::DepthStencilStatePtr m_efbEncodeDepthState;
		D3D::RasterizerStatePtr m_efbEncodeRastState;
		D3D::SamplerStatePtr m_efbSampler;

		// Stuff only used in static-linking mode (SM4.0-compatible)

		bool InitStaticMode();
		bool SetStaticShader(u32 dstFormat, u32 srcFormat,
			bool isIntensity, bool scaleByHalf);

		typedef u32 ComboKey; // Key for a shader combination

		ComboKey MakeComboKey(u32 dstFormat, u32 srcFormat,
			bool isIntensity, bool scaleByHalf)
		{
			return (dstFormat << 4) | (srcFormat << 2) | (isIntensity ? (1 << 1) : 0)
				| (scaleByHalf ? (1 << 0) : 0);
		}

		typedef std::map<ComboKey, D3D::PixelShaderPtr> ComboMap;

		ComboMap m_staticShaders;

		// Stuff only used for dynamic-linking mode (SM5.0+, available as soon as
		// Microsoft fixes their bloody HLSL compiler)

		bool InitDynamicMode();
		bool SetDynamicShader(u32 dstFormat, u32 srcFormat,
			bool isIntensity, bool scaleByHalf);

		D3D::PixelShaderPtr m_dynamicShader;
		D3D::ClkPtr m_classLinkage;

		// Interface slots
		UINT m_fetchSlot;
		UINT m_scaledFetchSlot;
		UINT m_intensitySlot;
		UINT m_generatorSlot;

		// Class instances
		// Fetch: 0 is RGB, 1 is RGBA, 2 is RGB565, 3 is Z
		D3D::CiPtr m_fetchClass[4];
		// ScaledFetch: 0 is off, 1 is on
		D3D::CiPtr m_scaledFetchClass[2];
		// Intensity: 0 is off, 1 is on
		D3D::CiPtr m_intensityClass[2];
		// Generator: one for each dst format, 16 total
		D3D::CiPtr m_generatorClass[16];

		std::vector<ID3D11ClassInstance*> m_linkageArray;

	};

}