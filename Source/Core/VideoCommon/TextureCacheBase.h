// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <map>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include "Common/CommonTypes.h"
#include "Common/Thread.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoCommon.h"

struct VideoConfig;
class TextureScaler;

enum TextureCacheParams
{
	// ugly
	TEXHASH_INVALID = 0,
	FRAMECOUNT_INVALID = 0,
	TEXTURE_KILL_MULTIPLIER = 2,
	TEXTURE_KILL_THRESHOLD = 120,
	TEXTURE_POOL_KILL_THRESHOLD = 3,
	TEXTURE_POOL_MEMORY_LIMIT = 64 * 1024 * 1024
};

class TextureCacheBase
{
public:
	struct TCacheEntryConfig
	{
		constexpr TCacheEntryConfig() = default;

		u32 GetSizeInBytes() const
		{
			u32 result = 0;
			switch (pcformat)
			{
			case PC_TEX_FMT_RGBA16_FLOAT:
				result = ((width + 3) & (~3)) * ((height + 3) & (~3)) * 8;
				break;
			case PC_TEX_FMT_RGBA_FLOAT:
				result = ((width + 3) & (~3)) * ((height + 3) & (~3)) * 16;
				break;
				break;
			case PC_TEX_FMT_DEPTH_FLOAT:
			case PC_TEX_FMT_R_FLOAT:
			case PC_TEX_FMT_BGRA32:
			case PC_TEX_FMT_RGBA32:
				result = ((width + 3) & (~3)) * ((height + 3) & (~3)) * 4;
				break;
			case PC_TEX_FMT_IA4_AS_IA8:
			case PC_TEX_FMT_IA8:
			case PC_TEX_FMT_RGB565:
				result = ((width + 3) & (~3)) * ((height + 3) & (~3)) * 2;
				break;
			case PC_TEX_FMT_DXT1:
				result = ((width + 3) >> 2)*((height + 3) >> 2) * 8;
				break;
			case PC_TEX_FMT_DXT3:
			case PC_TEX_FMT_DXT5:
				result = ((width + 3) >> 2)*((height + 3) >> 2) * 16;
				break;
			default:
				break;
			}
			if (levels > 1 || rendertarget)
			{
				result += result * 2;
			}
			if (materialmap)
			{
				result *= 2;
			}
			result = std::max(result, 4096u);
			return result;
		}

		bool operator == (const TCacheEntryConfig& o) const
		{
			return std::tie(width, height, levels, layers, rendertarget, pcformat, materialmap) ==
				std::tie(o.width, o.height, o.levels, o.layers, o.rendertarget, o.pcformat, o.materialmap);
		}

		struct Hasher
		{
			size_t operator()(const TCacheEntryConfig& c) const
			{
				return (u64)c.materialmap << 57	// 1 bit
					| (u64)c.rendertarget << 56	// 1 bit
					| (u64)c.pcformat << 48		// 8 bits
					| (u64)c.layers << 40		// 8 bits 
					| (u64)c.levels << 32		// 8 bits 
					| (u64)c.height << 16		// 16 bits 
					| (u64)c.width;				// 16 bits
			}
		};

		u32 width = 0, height = 0, levels = 1, layers = 1;
		bool rendertarget = false;
		bool materialmap = false;
		PC_TexFormat pcformat = PC_TEX_FMT_NONE;
	};

	struct TCacheEntryBase
	{
		TCacheEntryConfig config;

		// common members
		bool is_efb_copy = false;
		bool is_custom_tex = false;
		bool is_scaled = false;
		bool emissive_in_alpha = false;
		bool may_have_overlapping_textures = false;
		u32 addr = {};
		u32 size_in_bytes = {};
		u32 native_size_in_bytes = {};
		u32 format = {}; // bits 0-3 will contain the in-memory format.
		u32 memory_stride = {};
		u32 native_width = {}, native_height = {}; // Texture dimensions from the GameCube's point of view
		u32 native_levels = {};
		// used to delete textures which haven't been used for TEXTURE_KILL_THRESHOLD frames
		s32 frameCount = {};
		u64 hash = {};
		u64 base_hash = {};

		// Keep an iterator to the entry in textures_by_hash, so it does not need to be searched when removing the cache entry
		std::multimap<u64, TCacheEntryBase*>::iterator textures_by_hash_iter;

		// This is used to keep track of both:
		//   * efb copies used by this partially updated texture
		//   * partially updated textures which refer to this efb copy
		std::unordered_set<TCacheEntryBase*> references;

		std::string basename;

		void SetGeneralParameters(u32 _addr, u32 _size, u32 _format)
		{
			addr = _addr;
			size_in_bytes = _size;
			format = _format;
		}

		void SetDimensions(u32 _native_width, u32 _native_height, u32 _native_levels)
		{
			native_width = _native_width;
			native_height = _native_height;
			native_levels = _native_levels;
			memory_stride = _native_width;
		}

		void SetHiresParams(bool _is_custom_tex, const std::string & _basename, bool _is_scaled, bool _emissive_in_alpha)
		{
			is_custom_tex = _is_custom_tex;
			basename = _basename;
			is_scaled = _is_scaled;
			emissive_in_alpha = _emissive_in_alpha;
		}

		void SetHashes(u64 _hash, u64 _base_hash)
		{
			hash = _hash;
			base_hash = _base_hash;
		}

		// This texture entry is used by the other entry as a sub-texture
		void CreateReference(TCacheEntryBase* other_entry)
		{
			// References are two-way, so they can easily be destroyed later
			this->references.emplace(other_entry);
			other_entry->references.emplace(this);
		}

		void DestroyAllReferences()
		{
			for (auto& reference : references)
				reference->references.erase(this);

			references.clear();
		}

		void SetEfbCopy(u32 stride);

		TCacheEntryBase(const TCacheEntryConfig& c) : config(c)
		{
			native_size_in_bytes = config.GetSizeInBytes();
		}

		virtual ~TCacheEntryBase();
		virtual uintptr_t GetInternalObject() = 0;
		virtual void Bind(u32 stage) = 0;
		virtual bool Save(const std::string& filename, u32 level) = 0;

		virtual void CopyRectangleFromTexture(
			const TCacheEntryBase* source,
			const MathUtil::Rectangle<int> &srcrect,
			const MathUtil::Rectangle<int> &dstrect) = 0;

		virtual void Load(const u8* src, u32 width, u32 height,
			u32 expanded_width, u32 level) = 0;
		virtual void LoadMaterialMap(const u8* src, u32 width, u32 height, u32 level)
		{}
		virtual void FromRenderTarget(bool is_depth_copy, const EFBRectangle& srcRect,
			bool scaleByHalf, u32 cbufid, const float *colmat, u32 width, u32 height) = 0;
		bool OverlapsMemoryRange(u32 range_address, u32 range_size) const;
		virtual bool SupportsMaterialMap() const = 0;
		// Decodes the specified data to the GPU texture specified by entry.
		// width, height are the size of the image in pixels.
		// aligned_width, aligned_height are the size of the image in pixels, aligned to the block size.
		// row_stride is the number of bytes for a row of blocks, not pixels.
		virtual bool DecodeTextureOnGPU(u32 dst_level, const u8* data,
			u32 data_size, TextureFormat format, u32 width, u32 height,
			u32 aligned_width, u32 aligned_height, u32 row_stride,
			const u8* palette, TlutFormat palette_format)
		{
			return false;
		}
		

		bool IsEfbCopy() const
		{
			return is_efb_copy;
		}

		u32 NumBlocksY() const;
		u32 BytesPerRow() const;

		u64 CalculateHash() const;
	};

	virtual ~TextureCacheBase(); // needs virtual for DX11 dtor

	void OnConfigChanged(VideoConfig& config);
	// Removes textures which aren't used for more than TEXTURE_KILL_THRESHOLD frames,
	// frameCount is the current frame number.
	void Cleanup(int frameCount);
	void Invalidate();

	virtual PC_TexFormat GetNativeTextureFormat(const s32 texformat,
		const TlutFormat tlutfmt, u32 width, u32 height) = 0;
	TCacheEntryBase* AllocateTexture(const TCacheEntryConfig& config);
	void DisposeTexture(TCacheEntryBase* texture);

	
	virtual bool Palettize(TCacheEntryBase* entry, const TCacheEntryBase* base_entry) = 0;
	virtual void CopyEFB(u8* dst, const EFBCopyFormat& format, u32 native_width, u32 bytes_per_row,
		u32 num_blocks_y, u32 memory_stride,
		bool is_depth_copy, const EFBRectangle& src_rect, bool scale_by_half) = 0;

	virtual bool CompileShaders() = 0; // currently only implemented by OGL
	virtual void DeleteShaders() = 0; // currently only implemented by OGL

	virtual void LoadLut(u32 lutFmt, void* addr, u32 size) = 0;

	TCacheEntryBase* Load(const u32 stage);
	void UnbindTextures();
	virtual void BindTextures();
	void CopyRenderTargetToTexture(u32 dstAddr, u32 dstFormat, u32 dstStride,
		bool is_depth_copy, const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);
	u8* GetTemporalBuffer()
	{
		return temp;
	}
	// Returns true if the texture data and palette formats are supported by the GPU decoder.
	virtual bool SupportsGPUTextureDecode(TextureFormat format, TlutFormat palette_format)
	{
		return false;
	}
protected:
	alignas(16) u8 *temp = {};
	size_t temp_size = {};
	std::array<TCacheEntryBase*, 8> bound_textures{};
	TextureCacheBase();
	virtual TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) = 0;
private:
	typedef std::multimap<u32, TCacheEntryBase*> TexAddrCache;
	typedef std::multimap<u64, TCacheEntryBase*> TexHashCache;
	typedef std::unordered_multimap<TCacheEntryConfig, TCacheEntryBase*, TCacheEntryConfig::Hasher> TexPool;
	typedef std::unordered_map<std::string, TCacheEntryBase*> HiresTexPool;

	void SetBackupConfig(const VideoConfig& config);
	void ScaleTextureCacheEntryTo(TCacheEntryBase** entry, u32 new_width, u32 new_height);
	void CheckTempSize(size_t required_size);
	TCacheEntryBase* DoPartialTextureUpdates(TCacheEntryBase* entry_to_update, u32 tlutaddr, u32 tlutfmt, u32 palette_size);
	TextureCacheBase::TCacheEntryBase* ApplyPaletteToEntry(TCacheEntryBase* entry, u32 tlutaddr, u32 tlutfmt, u32 palette_size);
	void DumpTexture(TCacheEntryBase* entry, std::string basename, u32 level);

	TexPool::iterator FindMatchingTextureFromPool(const TCacheEntryConfig& config);
	TexAddrCache::iterator GetTexCacheIter(TCacheEntryBase* entry);
	TexAddrCache::iterator InvalidateTexture(TexAddrCache::iterator t_iter);
	TCacheEntryBase* ReturnEntry(u32 stage, TCacheEntryBase* entry);

	// Return all possible overlapping textures. As addr+size of the textures is not
	// indexed, this may return false positives.
	std::pair<TexAddrCache::iterator, TexAddrCache::iterator>
		FindOverlappingTextures(u32 addr, u32 size_in_bytes);

	TexAddrCache textures_by_address;
	TexHashCache textures_by_hash;
	TexPool texture_pool;
	size_t texture_pool_memory_usage = {};
	
	u32 s_last_texture = {};

	// Backup configuration values
	struct BackupConfig
	{
		s32 colorsamples;
		bool texfmt_overlay;
		bool texfmt_overlay_center;
		bool hires_textures;
		bool cache_hires_textures;
		bool stereo_3d;
		bool efb_mono_depth;
		s32 scaling_mode;
		s32 scaling_factor;
		bool scaling_deposterize;
		bool gpu_texture_decoding;
	};
	BackupConfig backup_config = {};
	std::unique_ptr<TextureScaler> m_scaler;
};

extern std::unique_ptr<TextureCacheBase> g_texture_cache;