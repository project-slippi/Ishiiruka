// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Added for Ishiiruka by Tino
#pragma once
#include <map>
#include "VideoCommon/NativeVertexFormat.h"
class G_SMNP01_pvt
{
public:
	static void Initialize(std::map<u64, TCompiledLoaderFunction> &pvlmap);
};
