// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "Common/Flag.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"

#include "VideoCommon/VideoCommon.h"

enum PostProcessingTrigger : u32
{
	POST_PROCESSING_TRIGGER_ON_SWAP,
	POST_PROCESSING_TRIGGER_ON_PROJECTION,
	POST_PROCESSING_TRIGGER_ON_EFB_COPY
};

enum PostProcessingOptionType : u32
{
	POST_PROCESSING_OPTION_TYPE_BOOL,
	POST_PROCESSING_OPTION_TYPE_FLOAT,
	POST_PROCESSING_OPTION_TYPE_INTEGER
};

enum PostProcessingInputType : u32
{
	POST_PROCESSING_INPUT_TYPE_IMAGE,                   // external image loaded from file
	POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER,            // colorbuffer at internal resolution
	POST_PROCESSING_INPUT_TYPE_DEPTH_BUFFER,            // depthbuffer at internal resolution
	POST_PROCESSING_INPUT_TYPE_PREVIOUS_PASS_OUTPUT,    // output of the previous pass
	POST_PROCESSING_INPUT_TYPE_PASS_OUTPUT              // output of a previous pass
};

enum PostProcessingInputFilter : u32
{
	POST_PROCESSING_INPUT_FILTER_NEAREST,               // nearest/point sampling
	POST_PROCESSING_INPUT_FILTER_LINEAR                 // linear sampling
};

enum PostProcessingAddressMode : u32
{
	POST_PROCESSING_ADDRESS_MODE_CLAMP,                 // clamp to edge
	POST_PROCESSING_ADDRESS_MODE_WRAP,                  // wrap around at edge
	POST_PROCESSING_ADDRESS_MODE_BORDER                 // fixed color (0) at edge
};

// Maximum number of texture inputs to a post-processing shader.
static const size_t POST_PROCESSING_MAX_TEXTURE_INPUTS = 4;

class PostProcessingShaderConfiguration
{
public:

	struct ConfigurationOption final
	{
		bool m_bool_value;
		bool m_default_bool_value;

		std::vector<float> m_default_float_values;
		std::vector<s32> m_default_integer_values;
		std::vector<float> m_float_values;
		std::vector<s32> m_integer_values;

		std::vector<float> m_float_min_values;
		std::vector<s32> m_integer_min_values;

		std::vector<float> m_float_max_values;
		std::vector<s32> m_integer_max_values;

		std::vector<float> m_float_step_values;
		std::vector<s32> m_integer_step_values;

		PostProcessingOptionType  m_type;

		std::string m_gui_name;
		std::string m_gui_description;
		std::string m_option_name;
		std::string m_dependent_option;

		bool m_compile_time_constant;
		bool m_dirty;
	};

	struct RenderPass final
	{
		struct Input final
		{
			PostProcessingInputType type;
			PostProcessingInputFilter filter;
			PostProcessingAddressMode address_mode;
			u32 texture_unit;
			u32 pass_output_index;

			std::unique_ptr<u8[]> external_image_data;
			u32 external_image_width;
			u32 external_image_height;
		};
		std::vector<Input> inputs;
		std::string entry_point;
		float output_scale;
		std::vector<const ConfigurationOption*> dependent_options;

		bool CheckEnabled() const
		{
			if (dependent_options.size() > 0)
			{
				for (const auto& option : dependent_options)
				{
					if (option->m_bool_value)
					{
						return true;
					}
				}
				return false;
			}
			return true;
		}
	};

	typedef std::map<std::string, ConfigurationOption> ConfigMap;
	typedef std::vector<RenderPass> RenderPassList;

	PostProcessingShaderConfiguration() = default;
	virtual ~PostProcessingShaderConfiguration() {}

	// Loads the configuration with a shader
	// If the argument is "" the class will load the shader from the g_activeConfig option.
	// Returns the loaded shader source from file
	bool LoadShader(const std::string& sub_dir, const std::string& shader);
	void SaveOptionsConfiguration();
	const std::string &GetShader() const { return m_current_shader; }
	const std::string &GetShaderSource() const { return m_shader_source; }

	bool IsDirty() const { return m_any_options_dirty; }
	bool IsCompileTimeConstantsDirty() const { return m_compile_time_constants_dirty; }
	void SetDirty(bool dirty = true) { m_any_options_dirty = dirty; }
	void ClearDirty();
	bool RequiresDepthBuffer() const { return m_requires_depth_buffer; }

	bool HasOptions() const { return !m_options.empty(); }
	ConfigMap& GetOptions() { return m_options; }
	const ConfigMap& GetOptions() const { return m_options; }
	const ConfigurationOption& GetOption(const std::string& option) { return m_options[option]; }

	const RenderPassList& GetPasses() const { return m_render_passes; }
	const RenderPass& GetPass(size_t index) const { return m_render_passes.at(index); }

	// For updating option's values
	void SetOptionf(const std::string& option, int index, float value);
	void SetOptioni(const std::string& option, int index, s32 value);
	void SetOptionb(const std::string& option, bool value);

	void PrintCompilationTimeOptions(std::string &options) const;
private:
	bool m_any_options_dirty = false;
	bool m_compile_time_constants_dirty = false;
	bool m_requires_depth_buffer = false;
	std::string m_current_shader;
	std::string m_shader_source;
	ConfigMap m_options;
	RenderPassList m_render_passes;

	bool ParseShader(const std::string& code);
	void LoadOptionsConfigurationFromSection(IniFile::Section* section);
	void LoadOptionsConfiguration();
};

class PostProcessor
{
public:
	PostProcessor();
	virtual ~PostProcessor();

	// Get a list of post-processing shaders.


	// Get the config for a shader in the current chain.
	PostProcessingShaderConfiguration* GetPostShaderConfig(const std::string& shader_name);

	// Get the current blit shader config.
	PostProcessingShaderConfiguration* GetBlitShaderConfig() { return m_blit_config.get(); }

	bool IsActive() const { return m_active; }
	bool RequiresDepthBuffer() const { return m_requires_depth_buffer; }

	void SetReloadFlag() { m_reload_flag.Set(); }
	bool RequiresReload() { return m_reload_flag.TestAndClear(); }

	void OnPerspectiveProjectionLoaded();
	void OnOrthographicProjectionLoaded();
	void OnEFBCopy();
	void OnEndFrame();

	// Should be implemented by the backends for backend specific code
	virtual bool Initialize() = 0;
	virtual void ReloadShaders() = 0;

	// Used when post-processing on perspective->ortho switch.
	virtual void PostProcessEFB() = 0;

	// Copy/resize src_texture to dst_texture (0 means backbuffer), using the resize/blit shader.
	virtual void BlitToFramebuffer(const TargetRectangle& dst, uintptr_t dst_texture,
		const TargetRectangle& src, uintptr_t src_texture, uintptr_t src_depth_texture, int src_width, int src_height, int src_layer, float gamma = 1.0f) = 0;

	// Post-process an image.
	virtual void PostProcess(const TargetRectangle& visible_rect, int target_width, int target_height,
		int target_layers, uintptr_t texture, uintptr_t depth_texture) = 0;

	// Construct the options uniform buffer source for the specified config.
	static void GetUniformBufferShaderSource(API_TYPE api, const PostProcessingShaderConfiguration* config, std::string& shader_source);

	// Construct a complete fragment shader (HLSL/GLSL) for the specified pass.
	static std::string GetPassFragmentShaderSource(API_TYPE api, const PostProcessingShaderConfiguration* config,
		const PostProcessingShaderConfiguration::RenderPass* pass);

	// Scale a target resolution to an output's scale
	static void ScaleTargetSize(int* scaled_width, int* scaled_height, int orig_width, int orig_height, float scale);

	// Scale a target rectangle to an output's scale
	static TargetRectangle ScaleTargetRectangle(API_TYPE api, const TargetRectangle& src, float scale);


protected:
	enum PROJECTION_STATE : u32
	{
		PROJECTION_STATE_INITIAL,
		PROJECTION_STATE_PERSPECTIVE,
		PROJECTION_STATE_FINAL
	};

	// Size of the constant buffer reserved for post-processing.
	// 4KiB = 256 constants, a shader should not have this many options.
	static const size_t UNIFORM_BUFFER_SIZE = 4096;

	// Update constant buffer with the current values from the config.
	void UpdateUniformBuffer(API_TYPE api, const PostProcessingShaderConfiguration* config,
		void* buffer_ptr, int input_resolutions[POST_PROCESSING_MAX_TEXTURE_INPUTS][2],
		const TargetRectangle& src_rect, const TargetRectangle& dst_rect, int src_width, int src_height, int src_layer, float gamma);

	// Load m_configs with the selected post-processing shaders.
	void ReloadShaderConfigs();

	// Timer for determining our time value
	Common::Timer m_timer;

	// Set by UI thread when the shader changes
	Common::Flag m_reload_flag;

	// List of current post-processing shaders, ordered by application order
	std::vector<std::string> m_shader_names;

	// Current post-processing shader config
	std::unordered_map<std::string, std::unique_ptr<PostProcessingShaderConfiguration>> m_shader_configs;

	// Blit/anaglyph shader config
	std::unique_ptr<PostProcessingShaderConfiguration> m_blit_config;

	// Projection state for detecting when to apply post
	PROJECTION_STATE m_projection_state = PROJECTION_STATE_INITIAL;

	// Global post-processing enable state
	bool m_active = false;
	bool m_requires_depth_buffer = false;

	// common shader code between backends
	static const std::string s_post_fragment_header_ogl;
	static const std::string s_post_fragment_header_d3d;
	static const std::string s_post_fragment_header_common;
};