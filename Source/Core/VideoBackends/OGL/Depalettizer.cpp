// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/OGL/Depalettizer.h"
#include "VideoBackends/OGL/FramebufferManager.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/Render.h"

#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/LookUpTables.h"

namespace OGL
{

static void OpenGL_PrintProgramInfoLog(GLuint program)
{
	GLsizei length = 0;

	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
	if (length > 0)
	{
		GLsizei charsWritten;
		GLchar* infoLog = new GLchar[length];
		glGetProgramInfoLog(program, length, &charsWritten, infoLog);
		WARN_LOG(VIDEO, "Program info log:\n%s", infoLog);
		delete[] infoLog;
	}
}

static bool OpenGL_LinkProgram(GLuint program)
{
	bool result = true;
	glLinkProgram(program);
	OpenGL_PrintProgramInfoLog(program);

	GLint linkStatus;
	glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
	if (linkStatus != GL_TRUE)
	{
		// Link failed
		ERROR_LOG(VIDEO, "Program link failed; see info log");
		result = false;
	}
	return result;
}

static const char DEPALETTIZE_FS[] = R"GLSL(
//
#ifndef NUM_COLORS
#error NUM_COLORS was not defined
#endif

// Uniforms
SAMPLER_BINDING(9) uniform sampler2DArray u_Base;
SAMPLER_BINDING(10) uniform sampler1D u_Palette;

// Shader entry point
void main()
{
	float src = texture(u_Base, vec3(gl_TexCoord[0].xy, 0)).r;
	float index = round(src * (NUM_COLORS-1));
	gl_FragColor = texture1D(u_Palette, (index + 0.5) / NUM_COLORS);
}
//
)GLSL";

Depalettizer::Depalettizer()
	: m_fbo(0)
{
	for (size_t i = 0; i < 3; i++)
	{
		for (size_t j = 0; j < 2; j++)
		{
			m_palette_texture[i][j] = 0;
		}
	}
}

Depalettizer::~Depalettizer()
{
	for (size_t i = 0; i < 3; i++)
	{
		for (size_t j = 0; j < 2; j++)
		{
			if (m_palette_texture[i][j])
			{
				glDeleteTextures(1, &m_palette_texture[i][j]);
			}
		}
	}
	glDeleteFramebuffers(1, &m_fbo);
	m_fbo = 0;
}

Depalettizer::DepalProgram::~DepalProgram()
{
	glDeleteProgram(program);
	program = 0;
}

bool Depalettizer::Depalettize(BaseType source_type, GLuint dstTex, GLuint baseTex,
	u32 width, u32 height)
{
	DepalProgram* program = GetProgram(source_type);
	if (!program || !program->program)
		return false;

	if (!m_fbo)
		glGenFramebuffers(1, &m_fbo);

	g_renderer->ResetAPIState();

	// Bind destination texture to the framebuffer
	FramebufferManager::SetFramebuffer(m_fbo);
	FramebufferManager::FramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_ARRAY, dstTex, 0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	// Bind fragment program and uniforms
	glUseProgram(program->program);

	// Bind base texture to sampler 9
	glActiveTexture(GL_TEXTURE9);
	glEnable(GL_TEXTURE_2D_ARRAY);
	glBindTexture(GL_TEXTURE_2D_ARRAY, baseTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	// Bind palette texture to sampler 10
	glActiveTexture(GL_TEXTURE10);
	glEnable(GL_TEXTURE_1D);
	glBindTexture(GL_TEXTURE_1D, m_palette_texture[m_pallete_format][source_type]);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glViewport(0, 0, width, height);

	// Depalettize!
	glBegin(GL_QUADS);
	glTexCoord2f(0.f, 1.f); glVertex2f(-1.f, 1.f); // Upper left
	glTexCoord2f(0.f, 0.f); glVertex2f(-1.f, -1.f); // Lower left
	glTexCoord2f(1.f, 0.f); glVertex2f(1.f, -1.f); // Lower right
	glTexCoord2f(1.f, 1.f); glVertex2f(1.f, 1.f); // Upper right
	glEnd();

	// Clean up

	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_1D, 0);
	glDisable(GL_TEXTURE_1D);

	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	glDisable(GL_TEXTURE_2D_ARRAY);

	glUseProgram(0);

	g_renderer->RestoreAPIState();

	FramebufferManager::SetFramebuffer(0);
	return true;
}

Depalettizer::DepalProgram* Depalettizer::GetProgram(BaseType type)
{
	// TODO: Clean up duplicate code
	const char* macros[1];
	switch (type)
	{
	case Unorm4:
		if (!m_unorm4Program.program)
		{
			macros[0] = "#define NUM_COLORS 16\n";
			GLuint shader = ProgramShaderCache::CompileSingleShader(GL_FRAGMENT_SHADER, DEPALETTIZE_FS, macros, 1);
			if (!shader)
			{
				ERROR_LOG(VIDEO, "Failed to compile Unorm4 depalettizer");
				return NULL;
			}

			m_unorm4Program.program = glCreateProgram();
			glAttachShader(m_unorm4Program.program, shader);
			if (!OpenGL_LinkProgram(m_unorm4Program.program))
			{
				glDeleteProgram(m_unorm4Program.program);
				glDeleteShader(shader);
				m_unorm4Program.program = 0;
				ERROR_LOG(VIDEO, "Failed to link Unorm4 depalettizer");
				return NULL;
			}

			// Shader is now embedded in the program
			glDeleteShader(shader);
		}
		return &m_unorm4Program;
	case Unorm8:
		if (!m_unorm8Program.program)
		{
			macros[0] = "#define NUM_COLORS 256\n";
			GLuint shader = ProgramShaderCache::CompileSingleShader(GL_FRAGMENT_SHADER, DEPALETTIZE_FS, macros, 1);
			if (!shader)
			{
				ERROR_LOG(VIDEO, "Failed to compile Unorm8 depalettizer");
				return NULL;
			}

			m_unorm8Program.program = glCreateProgram();
			glAttachShader(m_unorm8Program.program, shader);
			if (!OpenGL_LinkProgram(m_unorm8Program.program))
			{
				glDeleteProgram(m_unorm8Program.program);
				glDeleteShader(shader);
				m_unorm8Program.program = 0;
				ERROR_LOG(VIDEO, "Failed to link Unorm8 depalettizer");
				return NULL;
			}

			// Shader is now embedded in the program
			glDeleteShader(shader);
		}
		return &m_unorm8Program;
	default:
		_assert_msg_(VIDEO, 0, "Invalid depalettizer base type");
		return NULL;
	}
}

inline void DecodeIA8Palette(u16* dst, const u16* src, unsigned int numColors)
{
	for (unsigned int i = 0; i < numColors; ++i)
	{
		dst[i] = src[i];
	}
}

inline void DecodeRGB565Palette(u16* dst, const u16* src, unsigned int numColors)
{
	for (unsigned int i = 0; i < numColors; ++i)
		dst[i] = Common::swap16(src[i]);
}

inline u32 decode5A3(u16 val)
{
	int r, g, b, a;
	if ((val & 0x8000))
	{
		a = 0xFF;
		r = Convert5To8((val >> 10) & 0x1F);
		g = Convert5To8((val >> 5) & 0x1F);
		b = Convert5To8(val & 0x1F);
	}
	else
	{
		a = Convert3To8((val >> 12) & 0x7);
		r = Convert4To8((val >> 8) & 0xF);
		g = Convert4To8((val >> 4) & 0xF);
		b = Convert4To8(val & 0xF);
	}
	return (a << 24) | (b << 16) | (g << 8) | r;
}

// Decode to BGRA
inline void DecodeRGB5A3Palette(u32* dst, const u16* src, unsigned int numColors)
{
	for (unsigned int i = 0; i < numColors; ++i)
	{
		dst[i] = decode5A3(Common::swap16(src[i]));
	}
}

void Depalettizer::UploadPalette(u32 tlutFmt, void* addr, u32 size)
{
	InternalPaletteFormat format = IA;
	BaseType source_type = BaseType::Unorm4;
	u32 numColors = size / sizeof(u16);
	if (numColors <= 16)
	{
		source_type = BaseType::Unorm4;
	}
	else if (numColors <= 256)
	{
		source_type = BaseType::Unorm8;
	}
	else
	{
		return;
	}
	switch (tlutFmt)
	{
	case GX_TL_IA8: format = IA; break;
	case GX_TL_RGB565: format = RGB565; break;
	case GX_TL_RGB5A3: format = RGBA8; break;
	default:
		ERROR_LOG(VIDEO, "Invalid TLUT format");
		return;
	}
	const u16* tlut = (const u16*)addr;

	if (!m_palette_texture[tlutFmt][source_type])
		glGenTextures(1, &m_palette_texture[tlutFmt][source_type]);

	glBindTexture(GL_TEXTURE_1D, m_palette_texture[format][source_type]);

	GLint useInternalFormat;
	GLenum useFormat;
	GLenum useType;

	switch (format)
	{
	case IA:
		useInternalFormat = GL_LUMINANCE8_ALPHA8;
		useFormat = GL_LUMINANCE_ALPHA;
		useType = GL_UNSIGNED_BYTE;
		DecodeIA8Palette((u16*)m_temp_decoding_buffer, tlut, numColors);
		break;
	case RGB565:
		useInternalFormat = GL_RGB;
		useFormat = GL_RGB;
		useType = GL_UNSIGNED_SHORT_5_6_5;
		DecodeRGB565Palette((u16*)m_temp_decoding_buffer, tlut, numColors);
		break;
	case RGBA8:
		useInternalFormat = 4;
		useFormat = GL_RGBA;
		useType = GL_UNSIGNED_BYTE;
		DecodeRGB5A3Palette((u32*)m_temp_decoding_buffer, tlut, numColors);
		break;
	}

	glTexImage1D(GL_TEXTURE_1D, 0, useInternalFormat, numColors, 0, useFormat, useType, m_temp_decoding_buffer);

	glBindTexture(GL_TEXTURE_1D, 0);

	m_pallete_format = format;
}

}
