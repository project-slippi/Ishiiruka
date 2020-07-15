#include "SlippiDirectCodes.h"

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

#include "Core/ConfigManager.h"

#include <codecvt>
#include <locale>

#include <json.hpp>
using json = nlohmann::json;

SlippiDirectCodes::SlippiDirectCodes()
{
    if (directCodeInfos.empty())
        ReadFile();
}

SlippiDirectCodes::~SlippiDirectCodes()
{
    return;
}

void SlippiDirectCodes::ReadFile()
{
    std::string directCodesFilePath = getCodesFilePath();

    INFO_LOG(SLIPPI_ONLINE, "Looking for direct codes file at %s", directCodesFilePath);

    if (!File::Exists(directCodesFilePath))
    {
        if (File::CreateEmptyFile(directCodesFilePath))
        {
            File::WriteStringToFile("[\n]", directCodesFilePath);
        }
        else 
        {
            WARN_LOG(SLIPPI_ONLINE, "Was unable to create %s", directCodesFilePath);
        }
    }

    std::string directCodesFileContents;
    File::ReadFileToString(directCodesFilePath, directCodesFileContents);

    directCodeInfos = parseFile(directCodesFileContents);
}

void SlippiDirectCodes::AddOrUpdateCode(std::string code)
{
    WARN_LOG(SLIPPI_ONLINE, "Attempting to add or update direct code: %s", code);
    bool found = false;
    for (auto it = directCodeInfos.begin(); it != directCodeInfos.end(); ++it)
    {
       if (it->connectCode == code)
       {
           found = true;
           // TODO: Update timestamp
       }
    }
    
    if (!found)
    {
        INFO_LOG(SLIPPI_ONLINE, "Reached new entry");
        CodeInfo newDirectCode = {code, "today", false};
        directCodeInfos.push_back(newDirectCode);
    }

    // TODO: Remove from here. Or start a thread that is periodically called
    INFO_LOG(SLIPPI_ONLINE, "Attempting to write to file.");
    WriteFile();
}

void SlippiDirectCodes::WriteFile()
{
    std::string directCodesFilePath = getCodesFilePath();

    // Outer empty array.
    json fileData = json::array();
    // Inner contents.
    json directCodeData = json::object();

    // TODO Define constants for string literals. 
    for (auto it = directCodeInfos.begin(); it != directCodeInfos.end(); ++it)
    {
        directCodeData["name"] = it->connectCode;
        directCodeData["lastPlayed"] = it->lastPlayed;
        directCodeData["isFavorite"] = it->isFavorite;

        INFO_LOG(SLIPPI_ONLINE, "Reached pushing back data");

        fileData.push_back(directCodeData);
    }    

    INFO_LOG(SLIPPI_ONLINE, "Dumping file contents %s", fileData.dump());
    File::WriteStringToFile(fileData.dump(), directCodesFilePath);
}

std::string SlippiDirectCodes::getCodesFilePath()
{
#if defined(__APPLE__)
	std::string dirPath = File::GetBundleDirectory() + "/Contents/Resources";
#elif defined(_WIN32)
	std::string dirPath = File::GetExeDirectory();
#else
	std::string dirPath = File::GetSysDirectory();
	dirPath.pop_back();
#endif
	std::string directCodesFilePath = dirPath + DIR_SEP + "directcodes.json";
	return directCodesFilePath;
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

inline bool readBool(json obj, std::string key)
{
    auto item = obj.find(key);
    if (item == obj.end() || item.value().is_null())
    {
        return false;
    }

    return obj[key];
}

std::vector<SlippiDirectCodes::CodeInfo> SlippiDirectCodes::parseFile(std::string fileContents)
{
    std::vector<SlippiDirectCodes::CodeInfo> directCodes;

    json res = json::parse(fileContents, nullptr, false);
    // Unlike the user.json, the encapsulating type should be an array.
    if (res.is_discarded() || !res.is_array())
    {
        return directCodes;
    }

    // Retrieve all saved direct codes
    for (auto it = res.begin(); it != res.end(); ++it) 
    {
        if (it.value().is_array())
        {
           CodeInfo curDirectCode;
           curDirectCode.connectCode = readString(*it, "connectCode");
           curDirectCode.lastPlayed = readString(*it, "lastPlayed");
           curDirectCode.isFavorite = readBool(*it, "favorite");

           directCodes.push_back(curDirectCode);
        }
    }

    return directCodes;
}