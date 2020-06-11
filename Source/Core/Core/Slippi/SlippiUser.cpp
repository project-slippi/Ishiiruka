#include "SlippiUser.h"

#ifdef _WIN32
#include "AtlBase.h"
#include "AtlConv.h"
#endif

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"

#include "Core/ConfigManager.h"

#include <json.hpp>
using json = nlohmann::json;

#ifdef _WIN32
#define MAX_SYSTEM_PROGRAM (4096)
static int system_hidden(const char *cmd)
{
	PROCESS_INFORMATION p_info;
	STARTUPINFO s_info;
	DWORD ReturnValue;

	memset(&s_info, 0, sizeof(s_info));
	memset(&p_info, 0, sizeof(p_info));
	s_info.cb = sizeof(s_info);

	wchar_t utf16cmd[MAX_SYSTEM_PROGRAM] = {0};
	MultiByteToWideChar(CP_UTF8, 0, cmd, -1, utf16cmd, MAX_SYSTEM_PROGRAM);
	if (CreateProcessW(NULL, utf16cmd, NULL, NULL, 0, CREATE_NO_WINDOW, NULL, NULL, &s_info, &p_info))
	{
		WaitForSingleObject(p_info.hProcess, INFINITE);
		GetExitCodeProcess(p_info.hProcess, &ReturnValue);
		CloseHandle(p_info.hProcess);
		CloseHandle(p_info.hThread);
	}
	return ReturnValue;
}
#endif

SlippiUser::~SlippiUser()
{
	// Wait for thread to terminate
	runThread = false;
	if (fileListenThread.joinable())
		fileListenThread.join();
}

bool SlippiUser::AttemptLogin()
{
	std::string userFilePath = getUserFilePath();

	ERROR_LOG(SLIPPI_ONLINE, "Looking for file at: %s", userFilePath.c_str());

	// Get user file
	std::string userFileContents;
	File::ReadFileToString(userFilePath, userFileContents);

	userInfo = parseFile(userFileContents);

	isLoggedIn = !userInfo.uid.empty();
	if (isLoggedIn)
	{
		ERROR_LOG(SLIPPI_ONLINE, "Found user %s (%s)", userInfo.displayName.c_str(), userInfo.uid.c_str());
	}

	return isLoggedIn;
}

void SlippiUser::OpenLogInPage()
{
#ifdef _WIN32
	std::string folderSep = "%5C";
#else
	std::string folderSep = "%2F";
#endif

	std::string url = "https://slippi.gg/online/enable";
	std::string path = getUserFilePath();
	path = ReplaceAll(path, "\\", folderSep);
	path = ReplaceAll(path, "/", folderSep);
	std::string fullUrl = url + "?path=" + path;

#ifdef _WIN32
	std::string command = "explorer \"" + fullUrl + "\"";
#else
	std::string command = "open \"" + fullUrl + "\""; // Does this work on linux?
#endif

	system(command.c_str());
}

void SlippiUser::UpdateFile()
{
#ifdef _WIN32
	std::string path = File::GetExeDirectory() + "/dolphin-slippi-tools.exe";
	std::string command = path + " user-update";
	system_hidden(command.c_str());
#endif
}

void SlippiUser::UpdateApp()
{
#ifdef _WIN32
	auto isoPath = SConfig::GetInstance().m_strFilename;

	std::string path = File::GetExeDirectory() + "/dolphin-slippi-tools.exe";
	std::string command = "start \"Updating Dolphin\" \"" + path + "\" app-update -launch -iso \"" + isoPath + "\"";
	WARN_LOG(SLIPPI, "Executing app update command: %s", command);
	system(command.c_str());
#endif
}

void SlippiUser::ListenForLogIn()
{
	if (runThread)
		return;

	if (fileListenThread.joinable())
	{
		ERROR_LOG(SLIPPI_ONLINE, "Waiting for previous thread termination...");
		fileListenThread.join();
	}

	ERROR_LOG(SLIPPI_ONLINE, "Starting user file thread...");
	runThread = true;
	fileListenThread = std::thread(&SlippiUser::FileListenThread, this);
}

void SlippiUser::LogOut()
{
	runThread = false;
	deleteFile();

	UserInfo emptyUser;
	isLoggedIn = false;
	userInfo = emptyUser;
}

SlippiUser::UserInfo SlippiUser::GetUserInfo()
{
	return userInfo;
}

bool SlippiUser::IsLoggedIn()
{
	return isLoggedIn;
}

void SlippiUser::FileListenThread()
{
	while (runThread)
	{
		if (AttemptLogin())
		{
			runThread = false;
			break;
		}

		Common::SleepCurrentThread(500);
	}

	ERROR_LOG(SLIPPI_ONLINE, "Thread is terminating");
}

std::string SlippiUser::getUserFilePath()
{
#if defined(__APPLE__)
	std::string dirPath = File::GetBundleDirectory() + "/Contents/Resources";
#else
	std::string dirPath = File::GetExeDirectory();
#endif
	std::string userFilePath = dirPath + DIR_SEP + "user.json";
	return userFilePath;
}

inline std::string readString(json obj, std::string key)
{
	auto item = obj.find(key);
	if (item == obj.end() || item.value().is_null())
	{
		return "";
	}

	return obj[key];
}

SlippiUser::UserInfo SlippiUser::parseFile(std::string fileContents)
{
	UserInfo info;
	info.fileContents = fileContents;

	auto res = json::parse(fileContents, nullptr, false);
	if (res.is_discarded() || !res.is_object())
	{
		return info;
	}

	info.uid = readString(res, "uid");
	info.displayName = readString(res, "displayName");
	info.playKey = readString(res, "playKey");
	info.connectCode = readString(res, "connectCode");
	info.latestVersion = readString(res, "latestVersion");

	return info;
}

void SlippiUser::deleteFile()
{
	std::string userFilePath = getUserFilePath();
	File::Delete(userFilePath);
}