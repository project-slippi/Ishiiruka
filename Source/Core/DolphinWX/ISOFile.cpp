// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <wx/app.h>
#include <wx/bitmap.h>
#include <wx/filefn.h>
#include <wx/image.h>
#include <wx/toplevel.h>

#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"

#include "Core/Boot/Boot.h"
#include "Core/ConfigManager.h"

#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"

#include "DolphinWX/ISOFile.h"
#include "DolphinWX/WxUtils.h"

static const u32 CACHE_REVISION = 0x127;  // Last changed in PR 3309

static std::string GetLanguageString(DiscIO::Language language,
	std::map<DiscIO::Language, std::string> strings)
{
	auto end = strings.end();
	auto it = strings.find(language);
	if (it != end)
		return it->second;

	// English tends to be a good fallback when the requested language isn't available
	if (language != DiscIO::Language::LANGUAGE_ENGLISH)
	{
		it = strings.find(DiscIO::Language::LANGUAGE_ENGLISH);
		if (it != end)
			return it->second;
	}

	// If English isn't available either, just pick something
	if (!strings.empty())
		return strings.cbegin()->second;

	return "";
}

GameListItem::GameListItem(const std::string& _rFileName,
	const std::unordered_map<std::string, std::string>& custom_titles)
	: m_FileName(_rFileName), m_title_id(0), m_emu_state(0), m_FileSize(0),
	m_Country(DiscIO::Country::COUNTRY_UNKNOWN), m_Revision(0), m_Valid(false), m_ImageWidth(0),
	m_ImageHeight(0), m_disc_number(0), m_has_custom_name(false)
{
	if (LoadFromCache())
	{
		m_Valid = true;

		// Wii banners can only be read if there is a savefile,
		// so sometimes caches don't contain banners. Let's check
		// if a banner has become available after the cache was made.
		if (m_pImage.empty())
		{
			std::vector<u32> buffer =
				DiscIO::IVolume::GetWiiBanner(&m_ImageWidth, &m_ImageHeight, m_title_id);
			ReadVolumeBanner(buffer, m_ImageWidth, m_ImageHeight);
			if (!m_pImage.empty())
				SaveToCache();
		}
	}
	else
	{
		std::unique_ptr<DiscIO::IVolume> volume(DiscIO::CreateVolumeFromFilename(_rFileName));

		if (volume != nullptr)
		{
			m_Platform = volume->GetVolumeType();

			m_descriptions = volume->GetDescriptions();
			m_names = volume->GetLongNames();
			if (m_names.empty())
				m_names = volume->GetShortNames();
			m_company = GetLanguageString(DiscIO::Language::LANGUAGE_ENGLISH, volume->GetLongMakers());
			if (m_company.empty())
				m_company = GetLanguageString(DiscIO::Language::LANGUAGE_ENGLISH, volume->GetShortMakers());

			m_Country = volume->GetCountry();
			m_blob_type = volume->GetBlobType();
			m_FileSize = volume->GetRawSize();
			m_VolumeSize = volume->GetSize();

			m_game_id = volume->GetGameID();
			volume->GetTitleID(&m_title_id);
			m_disc_number = volume->GetDiscNumber();
			m_Revision = volume->GetRevision();

			std::vector<u32> buffer = volume->GetBanner(&m_ImageWidth, &m_ImageHeight);
			ReadVolumeBanner(buffer, m_ImageWidth, m_ImageHeight);

			m_Valid = true;
			SaveToCache();
		}
	}

	if (m_company.empty() && m_game_id.size() >= 6)
		m_company = DiscIO::GetCompanyFromID(m_game_id.substr(4, 2));

	if (IsValid())
	{
		std::string short_game_id = m_game_id;

		// Ignore publisher ID for WAD files
		if (m_Platform == DiscIO::Platform::WII_WAD && short_game_id.size() > 4)
			short_game_id.erase(4);

		auto it = custom_titles.find(short_game_id);
		if (it != custom_titles.end())
		{
			m_custom_name_titles_txt = it->second;
		}

		ReloadINI();
	}

	if (!IsValid() && IsElfOrDol())
	{
		m_Valid = true;
		m_FileSize = File::GetSize(_rFileName);
		m_Platform = DiscIO::Platform::ELF_DOL;
		m_blob_type = DiscIO::BlobType::DIRECTORY;
	}

	std::string path, name;
	SplitPath(m_FileName, &path, &name, nullptr);

	// A bit like the Homebrew Channel icon, except there can be multiple files
	// in a folder with their own icons. Useful for those who don't want to have
	// a Homebrew Channel-style folder structure.
	if (ReadPNGBanner(path + name + ".png"))
		return;

	// Homebrew Channel icon. Typical for DOLs and ELFs,
	// but can be also used with volumes.
	if (ReadPNGBanner(path + "icon.png"))
		return;

	// Volume banner. Typical for everything that isn't a DOL or ELF.
	if (!m_pImage.empty())
	{
		// Need to make explicit copy as wxImage uses reference counting for copies combined with only
		// taking a pointer, not the content, when given a buffer to its constructor.
		m_image.Create(m_ImageWidth, m_ImageHeight, false);
		std::memcpy(m_image.GetData(), m_pImage.data(), m_pImage.size());
		return;
	}
}

GameListItem::~GameListItem()
{
}

void GameListItem::ReloadINI()
{
	if (!IsValid())
		return;

	IniFile ini = SConfig::LoadGameIni(m_game_id, m_Revision);
	ini.GetIfExists("EmuState", "EmulationStateId", &m_emu_state, 0);
	ini.GetIfExists("EmuState", "EmulationIssues", &m_issues, std::string());

	m_custom_name.clear();
	m_has_custom_name = ini.GetIfExists("EmuState", "Title", &m_custom_name);
	if (!m_has_custom_name && !m_custom_name_titles_txt.empty())
	{
		m_custom_name = m_custom_name_titles_txt;
		m_has_custom_name = true;
	}
}

bool GameListItem::LoadFromCache()
{
	return CChunkFileReader::Load<GameListItem>(CreateCacheFilename(), CACHE_REVISION, *this);
}

void GameListItem::SaveToCache()
{
	if (!File::IsDirectory(File::GetUserPath(D_CACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_CACHE_IDX));

	CChunkFileReader::Save<GameListItem>(CreateCacheFilename(), CACHE_REVISION, *this);
}

void GameListItem::DoState(PointerWrap& p)
{
	p.Do(m_names);
	p.Do(m_descriptions);
	p.Do(m_company);
	p.Do(m_game_id);
	p.Do(m_title_id);
	p.Do(m_FileSize);
	p.Do(m_VolumeSize);
	p.Do(m_Country);
	p.Do(m_blob_type);
	p.Do(m_pImage);
	p.Do(m_ImageWidth);
	p.Do(m_ImageHeight);
	p.Do(m_Platform);
	p.Do(m_disc_number);
	p.Do(m_Revision);
}

bool GameListItem::IsElfOrDol() const
{
	if (m_FileName.size() < 4)
		return false;

	std::string name_end = m_FileName.substr(m_FileName.size() - 4);
	std::transform(name_end.begin(), name_end.end(), name_end.begin(), ::tolower);
	return name_end == ".elf" || name_end == ".dol";
}

std::string GameListItem::CreateCacheFilename() const
{
	std::string Filename, LegalPathname, extension;
	SplitPath(m_FileName, &LegalPathname, &Filename, &extension);

	if (Filename.empty())
		return Filename;  // Disc Drive

	// Filename.extension_HashOfFolderPath_Size.cache
	// Append hash to prevent ISO name-clashing in different folders.
	Filename.append(
		StringFromFormat("%s_%x_%" PRIx64 ".cache", extension.c_str(),
			HashFletcher((const u8*)LegalPathname.c_str(), LegalPathname.size()),
			File::GetSize(m_FileName)));

	std::string fullname(File::GetUserPath(D_CACHE_IDX));
	fullname += Filename;
	return fullname;
}

// Outputs to m_pImage
void GameListItem::ReadVolumeBanner(const std::vector<u32>& buffer, int width, int height)
{
	m_pImage.resize(width * height * 3);
	for (int i = 0; i < width * height; i++)
	{
		m_pImage[i * 3 + 0] = (buffer[i] & 0xFF0000) >> 16;
		m_pImage[i * 3 + 1] = (buffer[i] & 0x00FF00) >> 8;
		m_pImage[i * 3 + 2] = (buffer[i] & 0x0000FF) >> 0;
	}
}

// Outputs to m_Bitmap
bool GameListItem::ReadPNGBanner(const std::string& path)
{
	if (!File::Exists(path))
		return false;

	wxImage image(StrToWxStr(path), wxBITMAP_TYPE_PNG);
	if (!image.IsOk())
		return false;

	m_image = image;
	return true;
}

std::string GameListItem::GetDescription(DiscIO::Language language) const
{
	return GetLanguageString(language, m_descriptions);
}

std::string GameListItem::GetDescription() const
{
	bool wii = m_Platform != DiscIO::Platform::GAMECUBE_DISC;
	return GetDescription(SConfig::GetInstance().GetCurrentLanguage(wii));
}

std::string GameListItem::GetName(DiscIO::Language language) const
{
	return GetLanguageString(language, m_names);
}

std::string GameListItem::GetName() const
{
	if (m_has_custom_name)
		return m_custom_name;

	bool wii = m_Platform != DiscIO::Platform::GAMECUBE_DISC;
	std::string name = GetName(SConfig::GetInstance().GetCurrentLanguage(wii));
	if (!name.empty())
		return name;

	// No usable name, return filename (better than nothing)
	std::string ext;
	SplitPath(GetFileName(), nullptr, &name, &ext);
	return name + ext;
}

std::string GameListItem::GetUniqueIdentifier() const
{
	const DiscIO::Language lang = DiscIO::Language::LANGUAGE_ENGLISH;
	std::vector<std::string> info;
	if (!GetGameID().empty())
		info.push_back(GetGameID());
	if (GetRevision() != 0)
	{
		std::string rev_str = "Revision ";
		info.push_back(rev_str + std::to_string((long long)GetRevision()));
	}

	std::string name(GetName(lang));
	if (name.empty())
		name = GetName();

	int disc_number = GetDiscNumber() + 1;

	std::string lower_name = name;
	std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
	if (disc_number > 1 &&
		lower_name.find(wxString::Format("disc %i", disc_number).ToStdString()) == std::string::npos &&
		lower_name.find(wxString::Format("disc%i", disc_number).ToStdString()) == std::string::npos)
	{
		std::string disc_text = "Disc ";
		info.push_back(disc_text + std::to_string(disc_number));
	}
	if (info.empty())
		return name;
	std::ostringstream ss;
	std::copy(info.begin(), info.end() - 1, std::ostream_iterator<std::string>(ss, ", "));
	ss << info.back();
	return name + " (" + ss.str() + ")";
}

std::vector<DiscIO::Language> GameListItem::GetLanguages() const
{
	std::vector<DiscIO::Language> languages;
	for (std::pair<DiscIO::Language, std::string> name : m_names)
		languages.push_back(name.first);
	return languages;
}

const std::string GameListItem::GetWiiFSPath() const
{
	std::unique_ptr<DiscIO::IVolume> iso(DiscIO::CreateVolumeFromFilename(m_FileName));
	std::string ret;

	if (iso == nullptr)
		return ret;

	if (iso->GetVolumeType() != DiscIO::Platform::GAMECUBE_DISC)
	{
		u64 title_id = 0;
		iso->GetTitleID(&title_id);

		const std::string path =
			StringFromFormat("%s/title/%08x/%08x/data/", File::GetUserPath(D_WIIROOT_IDX).c_str(),
			(u32)(title_id >> 32), (u32)title_id);

		if (!File::Exists(path))
			File::CreateFullPath(path);

		if (path[0] == '.')
			ret = WxStrToStr(wxGetCwd()) + path.substr(strlen(ROOT_DIR));
		else
			ret = path;
	}

	return ret;
}

bool GameListItem::IsCompressed() const
{
	return m_blob_type == DiscIO::BlobType::GCZ || m_blob_type == DiscIO::BlobType::CISO ||
		m_blob_type == DiscIO::BlobType::WBFS;
}
