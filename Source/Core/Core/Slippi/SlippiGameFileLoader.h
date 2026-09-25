#pragma once

#include "Common/CommonTypes.h"
#include <open-vcdiff/src/google/vcdecoder.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <set>

class SlippiGameFileLoader
{
  public:
	u32 LoadFile(std::string fileName, std::string &contents);

	// static stadium transformation files
	std::set<std::string> grpsx_strings = {"GrPs1.dat", "GrPs2.dat", "GrPs3.dat", "GrPs4.dat"};

  protected:
	// Replaces the slp: links in an archive with the assets they name, read from the disc. Returns
	// false and empties contents when a link could not be resolved
	bool resolveLinks(const std::string &fileName, std::string &contents);
	static bool readDiscFile(void *ctx, const char *fileName, const u8 **outData, uintptr_t *outLen);

	std::unordered_map<std::string, std::string> fileCache;
	open_vcdiff::VCDiffDecoder decoder;

	// Holds the disc file most recently handed to Rust by readDiscFile
	std::vector<u8> discReadBuffer;
};
