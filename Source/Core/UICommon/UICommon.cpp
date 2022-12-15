// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef _WIN32
#include <shlobj.h>  // for SHGetFolderPath
#endif

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/LogManager.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "Core/ConfigManager.h"
#include "Core/HW/Wiimote.h"

#include "InputCommon/GCAdapter.h"

#include "UICommon/UICommon.h"

#include "VideoCommon/VideoBackendBase.h"

#include <iostream>

namespace UICommon
{
void Init()
{
  LogManager::Init();
  SConfig::Init();
  VideoBackendBase::PopulateList();
  WiimoteReal::LoadSettings();
  GCAdapter::Init();
  VideoBackendBase::ActivateBackend(SConfig::GetInstance().m_strVideoBackend);

  SetEnableAlert(SConfig::GetInstance().bUsePanicHandlers);
}

void Shutdown()
{
  GCAdapter::Shutdown();
  WiimoteReal::Shutdown();
  VideoBackendBase::ClearList();
  SConfig::Shutdown();
  LogManager::Shutdown();
}

void CreateDirectories()
{
  // Copy initial Wii NAND data from Sys to User.
  File::CopyDir(File::GetSysDirectory() + WII_USER_DIR, File::GetUserPath(D_WIIROOT_IDX));

	File::CreateFullPath(File::GetUserPath(D_USER_IDX));
	File::CreateFullPath(File::GetUserPath(D_CACHE_IDX));
	File::CreateFullPath(File::GetUserPath(D_CONFIG_IDX));
	File::CreateFullPath(File::GetUserPath(D_DUMPDSP_IDX));
	File::CreateFullPath(File::GetUserPath(D_DUMPSSL_IDX));
	File::CreateFullPath(File::GetUserPath(D_DUMPTEXTURES_IDX));
	File::CreateFullPath(File::GetUserPath(D_GAMESETTINGS_IDX));
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX));
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + USA_DIR DIR_SEP);
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + EUR_DIR DIR_SEP);
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + JAP_DIR DIR_SEP);
	File::CreateFullPath(File::GetUserPath(D_HIRESTEXTURES_IDX));
	File::CreateFullPath(File::GetUserPath(D_MAILLOGS_IDX));
	File::CreateFullPath(File::GetUserPath(D_MAPS_IDX));
	File::CreateFullPath(File::GetUserPath(D_SCREENSHOTS_IDX));
	File::CreateFullPath(File::GetUserPath(D_SHADERS_IDX));
	File::CreateFullPath(File::GetUserPath(D_SHADERS_IDX) + DIR_SEP POSTPROCESSING_SHADER_SUBDIR DIR_SEP);
	File::CreateFullPath(File::GetUserPath(D_SHADERS_IDX) + DIR_SEP SCALING_SHADER_SUBDIR DIR_SEP);
	File::CreateFullPath(File::GetUserPath(D_SHADERS_IDX) + DIR_SEP STEREO_SHADER_SUBDIR DIR_SEP);
	File::CreateFullPath(File::GetUserPath(D_STATESAVES_IDX));
	File::CreateFullPath(File::GetUserPath(D_THEMES_IDX));
}

void SetUserDirectory(const std::string& custom_path)
{
  if (!custom_path.empty())
  {
	File::CreateFullPath(custom_path + DIR_SEP);
	File::SetUserPath(D_USER_IDX, custom_path + DIR_SEP);
	return;
  }
  std::string user_path = "";

#ifdef _WIN32

  // Check our registry keys
  HKEY hkey;
  DWORD local = 0;
  TCHAR configPath[MAX_PATH] = {0};
  if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Dolphin Emulator"), 0, KEY_QUERY_VALUE,
				   &hkey) == ERROR_SUCCESS)
  {
	DWORD size = 4;
	if (RegQueryValueEx(hkey, TEXT("LocalUserConfig"), nullptr, nullptr,
						reinterpret_cast<LPBYTE>(&local), &size) != ERROR_SUCCESS)
	  local = 0;

	size = MAX_PATH;
	if (RegQueryValueEx(hkey, TEXT("UserConfigPath"), nullptr, nullptr, (LPBYTE)configPath,
						&size) != ERROR_SUCCESS)
	  configPath[0] = 0;
	RegCloseKey(hkey);
  }

  local = local || File::Exists(File::GetExeDirectory() + DIR_SEP "portable.txt");

  // Get Program Files path in case we need it.
  TCHAR my_documents[MAX_PATH];
  bool my_documents_found = SUCCEEDED(
	  SHGetFolderPath(nullptr, CSIDL_MYDOCUMENTS, nullptr, SHGFP_TYPE_CURRENT, my_documents));

  // Detect where the User directory is (on top of the command line flag, which overrides all this):
  // 1. GetExeDirectory()\portable.txt exists
  //    -> Use GetExeDirectory()\User
  // 2. HKCU\Software\Dolphin Emulator\LocalUserConfig exists and is true
  //    -> Use GetExeDirectory()\User
  // 3. HKCU\Software\Dolphin Emulator\UserConfigPath exists
  //    -> Use this as the user directory path
  // 4. My Documents exists
  //    -> Use My Documents\Dolphin Emulator as the User directory path
  // 5. Default
  //    -> Use GetExeDirectory()\User

  if (local) // Case 1-2
	user_path = File::GetExeDirectory() + DIR_SEP USERDATA_DIR DIR_SEP;
  else if (configPath[0]) // Case 3
	user_path = TStrToUTF8(configPath);
  else if (my_documents_found) // Case 4
	user_path = TStrToUTF8(my_documents) + DIR_SEP "Dolphin Emulator" DIR_SEP;
  else // Case 5
	user_path = File::GetExeDirectory() + DIR_SEP USERDATA_DIR DIR_SEP;

  // Prettify the path: it will be displayed in some places, we don't want a mix of \ and /.
  user_path = ReplaceAll(user_path, "\\", DIR_SEP);

  // Make sure it ends in DIR_SEP.
  if (*user_path.rbegin() != DIR_SEP_CHR)
	user_path += DIR_SEP;

#elif defined(__APPLE__) || defined(ANDROID)

  if (File::Exists(ROOT_DIR DIR_SEP USERDATA_DIR))
  {
	user_path = ROOT_DIR DIR_SEP USERDATA_DIR DIR_SEP;
  }
  else
  {
	user_path = File::GetApplicationSupportDirectory() + "/User" DIR_SEP;
  }

#else

  // If there's a ./User/ directory wherever we've executed this (?)
  if (File::Exists(ROOT_DIR DIR_SEP USERDATA_DIR))
  {
	user_path = ROOT_DIR DIR_SEP USERDATA_DIR DIR_SEP;
  }
  // If there's a portable.txt, use User/ in the executable path
  else if (File::Exists(File::GetExeDirectory() + DIR_SEP "portable.txt"))
  {
	  user_path = File::GetExeDirectory() + DIR_SEP "User" DIR_SEP;
  }
  // Otherwise, just use some XDG paths to keep user data
  else 
  {
	const char* home = getenv("HOME");
	if (!home)
	  home = getenv("PWD");
	if (!home)
	  home = "";
	std::string home_path = std::string(home) + DIR_SEP;
  
	// Set the cache path to
	// ~/.cache/SlippiOnline/ for Netplay
	// ~/.cache/SlippiPlayback/ for Playback
	const char* cache_home = getenv("XDG_CACHE_HOME");
	// Set the user path to 
	// ~/.config/SlippiOnline/ for Netplay
	// ~/.config/SlippiPlayback/ for Playback
	const char *config_home = getenv("XDG_CONFIG_HOME");
#ifdef IS_PLAYBACK
	std::string cache_path = std::string(cache_home && cache_home[0] == '/' ? cache_home : (home_path + ".cache")) +
							 DIR_SEP PLAYBACK_DATA_DIR DIR_SEP;
	user_path = std::string(config_home && config_home[0] == '/' ? config_home : (home_path + ".config")) +
				DIR_SEP PLAYBACK_DATA_DIR DIR_SEP;
#else
	std::string cache_path = std::string(cache_home && cache_home[0] == '/' ? cache_home : (home_path + ".cache")) +
							 DIR_SEP DOLPHIN_DATA_DIR DIR_SEP;
	user_path = std::string(config_home && config_home[0] == '/' ? config_home : (home_path + ".config")) +
				DIR_SEP DOLPHIN_DATA_DIR DIR_SEP;
#endif
	File::SetUserPath(D_CACHE_IDX, cache_path);
  }

#endif
  File::SetUserPath(D_USER_IDX, user_path);
}

}  // namespace UICommon
