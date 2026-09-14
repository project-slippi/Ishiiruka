// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Based off of tachtig/twintig http://git.infradead.org/?p=users/segher/wii.git
// Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <mbedtls/aes.h>
#include <mbedtls/md5.h>
#include <memory>
#include <string>
#include <vector>

#include "Common/Align.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"

#include "Core/HW/WiiSaveCrypted.h"

const u8 CWiiSaveCrypted::s_md5_blanker[16] = { 0x0E, 0x65, 0x37, 0x81, 0x99, 0xBE, 0x45, 0x17,
																							 0xAB, 0x06, 0xEC, 0x22, 0x45, 0x1A, 0x57, 0x93 };
const u32 CWiiSaveCrypted::s_ng_id = 0x0403AC68;

// Wii saves are encrypted with the Wii SD key, which Slippi Dolphin does not ship, so every
// import and export is refused up front and a CWiiSaveCrypted never becomes valid.
static void AlertWiiSavesUnsupported()
{
	PanicAlertT("Wii save import and export are not supported by Slippi Dolphin.");
}

bool CWiiSaveCrypted::ImportWiiSave(const std::string& /*filename*/)
{
	AlertWiiSavesUnsupported();
	return false;
}

bool CWiiSaveCrypted::ExportWiiSave(u64 /*title_id*/)
{
	AlertWiiSavesUnsupported();
	return false;
}

void CWiiSaveCrypted::ExportAllSaves()
{
	AlertWiiSavesUnsupported();
}

CWiiSaveCrypted::CWiiSaveCrypted(const std::string& filename, u64 title_id)
	: m_encrypted_save_path(filename), m_title_id(title_id), m_valid(false)
{
	// Without the SD key the AES context cannot be set up, so nothing below can run.
}

void CWiiSaveCrypted::ReadHDR()
{
	File::IOFile data_file(m_encrypted_save_path, "rb");
	if (!data_file)
	{
		ERROR_LOG(CONSOLE, "Cannot open %s", m_encrypted_save_path.c_str());
		m_valid = false;
		return;
	}
	if (!data_file.ReadBytes(&m_encrypted_header, HEADER_SZ))
	{
		ERROR_LOG(CONSOLE, "Failed to read header");
		m_valid = false;
		return;
	}
	data_file.Close();

	mbedtls_aes_crypt_cbc(&m_aes_ctx, MBEDTLS_AES_DECRYPT, HEADER_SZ, m_sd_iv,
		(const u8*)&m_encrypted_header, (u8*)&m_header);
	u32 banner_size = Common::swap32(m_header.hdr.BannerSize);
	if ((banner_size < FULL_BNR_MIN) || (banner_size > FULL_BNR_MAX) ||
		(((banner_size - BNR_SZ) % ICON_SZ) != 0))
	{
		ERROR_LOG(CONSOLE, "Not a Wii save or read failure for file header size %x", banner_size);
		m_valid = false;
		return;
	}
	m_title_id = Common::swap64(m_header.hdr.SaveGameTitle);

	u8 md5_file[16];
	u8 md5_calc[16];
	memcpy(md5_file, m_header.hdr.Md5, 0x10);
	memcpy(m_header.hdr.Md5, s_md5_blanker, 0x10);
	mbedtls_md5((u8*)&m_header, HEADER_SZ, md5_calc);
	if (memcmp(md5_file, md5_calc, 0x10))
	{
		ERROR_LOG(CONSOLE, "MD5 mismatch\n %016" PRIx64 "%016" PRIx64 " != %016" PRIx64 "%016" PRIx64,
			Common::swap64(md5_file), Common::swap64(md5_file + 8), Common::swap64(md5_calc),
			Common::swap64(md5_calc + 8));
		m_valid = false;
	}

	if (!getPaths())
	{
		m_valid = false;
		return;
	}
	std::string banner_file_path = m_wii_title_path + "banner.bin";
	if (!File::Exists(banner_file_path) ||
		AskYesNoT("%s already exists, overwrite?", banner_file_path.c_str()))
	{
		INFO_LOG(CONSOLE, "Creating file %s", banner_file_path.c_str());
		File::IOFile banner_file(banner_file_path, "wb");
		banner_file.WriteBytes(m_header.BNR, banner_size);
	}
}

void CWiiSaveCrypted::WriteHDR()
{
	if (!m_valid)
		return;
	memset(&m_header, 0, HEADER_SZ);

	std::string banner_file_path = m_wii_title_path + "banner.bin";
	u32 banner_size = static_cast<u32>(File::GetSize(banner_file_path));
	m_header.hdr.BannerSize = Common::swap32(banner_size);

	m_header.hdr.SaveGameTitle = Common::swap64(m_title_id);
	memcpy(m_header.hdr.Md5, s_md5_blanker, 0x10);
	m_header.hdr.Permissions = 0x3C;

	File::IOFile banner_file(banner_file_path, "rb");
	if (!banner_file.ReadBytes(m_header.BNR, banner_size))
	{
		ERROR_LOG(CONSOLE, "Failed to read banner.bin");
		m_valid = false;
		return;
	}
	// remove nocopy flag
	m_header.BNR[7] &= ~1;

	u8 md5_calc[16];
	mbedtls_md5((u8*)&m_header, HEADER_SZ, md5_calc);
	memcpy(m_header.hdr.Md5, md5_calc, 0x10);

	mbedtls_aes_crypt_cbc(&m_aes_ctx, MBEDTLS_AES_ENCRYPT, HEADER_SZ, m_sd_iv, (const u8*)&m_header,
		(u8*)&m_encrypted_header);

	File::IOFile data_file(m_encrypted_save_path, "wb");
	if (!data_file.WriteBytes(&m_encrypted_header, HEADER_SZ))
	{
		ERROR_LOG(CONSOLE, "Failed to write header for %s", m_encrypted_save_path.c_str());
		m_valid = false;
	}
}

void CWiiSaveCrypted::ReadBKHDR()
{
	if (!m_valid)
		return;

	File::IOFile fpData_bin(m_encrypted_save_path, "rb");
	if (!fpData_bin)
	{
		ERROR_LOG(CONSOLE, "Cannot open %s", m_encrypted_save_path.c_str());
		m_valid = false;
		return;
	}
	fpData_bin.Seek(HEADER_SZ, SEEK_SET);
	if (!fpData_bin.ReadBytes(&m_bk_hdr, BK_SZ))
	{
		ERROR_LOG(CONSOLE, "Failed to read bk header");
		m_valid = false;
		return;
	}
	fpData_bin.Close();

	if (m_bk_hdr.size != Common::swap32(BK_LISTED_SZ) ||
		m_bk_hdr.magic != Common::swap32(BK_HDR_MAGIC))
	{
		ERROR_LOG(CONSOLE, "Invalid Size(%x) or Magic word (%x)", m_bk_hdr.size, m_bk_hdr.magic);
		m_valid = false;
		return;
	}

	m_files_list_size = Common::swap32(m_bk_hdr.numberOfFiles);
	m_size_of_files = Common::swap32(m_bk_hdr.sizeOfFiles);
	m_total_size = Common::swap32(m_bk_hdr.totalSize);

	if (m_size_of_files + FULL_CERT_SZ != m_total_size)
	{
		WARN_LOG(CONSOLE, "Size(%x) + cert(%x) does not equal totalsize(%x)", m_size_of_files,
			FULL_CERT_SZ, m_total_size);
	}
	if (m_title_id != Common::swap64(m_bk_hdr.SaveGameTitle))
	{
		WARN_LOG(CONSOLE,
			"Encrypted title (%" PRIx64 ") does not match unencrypted title (%" PRIx64 ")",
			m_title_id, Common::swap64(m_bk_hdr.SaveGameTitle));
	}
}

void CWiiSaveCrypted::WriteBKHDR()
{
	if (!m_valid)
		return;
	m_files_list_size = 0;
	m_size_of_files = 0;

	ScanForFiles(m_wii_title_path, m_files_list, &m_files_list_size, &m_size_of_files);
	memset(&m_bk_hdr, 0, BK_SZ);
	m_bk_hdr.size = Common::swap32(BK_LISTED_SZ);
	m_bk_hdr.magic = Common::swap32(BK_HDR_MAGIC);
	m_bk_hdr.NGid = s_ng_id;
	m_bk_hdr.numberOfFiles = Common::swap32(m_files_list_size);
	m_bk_hdr.sizeOfFiles = Common::swap32(m_size_of_files);
	m_bk_hdr.totalSize = Common::swap32(m_size_of_files + FULL_CERT_SZ);
	m_bk_hdr.SaveGameTitle = Common::swap64(m_title_id);

	File::IOFile data_file(m_encrypted_save_path, "ab");
	if (!data_file.WriteBytes(&m_bk_hdr, BK_SZ))
	{
		ERROR_LOG(CONSOLE, "Failed to write bkhdr");
		m_valid = false;
	}
}

void CWiiSaveCrypted::ImportWiiSaveFiles()
{
	if (!m_valid)
		return;

	File::IOFile data_file(m_encrypted_save_path, "rb");
	if (!data_file)
	{
		ERROR_LOG(CONSOLE, "Cannot open %s", m_encrypted_save_path.c_str());
		m_valid = false;
		return;
	}

	data_file.Seek(HEADER_SZ + BK_SZ, SEEK_SET);

	FileHDR file_hdr_tmp;

	for (u32 i = 0; i < m_files_list_size; ++i)
	{
		memset(&file_hdr_tmp, 0, FILE_HDR_SZ);
		memset(m_iv, 0, 0x10);
		u32 file_size = 0;

		if (!data_file.ReadBytes(&file_hdr_tmp, FILE_HDR_SZ))
		{
			ERROR_LOG(CONSOLE, "Failed to read header for file %d", i);
			m_valid = false;
		}

		if (Common::swap32(file_hdr_tmp.magic) != FILE_HDR_MAGIC)
		{
			ERROR_LOG(CONSOLE, "Bad File Header");
			break;
		}
		else
		{
			// Allows files in subfolders to be escaped properly (ex: "nocopy/data00")
			// Special characters in path components will be escaped such as /../
			std::string file_path = Common::EscapePath(reinterpret_cast<const char*>(file_hdr_tmp.name));

			std::string file_path_full = m_wii_title_path + file_path;
			File::CreateFullPath(file_path_full);
			if (file_hdr_tmp.type == 1)
			{
				file_size = Common::swap32(file_hdr_tmp.size);
				u32 file_size_rounded = Common::AlignUp(file_size, BLOCK_SZ);
				std::vector<u8> file_data(file_size_rounded);
				std::vector<u8> file_data_enc(file_size_rounded);

				if (!data_file.ReadBytes(file_data_enc.data(), file_size_rounded))
				{
					ERROR_LOG(CONSOLE, "Failed to read data from file %d", i);
					m_valid = false;
					break;
				}

				memcpy(m_iv, file_hdr_tmp.IV, 0x10);
				mbedtls_aes_crypt_cbc(&m_aes_ctx, MBEDTLS_AES_DECRYPT, file_size_rounded, m_iv,
					static_cast<const u8*>(file_data_enc.data()), file_data.data());

				if (!File::Exists(file_path_full) ||
					AskYesNoT("%s already exists, overwrite?", file_path_full.c_str()))
				{
					INFO_LOG(CONSOLE, "Creating file %s", file_path_full.c_str());

					File::IOFile raw_save_file(file_path_full, "wb");
					raw_save_file.WriteBytes(file_data.data(), file_size);
				}
			}
			else if (file_hdr_tmp.type == 2)
			{
				if (!File::Exists(file_path_full))
				{
					if (!File::CreateDir(file_path_full))
						ERROR_LOG(CONSOLE, "Failed to create directory %s", file_path_full.c_str());
				}
				else if (!File::IsDirectory(file_path_full))
				{
					ERROR_LOG(CONSOLE,
						"Failed to create directory %s because a file with the same name exists",
						file_path_full.c_str());
				}
			}
		}
	}
}

void CWiiSaveCrypted::ExportWiiSaveFiles()
{
	if (!m_valid)
		return;

	for (u32 i = 0; i < m_files_list_size; i++)
	{
		FileHDR file_hdr_tmp;
		memset(&file_hdr_tmp, 0, FILE_HDR_SZ);

		u32 file_size = 0;
		if (File::IsDirectory(m_files_list[i]))
		{
			file_hdr_tmp.type = 2;
		}
		else
		{
			file_size = static_cast<u32>(File::GetSize(m_files_list[i]));
			file_hdr_tmp.type = 1;
		}

		u32 file_size_rounded = Common::AlignUp(file_size, BLOCK_SZ);
		file_hdr_tmp.magic = Common::swap32(FILE_HDR_MAGIC);
		file_hdr_tmp.size = Common::swap32(file_size);
		file_hdr_tmp.Permissions = 0x3c;

		std::string name =
			Common::UnescapeFileName(m_files_list[i].substr(m_wii_title_path.length() + 1));

		if (name.length() > 0x44)
		{
			ERROR_LOG(CONSOLE, "\"%s\" is too long for the filename, max length is 0x44 + \\0",
				name.c_str());
			m_valid = false;
			return;
		}
		strncpy((char*)file_hdr_tmp.name, name.c_str(), sizeof(file_hdr_tmp.name));

		{
			File::IOFile fpData_bin(m_encrypted_save_path, "ab");
			fpData_bin.WriteBytes(&file_hdr_tmp, FILE_HDR_SZ);
		}

		if (file_hdr_tmp.type == 1)
		{
			if (file_size == 0)
			{
				ERROR_LOG(CONSOLE, "%s is a 0 byte file", m_files_list[i].c_str());
				m_valid = false;
				return;
			}
			File::IOFile raw_save_file(m_files_list[i], "rb");
			if (!raw_save_file)
			{
				ERROR_LOG(CONSOLE, "%s failed to open", m_files_list[i].c_str());
				m_valid = false;
			}

			std::vector<u8> file_data(file_size_rounded);
			std::vector<u8> file_data_enc(file_size_rounded);

			if (!raw_save_file.ReadBytes(file_data.data(), file_size))
			{
				ERROR_LOG(CONSOLE, "Failed to read data from file: %s", m_files_list[i].c_str());
				m_valid = false;
			}

			mbedtls_aes_crypt_cbc(&m_aes_ctx, MBEDTLS_AES_ENCRYPT, file_size_rounded, file_hdr_tmp.IV,
				static_cast<const u8*>(file_data.data()), file_data_enc.data());

			File::IOFile fpData_bin(m_encrypted_save_path, "ab");
			if (!fpData_bin.WriteBytes(file_data_enc.data(), file_size_rounded))
			{
				ERROR_LOG(CONSOLE, "Failed to write data to file: %s", m_encrypted_save_path.c_str());
			}
		}
	}
}

bool CWiiSaveCrypted::getPaths(bool for_export)
{
	if (m_title_id)
	{
		// CONFIGURED because this whole class is only used from the GUI, not directly by games.
		m_wii_title_path = Common::GetTitleDataPath(m_title_id, Common::FROM_CONFIGURED_ROOT);
	}

	if (for_export)
	{
		char game_id[5];
		sprintf(game_id, "%c%c%c%c", (u8)(m_title_id >> 24) & 0xFF, (u8)(m_title_id >> 16) & 0xFF,
			(u8)(m_title_id >> 8) & 0xFF, (u8)m_title_id & 0xFF);

		if (!File::IsDirectory(m_wii_title_path))
		{
			m_valid = false;
			ERROR_LOG(CONSOLE, "No save folder found for title %s", game_id);
			return false;
		}

		if (!File::Exists(m_wii_title_path + "banner.bin"))
		{
			m_valid = false;
			ERROR_LOG(CONSOLE, "No banner file found for title  %s", game_id);
			return false;
		}
		if (m_encrypted_save_path.length() == 0)
		{
			// If no path was passed, use User folder
			m_encrypted_save_path = File::GetUserPath(D_USER_IDX);
		}
		m_encrypted_save_path += StringFromFormat("private/wii/title/%s/data.bin", game_id);
		File::CreateFullPath(m_encrypted_save_path);
	}
	else
	{
		File::CreateFullPath(m_wii_title_path);
		if (!AskYesNoT("Warning! it is advised to backup all files in the folder:\n%s\nDo you wish to "
			"continue?",
			m_wii_title_path.c_str()))
		{
			return false;
		}
	}
	return true;
}

void CWiiSaveCrypted::ScanForFiles(const std::string& save_directory,
	std::vector<std::string>& file_list, u32* num_files,
	u32* size_files)
{
	std::vector<std::string> directories;
	directories.push_back(save_directory);
	u32 num = 0;
	u32 size = 0;

	for (u32 i = 0; i < directories.size(); ++i)
	{
		if (i != 0)
		{
			// add dir to fst
			file_list.push_back(directories[i]);
		}

		File::FSTEntry fst_tmp = File::ScanDirectoryTree(directories[i], false);
		for (const File::FSTEntry& elem : fst_tmp.children)
		{
			if (elem.virtualName != "banner.bin")
			{
				num++;
				size += FILE_HDR_SZ;
				if (elem.isDirectory)
				{
					if (elem.virtualName == "nocopy" || elem.virtualName == "nomove")
					{
						NOTICE_LOG(CONSOLE,
							"This save will likely require homebrew tools to copy to a real Wii.");
					}

					directories.push_back(elem.physicalName);
				}
				else
				{
					file_list.push_back(elem.physicalName);
					size += static_cast<u32>(Common::AlignUp(elem.size, BLOCK_SZ));
				}
			}
		}
	}

	*num_files = num;
	*size_files = size;
}

CWiiSaveCrypted::~CWiiSaveCrypted()
{
}
