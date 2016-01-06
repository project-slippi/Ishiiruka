// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include <SOIL/SOIL.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VideoConfig.h"

#include <wx/language.h>


static const char s_default_shader[] = "void main() { SetOutput(ApplyGCGamma(Sample())); }\n";
struct LangDescriptor
{
	wxLanguage Lang;
	const char* Code;
};

#define LANGUAGE_ID_COUNT 26

static const LangDescriptor language_ids[LANGUAGE_ID_COUNT] =
{
	{ wxLANGUAGE_DEFAULT, ""},

	{ wxLANGUAGE_CATALAN, ".CAT" },
	{ wxLANGUAGE_CZECH, ".CZE" },
	{ wxLANGUAGE_GERMAN, ".GER" },
	{ wxLANGUAGE_ENGLISH, ".ENG" },
	{ wxLANGUAGE_SPANISH, ".SPA" },
	{ wxLANGUAGE_FRENCH, ".FRE" },
	{ wxLANGUAGE_ITALIAN, ".ITA" },
	{ wxLANGUAGE_HUNGARIAN, ".HUN" },
	{ wxLANGUAGE_DUTCH, ".DUT" },
	{ wxLANGUAGE_NORWEGIAN_BOKMAL, ".NOR" },
	{ wxLANGUAGE_POLISH, ".POL" },
	{ wxLANGUAGE_PORTUGUESE, ".POR" },
	{ wxLANGUAGE_PORTUGUESE_BRAZILIAN, ".BRA" },
	{ wxLANGUAGE_SERBIAN, ".SER" },
	{ wxLANGUAGE_SWEDISH, ".SWE" },
	{ wxLANGUAGE_TURKISH, ".TUR" },
	
	{ wxLANGUAGE_GREEK, ".GRE" },
	{ wxLANGUAGE_RUSSIAN, ".RUS" },
	{ wxLANGUAGE_HEBREW, ".HEB" },
	{ wxLANGUAGE_ARABIC, ".ARA" },
	{ wxLANGUAGE_FARSI, ".FAR" },
	{ wxLANGUAGE_KOREAN, ".KOR" },
	{ wxLANGUAGE_JAPANESE, ".JAP" },
	{ wxLANGUAGE_CHINESE_SIMPLIFIED, ".CHS" },
	{ wxLANGUAGE_CHINESE_TRADITIONAL, ".CHT" }
};

inline bool StartsWith(const std::string& str, const std::string& prefix)
{
	return str.compare(0, prefix.size(), prefix) == 0;
}

inline bool EndsWith(const std::string& str, const std::string& ending) {
	if (str.length() >= ending.length()) {
		return (0 == str.compare(str.length() - ending.length(), ending.length(), ending));
	}
	else {
		return false;
	}
}

PostProcessor::PostProcessor()
{
	m_timer.Start();
}

PostProcessor::~PostProcessor()
{
	m_timer.Stop();
}

void PostProcessor::OnPerspectiveProjectionLoaded()
{
	if (!m_active || !g_ActiveConfig.bPostProcessingEnable ||
		(g_ActiveConfig.iPostProcessingTrigger != POST_PROCESSING_TRIGGER_ON_PROJECTION &&
			(g_ActiveConfig.iPostProcessingTrigger != POST_PROCESSING_TRIGGER_ON_EFB_COPY)))
	{
		return;
	}

	// Only adjust the flag if this is our first perspective load.
	if (m_projection_state == PROJECTION_STATE_INITIAL)
		m_projection_state = PROJECTION_STATE_PERSPECTIVE;
}

void PostProcessor::OnOrthographicProjectionLoaded()
{
	if (!m_active || !g_ActiveConfig.bPostProcessingEnable ||
		g_ActiveConfig.iPostProcessingTrigger != POST_PROCESSING_TRIGGER_ON_PROJECTION)
	{
		return;
	}

	// Fire off postprocessing on the current efb if a perspective scene has been drawn.
	if (m_projection_state == PROJECTION_STATE_PERSPECTIVE)
	{
		DEBUG_LOG(VIDEO, "Triggered post-process on perspective->orthographic");
		m_projection_state = PROJECTION_STATE_FINAL;
		PostProcessEFB();
	}
}

void PostProcessor::OnEFBCopy()
{
	if (!m_active || !g_ActiveConfig.bPostProcessingEnable ||
		g_ActiveConfig.iPostProcessingTrigger != POST_PROCESSING_TRIGGER_ON_EFB_COPY)
	{
		return;
	}

	// Fire off postprocessing on the current efb if a perspective scene has been drawn.
	if (m_projection_state == PROJECTION_STATE_PERSPECTIVE)
	{
		m_projection_state = PROJECTION_STATE_FINAL;
		PostProcessEFB();
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
		PostProcessEFB();

	m_projection_state = PROJECTION_STATE_INITIAL;
}

bool  PostProcessingShaderConfiguration::LoadShader(const std::string& sub_dir, const std::string& shader)
{
	// loading shader code
	std::string code;
	std::string path = File::GetUserPath(D_SHADERS_IDX) + sub_dir + DIR_SEP + shader + ".glsl";

	if (shader.length() > 0)
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
	else
	{
		// Use pass-through (default) shader
		code = s_default_shader;
	}
	
	m_current_shader = shader;

	DEBUG_LOG(VIDEO, "Postprocessing: Parsing shader at %s", path.c_str());
	if (!ParseShader(code))
	{
		ERROR_LOG(VIDEO, "Failed to load options from post-processing shader: %s", path.c_str());
		return false;
	}

	LoadOptionsConfiguration();

	return true;
}

bool PostProcessingShaderConfiguration::ParseShader(const std::string& code)
{
	const std::string config_start_delimiter = "[configuration]";
	const std::string config_end_delimiter = "[/configuration]";
	size_t configuration_start = code.find(config_start_delimiter);
	size_t configuration_end = code.find(config_end_delimiter);
	m_render_passes.clear();
	m_options.clear();
	m_shader_source.clear();
	m_any_options_dirty = true;
	m_requires_depth_buffer = code.find("SampleDepth") != std::string::npos;

	if (configuration_start != std::string::npos && configuration_end != std::string::npos)
	{

		// Remove the configuration area from the source string, leaving only the GLSL code.
		m_shader_source = code;
		m_shader_source.erase(configuration_start, (configuration_end - configuration_start + config_end_delimiter.length()));

		// Extract configuration string, and parse options/passes
		std::string configuration_string = code.substr(configuration_start + config_start_delimiter.size(),
			configuration_end - configuration_start - config_start_delimiter.size());

		std::istringstream in(configuration_string);

		struct GLSLStringOption
		{
			std::string m_type;
			std::vector<std::pair<std::string, std::string>> m_options;
		};

		std::vector<GLSLStringOption> option_strings;
		std::vector<GLSLStringOption> pass_strings;
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
							if (sub == "Pass")
							{
								pass_strings.push_back({ sub });
								current_strings = &pass_strings.back();
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
		const char* LangCode = nullptr;
		for (size_t i = 1; i < LANGUAGE_ID_COUNT; i++)
		{
			if (language_ids[i].Lang == SConfig::GetInstance().m_InterfaceLanguage)
			{
				LangCode = language_ids[i].Code;
				break;
			}
		}

		for (const auto& it : option_strings)
		{
			ConfigurationOption option;
			option.m_dirty = true;
			option.m_type = POST_PROCESSING_OPTION_TYPE_FLOAT;
			option.m_compile_time_constant = false;

			if (it.m_type == "OptionBool")
			{
				option.m_type = POST_PROCESSING_OPTION_TYPE_BOOL;
			}
			else if (it.m_type == "OptionRangeFloat")
			{
				option.m_type = POST_PROCESSING_OPTION_TYPE_FLOAT;
			}
			else if (it.m_type == "OptionRangeInteger")
			{
				option.m_type = POST_PROCESSING_OPTION_TYPE_INTEGER;
			}
			else
			{
				WARN_LOG(VIDEO, "Unknown section name in post-processing shader config: '%s'", it.m_type.c_str());
				continue;
			}

			for (const auto& string_option : it.m_options)
			{
				const std::string& key = string_option.first;
				const std::string& value = string_option.second;
				if (StartsWith(key, "GUIName"))
				{
					if (key == "GUIName")
					{
						option.m_gui_name = value;
					}
					else if (LangCode != nullptr && EndsWith(key, LangCode))
					{
						option.m_gui_name = value;
					}
				}
				else if (StartsWith(key, "GUIDescription"))
				{
					if (key == "GUIDescription")
					{
						option.m_gui_description = value;
					}
					else if (LangCode != nullptr && EndsWith(key, LangCode))
					{
						option.m_gui_description = value;
					}
				}
				else if (key == "OptionName")
				{
					option.m_option_name = value;
				}
				else if (key == "DependentOption")
				{
					option.m_dependent_option = value;
				}
				else if (key == "ResolveAtCompilation")
				{
					TryParse(value, &option.m_compile_time_constant);
				}
				else if (key == "MinValue" ||
					key == "MaxValue" ||
					key == "DefaultValue" ||
					key == "StepAmount")
				{
					std::vector<s32>* output_integer = nullptr;
					std::vector<float>* output_float = nullptr;

					if (key == "MinValue")
					{
						output_integer = &option.m_integer_min_values;
						output_float = &option.m_float_min_values;
					}
					else if (key == "MaxValue")
					{
						output_integer = &option.m_integer_max_values;
						output_float = &option.m_float_max_values;
					}
					else if (key == "DefaultValue")
					{
						output_integer = &option.m_integer_values;
						output_float = &option.m_float_values;
					}
					else if (key == "StepAmount")
					{
						output_integer = &option.m_integer_step_values;
						output_float = &option.m_float_step_values;
					}

					if (option.m_type == POST_PROCESSING_OPTION_TYPE_BOOL)
					{
						TryParse(value, &option.m_bool_value);
					}
					else if (option.m_type == POST_PROCESSING_OPTION_TYPE_INTEGER)
					{
						TryParseVector(value, output_integer);
						if (output_integer->size() > 4)
							output_integer->erase(output_integer->begin() + 4, output_integer->end());
					}
					else if (option.m_type == POST_PROCESSING_OPTION_TYPE_FLOAT)
					{
						TryParseVector(value, output_float);
						if (output_float->size() > 4)
							output_float->erase(output_float->begin() + 4, output_float->end());
					}
				}
			}
			if (option.m_type == POST_PROCESSING_OPTION_TYPE_INTEGER)
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
			else if (option.m_type == POST_PROCESSING_OPTION_TYPE_FLOAT)
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
		for (const auto& it : pass_strings)
		{
			RenderPass pass;
			pass.output_scale = 1.0f;
			pass.dependent_options.resize(0);
			for (const auto& string_option : it.m_options)
			{
				const std::string& key = string_option.first;
				const std::string& value = string_option.second;
				if (key == "EntryPoint")
				{
					pass.entry_point = value;
				}
				else if (key == "OutputScale")
				{
					TryParse(value, &pass.output_scale);
				}
				else if (key == "DependentOption")
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
						new_input.external_image_width = 0;
						new_input.external_image_height = 0;
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
						else
						{
							ERROR_LOG(VIDEO, "Post processing configuration error: Invalid input mode: %s", value.c_str());
							return false;
						}
					}
					else if (extra == "Source")
					{
						// Load external image
						std::string path = File::GetUserPath(D_SHADERS_IDX) + value;
						if (!File::Exists(path))
						{
							path = File::GetSysDirectory() + SHADERS_DIR DIR_SEP + value;
							if (!File::Exists(path))
							{
								ERROR_LOG(VIDEO, "Post processing configuration error: Unable to load external image at '%s'", value.c_str());
								return false;
							}
						}

						File::IOFile file;
						file.Open(path, "rb");
						std::vector<u8> buffer(file.GetSize());
						if (!file.IsOpen() || !file.ReadBytes(buffer.data(), file.GetSize()))
						{
							ERROR_LOG(VIDEO, "Post processing configuration error: Unable to load external image at '%s'", value.c_str());
							return false;
						}

						int image_width;
						int image_height;
						int image_channels;
						u8* decoded = SOIL_load_image_from_memory(buffer.data(), (int)buffer.size(), &image_width, &image_height, &image_channels, SOIL_LOAD_RGBA);
						if (decoded == nullptr)
						{
							ERROR_LOG(VIDEO, "Post processing configuration error: Failed to parse external image at '%s'", value.c_str());
							return false;
						}

						// Reallocate the memory so we can manage it
						input->type = POST_PROCESSING_INPUT_TYPE_IMAGE;
						input->external_image_width = (u32)image_width;
						input->external_image_height = (u32)image_height;
						input->external_image_data = std::make_unique<u8[]>(image_width * image_height * 4);
						memcpy(input->external_image_data.get(), decoded, image_width * image_height * 4);
						SOIL_free_image_data(decoded);
					}
					else
					{
						ERROR_LOG(VIDEO, "Post processing configuration error: Unknown input key: %s", key.c_str());
						return false;
					}
				}
			}
			m_render_passes.push_back(std::move(pass));
		}
	}
	else
	{
		// If there is no configuration block. Assume the entire file is code.
		m_shader_source = code;
	}
	// If no render passes are specified, create a default pass.
	if (m_render_passes.empty())
	{
		RenderPass::Input input;
		input.type = POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER;
		input.filter = POST_PROCESSING_INPUT_FILTER_LINEAR;
		input.address_mode = POST_PROCESSING_ADDRESS_MODE_CLAMP;
		input.texture_unit = 0;
		input.pass_output_index = 0;
		input.external_image_width = 0;
		input.external_image_height = 0;

		RenderPass pass;
		pass.entry_point = "main";
		pass.inputs.push_back(std::move(input));
		pass.output_scale = 1;
		m_render_passes.push_back(std::move(pass));

		if (m_requires_depth_buffer)
		{
			RenderPass::Input dinput;
			dinput.type = POST_PROCESSING_INPUT_TYPE_DEPTH_BUFFER;
			dinput.filter = POST_PROCESSING_INPUT_FILTER_NEAREST;
			dinput.address_mode = POST_PROCESSING_ADDRESS_MODE_CLAMP;
			dinput.texture_unit = 0;
			dinput.pass_output_index = 0;
			dinput.external_image_width = 0;
			dinput.external_image_height = 0;
			pass.inputs.push_back(std::move(dinput));
		}
	}

	return true;
}

void PostProcessingShaderConfiguration::PrintCompilationTimeOptions(std::string &options) const
{
	for (auto& it : m_options)
	{
		auto& option = it.second;
		if (!option.m_compile_time_constant)
		{
			continue;
		}
		if (option.m_type == POST_PROCESSING_OPTION_TYPE_BOOL)
		{
			options += StringFromFormat("#define %s %d\n", it.first.c_str(), option.m_bool_value ? 1 : 0);
		}
		else if (option.m_type == POST_PROCESSING_OPTION_TYPE_INTEGER)
		{
			u32 count = static_cast<u32>(option.m_integer_values.size());
			switch (count)
			{
			case 1:
				options += StringFromFormat("#define %s %d\n",
					it.first.c_str(),
					option.m_integer_values[0]);
				break;
			case 2:
				options += StringFromFormat("#define %s int2(%d,%d)\n",
					it.first.c_str(),
					option.m_integer_values[0],
					option.m_integer_values[1]);
				break;
			case 3:
				options += StringFromFormat("#define %s int3(%d,%d,%d)\n",
					it.first.c_str(),
					option.m_integer_values[0],
					option.m_integer_values[1],
					option.m_integer_values[2]);
				break;
			case 4:
				options += StringFromFormat("#define %s int4(%d,%d,%d, %d)\n",
					it.first.c_str(),
					option.m_integer_values[0],
					option.m_integer_values[1],
					option.m_integer_values[2],
					option.m_integer_values[3]);
				break;
			default:
				break;
			}
		}
		else if (option.m_type == POST_PROCESSING_OPTION_TYPE_FLOAT)
		{
			u32 count = static_cast<u32>(option.m_float_values.size());
			switch (count)
			{
			case 1:
				options += StringFromFormat("#define %s %f\n",
					it.first.c_str(),
					option.m_float_values[0]);
				break;
			case 2:
				options += StringFromFormat("#define %s float2(%f,%f)\n",
					it.first.c_str(),
					option.m_float_values[0],
					option.m_float_values[1]);
				break;
			case 3:
				options += StringFromFormat("#define %s float3(%f,%f,%f)\n",
					it.first.c_str(),
					option.m_float_values[0],
					option.m_float_values[1],
					option.m_float_values[2]);
				break;
			case 4:
				options += StringFromFormat("#define %s float4(%f,%f,%f,%f)\n",
					it.first.c_str(),
					option.m_float_values[0],
					option.m_float_values[1],
					option.m_float_values[2],
					option.m_float_values[3]);
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
				s32 value = current.m_integer_values[i];
				value = std::max(value, current.m_integer_min_values[i]);
				value = std::min(value, current.m_integer_max_values[i]);
				current.m_integer_values[i] = value;
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
				float value = current.m_float_values[i];
				value = std::max(value, current.m_float_min_values[i]);
				value = std::min(value, current.m_float_max_values[i]);
				current.m_float_values[i] = value;
			}
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
		if (Core::IsRunning())
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
	if (Core::IsRunning())
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

void PostProcessingShaderConfiguration::ClearDirty()
{
	m_any_options_dirty = false;
	m_compile_time_constants_dirty = false;
	for (auto& it : m_options)
		it.second.m_dirty = false;
}

void PostProcessingShaderConfiguration::SetOptionf(const std::string& option, int index, float value)
{
	auto it = m_options.find(option);

	it->second.m_float_values[index] = value;
	it->second.m_dirty = true;
	m_any_options_dirty = true;

	if (it->second.m_compile_time_constant)
		m_compile_time_constants_dirty = true;
}

void PostProcessingShaderConfiguration::SetOptioni(const std::string& option, int index, s32 value)
{
	auto it = m_options.find(option);

	it->second.m_integer_values[index] = value;
	it->second.m_dirty = true;
	m_any_options_dirty = true;
	
	if (it->second.m_compile_time_constant)
		m_compile_time_constants_dirty = true;
}

void PostProcessingShaderConfiguration::SetOptionb(const std::string& option, bool value)
{
	auto it = m_options.find(option);

	it->second.m_bool_value = value;	
	it->second.m_dirty = true;
	m_any_options_dirty = true;

	if (it->second.m_compile_time_constant)
		m_compile_time_constants_dirty = true;
}

void PostProcessor::UpdateUniformBuffer(API_TYPE api, const PostProcessingShaderConfiguration* config,
	void* buffer_ptr, int input_resolutions[POST_PROCESSING_MAX_TEXTURE_INPUTS][2],
	const TargetRectangle& src_rect, const TargetRectangle& dst_rect, int src_width, int src_height, int src_layer, float gamma)
{
	// Each option is aligned to a float4
	union Constant
	{
		int bool_constant;
		float float_constant[4];
		s32 int_constant[4];
	};

	// First two constants are always resolution, time
	Constant* constants = reinterpret_cast<Constant*>(buffer_ptr);

	// resolutions are at slot 0-3. rect is at slot 4. time at slot 5.
	for (size_t i = 0; i < POST_PROCESSING_MAX_TEXTURE_INPUTS; i++)
	{
		constants[i].float_constant[0] = (float)input_resolutions[i][0];
		constants[i].float_constant[1] = (float)input_resolutions[i][1];
		constants[i].float_constant[2] = 1.0f / (float)input_resolutions[i][0];
		constants[i].float_constant[3] = 1.0f / (float)input_resolutions[i][1];
	}

	constants[4].float_constant[0] = (float)src_rect.left / (float)src_width;	
	constants[4].float_constant[1] = (float)((api == API_OPENGL) ? src_rect.bottom : src_rect.top) / (float)src_height;
	constants[4].float_constant[2] = (float)src_rect.GetWidth() / (float)src_width;
	constants[4].float_constant[3] = (float)src_rect.GetHeight() / (float)src_height;

	constants[5].float_constant[0] = float(dst_rect.GetWidth());
	constants[5].float_constant[1] = float(dst_rect.GetHeight());
	constants[5].float_constant[2] = 1.0f / constants[2].float_constant[0];
	constants[5].float_constant[3] = 1.0f / constants[2].float_constant[1];
	
	constants[6].float_constant[0] = float(double(m_timer.GetTimeDifference()) / 1000.0);
	constants[6].float_constant[1] = float(std::max(src_layer, 0));
	constants[6].float_constant[2] = gamma;
	constants[6].int_constant[3] = (src_rect.GetWidth() > dst_rect.GetWidth() && dst_rect.GetHeight() > dst_rect.GetHeight() && g_ActiveConfig.bUseScalingFilter) ? 1u : 0;

	// Set from options. This is an ordered map so it will always match the order in the shader code generated.
	u32 current_slot = 7;
	for (const auto& it : m_config.GetOptions())
	{
		if (it.second.m_compile_time_constant)
		{
			continue;
		}
		switch (it.second.m_type)
		{
		case POST_PROCESSING_OPTION_TYPE_BOOL:
			constants[current_slot].bool_constant = (int)it.second.m_bool_value;
			constants[current_slot].int_constant[1] = 0;
			constants[current_slot].int_constant[2] = 0;
			constants[current_slot].int_constant[3] = 0;
			current_slot++;
			break;

		case POST_PROCESSING_OPTION_TYPE_INTEGER:
		{
			u32 components = std::max((u32)it.second.m_integer_values.size(), (u32)0);
			for (u32 i = 0; i < components; i++)
				constants[current_slot].int_constant[i] = it.second.m_integer_values[i];
			for (u32 i = components; i < 4; i++)
				constants[current_slot].int_constant[i] = 0;

			current_slot++;
		}
		break;

		case POST_PROCESSING_OPTION_TYPE_FLOAT:
		{
			u32 components = std::max((u32)it.second.m_float_values.size(), (u32)0);
			for (u32 i = 0; i < components; i++)
				constants[current_slot].float_constant[i] = it.second.m_float_values[i];
			for (u32 i = components; i < 4; i++)
				constants[current_slot].int_constant[i] = 0;

			current_slot++;
		}
		break;
		}
	}

	// Sanity check, should never fail
	_dbg_assert_(VIDEO, current_slot <= (UNIFORM_BUFFER_SIZE / 16));
}


std::string PostProcessor::GetUniformBufferShaderSource(API_TYPE api, const PostProcessingShaderConfiguration* config)
{
	std::string shader_source;
	
	// Add options resolved at compilation Time
	config->PrintCompilationTimeOptions(shader_source);

	// Constant block
	if (api == API_OPENGL)
		shader_source += "layout(std140) uniform PostProcessingConstants {\n";
	else if (api == API_D3D11)
		shader_source += "cbuffer PostProcessingConstants : register(b0) {\n";

	// Common constants
	shader_source += "\tfloat4 input_resolutions[4];\n";
	shader_source += "\tfloat4 src_rect;\n";
	shader_source += "\tfloat4 dst_scale;\n";
	shader_source += "\tfloat time;\n";
	shader_source += "\tfloat src_layer;\n";
	shader_source += "\tfloat native_gamma;\n";
	shader_source += "\tuint scaling_filter;\n";	

																// User options
	u32 unused_counter = 2;
	for (const auto& it : config->GetOptions())
	{
		if (it.second.m_compile_time_constant)
		{
			continue;
		}
		if (it.second.m_type == POST_PROCESSING_OPTION_TYPE_BOOL)
		{
			shader_source += StringFromFormat("\tbool o_%s;\n", it.first.c_str());
			for (u32 i = 1; i < 4; i++)
				shader_source += StringFromFormat("\tbool unused%u_;\n", unused_counter++);
		}
		else if (it.second.m_type == POST_PROCESSING_OPTION_TYPE_INTEGER)
		{
			u32 count = static_cast<u32>(it.second.m_integer_values.size());
			if (count == 1)
				shader_source += StringFromFormat("\tint o_%s;\n", it.first.c_str());
			else
				shader_source += StringFromFormat("\tint%d o_%s;\n", count, it.first.c_str());

			for (u32 i = count; i < 4; i++)
				shader_source += StringFromFormat("\tint unused%u_;\n", unused_counter++);
		}
		else if (it.second.m_type == POST_PROCESSING_OPTION_TYPE_FLOAT)
		{
			u32 count = static_cast<u32>(it.second.m_float_values.size());
			if (count == 1)
				shader_source += StringFromFormat("\tfloat o_%s;\n", it.first.c_str());
			else
				shader_source += StringFromFormat("\tfloat%d o_%s;\n", count, it.first.c_str());

			for (u32 i = count; i < 4; i++)
				shader_source += StringFromFormat("\tint unused%u_;\n", unused_counter++);
		}
	}

	// End constant block
	shader_source += "};\n";	
	return shader_source;
}

std::string PostProcessor::GetPassFragmentShaderSource(API_TYPE api, const PostProcessingShaderConfiguration* config,
	const PostProcessingShaderConfiguration::RenderPass* pass)
{
	std::string shader_source;
	if (api == API_OPENGL)
	{
		shader_source += "#define API_OPENGL 1\n";
		shader_source += s_post_fragment_header_ogl;
	}
	else if (api == API_D3D11)
	{
		shader_source += "#define API_D3D 1\n";
		shader_source += s_post_fragment_header_d3d;
	}

	// Add uniform buffer
	shader_source += GetUniformBufferShaderSource(api, config);

	// Figure out which input indices map to color/depth/previous buffers.
	// If any of these buffers is not bound, defaults of zero are fine here,
	// since that buffer may not even be used by the shdaer.
	int color_buffer_index = 0;
	int depth_buffer_index = 0;
	int prev_output_index = 0;
	for (const PostProcessingShaderConfiguration::RenderPass::Input& input : pass->inputs)
	{
		switch (input.type)
		{
		case POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER:
			color_buffer_index = input.texture_unit;
			break;

		case POST_PROCESSING_INPUT_TYPE_DEPTH_BUFFER:
			depth_buffer_index = input.texture_unit;
			break;

		case POST_PROCESSING_INPUT_TYPE_PREVIOUS_PASS_OUTPUT:
			prev_output_index = input.texture_unit;
			break;
		}
	}

	// Hook the discovered indices up to macros.
	// This is to support the SampleDepth, SamplePrev, etc. macros.
	shader_source += StringFromFormat("#define COLOR_BUFFER_INPUT_INDEX %d\n", color_buffer_index);
	shader_source += StringFromFormat("#define DEPTH_BUFFER_INPUT_INDEX %d\n", depth_buffer_index);
	shader_source += StringFromFormat("#define PREV_OUTPUT_INPUT_INDEX %d\n", prev_output_index);

	// Remaining wrapper/interfacing functions
	shader_source += s_post_fragment_header_common;

	// Bit of a hack, but we need to change the name of main temporarily.
	// This can go once the compiler is modified to support different entry points.
	if (api == API_D3D11 && pass->entry_point == "main")
		shader_source += "#define main real_main\n";

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
		if (pass->entry_point == "main")
			shader_source += "#undef main\n";

		shader_source += "void main(\n";
		shader_source += "out float4 out_col0 : SV_Target,\n";
		shader_source += "in float4 in_pos : SV_Position,\n";
		shader_source += "in float2 _uv0 : TEXCOORD0,\n";
		shader_source += "in float4 _uv1 : TEXCOORD1,\n";
		shader_source += "in float4 _uv2 : TEXCOORD2,\n";
		shader_source += "in float _layer : TEXCOORD3)\n";
		shader_source += "{\n";
		shader_source += "\tfragcoord = in_pos.xy;\n";
		shader_source += "\tuv0 = _uv0;\n";
		shader_source += "\tuv1 = _uv1;\n";
		shader_source += "\tuv2 = _uv2;\n";
		shader_source += "\tlayer = _layer;\n";

		// No entry point? This pass should perform a copy.
		if (pass->entry_point.empty())
			shader_source += "\tocol0 = SampleInput(0);\n";
		else
			shader_source += StringFromFormat("\t%s();\n", (pass->entry_point != "main") ? pass->entry_point.c_str() : "real_main");

		shader_source += "\tout_col0 = ocol0;\n";
		shader_source += "}\n";
	}

	return shader_source;
}

void PostProcessor::ScaleTargetSize(int* scaled_width, int* scaled_height, int orig_width, int orig_height, float scale)
{
	*scaled_width = std::max(static_cast<int>(std::round((float)orig_width * scale)), 1);
	*scaled_height = std::max(static_cast<int>(std::round((float)orig_height * scale)), 1);
}

TargetRectangle PostProcessor::ScaleTargetRectangle(API_TYPE api, const TargetRectangle& src, float scale)
{
	TargetRectangle dst;
	dst.left = static_cast<int>(std::round((float)src.left * scale));
	dst.right = static_cast<int>(std::round((float)src.right * scale));
	dst.top = static_cast<int>(std::round((float)src.top * scale));
	dst.bottom = static_cast<int>(std::round((float)src.bottom * scale));

	// D3D can't handle zero viewports, so protect against this here
	if (api == API_D3D11)
	{
		dst.right = std::max(dst.right, dst.left + 1);
		dst.bottom = std::max(dst.bottom, dst.top + 1);
	}

	return dst;
}

const std::string PostProcessor::s_post_fragment_header_ogl = R"(
// Depth value is not inverted for GL
#define DEPTH_VALUE(val) (val)
// Shader inputs/outputs
SAMPLER_BINDING(9) uniform sampler2DArray pp_inputs[4];
in float2 uv0;
in float4 uv1;
in float4 uv2;
flat in float layer;
out float4 ocol0;
// Input sampling wrappers
float4 SampleInput(int index) { return texture(pp_inputs[index], float3(uv0, layer)); }
float4 SampleInputLocation(int index, float2 location) { return texture(pp_inputs[index], float3(location, layer)); }
float4 SampleInputLayer(int index, int slayer) { return texture(pp_inputs[index], float3(uv0, float(slayer))); }
float4 SampleInputLayerLocation(int index, int slayer, float2 location) { return texture(pp_inputs[index], float3(location, float(slayer))); }
// Input sampling with offset, macro because offset must be a constant expression.
#define SampleInputOffset(index, offset) (textureOffset(pp_inputs[index], float3(uv0, layer), offset))
#define SampleInputLayerOffset(index, slayer, offset) (textureOffset(pp_inputs[index], float3(uv0, float(slayer)), offset))
float2 GetFragmentCoord()
{
	return gl_FragCoord.xy;
}
)";

const std::string PostProcessor::s_post_fragment_header_d3d = R"(
// Depth value is inverted for D3D
#define DEPTH_VALUE(val) (1.0f - (val))
// Shader inputs
Texture2DArray pp_inputs[4] : register(t9);
SamplerState pp_input_samplers[4] : register(s9);
// Shadows of those read/written in main
static float layer;
static float2 uv0, fragcoord;
static float4 uv1, uv2, ocol0;
// Input sampling wrappers
float4 SampleInput(int index) { return pp_inputs[index].Sample(pp_input_samplers[index], float3(uv0, layer)); }
float4 SampleInputLocation(int index, float2 location) { return pp_inputs[index].Sample(pp_input_samplers[index], float3(location, layer)); }
float4 SampleInputLayer(int index, int slayer) { return pp_inputs[index].Sample(pp_input_samplers[index], float3(uv0, float(slayer))); }
float4 SampleInputLayerLocation(int index, int slayer, float2 location) { return pp_inputs[index].Sample(pp_input_samplers[index], float3(location, float(slayer))); }
// Input sampling with offset, macro because offset must be a constant expression.
#define SampleInputOffset(index, offset) (pp_inputs[index].Sample(pp_input_samplers[index], float3(uv0, layer), offset))
#define SampleInputLayerOffset(index, slayer, offset) (pp_inputs[index].Sample(pp_input_samplers[index], float3(uv0, float(slayer)), offset))
float2 GetFragmentCoord()
{
	return fragcoord;
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
float2 GetInputResolution(int index) { return input_resolutions[index].xy; }
float2 GetInputInvResolution(int index) { return input_resolutions[index].zw; }
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
#define SampleLayerOffset(offset, slayer) (SampleInputLayerOffset(COLOR_BUFFER_INPUT_INDEX, slayer, offset))
#define SamplePrevOffset(offset) (SampleInputOffset(PREV_OUTPUT_INPUT_INDEX, offset))
#define SampleRawDepthOffset(offset) (DEPTH_VALUE(SampleInputOffset(DEPTH_BUFFER_INPUT_INDEX, offset).x))
#define SampleDepthOffset(offset) (ToLinearDepth(SampleRawDepthOffset(offset)))
// Backwards compatibility
float2 GetResolution() { return GetInputResolution(COLOR_BUFFER_INPUT_INDEX); }
float2 GetInvResolution() { return GetInputInvResolution(COLOR_BUFFER_INPUT_INDEX); }
// Variable wrappers
float2 GetCoordinates() { return uv0; }
float GetTime() { return time; }
void SetOutput(float4 color) { ocol0 = color; }
float4 ApplyGCGamma(float4 col) { return pow(col, native_gamma); }
//Random
float global_rnd_state;
float RandomSeedfloat(float2 seed)
{
	float noise = frac(sin(dot(seed, float2(12.9898, 78.233)*2.0)) * 43758.5453);
	return noise;
}

void rnd_advance()
{
    global_rnd_state = RandomSeedfloat(uv0 + global_rnd_state);
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
float4 SampleInputBicubic(int idx, float2 location)
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
	return SampleInputBicubic(idx, uv0);
}

float4 SampleBicubicLocation(float2 location)
{
	return SampleInputBicubic(COLOR_BUFFER_INPUT_INDEX, location);
}

float4 SamplePrevBicubic()
{
	return SampleInputBicubic(PREV_OUTPUT_INPUT_INDEX, uv0);
}

float4 SamplePrevBicubicLocation(float2 location)
{
	return SampleInputBicubic(PREV_OUTPUT_INPUT_INDEX, location);
}

float4 SampleBicubic() 
{ 
	float4 outputcolor = SampleInputBicubic(COLOR_BUFFER_INPUT_INDEX, uv0);
	if (scaling_filter != 0)
	{
		outputcolor += SampleBicubicLocation(uv1.xy);
		outputcolor += SampleBicubicLocation(uv1.wz);
		outputcolor += SampleBicubicLocation(uv2.xy);
		outputcolor += SampleBicubicLocation(uv2.wz);
		outputcolor *= 0.2;
	}
	return outputcolor;
}

#define SetOutput(color) ocol0 = color
// Option check macro
#define GetOption(x) (o_##x)
#define OptionEnabled(x) (o_##x)
)";