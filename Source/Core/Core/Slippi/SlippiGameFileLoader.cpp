#include "SlippiGameFileLoader.h"

#include "Common/Logging/Log.h"

#include "Common/FileUtil.h"
#include "DiscIO/FileMonitor.h"

std::string getFilePath(std::string fileName)
{
	std::string dirPath = File::GetSysDirectory();
	std::string filePath = dirPath + "GameFiles/GALE01/" + fileName; // TODO: Handle other games?

	if (File::Exists(filePath))
	{
		return filePath;
	}

	filePath = filePath + ".diff";
	if (File::Exists(filePath))
	{
		return filePath;
	}

	return "";
}

u32 SlippiGameFileLoader::LoadFile(std::string fileName, std::string &data)
{
	if (fileCache.count(fileName))
	{
		data = fileCache[fileName];
		return (u32)data.size();
	}

	INFO_LOG(SLIPPI, "Loading file: %s", fileName.c_str());

	std::string gameFilePath = getFilePath(fileName);
	if (gameFilePath.empty())
	{
		fileCache[fileName] = "";
		data = "";
		return 0;
	}

	std::string fileContents;

	// Don't read MxDt.dat because our Launcher may not have successfully deleted it and
	// loading the old one from the file system would break m-ex based ISOs
	if (fileName != "MxDt.dat")
	{
		File::ReadFileToString(gameFilePath, fileContents);
	}

	if (gameFilePath.substr(gameFilePath.length() - 5) == ".diff")
	{
		// If the file was a diff file, load the main file from ISO and apply patch
		std::vector<u8> buf;
		INFO_LOG(SLIPPI, "Will process diff");
		FileMon::ReadFileWithName(fileName, buf);
		std::string diffContents = fileContents;

		decoder.Decode((char *)buf.data(), buf.size(), diffContents, &fileContents);
	}

	fileCache[fileName] = fileContents;
	data = fileCache[fileName];
	INFO_LOG(SLIPPI, "File size: %d", (u32)data.size());
	return (u32)data.size();
}
