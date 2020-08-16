#pragma once

#include "Common/CommonTypes.h"
#include <atomic>
#include <curl/curl.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class SlippiUser
{
  public:
	struct UserInfo
	{
		std::string uid = "";
		std::string playKey = "";
		std::string displayName = "";
		std::string connectCode = "";
		std::string latestVersion = "";
		std::string fileContents = "";
	};

	SlippiUser();
	~SlippiUser();

	bool AttemptLogin();
	void OpenLogInPage();
	void UpdateApp();
	void ListenForLogIn();
	void LogOut();
	void OverwriteLatestVersion(std::string version);
	UserInfo GetUserInfo();
	UserInfo ReadUserInfo(bool assignResult);
	bool IsLoggedIn();
	void FileListenThread();
	void ChangeDisplayName(std::string name);
	UserInfo userInfo;

  protected:
	std::string getUserFilePath();
	UserInfo parseFile(std::string fileContents);
	void deleteFile();
	void overwriteFromServer();

	bool isLoggedIn = false;

	const std::string URL_START = "https://users-rest-dot-slippi.uc.r.appspot.com/user";
	CURL *m_curl = nullptr;
	struct curl_slist *m_curlHeaderList = nullptr;
	std::vector<char> receiveBuf;

	std::thread fileListenThread;
	std::atomic<bool> runThread;
};
