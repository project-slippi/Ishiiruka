// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "Common/Common.h"
#include "VideoCommon/ImageLoader.h"

bool SaveData(const std::string& filename, const std::string& data);
bool TextureToPng(const u8* data, int row_stride, const std::string& filename, int width,
	int height, bool saveAlpha = false, bool frombgra = false);
bool TextureToDDS(const u8* data, int row_stride, const std::string& filename, int width, int height, DDSCompression format = DDSCompression::DDSC_DXT3);
