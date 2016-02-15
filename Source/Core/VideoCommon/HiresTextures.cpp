// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <xxhash.h>

#include <SOIL/SOIL.h>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/Flag.h"
#include "Common/MemoryUtil.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Timer.h"
#include "Common/Logging/Log.h"

#include "Core/ConfigManager.h"

#include "VideoCommon/ImageLoader.h"
#include "VideoCommon/HiresTextures.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/TextureUtil.h"
#include "VideoCommon/VideoConfig.h"

struct hires_mip_level
{
	bool is_compressed;
	std::string path;
	std::string extension;
	hires_mip_level() : is_compressed(false), path(), extension() { }
	hires_mip_level(const std::string &p, const std::string &e, bool compressed) : is_compressed(compressed), path(p), extension(e) { }
};
struct HiresTextureCacheItem{
	std::vector<hires_mip_level> color_map;
	std::vector<hires_mip_level> normal_map;
	HiresTextureCacheItem(size_t minsize) : color_map(minsize), normal_map(){}
};

typedef std::unordered_map<std::string, HiresTextureCacheItem> HiresTextureCache;
static HiresTextureCache s_textureMap;

static std::unordered_map<std::string, std::shared_ptr<HiresTexture>> s_textureCache;
static std::mutex s_textureCacheMutex;
static Common::Flag s_textureCacheAbortLoading;

static bool s_check_native_format;
static bool s_check_new_format;
static std::atomic<size_t> size_sum;
static size_t max_mem = 0;
static std::thread s_prefetcher;

static const std::string s_format_prefix = "tex1_";
HiresTexture::HiresTexture() : 
	m_format(PC_TEX_FMT_NONE),
	m_height(0),
	m_levels(0),
	m_nrm_levels(0),
	m_cached_data(nullptr),
	m_cached_data_size(0) 
{
}

void HiresTexture::Init()
{
	size_sum.store(0);
	size_t sys_mem = MemPhysical();
	size_t recommended_min_mem = 2 * size_t(1024 * 1024 * 1024);
	// keep 2GB memory for system stability if system RAM is 4GB+ - use half of memory in other cases
	max_mem = (sys_mem / 2 < recommended_min_mem) ? (sys_mem / 2) : (sys_mem - recommended_min_mem);
	Update();
}

void HiresTexture::Shutdown()
{
	if (s_prefetcher.joinable())
	{
		s_textureCacheAbortLoading.Set();
		s_prefetcher.join();
	}

	s_textureMap.clear();
	s_textureCache.clear();
}

std::string HiresTexture::GetTextureDirectory(const std::string& game_id)
{
	const std::string texture_directory = File::GetUserPath(D_HIRESTEXTURES_IDX) + game_id;

	// If there's no directory with the region-specific ID, look for a 3-character region-free one
	if (!File::Exists(texture_directory))
		return File::GetUserPath(D_HIRESTEXTURES_IDX) + game_id.substr(0, 3);

	return texture_directory;
}

void HiresTexture::Update()
{
	s_check_native_format = false;
	s_check_new_format = false;

	if (s_prefetcher.joinable())
	{
		s_textureCacheAbortLoading.Set();
		s_prefetcher.join();
	}

	if (!g_ActiveConfig.bHiresTextures)
	{
		s_textureMap.clear();
		s_textureCache.clear();
		size_sum.store(0);
		return;
	}

	if (!g_ActiveConfig.bCacheHiresTextures)
	{
		s_textureCache.clear();
		size_sum.store(0);
	}
	
	s_textureMap.clear();
	const std::string& game_id = SConfig::GetInstance().m_strUniqueID;
	const std::string texture_directory = GetTextureDirectory(game_id);

	std::string ddscode(".dds");
	std::string cddscode(".DDS");
	std::vector<std::string> Extensions = {
		".png",
		".dds"
	};

	std::vector<std::string> filenames = DoFileSearch(Extensions, { texture_directory }, /*recursive*/ true);

	const std::string code = game_id + "_";
	const std::string miptag = "mip";
	const std::string normaltag = ".nrm";
	for (u32 i = 0; i < filenames.size(); i++)
	{
		std::string FileName;
		std::string Extension;
		SplitPath(filenames[i], nullptr, &FileName, &Extension);
		if (FileName.substr(0, code.length()) == code)
		{
			s_check_native_format = true;
		}
		else if (FileName.substr(0, s_format_prefix.length()) == s_format_prefix)
		{
			s_check_new_format = true;
		}
		else
		{
			// Discard wrong files
			continue;
		}
		const bool is_compressed = Extension.compare(ddscode) == 0 || Extension.compare(cddscode) == 0;
		const bool is_normal_map = EndsWith(FileName, normaltag);
		if (is_normal_map)
		{
			FileName = FileName.substr(0, FileName.size() - normaltag.size());
		}
		hires_mip_level mip_level_detail(filenames[i], Extension, is_compressed);
		u32 level = 0;
		size_t idx = FileName.find_last_of('_');
		std::string miplevel = FileName.substr(idx + 1, std::string::npos);
		if (miplevel.substr(0, miptag.length()) == miptag)
		{
			sscanf(miplevel.substr(3, std::string::npos).c_str(), "%i", &level);
			FileName = FileName.substr(0, idx);
		}
		HiresTextureCache::iterator iter = s_textureMap.find(FileName);
		u32 min_item_size = level + 1;
		if (iter == s_textureMap.end())
		{
			HiresTextureCacheItem item(min_item_size);
			if (is_normal_map)
			{
				item.normal_map.resize(min_item_size);
			}
			std::vector<hires_mip_level> &dst = is_normal_map ? item.normal_map : item.color_map;
			dst[level] = mip_level_detail;
			s_textureMap.emplace(FileName, item);
		}
		else
		{
			std::vector<hires_mip_level> &dst = is_normal_map ? iter->second.normal_map : iter->second.color_map;
			if (dst.size() < min_item_size)
			{
				dst.resize(min_item_size);
			}
			dst[level] = mip_level_detail;
		}
	}

	if (g_ActiveConfig.bCacheHiresTextures && s_textureMap.size() > 0)
	{
		// remove cached but deleted textures
		auto iter = s_textureCache.begin();
		while (iter != s_textureCache.end())
		{
			if (s_textureMap.find(iter->first) == s_textureMap.end())
			{
				size_sum.fetch_sub(iter->second->m_cached_data_size);
				iter = s_textureCache.erase(iter);
			}
			else
			{
				iter++;
			}
		}
		s_textureCacheAbortLoading.Clear();
		s_prefetcher = std::thread(Prefetch);
	}
}

void HiresTexture::Prefetch()
{
	Common::SetCurrentThreadName("Prefetcher");

	u32 starttime = Common::Timer::GetTimeMs();
	for (const auto& entry : s_textureMap)
	{
		const std::string& base_filename = entry.first;

		std::unique_lock<std::mutex> lk(s_textureCacheMutex);

		auto iter = s_textureCache.find(base_filename);
		if (iter == s_textureCache.end())
		{
			lk.unlock();
			HiresTexture* ptr = Load(base_filename, [](size_t requested_size)
			{
				return new u8[requested_size];
			}, true);
			lk.lock();
			if (ptr != nullptr)
			{
				size_sum.fetch_add(ptr->m_cached_data_size);
				iter = s_textureCache.insert(iter, std::make_pair(base_filename, std::shared_ptr<HiresTexture>(ptr)));
			}
		}

		if (s_textureCacheAbortLoading.IsSet())
		{
			return;
		}

		if (size_sum.load() > max_mem)
		{
			g_Config.bCacheHiresTextures = false;

			OSD::AddMessage(StringFromFormat("Custom Textures prefetching after %.1f MB aborted, not enough RAM available", size_sum / (1024.0 * 1024.0)), 10000);
			return;
		}
	}
	u32 stoptime = Common::Timer::GetTimeMs();
	OSD::AddMessage(StringFromFormat("Custom Textures loaded, %.1f MB in %.1f s", size_sum / (1024.0 * 1024.0), (stoptime - starttime) / 1000.0), 10000);
}

std::string HiresTexture::GenBaseName(
	const u8* texture, size_t texture_size,
	const u8* tlut, size_t tlut_size,
	u32 width, u32 height,
	int format,
	bool has_mipmaps,
	bool dump)
{
	std::string name = "";
	bool convert = false;
	HiresTextureCache::iterator convert_iter;
	if ((!dump || convert) && s_check_native_format)
	{
		// try to load the old format first
		u64 tex_hash = GetHashHiresTexture(texture, (int)texture_size, g_ActiveConfig.iSafeTextureCache_ColorSamples);
		u64 tlut_hash = 0;
		if(tlut_size)
			tlut_hash = GetHashHiresTexture(tlut, (int)tlut_size, g_ActiveConfig.iSafeTextureCache_ColorSamples);
		name = StringFromFormat("%s_%08x_%i", SConfig::GetInstance().m_strUniqueID.c_str(), (u32)(tex_hash ^ tlut_hash), (u16)format);
		convert_iter = s_textureMap.find(name);
		if (convert_iter != s_textureMap.end())
		{
			if (g_ActiveConfig.bConvertHiresTextures)
				convert = true;
			else
				return name;
		}
	}
	if (dump || s_check_new_format || convert)
	{
		// checking for min/max on paletted textures
		u32 min = 0xffff;
		u32 max = 0;
		switch (tlut_size)
		{
		case 0: break;
		case 16 * 2:
			for (size_t i = 0; i < texture_size; i++)
			{
				min = std::min<u32>(min, texture[i] & 0xf);
				min = std::min<u32>(min, texture[i] >> 4);
				max = std::max<u32>(max, texture[i] & 0xf);
				max = std::max<u32>(max, texture[i] >> 4);
			}
			break;
		case 256 * 2:
			for (size_t i = 0; i < texture_size; i++)
			{
				min = std::min<u32>(min, texture[i]);
				max = std::max<u32>(max, texture[i]);
			}
			break;
		case 16384 * 2:
			for (size_t i = 0; i < texture_size / 2; i++)
			{
				min = std::min<u32>(min, Common::swap16(((u16*)texture)[i]) & 0x3fff);
				max = std::max<u32>(max, Common::swap16(((u16*)texture)[i]) & 0x3fff);
			}
			break;
		}
		if (tlut_size > 0)
		{
			tlut_size = 2 * (max + 1 - min);
			tlut += 2 * min;
		}
		u64 tex_hash = XXH64(texture, texture_size);
		u64 tlut_hash = 0;
		if(tlut_size)
			tlut_hash = XXH64(tlut, tlut_size);
		std::string basename = s_format_prefix + StringFromFormat("%dx%d%s_%016" PRIx64, width, height, has_mipmaps ? "_m" : "", tex_hash);
		std::string tlutname = tlut_size ? StringFromFormat("_%016" PRIx64, tlut_hash) : "";
		std::string formatname = StringFromFormat("_%d", format);
		std::string fullname = basename + tlutname + formatname;
		if (convert)
		{
			// new texture
			if (s_textureMap.find(fullname) == s_textureMap.end())
			{
				HiresTextureCacheItem newitem(convert_iter->second.color_map.size());
				for (size_t level = 0; level < convert_iter->second.color_map.size(); level++)
				{
					std::string newname = fullname;
					if (level)
						newname += StringFromFormat("_mip%d", (int)level);
					newname += convert_iter->second.color_map[level].extension;
					std::string &src = convert_iter->second.color_map[level].path;
					size_t postfix = src.find(name);
					std::string dst = src.substr(0, postfix) + newname;
					if (File::Rename(src, dst))
					{
						s_check_new_format = true;
						OSD::AddMessage(StringFromFormat("Rename custom texture %s to %s", src.c_str(), dst.c_str()), 5000);
					}
					else
					{
						ERROR_LOG(VIDEO, "rename failed");
					}
					newitem.color_map[level] = hires_mip_level(dst, convert_iter->second.color_map[level].extension, convert_iter->second.color_map[level].is_compressed);
				}
				s_textureMap.emplace(fullname, newitem);
			}
			else
			{
				for (size_t level = 0; level < convert_iter->second.color_map.size(); level++)
				{
					if (File::Delete(convert_iter->second.color_map[level].path))
					{
						OSD::AddMessage(StringFromFormat("Delete double old custom texture %s", convert_iter->second.color_map[level].path.c_str()), 5000);
					}
					else
					{
						ERROR_LOG(VIDEO, "delete failed");
					}
				}				
			}
			s_textureMap.erase(name);
		}
		return fullname;
	}
	return name;
}

inline void ReadPNG(ImageLoaderParams &ImgInfo)
{
	// libpng path seems to fail with some png files using soil meanwhile
	/*if (ImageLoader::ReadPNG(ImgInfo))
	{
		ImgInfo.resultTex = PC_TEX_FMT_RGBA32;
	}
	*/
	File::IOFile file(ImgInfo.Path, "rb");
	std::vector<u8> buffer(file.GetSize());
	ImgInfo.resultTex = PC_TEX_FMT_NONE;
	if (!file.IsOpen() || !file.ReadBytes(buffer.data(), file.GetSize()))
	{	
		return;
	}
	int image_width;
	int image_height;
	int image_channels;
	u8* decoded = SOIL_load_image_from_memory(buffer.data(), (int)buffer.size(), &image_width, &image_height, &image_channels, SOIL_LOAD_RGBA);
	if (decoded == nullptr)
	{
		return;
	}

	// Reallocate the memory so we can manage it
	ImgInfo.Width = image_width;
	ImgInfo.Height = image_height;
	ImgInfo.data_size = image_width * image_height * 4;
	ImgInfo.dst = ImgInfo.request_buffer_delegate(ImgInfo.data_size, false);
	if (ImgInfo.dst)
	{
		memcpy(ImgInfo.dst, decoded, ImgInfo.data_size);
		ImgInfo.resultTex = PC_TEX_FMT_RGBA32;
	}
	SOIL_free_image_data(decoded);
}

inline void ReadDDS(ImageLoaderParams &ImgInfo)
{
	DDSCompression ddsc = ImageLoader::ReadDDS(ImgInfo);
	if (ddsc != DDSCompression::DDSC_NONE)
	{
		switch (ddsc)
		{
		case DDSC_DXT1:
			ImgInfo.resultTex = PC_TEX_FMT_DXT1;
			break;
		case DDSC_DXT3:
			ImgInfo.resultTex = PC_TEX_FMT_DXT3;
			break;
		case DDSC_DXT5:
			ImgInfo.resultTex = PC_TEX_FMT_DXT5;
			break;
		default:
			break;
		}
	}
}

std::shared_ptr<HiresTexture> HiresTexture::Search(
	const std::string& basename,
	std::function<u8*(size_t)> request_buffer_delegate)
{
	if (g_ActiveConfig.bCacheHiresTextures && size_sum.load() < max_mem)
	{
		std::unique_lock<std::mutex> lk(s_textureCacheMutex);

		auto iter = s_textureCache.find(basename);
		if (iter != s_textureCache.end())
		{
			HiresTexture* current = iter->second.get();
			u8* dst = request_buffer_delegate(current->m_cached_data_size);
			memcpy(dst, current->m_cached_data.get(), current->m_cached_data_size);
			return iter->second;
		}
		lk.unlock();
		std::shared_ptr<HiresTexture> ptr(Load(basename, [](size_t requested_size)
		{
			return new u8[requested_size];
		}, true));
		lk.lock();
		if (ptr)
		{
			s_textureCache[basename] = ptr;
			HiresTexture* current = ptr.get();
			size_sum.fetch_add(current->m_cached_data_size);
			u8* dst = request_buffer_delegate(current->m_cached_data_size);
			memcpy(dst, current->m_cached_data.get(), current->m_cached_data_size);
		}
		return ptr;
	}
	else
	{
		return std::shared_ptr<HiresTexture> (Load(basename, request_buffer_delegate, false));
	}
}

HiresTexture* HiresTexture::Load(const std::string& basename,
	std::function<u8*(size_t)> request_buffer_delegate, bool cacheresult)
{
	if (s_textureMap.size() == 0)
	{
		return nullptr;
	}	
	HiresTextureCache::iterator iter = s_textureMap.find(basename);
	if (iter == s_textureMap.end())
	{
		return nullptr;
	}
	HiresTextureCacheItem& current = iter->second;
	if (current.color_map.size() == 0)
	{
		return nullptr;
	}
	// First level is mandatory
	if (current.color_map[0].path.size() == 0)
	{
		return nullptr;
	}
	HiresTexture* ret = nullptr;
	u8* buffer_pointer;
	u32 maxwidth = 0;
	u32 maxheight = 0;
	bool last_level_is_dds = false;
	bool allocated_data = false;
	bool mipmapsize_included = false;
	bool nrm_posible = current.normal_map.size() == current.color_map.size() && g_ActiveConfig.HiresMaterialMapsEnabled();
	size_t remaining_buffer_size = 0;
	for (size_t level = 0; level < current.color_map.size(); level++)
	{
		ImageLoaderParams imgInfo;
		hires_mip_level &item = current.color_map[level];
		imgInfo.dst = nullptr;
		imgInfo.Path = item.path.c_str();
		if (nrm_posible)
		{
			nrm_posible = current.normal_map[level].path.size() > 0;
		}
		if (level == 0)
		{
			imgInfo.request_buffer_delegate = [&](size_t requiredsize, bool mipmapsincluded)
			{
				// Allocate double side buffer if we are going to load normal maps
				allocated_data = true;
				mipmapsize_included = mipmapsincluded;
				// Pre allocate space for the textures and potetially all the posible mip levels 
				if (current.color_map.size() > 1 && !mipmapsincluded)
				{
					requiredsize = (requiredsize * 4) / 3;
				}
				requiredsize *= (nrm_posible ? 2 : 1);
				remaining_buffer_size = requiredsize;
				return request_buffer_delegate(requiredsize);
			};
		}
		else
		{
			imgInfo.request_buffer_delegate = [&](size_t requiredsize, bool mipmapsincluded)
			{
				// is required size is biguer that the remaining size on the packed buffer just reject
				if (requiredsize > remaining_buffer_size)
				{
					return static_cast<u8*>(nullptr);
				}
				// just return the pointer to pack the textures in a single buffer.
				return buffer_pointer;
			};
		}
		bool ddsfile = false;
		if (item.is_compressed)
		{
			if (level > 0 && !last_level_is_dds)
			{
				// don't give support to mixed formats
				break;
			}
			ddsfile = true;
			last_level_is_dds = true;
			ReadDDS(imgInfo);
		}
		else
		{
			if (level > 0 && last_level_is_dds)
			{
				// don't give support to mixed formats
				break;
			}
			last_level_is_dds = false;
			ReadPNG(imgInfo);
		}
		if (imgInfo.dst == nullptr || imgInfo.resultTex == PC_TEX_FMT_NONE)
		{
			if (allocated_data && cacheresult && imgInfo.dst != nullptr)
			{
				delete [] imgInfo.dst;
			}
			ERROR_LOG(VIDEO, "Custom texture %s failed to load level %zu", imgInfo.Path, level);
			break;
		}
		if (level == 0)
		{
			ret = new HiresTexture();
			ret->m_format = imgInfo.resultTex;
			ret->m_width = maxwidth = imgInfo.Width;
			ret->m_height = maxheight = imgInfo.Height;
			if (cacheresult)
			{
				ret->m_cached_data.reset(imgInfo.dst);
				if (current.color_map.size() > 1 && !mipmapsize_included)
				{
					ret->m_cached_data_size = ((imgInfo.data_size * 4) / 3) * (nrm_posible ? 2 : 1);
				}
				else
				{
					ret->m_cached_data_size = imgInfo.data_size;
				}
			}
		}
		else
		{
			if (maxwidth != imgInfo.Width
				|| maxheight != imgInfo.Height
				|| ret->m_format != imgInfo.resultTex)
			{
				ERROR_LOG(VIDEO, 
					"Custom texture %s invalid level %zu size: %u %u required: %u %u format: %u", 
					imgInfo.Path, 
					level, 
					imgInfo.Width, 
					imgInfo.Height, 
					maxwidth, 
					maxheight,
					ret->m_format);
				break;
			}
		}
		ret->m_levels++;
		if (ddsfile)
		{
			u32 requiredsize = TextureUtil::GetTextureSizeInBytes(maxwidth, maxheight, imgInfo.resultTex);
			buffer_pointer = imgInfo.dst + requiredsize;
			remaining_buffer_size -= requiredsize;
		}
		else
		{
			buffer_pointer = imgInfo.dst + imgInfo.data_size;
			remaining_buffer_size -= imgInfo.data_size;
		}
		maxwidth = std::max(maxwidth >> 1, 1u);
		maxheight = std::max(maxheight >> 1, 1u);
		if (ddsfile &&  maxwidth < 4 && maxheight < 4)
		{
			break;
		}
		if (imgInfo.nummipmaps > 0)
		{
			// Give priority to load dds with packed levels
			ret->m_levels = imgInfo.nummipmaps + 1;
			if (nrm_posible)
			{
				for (u32 mip_level = 1; mip_level != ret->m_levels; ++mip_level)
				{
					u32 mip_width = TextureUtil::CalculateLevelSize(imgInfo.Width, mip_level);
					u32 mip_height = TextureUtil::CalculateLevelSize(imgInfo.Height, mip_level);
					u32 requiredsize = TextureUtil::GetTextureSizeInBytes(mip_width, mip_height, ret->m_format);
					buffer_pointer += TextureUtil::GetTextureSizeInBytes(mip_width, mip_height, ret->m_format);
					remaining_buffer_size -= requiredsize;
				}
			}
			break;
		}
	}
	if (nrm_posible)
	{
		for (size_t level = 0; level < current.normal_map.size(); level++)
		{
			ImageLoaderParams imgInfo;
			hires_mip_level &item = current.normal_map[level];
			imgInfo.dst = nullptr;
			imgInfo.Path = item.path.c_str();
			imgInfo.request_buffer_delegate = [&](size_t requiredsize, bool mipmapsincluded)
			{
				// is required size is biguer that the remaining size on the packed buffer just reject
				if (requiredsize > remaining_buffer_size)
				{
					return static_cast<u8*>(nullptr);
				}
				// just return the pointer to pack the textures in a single buffer.
				return buffer_pointer;
			};
			bool ddsfile = false;
			if (item.is_compressed)
			{
				if (!last_level_is_dds)
				{
					// don't give support to mixed formats
					break;
				}
				ddsfile = true;
				last_level_is_dds = true;
				ReadDDS(imgInfo);
			}
			else
			{
				if (last_level_is_dds)
				{
					// don't give support to mixed formats
					break;
				}
				last_level_is_dds = false;
				ReadPNG(imgInfo);
			}
			if (imgInfo.dst == nullptr || imgInfo.resultTex == PC_TEX_FMT_NONE)
			{
				if (allocated_data && cacheresult && imgInfo.dst != nullptr)
				{
					delete[] imgInfo.dst;
				}
				ERROR_LOG(VIDEO, "Custom texture %s failed to load normal map level %zu", imgInfo.Path, level);
				break;
			}
			if (level == 0)
			{
				maxwidth = imgInfo.Width;
				maxheight = imgInfo.Height;
				if (ret->m_width != imgInfo.Width
					|| ret->m_height != imgInfo.Height
					|| ret->m_format != imgInfo.resultTex)
				{
					ERROR_LOG(VIDEO,
						"Custom texture %s invalid level %zu size for normal map: %u %u required: %u %u format: %u",
						imgInfo.Path,
						level,
						imgInfo.Width,
						imgInfo.Height,
						maxwidth,
						maxheight,
						ret->m_format);
					break;
				}
			}
			else
			{
				if (maxwidth != imgInfo.Width
					|| maxheight != imgInfo.Height
					|| ret->m_format != imgInfo.resultTex)
				{
					ERROR_LOG(VIDEO,
						"Custom texture %s invalid normal map level %zu size: %u %u required: %u %u format: %u",
						imgInfo.Path,
						level,
						imgInfo.Width,
						imgInfo.Height,
						maxwidth,
						maxheight,
						ret->m_format);
					break;
				}
			}
			ret->m_nrm_levels++;
			if (ddsfile)
			{
				u32 requiredsize = TextureUtil::GetTextureSizeInBytes(maxwidth, maxheight, imgInfo.resultTex);
				buffer_pointer = imgInfo.dst + requiredsize;
				remaining_buffer_size -= requiredsize;
			}
			else
			{
				buffer_pointer = imgInfo.dst + imgInfo.data_size;
				remaining_buffer_size -= imgInfo.data_size;
			}
			maxwidth = std::max(maxwidth >> 1, 1u);
			maxheight = std::max(maxheight >> 1, 1u);
			if (ddsfile &&  maxwidth < 4 && maxheight < 4)
			{
				break;
			}
			if (imgInfo.nummipmaps > 0)
			{
				// Give priority to load dds with packed levels
				ret->m_nrm_levels = imgInfo.nummipmaps + 1;
				break;
			}
		}
	}
	if (ret != nullptr && ret->m_nrm_levels != ret->m_levels)
	{
		// disable normal map if the size or levels are different
		ret->m_nrm_levels = 0;
	}
	return ret;
}
