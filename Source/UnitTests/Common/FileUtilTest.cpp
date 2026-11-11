// Copyright 2026 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <gtest/gtest.h>

#include "Common/FileUtil.h"
#include "../TestUtils/TempFiles.h"

TEST(FileUtil, GetFileModTimeTracksSubsecondChanges)
{
	TestUtils::ScopedTempDir temp_dir;
	ASSERT_FALSE(temp_dir.GetPath().empty());

	const std::string path = TestUtils::JoinPath(temp_dir.GetPath(), "comm.json");
	ASSERT_TRUE(File::WriteStringToFile("one", path));
	TestUtils::SetFileModificationTime(path, 1700000000ULL, 100000000ULL);

	const u64 first_mod_time = File::GetFileModTime(path);
	ASSERT_NE(0ULL, first_mod_time);

	ASSERT_TRUE(File::WriteStringToFile("two", path));
	TestUtils::SetFileModificationTime(path, 1700000000ULL, 200000000ULL);

	const u64 second_mod_time = File::GetFileModTime(path);
	EXPECT_NE(first_mod_time, second_mod_time);
}
