// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/GL/GLUtil.h"
#include "Common/LinearDiskCache.h"

#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/ObjectUsageProfiler.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"

namespace OGL
{

class SHADERUID
{
	size_t hash = {};
public:
	VertexShaderUid vuid;
	PixelShaderUid puid;
	GeometryShaderUid guid;
	void CalculateHash()
	{
		VertexShaderUid::ShaderUidHasher vshasher;
		PixelShaderUid::ShaderUidHasher pshasher;
		GeometryShaderUid::ShaderUidHasher gshasher;
		hash = vshasher(vuid) ^ pshasher(puid) ^ gshasher(guid);
	}
	bool operator <(const SHADERUID& r) const
	{
		if (puid < r.puid)
			return true;

		if (r.puid < puid)
			return false;

		if (vuid < r.vuid)
			return true;

		if (r.vuid < vuid)
			return false;

		if (guid < r.guid)
			return true;

		return false;
	}

	bool operator ==(const SHADERUID& r) const
	{
		return puid == r.puid && vuid == r.vuid && guid == r.guid;
	}

	struct ShaderUidHasher
	{
		std::size_t operator()(const SHADERUID& k) const
		{
			return k.hash;
		}
	};
};


struct SHADER
{
	SHADER() : glprogid(0), initialized(false)
	{}
	void Destroy()
	{
		if (glprogid != 0)
		{
			glDeleteProgram(glprogid);
		}
		glprogid = 0;
		initialized = false;
	}
	GLuint glprogid; // OpenGL program id
	bool initialized;
	void SetProgramVariables();
	void SetProgramBindings(bool is_compute);
	void Bind();
};

class ProgramShaderCache
{
public:

	struct PCacheEntry
	{
		SHADER shader;
		bool in_cache;

		void Destroy()
		{
			shader.Destroy();
		}
	};

	typedef ObjectUsageProfiler<SHADERUID, pKey_t, PCacheEntry, SHADERUID::ShaderUidHasher> PCache;

	static GLuint GetCurrentProgram();
	static SHADER* SetShader(PIXEL_SHADER_RENDER_MODE render_mode, u32 components, u32 primitive_type);
	static SHADER* CompileShader(const SHADERUID& uid);
	static void GetShaderId(SHADERUID *uid, PIXEL_SHADER_RENDER_MODE render_mode, u32 components, u32 primitive_type);

	static bool CompileShader(SHADER &shader, const char* vcode, const char* pcode, const char* gcode = nullptr, const char **macros = nullptr, const u32 macro_count = 0);
	static bool CompileComputeShader(SHADER& shader, const std::string& code);
	static GLuint CompileSingleShader(GLuint type, const char *code, const char **macros = nullptr, const u32 count = 0);
	static void UploadConstants();

	static void Init();
	static void Shutdown();
	static void CreateHeader();

	static u32 GetUniformBufferAlignment();

private:
	class ProgramShaderCacheInserter : public LinearDiskCacheReader<SHADERUID, u8>
	{
	public:
		void Read(const SHADERUID &key, const u8 *value, u32 value_size) override;
	};

	static PCache* pshaders;
	static std::array<PCacheEntry*, PIXEL_SHADER_RENDER_MODE::PSRM_DEPTH_ONLY + 1> last_entry;
	static std::array<SHADERUID, PIXEL_SHADER_RENDER_MODE::PSRM_DEPTH_ONLY + 1>  last_uid;

	static u32 s_ubo_buffer_size;
	static u32 s_p_ubo_buffer_size;
	static u32 s_v_ubo_buffer_size;
	static u32 s_g_ubo_buffer_size;
	static s32 s_ubo_align;
};

}  // namespace OGL
