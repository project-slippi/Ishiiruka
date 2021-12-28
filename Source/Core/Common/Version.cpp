// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Common/scmrev.h"

#ifdef _DEBUG
#define BUILD_TYPE_STR "Debug "
#elif defined DEBUGFAST
#define BUILD_TYPE_STR "DebugFast "
#else
#define BUILD_TYPE_STR ""
#endif

// const std::string scm_rev_str = "Ishiiruka-Dolphin"
//#if !SCM_IS_MASTER
//"[" SCM_BRANCH_STR "] "
//#endif
//
//#ifdef __INTEL_COMPILER
//" " BUILD_TYPE_STR " " SCM_DESC_STR "-ICC";
//#else
//" " BUILD_TYPE_STR " " SCM_DESC_STR;
//#endif
#ifndef IS_PLAYBACK
#define SLIPPI_REV_STR "2.3.3" // netplay version
#else
#define SLIPPI_REV_STR "2.3.7" // playback version
#endif
#ifdef IS_PLAYBACK
const std::string scm_rev_str = "Faster Melee - Slippi (" SLIPPI_REV_STR ") - Playback";
#else
const std::string scm_rev_str = "Faster Melee - Slippi (" SLIPPI_REV_STR ")";
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
