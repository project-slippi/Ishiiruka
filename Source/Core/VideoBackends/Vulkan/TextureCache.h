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
class TextureConverter;
class StateTracker;
class Texture2D;

class TextureCache : public TextureCacheBase
{
public:
	struct TCacheEntry : TCacheEntryBase
	{
		TCacheEntry(const TCacheEntryConfig& config_, std::unique_ptr<Texture2D> texture, std::unique_ptr<Texture2D> nrmtexture,
			VkFramebuffer framebuffer);
		~TCacheEntry();

		Texture2D* GetTexture() const { return m_texture.get(); }
		VkFramebuffer GetFramebuffer() const;
		void Load(const u8* src, u32 width, u32 height,
			u32 expanded_width, u32 level) override;
		void LoadMaterialMap(const u8* src, u32 width, u32 height, u32 level) override;

		void FromRenderTarget(bool is_depth_copy, const EFBRectangle& srcRect,
			bool scaleByHalf, unsigned int cbufid, const float *colmat, u32 width, u32 height) override;
		void CopyRectangleFromTexture(const TCacheEntryBase* source,
			const MathUtil::Rectangle<int>& src_rect,
			const MathUtil::Rectangle<int>& dst_rect) override;
		bool DecodeTextureOnGPU(u32 dst_level, const u8* data,
			u32 data_size, TextureFormat format, u32 width, u32 height,
			u32 aligned_width, u32 aligned_height, u32 row_stride,
			const u8* palette, TlutFormat palette_format) override;
		void Bind(u32 stage) override;
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
		std::unique_ptr<Texture2D> m_texture;
		std::unique_ptr<Texture2D> m_nrmtexture;
	};

	TextureCache();
	~TextureCache();

	void LoadData(Texture2D* dst, const u8* src, u32 width, u32 height,
		u32 expanded_width, u32 level);

	static TextureCache* GetInstance();
	TextureConverter* GetTextureConverter() const { return m_texture_converter.get(); }
	bool Initialize();

	bool CompileShaders() override;
	void DeleteShaders() override;

	TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) override;

	void CopyEFB(u8* dst, const EFBCopyFormat& format, u32 native_width, u32 bytes_per_row,
		u32 num_blocks_y, u32 memory_stride,
		bool is_depth_copy, const EFBRectangle& src_rect, bool scale_by_half) override;

	void CopyRectangleFromTexture(TCacheEntry* dst_texture, const MathUtil::Rectangle<int>& dst_rect,
		Texture2D* src_texture, const MathUtil::Rectangle<int>& src_rect);
	bool SupportsGPUTextureDecode(TextureFormat format, TlutFormat palette_format) override;
	TextureConverter* GetTextureConverter()
	{
		return m_texture_converter.get();
	}
	VkRenderPass GetRenderPass() const
	{
		return m_render_pass;
	}
	VkShaderModule GetCopyShader() const
	{
		return m_copy_shader;
	}
private:
	bool CreateRenderPasses();

	// Copies the contents of a texture using vkCmdCopyImage
	void CopyTextureRectangle(TCacheEntry* dst_texture, const MathUtil::Rectangle<int>& dst_rect,
		Texture2D* src_texture, const MathUtil::Rectangle<int>& src_rect);

	// Copies (and optionally scales) the contents of a texture using a framgent shader.
	void ScaleTextureRectangle(TCacheEntry* dst_texture, const MathUtil::Rectangle<int>& dst_rect,
		Texture2D* src_texture, const MathUtil::Rectangle<int>& src_rect);

	VkRenderPass m_render_pass = VK_NULL_HANDLE;

	std::unique_ptr<StreamBuffer> m_texture_upload_buffer;

	std::unique_ptr<TextureConverter> m_texture_converter;

	std::unique_ptr<TextureScaler> m_scaler;

	VkShaderModule m_copy_shader = VK_NULL_HANDLE;
	VkShaderModule m_efb_color_to_tex_shader = VK_NULL_HANDLE;
	VkShaderModule m_efb_depth_to_tex_shader = VK_NULL_HANDLE;

	void* m_pallette;
	TlutFormat m_pallette_format;
	u32 m_pallette_size;
	bool Palettize(TCacheEntryBase* entry, const TCacheEntryBase* base_entry) override;
	void LoadLut(u32 lutFmt, void* addr, u32 size) override;
	PC_TexFormat GetNativeTextureFormat(const s32 texformat, const TlutFormat tlutfmt, u32 width, u32 height) override;
};

}  // namespace Vulkan
