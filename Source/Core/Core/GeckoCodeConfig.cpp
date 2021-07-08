// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <sstream>
#include <string>
#include <vector>

#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Core/GeckoCodeConfig.h"

namespace Gecko
{

// Populate some vector of Gecko codes from a particular INI file.
void ParseCodes(const IniFile &ini, std::vector<GeckoCode> &gcodes, bool is_user_ini)
{
	std::vector<std::string> lines;
	ini.GetLines("Gecko", &lines, false);
	GeckoCode gcode;

	for (auto &line : lines)
	{
		if (line.empty())
			continue;
		std::istringstream ss(line);
		switch ((line)[0])
		{
		// Start of a new entry
		case '$':
			if (gcode.name.size())
				gcodes.push_back(gcode);
			gcode = GeckoCode();
			gcode.user_defined = is_user_ini;
			ss.seekg(1, std::ios_base::cur);
			std::getline(ss, gcode.name, '[');
			gcode.name = StripSpaces(gcode.name);
			std::getline(ss, gcode.creator, ']');
			break;

		// Code notes/comments
		case '*':
			gcode.notes.push_back(std::string(++line.begin(), line.end()));
			break;

		// Line with actual gecko code contents
		default:
		{
			GeckoCode::Code new_code;
			new_code.original_line = line;
			ss >> std::hex >> new_code.address >> new_code.data;
			gcode.codes.push_back(new_code);
		}
		break;
		}
	}

	// Add the last code
	if (gcode.name.size())
		gcodes.push_back(gcode);
}

// For each line in the Gecko_Enabled section of the global INI file, mark all
// matching gecko codes as enabled, do the same for the user INI file but also handle disabling
void MarkEnabledCodes(const IniFile &globalIni, const IniFile &localIni, std::vector<GeckoCode> &gcodes)
{
	std::vector<std::string> globallines;
	std::vector<std::string> userlines;
	std::vector<std::string> userlines_disabled;
	std::vector<std::string> lines;
	globalIni.GetLines("Gecko_Enabled", &globallines, false);
	localIni.GetLines("Gecko_Enabled", &userlines, false);
	localIni.GetLines("Gecko_Disabled", &userlines_disabled, false);

	SetEnabledCodes(globallines, gcodes, true, true);
	SetEnabledCodes(userlines, gcodes, true);
	SetEnabledCodes(userlines_disabled, gcodes, false);
}

void SetEnabledCodes(const std::vector<std::string> lines, std::vector<GeckoCode> &gcodes, bool enable, bool is_default)
{
	for (const std::string &line : lines)
	{
		if (line.size() == 0)
			continue;
		std::string name = line.substr(1);
		switch (line[0])
		{
		case '$':
			for (GeckoCode &ogcode : gcodes)
			{
				if (ogcode.name == name)
				{
					ogcode.enabled = enable;
					if (is_default)
						ogcode.default_enabled = is_default;
				}
			}
			break;
		default:
			continue;
		}
	}
}

// Merge the global and local INIs into a single set of gecko codes.
// NOTE: This doesn't read any information about enabled codes.
void MergeCodes(const IniFile &globalIni, const IniFile &localIni, std::vector<GeckoCode> &working_set)
{
	std::vector<GeckoCode> global_codes;
	std::vector<GeckoCode> local_codes;

	// Obtain codes from global config and add them to the working set
	ParseCodes(globalIni, global_codes, false);
	for (GeckoCode &global_code : global_codes)
	{
		working_set.push_back(global_code);
	}

	// Obtain codes from local config and add them to the working set if they
	// don't collide with any global ones
	ParseCodes(localIni, local_codes, true);
	for (GeckoCode &local_code : local_codes)
	{
		bool conflict = false;
		for (GeckoCode &working_code : working_set)
		{
			if (working_code.name == local_code.name)
			{
				conflict = true;
				break;
			}
			else
			{
				continue;
			}
		}
		if (conflict == false)
		{
			working_set.push_back(local_code);
		}
		else
		{
			continue;
		}
	}
}

static std::string MakeGeckoCodeTitle(const GeckoCode &code)
{
	std::string title = '$' + code.name;

	if (!code.creator.empty())
	{
		title += " [" + code.creator + ']';
	}

	return title;
}

// Convert from a set of gecko codes to INI file contents (lines of text).
static void SaveGeckoCode(std::vector<std::string> &lines, const GeckoCode &gcode)
{
	if (!gcode.user_defined)
		return;

	lines.push_back(MakeGeckoCodeTitle(gcode));

	// save all the code lines
	for (const GeckoCode::Code &code : gcode.codes)
	{
		lines.push_back(code.original_line);
	}

	// save the notes
	for (const std::string &note : gcode.notes)
		lines.push_back('*' + note);
}

// Convert from a set of gecko codes to a whole INI file.
void SaveCodes(IniFile &inifile, const std::vector<GeckoCode> &gcodes)
{
	std::vector<std::string> lines;
	std::vector<std::string> enabled_lines;
	std::vector<std::string> disabled_lines;

	for (const GeckoCode &geckoCode : gcodes)
	{
		if (geckoCode.enabled != geckoCode.default_enabled)
			(geckoCode.enabled ? enabled_lines : disabled_lines).push_back('$' + geckoCode.name);

		SaveGeckoCode(lines, geckoCode);
	}

	// TODO: add a write flag to the Section class so we don't need this
	if (lines.size() == 0)
		lines.push_back("");
	inifile.SetLines("Gecko", lines);
	inifile.SetLines("Gecko_Enabled", enabled_lines);
	inifile.SetLines("Gecko_Disabled", disabled_lines);
}

} // namespace Gecko
