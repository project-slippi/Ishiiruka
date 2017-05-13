// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include <memory>
#include "VideoBackends/D3D12/D3DTexture.h"
#include "VideoCommon/TextureCacheBase.h"

namespace DX12
{

class D3DStreamBuffer;

class TextureCache : public TextureCacheBase
{
public:
	TextureCache();
	~TextureCache();
	static D3D12_GPU_DESCRIPTOR_HANDLE GetTextureGroupHandle();
	virtual void BindTextures();
private:
	struct TCacheEntry : TCacheEntryBase
	{
		D3DTexture2D* m_texture;
		D3DTexture2D* m_nrm_texture;
		DXGI_FORMAT DXGI_format;
		bool compressed;

		TCacheEntry(const TCacheEntryConfig& config, D3DTexture2D *_tex) : TCacheEntryBase(config), m_texture(_tex), m_nrm_texture(nullptr), compressed(false)
		{}
		~TCacheEntry();
		void CopyRectangleFromTexture(
			const TCacheEntryBase* source,
			const MathUtil::Rectangle<int> &src_rect,
			const MathUtil::Rectangle<int> &dst_rect) override;
		void Load(const u8* src, u32 width, u32 height,
			u32 expanded_width, u32 level) override;
		void LoadMaterialMap(const u8* src, u32 width, u32 height, u32 level) override;
		void FromRenderTarget(bool is_depth_copy, const EFBRectangle& src_rect,
			bool scale_by_half, u32 cbuf_id, const float* colmat, u32 width, u32 height) override;
		bool SupportsMaterialMap() const override
		{
			return m_nrm_texture != nullptr;
		};
		void Bind(u32 stage) override;
		bool Save(const std::string& filename, u32 level) override;
		inline uintptr_t GetInternalObject() override
		{
			return reinterpret_cast<uintptr_t>(m_texture);
		}
	};

	PC_TexFormat GetNativeTextureFormat(const s32 texformat, const TlutFormat tlutfmt, u32 width, u32 height) override;

	TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) override;
	bool Palettize(TCacheEntryBase* entry, const TCacheEntryBase* base_entry) override;
	void CopyEFB(u8* dst, const EFBCopyFormat& format, u32 native_width, u32 bytes_per_row,
		u32 num_blocks_y, u32 memory_stride, bool is_depth_copy,
		const EFBRectangle& src_rect, bool scale_by_half) override;
	void LoadLut(u32 lutFmt, void* addr, u32 size) override;
	bool CompileShaders() override
	{
		return true;
	}
	void DeleteShaders() override
	{}

	TlutFormat m_lut_format = {};
	u32 m_lut_size = {};
	void* m_addr = {};
	u64 m_hash = {};
	std::unique_ptr<D3DStreamBuffer> m_palette_stream_buffer;

	ID3D12Resource* m_palette_uniform_buffer = nullptr;
	D3D12_SHADER_BYTECODE m_palette_pixel_shaders[3] = {};
};

}
