// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace UICommon
{

void Init();
void Shutdown();

void CreateDirectories();
void SetUserDirectory(const std::string& custom_path);

void RaiseRenderWindow();
void LowerRenderWindow();

} // namespace UICommon
