#include "SlippiUser.h"

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"

#include <json.hpp>
using json = nlohmann::json;

SlippiUser::~SlippiUser()
{
	// Wait for thread to terminate
	runThread = false;
	if (fileListenThread.joinable())
		fileListenThread.join();
}

void SlippiUser::OpenLogInPage()
{
	system("start chrome https://slippi.gg/online/enable");
}

void SlippiUser::ListenForLogIn()
{
	if (runThread)
		return;

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
	std::string userFilePath = getUserFilePath();

	while (runThread)
	{
		ERROR_LOG(SLIPPI_ONLINE, "Reading file at: %s", userFilePath.c_str());

		// Get user file
		std::string userFileContents;
		File::ReadFileToString(userFilePath, userFileContents);

		userInfo = parseFile(userFileContents);
		if (!userInfo.uid.empty())
		{
			ERROR_LOG(SLIPPI_ONLINE, "Found user %s (%s)", userInfo.displayName.c_str(), userInfo.uid.c_str());
			isLoggedIn = true;
			runThread = false;
			break;
		}

		isLoggedIn = false;
		Common::SleepCurrentThread(500);
	}
}

std::string SlippiUser::getUserFilePath()
{
	std::string dirPath = File::GetExeDirectory();
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

	return info;
}

void SlippiUser::deleteFile()
{
	std::string userFilePath = getUserFilePath();
	File::Delete(userFilePath);
}