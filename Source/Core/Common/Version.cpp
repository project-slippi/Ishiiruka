// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Common/scmrev.h"

// release commit notes
// if releasing both builds, use `release: {combined_version}` or `release: {netplay_version} | {playback_version}`
// for releasing a single build, use `release(netplay): {netplay_version}` or `release(playback): {playback_version}`
#ifndef IS_PLAYBACK
#define SLIPPI_REV_STR "3.6.2" // netplay version
#else
#define SLIPPI_REV_STR "3.6.0" // playback version
#endif

#ifdef DEBUGFAST
#define DEBUGFAST_STR " [DEBUGFAST]"
#elif _DEBUG
#define DEBUGFAST_STR " [DEBUG]"
#else
#define DEBUGFAST_STR ""
#endif

#ifdef IS_PLAYBACK
const std::string scm_rev_str = "Faster Melee - Slippi (" SLIPPI_REV_STR ") - Playback" DEBUGFAST_STR;
#else
const std::string scm_rev_str = "Faster Melee - Slippi (" SLIPPI_REV_STR ")" DEBUGFAST_STR;
#endif
const std::string scm_slippi_semver_str = SLIPPI_REV_STR;

#ifdef _WIN32
const std::string netplay_dolphin_ver = "Slippi-" SLIPPI_REV_STR " Win";
#elif __APPLE__
const std::string netplay_dolphin_ver = "Slippi-" SLIPPI_REV_STR " Mac";
#else
const std::string netplay_dolphin_ver = "Slippi-" SLIPPI_REV_STR " Lin";
#endif

const std::string scm_rev_git_str = SCM_REV_STR;
const std::string scm_rev_cache_str = "201705092203";
const std::string scm_desc_str = SCM_DESC_STR;
const std::string scm_branch_str = SCM_BRANCH_STR;
const std::string scm_distributor_str = SCM_DISTRIBUTOR_STR;
