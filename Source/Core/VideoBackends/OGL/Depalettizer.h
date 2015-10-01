// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/GL/GLUtil.h"

namespace OGL
{

class Depalettizer
{

public:

	enum BaseType
	{
		Unorm4,
		Unorm8
	};

	Depalettizer();
	~Depalettizer();

	bool Depalettize(BaseType baseType, GLuint dstTex, GLuint baseTex, u32 width, u32 height);
	void UploadPalette(u32 tlutFmt, void* addr, u32 size);
private:
	enum InternalPaletteFormat
	{
		IA = 0,
		RGB565,
		RGBA8
	};
	struct DepalProgram
	{
		DepalProgram()
			: program(0)
		{ }
		~DepalProgram();

		GLuint program;
	};
	DepalProgram* GetProgram(BaseType type);

	GLuint m_fbo;

	DepalProgram m_unorm4Program;
	DepalProgram m_unorm8Program;
	
	InternalPaletteFormat m_pallete_format;
	GLuint m_palette_texture[3][2];
	u8 m_temp_decoding_buffer[256 * 4];

};

}