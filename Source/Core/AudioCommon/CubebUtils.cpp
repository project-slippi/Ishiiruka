// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdarg>
#include <cstddef>
#include <cstring>

#include "AudioCommon/CubebUtils.h"
#include "Common/CommonPaths.h"
#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include "Common/StringUtil.h"

#include <cubeb/cubeb.h>

static ptrdiff_t s_path_cutoff_point = 0;

static void LogCallback(const char* format, ...)
{
  if (!LogManager::GetInstance())
    return;

  va_list args;
  va_start(args, format);

  std::string adapted_format = StripSpaces(format + strlen("%s:%d:"));

  va_end(args);
}

static void DestroyContext(cubeb* ctx)
{
  cubeb_destroy(ctx);
  if (cubeb_set_log_callback(CUBEB_LOG_DISABLED, nullptr) != CUBEB_OK)
  {
    ERROR_LOG(AUDIO, "Error removing cubeb log callback");
  }
}

std::shared_ptr<cubeb> CubebUtils::GetContext()
{
  static std::weak_ptr<cubeb> weak;

  std::shared_ptr<cubeb> shared = weak.lock();
  // Already initialized
  if (shared)
    return shared;

  const char* filename = __FILE__;
  const char* match_point = strstr(filename, DIR_SEP "Source" DIR_SEP "Core" DIR_SEP);
  if (match_point)
  {
    s_path_cutoff_point = match_point - filename + strlen(DIR_SEP "Externals" DIR_SEP);
  }
  if (cubeb_set_log_callback(CUBEB_LOG_NORMAL, LogCallback) != CUBEB_OK)
  {
    ERROR_LOG(AUDIO, "Error setting cubeb log callback");
  }

  cubeb* ctx;
  if (cubeb_init(&ctx, "Dolphin", nullptr) != CUBEB_OK)
  {
    ERROR_LOG(AUDIO, "Error initializing cubeb library");
    return nullptr;
  }
  INFO_LOG(AUDIO, "Cubeb initialized using %s backend", cubeb_get_backend_id(ctx));

  weak = shared = {ctx, DestroyContext};
  return shared;
}
