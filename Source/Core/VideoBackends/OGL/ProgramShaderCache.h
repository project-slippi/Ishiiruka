// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/LinearDiskCache.h"
#include "Core/ConfigManager.h"
#include "Common/GL/GLUtil.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"

namespace OGL
{

class SHADERUID
{
public:
	VertexShaderUid vuid;
	PixelShaderUid puid;
	GeometryShaderUid guid;

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
};


struct SHADER
{
	SHADER() : glprogid(0) { }
	void Destroy()
	{
		glDeleteProgram(glprogid);
		glprogid = 0;
	}
	GLuint glprogid; // OpenGL program id

	void SetProgramVariables();
	void SetProgramBindings();
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

	typedef std::map<SHADERUID, PCacheEntry> PCache;

	static PCacheEntry GetShaderProgram();
	static GLuint GetCurrentProgram();
	static SHADER* SetShader(PIXEL_SHADER_RENDER_MODE render_mode, u32 components, u32 primitive_type);
	static void GetShaderId(SHADERUID *uid, PIXEL_SHADER_RENDER_MODE render_mode, u32 components, u32 primitive_type);

	static bool CompileShader(SHADER &shader, const char* vcode, const char* pcode, const char* gcode = nullptr, const char **macros = nullptr, const u32 macro_count = 0);
	static GLuint CompileSingleShader(GLuint type, const char *code, const char **macros = nullptr, const u32 count = 0);
	static void UploadConstants();

	static void Init();
	static void Shutdown();
	static void CreateHeader();

	static u32 GetUniformBufferAlignment();
	static void BindUniformBuffer();

private:
	class ProgramShaderCacheInserter : public LinearDiskCacheReader<SHADERUID, u8>
	{
	public:
		void Read(const SHADERUID &key, const u8 *value, u32 value_size) override;
	};

	static PCache pshaders;
	static PCacheEntry* last_entry;
	static SHADERUID last_uid;

	static u32 s_v_ubo_buffer_size;
	static u32 s_p_ubo_buffer_size;
	static u32 s_g_ubo_buffer_size;
	static s32 s_ubo_align;
};

}  // namespace OGL
