// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
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
#include "VideoCommon/TextureCacheBase.h"

enum PostProcessingTrigger : u32
{
	POST_PROCESSING_TRIGGER_ON_SWAP,
	POST_PROCESSING_TRIGGER_ON_PROJECTION,
	POST_PROCESSING_TRIGGER_ON_EFB_COPY,
	POST_PROCESSING_TRIGGER_AFTER_BLIT
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
	POST_PROCESSING_INPUT_TYPE_PASS_OUTPUT,             // output of a previous pass
	POST_PROCESSING_INPUT_TYPE_PASS_FRAME_OUTPUT,       // output of a previous pass
	POST_PROCESSING_INPUT_TYPE_PASS_DEPTH_FRAME_OUTPUT  // output of a previous pass
};

enum PostProcessingInputFilter : u32
{
	POST_PROCESSING_INPUT_FILTER_NEAREST,               // nearest/point sampling
	POST_PROCESSING_INPUT_FILTER_LINEAR,                 // linear sampling
	POST_PROCESSING_INPUT_FILTER_COUNT
};

enum PostProcessingAddressMode : u32
{
	POST_PROCESSING_ADDRESS_MODE_CLAMP,                 // clamp to edge
	POST_PROCESSING_ADDRESS_MODE_WRAP,                  // wrap around at edge
	POST_PROCESSING_ADDRESS_MODE_BORDER,                // fixed color (0) at edge
	POST_PROCESSING_ADDRESS_MODE_MIRROR,                // Mirror the texture
	POST_PROCESSING_ADDRESS_MODE_COUNT
};
// Each option is aligned to a float4
union Constant
{
	float float_constant[4];
	s32 int_constant[4];
};

// Maximum number of texture inputs to a post-processing shader.
static const size_t POST_PROCESSING_MAX_TEXTURE_INPUTS = 8;
static const size_t POST_PROCESSING_CONTANTS = POST_PROCESSING_MAX_TEXTURE_INPUTS + 6;
static const size_t POST_PROCESSING_CONTANTS_BUFFER_SIZE = POST_PROCESSING_CONTANTS * sizeof(Constant);

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
			TargetSize external_image_size;
		};
		std::vector<Input> inputs;
		std::string entry_point;
		float output_scale;
		PC_TexFormat output_format;
		std::vector<const ConfigurationOption*> dependent_options;

		void GetInputLocations(
			int& color_buffer_index,
			int& depth_buffer_index,
			int& prev_output_index
		) const
		{
			color_buffer_index = 0;
			depth_buffer_index = 0;
			prev_output_index = 0;
			for (const Input& input : inputs)
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
				default:
					break;
				}
			}
		}

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

	struct FrameOutput final
	{
		float color_output_scale;
		float depth_scale;
		int color_count;
		int depth_count;
	};

	using ConfigMap = std::map<std::string, ConfigurationOption>;
	using RenderPassList = std::vector<RenderPass>;

	PostProcessingShaderConfiguration() = default;
	virtual ~PostProcessingShaderConfiguration()
	{}

	// Loads the configuration with a shader
	// If the argument is "" the class will load the shader from the g_activeConfig option.
	// Returns the loaded shader source from file
	bool LoadShader(const std::string& sub_dir, const std::string& name);
	void SaveOptionsConfiguration();
	const std::string &GetShaderName() const
	{
		return m_shader_name;
	}
	const std::string &GetShaderSource() const
	{
		return m_shader_source;
	}

	bool IsDirty() const
	{
		return m_any_options_dirty;
	}
	bool IsCompileTimeConstantsDirty() const
	{
		return m_compile_time_constants_dirty;
	}
	void SetDirty(bool dirty = true)
	{
		m_any_options_dirty = dirty;
	}
	void ClearDirty()
	{
		if (m_any_options_dirty || m_compile_time_constants_dirty)
		{
			m_configuration_buffer_dirty = m_any_options_dirty;
			m_any_options_dirty = false;
			m_compile_time_constants_dirty = false;
			for (auto& it : m_options)
				it.second.m_dirty = false;
		}
	}
	bool RequiresDepthBuffer() const
	{
		return m_requires_depth_buffer;
	}

	bool HasOptions() const
	{
		return !m_options.empty();
	}
	ConfigMap& GetOptions()
	{
		return m_options;
	}
	const ConfigMap& GetOptions() const
	{
		return m_options;
	}

	const FrameOutput& GetFrameOutput() const
	{
		return m_frame_output;
	}

	const ConfigurationOption& GetOption(const std::string& option)
	{
		return m_options[option];
	}

	const RenderPassList& GetPasses() const
	{
		return m_render_passes;
	}
	const RenderPass& GetPass(size_t index) const
	{
		return m_render_passes.at(index);
	}

	// For updating option's values
	void SetOptionf(const std::string& option, int index, float value);
	void SetOptioni(const std::string& option, int index, s32 value);
	void SetOptionb(const std::string& option, bool value);

	// Get a list of available post-processing shaders.
	static std::vector<std::string> GetAvailableShaderNames(const std::string& sub_dir);

	void* UpdateConfigurationBuffer(u32* buffer_size, bool packbuffer = false);
	void* GetConfigurationBuffer(u32* buffer_size);

private:
	struct ConfigBlock final
	{
		std::string m_type;
		std::vector<std::pair<std::string, std::string>> m_options;
	};

	using StringOptionList = std::vector<ConfigBlock>;
	bool m_configuration_buffer_dirty = true;
	bool m_any_options_dirty = false;
	bool m_compile_time_constants_dirty = false;
	bool m_requires_depth_buffer = false;
	std::string m_shader_name;
	std::string m_shader_source;
	ConfigMap m_options;
	RenderPassList m_render_passes;
	FrameOutput  m_frame_output;
	std::vector<Constant> m_constants;

	bool ParseShader(const std::string& dirname, const std::string& path);
	bool ParseConfiguration(const std::string& dirname, const std::string& configuration_string);

	std::vector<ConfigBlock> ReadConfigSections(const std::string& configuration_string);
	bool ParseConfigSections(const std::string& dirname, const std::vector<ConfigBlock>& config_blocks);

	bool ParseOptionBlock(const std::string& dirname, const ConfigBlock& block);
	bool ParsePassBlock(const std::string& dirname, const ConfigBlock& block);
	bool ParseFrameBlock(const ConfigBlock& block);
	bool LoadExternalImage(const std::string& path, RenderPass::Input* input);

	void CreateDefaultPass();
	void LoadOptionsConfigurationFromSection(IniFile::Section* section);
	void LoadOptionsConfiguration();

	static bool ValidatePassInputs(const RenderPass& pass);
};

class PostProcessor;

class PostProcessingShader
{
public:
	PostProcessingShader()
	{};
	virtual ~PostProcessingShader();

	TextureCacheBase::TCacheEntryBase* GetLastPassOutputTexture() const;
	bool IsLastPassScaled() const;

	bool IsReady() const
	{
		return m_ready;
	}

	bool Initialize(PostProcessingShaderConfiguration* config, int target_layers);
	bool Reconfigure(const TargetSize& new_size);
	virtual void MapAndUpdateConfigurationBuffer() = 0;
	virtual void Draw(PostProcessor* parent,
		const TargetRectangle& dst_rect, const TargetSize& dst_size, uintptr_t dst_texture,
		const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_texture,
		uintptr_t src_depth_texture, int src_layer, float gamma) = 0;

protected:
	struct InputBinding final
	{
		PostProcessingInputType type{};
		u32 texture_unit{};
		TargetSize size{};
		u32 frame_index{};
		TextureCacheBase::TCacheEntryBase* texture{};	// only set for external images
		TextureCacheBase::TCacheEntryBase* prev_texture{};
		uintptr_t texture_sampler{};
	};

	virtual void ReleaseBindingSampler(uintptr_t sampler) = 0;
	virtual uintptr_t CreateBindingSampler(const PostProcessingShaderConfiguration::RenderPass::Input& input_config) = 0;

	struct RenderPassData final
	{
		uintptr_t shader{};

		std::vector<InputBinding> inputs;

		TextureCacheBase::TCacheEntryBase* output_texture{};
		TargetSize output_size{};
		float output_scale{};
		PC_TexFormat output_format = PC_TexFormat::PC_TEX_FMT_RGBA32;

		bool enabled{};
	};

	virtual void ReleasePassNativeResources(RenderPassData& pass) = 0;


	bool CreatePasses();
	virtual bool RecompileShaders() = 0;
	bool ResizeOutputTextures(const TargetSize& new_size);
	void LinkPassOutputs();

	PostProcessingShaderConfiguration* m_config;
	uintptr_t m_uniform_buffer;

	TargetSize m_internal_size;
	int m_internal_layers = 0;

	std::vector<RenderPassData> m_passes;
	size_t m_last_pass_index = 0;
	bool m_last_pass_uses_color_buffer = false;
	bool m_ready = false;
	bool m_prev_frame_enabled = false;
	bool m_prev_depth_enabled = false;
	struct past_frame_data
	{
		TextureCacheBase::TCacheEntryBase* color_frame;
		TextureCacheBase::TCacheEntryBase* depth_frame;
	};
	std::vector<past_frame_data> m_prev_frame_texture;
	TargetSize m_prev_frame_size{};
	TargetSize m_prev_depth_frame_size{};
	int m_prev_frame_index = -1;
	int m_prev_depth_frame_index = -1;
	inline TextureCacheBase::TCacheEntryBase* GetPrevColorFrame(int frame_index)
	{
		if (!m_prev_frame_enabled)
		{
			return nullptr;
		}
		int index = static_cast<int>(m_config->GetFrameOutput().color_count);
		index = (m_prev_frame_index - frame_index + index) % index;
		return m_prev_frame_texture[index].color_frame;
	}

	inline TextureCacheBase::TCacheEntryBase* GetPrevDepthFrame(int frame_index)
	{
		if (!m_prev_depth_enabled)
		{
			return nullptr;
		}
		int index = static_cast<int>(m_config->GetFrameOutput().depth_count);
		index = (m_prev_depth_frame_index - frame_index + index) % index;
		return m_prev_frame_texture[index].depth_frame;
	}
	inline void IncrementFrame()
	{
		if(m_prev_frame_enabled)
			m_prev_frame_index = (m_prev_frame_index + 1) % m_config->GetFrameOutput().color_count;
		if(m_prev_depth_enabled)
			m_prev_depth_frame_index = (m_prev_depth_frame_index + 1) % m_config->GetFrameOutput().depth_count;
	}

};

class PostProcessor
{
public:
	// Size of the constant buffer reserved for post-processing.
	// 4KiB = 256 constants, a shader should not have this many options.
	static const size_t UNIFORM_BUFFER_SIZE = 4096;

	// List of texture sizes for shader inputs, used to update uniforms.
	using InputTextureSizeArray = std::array<TargetSize, POST_PROCESSING_MAX_TEXTURE_INPUTS>;
	// Constructor needed for timer object
	PostProcessor(API_TYPE apitype);
	virtual ~PostProcessor();

	// Get a list of post-processing shaders.


	// Get the config for a shader in the current chain.
	PostProcessingShaderConfiguration* GetPostShaderConfig(const std::string& shader_name);

	// Get the current blit shader config.
	PostProcessingShaderConfiguration* GetScalingShaderConfig()
	{
		return m_scaling_config.get();
	}

	bool IsActive() const
	{
		return m_active;
	}
	bool RequiresDepthBuffer() const
	{
		return m_requires_depth_buffer;
	}
	bool ShouldTriggerOnSwap() const;

	bool ShouldTriggerAfterBlit() const;

	bool XFBDepthDataRequired() const;

	void SetReloadFlag()
	{
		m_reload_flag.Set();
	}
	bool RequiresReload()
	{
		return m_reload_flag.TestAndClear();
	}

	void OnProjectionLoaded(u32 type);
	void OnEFBCopy(const TargetRectangle* src_rect);
	void OnEndFrame();

	// Should be implemented by the backends for backend specific code
	virtual bool Initialize() = 0;
	void ReloadShaders();

	void DoEFB(const TargetRectangle* src_rect);
	// Used when post-processing on perspective->ortho switch.
	virtual void PostProcessEFB(const TargetRectangle& target_rect, const TargetSize& target_size) = 0;

	// Used when virtual xfb is enabled
	virtual void PostProcessEFBToTexture(uintptr_t dst_texture) = 0;

	// Copy/resize src_texture to dst_texture (0 means backbuffer), using the resize/blit shader.
	void BlitScreen(const TargetRectangle& dst_rect, const TargetSize& dst_size, uintptr_t dst_texture,
		const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_texture, uintptr_t src_depth_texture,
		int src_layer, float gamma = 1.0f);

	// Post-process the source image, if output_texture is null, it will be written back to src_texture,
	// otherwise a temporary texture will be returned that is valid until the next call to PostProcess.
	void PostProcess(TargetRectangle* output_rect, TargetSize* output_size, uintptr_t* output_texture,
		const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_texture,
		const TargetRectangle& src_depth_rect, const TargetSize& src_depth_size, uintptr_t src_depth_texture,
		uintptr_t dst_texture = 0, const TargetRectangle* dst_rect = 0, const TargetSize* dst_size = 0);

	// Construct the options uniform buffer source for the specified config.
	static void GetUniformBufferShaderSource(API_TYPE api, const PostProcessingShaderConfiguration* config, std::string& shader_source);

	// Construct a complete fragment shader (HLSL/GLSL) for the specified pass.
	static std::string GetCommonFragmentShaderSource(API_TYPE api, const PostProcessingShaderConfiguration* config, int texture_register_start = 9);

	// Construct a complete fragment shader (HLSL/GLSL) for the specified pass.
	static std::string GetPassFragmentShaderSource(API_TYPE api, const PostProcessingShaderConfiguration* config, const PostProcessingShaderConfiguration::RenderPass* pass);

	// Scale a target resolution to an output's scale
	static TargetSize ScaleTargetSize(const TargetSize& orig_size, float scale);

	// Scale a target rectangle to an output's scale
	static TargetRectangle ScaleTargetRectangle(API_TYPE api, const TargetRectangle& src, float scale);
	
	void*  GetConstatsData()
	{
		return m_current_constants.data();
	}

protected:
	virtual std::unique_ptr<PostProcessingShader> CreateShader(PostProcessingShaderConfiguration* config) = 0;
	// NOTE: Can change current render target and viewport.
	// If src_layer <0, copy all layers, otherwise, copy src_layer to layer 0.
	virtual void CopyTexture(const TargetRectangle& dst_rect, uintptr_t dst_texture,
		const TargetRectangle& src_rect, uintptr_t src_texture,
		const TargetSize& src_size, int src_layer, bool is_depth_texture = false,
		bool force_shader_copy = false) = 0;

	void DrawStereoBuffers(const TargetRectangle& dst_rect, const TargetSize& dst_size, uintptr_t dst_texture,
		const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_texture, uintptr_t src_depth_texture, float gamma);

	void CreatePostProcessingShaders();
	void CreateScalingShader();
	void CreateStereoShader();
	bool ReconfigurePostProcessingShaders(const TargetSize& size);
	bool ReconfigureScalingShader(const TargetSize& size);
	bool ReconfigureStereoShader(const TargetSize& size);
	bool ResizeCopyBuffers(const TargetSize& size, int layers);
	bool ResizeStereoBuffer(const TargetSize& size);

	enum PROJECTION_STATE : u32
	{
		PROJECTION_STATE_INITIAL,
		PROJECTION_STATE_PERSPECTIVE,
		PROJECTION_STATE_FINAL
	};

	// Update constant buffer with the current values from the config.
	// Returns true if the buffer contents has changed.
	bool UpdateConstantUniformBuffer(
		const InputTextureSizeArray& input_sizes,
		const TargetRectangle& dst_rect, const TargetSize& dst_size,
		const TargetRectangle& src_rect, const TargetSize& src_size,
		int src_layer, float gamma);

	// Load m_configs with the selected post-processing shaders.
	void ReloadShaderConfigs();
	void ReloadPostProcessingShaderConfigs();
	void ReloadScalingShaderConfig();
	void ReloadStereoShaderConfig();
	void DisablePostProcessor();

	// Timer for determining our time value
	Common::Timer m_timer;

	// Set by UI thread when the shader changes
	Common::Flag m_reload_flag;

	// List of current post-processing shaders, ordered by application order
	std::vector<std::string> m_shader_names;

	// Current post-processing shader config
	std::unordered_map<std::string, std::unique_ptr<PostProcessingShaderConfiguration>> m_shader_configs;

	// Blit/anaglyph shader config
	std::unique_ptr<PostProcessingShaderConfiguration> m_scaling_config;

	// Stereo shader config
	std::unique_ptr<PostProcessingShaderConfiguration> m_stereo_config;

	// shaders
	std::vector<std::unique_ptr<PostProcessingShader>> m_post_processing_shaders;
	std::unique_ptr<PostProcessingShader> m_scaling_shader;
	std::unique_ptr<PostProcessingShader> m_stereo_shader;

	// buffers
	TargetSize m_copy_size;
	int m_copy_layers = 0;
	TextureCacheBase::TCacheEntryBase* m_color_copy_texture = nullptr;
	TextureCacheBase::TCacheEntryBase* m_depth_copy_texture = nullptr;

	TargetSize m_stereo_buffer_size;
	TextureCacheBase::TCacheEntryBase* m_stereo_buffer_texture = nullptr;

	// Projection state for detecting when to apply post
	PROJECTION_STATE m_projection_state = PROJECTION_STATE_INITIAL;

	// Global post-processing enable state
	bool m_active = false;
	bool m_requires_depth_buffer = false;
	const API_TYPE m_APIType;
	// Uniform buffer data, double-buffered so we don't update if unnecessary
	std::array<Constant, POST_PROCESSING_CONTANTS> m_current_constants;
	std::array<Constant, POST_PROCESSING_CONTANTS> m_new_constants;

	// common shader code between backends
	static const std::string s_post_fragment_header_ogl;
	static const std::string s_post_fragment_header_d3d;
	static const std::string s_post_fragment_header_common;
};