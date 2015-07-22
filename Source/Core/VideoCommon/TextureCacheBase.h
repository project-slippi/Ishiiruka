// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include <map>
#include <unordered_map>

#include "Common/CommonTypes.h"
#include "Common/Thread.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoCommon.h"

struct VideoConfig;

enum TextureCacheParams
{
	// ugly
	TEXHASH_INVALID = 0,
	FRAMECOUNT_INVALID = 0,
	TEXTURE_KILL_MULTIPLIER = 2,
	TEXTURE_KILL_THRESHOLD = 120,
	TEXTURE_POOL_KILL_THRESHOLD = 3,
	HIRES_POOL_KILL_THRESHOLD = 600,
	TEXTURE_POOL_MEMORY_LIMIT = 64 * 1024 * 1024
};

class TextureCache
{
public:
	struct TCacheEntryConfig
	{
		TCacheEntryConfig() : width(0), height(0), levels(1), layers(1), rendertarget(false), pcformat(PC_TEX_FMT_NONE) {}

		u32 width, height;
		u32 levels, layers;
		bool rendertarget;
		PC_TexFormat pcformat;

		u32 GetSizeInBytes() const
		{
			u32 result = 0; 
			switch (pcformat)
			{
			case PC_TEX_FMT_BGRA32:
			case PC_TEX_FMT_RGBA32:			
				result = ((width + 3) & (~3)) * ((height + 3) & (~3)) *  4;
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
			result = std::max(result, 4096u);
			return result;
		}

		bool operator == (const TCacheEntryConfig& b) const
		{
			return width == b.width
				&& height == b.height
				&& levels == b.levels
				&& layers == b.layers
				&& rendertarget == b.rendertarget
				&& pcformat == b.pcformat;
		}

		struct Hasher
		{
			size_t operator()(const TextureCache::TCacheEntryConfig& c) const
			{
				return (u64)c.rendertarget << 56	// 1 bit
					| (u64)c.pcformat << 48			// 8 bits
					| (u64)c.layers << 40			// 8 bits 
					| (u64)c.levels << 32			// 8 bits 
					| (u64)c.height << 16			// 16 bits 
					| (u64)c.width;					// 16 bits
			}
		};
	};

	struct TCacheEntryBase
	{
		TCacheEntryConfig config;

		// common members
		u32 addr;
		u32 size_in_bytes;
		u32 native_size_in_bytes;
		u64 hash;
		u64 base_hash;
		u32 format;
		bool is_efb_copy;

		u32 native_width, native_height; // Texture dimensions from the GameCube's point of view
		u32 native_levels;
		
		// used to delete textures which haven't been used for TEXTURE_KILL_THRESHOLD frames
		s32 frameCount;
		// Keep an iterator to the entry in textures_by_hash, so it does not need to be searched when removing the cache entry
		std::multimap<u64, TCacheEntryBase*>::iterator textures_by_hash_iter;
		bool is_custom_tex;
		std::string basename;
		u32 copyMipMapStrideChannels;

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
		}

		void SetHiresParams(bool _is_custom_tex, const std::string & _basename)
		{
			is_custom_tex = _is_custom_tex;
			basename = _basename;
		}

		void SetHashes(u64 _hash, u64 _base_hash)
		{
			hash = _hash;
			base_hash = _base_hash;
		}

		TCacheEntryBase(const TCacheEntryConfig& c) : config(c), is_custom_tex(false), basename()
		{
			native_size_in_bytes = config.GetSizeInBytes();
		}
		virtual ~TCacheEntryBase();

		virtual void Bind(u32 stage) = 0;
		virtual bool Save(const std::string& filename, u32 level) = 0;

		virtual void CopyRectangleFromTexture(
			const TCacheEntryBase* source,
			const MathUtil::Rectangle<int> &srcrect,
			const MathUtil::Rectangle<int> &dstrect) = 0;

		virtual void Load(const u8* src, u32 width, u32 height,
			u32 expanded_width, u32 level) = 0;
		virtual void Load(const u8* src, u32 width, u32 height, u32 expandedWidth,
			u32 expandedHeight, const s32 texformat, const u32 tlutaddr, const TlutFormat tlutfmt, u32 level) = 0;
		virtual void LoadFromTmem(const u8* ar_src, const u8* gb_src, u32 width, u32 height,
			u32 expanded_width, u32 expanded_Height, u32 level) = 0;
		virtual void FromRenderTarget(
			PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
			bool isIntensity, bool scaleByHalf, unsigned int cbufid,
			const float *colmat) = 0;
		virtual bool PalettizeFromBase(const TCacheEntryBase* base_entry) = 0;
		bool OverlapsMemoryRange(u32 range_address, u32 range_size) const;

		bool IsEfbCopy() { return is_efb_copy; }
	};

	virtual ~TextureCache(); // needs virtual for DX11 dtor

	static void OnConfigChanged(VideoConfig& config);
	// Removes textures which aren't used for more than TEXTURE_KILL_THRESHOLD frames,
	// frameCount is the current frame number.
	static void Cleanup(int frameCount);

	static void Invalidate();
	static void MakeRangeDynamic(u32 start_address, u32 size);

	virtual PC_TexFormat GetNativeTextureFormat(const s32 texformat,
		const TlutFormat tlutfmt, u32 width, u32 height) = 0;
	virtual TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) = 0;

	virtual void CompileShaders() = 0; // currently only implemented by OGL
	virtual void DeleteShaders() = 0; // currently only implemented by OGL

	static TCacheEntryBase* Load(const u32 stage);
	static void UnbindTextures();
	static void BindTextures();
	static void CopyRenderTargetToTexture(u32 dstAddr, u32 dstFormat, PEControl::PixelFormat srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);

	static void RequestInvalidateTextureCache();

	virtual void LoadLut(u32 lutFmt, void* addr, u32 size) = 0;

protected:
	static GC_ALIGNED16(u8 *temp);
	static size_t temp_size;
	TextureCache();
private:
	typedef std::multimap<u64, TCacheEntryBase*> TexCache;
	typedef std::unordered_multimap<TCacheEntryConfig, TCacheEntryBase*, TCacheEntryConfig::Hasher> TexPool;
	typedef std::unordered_map<std::string, TCacheEntryBase*> HiresTexPool;

	static void CheckTempSize(size_t required_size);
	static TCacheEntryBase* DoPartialTextureUpdates(TexCache::iterator iter);
	static void DumpTexture(TCacheEntryBase* entry, std::string basename, u32 level);
	static void InvalidateHiresCache();
	static u32 s_prev_tlut_address;
	static u32 s_prev_tlut_size;
	static u64 s_prev_tlut_hash;
	static TCacheEntryBase* AllocateTexture(const TCacheEntryConfig& config);
	static TextureCache::TexCache::iterator FreeTexture(TexCache::iterator t_iter);
	static TCacheEntryBase* ReturnEntry(unsigned int stage, TCacheEntryBase* entry);

	

	static TexCache textures_by_address;
	static TexCache textures_by_hash;
	static TexPool texture_pool;
	static size_t texture_pool_memory_usage;
	static HiresTexPool hires_texture_pool;
	static TCacheEntryBase* bound_textures[8];

	// Backup configuration values
	static struct BackupConfig
	{
		s32 s_colorsamples;
		bool s_texfmt_overlay;
		bool s_texfmt_overlay_center;
		bool s_hires_textures;
		bool s_cache_hires_textures;
		bool s_stereo_3d;
		bool s_efb_mono_depth;
	} backup_config;
};

extern TextureCache *g_texture_cache;