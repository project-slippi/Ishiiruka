// Copyright 2026 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Slippi/SlippiNetplayTiming.h"

#include <algorithm>

s32 CalcSlippiPlayerTimeOffsetUs(std::vector<s32> offsets)
{
	if (offsets.empty())
		return 0;

	std::sort(offsets.begin(), offsets.end());

	const int bufSize = static_cast<int>(offsets.size());
	const int offset = static_cast<int>((1.0f / 3.0f) * bufSize);
	const int end = bufSize - offset;

	s64 sum = 0;
	for (int i = offset; i < end; i++)
		sum += offsets[i];

	const int count = end - offset;
	if (count <= 0)
		return 0;

	return static_cast<s32>(sum / count);
}

s32 SelectSlippiTimeOffsetUs(const std::vector<SlippiTimeOffset>& offsets)
{
	if (offsets.empty())
		return 0;

	bool hasHumanOffset = false;
	for (const auto& offset : offsets)
	{
		if (!offset.isBot)
		{
			hasHumanOffset = true;
			break;
		}
	}

	bool hasSelectedOffset = false;
	s32 minOffset = 0;
	for (const auto& offset : offsets)
	{
		if (hasHumanOffset && offset.isBot)
			continue;

		if (!hasSelectedOffset || offset.offsetUs < minOffset)
		{
			minOffset = offset.offsetUs;
			hasSelectedOffset = true;
		}
	}

	return minOffset;
}
