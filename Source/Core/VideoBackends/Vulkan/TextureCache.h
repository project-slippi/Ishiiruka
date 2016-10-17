// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureScalerCommon.h"

namespace Vulkan
{
class PaletteTextureConverter;
class StateTracker;
class Texture2D;
class TextureEncoder;

class TextureCache : public TextureCacheBase
{
public:
	TextureCache();
	~TextureCache();

	bool Initialize(StateTracker* state_tracker);
private:
	struct TCacheEntry : TCacheEntryBase
	{
		TCacheEntry(const TCacheEntryConfig& config_, TextureCache* parent,
			std::unique_ptr<Texture2D> texture, VkFramebuffer framebuffer);
		~TCacheEntry();

		Texture2D* GetTexture() const { return m_texture.get(); }
		VkFramebuffer GetFramebuffer() const { return m_framebuffer; }
		void Load(const u8* src, u32 width, u32 height,
			u32 expanded_width, u32 level) override;
		void LoadMaterialMap(const u8* src, u32 width, u32 height, u32 level) override;
		void Load(const u8* src, u32 width, u32 height, u32 expandedWidth,
			u32 expandedHeight, const s32 texformat, const u32 tlutaddr, const TlutFormat tlutfmt, u32 level) override;
		void LoadFromTmem(const u8* ar_src, const u8* gb_src, u32 width, u32 height,
			u32 expanded_width, u32 expanded_Height, u32 level) override;

		void FromRenderTarget(u8* dst, PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
			bool scaleByHalf, unsigned int cbufid, const float *colmat) override;
		void CopyRectangleFromTexture(const TCacheEntryBase* source,
			const MathUtil::Rectangle<int>& src_rect,
			const MathUtil::Rectangle<int>& dst_rect) override;

		void Bind(u32 stage, u32 last_texture) override;
		bool Save(const std::string& filename, unsigned int level) override;
		bool SupportsMaterialMap() const override
		{
			return !!m_nrmtexture;
		};
		inline uintptr_t GetInternalObject() override
		{
			return reinterpret_cast<uintptr_t>(m_texture.get());
		};
		bool compressed;
	private:
		TextureCache* m_parent;
		std::unique_ptr<Texture2D> m_texture;
		std::unique_ptr<Texture2D> m_nrmtexture;

		// If we're an EFB copy, framebuffer for drawing into.
		VkFramebuffer m_framebuffer;
	};

	void LoadData(Texture2D* dst, const u8* src, u32 width, u32 height,
		u32 expanded_width, u32 level);

	TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) override;

	bool CreateRenderPasses();

	VkRenderPass GetRenderPassForTextureUpdate(const Texture2D* texture) const;

	StateTracker* m_state_tracker = nullptr;

	VkRenderPass m_initialize_render_pass = VK_NULL_HANDLE;
	VkRenderPass m_update_render_pass = VK_NULL_HANDLE;

	std::unique_ptr<StreamBuffer> m_texture_upload_buffer;

	std::unique_ptr<TextureEncoder> m_texture_encoder;

	std::unique_ptr<PaletteTextureConverter> m_palette_texture_converter;

	std::unique_ptr<TextureScaler> m_scaler;

	VkShaderModule m_copy_shader = VK_NULL_HANDLE;
	VkShaderModule m_efb_color_to_tex_shader = VK_NULL_HANDLE;
	VkShaderModule m_efb_depth_to_tex_shader = VK_NULL_HANDLE;

	void* m_pallette;
	TlutFormat m_pallette_format;
	u32 m_pallette_size;
	bool CompileShaders() override;
	void DeleteShaders() override;
	bool Palettize(TCacheEntryBase* entry, const TCacheEntryBase* base_entry) override;
	void LoadLut(u32 lutFmt, void* addr, u32 size) override;
	void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
		PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
		bool isIntensity, bool scaleByHalf) override;
	PC_TexFormat GetNativeTextureFormat(const s32 texformat, const TlutFormat tlutfmt, u32 width, u32 height) override;
};

}  // namespace Vulkan
