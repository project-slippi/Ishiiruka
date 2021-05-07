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

		int port;
	};

	SlippiUser();
	~SlippiUser();

	bool AttemptLogin();
	void OpenLogInPage();
	bool UpdateApp();
	void ListenForLogIn();
	void LogOut();
	void OverwriteLatestVersion(std::string version);
	UserInfo GetUserInfo();
	bool IsLoggedIn();
	void FileListenThread();

  protected:
	UserInfo parseFile(std::string fileContents);
	void deleteFile();
	void overwriteFromServer();

	UserInfo userInfo;
	bool isLoggedIn = false;

	const std::string URL_START = "https://users-rest-dot-slippi.uc.r.appspot.com/user";
	CURL *m_curl = nullptr;
	struct curl_slist *m_curlHeaderList = nullptr;
	std::vector<char> receiveBuf;

	std::thread fileListenThread;
	std::atomic<bool> runThread;
};
