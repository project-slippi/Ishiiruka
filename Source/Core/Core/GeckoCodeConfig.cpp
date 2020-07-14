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
void MarkEnabledCodes(const IniFile& globalIni, const IniFile &localIni, std::vector<GeckoCode> &gcodes)
{
	std::vector<std::string> globallines;
	std::vector<std::string> userlines;
	std::vector<std::string> lines;
	globalIni.GetLines("Gecko_Enabled", &globallines, false);
	localIni.GetLines("Gecko_Enabled", &userlines, false);
	lines.reserve(globallines.size() + userlines.size());
	lines.insert(lines.end(), globallines.begin(), globallines.end());
	lines.insert(lines.end(), userlines.begin(), userlines.end());
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
					ogcode.enabled = true;
				}
			}
			break;
		case '-':
			for (GeckoCode &ogcode : gcodes)
			{
				if (ogcode.name == name)
				{
					ogcode.enabled = false;
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

// Convert from a set of gecko codes to INI file contents (lines of text).
static void FillLines(std::vector<std::string> &lines, std::vector<std::string> &enabledLines, const GeckoCode &gcode)
{
	if (gcode.enabled)
		enabledLines.push_back("$" + gcode.name);
	else
		enabledLines.push_back("-" + gcode.name);

	if (!gcode.user_defined)
		return;

	std::string name;

	// save the name
	name += '$';
	name += gcode.name;

	// save the creator name
	if (gcode.creator.size())
	{
		name += " [";
		name += gcode.creator;
		name += ']';
	}

	lines.push_back(name);

	// save all the code lines
	for (const GeckoCode::Code &code : gcode.codes)
	{
		lines.push_back(code.original_line);
	}

	// save the notes
	for (const std::string &note : gcode.notes)
		lines.push_back(std::string("*") + note);
}

// Convert from a set of gecko codes to a whole INI file.
void FillIni(IniFile &inifile, const std::vector<GeckoCode> &gcodes)
{
	std::vector<std::string> lines;
	std::vector<std::string> enabledLines;

	for (const GeckoCode &geckoCode : gcodes)
	{
		FillLines(lines, enabledLines, geckoCode);
	}

	// TODO: add a write flag to the Section class so we don't need this
	if (lines.size() == 0)
		lines.push_back("");
	inifile.SetLines("Gecko", lines);
	inifile.SetLines("Gecko_Enabled", enabledLines);
}

} // namespace Gecko
