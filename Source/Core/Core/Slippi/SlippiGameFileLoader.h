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
	std::unordered_map<std::string, std::string> fileCache;
	open_vcdiff::VCDiffDecoder decoder;
};
