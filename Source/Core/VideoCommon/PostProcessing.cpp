// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VideoConfig.h"


static const char s_default_shader[] = "void main() { SetOutput(ApplyGCGamma(Sample())); }\n";

PostProcessingShaderImplementation::PostProcessingShaderImplementation()
{
	m_timer.Start();
}

PostProcessingShaderImplementation::~PostProcessingShaderImplementation()
{
	m_timer.Stop();
}

std::string PostProcessingShaderConfiguration::LoadShader(std::string shader)
{
	// Load the shader from the configuration if there isn't one sent to us.
	if (shader == "")
		shader = g_ActiveConfig.sPostProcessingShader;
	m_current_shader = shader;

	const std::string sub_dir = (g_Config.iStereoMode == STEREO_ANAGLYPH || g_Config.iStereoMode == STEREO_INTERLACED) ? ANAGLYPH_DIR DIR_SEP : "";

	// loading shader code
	std::string code;
	std::string path = File::GetUserPath(D_SHADERS_IDX) + sub_dir + shader + ".glsl";

	if (shader == "")
	{
		code = s_default_shader;
	}
	else
	{
		if (!File::Exists(path))
		{
			// Fallback to shared user dir
			path = File::GetSysDirectory() + SHADERS_DIR DIR_SEP + sub_dir + shader + ".glsl";
		}

		if (!File::ReadFileToString(path, code))
		{
			ERROR_LOG(VIDEO, "Post-processing shader not found: %s", path.c_str());
			code = s_default_shader;
		}
	}

	code = LoadOptions(code);
	LoadOptionsConfiguration();

	return code;
}

std::string PostProcessingShaderConfiguration::LoadOptions(const std::string& code)
{
	const std::string config_start_delimiter = "[configuration]";
	const std::string config_end_delimiter = "[/configuration]";
	size_t configuration_start = code.find(config_start_delimiter);
	size_t configuration_end = code.find(config_end_delimiter);
	m_stages.clear();
	m_options.clear();
	m_any_options_dirty = true;
	m_requires_depth_input = code.find("SampleDepth") != std::string::npos;
	if (configuration_start == std::string::npos ||
		configuration_end == std::string::npos)
	{
		// Issue loading configuration or there isn't one.
		//Add Default Stage
		StageOption option;
		option.m_stage_entry_point = "main";
		option.m_outputScale = 1.0f;
		m_stages.push_back(option);
		return code;
	}

	std::string configuration_string = code.substr(configuration_start + config_start_delimiter.size(),
		configuration_end - configuration_start - config_start_delimiter.size());

	std::istringstream in(configuration_string);

	struct GLSLStringOption
	{
		std::string m_type;
		std::vector<std::pair<std::string, std::string>> m_options;
	};

	std::vector<GLSLStringOption> option_strings;
	std::vector<GLSLStringOption> stage_strings;
	GLSLStringOption* current_strings = nullptr;
	while (!in.eof())
	{
		std::string line;

		if (std::getline(in, line))
		{
#ifndef _WIN32
			// Check for CRLF eol and convert it to LF
			if (!line.empty() && line.at(line.size() - 1) == '\r')
			{
				line.erase(line.size() - 1);
			}
#endif

			if (line.size() > 0)
			{
				if (line[0] == '[')
				{
					size_t endpos = line.find("]");

					if (endpos != std::string::npos)
					{
						// New section!
						std::string sub = line.substr(1, endpos - 1);
						if (sub == "Stage")
						{
							stage_strings.push_back({ sub });
							current_strings = &stage_strings.back();
						}
						else
						{
							option_strings.push_back({ sub });
							current_strings = &option_strings.back();
						}
					}
				}
				else
				{
					if (current_strings)
					{
						std::string key, value;
						IniFile::ParseLine(line, &key, &value);

						if (!(key == "" && value == ""))
							current_strings->m_options.emplace_back(key, value);
					}
				}
			}
		}
	}

	for (const auto& it : option_strings)
	{
		ConfigurationOption option;
		option.m_dirty = true;
		option.m_resolve_at_compilation = false;
		if (it.m_type == "OptionBool")
			option.m_type = ConfigurationOption::OptionType::OPTION_BOOL;
		else if (it.m_type == "OptionRangeFloat")
			option.m_type = ConfigurationOption::OptionType::OPTION_FLOAT;
		else if (it.m_type == "OptionRangeInteger")
			option.m_type = ConfigurationOption::OptionType::OPTION_INTEGER;

		for (const auto& string_option : it.m_options)
		{
			if (string_option.first == "GUIName")
			{
				option.m_gui_name = string_option.second;
			}
			else if (string_option.first == "OptionName")
			{
				option.m_option_name = string_option.second;
			}
			else if (string_option.first == "DependentOption")
			{
				option.m_dependent_option = string_option.second;
			}
			else if (string_option.first == "ResolveAtCompilation")
			{
				TryParse(string_option.second, &option.m_resolve_at_compilation);
			}
			else if (string_option.first == "MinValue" ||
				string_option.first == "MaxValue" ||
				string_option.first == "DefaultValue" ||
				string_option.first == "StepAmount")
			{
				std::vector<s32>* output_integer = nullptr;
				std::vector<float>* output_float = nullptr;

				if (string_option.first == "MinValue")
				{
					output_integer = &option.m_integer_min_values;
					output_float = &option.m_float_min_values;
				}
				else if (string_option.first == "MaxValue")
				{
					output_integer = &option.m_integer_max_values;
					output_float = &option.m_float_max_values;
				}
				else if (string_option.first == "DefaultValue")
				{
					output_integer = &option.m_integer_values;
					output_float = &option.m_float_values;
				}
				else if (string_option.first == "StepAmount")
				{
					output_integer = &option.m_integer_step_values;
					output_float = &option.m_float_step_values;
				}

				if (option.m_type == ConfigurationOption::OptionType::OPTION_BOOL)
				{
					TryParse(string_option.second, &option.m_bool_value);
				}
				else if (option.m_type == ConfigurationOption::OptionType::OPTION_INTEGER)
				{
					TryParseVector(string_option.second, output_integer);
					if (output_integer->size() > 4)
						output_integer->erase(output_integer->begin() + 4, output_integer->end());
				}
				else if (option.m_type == ConfigurationOption::OptionType::OPTION_FLOAT)
				{
					TryParseVector(string_option.second, output_float);
					if (output_float->size() > 4)
						output_float->erase(output_float->begin() + 4, output_float->end());
				}
			}
		}
		if (option.m_type == ConfigurationOption::OptionType::OPTION_INTEGER)
		{
			size_t array_size = std::max(option.m_integer_min_values.size(), option.m_integer_max_values.size());
			array_size = std::max(array_size, option.m_integer_step_values.size());
			array_size = std::max(array_size, option.m_integer_values.size());
			array_size = std::max(array_size, size_t(1));
			if (option.m_integer_min_values.size() < array_size) option.m_integer_min_values.resize(array_size);
			if (option.m_integer_max_values.size() < array_size) option.m_integer_max_values.resize(array_size);
			if (option.m_integer_step_values.size() < array_size) option.m_integer_step_values.resize(array_size);
			if (option.m_integer_values.size() < array_size) option.m_integer_values.resize(array_size);
		}
		else if (option.m_type == ConfigurationOption::OptionType::OPTION_FLOAT)
		{
			size_t array_size = std::max(option.m_float_min_values.size(), option.m_float_max_values.size());
			array_size = std::max(array_size, option.m_float_step_values.size());
			array_size = std::max(array_size, option.m_float_values.size());
			array_size = std::max(array_size, size_t(1));
			if (option.m_float_min_values.size() < array_size) option.m_float_min_values.resize(array_size);
			if (option.m_float_max_values.size() < array_size) option.m_float_max_values.resize(array_size);
			if (option.m_float_step_values.size() < array_size) option.m_float_step_values.resize(array_size);
			if (option.m_float_values.size() < array_size) option.m_float_values.resize(array_size);
		}
		m_options[option.m_option_name] = option;
	}
	if (stage_strings.size() > 0)
	{
		for (const auto& it : stage_strings)
		{
			StageOption option;
			option.m_outputScale = 1.0f;
			option.m_use_source_resolution = false;
			option.m_dependent_options.resize(0);
			for (const auto& string_option : it.m_options)
			{
				if (string_option.first == "EntryPoint")
				{
					option.m_stage_entry_point = string_option.second;
				}
				else if (string_option.first == "OutputScale")
				{
					TryParse(string_option.second, &option.m_outputScale);
				}
				else if (string_option.first == "UseSourceResolution")
				{
					TryParse(string_option.second, &option.m_use_source_resolution);
				}
				else if (string_option.first == "DependentOption")
				{
					ConfigMap::const_iterator it = m_options.find(string_option.second);
					if (it != m_options.cend())
					{
						if (it->second.m_type == ConfigurationOption::OptionType::OPTION_BOOL)
						{
							// Only Add boolean options as parents
							option.m_dependent_options.push_back(&it->second);
						}
					}
				}
				else if (string_option.first == "Inputs" && m_stages.size() > 0)
				{
					TryParseVector(string_option.second, &option.m_inputs);
					//allow only as much inputs as previous stages and no more than 4
					size_t stage_limit = std::min(m_stages.size(), size_t(4));
					if (option.m_inputs.size() > stage_limit)
						option.m_inputs.erase(option.m_inputs.begin() + stage_limit, option.m_inputs.end());
					for (u32& inp : option.m_inputs)
					{
						// Allow only inputs from previous stages
						if (inp >= m_stages.size())
						{
							inp = u32(m_stages.size() - 1);
						}
					}
				}
			}
			if (m_stages.size() > 0 && option.m_inputs.size() == 0)
			{
				// by default if no input is defined we bind the previous stage as input 0
				option.m_inputs.push_back(u32(m_stages.size() - 1));
			}
			m_stages.push_back(option);
		}
	}
	else
	{
		//Add Default Stage
		StageOption option;
		option.m_stage_entry_point = "main";
		option.m_outputScale = 1.0f;
		m_stages.push_back(option);
		option.m_dependent_options.resize(0);
	}
	return code.substr(0, configuration_start) + code.substr(configuration_end + config_end_delimiter.size());
}

void PostProcessingShaderConfiguration::PrintCompilationTimeOptions(std::string &options)
{
	for (auto& it : m_options)
	{
		if (!it.second.m_resolve_at_compilation)
		{
			continue;
		}
		it.second.m_dirty = false;
		if (it.second.m_type == PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_BOOL)
		{
			options += StringFromFormat("#define %s %d\n", it.first.c_str(), it.second.m_bool_value ? 1 : 0);
		}
		else if (it.second.m_type == PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_INTEGER)
		{
			u32 count = static_cast<u32>(it.second.m_integer_values.size());
			switch (count)
			{
			case 1:
				options += StringFromFormat("#define %s %d\n",
					it.first.c_str(),
					it.second.m_integer_values[0]);
				break;
			case 2:
				options += StringFromFormat("#define %s int2(%d,%d)\n",
					it.first.c_str(),
					it.second.m_integer_values[0],
					it.second.m_integer_values[1]);
				break;
			case 3:
				options += StringFromFormat("#define %s int3(%d,%d,%d)\n",
					it.first.c_str(),
					it.second.m_integer_values[0],
					it.second.m_integer_values[1],
					it.second.m_integer_values[2]);
				break;
			case 4:
				options += StringFromFormat("#define %s int4(%d,%d,%d, %d)\n",
					it.first.c_str(),
					it.second.m_integer_values[0],
					it.second.m_integer_values[1],
					it.second.m_integer_values[2],
					it.second.m_integer_values[3]);
				break;
			default:
				break;
			}
		}
		else if (it.second.m_type == PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_FLOAT)
		{
			u32 count = static_cast<u32>(it.second.m_float_values.size());
			switch (count)
			{
			case 1:
				options += StringFromFormat("#define %s %f\n",
					it.first.c_str(),
					it.second.m_float_values[0]);
				break;
			case 2:
				options += StringFromFormat("#define %s float2(%f,%f)\n",
					it.first.c_str(),
					it.second.m_float_values[0],
					it.second.m_float_values[1]);
				break;
			case 3:
				options += StringFromFormat("#define %s float3(%f,%f,%f)\n",
					it.first.c_str(),
					it.second.m_float_values[0],
					it.second.m_float_values[1],
					it.second.m_float_values[2]);
				break;
			case 4:
				options += StringFromFormat("#define %s float4(%f,%f,%f,%f)\n",
					it.first.c_str(),
					it.second.m_float_values[0],
					it.second.m_float_values[1],
					it.second.m_float_values[2],
					it.second.m_float_values[3]);
				break;
			default:
				break;
			}
		}
	}
}

void PostProcessingShaderConfiguration::LoadOptionsConfigurationFromSection(IniFile::Section* section)
{
	for (auto& it : m_options)
	{
		switch (it.second.m_type)
		{
		case ConfigurationOption::OptionType::OPTION_BOOL:
			section->Get(it.second.m_option_name, &it.second.m_bool_value, it.second.m_bool_value);
			break;
		case ConfigurationOption::OptionType::OPTION_INTEGER:
		{
			std::string value;
			section->Get(it.second.m_option_name, &value);
			if (value != "")
				TryParseVector(value, &it.second.m_integer_values);
		}
		break;
		case ConfigurationOption::OptionType::OPTION_FLOAT:
		{
			std::string value;
			section->Get(it.second.m_option_name, &value);
			if (value != "")
				TryParseVector(value, &it.second.m_float_values);
		}
		break;
		}
	}
}

void PostProcessingShaderConfiguration::LoadOptionsConfiguration()
{
	if (m_current_shader != "")
	{
		IniFile ini;
		ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
		IniFile::Section* section = ini.GetOrCreateSection(m_current_shader + "-options");
		// Load Global Setings
		LoadOptionsConfigurationFromSection(section);
		if (Core::IsRunningAndStarted())
		{
			std::string PresetPath = File::GetUserPath(D_PPSHADERSPRESETS_IDX);
			PresetPath += SConfig::GetInstance().m_strUniqueID + DIR_SEP;
			PresetPath += m_current_shader + ".ini";
			if (File::Exists(PresetPath))
			{
				//Override with specific game settings
				ini.Load(PresetPath);
				IniFile::Section* gameini_section = ini.GetOrCreateSection("options");
				LoadOptionsConfigurationFromSection(gameini_section);
			}
		}
	}
	CheckStages();
}

void PostProcessingShaderConfiguration::SaveOptionsConfiguration()
{
	if (m_current_shader == "")
	{
		return;
	}
	std::string file_path;
	IniFile ini;
	IniFile::Section* section = nullptr;
	if (Core::IsRunningAndStarted())
	{
		file_path = File::GetUserPath(D_PPSHADERSPRESETS_IDX);
		if (!File::Exists(file_path))
		{
			File::CreateDir(file_path);
		}
		file_path += SConfig::GetInstance().m_strUniqueID + DIR_SEP;
		if (!File::Exists(file_path))
		{
			File::CreateDir(file_path);
		}
		file_path += m_current_shader + ".ini";
		ini.Load(file_path);
		section = ini.GetOrCreateSection("options");
	}
	else
	{
		file_path = File::GetUserPath(F_DOLPHINCONFIG_IDX);
		ini.Load(file_path);
		section = ini.GetOrCreateSection(m_current_shader + "-options");
	}

	for (auto& it : m_options)
	{
		switch (it.second.m_type)
		{
		case ConfigurationOption::OptionType::OPTION_BOOL:
		{
			section->Set(it.second.m_option_name, it.second.m_bool_value);
		}
		break;
		case ConfigurationOption::OptionType::OPTION_INTEGER:
		{
			std::string value = "";
			for (size_t i = 0; i < it.second.m_integer_values.size(); ++i)
				value += StringFromFormat("%d%s", it.second.m_integer_values[i], i == (it.second.m_integer_values.size() - 1) ? "" : ", ");
			section->Set(it.second.m_option_name, value);
		}
		break;
		case ConfigurationOption::OptionType::OPTION_FLOAT:
		{
			std::ostringstream value;
			value.imbue(std::locale("C"));

			for (size_t i = 0; i < it.second.m_float_values.size(); ++i)
			{
				value << it.second.m_float_values[i];
				if (i != (it.second.m_float_values.size() - 1))
					value << ", ";
			}
			section->Set(it.second.m_option_name, value.str());
		}
		break;
		}
	}
	ini.Save(file_path);
}

void PostProcessingShaderConfiguration::ReloadShader()
{
	m_current_shader = "";
}

void PostProcessingShaderConfiguration::SetOptionf(const std::string& option, int index, float value)
{
	auto it = m_options.find(option);

	it->second.m_float_values[index] = value;
	if (it->second.m_resolve_at_compilation)
		m_requires_recompilation = true;
	it->second.m_dirty = true;
	m_any_options_dirty = true;
}

void PostProcessingShaderConfiguration::SetOptioni(const std::string& option, int index, s32 value)
{
	auto it = m_options.find(option);

	it->second.m_integer_values[index] = value;
	if (it->second.m_resolve_at_compilation)
		m_requires_recompilation = true;
	it->second.m_dirty = true;
	m_any_options_dirty = true;
}

void PostProcessingShaderConfiguration::SetOptionb(const std::string& option, bool value)
{
	auto it = m_options.find(option);

	it->second.m_bool_value = value;	
	if (it->second.m_resolve_at_compilation)
		m_requires_recompilation = true;
	it->second.m_dirty = true;
	m_any_options_dirty = true;
	CheckStages();
}

void PostProcessingShaderConfiguration::CheckStages()
{
	m_last_stage = 0;
	for (u32 i = 0; i < m_stages.size(); i++)
	{
		m_stages[i].CheckEnabled();
		m_last_stage = m_stages[i].m_isEnabled ? i : m_last_stage;
	}
}
