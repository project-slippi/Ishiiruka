// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.


#include <cmath>
#include <string>

#include <SOIL/SOIL.h>

#include "Common/CommonFuncs.h"
#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/FramebufferManagerBase.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"


static const char s_default_shader[] = "void main() { SetOutput(ApplyGCGamma(Sample())); }\n";
struct LangDescriptor
{
	const char* Lang;
	const char* Code;
};

#define LANGUAGE_ID_COUNT 29

static const LangDescriptor language_ids[LANGUAGE_ID_COUNT] =
{
	{ "", "" },
	{ "ms", ".MAL" },
	{ "ca", ".CAT" },
	{ "cs", ".CZE" },
	{ "da", ".DAN" },
	{ "de", ".GER" },
	{ "en", ".ENG" },
	{ "es", ".SPA" },
	{ "fr", ".FRE" },
	{ "hr", ".CRO" },
	{ "it", ".ITA" },
	{ "hu", ".HUN" },
	{ "nl", ".DUT" },
	{ "nb", ".NOR" },
	{ "pl", ".POL" },
	{ "pt", ".POR" },
	{ "pt_BR", ".BRA" },
	{ "ro", ".ROM" },
	{ "sr", ".SER" },
	{ "sv", ".SWE" },
	{ "tr", ".TUR" },

	{ "el", ".GRE" },
	{ "ru", ".RUS" },
	{ "ar", ".ARA" },
	{ "fa", ".FAR" },
	{ "ko", ".KOR" },
	{ "ja", ".JAP" },
	{ "zh_CN", ".CHS" },
	{ "zh_TW", ".CHT" }
};

std::vector<std::string> PostProcessingShaderConfiguration::GetAvailableShaderNames(const std::string& sub_dir)
{
	const std::vector<std::string> search_dirs = { File::GetUserPath(D_SHADERS_IDX) + sub_dir, File::GetSysDirectory() + SHADERS_DIR DIR_SEP + sub_dir };
	const std::vector<std::string> search_extensions = { ".glsl" };
	std::vector<std::string> result;
	std::vector<std::string> paths;

	// main folder
	paths = DoFileSearch(search_extensions, search_dirs, false);
	for (const std::string& path : paths)
	{
		std::string filename;
		if (SplitPath(path, nullptr, &filename, nullptr))
		{
			if (std::find(result.begin(), result.end(), filename) == result.end())
				result.push_back(filename);
		}
	}

	// folders/sub-shaders
	paths = FindSubdirectories(search_dirs, false);
	for (const std::string& dirname : paths)
	{
		// find sub-shaders in this folder
		size_t pos = dirname.find_last_of(DIR_SEP_CHR);
		if (pos != std::string::npos && (pos != dirname.length() - 1))
		{
			std::string shader_dirname = dirname.substr(pos + 1);
			std::vector<std::string> sub_paths = DoFileSearch(search_extensions, { dirname }, false);
			for (const std::string& sub_path : sub_paths)
			{
				std::string filename;
				if (SplitPath(sub_path, nullptr, &filename, nullptr))
				{
					// Remove /main for main shader
					std::string name = (!strcasecmp(filename.c_str(), "main")) ? (shader_dirname) : (shader_dirname + DIR_SEP + filename);
					if (std::find(result.begin(), result.end(), filename) == result.end())
						result.push_back(name);
				}
			}
		}
	}

	// sort lexicographically
	std::sort(result.begin(), result.end());
	return result;
}

bool PostProcessingShaderConfiguration::LoadShader(const std::string& sub_dir, const std::string& name)
{
	// clear all state
	m_shader_name = name;
	m_shader_source.clear();
	m_options.clear();
	m_render_passes.clear();
	m_any_options_dirty = false;
	m_compile_time_constants_dirty = false;
	m_requires_depth_buffer = false;
	m_frame_output.color_output_scale = 1.0;
	m_frame_output.depth_scale = 1.0;
	m_frame_output.depth_count = 0;
	m_frame_output.color_count = 0;
	// special case: default shader, no path, use inbuilt code
	if (name.empty())
	{
		m_shader_source = s_default_shader;
		return ParseConfiguration(sub_dir, "");
	}

	// This is kind of horrifying, but we only want to load one shader, in a specific order.
	// (if there's an error, this class is left in an unknown state)
	std::string dirname;
	std::string filename;
	bool found = false;
	bool result = false;

	// Try old-style shaders first, but completely skip if the name is a sub-shader
	if (name.find(DIR_SEP_CHR) == std::string::npos)
	{
		// User/Shaders/sub_dir/<name>.glsl
		dirname = File::GetUserPath(D_SHADERS_IDX) + sub_dir + DIR_SEP;
		filename = dirname + name + ".glsl";
		if (File::Exists(filename))
		{
			result = ParseShader(dirname, filename);
			found = true;
		}
		else
		{
			// Sys/Shaders/sub_dir/<name>.glsl
			dirname = File::GetSysDirectory() + SHADERS_DIR DIR_SEP + sub_dir + DIR_SEP;
			filename = dirname + name + ".glsl";
			if (File::Exists(filename))
			{
				result = ParseShader(dirname, filename);
				found = true;
			}
		}
	}

	// Try shader directories/sub-shaders
	if (!found)
	{
		std::string shader_name;
		std::string sub_shader_name;
		size_t sep_pos = name.find(DIR_SEP_CHR);
		if (sep_pos != std::string::npos && (sep_pos != name.length() - 1))
		{
			shader_name = name.substr(0, sep_pos);
			sub_shader_name = name.substr(sep_pos + 1);
		}
		else
		{
			// default name is main.glsl
			shader_name = name;
			sub_shader_name = "main";
		}

		// User/Shaders/sub_dir/<shader_name>/<sub_shader_name>.glsl
		dirname = File::GetUserPath(D_SHADERS_IDX) + sub_dir + DIR_SEP + shader_name + DIR_SEP;
		filename = dirname + sub_shader_name + ".glsl";
		if (File::Exists(filename))
		{
			result = ParseShader(dirname, filename);
			found = true;
		}
		else
		{
			// Sys/Shaders/sub_dir/<shader_name>/<sub_shader_name>.glsl
			dirname = File::GetSysDirectory() + SHADERS_DIR DIR_SEP + sub_dir + DIR_SEP + shader_name + DIR_SEP;
			filename = dirname + sub_shader_name + ".glsl";
			if (File::Exists(filename))
			{
				result = ParseShader(dirname, filename);
				found = true;
			}
		}
	}

	if (!found)
	{
		ERROR_LOG(VIDEO, "Post-processing shader not found: %s", name.c_str());
		return false;
	}

	if (!result)
	{
		ERROR_LOG(VIDEO, "Failed to parse post-processing shader at %s", filename.c_str());
		return false;
	}

	LoadOptionsConfiguration();
	return true;
}

bool PostProcessingShaderConfiguration::ParseShader(const std::string& dirname, const std::string& path)
{
	// Read to a single string we can work with
	std::string code;
	if (!File::ReadFileToString(path, code))
		return false;

	// Find configuration block, if any
	const std::string config_start_delimiter = "[configuration]";
	const std::string config_end_delimiter = "[/configuration]";
	size_t configuration_start = code.find(config_start_delimiter);
	size_t configuration_end = code.find(config_end_delimiter);
	std::string configuration_string;

	if (configuration_start != std::string::npos && configuration_end != std::string::npos)
	{
		// Remove the configuration area from the source string, leaving only the GLSL code.
		m_shader_source = code;
		m_shader_source.erase(configuration_start, (configuration_end - configuration_start + config_end_delimiter.length()));

		// Extract configuration string, and parse options/passes
		configuration_string = code.substr(configuration_start + config_start_delimiter.size(),
			configuration_end - configuration_start - config_start_delimiter.size());
	}
	else
	{
		// If there is no configuration block. Assume the entire file is code.
		m_shader_source = code;
	}

	return ParseConfiguration(dirname, configuration_string);
}

bool PostProcessingShaderConfiguration::ParseConfiguration(const std::string& dirname, const std::string& configuration_string)
{
	std::vector<ConfigBlock> config_blocks = ReadConfigSections(configuration_string);
	if (!ParseConfigSections(dirname, config_blocks))
		return false;

	// If no render passes are specified, create a default pass.
	if (m_render_passes.empty())
		CreateDefaultPass();

	return true;
}

std::vector<PostProcessingShaderConfiguration::ConfigBlock> PostProcessingShaderConfiguration::ReadConfigSections(const std::string& configuration_string)
{
	std::istringstream in(configuration_string);

	std::vector<ConfigBlock> config_blocks;
	ConfigBlock* current_block = nullptr;

	while (!in.eof())
	{
		std::string line;

		if (std::getline(in, line))
		{
#ifndef _WIN32
			// Check for CRLF eol and convert it to LF
			if (!line.empty() && line.at(line.size() - 1) == '\r')
				line.erase(line.size() - 1);
#endif

			if (line.size() > 0)
			{
				if (line[0] == '[')
				{
					size_t endpos = line.find("]");

					if (endpos != std::string::npos)
					{
						std::string sub = line.substr(1, endpos - 1);
						ConfigBlock section;
						section.m_type = sub;
						config_blocks.push_back(std::move(section));
						current_block = &config_blocks.back();
					}
				}
				else
				{
					std::string key, value;
					IniFile::ParseLine(line, &key, &value);
					if (!key.empty() && !value.empty())
					{
						if (current_block)
							current_block->m_options.emplace_back(key, value);
					}
				}
			}
		}
	}

	return config_blocks;
}

bool PostProcessingShaderConfiguration::ParseConfigSections(const std::string& dirname, const std::vector<ConfigBlock>& config_blocks)
{
	for (const ConfigBlock& option : config_blocks)
	{
		if (option.m_type == "Pass")
		{
			if (!ParsePassBlock(dirname, option))
				return false;
		}
		else if (option.m_type == "Frame")
		{
			if (!ParseFrameBlock(option))
				return false;
		}
		else
		{
			if (!ParseOptionBlock(dirname, option))
				return false;
		}
	}
	return true;
}

bool PostProcessingShaderConfiguration::ParseFrameBlock(const ConfigBlock& block)
{
	for (const auto& option : block.m_options)
	{
		const std::string& key = option.first;
		const std::string& value = option.second;
		if (key == "OutputScale")
		{
			TryParse(value, &m_frame_output.color_output_scale);
			if (m_frame_output.color_output_scale <= 0.0f)
				return false;
		}
		else if (key == "DepthOutputScale")
		{
			TryParse(value, &m_frame_output.depth_scale);
			if (m_frame_output.depth_scale <= 0.0f)
				return false;
		}
		else if (key == "Count")
		{
			int count = 0;
			TryParse(value, &count);
			if (count <= 0)
				return false;
			m_frame_output.color_count = std::max(m_frame_output.color_count, count + 1);
		}
		else if (key == "DepthCount")
		{
			int count = 0;
			TryParse(value, &count);
			if (count <= 0)
				return false;
			m_frame_output.depth_count = std::max(m_frame_output.depth_count, count + 1);
		}
		else
		{
			ERROR_LOG(VIDEO, "Post processing configuration error: Unknown input key: %s", key.c_str());
			return false;
		}
	}
	return true;
}

bool PostProcessingShaderConfiguration::ParseOptionBlock(const std::string& dirname, const ConfigBlock& block)
{
	// Initialize to default values, in case the configuration section is incomplete.
	ConfigurationOption option;
	option.m_bool_value = false;
	option.m_type = POST_PROCESSING_OPTION_TYPE_FLOAT;
	option.m_compile_time_constant = false;
	option.m_dirty = false;

	if (block.m_type == "OptionBool")
	{
		option.m_type = POST_PROCESSING_OPTION_TYPE_BOOL;
	}
	else if (block.m_type == "OptionRangeFloat")
	{
		option.m_type = POST_PROCESSING_OPTION_TYPE_FLOAT;
	}
	else if (block.m_type == "OptionRangeInteger")
	{
		option.m_type = POST_PROCESSING_OPTION_TYPE_INTEGER;
	}
	else
	{
		// not fatal, provided for forwards compatibility
		WARN_LOG(VIDEO, "Unknown section name in post-processing shader config: '%s'", block.m_type.c_str());
		return true;
	}

	const char* LangCode = nullptr;
	for (size_t i = 1; i < LANGUAGE_ID_COUNT; i++)
	{
		if (language_ids[i].Lang == SConfig::GetInstance().m_InterfaceLanguage)
		{
			LangCode = language_ids[i].Code;
			break;
		}
	}

	for (const auto& string_option : block.m_options)
	{
		if (StringStartsWith(string_option.first, "GUIName"))
		{
			if (string_option.first == "GUIName")
			{
				option.m_gui_name = string_option.second;
			}
			else if (LangCode != nullptr && StringEndsWith(string_option.first, LangCode))
			{
				option.m_gui_name = string_option.second;
			}
		}
		else if (StringStartsWith(string_option.first, "GUIDescription"))
		{
			if (string_option.first == "GUIDescription")
			{
				option.m_gui_description = string_option.second;
			}
			else if (LangCode != nullptr && StringEndsWith(string_option.first, LangCode))
			{
				option.m_gui_description = string_option.second;
			}
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
			TryParse(string_option.second, &option.m_compile_time_constant);
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

			if (option.m_type == POST_PROCESSING_OPTION_TYPE_BOOL)
			{
				TryParse(string_option.second, &option.m_bool_value);
			}
			else if (option.m_type == POST_PROCESSING_OPTION_TYPE_INTEGER)
			{
				TryParseVector(string_option.second, output_integer);
				if (output_integer->size() > 4)
					output_integer->erase(output_integer->begin() + 4, output_integer->end());
			}
			else if (option.m_type == POST_PROCESSING_OPTION_TYPE_FLOAT)
			{
				TryParseVector(string_option.second, output_float);
				if (output_float->size() > 4)
					output_float->erase(output_float->begin() + 4, output_float->end());
			}
		}
	}
	option.m_default_bool_value = option.m_bool_value;
	option.m_default_float_values = option.m_float_values;
	option.m_default_integer_values = option.m_integer_values;
	m_options[option.m_option_name] = option;
	return true;
}

bool PostProcessingShaderConfiguration::ParsePassBlock(const std::string& dirname, const ConfigBlock& block)
{
	RenderPass pass;
	pass.output_scale = 1.0f;
	pass.output_format = PC_TexFormat::PC_TEX_FMT_RGBA32;

	for (const auto& option : block.m_options)
	{
		const std::string& key = option.first;
		const std::string& value = option.second;
		if (key == "EntryPoint")
		{
			pass.entry_point = value;
		}
		else if (key == "OutputScale")
		{
			TryParse(value, &pass.output_scale);
			if (pass.output_scale <= 0.0f)
				return false;
		}
		else if (key == "OutputFormat")
		{
			if (value == "R32_FLOAT")
			{
				pass.output_format = PC_TexFormat::PC_TEX_FMT_R_FLOAT;
			}
			else if (value == "RGBA16_FLOAT")
			{
				pass.output_format = PC_TexFormat::PC_TEX_FMT_RGBA16_FLOAT;
			}
			else if (value == "RGBA32_FLOAT")
			{
				pass.output_format = PC_TexFormat::PC_TEX_FMT_RGBA_FLOAT;
			}
			else
			{
				return false;
			}
				
		}
		else if (key == "OutputScaleNative")
		{
			TryParse(value, &pass.output_scale);
			if (pass.output_scale <= 0.0f)
				return false;

			// negative means native scale
			pass.output_scale = -pass.output_scale;
		}
		else if (key == "DependantOption" || key == "DependentOption")
		{
			ConfigMap::const_iterator it = m_options.find(value);
			if (it != m_options.cend())
			{
				if (it->second.m_type == POST_PROCESSING_OPTION_TYPE_BOOL)
				{
					// Only Add boolean options as parents
					pass.dependent_options.push_back(&it->second);
				}
			}
		}
		else if (key.compare(0, 5, "Input") == 0 && key.length() > 5)
		{
			u32 texture_unit = key[5] - '0';
			if (texture_unit > POST_PROCESSING_MAX_TEXTURE_INPUTS)
			{
				ERROR_LOG(VIDEO, "Post processing configuration error: Out-of-range texture unit: %u", texture_unit);
				return false;
			}

			// Input declared yet?
			RenderPass::Input* input = nullptr;
			for (RenderPass::Input& input_it : pass.inputs)
			{
				if (input_it.texture_unit == texture_unit)
				{
					input = &input_it;
					break;
				}
			}
			if (input == nullptr)
			{
				RenderPass::Input new_input;
				new_input.type = POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER;
				new_input.filter = POST_PROCESSING_INPUT_FILTER_LINEAR;
				new_input.address_mode = POST_PROCESSING_ADDRESS_MODE_BORDER;
				new_input.texture_unit = texture_unit;
				new_input.pass_output_index = 0;
				pass.inputs.push_back(std::move(new_input));
				input = &pass.inputs.back();
			}

			// Input#(Filter|Mode|Source)
			std::string extra = (key.length() > 6) ? key.substr(6) : "";
			if (extra.empty())
			{
				// Type
				if (value == "ColorBuffer")
				{
					input->type = POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER;
				}
				else if (value == "DepthBuffer")
				{
					input->type = POST_PROCESSING_INPUT_TYPE_DEPTH_BUFFER;
					m_requires_depth_buffer = true;
				}
				else if (value == "PreviousPass")
				{
					input->type = POST_PROCESSING_INPUT_TYPE_PREVIOUS_PASS_OUTPUT;
				}
				else if (value == "Image")
				{
					input->type = POST_PROCESSING_INPUT_TYPE_IMAGE;
				}
				else if (value.compare(0, 4, "Pass") == 0)
				{
					input->type = POST_PROCESSING_INPUT_TYPE_PASS_OUTPUT;
					if (!TryParse(value.substr(4), &input->pass_output_index) || input->pass_output_index >= m_render_passes.size())
					{
						ERROR_LOG(VIDEO, "Post processing configuration error: Out-of-range render pass reference: %u", input->pass_output_index);
						return false;
					}
				}
				else if (value.compare(0, 5, "Frame") == 0)
				{
					input->type = POST_PROCESSING_INPUT_TYPE_PASS_FRAME_OUTPUT;
					if (!TryParse(value.substr(5), &input->pass_output_index))
					{
						ERROR_LOG(VIDEO, "Post processing configuration error: Out-of-range frame reference: %u", input->pass_output_index);
						return false;
					}
					m_frame_output.color_count = std::max(m_frame_output.color_count, static_cast<int>(input->pass_output_index) + 2);
				}
				else if (value.compare(0, 5, "DepthFrame") == 0)
				{
					input->type = POST_PROCESSING_INPUT_TYPE_PASS_DEPTH_FRAME_OUTPUT;
					if (!TryParse(value.substr(5), &input->pass_output_index))
					{
						ERROR_LOG(VIDEO, "Post processing configuration error: Out-of-range frame reference: %u", input->pass_output_index);
						return false;
					}
					m_frame_output.depth_count = std::max(m_frame_output.depth_count, static_cast<int>(input->pass_output_index) + 2);
				}
				else
				{
					ERROR_LOG(VIDEO, "Post processing configuration error: Invalid input type: %s", value.c_str());
					return false;
				}
			}
			else if (extra == "Filter")
			{
				if (value == "Nearest")
				{
					input->filter = POST_PROCESSING_INPUT_FILTER_NEAREST;
				}
				else if (value == "Linear")
				{
					input->filter = POST_PROCESSING_INPUT_FILTER_LINEAR;
				}
				else
				{
					ERROR_LOG(VIDEO, "Post processing configuration error: Invalid input filter: %s", value.c_str());
					return false;
				}
			}
			else if (extra == "Mode")
			{
				if (value == "Clamp")
				{
					input->address_mode = POST_PROCESSING_ADDRESS_MODE_CLAMP;
				}
				else if (value == "Wrap")
				{
					input->address_mode = POST_PROCESSING_ADDRESS_MODE_WRAP;
				}
				else if (value == "Border")
				{
					input->address_mode = POST_PROCESSING_ADDRESS_MODE_BORDER;
				}
				else if (value == "Mirror")
				{
					input->address_mode = POST_PROCESSING_ADDRESS_MODE_MIRROR;
				}
				else
				{
					ERROR_LOG(VIDEO, "Post processing configuration error: Invalid input mode: %s", value.c_str());
					return false;
				}
			}
			else if (extra == "Source")
			{
				// Load external image
				std::string path = dirname + value;
				if (!File::Exists(path) || !LoadExternalImage(path, input))
				{
					ERROR_LOG(VIDEO, "Post processing configuration error: Unable to load external image at '%s'", value.c_str());
					return false;
				}
			}
			else
			{
				ERROR_LOG(VIDEO, "Post processing configuration error: Unknown input key: %s", key.c_str());
				return false;
			}
		}
	}

	if (!ValidatePassInputs(pass))
		return false;

	m_render_passes.push_back(std::move(pass));
	return true;
}

bool PostProcessingShaderConfiguration::LoadExternalImage(const std::string& path, RenderPass::Input* input)
{
	File::IOFile file(path, "rb");
	std::vector<u8> buffer(file.GetSize());
	if (!file.IsOpen() || !file.ReadBytes(buffer.data(), file.GetSize()))
		return false;

	int image_width;
	int image_height;
	int image_channels;
	u8* decoded = SOIL_load_image_from_memory(buffer.data(), (int)buffer.size(), &image_width, &image_height, &image_channels, SOIL_LOAD_RGBA);
	if (decoded == nullptr)
		return false;

	// Reallocate the memory so we can manage it
	input->type = POST_PROCESSING_INPUT_TYPE_IMAGE;
	input->external_image_size.width = image_width;
	input->external_image_size.height = image_height;
	input->external_image_data = std::make_unique<u8[]>(image_width * image_height * 4);
	memcpy(input->external_image_data.get(), decoded, image_width * image_height * 4);
	SOIL_free_image_data(decoded);
	return true;
}

bool PostProcessingShaderConfiguration::ValidatePassInputs(const RenderPass& pass)
{
	return std::all_of(pass.inputs.begin(), pass.inputs.end(), [&pass](const RenderPass::Input& input)
	{
		// Check for image inputs without valid data
		if (input.type == POST_PROCESSING_INPUT_TYPE_IMAGE && !input.external_image_data)
		{
			ERROR_LOG(VIDEO, "Post processing configuration error: Pass '%s' input %u is missing image source.", pass.entry_point.c_str(), input.texture_unit);
			return false;
		}

		return true;
	});
}

void PostProcessingShaderConfiguration::CreateDefaultPass()
{
	RenderPass::Input input;
	input.type = POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER;
	input.filter = POST_PROCESSING_INPUT_FILTER_LINEAR;
	input.address_mode = POST_PROCESSING_ADDRESS_MODE_CLAMP;
	input.texture_unit = 0;
	input.pass_output_index = 0;

	RenderPass pass;
	pass.entry_point = "main";
	pass.inputs.push_back(std::move(input));
	pass.output_scale = 1;
	m_render_passes.push_back(std::move(pass));
}

void PostProcessingShaderConfiguration::LoadOptionsConfigurationFromSection(IniFile::Section* section)
{
	for (auto& it : m_options)
	{
		auto& current = it.second;
		switch (current.m_type)
		{
		case POST_PROCESSING_OPTION_TYPE_BOOL:
			section->Get(current.m_option_name, &current.m_bool_value, current.m_bool_value);
			break;
		case POST_PROCESSING_OPTION_TYPE_INTEGER:
		{
			std::string value;
			section->Get(current.m_option_name, &value);
			if (value != "")
				TryParseVector(value, &current.m_integer_values);
			for (size_t i = 0; i < current.m_integer_values.size(); i++)
			{
				s32 val = current.m_integer_values[i];
				val = std::max(val, current.m_integer_min_values[i]);
				val = std::min(val, current.m_integer_max_values[i]);
				current.m_integer_values[i] = val;
			}
		}
		break;
		case POST_PROCESSING_OPTION_TYPE_FLOAT:
		{
			std::string value;
			section->Get(current.m_option_name, &value);
			if (value != "")
				TryParseVector(value, &current.m_float_values);
			for (size_t i = 0; i < current.m_float_values.size(); i++)
			{
				float val = current.m_float_values[i];
				val = std::max(val, current.m_float_min_values[i]);
				val = std::min(val, current.m_float_max_values[i]);
				current.m_float_values[i] = val;
			}
		}
		break;
		}
	}
}

void PostProcessingShaderConfiguration::LoadOptionsConfiguration()
{
	IniFile ini;
	std::string GlobalPath = File::GetUserPath(D_PPSHADERSPRESETS_IDX);
	GlobalPath += m_shader_name + ".ini";
	ini.Load(GlobalPath);
	IniFile::Section* section = ini.GetOrCreateSection("options");
	// Load Global Setings
	LoadOptionsConfigurationFromSection(section);
	if (Core::IsRunning())
	{
		std::string PresetPath = File::GetUserPath(D_PPSHADERSPRESETS_IDX);
		PresetPath += SConfig::GetInstance().m_strGameID + DIR_SEP;
		PresetPath += m_shader_name + ".ini";
		if (File::Exists(PresetPath))
		{
			//Override with specific game settings
			ini.Load(PresetPath);
			IniFile::Section* gameini_section = ini.GetOrCreateSection("options");
			LoadOptionsConfigurationFromSection(gameini_section);
		}
	}
	m_configuration_buffer_dirty = true;
}

void PostProcessingShaderConfiguration::SaveOptionsConfiguration()
{
	std::string file_path;
	IniFile ini;
	IniFile::Section* section = nullptr;
	if (Core::IsRunning())
	{
		file_path = File::GetUserPath(D_PPSHADERSPRESETS_IDX);
		if (!File::Exists(file_path))
		{
			File::CreateDir(file_path);
		}
		file_path += SConfig::GetInstance().m_strGameID + DIR_SEP;
		if (!File::Exists(file_path))
		{
			File::CreateDir(file_path);
		}
		file_path += m_shader_name + ".ini";
		ini.Load(file_path);
		section = ini.GetOrCreateSection("options");
	}
	else
	{
		file_path = File::GetUserPath(D_PPSHADERSPRESETS_IDX);
		if (!File::Exists(file_path))
		{
			File::CreateDir(file_path);
		}
		file_path += m_shader_name + ".ini";
		ini.Load(file_path);
		section = ini.GetOrCreateSection("options");
	}

	for (auto& it : m_options)
	{
		auto& current = it.second;
		switch (current.m_type)
		{
		case POST_PROCESSING_OPTION_TYPE_BOOL:
		{
			section->Set(current.m_option_name, current.m_bool_value);
		}
		break;
		case POST_PROCESSING_OPTION_TYPE_INTEGER:
		{
			std::string value = "";
			for (size_t i = 0; i < current.m_integer_values.size(); ++i)
				value += StringFromFormat("%d%s", current.m_integer_values[i], i == (current.m_integer_values.size() - 1) ? "" : ", ");
			section->Set(current.m_option_name, value);
		}
		break;
		case POST_PROCESSING_OPTION_TYPE_FLOAT:
		{
			std::ostringstream value;
			value.imbue(std::locale("C"));

			for (size_t i = 0; i < current.m_float_values.size(); ++i)
			{
				value << current.m_float_values[i];
				if (i != (current.m_float_values.size() - 1))
					value << ", ";
			}
			section->Set(current.m_option_name, value.str());
		}
		break;
		}
	}
	ini.Save(file_path);
}

void PostProcessingShaderConfiguration::SetOptionf(const std::string& option, int index, float value)
{
	auto it = m_options.find(option);

	if (it->second.m_float_values[index] == value)
		return;

	it->second.m_float_values[index] = value;
	it->second.m_dirty = true;
	m_any_options_dirty = true;

	if (it->second.m_compile_time_constant)
		m_compile_time_constants_dirty = true;
}

void PostProcessingShaderConfiguration::SetOptioni(const std::string& option, int index, s32 value)
{
	auto it = m_options.find(option);

	if (it->second.m_integer_values[index] == value)
		return;

	it->second.m_integer_values[index] = value;
	it->second.m_dirty = true;
	m_any_options_dirty = true;

	if (it->second.m_compile_time_constant)
		m_compile_time_constants_dirty = true;
}

void PostProcessingShaderConfiguration::SetOptionb(const std::string& option, bool value)
{
	auto it = m_options.find(option);

	if (it->second.m_bool_value == value)
		return;

	it->second.m_bool_value = value;
	it->second.m_dirty = true;
	m_any_options_dirty = true;

	if (it->second.m_compile_time_constant)
		m_compile_time_constants_dirty = true;
}

PostProcessingShader::~PostProcessingShader()
{
	for (size_t i = 0; i < m_prev_frame_texture.size(); i++)
	{
		m_prev_frame_texture[i].color_frame;
		m_prev_frame_texture[i].depth_frame;
	}
	for (RenderPassData& pass : m_passes)
	{
		for (InputBinding& input : pass.inputs)
		{
			// External textures
			if (input.texture)
			{
				delete input.texture;
			}
		}
		if (pass.output_texture)
		{
			delete pass.output_texture;
		}
	}
}

TextureCacheBase::TCacheEntryBase* PostProcessingShader::GetLastPassOutputTexture() const
{
	return m_passes[m_last_pass_index].output_texture;
}

bool PostProcessingShader::IsLastPassScaled() const
{
	return (m_passes[m_last_pass_index].output_size != m_internal_size);
}

bool PostProcessingShader::Initialize(PostProcessingShaderConfiguration* config, int target_layers)
{
	m_internal_layers = target_layers;
	m_config = config;
	m_ready = false;
	m_prev_frame_index = -1;
	if (!CreatePasses())
		return false;

	// Set size to zero, that way it'll always be reconfigured on first use
	m_internal_size.Set(0, 0);
	m_ready = RecompileShaders();
	return m_ready;
}

bool PostProcessingShader::Reconfigure(const TargetSize& new_size)
{
	m_ready = true;

	const bool size_changed = (m_internal_size != new_size);
	if (size_changed)
		m_ready = ResizeOutputTextures(new_size);

	// Re-link on size change due to the input pointer changes
	if (m_ready && (m_config->IsDirty() || size_changed))
		LinkPassOutputs();

	// Recompile shaders if compile-time constants have changed
	if (m_ready && m_config->IsCompileTimeConstantsDirty())
		m_ready = RecompileShaders();

	return m_ready;
}

bool PostProcessingShader::CreatePasses()
{
	m_passes.reserve(m_config->GetPasses().size());
	for (const auto& pass_config : m_config->GetPasses())
	{
		RenderPassData pass;
		pass.output_texture = nullptr;
		pass.output_scale = pass_config.output_scale;
		pass.enabled = true;
		pass.inputs.reserve(pass_config.inputs.size());

		for (const PostProcessingShaderConfiguration::RenderPass::Input& input_config : pass_config.inputs)
		{
			InputBinding input;
			input.type = input_config.type;
			input.texture_unit = input_config.texture_unit;
			input.texture = nullptr;
			input.texture_sampler = 0;

			if (input.type == POST_PROCESSING_INPUT_TYPE_IMAGE)
			{
				TextureCacheBase::TCacheEntryConfig config;
				config.width = input_config.external_image_size.width;
				config.height = input_config.external_image_size.height;
				config.pcformat = PC_TexFormat::PC_TEX_FMT_RGBA32;
				config.rendertarget = false;

				input.texture = g_texture_cache->AllocateTexture(config);
				input.texture->Load(input_config.external_image_data.get(), config.width, config.height, config.width, 0);
				input.size = input_config.external_image_size;
			}

			// If set to previous pass, but we are the first pass, use the color buffer instead.
			if (input.type == POST_PROCESSING_INPUT_TYPE_PREVIOUS_PASS_OUTPUT && m_passes.empty())
				input.type = POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER;

			input.texture_sampler = CreateBindingSampler(input_config);

			if (!input.texture_sampler)
			{
				ERROR_LOG(VIDEO, "Failed to create post-processing sampler for shader %s (pass %s)", m_config->GetShaderName().c_str(), pass_config.entry_point.c_str());
				return false;
			}

			pass.inputs.push_back(std::move(input));
		}
		m_passes.push_back(std::move(pass));
	}
	return true;
}

void PostProcessingShader::LinkPassOutputs()
{
	m_last_pass_index = 0;
	m_last_pass_uses_color_buffer = false;

	// Update dependant options (enable/disable passes)
	for (size_t pass_index = 0; pass_index < m_passes.size(); pass_index++)
	{
		const PostProcessingShaderConfiguration::RenderPass& pass_config = m_config->GetPass(pass_index);
		RenderPassData& pass = m_passes[pass_index];
		pass.enabled = pass_config.CheckEnabled();
		if (!pass.enabled)
			continue;
		size_t previous_pass_index = m_last_pass_index;
		m_last_pass_index = pass_index;
		m_last_pass_uses_color_buffer = false;
		for (size_t input_index = 0; input_index < pass_config.inputs.size(); input_index++)
		{
			InputBinding& input_binding = pass.inputs[input_index];
			switch (input_binding.type)
			{
			case POST_PROCESSING_INPUT_TYPE_PASS_FRAME_OUTPUT:
				input_binding.frame_index = pass_config.inputs[input_index].pass_output_index;
				break;
			case POST_PROCESSING_INPUT_TYPE_PASS_OUTPUT:
			case POST_PROCESSING_INPUT_TYPE_PREVIOUS_PASS_OUTPUT:
			{
				s32 pass_output_index = (input_binding.type == POST_PROCESSING_INPUT_TYPE_PASS_OUTPUT) ?
					static_cast<s32>(pass_config.inputs[input_index].pass_output_index)
					: static_cast<s32>(previous_pass_index);
				while (pass_output_index >= 0)
				{
					if (m_passes[pass_output_index].enabled)
					{
						break;
					}
					pass_output_index--;
				}
				if (pass_output_index < 0)
				{
					input_binding.prev_texture = nullptr;
					m_last_pass_uses_color_buffer = true;
				}
				else
				{
					input_binding.prev_texture = m_passes[pass_output_index].output_texture;
					input_binding.size = m_passes[pass_output_index].output_size;
				}
			}
			break;
			case POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER:
				m_last_pass_uses_color_buffer = true;
				break;

			default:
				break;
			}
		}
	}
}

bool PostProcessingShader::ResizeOutputTextures(const TargetSize& new_size)
{
	const PostProcessingShaderConfiguration::FrameOutput& frameoutput = m_config->GetFrameOutput();
	m_prev_frame_size = PostProcessor::ScaleTargetSize(new_size, frameoutput.color_output_scale);
	m_prev_depth_frame_size = PostProcessor::ScaleTargetSize(new_size, frameoutput.depth_scale);
	m_prev_frame_enabled = frameoutput.color_count > 0;
	m_prev_depth_enabled = frameoutput.depth_count > 0;
	for (size_t i = 0; i < m_prev_frame_texture.size(); i++)
	{
		g_texture_cache->DisposeTexture(m_prev_frame_texture[i].color_frame);
		g_texture_cache->DisposeTexture(m_prev_frame_texture[i].depth_frame);
	}
	m_prev_frame_texture.resize(std::max(frameoutput.color_count, frameoutput.depth_count));
	TextureCacheBase::TCacheEntryConfig config;
	config.width = m_prev_frame_size.width;
	config.height = m_prev_frame_size.height;

	config.rendertarget = true;
	config.layers = m_internal_layers;

	for (size_t i = 0; i < m_prev_frame_texture.size(); i++)
	{
		config.pcformat = PC_TexFormat::PC_TEX_FMT_RGBA32;
		if (i < static_cast<size_t>(frameoutput.color_count))
			m_prev_frame_texture[i].color_frame = g_texture_cache->AllocateTexture(config);
		config.pcformat = PC_TexFormat::PC_TEX_FMT_DEPTH_FLOAT;
		if (i < static_cast<size_t>(frameoutput.depth_count))
			m_prev_frame_texture[i].depth_frame = g_texture_cache->AllocateTexture(config);
	}
	config.pcformat = PC_TexFormat::PC_TEX_FMT_RGBA32;
	for (size_t pass_index = 0; pass_index < m_passes.size(); pass_index++)
	{
		RenderPassData& pass = m_passes[pass_index];
		const PostProcessingShaderConfiguration::RenderPass& pass_config = m_config->GetPass(pass_index);
		pass.output_size = PostProcessor::ScaleTargetSize(new_size, pass_config.output_scale);

		if (pass.output_texture != nullptr)
		{
			g_texture_cache->DisposeTexture(pass.output_texture);
			pass.output_texture = nullptr;
		}

		config.width = pass.output_size.width;
		config.height = pass.output_size.height;
		// Last pass output is always RGBA32
		config.pcformat = pass_index < m_passes.size() - 1 ? pass.output_format : PC_TexFormat::PC_TEX_FMT_RGBA32;
		pass.output_texture = g_texture_cache->AllocateTexture(config);
	}
	m_internal_size = new_size;
	return true;
}

PostProcessor::PostProcessor(API_TYPE apitype) : m_APIType(apitype)
{
	m_timer.Start();
}

PostProcessor::~PostProcessor()
{
	m_timer.Stop();
	if (m_color_copy_texture)
	{
		delete m_color_copy_texture;
		m_color_copy_texture = nullptr;
	}
	if (m_depth_copy_texture)
	{
		delete m_depth_copy_texture;
		m_depth_copy_texture = nullptr;
	}
	if (m_stereo_buffer_texture)
	{
		delete m_stereo_buffer_texture;
		m_stereo_buffer_texture = nullptr;
	}
}

void PostProcessor::DisablePostProcessor()
{
	m_post_processing_shaders.clear();
	m_active = false;
}

void PostProcessor::ReloadShaders()
{
	m_reload_flag.Clear();
	m_post_processing_shaders.clear();
	m_scaling_shader.reset();
	m_stereo_shader.reset();
	m_active = false;

	ReloadShaderConfigs();

	if (g_ActiveConfig.bPostProcessingEnable)
		CreatePostProcessingShaders();

	CreateScalingShader();

	if (m_stereo_config)
		CreateStereoShader();

	// Set initial sizes to 0,0 to force texture creation on next draw
	m_copy_size.Set(0, 0);
	m_stereo_buffer_size.Set(0, 0);
	Constant empty = {};
	m_current_constants.fill(empty);
}

bool PostProcessor::ShouldTriggerOnSwap() const
{
	return g_ActiveConfig.bPostProcessingEnable &&
		g_ActiveConfig.iPostProcessingTrigger == POST_PROCESSING_TRIGGER_ON_SWAP &&
		m_active;
}

bool PostProcessor::ShouldTriggerAfterBlit() const
{
	return g_ActiveConfig.bPostProcessingEnable &&
		g_ActiveConfig.iPostProcessingTrigger == POST_PROCESSING_TRIGGER_AFTER_BLIT &&
		m_active;
}

bool PostProcessor::XFBDepthDataRequired() const
{
	return (m_scaling_config && m_scaling_config->RequiresDepthBuffer())
		|| (g_ActiveConfig.bPostProcessingEnable &&
			m_active &&
			(g_ActiveConfig.iPostProcessingTrigger == POST_PROCESSING_TRIGGER_AFTER_BLIT ||
			(g_ActiveConfig.iPostProcessingTrigger == POST_PROCESSING_TRIGGER_ON_SWAP && !g_ActiveConfig.bUseXFB)));
}

void  PostProcessor::DoEFB(const TargetRectangle* src_rect)
{
	TargetSize target_size(g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight());
	TargetRectangle target_rect;
	if (src_rect)
	{
		target_rect = { src_rect->left, src_rect->top, src_rect->right, src_rect->bottom };
		if (m_APIType == API_OPENGL)
		{
			// hack to avoid vieport erro in pp shaders, it works well on amd but fails in everything else
			// TODO: investigate the reazon
			target_rect.bottom = 0;
			target_rect.top = target_size.height;
		}
	}
	else
	{
		// Copied fromg_renderer->SetViewport
		int scissorXOff = bpmem.scissorOffset.x * 2;
		int scissorYOff = bpmem.scissorOffset.y * 2;
		float X =g_renderer->EFBToScaledXf(xfmem.viewport.xOrig - xfmem.viewport.wd - (float)scissorXOff);
		float Y;
		if (m_APIType == API_OPENGL)
		{
			Y =g_renderer->EFBToScaledYf((float)EFB_HEIGHT - xfmem.viewport.yOrig + xfmem.viewport.ht +
				(float)scissorYOff);
		}
		else
		{
			Y =g_renderer->EFBToScaledYf(xfmem.viewport.yOrig + xfmem.viewport.ht - (float)scissorYOff);
		}
		
		float Width =g_renderer->EFBToScaledXf(2.0f * xfmem.viewport.wd);
		float Height =g_renderer->EFBToScaledYf(-2.0f * xfmem.viewport.ht);
		if (Width < 0)
		{
			X += Width;
			Width *= -1;
		}
		if (Height < 0)
		{
			Y += Height;
			Height *= -1;
		}
		target_rect = { static_cast<int>(X), static_cast<int>(Y),
			static_cast<int>(X + Width), static_cast<int>(Y + Height) };
		if (m_APIType == API_OPENGL)
		{
			std::swap(target_rect.top, target_rect.bottom);
		}
	}
	PostProcessEFB(target_rect, target_size);
}

void PostProcessor::OnProjectionLoaded(u32 type)
{
	if (!m_active || !g_ActiveConfig.bPostProcessingEnable ||
		(g_ActiveConfig.iPostProcessingTrigger != POST_PROCESSING_TRIGGER_ON_PROJECTION &&
		(g_ActiveConfig.iPostProcessingTrigger != POST_PROCESSING_TRIGGER_ON_EFB_COPY)))
	{
		return;
	}

	if (type == GX_PERSPECTIVE)
	{
		// Only adjust the flag if this is our first perspective load.
		if (m_projection_state == PROJECTION_STATE_INITIAL)
			m_projection_state = PROJECTION_STATE_PERSPECTIVE;
	}
	else if (type == GX_ORTHOGRAPHIC)
	{
		// Fire off postprocessing on the current efb if a perspective scene has been drawn.
		if (g_ActiveConfig.iPostProcessingTrigger == POST_PROCESSING_TRIGGER_ON_PROJECTION &&
			m_projection_state == PROJECTION_STATE_PERSPECTIVE)
		{
			m_projection_state = PROJECTION_STATE_FINAL;
			DoEFB(nullptr);
		}
	}
}

void PostProcessor::OnEFBCopy(const TargetRectangle* src_rect)
{
	if (!m_active || !g_ActiveConfig.bPostProcessingEnable ||
		g_ActiveConfig.iPostProcessingTrigger != POST_PROCESSING_TRIGGER_ON_EFB_COPY)
	{
		return;
	}

	// Fire off postprocessing on the current efb if a perspective scene has been drawn.
	if (m_projection_state == PROJECTION_STATE_PERSPECTIVE
		&& (src_rect == nullptr || (src_rect->GetWidth() > ((g_renderer->GetTargetWidth() * 2) / 3))))
	{
		DoEFB(src_rect);
		m_projection_state = PROJECTION_STATE_FINAL;
	}
}

void PostProcessor::OnEndFrame()
{
	if (!m_active || !g_ActiveConfig.bPostProcessingEnable ||
		(g_ActiveConfig.iPostProcessingTrigger != POST_PROCESSING_TRIGGER_ON_PROJECTION &&
		(g_ActiveConfig.iPostProcessingTrigger != POST_PROCESSING_TRIGGER_ON_EFB_COPY)))
	{
		return;
	}

	// If we didn't switch to orthographic after perspective, post-process now (e.g. if no HUD was drawn)
	if (m_projection_state == PROJECTION_STATE_PERSPECTIVE)
		DoEFB(nullptr);

	m_projection_state = PROJECTION_STATE_INITIAL;
}

PostProcessingShaderConfiguration* PostProcessor::GetPostShaderConfig(const std::string& shader_name)
{
	const auto& it = m_shader_configs.find(shader_name);
	if (it == m_shader_configs.end())
		return nullptr;

	return it->second.get();
}

void PostProcessor::ReloadShaderConfigs()
{
	ReloadPostProcessingShaderConfigs();
	ReloadScalingShaderConfig();
	ReloadStereoShaderConfig();
}

void PostProcessor::ReloadPostProcessingShaderConfigs()
{
	// Load post-processing shader list
	m_shader_names.clear();
	SplitString(g_ActiveConfig.sPostProcessingShaders, ':', m_shader_names);

	// Load shaders
	m_shader_configs.clear();
	m_requires_depth_buffer = false;
	for (const std::string& shader_name : m_shader_names)
	{
		// Shaders can be repeated. In this case, only load it once.
		if (m_shader_configs.find(shader_name) != m_shader_configs.end())
			continue;

		// Load this shader.
		std::unique_ptr<PostProcessingShaderConfiguration> shader_config = std::make_unique<PostProcessingShaderConfiguration>();
		if (!shader_config->LoadShader(POSTPROCESSING_SHADER_SUBDIR, shader_name))
		{
			ERROR_LOG(VIDEO, "Failed to load postprocessing shader ('%s'). This shader will be ignored.", shader_name.c_str());
			OSD::AddMessage(StringFromFormat("Failed to load postprocessing shader ('%s'). This shader will be ignored.", shader_name.c_str()));
			continue;
		}

		// Store in map for use by backend
		m_requires_depth_buffer |= shader_config->RequiresDepthBuffer();
		m_shader_configs[shader_name] = std::move(shader_config);
	}
}

void PostProcessor::ReloadScalingShaderConfig()
{
	m_scaling_config = std::make_unique<PostProcessingShaderConfiguration>();
	if (!m_scaling_config->LoadShader(SCALING_SHADER_SUBDIR, g_ActiveConfig.sScalingShader))
	{
		ERROR_LOG(VIDEO, "Failed to load scaling shader ('%s'). Falling back to copy shader.", g_ActiveConfig.sScalingShader.c_str());
		OSD::AddMessage(StringFromFormat("Failed to load scaling shader ('%s'). Falling back to copy shader.", g_ActiveConfig.sScalingShader.c_str()));
		m_scaling_config.reset();
	}
}

void PostProcessor::ReloadStereoShaderConfig()
{
	m_stereo_config.reset();
	if (g_ActiveConfig.iStereoMode == STEREO_SHADER)
	{
		m_stereo_config = std::make_unique<PostProcessingShaderConfiguration>();
		if (!m_stereo_config->LoadShader(STEREO_SHADER_SUBDIR, g_ActiveConfig.sStereoShader))
		{
			ERROR_LOG(VIDEO, "Failed to load scaling shader ('%s'). Falling back to blit.", g_ActiveConfig.sStereoShader.c_str());
			OSD::AddMessage(StringFromFormat("Failed to load scaling shader ('%s'). Falling back to blit.", g_ActiveConfig.sStereoShader.c_str()));
			m_stereo_config.reset();
		}
	}
}

TargetSize PostProcessor::ScaleTargetSize(const TargetSize& orig_size, float scale)
{
	TargetSize size;

	// negative scale means scale to native first
	if (scale < 0.0f)
	{
		float native_scale = -scale;
		int native_width = orig_size.width * EFB_WIDTH / g_renderer->GetTargetWidth();
		int native_height = orig_size.height * EFB_HEIGHT / g_renderer->GetTargetHeight();
		size.width = std::max(static_cast<int>(std::round(((float)native_width * native_scale))), 1);
		size.height = std::max(static_cast<int>(std::round(((float)native_height * native_scale))), 1);

	}
	else
	{
		size.width = std::max(static_cast<int>(std::round((float)orig_size.width * scale)), 1);
		size.height = std::max(static_cast<int>(std::round((float)orig_size.height * scale)), 1);
	}

	return size;
}

TargetRectangle PostProcessor::ScaleTargetRectangle(API_TYPE api, const TargetRectangle& src, float scale)
{
	TargetRectangle dst;

	// negative scale means scale to native first
	if (scale < 0.0f)
	{
		float native_scale = -scale;
		int native_left = src.left * EFB_WIDTH / g_renderer->GetTargetWidth();
		int native_right = src.right * EFB_WIDTH / g_renderer->GetTargetWidth();
		int native_top = src.top * EFB_HEIGHT / g_renderer->GetTargetHeight();
		int native_bottom = src.bottom * EFB_HEIGHT / g_renderer->GetTargetHeight();
		dst.left = static_cast<int>(std::round((float)native_left * native_scale));
		dst.right = static_cast<int>(std::round((float)native_right * native_scale));
		dst.top = static_cast<int>(std::round((float)native_top * native_scale));
		dst.bottom = static_cast<int>(std::round((float)native_bottom * native_scale));
	}
	else
	{
		dst.left = static_cast<int>(std::round((float)src.left * scale));
		dst.right = static_cast<int>(std::round((float)src.right * scale));
		dst.top = static_cast<int>(std::round((float)src.top * scale));
		dst.bottom = static_cast<int>(std::round((float)src.bottom * scale));
	}

	// D3D can't handle zero viewports, so protect against this here
	if (api == API_D3D11)
	{
		dst.right = std::max(dst.right, dst.left + 1);
		dst.bottom = std::max(dst.bottom, dst.top + 1);
	}

	return dst;
}

void PostProcessor::CreatePostProcessingShaders()
{
	for (const std::string& shader_name : m_shader_names)
	{
		const auto& it = m_shader_configs.find(shader_name);
		if (it == m_shader_configs.end())
			continue;

		std::unique_ptr<PostProcessingShader> shader = CreateShader(it->second.get());
		if (!shader)
		{
			ERROR_LOG(VIDEO, "Failed to initialize postprocessing shader ('%s'). This shader will be ignored.", shader_name.c_str());
			OSD::AddMessage(StringFromFormat("Failed to initialize postprocessing shader ('%s'). This shader will be ignored.", shader_name.c_str()));
			continue;
		}

		m_post_processing_shaders.push_back(std::move(shader));
	}

	// If no shaders initialized successfully, disable post processing
	m_active = !m_post_processing_shaders.empty();
	if (m_active)
	{
		DEBUG_LOG(VIDEO, "Postprocessing is enabled with %u shaders in sequence.", (u32)m_post_processing_shaders.size());
		OSD::AddMessage(StringFromFormat("Postprocessing is enabled with %u shaders in sequence.", (u32)m_post_processing_shaders.size()));
	}
}

void PostProcessor::CreateScalingShader()
{
	if (!m_scaling_config)
		return;

	m_scaling_shader = CreateShader(m_scaling_config.get());
	if (!m_scaling_shader)
	{
		ERROR_LOG(VIDEO, "Failed to initialize scaling shader ('%s'). Falling back to copy shader.", m_scaling_config->GetShaderName().c_str());
		OSD::AddMessage(StringFromFormat("Failed to initialize scaling shader ('%s'). Falling back to copy shader.", m_scaling_config->GetShaderName().c_str()));
		m_scaling_shader.reset();
	}
}

void PostProcessor::CreateStereoShader()
{
	if (!m_stereo_config)
		return;

	m_stereo_shader = CreateShader(m_stereo_config.get());
	if (!m_stereo_shader)
	{
		ERROR_LOG(VIDEO, "Failed to initialize stereoscopy shader ('%s'). Falling back to blit.", m_scaling_config->GetShaderName().c_str());
		OSD::AddMessage(StringFromFormat("Failed to initialize stereoscopy shader ('%s'). Falling back to blit.", m_scaling_config->GetShaderName().c_str()));
		m_stereo_shader.reset();
	}
}

bool PostProcessor::ResizeCopyBuffers(const TargetSize& size, int layers)
{
	if (m_copy_size == size && m_copy_layers == layers)
		return true;

	// reset before creating, in case it fails
	if (m_color_copy_texture)
	{
		g_texture_cache->DisposeTexture(m_color_copy_texture);
		m_color_copy_texture = nullptr;
	}
	if (m_depth_copy_texture)
	{
		g_texture_cache->DisposeTexture(m_depth_copy_texture);
		m_depth_copy_texture = nullptr;
	}
	m_copy_size.Set(0, 0);
	m_copy_layers = 0;

	TextureCacheBase::TCacheEntryConfig config;
	config.width = size.width;
	config.height = size.height;
	config.pcformat = PC_TexFormat::PC_TEX_FMT_RGBA32;
	config.rendertarget = true;
	config.layers = layers;
	m_color_copy_texture = g_texture_cache->AllocateTexture(config);
	config.pcformat = PC_TexFormat::PC_TEX_FMT_DEPTH_FLOAT;
	m_depth_copy_texture = g_texture_cache->AllocateTexture(config);
	if (m_color_copy_texture && m_depth_copy_texture)
	{
		m_copy_size = size;
		m_copy_layers = layers;
	}
	return true;
}

bool PostProcessor::ResizeStereoBuffer(const TargetSize& size)
{
	if (m_stereo_buffer_size == size)
		return true;

	if (m_stereo_buffer_texture)
	{
		g_texture_cache->DisposeTexture(m_stereo_buffer_texture);
		m_stereo_buffer_texture = nullptr;
	}
	m_stereo_buffer_size.Set(0, 0);
	TextureCacheBase::TCacheEntryConfig config;
	config.width = size.width;
	config.height = size.height;
	config.pcformat = PC_TexFormat::PC_TEX_FMT_RGBA32;
	config.rendertarget = true;
	config.layers = 2;
	m_stereo_buffer_texture = g_texture_cache->AllocateTexture(config);
	if (m_stereo_buffer_texture)
	{
		m_stereo_buffer_size = size;
	}
	return true;
}

bool PostProcessor::ReconfigurePostProcessingShaders(const TargetSize& size)
{
	for (const auto& shader : m_post_processing_shaders)
	{
		if (!shader->IsReady() || !shader->Reconfigure(size))
			return false;
	}

	// Remove dirty flags afterwards. This is because we can have the same shader twice.
	for (auto& it : m_shader_configs)
		it.second->ClearDirty();

	return true;
}

bool PostProcessor::ReconfigureScalingShader(const TargetSize& size)
{
	if (m_scaling_shader)
	{
		if (!m_scaling_shader->IsReady() ||
			!m_scaling_shader->Reconfigure(size))
		{
			m_scaling_shader.reset();
			return false;
		}

		m_scaling_config->ClearDirty();
	}

	return true;
}

bool PostProcessor::ReconfigureStereoShader(const TargetSize& size)
{
	if (m_stereo_shader)
	{
		if (!m_stereo_shader->IsReady() ||
			!m_stereo_shader->Reconfigure(size) ||
			!ResizeStereoBuffer(size))
		{
			m_stereo_shader.reset();
			return false;
		}

		m_stereo_config->ClearDirty();
	}

	return true;
}

void PostProcessor::BlitScreen(const TargetRectangle& dst_rect, const TargetSize& dst_size, uintptr_t dst_texture,
	const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_texture, uintptr_t src_depth_texture,
	int src_layer, float gamma)
{
	const bool triguer_after_blit = ShouldTriggerAfterBlit();
	_dbg_assert_msg_(VIDEO, src_layer >= 0, "BlitToFramebuffer should always be called with a single source layer");

	ReconfigureScalingShader(src_size);
	ReconfigureStereoShader(dst_size);
	if (triguer_after_blit)
	{
		TargetSize buffer_size(dst_rect.GetWidth(), dst_rect.GetHeight());
		if (!ResizeCopyBuffers(buffer_size, FramebufferManagerBase::GetEFBLayers()) ||
			!ReconfigurePostProcessingShaders(buffer_size))
		{
			ERROR_LOG(VIDEO, "Failed to update post-processor state. Disabling post processor.");
			DisablePostProcessor();
			return;
		}
	}

	// Use stereo shader if enabled, otherwise invoke scaling shader, if that is invalid, fall back to blit.
	if (m_stereo_shader)
		DrawStereoBuffers(dst_rect, dst_size, dst_texture, src_rect, src_size, src_texture, src_depth_texture, gamma);
	else if (triguer_after_blit)
	{
		TargetSize buffer_size(dst_rect.GetWidth(), dst_rect.GetHeight());
		TargetRectangle buffer_rect(0, 0, buffer_size.width, 0);
		if (m_APIType == API_OPENGL)
		{
			buffer_rect.top = buffer_size.height;
		}
		else
		{
			buffer_rect.bottom = buffer_size.height;
		}
		if (m_scaling_shader)
		{
			m_scaling_shader->Draw(this, buffer_rect, buffer_size, m_color_copy_texture->GetInternalObject(), src_rect, src_size, src_texture, src_depth_texture, src_layer, gamma);
		}
		else
		{
			CopyTexture(buffer_rect, m_color_copy_texture->GetInternalObject(), src_rect, src_texture, src_size, -1);
		}
		PostProcess(nullptr, nullptr, nullptr, buffer_rect, buffer_size, m_color_copy_texture->GetInternalObject(), src_rect, src_size, src_depth_texture, dst_texture, &dst_rect, &dst_size);
	}
	else if (m_scaling_shader)
		m_scaling_shader->Draw(this, dst_rect, dst_size, dst_texture, src_rect, src_size, src_texture, src_depth_texture, src_layer, gamma);
	else
		CopyTexture(dst_rect, dst_texture, src_rect, src_texture, src_size, src_layer);
}

void PostProcessor::PostProcess(TargetRectangle* output_rect, TargetSize* output_size, uintptr_t* output_texture,
	const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_texture,
	const TargetRectangle& src_depth_rect, const TargetSize& src_depth_size, uintptr_t src_depth_texture,
	uintptr_t dst_texture, const TargetRectangle* dst_rect, const TargetSize* dst_size)
{
	if (!m_active)
		return;
	uintptr_t real_dst_texture = dst_texture == 0 && dst_rect == nullptr ? src_texture : dst_texture;
	// Setup copy buffers first, and update compile-time constants.
	TargetSize buffer_size(src_rect.GetWidth(), src_rect.GetHeight());
	if (!ResizeCopyBuffers(buffer_size, FramebufferManagerBase::GetEFBLayers()) ||
		!ReconfigurePostProcessingShaders(buffer_size))
	{
		ERROR_LOG(VIDEO, "Failed to update post-processor state. Disabling post processor.");

		// We need to fill in an output texture if we're bailing out, so set it to the source.
		if (output_texture)
		{
			*output_rect = src_rect;
			*output_size = src_size;
			*output_texture = src_texture;
		}

		DisablePostProcessor();
		return;
	}

	// Copy only the visible region to our buffers.
	TargetRectangle buffer_rect(0, 0, buffer_size.width, 0);
	if (m_APIType == API_OPENGL)
	{
		buffer_rect.top = buffer_size.height;
	}
	else
	{
		buffer_rect.bottom = buffer_size.height;
	}
	uintptr_t input_color_texture = m_color_copy_texture->GetInternalObject();
	uintptr_t input_depth_texture = m_depth_copy_texture->GetInternalObject();
	// Only copy if the size is different
	if (src_size != buffer_size || real_dst_texture == src_texture)
	{
		CopyTexture(buffer_rect, m_color_copy_texture->GetInternalObject(), src_rect, src_texture, src_size, -1);
	}
	else
	{
		input_color_texture = src_texture;
	}
	if (src_depth_texture != 0 && src_depth_size != buffer_size)
	{
		CopyTexture(buffer_rect, m_depth_copy_texture->GetInternalObject(), src_depth_rect, src_depth_texture, src_depth_size, -1, true, true);
	}
	else
	{
		input_depth_texture = src_depth_texture;
	}

	// Loop through the shader list, applying each of them in sequence.
	for (size_t shader_index = 0; shader_index < m_post_processing_shaders.size(); shader_index++)
	{
		PostProcessingShader* shader = m_post_processing_shaders[shader_index].get();

		// To save a copy, we use the output of one shader as the input to the next.
		// This works except when the last pass is scaled, as the next shader expects a full-size input, so re-use the copy buffer for this case.
		uintptr_t output_color_texture = (shader->IsLastPassScaled()) ? m_color_copy_texture->GetInternalObject() : shader->GetLastPassOutputTexture()->GetInternalObject();

		// Last shader in the sequence? If so, write to the output texture.
		if (shader_index == (m_post_processing_shaders.size() - 1))
		{
			// Are we returning our temporary texture, or storing back to the original buffer?
			if (output_texture && !dst_texture)
			{
				// Use the same texture as if it was a previous pass, and return it.
				shader->Draw(this, buffer_rect, buffer_size, output_color_texture, buffer_rect, buffer_size, input_color_texture, input_depth_texture, -1, 1.0f);
				*output_rect = buffer_rect;
				*output_size = buffer_size;
				*output_texture = output_color_texture;
			}
			else
			{
				// Write to The output texturre directly.
				shader->Draw(this, dst_rect != nullptr ? *dst_rect : src_rect, dst_size != nullptr ? *dst_size : src_size, real_dst_texture, buffer_rect, buffer_size, input_color_texture, input_depth_texture, -1, 1.0f);
				if (output_texture)
				{
					*output_rect = buffer_rect;
					*output_size = buffer_size;
					*output_texture = real_dst_texture;
				}
			}
		}
		else
		{
			shader->Draw(this, buffer_rect, buffer_size, output_color_texture, buffer_rect, buffer_size, input_color_texture, input_depth_texture, -1, 1.0f);
			input_color_texture = output_color_texture;
		}
	}
}

void PostProcessor::DrawStereoBuffers(const TargetRectangle& dst_rect, const TargetSize& dst_size, uintptr_t dst_texture,
	const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_texture, uintptr_t src_depth_texture, float gamma)
{
	const bool triguer_after_blit = ShouldTriggerAfterBlit();
	uintptr_t stereo_buffer = (m_scaling_shader || triguer_after_blit) ? m_stereo_buffer_texture->GetInternalObject() : src_texture;
	TargetRectangle stereo_buffer_rect(src_rect);
	TargetSize stereo_buffer_size(src_size);

	// Apply scaling shader if enabled, otherwise just use the source buffers
	if (triguer_after_blit)
	{
		stereo_buffer_rect = TargetRectangle(0, 0, dst_size.width, 0);
		if (m_APIType == API_OPENGL)
		{
			stereo_buffer_rect.top = dst_size.height;
		}
		else
		{
			stereo_buffer_rect.bottom = dst_size.height;
		}
		stereo_buffer_size = dst_size;
		TargetSize buffer_size(dst_rect.GetWidth(), dst_rect.GetHeight());
		TargetRectangle buffer_rect(0, 0, buffer_size.width, 0);
		if (m_APIType == API_OPENGL)
		{
			buffer_rect.top = buffer_size.height;
		}
		else
		{
			buffer_rect.bottom = buffer_size.height;
		}
		if (m_scaling_shader)
		{
			m_scaling_shader->Draw(this, buffer_rect, buffer_size, m_color_copy_texture->GetInternalObject(), src_rect, src_size, src_texture, src_depth_texture, -1, gamma);
		}
		else
		{
			CopyTexture(buffer_rect, m_color_copy_texture->GetInternalObject(), src_rect, src_texture, src_size, -1);
		}
		PostProcess(nullptr, nullptr, nullptr, buffer_rect, buffer_size, m_color_copy_texture->GetInternalObject(), src_rect, src_size, src_depth_texture, stereo_buffer, &stereo_buffer_rect, &stereo_buffer_size);
	}
	else if (m_scaling_shader)
	{
		stereo_buffer_rect = TargetRectangle(0, 0, dst_size.width, 0);
		if (m_APIType == API_OPENGL)
		{
			stereo_buffer_rect.top = dst_size.height;
		}
		else
		{
			stereo_buffer_rect.bottom = dst_size.height;
		}
		stereo_buffer_size = dst_size;
		m_scaling_shader->Draw(this, stereo_buffer_rect, stereo_buffer_size, stereo_buffer, src_rect, src_size, src_texture, src_depth_texture, -1, gamma);
	}
	m_stereo_shader->Draw(this, dst_rect, dst_size, dst_texture, stereo_buffer_rect, stereo_buffer_size, stereo_buffer, 0, 0, 1.0f);
}

const std::string PostProcessor::s_post_fragment_header_ogl = R"(
// Depth value is not inverted for GL
#define DEPTH_VALUE(val) (val)
// Shader inputs/outputs
SAMPLER_BINDING(9) uniform sampler2DArray pp_inputs[8];
in float2 v_source_uv;
in float2 v_target_uv;
flat in float v_layer;
out float4 ocol0;
// Input sampling wrappers. Has to be a macro because the array index must be a constant expression.
#define SampleInput(index) (texture(pp_inputs[index], float3(v_source_uv, v_layer)))
#define SampleInputLocation(index, location) (texture(pp_inputs[index], float3(location, v_layer)))
#define SampleInputLayer(index, layer) (texture(pp_inputs[index], float3(v_source_uv, float(layer))))
#define SampleInputLayerLocation(index, layer, location) (texture(pp_inputs[index], float3(location, float(layer))))
#define GetFragmentCoord() (gl_FragCoord.xy)
#define GetTargetCoordinates() (v_target_uv)
#define GetCoordinates() (v_source_uv)
#define GetLayer() (v_layer)
// Input sampling with offset, macro because offset must be a constant expression.
#define SampleInputOffset(index, offset) (textureOffset(pp_inputs[index], float3(v_source_uv, v_layer), offset))
#define SampleInputLayerOffset(index, layer, offset) (textureOffset(pp_inputs[index], float3(v_source_uv, float(layer)), offset))
float4 GetBicubicSampleLocation(int idx, float2 location, out float4 scalingFactor);

float4 SampleInputBicubicLocation0(float2 location)
{
	float4 scalingFactor;
	float4 texCoord = GetBicubicSampleLocation(0, location, scalingFactor);
	return
		 SampleInputLocation(0, texCoord.xy) * scalingFactor.x +
		 SampleInputLocation(0, texCoord.zy) * scalingFactor.y +
		 SampleInputLocation(0, texCoord.xw) * scalingFactor.z +
		 SampleInputLocation(0, texCoord.zw) * scalingFactor.w;
}

float4 SampleInputBicubicLocation1(float2 location)
{
	float4 scalingFactor;
	float4 texCoord = GetBicubicSampleLocation(1, location, scalingFactor);
	return
		 SampleInputLocation(1, texCoord.xy) * scalingFactor.x +
		 SampleInputLocation(1, texCoord.zy) * scalingFactor.y +
		 SampleInputLocation(1, texCoord.xw) * scalingFactor.z +
		 SampleInputLocation(1, texCoord.zw) * scalingFactor.w;
}

float4 SampleInputBicubicLocation2(float2 location)
{
	float4 scalingFactor;
	float4 texCoord = GetBicubicSampleLocation(2, location, scalingFactor);
	return
		 SampleInputLocation(2, texCoord.xy) * scalingFactor.x +
		 SampleInputLocation(2, texCoord.zy) * scalingFactor.y +
		 SampleInputLocation(2, texCoord.xw) * scalingFactor.z +
		 SampleInputLocation(2, texCoord.zw) * scalingFactor.w;
}

float4 SampleInputBicubicLocation3(float2 location)
{
	float4 scalingFactor;
	float4 texCoord = GetBicubicSampleLocation(3, location, scalingFactor);
	return
		 SampleInputLocation(3, texCoord.xy) * scalingFactor.x +
		 SampleInputLocation(3, texCoord.zy) * scalingFactor.y +
		 SampleInputLocation(3, texCoord.xw) * scalingFactor.z +
		 SampleInputLocation(3, texCoord.zw) * scalingFactor.w;
}

float4 SampleInputBicubic0()
{
	return SampleInputBicubicLocation0(GetCoordinates());
}

float4 SampleInputBicubic1()
{
	return SampleInputBicubicLocation1(GetCoordinates());
}

float4 SampleInputBicubic2()
{
	return SampleInputBicubicLocation2(GetCoordinates());
}

float4 SampleInputBicubic3()
{
	return SampleInputBicubicLocation3(GetCoordinates());
}

#define SampleInputBicubic(idx)  SampleInputBicubic##idx()
#define SampleInputBicubicLocation(idx, location)  SampleInputBicubicLocation##idx(location)

float4 SampleBicubicLocation(float2 location)
{
	float4 scalingFactor;
	float4 texCoord = GetBicubicSampleLocation(COLOR_BUFFER_INPUT_INDEX, location, scalingFactor);
	return
			SampleInputLocation(COLOR_BUFFER_INPUT_INDEX, texCoord.xy) * scalingFactor.x +
			SampleInputLocation(COLOR_BUFFER_INPUT_INDEX, texCoord.zy) * scalingFactor.y +
			SampleInputLocation(COLOR_BUFFER_INPUT_INDEX, texCoord.xw) * scalingFactor.z +
			SampleInputLocation(COLOR_BUFFER_INPUT_INDEX, texCoord.zw) * scalingFactor.w;
}

float4 SamplePrevBicubic()
{
	float4 scalingFactor;
	float4 texCoord = GetBicubicSampleLocation(PREV_OUTPUT_INPUT_INDEX, GetCoordinates(), scalingFactor);
	return
			SampleInputLocation(PREV_OUTPUT_INPUT_INDEX, texCoord.xy) * scalingFactor.x +
			SampleInputLocation(PREV_OUTPUT_INPUT_INDEX, texCoord.zy) * scalingFactor.y +
			SampleInputLocation(PREV_OUTPUT_INPUT_INDEX, texCoord.xw) * scalingFactor.z +
			SampleInputLocation(PREV_OUTPUT_INPUT_INDEX, texCoord.zw) * scalingFactor.w;
}

float4 SamplePrevBicubicLocation(float2 location)
{
	float4 scalingFactor;
	float4 texCoord = GetBicubicSampleLocation(PREV_OUTPUT_INPUT_INDEX, location, scalingFactor);
	return
			SampleInputLocation(PREV_OUTPUT_INPUT_INDEX, texCoord.xy) * scalingFactor.x +
			SampleInputLocation(PREV_OUTPUT_INPUT_INDEX, texCoord.zy) * scalingFactor.y +
			SampleInputLocation(PREV_OUTPUT_INPUT_INDEX, texCoord.xw) * scalingFactor.z +
			SampleInputLocation(PREV_OUTPUT_INPUT_INDEX, texCoord.zw) * scalingFactor.w;
}

float4 SampleBicubic() 
{ 
	float4 scalingFactor;
	float4 texCoord = GetBicubicSampleLocation(COLOR_BUFFER_INPUT_INDEX, GetCoordinates(), scalingFactor);
	return
			SampleInputLocation(COLOR_BUFFER_INPUT_INDEX, texCoord.xy) * scalingFactor.x +
			SampleInputLocation(COLOR_BUFFER_INPUT_INDEX, texCoord.zy) * scalingFactor.y +
			SampleInputLocation(COLOR_BUFFER_INPUT_INDEX, texCoord.xw) * scalingFactor.z +
			SampleInputLocation(COLOR_BUFFER_INPUT_INDEX, texCoord.zw) * scalingFactor.w;
}

)";

const std::string PostProcessor::s_post_fragment_header_d3d = R"(
// Depth value is inverted for D3D
#define DEPTH_VALUE(val) (1.0f - (val))

// Shader inputs
Texture2DArray pp_inputs[8] : register(t%i);
SamplerState pp_input_samplers[8] : register(s%i);
// Shadows of those read/written in main
static float v_layer;
static float2 v_source_uv, v_target_uv, v_fragcoord;
static float4 ocol0;
// Input sampling wrappers
float4 SampleInput(int index) { return pp_inputs[index].Sample(pp_input_samplers[index], float3(v_source_uv, v_layer)); }
float4 SampleInputLocation(int index, float2 location) { return pp_inputs[index].Sample(pp_input_samplers[index], float3(location, v_layer)); }
float4 SampleInputLayer(int index, int slayer) { return pp_inputs[index].Sample(pp_input_samplers[index], float3(v_source_uv, float(slayer))); }
float4 SampleInputLayerLocation(int index, int slayer, float2 location) { return pp_inputs[index].Sample(pp_input_samplers[index], float3(location, float(slayer))); }
// Input sampling with offset, macro because offset must be a constant expression.
#define SampleInputOffset(index, offset) (pp_inputs[index].Sample(pp_input_samplers[index], float3(v_source_uv, v_layer), offset))
#define SampleInputLayerOffset(index, slayer, offset) (pp_inputs[index].Sample(pp_input_samplers[index], float3(v_source_uv, float(slayer)), offset))
float2 GetFragmentCoord() { return v_fragcoord; }
float2 GetTargetCoordinates() { return v_target_uv; }
float2 GetCoordinates() { return v_source_uv; }
float GetLayer() { return v_layer; }
float4 GetBicubicSampleLocation(int idx, float2 location, out float4 scalingFactor);

float4 SampleInputBicubicLocation(int idx, float2 location)
{
	float4 scalingFactor;
	float4 texCoord = GetBicubicSampleLocation(idx, location, scalingFactor);
	return
		 SampleInputLocation(idx, texCoord.xy) * scalingFactor.x +
		 SampleInputLocation(idx, texCoord.zy) * scalingFactor.y +
		 SampleInputLocation(idx, texCoord.xw) * scalingFactor.z +
		 SampleInputLocation(idx, texCoord.zw) * scalingFactor.w;
}

float4 SampleInputBicubic(int idx)
{
	return SampleInputBicubicLocation(idx, GetCoordinates());
}

float4 SampleBicubicLocation(float2 location)
{
	return SampleInputBicubicLocation(COLOR_BUFFER_INPUT_INDEX, location);
}

float4 SamplePrevBicubic()
{
	return SampleInputBicubicLocation(PREV_OUTPUT_INPUT_INDEX, GetCoordinates());
}

float4 SamplePrevBicubicLocation(float2 location)
{
	return SampleInputBicubicLocation(PREV_OUTPUT_INPUT_INDEX, location);
}

float4 SampleBicubic() 
{ 
	return SampleInputBicubicLocation(COLOR_BUFFER_INPUT_INDEX, GetCoordinates());
}

)";

const std::string PostProcessor::s_post_fragment_header_common = R"(
// Convert z/w -> linear depth
float ToLinearDepth(float depth)
{
	// D3D has inverted depth
	float A = -499.5;
	float B =  500.5;
	depth = 1.0 / (A * depth + B);
	return depth;
}
// Input resolution accessors
float2 GetInputResolution(int index) { return u_input_resolutions[index].xy; }
float2 GetInvInputResolution(int index) { return u_input_resolutions[index].zw; }
float2 GetPrevResolution() { return u_input_resolutions[PREV_OUTPUT_INPUT_INDEX].xy; }
float2 GetInvPrevResolution() { return u_input_resolutions[PREV_OUTPUT_INPUT_INDEX].zw; }
float2 GetTargetResolution() { return u_target_resolution.xy; }
float2 GetInvTargetResolution() { return u_target_resolution.zw; }
float2 GetSourceRectOrigin() { return u_source_rect.xy; }
float2 GetSourceRectSize() { return u_source_rect.zw; }
float2 GetTargetRectOrigin() { return u_target_rect.xy; }
float2 GetTargetRectSize() { return u_target_rect.zw; }
float4 GetViewportRect() { return u_viewport_rect; }
float4 GetWindowRect() { return u_window_rect; }
float GetTime() { return u_time; }

// Interface wrappers - provided for compatibility.
float4 Sample() { return SampleInput(COLOR_BUFFER_INPUT_INDEX); }
float4 SampleLocation(float2 location) { return SampleInputLocation(COLOR_BUFFER_INPUT_INDEX, location); }
float4 SampleLayer(int layer) { return SampleInputLayer(COLOR_BUFFER_INPUT_INDEX, layer); }
float4 SampleLayerLocation(int layer, float2 location) { return SampleInputLayerLocation(COLOR_BUFFER_INPUT_INDEX, layer, location); }
float4 SamplePrev() { return SampleInput(PREV_OUTPUT_INPUT_INDEX); }
float4 SamplePrevLocation(float2 location) { return SampleInputLocation(PREV_OUTPUT_INPUT_INDEX, location); }
float SampleRawDepth() { return DEPTH_VALUE(SampleInput(DEPTH_BUFFER_INPUT_INDEX).x); }
float SampleRawDepthLocation(float2 location) { return DEPTH_VALUE(SampleInputLocation(DEPTH_BUFFER_INPUT_INDEX, location).x); }
float SampleDepth() { return ToLinearDepth(SampleRawDepth()); }
float SampleDepthLocation(float2 location) { return ToLinearDepth(SampleRawDepthLocation(location)); }

// Offset methods are macros, because the offset must be a constant expression.
#define SampleOffset(offset) (SampleInputOffset(COLOR_BUFFER_INPUT_INDEX, offset))
#define SampleLayerOffset(offset, layer) (SampleInputLayerOffset(COLOR_BUFFER_INPUT_INDEX, layer, offset))
#define SamplePrevOffset(offset) (SampleInputOffset(PREV_OUTPUT_INPUT_INDEX, offset))
#define SampleRawDepthOffset(offset) (DEPTH_VALUE(SampleInputOffset(DEPTH_BUFFER_INPUT_INDEX, offset).x))
#define SampleDepthOffset(offset) (ToLinearDepth(SampleRawDepthOffset(offset)))

// Backwards compatibility
float2 GetResolution() { return GetInputResolution(COLOR_BUFFER_INPUT_INDEX); }
float2 GetInvResolution() { return GetInvInputResolution(COLOR_BUFFER_INPUT_INDEX); }

// Variable wrappers
float4 ApplyGCGamma(float4 col) { return pow(col, float4(u_native_gamma, u_native_gamma, u_native_gamma, u_native_gamma)); }
//Random
float global_rnd_state = 1.0;
float RandomSeedfloat(float2 seed)
{
	float noise = frac(sin(dot(seed, float2(12.9898, 78.233)*2.0)) * 43758.5453);
	return noise;
}

void rnd_advance()
{
    global_rnd_state = RandomSeedfloat(GetCoordinates() + global_rnd_state);
}

uint RandomSeeduint(float2 seed)
{
	float noise = RandomSeedfloat(seed);
	return uint(noise * 0xFFFFFF);
}

void Randomize()
{
	global_rnd_state = frac(float(GetTime())*0.0001);
}

uint Rndint()
{
	rnd_advance();
	return uint(global_rnd_state * 0xFFFFFF);
}

float Rndfloat()
{
	rnd_advance();
	return global_rnd_state;
}

float2 Rndfloat2()
{
	float2 val;
	rnd_advance();
	val.x = global_rnd_state;
	rnd_advance();
	val.y = global_rnd_state;
	return val;
}

float3 Rndfloat3()
{
	float3 val;
	rnd_advance();
	val.x = global_rnd_state;
	rnd_advance();
	val.y = global_rnd_state;
	rnd_advance();
	val.z = global_rnd_state;
	return val;
}

float4 Rndfloat4()
{
	float4 val;
	rnd_advance();
	val.x = global_rnd_state;
	rnd_advance();
	val.y = global_rnd_state;
	rnd_advance();
	val.z = global_rnd_state;
	rnd_advance();
	val.w = global_rnd_state;
	return val;
}

float4 GetBicubicSampleLocation(int idx, float2 location, out float4 scalingFactor)
{
	float2 textureDimensions    = GetInputResolution(idx);
	float2 invTextureDimensions = 1.f / textureDimensions;

					location *= textureDimensions;

						float2 texelCenter   = floor( location - 0.5f ) + 0.5f;
	float2 fracOffset    = location - texelCenter;
	float2 fracOffset_x2 = fracOffset * fracOffset;
	float2 fracOffset_x3 = fracOffset * fracOffset_x2;
	float2 weight0 = fracOffset_x2 - 0.5f * ( fracOffset_x3 + fracOffset );
	float2 weight1 = 1.5f * fracOffset_x3 - 2.5f * fracOffset_x2 + 1.f;
	float2 weight3 = 0.5f * ( fracOffset_x3 - fracOffset_x2 );
	float2 weight2 = 1.f - weight0 - weight1 - weight3;

					scalingFactor = float4(weight0 + weight1,  weight2 + weight3);	
	scalingFactor = scalingFactor.xzxz * scalingFactor.yyww;
	float2 f0 = weight1 / ( weight0 + weight1 );
	float2 f1 = weight3 / ( weight2 + weight3 );

		return float4(texelCenter - 1.f + f0,texelCenter + 1.f + f1) * invTextureDimensions.xyxy;
}

#define SetOutput(color) ocol0 = color
// Option check macro
#define GetOption(x) (o_##x)
#define OptionEnabled(x) ((o_##x) != 0)
)";

void PostProcessor::GetUniformBufferShaderSource(API_TYPE api, const PostProcessingShaderConfiguration* config, std::string& shader_source)
{
	// Constant block
	if (api == API_OPENGL)
		shader_source += "layout(std140) uniform PostProcessingConstants {\n";
	else if (api == API_D3D11)
		shader_source += "cbuffer PostProcessingConstants : register(b0) {\n";

	// Common constants
	shader_source += "\tfloat4 u_input_resolutions[8];\n"
		"\tfloat4 u_target_resolution;\n"
		"\tfloat4 u_source_rect;\n"
		"\tfloat4 u_target_rect;\n"
		"\tfloat4 u_viewport_rect;\n"
		"\tfloat4 u_window_rect;\n"
		"\tfloat u_time;\n"
		"\tfloat u_src_layer;\n"
		"\tfloat u_native_gamma;\n"
		"\tuint u_scaling_filter;\n"
		"};\n";
	if (config->GetOptions().size() == 0)
	{
		return;
	}
	bool bufferpacking = false;
	// User options
	if (api == API_OPENGL)
		shader_source += "layout(std140) uniform ConfigurationConstants {\n";
	else if (api == API_D3D11)
	{
		bufferpacking = true;
		shader_source += "cbuffer ConfigurationConstants : register(b1) {\n";
	}


	u32 unused_counter = 2;
	u32 size = 0;
	const u32 align = 4;
	const u32 mask = align - 1;
	for (const auto& it : config->GetOptions())
	{
		if (it.second.m_compile_time_constant)
		{
			continue;
		}
		u32 count = 1;

		if (it.second.m_type == POST_PROCESSING_OPTION_TYPE_INTEGER)
		{
			count = static_cast<u32>(it.second.m_integer_values.size());
		}
		else if (it.second.m_type == POST_PROCESSING_OPTION_TYPE_FLOAT)
		{
			count = static_cast<u32>(it.second.m_float_values.size());
		}
		if (bufferpacking)
		{
			u32 remaining = ((size + mask) & (~mask)) - size;

			if (remaining < count && remaining > 0)
			{
				if (remaining < 2)
					shader_source += StringFromFormat("\tint unused%u_;\n", unused_counter++);
				else
					shader_source += StringFromFormat("\tint%u unused%u_;\n", remaining, unused_counter++);
				// Padding needed to compensate contant buffer padding to 16 bytes / 4 32 bits elements
				size += remaining;
			}

			size += count;
		}

		if (it.second.m_type == POST_PROCESSING_OPTION_TYPE_BOOL)
		{
			shader_source += StringFromFormat("\tint o_%s;\n", it.first.c_str());
		}
		else if (it.second.m_type == POST_PROCESSING_OPTION_TYPE_INTEGER)
		{
			count = static_cast<u32>(it.second.m_integer_values.size());

			if (count == 1)
				shader_source += StringFromFormat("\tint o_%s;\n", it.first.c_str());
			else
				shader_source += StringFromFormat("\tint%d o_%s;\n", count, it.first.c_str());
		}
		else if (it.second.m_type == POST_PROCESSING_OPTION_TYPE_FLOAT)
		{
			count = static_cast<u32>(it.second.m_float_values.size());
			if (count == 1)
				shader_source += StringFromFormat("\tfloat o_%s;\n", it.first.c_str());
			else
				shader_source += StringFromFormat("\tfloat%d o_%s;\n", count, it.first.c_str());
		}
		if (!bufferpacking)
		{
			for (u32 i = count; i < 4; i++)
				shader_source += StringFromFormat("\tint unused%u_;\n", unused_counter++);
		}
	}

	// End constant block
	shader_source += "};\n";
}

std::string PostProcessor::GetCommonFragmentShaderSource(API_TYPE api, const PostProcessingShaderConfiguration* config, int texture_register_start)
{
	std::string shader_source;
	if (api == API_OPENGL)
	{
		shader_source += s_post_fragment_header_ogl;
	}
	else if (api == API_D3D11)
	{
		shader_source += StringFromFormat(s_post_fragment_header_d3d.c_str(), texture_register_start, texture_register_start);
	}

	// Add uniform buffer
	GetUniformBufferShaderSource(api, config, shader_source);

	// Add compile-time constants
	for (const auto& it : config->GetOptions())
	{
		if (!it.second.m_compile_time_constant)
			continue;

		if (it.second.m_type == POST_PROCESSING_OPTION_TYPE_BOOL)
		{
			shader_source += StringFromFormat("#define %s (%d)\n", it.first.c_str(), (int)it.second.m_bool_value);
		}
		else if (it.second.m_type == POST_PROCESSING_OPTION_TYPE_INTEGER)
		{
			int count = static_cast<int>(it.second.m_integer_values.size());
			std::string type_str = (count == 1) ? "int" : StringFromFormat("int%d", count);
			shader_source += StringFromFormat("#define %s %s(", it.first.c_str(), type_str.c_str());
			for (int i = 0; i < count; i++)
				shader_source += StringFromInt(it.second.m_integer_values[i]);
			shader_source += ")\n";
		}
		else if (it.second.m_type == POST_PROCESSING_OPTION_TYPE_FLOAT)
		{
			int count = static_cast<int>(it.second.m_float_values.size());
			std::string type_str = (count == 1) ? "float" : StringFromFormat("float%d", count);
			shader_source += StringFromFormat("#define %s %s(", it.first.c_str(), type_str.c_str());
			for (int i = 0; i < count; i++)
				shader_source += StringFromFormat("%f", it.second.m_float_values[i]);
			shader_source += ")\n";
		}
	}

	// Remaining wrapper/interfacing functions
	shader_source += s_post_fragment_header_common;
	return shader_source;
}

std::string PostProcessor::GetPassFragmentShaderSource(
	API_TYPE api,
	const PostProcessingShaderConfiguration* config,
	const PostProcessingShaderConfiguration::RenderPass* pass)
{
	std::string shader_source;

	// Include the user's code here
	if (!pass->entry_point.empty())
	{
		shader_source += config->GetShaderSource();
		shader_source += '\n';
	}

	// API-specific wrapper
	if (api == API_OPENGL)
	{
		// No entry point? This pass should perform a copy.
		if (pass->entry_point.empty())
			shader_source += "void main() { ocol0 = SampleInput(0); }\n";
		else if (pass->entry_point != "main")
			shader_source += StringFromFormat("void main() { %s(); }\n", pass->entry_point.c_str());
	}
	else if (api == API_D3D11)
	{
		shader_source += "void passmain(in float4 in_pos : SV_Position,\n"
			"          in float2 in_srcTexCoord : TEXCOORD0,\n"
			"          in float2 in_dstTexCoord : TEXCOORD1,\n"
			"          in float in_layer : TEXCOORD2,\n"
			"          out float4 out_col0 : SV_Target)\n"
			"{\n"
			"\tv_fragcoord = u_viewport_rect.xy + in_pos.xy;\n"
			"\tv_source_uv = in_srcTexCoord;\n"
			"\tv_target_uv = in_dstTexCoord;\n"
			"\tv_layer = in_layer;\n";

		// No entry point? This pass should perform a copy.
		if (pass->entry_point.empty())
			shader_source += "\tocol0 = SampleInput(0);\n";
		else
			shader_source += StringFromFormat("\t%s();\n", (pass->entry_point != "main") ? pass->entry_point.c_str() : "main");

		shader_source += "\tout_col0 = ocol0;\n";
		shader_source += "}\n";
	}

	return shader_source;
}

bool  PostProcessor::UpdateConstantUniformBuffer(
	const InputTextureSizeArray& input_sizes,
	const TargetRectangle& dst_rect, const TargetSize& dst_size,
	const TargetRectangle& src_rect, const TargetSize& src_size,
	int src_layer, float gamma)
{
	// Check if the size has changed, due to options.
	Constant temp;
	// float4 input_resolutions[4]
	for (size_t i = 0; i < POST_PROCESSING_MAX_TEXTURE_INPUTS; i++)
	{
		temp.float_constant[0] = (float)input_sizes[i].width;
		temp.float_constant[1] = (float)input_sizes[i].height;
		temp.float_constant[2] = 1.0f / (float)input_sizes[i].width;
		temp.float_constant[3] = 1.0f / (float)input_sizes[i].height;
		m_new_constants[i] = temp;
	}

	// float4 target_resolution
	u32 constant_idx = POST_PROCESSING_MAX_TEXTURE_INPUTS;
	temp.float_constant[0] = (float)dst_size.width;
	temp.float_constant[1] = (float)dst_size.height;
	temp.float_constant[2] = 1.0f / (float)dst_size.width;
	temp.float_constant[3] = 1.0f / (float)dst_size.height;
	m_new_constants[constant_idx] = temp;
	constant_idx++;

	// float4 src_rect
	temp.float_constant[0] = (float)src_rect.left / (float)src_size.width;
	temp.float_constant[1] = (float)((m_APIType == API_OPENGL) ? src_rect.bottom : src_rect.top) / (float)src_size.height;
	temp.float_constant[2] = (float)src_rect.GetWidth() / (float)src_size.width;
	temp.float_constant[3] = (float)src_rect.GetHeight() / (float)src_size.height;
	m_new_constants[constant_idx] = temp;
	constant_idx++;

	// float4 target_rect
	temp.float_constant[0] = (float)dst_rect.left / (float)dst_size.width;
	temp.float_constant[1] = (float)((m_APIType == API_OPENGL) ? dst_rect.bottom : dst_rect.top) / (float)dst_size.height;
	temp.float_constant[2] = (float)dst_rect.GetWidth() / (float)dst_size.width;
	temp.float_constant[3] = (float)dst_rect.GetHeight() / (float)dst_size.height;
	m_new_constants[constant_idx] = temp;
	constant_idx++;

	// float4 viewport_rect
	temp.float_constant[0] = (float)dst_rect.left;
	temp.float_constant[1] = (float)dst_rect.top;
	temp.float_constant[2] = (float)dst_rect.right;
	temp.float_constant[3] = (float)dst_rect.bottom;
	m_new_constants[constant_idx] = temp;
	constant_idx++;

	// float4 window_rect
	const TargetRectangle& window_rect =g_renderer->GetWindowRectangle();
	temp.float_constant[0] = (float)window_rect.left;
	temp.float_constant[1] = (float)window_rect.top;
	temp.float_constant[2] = (float)window_rect.right;
	temp.float_constant[3] = (float)window_rect.bottom;
	m_new_constants[constant_idx] = temp;
	constant_idx++;

	// float time, float layer
	temp.float_constant[0] = float(double(m_timer.GetTimeDifference()) / 1000.0);
	temp.float_constant[1] = float(std::max(src_layer, 0));
	temp.float_constant[2] = gamma;
	temp.int_constant[3] = (src_rect.GetWidth() > dst_rect.GetWidth() && dst_rect.GetHeight() > dst_rect.GetHeight() && g_ActiveConfig.bUseScalingFilter) ? 1u : 0;
	m_new_constants[constant_idx] = temp;
	constant_idx++;

	// Any changes?
	if (!memcmp(m_current_constants.data(), m_new_constants.data(), POST_PROCESSING_CONTANTS_BUFFER_SIZE))
		return false;

	// Swap buffer pointers, to avoid copying data again
	std::swap(m_current_constants, m_new_constants);
	return true;
}

void* PostProcessingShaderConfiguration::GetConfigurationBuffer(u32* buffer_size)
{
	*buffer_size = static_cast<u32>(m_constants.size() * sizeof(Constant));
	return m_constants.data();
}

void* PostProcessingShaderConfiguration::UpdateConfigurationBuffer(u32* buffer_size, bool packbuffer)
{
	size_t active_constant_count = m_options.size();
	if (!m_configuration_buffer_dirty || !active_constant_count)
	{
		return nullptr;
	}
	m_configuration_buffer_dirty = false;
	// Check if the size has changed, due to options.
	m_constants.resize(active_constant_count);
	Constant temp = {};

	size_t constant_idx = 0;
	size_t element_idx = 0;
	// Set from options. This is an ordered map so it will always match the order in the shader code generated.
	for (const auto& it : m_options)
	{
		// Skip compile-time constants, since they're set in the program source.
		if (it.second.m_compile_time_constant)
			continue;

		u32 components = 1;
		if (it.second.m_type == POST_PROCESSING_OPTION_TYPE_INTEGER)
		{
			components = std::max((u32)it.second.m_integer_values.size(), (u32)0);
		}
		else if (it.second.m_type == POST_PROCESSING_OPTION_TYPE_FLOAT)
		{
			components = std::max((u32)it.second.m_float_values.size(), (u32)0);
		}
		if (packbuffer)
		{
			if (element_idx + components > 4)
			{
				m_constants[constant_idx] = temp;
				temp = {};
				element_idx = 0;
				constant_idx++;
			}
		}


		switch (it.second.m_type)
		{
		case POST_PROCESSING_OPTION_TYPE_BOOL:
			temp.int_constant[element_idx] = (int)it.second.m_bool_value;
			break;

		case POST_PROCESSING_OPTION_TYPE_INTEGER:
		{
			for (u32 i = 0; i < components; i++)
				temp.int_constant[element_idx + i] = it.second.m_integer_values[i];
		}
		break;

		case POST_PROCESSING_OPTION_TYPE_FLOAT:
		{
			for (u32 i = 0; i < components; i++)
				temp.float_constant[element_idx + i] = it.second.m_float_values[i];
		}
		break;
		}
		if (packbuffer)
		{
			element_idx += components;
		}
		else
		{
			m_constants[constant_idx] = temp;
			temp = {};
			element_idx = 0;
			constant_idx++;
		}
	}
	if (element_idx > 0)
	{
		m_constants[constant_idx] = temp;
		constant_idx++;
	}
	m_constants.resize(constant_idx);
	*buffer_size = static_cast<u32>(constant_idx * sizeof(Constant));
	return m_constants.data();
}
