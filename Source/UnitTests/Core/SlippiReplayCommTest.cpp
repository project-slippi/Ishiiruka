// Copyright 2026 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <gtest/gtest.h>

#include "Common/FileUtil.h"
#include "Core/Slippi/SlippiReplayComm.h"
#include "../TestUtils/TempFiles.h"

TEST(SlippiReplayComm, ReloadsCommFileWithinSameSecond)
{
	TestUtils::ScopedTempDir temp_dir;
	ASSERT_FALSE(temp_dir.GetPath().empty());

	const std::string path = TestUtils::JoinPath(temp_dir.GetPath(), "comm.json");
	ASSERT_TRUE(File::WriteStringToFile("{\"replay\":\"first.slp\",\"commandId\":\"first\"}", path));
	TestUtils::SetFileModificationTime(path, 1700000000ULL, 100000000ULL);

	SlippiReplayComm comm(path);
	EXPECT_TRUE(comm.isNewReplay());
	EXPECT_EQ("first.slp", comm.getSettings().replayPath);
	EXPECT_EQ("first", comm.getSettings().commandId);

	ASSERT_TRUE(File::WriteStringToFile("{\"replay\":\"second.slp\",\"commandId\":\"second\"}", path));
	TestUtils::SetFileModificationTime(path, 1700000000ULL, 200000000ULL);

	EXPECT_TRUE(comm.isNewReplay());
	EXPECT_EQ("second.slp", comm.getSettings().replayPath);
	EXPECT_EQ("second", comm.getSettings().commandId);
}
