// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "Core/GeckoCode.h"

class IniFile;

namespace Gecko
{

// For each code marked as "bootstrap" in 'global_codes', add a matching line
// to the [Gecko_Enabled] section in 'local_ini'.
void BootstrapLocalConfig(IniFile& local_ini, std::vector<GeckoCode>& global_codes);

// For each enabled code in 'globalIni, mark the matching code as a "bootstrap" code.
void MarkBootstrapCodes(const IniFile& globalIni, std::vector<GeckoCode>& gcodes);

// For each enabled code in 'someIni', enable the matching code in 'gcodes'.
void MarkEnabledCodes(const IniFile& globalIni, const IniFile& localIni, std::vector<GeckoCode>& gcodes);

// Parse an 'ini', producing a set 'gcodes'.
void ParseCodes(const IniFile& ini, std::vector<GeckoCode>& gcodes, bool is_user_ini);

// Merge 'globalIni' and 'localIni' to produce a working set 'gcodes'.
void MergeCodes(const IniFile& globalIni, const IniFile& localIni, std::vector<GeckoCode>& gcodes);

// Fill the contents of 'iniFile' with 'gcodes' (this does not flush to disk).
void FillIni(IniFile& inifile, const std::vector<GeckoCode>& gcodes);

}
