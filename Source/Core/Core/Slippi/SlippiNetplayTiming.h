// Copyright 2026 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

#include <vector>

struct SlippiTimeOffset
{
	s32 offsetUs;
	bool isBot;
};

s32 CalcSlippiPlayerTimeOffsetUs(std::vector<s32> offsets);
s32 SelectSlippiTimeOffsetUs(const std::vector<SlippiTimeOffset>& offsets);
