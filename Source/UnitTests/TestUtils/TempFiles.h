// Copyright 2026 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <gtest/gtest.h>

#include <string>

#ifdef _WIN32
#include <windows.h>

#include "Common/StringUtil.h"
#else
#include <sys/time.h>
#endif

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"

namespace TestUtils
{
class ScopedTempDir
{
public:
	ScopedTempDir() : m_path(File::CreateTempDir()) {}
	ScopedTempDir(const ScopedTempDir &) = delete;
	ScopedTempDir &operator=(const ScopedTempDir &) = delete;
	~ScopedTempDir()
	{
		if (!m_path.empty())
			File::DeleteDirRecursively(m_path);
	}

	const std::string &GetPath() const { return m_path; }

private:
	std::string m_path;
};

inline std::string JoinPath(const std::string &directory, const std::string &filename)
{
	return directory + DIR_SEP + filename;
}

inline void SetFileModificationTime(const std::string &path, const u64 seconds,
                                    const u64 nanoseconds)
{
#ifdef _WIN32
	const u64 windows_ticks_per_second = 10000000ULL;
	const u64 windows_unix_epoch_delta_seconds = 11644473600ULL;

	ULARGE_INTEGER timestamp;
	timestamp.QuadPart = (seconds + windows_unix_epoch_delta_seconds) *
	                         windows_ticks_per_second +
	                     nanoseconds / 100;

	FILETIME file_time;
	file_time.dwLowDateTime = timestamp.LowPart;
	file_time.dwHighDateTime = timestamp.HighPart;

	HANDLE file =
	    CreateFile(UTF8ToTStr(path).c_str(), GENERIC_WRITE,
	               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
	               FILE_ATTRIBUTE_NORMAL, nullptr);
	ASSERT_TRUE(file != INVALID_HANDLE_VALUE);
	EXPECT_TRUE(SetFileTime(file, nullptr, nullptr, &file_time));
	CloseHandle(file);
#else
	timeval times[2] = {};
	times[0].tv_sec = static_cast<time_t>(seconds);
	times[0].tv_usec = static_cast<suseconds_t>(nanoseconds / 1000);
	times[1] = times[0];

	ASSERT_EQ(0, utimes(path.c_str(), times));
#endif
}
}
