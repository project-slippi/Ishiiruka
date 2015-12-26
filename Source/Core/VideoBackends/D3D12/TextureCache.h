// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/D3D12/D3DTexture.h"
#include "VideoCommon/TextureCacheBase.h"

namespace DX12
{

class TextureCache : public TextureCacheBase
{
public:
	TextureCache();
	~TextureCache();

private:
	struct TCacheEntry : TCacheEntryBase
	{
		D3DTexture2D *const texture;
		D3DTexture2D *nrm_texture;
		D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE srvGpuHandleCpuShadow;
		D3D12_CPU_DESCRIPTOR_HANDLE nrm_srvCpuHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE nrm_srvGpuHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE nrm_srvGpuHandleCpuShadow;
		DXGI_FORMAT DXGI_format;
		bool compressed;

		TCacheEntry(const TCacheEntryConfig& config, D3DTexture2D *_tex) : TCacheEntryBase(config), texture(_tex), nrm_texture(nullptr), compressed(false) {}
		~TCacheEntry();

		void CopyRectangleFromTexture(
			const TCacheEntryBase* source,
			const MathUtil::Rectangle<int> &srcrect,
			const MathUtil::Rectangle<int> &dstrect) override;
		void Load(const u8* src, u32 width, u32 height,
			u32 expanded_width, u32 level) override;
		void LoadMaterialMap(const u8* src, u32 width, u32 height, u32 level) override;
		void Load(const u8* src, u32 width, u32 height, u32 expandedWidth,
			u32 expandedHeight, const s32 texformat, const u32 tlutaddr, const TlutFormat tlutfmt, u32 level) override;
		void LoadFromTmem(const u8* ar_src, const u8* gb_src, u32 width, u32 height,
			u32 expanded_width, u32 expanded_Height, u32 level) override;

		void FromRenderTarget(u8* dst, PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
			bool scaleByHalf, unsigned int cbufid, const float *colmat) override;
		bool SupportsMaterialMap() const override { return nrm_texture != nullptr; };
		void Bind(unsigned int stage, unsigned int lastTexture) override;
		bool Save(const std::string& filename, unsigned int level) override;
	};

	PC_TexFormat GetNativeTextureFormat(const s32 texformat, const TlutFormat tlutfmt, u32 width, u32 height);

	TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) override;
	bool Palettize(TCacheEntryBase* entry, const TCacheEntryBase* base_entry) override;
	void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
		PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
		bool isIntensity, bool scaleByHalf) override;
	void LoadLut(u32 lutFmt, void* addr, u32 size) override;
	void CompileShaders() override { }
	void DeleteShaders() override { }

	TlutFormat m_lut_format;
	u32 m_lut_size;
	ID3D12Resource* palette_buf12;
	UINT palette_buf12index;
	void* palette_buf12data;
	D3D12_CPU_DESCRIPTOR_HANDLE palette_buf12cpu[1024];
	D3D12_GPU_DESCRIPTOR_HANDLE palette_buf12gpu[1024];
	ID3D12Resource* palette_uniform12;
	UINT palette_uniform12offset;
	void* palette_uniform12data;
	D3D12_SHADER_BYTECODE palette_pixel_shader12[3];
};

}
