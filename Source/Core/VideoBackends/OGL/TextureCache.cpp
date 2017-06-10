// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>
#include <fstream>
#include <memory>
#include <vector>

#ifdef _WIN32
#include <intrin.h>
#endif

#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/MemoryUtil.h"
#include "Common/StringUtil.h"

#include "Core/HW/Memmap.h"

#include "VideoBackends/OGL/FramebufferManager.h"
#include "VideoBackends/OGL/GPUTimer.h"
#include "Common/GL/GLInterfaceBase.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/SamplerCache.h"
#include "VideoBackends/OGL/StreamBuffer.h"
#include "VideoBackends/OGL/TextureCache.h"
#include "VideoBackends/OGL/TextureConverter.h"

#include "VideoCommon/BPStructs.h"
#include "VideoCommon/HiresTextures.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/TextureConversionShader.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/TextureScalerCommon.h"
#include "VideoCommon/VideoConfig.h"

#define 	GL_COMPRESSED_RGB_S3TC_DXT1_EXT   0x83F0
#define 	GL_COMPRESSED_RGBA_S3TC_DXT1_EXT   0x83F1
#define 	GL_COMPRESSED_RGBA_S3TC_DXT3_EXT   0x83F2
#define 	GL_COMPRESSED_RGBA_S3TC_DXT5_EXT   0x83F3

namespace OGL
{
static SHADER s_ColorCopyProgram;
static SHADER s_ColorMatrixProgram;
static SHADER s_DepthMatrixProgram;
static GLuint s_ColorMatrixUniform;
static GLuint s_DepthMatrixUniform;
static GLuint s_ColorCopyPositionUniform;
static GLuint s_ColorMatrixPositionUniform;
static GLuint s_DepthCopyPositionUniform;
static u32 s_ColorCbufid;
static u32 s_DepthCbufid;

static u32 s_Textures[8];
static u32 s_ActiveTexture;

static SHADER s_palette_pixel_shader[3];
static std::unique_ptr<StreamBuffer> s_palette_stream_buffer;
static GLuint s_palette_resolv_texture;
static GLuint s_palette_buffer_offset_uniform[3];
static GLuint s_palette_multiplier_uniform[3];
static GLuint s_palette_copy_position_uniform[3];
static u32 s_last_pallet_Buffer;
static TlutFormat s_last_TlutFormat = TlutFormat::GX_TL_IA8;

struct TextureDecodingProgramInfo
{
	const TextureConversionShader::DecodingShaderInfo* base_info = nullptr;
	SHADER program;
	GLint uniform_dst_size = -1;
	GLint uniform_src_size = -1;
	GLint uniform_src_row_stride = -1;
	GLint uniform_src_offset = -1;
	GLint uniform_palette_offset = -1;
	bool valid = false;
};

//#define TIME_TEXTURE_DECODING 1

static std::map<std::pair<u32, u32>, TextureDecodingProgramInfo> s_texture_decoding_program_info;
static std::array<GLuint, TextureConversionShader::BUFFER_FORMAT_COUNT>
s_texture_decoding_buffer_views;
static void CreateTextureDecodingResources();
static void DestroyTextureDecodingResources();

bool SaveTexture(const std::string& filename, u32 textarget, u32 tex, int virtual_width, int virtual_height, u32 level, bool compressed)
{
	if (GLInterface->GetMode() != GLInterfaceMode::MODE_OPENGL)
		return false;
	int width = std::max(virtual_width >> level, 1);
	int height = std::max(virtual_height >> level, 1);
	int size = compressed ? (((width + 3) >> 2) * ((height + 3) >> 2) * 16) : (width * height * 4);
	std::vector<u8> data(size);
	glActiveTexture(GL_TEXTURE9);
	glBindTexture(textarget, tex);
	bool saved = false;
	if (compressed)
	{
		glGetCompressedTexImage(textarget, level, data.data());
		saved = TextureToDDS(data.data(), width * 4, filename, width, height);
	}
	else
	{
		glGetTexImage(textarget, level, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
		saved = TextureToPng(data.data(), width * 4, filename, width, height);
	}
	TextureCache::SetStage();
	return saved;
}

TextureCache::TCacheEntry::~TCacheEntry()
{
	if (texture)
	{
		for (auto& gtex : s_Textures)
			if (gtex == texture)
				gtex = 0;
		glDeleteTextures(1, &texture);
		texture = 0;
	}

	if (nrm_texture)
	{
		glDeleteTextures(1, &nrm_texture);
		nrm_texture = 0;
	}

	if (framebuffer)
	{
		glDeleteFramebuffers(1, &framebuffer);
		framebuffer = 0;
	}
}

TextureCache::TCacheEntry::TCacheEntry(const TCacheEntryConfig& _config)
	: TCacheEntryBase(_config)
{
	glGenTextures(1, &texture);
	nrm_texture = 0;
	framebuffer = 0;
}

void TextureCache::TCacheEntry::Bind(u32 stage)
{
	if (nrm_texture && g_ActiveConfig.HiresMaterialMapsEnabled())
	{
		s_ActiveTexture = 8 + stage;
		glActiveTexture(GL_TEXTURE8 + stage);
		glBindTexture(GL_TEXTURE_2D_ARRAY, nrm_texture);
	}
	if (s_Textures[stage] != texture || !g_ActiveConfig.backend_info.bSupportsBindingLayout)
	{
		if (s_ActiveTexture != stage)
		{
			glActiveTexture(GL_TEXTURE0 + stage);
			s_ActiveTexture = stage;
		}

		glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
		s_Textures[stage] = texture;
	}
}

bool TextureCache::TCacheEntry::Save(const std::string& filename, u32 level)
{
	return SaveTexture(filename, GL_TEXTURE_2D_ARRAY, texture, config.width, config.height, level, this->compressed);
}

PC_TexFormat TextureCache::GetNativeTextureFormat(const s32 texformat, const TlutFormat tlutfmt, u32 width, u32 height)
{
	const bool compressed_supported = ((width & 3) == 0) && ((height & 3) == 0);
	PC_TexFormat pcfmt = GetPC_TexFormat(texformat, tlutfmt, compressed_supported);
	pcfmt = !g_ActiveConfig.backend_info.bSupportedFormats[pcfmt] ? PC_TEX_FMT_RGBA32 : pcfmt;
	return pcfmt;
}

void TextureCache::TCacheEntry::SetFormat()
{
	compressed = false;
	switch (config.pcformat)
	{
	default:
	case PC_TEX_FMT_NONE:
		PanicAlert("Invalid PC texture format %i", config.pcformat);
	case PC_TEX_FMT_BGRA32:
		gl_format = GL_BGRA;
		gl_iformat = GL_RGBA;
		gl_siformat = GL_RGBA8;
		gl_type = GL_UNSIGNED_BYTE;
		break;

	case PC_TEX_FMT_RGBA32:
		gl_format = GL_RGBA;
		gl_iformat = GL_RGBA;
		gl_siformat = GL_RGBA8;
		gl_type = GL_UNSIGNED_BYTE;
		break;
	case PC_TEX_FMT_I4_AS_I8:
		gl_format = GL_LUMINANCE;
		gl_iformat = GL_INTENSITY4;
		gl_siformat = GL_R8;
		gl_type = GL_UNSIGNED_BYTE;
		break;

	case PC_TEX_FMT_IA4_AS_IA8:
		gl_format = GL_LUMINANCE_ALPHA;
		gl_iformat = GL_LUMINANCE4_ALPHA4;
		gl_siformat = GL_RG8;
		gl_type = GL_UNSIGNED_BYTE;
		break;

	case PC_TEX_FMT_I8:
		gl_format = GL_LUMINANCE;
		gl_iformat = GL_INTENSITY8;
		gl_siformat = GL_R8;
		gl_type = GL_UNSIGNED_BYTE;
		break;

	case PC_TEX_FMT_IA8:
		gl_format = GL_LUMINANCE_ALPHA;
		gl_iformat = GL_LUMINANCE8_ALPHA8;
		gl_siformat = GL_RG8;
		gl_type = GL_UNSIGNED_BYTE;
		break;
	case PC_TEX_FMT_RGB565:
		gl_format = GL_RGB;
		gl_iformat = GL_RGB;
		gl_siformat = GL_RGB5;
		gl_type = GL_UNSIGNED_SHORT_5_6_5;
		break;
	case PC_TEX_FMT_DXT1:
		gl_format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		gl_iformat = GL_RGB;
		gl_siformat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		gl_type = GL_UNSIGNED_BYTE;
		compressed = true;
		break;
	case PC_TEX_FMT_DXT3:
		gl_format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		gl_iformat = GL_RGBA;
		gl_siformat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		gl_type = GL_UNSIGNED_BYTE;
		compressed = true;
		break;
	case PC_TEX_FMT_DXT5:
		gl_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		gl_iformat = GL_RGBA;
		gl_siformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		gl_type = GL_UNSIGNED_BYTE;
		compressed = true;
		break;
	case PC_TEX_FMT_DEPTH_FLOAT:
		gl_format = GL_DEPTH_COMPONENT32F;
		gl_iformat = GL_DEPTH_COMPONENT;
		gl_siformat = GL_DEPTH_COMPONENT32;
		gl_type = GL_FLOAT;
		compressed = false;
		break;
	case PC_TEX_FMT_R_FLOAT:
		gl_format = GL_R32F;
		gl_iformat = GL_RED;
		gl_siformat = GL_R32F;
		gl_type = GL_FLOAT;
		compressed = false;
		break;
	case PC_TEX_FMT_RGBA16_FLOAT:
		gl_format = GL_RGBA16F;
		gl_iformat = GL_RGBA;
		gl_siformat = GL_RGBA16F;
		gl_type = GL_FLOAT;
		compressed = false;
		break;
	case PC_TEX_FMT_RGBA_FLOAT:
		gl_format = GL_RGBA32F;
		gl_iformat = GL_RGBA;
		gl_siformat = GL_RGBA32F;
		gl_type = GL_FLOAT;
		compressed = false;
		break;
	}
}

TextureCache::TCacheEntryBase* TextureCache::CreateTexture(const TCacheEntryConfig& config)
{
	TCacheEntry* entry = new TCacheEntry(config);

	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D_ARRAY, entry->texture);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, config.levels - 1);
	entry->SetFormat();

	if (g_ogl_config.bSupportsTextureStorage)
	{
		glTexStorage3D(GL_TEXTURE_2D_ARRAY, config.levels, entry->gl_siformat, config.width, config.height,
			config.layers);
	}

	if (config.rendertarget)
	{
		if (!g_ogl_config.bSupportsTextureStorage)
		{
			for (u32 level = 0; level < config.levels; level++)
			{
				glTexImage3D(GL_TEXTURE_2D_ARRAY, level, entry->gl_format,
					std::max(config.width >> level, 1u),
					std::max(config.height >> level, 1u),
					config.layers, 0, entry->gl_iformat, entry->gl_type, nullptr);
			}
		}
		glGenFramebuffers(1, &entry->framebuffer);
		FramebufferManager::SetFramebuffer(entry->framebuffer);
		FramebufferManager::FramebufferTexture(GL_FRAMEBUFFER, (entry->gl_iformat == GL_DEPTH_COMPONENT) ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_ARRAY, entry->texture, 0);
	}
	else
	{
		if (config.materialmap)
		{
			glGenTextures(1, &entry->nrm_texture);			
			glBindTexture(GL_TEXTURE_2D_ARRAY, entry->nrm_texture);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, config.levels - 1);
			if (g_ogl_config.bSupportsTextureStorage)
			{
				glTexStorage3D(GL_TEXTURE_2D_ARRAY, config.levels, entry->gl_siformat, config.width, config.height,
					config.layers);
			}
		}
	}
	TextureCache::SetStage();
	return entry;
}

void TextureCache::TCacheEntry::CopyRectangleFromTexture(
	const TCacheEntryBase* source,
	const MathUtil::Rectangle<int> &srcrect,
	const MathUtil::Rectangle<int> &dstrect)
{
	TCacheEntry* srcentry = (TCacheEntry*)source;
	if (srcrect.GetWidth() == dstrect.GetWidth()
		&& srcrect.GetHeight() == dstrect.GetHeight()
		&& g_ogl_config.bSupportsCopySubImage)
	{
		glCopyImageSubData(
			srcentry->texture,
			GL_TEXTURE_2D_ARRAY,
			0,
			srcrect.left,
			srcrect.top,
			0,
			texture,
			GL_TEXTURE_2D_ARRAY,
			0,
			dstrect.left,
			dstrect.top,
			0,
			dstrect.GetWidth(),
			dstrect.GetHeight(),
			srcentry->config.layers);
		return;
	}
	else if (!framebuffer)
	{
		config.rendertarget = true;
		glGenFramebuffers(1, &framebuffer);
		FramebufferManager::SetFramebuffer(framebuffer);
		FramebufferManager::FramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_ARRAY, texture, 0);
	}
	g_renderer->ResetAPIState();
	FramebufferManager::SetFramebuffer(framebuffer);
	glActiveTexture(g_ActiveConfig.backend_info.bSupportsBindingLayout ? GL_TEXTURE9 : GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, srcentry->texture);
	g_sampler_cache->BindLinearSampler(g_ActiveConfig.backend_info.bSupportsBindingLayout ? 9 : 0);
	glViewport(dstrect.left, dstrect.top, dstrect.GetWidth(), dstrect.GetHeight());
	s_ColorCopyProgram.Bind();
	glUniform4f(s_ColorCopyPositionUniform,
		float(srcrect.left),
		float(srcrect.top),
		float(srcrect.GetWidth()),
		float(srcrect.GetHeight()));
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	FramebufferManager::SetFramebuffer(0);
	g_renderer->RestoreAPIState();
}

void TextureCache::TCacheEntry::Load(const u8* src, u32 width, u32 height,
	u32 expanded_width, u32 level)
{
	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

	u32 blocksize = (config.pcformat == PC_TEX_FMT_DXT1) ? 8u : 16u;
	switch (config.pcformat)
	{
	case PC_TEX_FMT_DXT1:
	case PC_TEX_FMT_DXT3:
	case PC_TEX_FMT_DXT5:
	{
		if (expanded_width != width)
		{
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_WIDTH, 4);
			glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_HEIGHT, 4);
			glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_DEPTH, 1);
			glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_SIZE, blocksize);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, expanded_width);
		}
		if (g_ogl_config.bSupportsTextureStorage)
		{
			glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, level, 0, 0, 0,
				width, height, 1, gl_format, ((width + 3) >> 2) * ((height + 3) >> 2) * blocksize, src);
		}
		else
		{
			glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, level, gl_format,
				width, height, 1, 0, ((width + 3) >> 2) * ((height + 3) >> 2) * blocksize, src);
		}
		if (expanded_width != width)
		{
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
			glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_WIDTH, 0);
			glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_HEIGHT, 0);
			glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_DEPTH, 0);
			glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_SIZE, 0);
		}
	}
	break;
	default:
	{
		if (expanded_width != width)
			glPixelStorei(GL_UNPACK_ROW_LENGTH, expanded_width);
		if (g_ogl_config.bSupportsTextureStorage)
		{
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, level, 0, 0, 0, width, height, 1, gl_format,
				gl_type, src);
		}
		else
		{
			glTexImage3D(GL_TEXTURE_2D_ARRAY, level, gl_iformat, width, height, 1, 0, gl_format,
				gl_type, src);
		}
		if (expanded_width != width)
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	}
	break;
	}
	TextureCache::SetStage();
}

void TextureCache::TCacheEntry::LoadMaterialMap(const u8* src, u32 width, u32 height, u32 level)
{
	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D_ARRAY, nrm_texture);

	u32 blocksize = (config.pcformat == PC_TEX_FMT_DXT1) ? 8u : 16u;
	switch (config.pcformat)
	{
	case PC_TEX_FMT_DXT1:
	case PC_TEX_FMT_DXT3:
	case PC_TEX_FMT_DXT5:
	{
		if (g_ogl_config.bSupportsTextureStorage)
		{
			glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, level, 0, 0, 0,
				width, height, 1, gl_format, ((width + 3) >> 2) * ((height + 3) >> 2) * blocksize, src);
		}
		else
		{
			glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, level, gl_format,
				width, height, 1, 0, ((width + 3) >> 2) * ((height + 3) >> 2) * blocksize, src);
		}
	}
	break;
	default:
		if (g_ogl_config.bSupportsTextureStorage)
		{
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, level, 0, 0, 0, width, height, 1, gl_format,
				gl_type, src);
		}
		else
		{
			glTexImage3D(GL_TEXTURE_2D_ARRAY, level, gl_format, width, height, 1, 0, gl_iformat,
				gl_type, src);
		}
		break;
	}
	TextureCache::SetStage();
}

void TextureCache::TCacheEntry::FromRenderTarget(bool is_depth_copy, const EFBRectangle& srcRect,
	bool scaleByHalf, unsigned int cbufid, const float *colmat, u32 width, u32 height)
{
	g_renderer->ResetAPIState(); // reset any game specific settings

	// Make sure to resolve anything we need to read from.
	const GLuint read_texture = is_depth_copy ?
		FramebufferManager::ResolveAndGetDepthTarget(srcRect) :
		FramebufferManager::ResolveAndGetRenderTarget(srcRect);

	FramebufferManager::SetFramebuffer(framebuffer);

	OpenGL_BindAttributelessVAO();
	TargetRectangle R = g_renderer->ConvertEFBRectangle(srcRect);
	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D_ARRAY, read_texture);
	if (scaleByHalf)
		g_sampler_cache->BindLinearSampler(9);
	else
		g_sampler_cache->BindNearestSampler(9);
	glViewport(0, 0, width, height);

	GLuint uniform_location;
	if (is_depth_copy)
	{
		s_DepthMatrixProgram.Bind();
		if (s_DepthCbufid != cbufid)
			glUniform4fv(s_DepthMatrixUniform, 5, colmat);
		s_DepthCbufid = cbufid;
		uniform_location = s_DepthCopyPositionUniform;
	}
	else
	{
		s_ColorMatrixProgram.Bind();
		if (s_ColorCbufid != cbufid)
			glUniform4fv(s_ColorMatrixUniform, 7, colmat);
		s_ColorCbufid = cbufid;
		uniform_location = s_ColorMatrixPositionUniform;
	}

	
	glUniform4f(uniform_location, static_cast<float>(R.left), static_cast<float>(R.top),
		static_cast<float>(R.right), static_cast<float>(R.bottom));

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	FramebufferManager::SetFramebuffer(0);

	g_renderer->RestoreAPIState();
}

void TextureCache::CopyEFB(u8* dst, const EFBCopyFormat& format, u32 native_width, u32 bytes_per_row,
	u32 num_blocks_y, u32 memory_stride, bool is_depth_copy,
	const EFBRectangle& src_rect, bool scale_by_half)
{
	TextureConverter::EncodeToRamFromTexture(dst, format, native_width, bytes_per_row, num_blocks_y,
		memory_stride, is_depth_copy, src_rect, scale_by_half);
}

bool TextureCache::Palettize(TCacheEntryBase* src_entry, const TCacheEntryBase* base_entry)
{
	TextureCache::TCacheEntry* entry = (TextureCache::TCacheEntry*)src_entry;
	u32 texformat = entry->format & 0xf;
	if (!g_ActiveConfig.backend_info.bSupportsPaletteConversion)
	{
		return false;
	}
	g_renderer->ResetAPIState();

	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D_ARRAY, ((TextureCache::TCacheEntry*)base_entry)->texture);
	g_sampler_cache->BindLinearSampler(9);

	FramebufferManager::SetFramebuffer(entry->framebuffer);
	glViewport(0, 0, entry->config.width, entry->config.height);
	s_palette_pixel_shader[s_last_TlutFormat].Bind();


	glUniform1i(s_palette_buffer_offset_uniform[s_last_TlutFormat], s_last_pallet_Buffer / 2);
	glUniform1f(s_palette_multiplier_uniform[s_last_TlutFormat], texformat == GX_TF_C4 || texformat == GX_TF_I4 ? 15.0f : 255.0f);
	glUniform4f(s_palette_copy_position_uniform[s_last_TlutFormat], 0.0f, 0.0f, (float)entry->config.width, (float)entry->config.height);

	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_BUFFER, s_palette_resolv_texture);
	g_sampler_cache->BindNearestSampler(10);
	OpenGL_BindAttributelessVAO();
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	FramebufferManager::SetFramebuffer(0);
	g_renderer->RestoreAPIState();
	return true;
}

TextureCache::TextureCache()
{
	CompileShaders();

	s_ActiveTexture = -1;
	for (auto& gtex : s_Textures)
		gtex = -1;
	if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
	{
		s32 buffer_size_mb = (g_ActiveConfig.backend_info.bSupportsGPUTextureDecoding ? 32 : 1);
		s32 buffer_size = buffer_size_mb * 1024 * 1024;
		s32 max_buffer_size = 0;

		// The minimum MAX_TEXTURE_BUFFER_SIZE that the spec mandates is 65KB, we are asking for a 1MB
		// buffer here. This buffer is also used as storage for undecoded textures when compute shader
		// texture decoding is enabled, in which case the requested size is 32MB.
		glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &max_buffer_size);
		
		// Clamp the buffer size to the maximum size that the driver supports.
		buffer_size = std::min(buffer_size, max_buffer_size);

		s_palette_stream_buffer = StreamBuffer::Create(GL_TEXTURE_BUFFER, buffer_size);
		glGenTextures(1, &s_palette_resolv_texture);
		glBindTexture(GL_TEXTURE_BUFFER, s_palette_resolv_texture);
		glTexBuffer(GL_TEXTURE_BUFFER, GL_R16UI, s_palette_stream_buffer->m_buffer);
		CreateTextureDecodingResources();
	}
}

bool TextureCache::CompileShaders()
{
	const char *pColorCopyProg =
		"SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"
		"uniform vec4 colmat[7];\n"
		"in vec3 f_uv0;\n"
		"out vec4 ocol0;\n"
		"\n"
		"void main(){\n"
		"	vec4 texcol = texture(samp9, f_uv0);\n"
		"	ocol0 = texcol;\n"
		"}\n";

	const char *pColorMatrixProg =
		"SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"
		"uniform vec4 colmat[7];\n"
		"in vec3 f_uv0;\n"
		"out vec4 ocol0;\n"
		"\n"
		"void main(){\n"
		"	vec4 texcol = texture(samp9, f_uv0);\n"
		"	texcol = floor(texcol * colmat[5]) * colmat[6];\n"
		"	ocol0 = texcol * mat4(colmat[0], colmat[1], colmat[2], colmat[3]) + colmat[4];\n"
		"}\n";

	const char *pDepthMatrixProg =
		"SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"
		"uniform vec4 colmat[5];\n"
		"in vec3 f_uv0;\n"
		"out vec4 ocol0;\n"
		"\n"
		"void main(){\n"
		"	vec4 texcol = texture(samp9, vec3(f_uv0.xy, %s));\n"

		"	int workspace = int(texcol.x * 16777216.0f);\n"
		"	texcol.z = float(workspace & 255);\n"   // z component
		"	workspace = workspace >> 8;\n"
		"	texcol.y = float(workspace & 255);\n"   // y component
		"	workspace = workspace >> 8;\n"
		"	texcol.x = float(workspace & 255);\n"   // x component
		"	texcol.w = float(workspace & 240);\n"    // w component
		"	texcol = texcol / 255.0;\n"             // normalize components to [0.0..1.0]

		"	ocol0 = texcol * mat4(colmat[0], colmat[1], colmat[2], colmat[3]) + colmat[4];\n"
		"}\n";

	const char *VProgram =
		"out vec3 %s_uv0;\n"
		"SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"
		"uniform vec4 copy_position;\n" // left, top, right, bottom
		"void main()\n"
		"{\n"
		"	vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);\n"
		"	%s_uv0 = vec3(mix(copy_position.xy, copy_position.zw, rawpos) / vec2(textureSize(samp9, 0).xy), 0.0);\n"
		"	gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);\n"
		"}\n";

	const char *GProgram = g_ActiveConfig.iStereoMode > 0 ?
		"layout(triangles) in;\n"
		"layout(triangle_strip, max_vertices = 6) out;\n"
		"in vec3 v_uv0[3];\n"
		"out vec3 f_uv0;\n"
		"SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"
		"void main()\n"
		"{\n"
		"	int layers = textureSize(samp9, 0).z;\n"
		"	for (int layer = 0; layer < layers; ++layer) {\n"
		"		for (int i = 0; i < 3; ++i) {\n"
		"			f_uv0 = vec3(v_uv0[i].xy, layer);\n"
		"			gl_Position = gl_in[i].gl_Position;\n"
		"			gl_Layer = layer;\n"
		"			EmitVertex();\n"
		"		}\n"
		"		EndPrimitive();\n"
		"	}\n"
		"}\n" : nullptr;

	const char* prefix = (GProgram == nullptr) ? "f" : "v";
	const char* depth_layer = (g_ActiveConfig.bStereoEFBMonoDepth) ? "0.0" : "f_uv0.z";
	bool compiled = true;
	compiled = compiled && ProgramShaderCache::CompileShader(s_ColorCopyProgram, StringFromFormat(VProgram, prefix, prefix).c_str(), pColorCopyProg, GProgram);
	compiled = compiled && ProgramShaderCache::CompileShader(s_ColorMatrixProgram, StringFromFormat(VProgram, prefix, prefix).c_str(), pColorMatrixProg, GProgram);
	compiled = compiled && ProgramShaderCache::CompileShader(s_DepthMatrixProgram, StringFromFormat(VProgram, prefix, prefix).c_str(), StringFromFormat(pDepthMatrixProg, depth_layer).c_str(), GProgram);

	s_ColorMatrixUniform = glGetUniformLocation(s_ColorMatrixProgram.glprogid, "colmat");
	s_DepthMatrixUniform = glGetUniformLocation(s_DepthMatrixProgram.glprogid, "colmat");
	s_ColorCbufid = -1;
	s_DepthCbufid = -1;

	s_ColorCopyPositionUniform = glGetUniformLocation(s_ColorCopyProgram.glprogid, "copy_position");
	s_ColorMatrixPositionUniform = glGetUniformLocation(s_ColorMatrixProgram.glprogid, "copy_position");
	s_DepthCopyPositionUniform = glGetUniformLocation(s_DepthMatrixProgram.glprogid, "copy_position");

	std::string palette_shader =
		R"GLSL(
		uniform int texture_buffer_offset;
		uniform float multiplier;
		SAMPLER_BINDING(9) uniform sampler2DArray samp9;
		SAMPLER_BINDING(10) uniform usamplerBuffer samp10;

		in vec3 f_uv0;
		out vec4 ocol0;

		int Convert3To8(int v)
		{
			// Swizzle bits: 00000123 -> 12312312
			return (v << 5) | (v << 2) | (v >> 1);
		}

		int Convert4To8(int v)
		{
			// Swizzle bits: 00001234 -> 12341234
			return (v << 4) | v;
		}

		int Convert5To8(int v)
		{
			// Swizzle bits: 00012345 -> 12345123
			return (v << 3) | (v >> 2);
		}

		int Convert6To8(int v)
		{
			// Swizzle bits: 00123456 -> 12345612
			return (v << 2) | (v >> 4);
		}

		float4 DecodePixel_RGB5A3(int val)
		{
			int r,g,b,a;
			if ((val&0x8000) > 0)
			{
				r=Convert5To8((val>>10) & 0x1f);
				g=Convert5To8((val>>5 ) & 0x1f);
				b=Convert5To8((val    ) & 0x1f);
				a=0xFF;
			}
			else
			{
				a=Convert3To8((val>>12) & 0x7);
				r=Convert4To8((val>>8 ) & 0xf);
				g=Convert4To8((val>>4 ) & 0xf);
				b=Convert4To8((val    ) & 0xf);
			}
			return float4(r, g, b, a) / 255.0;
		}

		float4 DecodePixel_RGB565(int val)
		{
			int r, g, b, a;
			r = Convert5To8((val >> 11) & 0x1f);
			g = Convert6To8((val >> 5) & 0x3f);
			b = Convert5To8((val) & 0x1f);
			a = 0xFF;
			return float4(r, g, b, a) / 255.0;
		}

		float4 DecodePixel_IA8(int val)
		{
			int i = val & 0xFF;
			int a = val >> 8;
			return float4(i, i, i, a) / 255.0;
		}

		void main()
		{
			int src = int(round(texture(samp9, f_uv0).r * multiplier));
			src = int(texelFetch(samp10, src + texture_buffer_offset).r);
			src = ((src << 8) & 0xFF00) | (src >> 8);
			ocol0 = DECODE(src);
		}
		)GLSL";

	if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
	{
		compiled = compiled && ProgramShaderCache::CompileShader(
			s_palette_pixel_shader[GX_TL_IA8],
			StringFromFormat(VProgram, prefix, prefix).c_str(),
			("#define DECODE DecodePixel_IA8" + palette_shader).c_str(),
			GProgram);
		s_palette_buffer_offset_uniform[GX_TL_IA8] = glGetUniformLocation(s_palette_pixel_shader[GX_TL_IA8].glprogid, "texture_buffer_offset");
		s_palette_multiplier_uniform[GX_TL_IA8] = glGetUniformLocation(s_palette_pixel_shader[GX_TL_IA8].glprogid, "multiplier");
		s_palette_copy_position_uniform[GX_TL_IA8] = glGetUniformLocation(s_palette_pixel_shader[GX_TL_IA8].glprogid, "copy_position");

		compiled = compiled && ProgramShaderCache::CompileShader(
			s_palette_pixel_shader[GX_TL_RGB565],
			StringFromFormat(VProgram, prefix, prefix).c_str(),
			("#define DECODE DecodePixel_RGB565" + palette_shader).c_str(),
			GProgram);
		s_palette_buffer_offset_uniform[GX_TL_RGB565] = glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB565].glprogid, "texture_buffer_offset");
		s_palette_multiplier_uniform[GX_TL_RGB565] = glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB565].glprogid, "multiplier");
		s_palette_copy_position_uniform[GX_TL_RGB565] = glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB565].glprogid, "copy_position");

		compiled = compiled && ProgramShaderCache::CompileShader(
			s_palette_pixel_shader[GX_TL_RGB5A3],
			StringFromFormat(VProgram, prefix, prefix).c_str(),
			("#define DECODE DecodePixel_RGB5A3" + palette_shader).c_str(),
			GProgram);
		s_palette_buffer_offset_uniform[GX_TL_RGB5A3] = glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB5A3].glprogid, "texture_buffer_offset");
		s_palette_multiplier_uniform[GX_TL_RGB5A3] = glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB5A3].glprogid, "multiplier");
		s_palette_copy_position_uniform[GX_TL_RGB5A3] = glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB5A3].glprogid, "copy_position");
	}
	return compiled;
}

void TextureCache::DeleteShaders()
{
	s_ColorCopyProgram.Destroy();
	s_ColorMatrixProgram.Destroy();
	s_DepthMatrixProgram.Destroy();

	if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
		for (auto& shader : s_palette_pixel_shader)
			shader.Destroy();
}

TextureCache::~TextureCache()
{
	DeleteShaders();
	DestroyTextureDecodingResources();
	if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
	{
		s_palette_stream_buffer.reset();
		glDeleteTextures(1, &s_palette_resolv_texture);
	}
}

void TextureCache::LoadLut(u32 lutFmt, void* addr, u32 size)
{
	if (lutFmt == m_last_lutFmt && addr == m_last_addr && size == m_last_size && m_last_hash)
	{
		u64 hash = GetHash64(reinterpret_cast<u8*>(addr), size, g_ActiveConfig.iSafeTextureCache_ColorSamples);
		if (hash == m_last_hash)
		{
			return;
		}
		m_last_hash = hash;
	}
	else
	{
		m_last_hash = GetHash64(reinterpret_cast<u8*>(addr), size, g_ActiveConfig.iSafeTextureCache_ColorSamples);
	}
	m_last_lutFmt = lutFmt;
	m_last_addr = addr;
	m_last_size = size;
	if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
	{
		s_last_TlutFormat = (TlutFormat)lutFmt;
		s_last_pallet_Buffer = s_palette_stream_buffer->Stream(size, addr);
	}
}

void TextureCache::DisableStage(u32 stage)
{}

void TextureCache::SetStage()
{
	// -1 is the initial value as we don't know which testure should be bound
	if (s_ActiveTexture != (u32)-1)
		glActiveTexture(GL_TEXTURE0 + s_ActiveTexture);
}

static const std::string decoding_vertex_shader = R"(
void main()
{
  vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);
  gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);
}
)";

void CreateTextureDecodingResources()
{
	static const GLenum gl_view_types[TextureConversionShader::BUFFER_FORMAT_COUNT] = {
		GL_R8UI,    // BUFFER_FORMAT_R8_UINT
		GL_R16UI,   // BUFFER_FORMAT_R16_UINT
		GL_RG32UI,  // BUFFER_FORMAT_R32G32_UINT
	};

	glGenTextures(TextureConversionShader::BUFFER_FORMAT_COUNT,
		s_texture_decoding_buffer_views.data());
	for (size_t i = 0; i < TextureConversionShader::BUFFER_FORMAT_COUNT; i++)
	{
		glBindTexture(GL_TEXTURE_BUFFER, s_texture_decoding_buffer_views[i]);
		glTexBuffer(GL_TEXTURE_BUFFER, gl_view_types[i], s_palette_stream_buffer->m_buffer);
	}
}

void DestroyTextureDecodingResources()
{
	glDeleteTextures(TextureConversionShader::BUFFER_FORMAT_COUNT,
		s_texture_decoding_buffer_views.data());
	s_texture_decoding_buffer_views.fill(0);
	s_texture_decoding_program_info.clear();
}

bool TextureCache::SupportsGPUTextureDecode(TextureFormat format, TlutFormat palette_format)
{
	auto key = std::make_pair(static_cast<u32>(format), static_cast<u32>(palette_format));
	auto iter = s_texture_decoding_program_info.find(key);
	if (iter != s_texture_decoding_program_info.end())
		return iter->second.valid;

	TextureDecodingProgramInfo info;
	info.base_info = TextureConversionShader::GetDecodingShaderInfo(format);
	if (!info.base_info)
	{
		s_texture_decoding_program_info.emplace(key, info);
		return false;
	}

	std::string shader_source =
		TextureConversionShader::GenerateDecodingShader(format, palette_format, API_OPENGL);
	if (shader_source.empty())
	{
		s_texture_decoding_program_info.emplace(key, info);
		return false;
	}

	if (!ProgramShaderCache::CompileComputeShader(info.program, shader_source))
	{
		s_texture_decoding_program_info.emplace(key, info);
		return false;
	}

	info.uniform_dst_size = glGetUniformLocation(info.program.glprogid, "u_dst_size");
	info.uniform_src_size = glGetUniformLocation(info.program.glprogid, "u_src_size");
	info.uniform_src_offset = glGetUniformLocation(info.program.glprogid, "u_src_offset");
	info.uniform_src_row_stride = glGetUniformLocation(info.program.glprogid, "u_src_row_stride");
	info.uniform_palette_offset = glGetUniformLocation(info.program.glprogid, "u_palette_offset");
	info.valid = true;
	s_texture_decoding_program_info.emplace(key, info);
	return true;
}

bool TextureCache::TCacheEntry::DecodeTextureOnGPU(u32 dst_level, const u8* data,
	u32 data_size, TextureFormat format, u32 width, u32 height,
	u32 aligned_width, u32 aligned_height, u32 row_stride,
	const u8* palette, TlutFormat palette_format)
{
	auto key = std::make_pair(static_cast<u32>(format), static_cast<u32>(palette_format));
	auto iter = s_texture_decoding_program_info.find(key);
	if (iter == s_texture_decoding_program_info.end())
		return false;

#ifdef TIME_TEXTURE_DECODING
	GPUTimer timer;
#endif

	// Copy to GPU-visible buffer, aligned to the data type.
	auto info = iter->second;
	u32 bytes_per_buffer_elem =
		TextureConversionShader::GetBytesPerBufferElement(info.base_info->buffer_format);

	// Only copy palette if it is required.
	bool has_palette = info.base_info->palette_size > 0;
	u32 total_upload_size = static_cast<u32>(data_size);
	u32 palette_offset = total_upload_size;
	if (has_palette)
	{
		// Align to u16.
		if ((total_upload_size % sizeof(u16)) != 0)
		{
			total_upload_size++;
			palette_offset++;
		}

		total_upload_size += info.base_info->palette_size;
	}

	// Allocate space in stream buffer, and copy texture + palette across.
	u32 offset = s_palette_stream_buffer->Stream(data_size, data);
	if (has_palette)
		s_palette_stream_buffer->Stream(info.base_info->palette_size, sizeof(u16), palette);
	info.program.Bind();

	// Calculate stride in buffer elements
	u32 row_stride_in_elements = row_stride / bytes_per_buffer_elem;
	u32 offset_in_elements = offset / bytes_per_buffer_elem;
	u32 palette_offset_in_elements = (offset + palette_offset) / sizeof(u16);
	if (info.uniform_dst_size >= 0)
		glUniform2ui(info.uniform_dst_size, width, height);
	if (info.uniform_src_size >= 0)
		glUniform2ui(info.uniform_src_size, aligned_width, aligned_height);
	if (info.uniform_src_offset >= 0)
		glUniform1ui(info.uniform_src_offset, offset_in_elements);
	if (info.uniform_src_row_stride >= 0)
		glUniform1ui(info.uniform_src_row_stride, row_stride_in_elements);
	if (info.uniform_palette_offset >= 0)
		glUniform1ui(info.uniform_palette_offset, palette_offset_in_elements);

	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_BUFFER, s_texture_decoding_buffer_views[info.base_info->buffer_format]);

	if (has_palette)
	{
		// Use an R16UI view for the palette.
		glActiveTexture(GL_TEXTURE10);
		glBindTexture(GL_TEXTURE_BUFFER, s_palette_resolv_texture);
	}

	auto dispatch_groups =
		TextureConversionShader::GetDispatchCount(info.base_info, aligned_width, aligned_height);
	glBindImageTexture(0, texture, dst_level, GL_TRUE, 0,
		GL_WRITE_ONLY, GL_RGBA8);
	glDispatchCompute(dispatch_groups.first, dispatch_groups.second, 1);
	glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);

	TextureCache::SetStage();

#ifdef TIME_TEXTURE_DECODING
	WARN_LOG(VIDEO, "Decode texture format %u size %ux%u took %.4fms", static_cast<u32>(format),
		width, height, timer.GetTimeMilliseconds());
#endif
	return true;
}

}
