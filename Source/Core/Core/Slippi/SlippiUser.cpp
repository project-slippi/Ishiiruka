#include "SlippiUser.h"

#ifdef _WIN32
#include "AtlBase.h"
#include "AtlConv.h"
#endif

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"

#include "Common/Common.h"
#include "Core/ConfigManager.h"

// #include "DolphinWX/Frame.h"
// #include "DolphinWX/Main.h"

#ifdef __APPLE__
#include "DolphinWX/SlippiAuthWebView/SlippiAuthWebView.h"
#endif

#include "VideoCommon/OnScreenDisplay.h"

#include <codecvt>
#include <locale>

#include <json.hpp>
using json = nlohmann::json;

#ifdef _WIN32
#define MAX_SYSTEM_PROGRAM (4096)
static void system_hidden(const char *cmd)
{
	PROCESS_INFORMATION p_info;
	STARTUPINFO s_info;

	memset(&s_info, 0, sizeof(s_info));
	memset(&p_info, 0, sizeof(p_info));
	s_info.cb = sizeof(s_info);

	wchar_t utf16cmd[MAX_SYSTEM_PROGRAM] = {0};
	MultiByteToWideChar(CP_UTF8, 0, cmd, -1, utf16cmd, MAX_SYSTEM_PROGRAM);
	if (CreateProcessW(NULL, utf16cmd, NULL, NULL, 0, CREATE_NO_WINDOW, NULL, NULL, &s_info, &p_info))
	{
		DWORD ExitCode;
		WaitForSingleObject(p_info.hProcess, INFINITE);
		GetExitCodeProcess(p_info.hProcess, &ExitCode);
		CloseHandle(p_info.hProcess);
		CloseHandle(p_info.hThread);
	}
}
#endif

static void RunSystemCommand(const std::string &command)
{
#ifdef _WIN32
	_wsystem(UTF8ToUTF16(command).c_str());
#else
	system(command.c_str());
#endif
}

static size_t receive(char *ptr, size_t size, size_t nmemb, void *rcvBuf)
{
	size_t len = size * nmemb;
	INFO_LOG(SLIPPI_ONLINE, "[User] Received data: %d", len);

	std::string *buf = (std::string *)rcvBuf;

	buf->insert(buf->end(), ptr, ptr + len);

	return len;
}

SlippiUser::SlippiUser()
{
	CURL *curl = curl_easy_init();
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &receive);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000);

		// Set up HTTP Headers
		m_curlHeaderList = curl_slist_append(m_curlHeaderList, "Content-Type: application/json");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, m_curlHeaderList);

#ifdef _WIN32
		// ALPN support is enabled by default but requires Windows >= 8.1.
		curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, false);
#endif

		m_curl = curl;
	}
}

SlippiUser::~SlippiUser()
{
	// Wait for thread to terminate
	runThread = false;
	if (fileListenThread.joinable())
		fileListenThread.join();

	if (m_curl)
	{
		curl_slist_free_all(m_curlHeaderList);
		curl_easy_cleanup(m_curl);
	}
}

bool SlippiUser::AttemptLogin()
{
	std::string userFilePath = File::GetSlippiUserJSONPath();

	//INFO_LOG(SLIPPI_ONLINE, "Looking for file at: %s", userFilePath.c_str());

	{
		std::string userFilePathTxt =
		    userFilePath + ".txt"; // Put the filename here in its own scope because we don't really need it elsewhere
		if (File::Exists(userFilePathTxt))
		{
			// If both files exist we just log they exist and take no further action
			if (File::Exists(userFilePath))
			{
				INFO_LOG(SLIPPI_ONLINE,
				         "Found both .json.txt and .json file for user data. Using .json and ignoring the .json.txt");
			}
			// If only the .txt file exists move the contents to a json file and log if it fails
			else if (!File::Rename(userFilePathTxt, userFilePath))
			{
				WARN_LOG(SLIPPI_ONLINE, "Could not move file %s to %s", userFilePathTxt.c_str(), userFilePath.c_str());
			}
		}
	}

	// Get user file
	std::string userFileContents;
	File::ReadFileToString(userFilePath, userFileContents);

	userInfo = parseFile(userFileContents);

	isLoggedIn = !userInfo.uid.empty();
	if (isLoggedIn)
	{
		overwriteFromServer();
		WARN_LOG(SLIPPI_ONLINE, "Found user %s (%s)", userInfo.displayName.c_str(), userInfo.uid.c_str());
	}

	return isLoggedIn;
}

// On macOS, this will pop open a built-in webview to handle authentication. This is likely to see less and less
// use over time but should hang around for a bit longer; macOS in particular benefits from having this for some
// testing scenarios due to the cumbersome user.json location placement on that system.
//
// Windows and Linux don't have reliable WebView components, so this just pops the user over to slippi.gg for those
// platforms.
void SlippiUser::OpenLogInPage()
{
#ifdef __APPLE__
	CFrame *cframe = wxGetApp().GetCFrame();
	cframe->OpenSlippiAuthenticationDialog();
#else
	std::string url = "https://slippi.gg/online/enable";
	std::string path = File::GetSlippiUserJSONPath();

#ifdef _WIN32
	// On windows, sometimes the path can have backslashes and slashes mixed, convert all to backslashes
	path = ReplaceAll(path, "\\", "\\");
	path = ReplaceAll(path, "/", "\\");
#endif

	std::string fullUrl = url + "?path=" + path;
	INFO_LOG(SLIPPI_ONLINE, "[User] Login at path: %s", fullUrl.c_str());

#ifdef _WIN32
	std::string command = "explorer \"" + fullUrl + "\"";
#else
	std::string command = "xdg-open \"" + fullUrl + "\""; // Linux
#endif

	RunSystemCommand(command);
#endif
}

bool SlippiUser::UpdateApp()
{
#ifdef _WIN32
	auto isoPath = SConfig::GetInstance().m_strFilename;

	std::string path = File::GetExeDirectory() + "/dolphin-slippi-tools.exe";
	std::string echoMsg = "echo Starting update process. If nothing happens after a few "
	                      "minutes, you may need to update manually from https://slippi.gg/netplay ...";

	// Check if updater exists, anti-virus sometimes deletes it
	if (!File::Exists(path))
	{
		ERROR_LOG(SLIPPI_ONLINE, "Update requested but updater does not exist.");
		OSD::AddMessage("Updater cannot be found. Please download the latest Slippi version from slippi.gg.", 30000, 0xFFFF0000);
		return false;
	}

	std::string command = "start /b cmd /c " + echoMsg + " && \"" + path + "\" app-update -launch -iso \"" + isoPath +
	                      "\" -version \"" + scm_slippi_semver_str + "\"";
	WARN_LOG(SLIPPI, "Executing app update command: %s", command);
	RunSystemCommand(command);
	return true;
#elif defined(__APPLE__)
	CriticalAlertT(
		"Automatic updates are not available for standalone Netplay builds on macOS. Please get the latest update from slippi.gg/netplay. "
		"(The Slippi Launcher has automatic updates on macOS, and you should consider switching to that)"
	);
	return false;
#else
	const char *appimage_path = getenv("APPIMAGE");
	const char *appmount_path = getenv("APPDIR");
	if (!appimage_path)
	{
		CriticalAlertT("Automatic updates are not available for non-AppImage Linux builds.");
		return false;
	}
	std::string path(appimage_path);
	std::string mount_path(appmount_path);
	std::string command = mount_path + "/usr/bin/appimageupdatetool " + path;
	WARN_LOG(SLIPPI, "Executing app update command: %s", command.c_str());
	RunSystemCommand(command);
	CriticalAlertT("Restart Dolphin to finish the update. If there was an issue, please head over to the Slippi "
	               "Discord for support.");
	return true;
#endif
}

void SlippiUser::ListenForLogIn()
{
	if (runThread)
		return;

	if (fileListenThread.joinable())
		fileListenThread.join();

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

void SlippiUser::OverwriteLatestVersion(std::string version)
{
	userInfo.latestVersion = version;
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
			// main_frame->RaiseRenderWindow();
			break;
		}

		Common::SleepCurrentThread(500);
	}
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
	std::string userFilePath = File::GetSlippiUserJSONPath();
	File::Delete(userFilePath);
}

void SlippiUser::overwriteFromServer()
{
	if (!m_curl)
		return;

	// Generate URL. If this is a beta version, use the beta endpoint
	std::string url = URL_START;
	if (scm_slippi_semver_str.find("beta") != std::string::npos)
	{
		url = url + "-beta";
	}

	ERROR_LOG(SLIPPI_ONLINE, "URL: %s", url.c_str());

	// Perform curl request
	std::string resp;
	curl_easy_setopt(m_curl, CURLOPT_URL, (url + "/" + userInfo.uid).c_str());
	curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &resp);
	CURLcode res = curl_easy_perform(m_curl);

	if (res != 0)
	{
		ERROR_LOG(SLIPPI, "[User] Error fetching user info from server, code: %d", res);
		return;
	}

	long responseCode;
	curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &responseCode);
	if (responseCode != 200)
	{
		ERROR_LOG(SLIPPI, "[User] Server responded with non-success status: %d", responseCode);
		return;
	}

	// Overwrite userInfo with data from server
	auto r = json::parse(resp);
	userInfo.connectCode = r.value("connectCode", userInfo.connectCode);
	userInfo.latestVersion = r.value("latestVersion", userInfo.latestVersion);
	userInfo.displayName = r.value("displayName", userInfo.displayName);
}
