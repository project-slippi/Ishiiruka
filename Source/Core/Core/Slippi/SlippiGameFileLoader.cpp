#include "SlippiGameFileLoader.h"

#include "SlippiRustExtensions.h"

#include "Common/Logging/Log.h"

#include "Common/FileUtil.h"
#include "Common/Timer.h"
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

bool SlippiGameFileLoader::readDiscFile(void *ctx, const char *fileName, const u8 **outData, uintptr_t *outLen)
{
	auto *loader = static_cast<SlippiGameFileLoader *>(ctx);
	std::string name(fileName);
	FileMon::ReadFileWithName(name, loader->discReadBuffer);

	*outData = loader->discReadBuffer.data();
	*outLen = loader->discReadBuffer.size();
	return !loader->discReadBuffer.empty();
}

bool SlippiGameFileLoader::resolveLinks(const std::string &fileName, std::string &contents)
{
	u32 startMs = Common::Timer::GetTimeMs();

	SlippiResolvedGameFile result = slprs_gamefile_resolve(fileName.c_str(), (const u8 *)contents.data(), contents.size(),
	                                                       this, &SlippiGameFileLoader::readDiscFile);
	discReadBuffer.clear();
	discReadBuffer.shrink_to_fit();

	if (result.status == SLPRS_GAMEFILE_NO_LINKS)
	{
		return true;
	}

	if (result.status != SLPRS_GAMEFILE_RESOLVED)
	{
		// Serve nothing rather than a file with dangling links. The reason was logged by Rust
		ERROR_LOG(SLIPPI, "Serving nothing for %s because its links could not be resolved", fileName.c_str());
		contents.clear();
		return false;
	}

	contents.assign((const char *)result.data, result.len);
	slprs_gamefile_free(result);
	INFO_LOG(SLIPPI, "Resolved links in %s, %u bytes, took %u ms", fileName.c_str(), (u32)contents.size(),
	         Common::Timer::GetTimeMs() - startMs);

	// Drop a copy for offline comparison when the user has created the dump folder
	std::string dumpDir = File::GetUserPath(D_DUMP_IDX) + "GameFiles/";
	if (File::IsDirectory(dumpDir))
	{
		std::string dumpPath = dumpDir + fileName;
		if (File::WriteStringToFile(contents, dumpPath))
			INFO_LOG(SLIPPI, "Wrote resolved %s to %s", fileName.c_str(), dumpPath.c_str());
	}
	else
	{
		INFO_LOG(SLIPPI, "Create %s to dump resolved game files", dumpDir.c_str());
	}

	return true;
}

u32 SlippiGameFileLoader::LoadFile(std::string fileName, std::string &data)
{
	if (fileCache.count(fileName))
	{
		data = fileCache[fileName];
		return (u32)data.size();
	}

	if (grpsx_strings.count(fileName))
	{
		std::vector<u8> buf;
		FileMon::ReadFileWithName(fileName, buf);
		std::string contents(buf.begin(), buf.end());

		fileCache[fileName] = contents;
		data = fileCache[fileName];
		INFO_LOG(SLIPPI, "Preloaded Transformation: %s -> %d", fileName.c_str(), (u32)data.size());
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

	// Pull in any assets the file links to on the disc
	if (!fileContents.empty())
	{
		resolveLinks(fileName, fileContents);
	}

	fileCache[fileName] = fileContents;
	data = fileCache[fileName];
	INFO_LOG(SLIPPI, "File size: %d", (u32)data.size());
	return (u32)data.size();
}
