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

#include "VideoBackends/OGL/Depalettizer.h"
#include "VideoBackends/OGL/FramebufferManager.h"
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
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/TextureScalerCommon.h"
#include "VideoCommon/VideoConfig.h"

#define 	GL_COMPRESSED_RGB_S3TC_DXT1_EXT   0x83F0
#define 	GL_COMPRESSED_RGBA_S3TC_DXT1_EXT   0x83F1
#define 	GL_COMPRESSED_RGBA_S3TC_DXT3_EXT   0x83F2
#define 	GL_COMPRESSED_RGBA_S3TC_DXT5_EXT   0x83F3

#define GL_UNPACK_COMPRESSED_BLOCK_WIDTH  0x9127
#define GL_UNPACK_COMPRESSED_BLOCK_HEIGHT 0x9128
#define GL_UNPACK_COMPRESSED_BLOCK_DEPTH  0x9129
#define GL_UNPACK_COMPRESSED_BLOCK_SIZE 0x912A

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
static std::unique_ptr<Depalettizer> s_depaletizer;
static std::unique_ptr<TextureScaler> s_scaler;
static std::pair<u8*, u32> s_last_pallet_Buffer;
static TlutFormat s_last_TlutFormat = TlutFormat::GX_TL_IA8;
bool SaveTexture(const std::string& filename, u32 textarget, u32 tex, int virtual_width, int virtual_height, u32 level, bool compressed = false)
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

void TextureCache::TCacheEntry::Bind(u32 stage, u32 last_texture)
{
	if (s_Textures[stage] != texture)
	{
		if (s_ActiveTexture != stage)
		{
			glActiveTexture(GL_TEXTURE0 + stage);
			s_ActiveTexture = stage;
		}

		glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
		s_Textures[stage] = texture;
	}
	if (nrm_texture && g_ActiveConfig.HiresMaterialMapsEnabled())
	{
		glActiveTexture(GL_TEXTURE8 + stage);
		s_ActiveTexture = 8 + stage;
		glBindTexture(GL_TEXTURE_2D_ARRAY, nrm_texture);
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
	switch (config.pcformat)
	{
	default:
	case PC_TEX_FMT_NONE:
		PanicAlert("Invalid PC texture format %i", config.pcformat);
	case PC_TEX_FMT_BGRA32:
		gl_format = GL_BGRA;
		gl_iformat = GL_RGBA;
		gl_type = GL_UNSIGNED_BYTE;
		break;

	case PC_TEX_FMT_RGBA32:
		gl_format = GL_RGBA;
		gl_iformat = GL_RGBA;
		gl_type = GL_UNSIGNED_BYTE;
		break;
	case PC_TEX_FMT_I4_AS_I8:
		gl_format = GL_LUMINANCE;
		gl_iformat = GL_INTENSITY4;
		gl_type = GL_UNSIGNED_BYTE;
		break;

	case PC_TEX_FMT_IA4_AS_IA8:
		gl_format = GL_LUMINANCE_ALPHA;
		gl_iformat = GL_LUMINANCE4_ALPHA4;
		gl_type = GL_UNSIGNED_BYTE;
		break;

	case PC_TEX_FMT_I8:
		gl_format = GL_LUMINANCE;
		gl_iformat = GL_INTENSITY8;
		gl_type = GL_UNSIGNED_BYTE;
		break;

	case PC_TEX_FMT_IA8:
		gl_format = GL_LUMINANCE_ALPHA;
		gl_iformat = GL_LUMINANCE8_ALPHA8;
		gl_type = GL_UNSIGNED_BYTE;
		break;
	case PC_TEX_FMT_RGB565:
		gl_format = GL_RGB;
		gl_iformat = GL_RGB;
		gl_type = GL_UNSIGNED_SHORT_5_6_5;
		break;
	case PC_TEX_FMT_DXT1:
		gl_format = 0;
		gl_iformat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		gl_type = 0;
		compressed = true;
		break;
	case PC_TEX_FMT_DXT3:
		gl_format = 0;
		gl_iformat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		gl_type = 0;
		compressed = true;
		break;
	case PC_TEX_FMT_DXT5:
		gl_format = 0;
		gl_iformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		gl_type = 0;
		compressed = true;
		break;
	}
}

TextureCache::TCacheEntryBase* TextureCache::CreateTexture(const TCacheEntryConfig& config)
{
	TCacheEntry* entry = new TCacheEntry(config);	
	
	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D_ARRAY, entry->texture);	
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, config.levels - 1);

	if (config.rendertarget)
	{
		entry->gl_format = GL_RGBA;
		entry->gl_iformat = GL_RGBA;
		entry->gl_type = GL_UNSIGNED_BYTE;

		for (u32 level = 0; level <= config.levels; level++)
		{
			glTexImage3D(GL_TEXTURE_2D_ARRAY, level, GL_RGBA, config.width, config.height, config.layers, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		}		
		glGenFramebuffers(1, &entry->framebuffer);
		FramebufferManager::SetFramebuffer(entry->framebuffer);
		FramebufferManager::FramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_ARRAY, entry->texture, 0);
	}
	else
	{
		if (config.materialmap)
		{
			glGenTextures(1, &entry->nrm_texture);
			glActiveTexture(GL_TEXTURE9);
			glBindTexture(GL_TEXTURE_2D_ARRAY, entry->nrm_texture);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, config.levels - 1);
		}
		entry->SetFormat();
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
	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D_ARRAY, srcentry->texture);
	g_sampler_cache->BindLinearSampler(9);
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
		glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, level, gl_iformat,
			width, height, 1, 0, ((width + 3) >> 2) * ((height + 3) >> 2) * blocksize, src);
		if (expanded_width != width)
		{
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 0);
			glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_WIDTH, 0);
			glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_HEIGHT, 0);
			glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_DEPTH, 0);
			glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_SIZE, 0);
		}
	}
	break;
	default:
		if (expanded_width != width)
			glPixelStorei(GL_UNPACK_ROW_LENGTH, expanded_width);
		glTexImage3D(GL_TEXTURE_2D_ARRAY, level, gl_iformat, width, height, 1, 0, gl_format, gl_type, src);
		if (expanded_width != width)
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		break;
	}
	TextureCache::SetStage();
}

void TextureCache::TCacheEntry::LoadMaterialMap(const u8* src, u32 width, u32 height,u32 level)
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
		glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, level, gl_iformat,
			width, height, 1, 0, ((width + 3) >> 2) * ((height + 3) >> 2) * blocksize, src);
	}
	break;
	default:
		glTexImage3D(GL_TEXTURE_2D_ARRAY, level, gl_iformat, width, height, 1, 0, gl_format, gl_type, src);
		break;
	}
	TextureCache::SetStage();
}

void TextureCache::TCacheEntry::Load(const u8* src, u32 width, u32 height, u32 expandedWidth,
	u32 expandedHeight, const s32 texformat, const u32 tlutaddr, const TlutFormat tlutfmt, u32 level)
{
	TexDecoder_Decode(TextureCache::temp, src, expandedWidth, expandedHeight, texformat, tlutaddr, tlutfmt, PC_TEX_FMT_RGBA32 == config.pcformat, compressed);
	u8* data = TextureCache::temp;
	if (is_scaled)
	{
		data = (u8*)s_scaler->Scale((u32*)data, expandedWidth, height);
		width *= g_ActiveConfig.iTexScalingFactor;
		height *= g_ActiveConfig.iTexScalingFactor;
		expandedWidth *= g_ActiveConfig.iTexScalingFactor;
	}	
	Load(data, width, height, expandedWidth, level);
}
void TextureCache::TCacheEntry::LoadFromTmem(const u8* ar_src, const u8* gb_src, u32 width, u32 height,
	u32 expanded_width, u32 expanded_Height, u32 level)
{
	TexDecoder_DecodeRGBA8FromTmem((u32*)TextureCache::temp, ar_src, gb_src, expanded_width, expanded_Height);
	u8* data = TextureCache::temp;
	if (is_scaled)
	{
		data = (u8*)s_scaler->Scale((u32*)data, expanded_width, height);
		width *= g_ActiveConfig.iTexScalingFactor;
		height *= g_ActiveConfig.iTexScalingFactor;
		expanded_width *= g_ActiveConfig.iTexScalingFactor;
	}
	Load(data, width, height, expanded_width, level);
}

void TextureCache::TCacheEntry::FromRenderTarget(u8* dst, PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
	bool scaleByHalf, unsigned int cbufid, const float *colmat)
{
	g_renderer->ResetAPIState(); // reset any game specific settings

	// Make sure to resolve anything we need to read from.
	const GLuint read_texture = (srcFormat == PEControl::Z24) ?
		FramebufferManager::ResolveAndGetDepthTarget(srcRect) :
		FramebufferManager::ResolveAndGetRenderTarget(srcRect);

	FramebufferManager::SetFramebuffer(framebuffer);

	OpenGL_BindAttributelessVAO();

	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D_ARRAY, read_texture);
	if (scaleByHalf)
		g_sampler_cache->BindLinearSampler(9);
	else
		g_sampler_cache->BindNearestSampler(9);
	glViewport(0, 0, config.width, config.height);

	GLuint uniform_location;
	if (srcFormat == PEControl::Z24)
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

	TargetRectangle R = g_renderer->ConvertEFBRectangle(srcRect);
	glUniform4f(uniform_location, static_cast<float>(R.left), static_cast<float>(R.top),
		static_cast<float>(R.right), static_cast<float>(R.bottom));

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	FramebufferManager::SetFramebuffer(0);

	g_renderer->RestoreAPIState();
}

void TextureCache::CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
	PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
	bool isIntensity, bool scaleByHalf)
{
	TextureConverter::EncodeToRamFromTexture(
		dst,
		format,
		native_width,
		bytes_per_row,
		num_blocks_y,
		memory_stride,
		srcFormat,
		isIntensity,
		scaleByHalf,
		srcRect);
}

bool TextureCache::Palettize(TCacheEntryBase* src_entry,const TCacheEntryBase* base_entry)
{
	TextureCache::TCacheEntry* entry = (TextureCache::TCacheEntry*)src_entry;
	u32 texformat = entry->format & 0xf;
	if (!g_ActiveConfig.backend_info.bSupportsPaletteConversion)
	{
		
		Depalettizer::BaseType baseType = Depalettizer::Unorm8;
		if (texformat == GX_TF_C4)
			baseType = Depalettizer::Unorm4;
		else if (texformat == GX_TF_C8)
			baseType = Depalettizer::Unorm8;
		else
			return false;
		return s_depaletizer->Depalettize(baseType, entry->texture, ((TextureCache::TCacheEntry*)base_entry)->texture, entry->config.width, entry->config.height);
	}
	g_renderer->ResetAPIState();

	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D_ARRAY, ((TextureCache::TCacheEntry*)base_entry)->texture);
	g_sampler_cache->BindLinearSampler(9);
	
	FramebufferManager::SetFramebuffer(entry->framebuffer);
	glViewport(0, 0, entry->config.width, entry->config.height);
	s_palette_pixel_shader[s_last_TlutFormat].Bind();

	
	glUniform1i(s_palette_buffer_offset_uniform[s_last_TlutFormat], s_last_pallet_Buffer.second / 2);
	glUniform1f(s_palette_multiplier_uniform[s_last_TlutFormat], texformat == GX_TF_C4 ? 15.0f : 255.0f);
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
		s32 buffer_size = 1024 * 1024;
		s32 max_buffer_size = 0;

		// The minimum MAX_TEXTURE_BUFFER_SIZE that the spec mandates
		// is 65KB, we are asking for a 1MB buffer here.
		// Make sure to check the maximum size and if it is below 1MB
		// then use the maximum the hardware supports instead.
		glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &max_buffer_size);
		buffer_size = std::min(buffer_size, max_buffer_size);

		s_palette_stream_buffer = StreamBuffer::Create(GL_TEXTURE_BUFFER, buffer_size);
		glGenTextures(1, &s_palette_resolv_texture);
		glBindTexture(GL_TEXTURE_BUFFER, s_palette_resolv_texture);
		glTexBuffer(GL_TEXTURE_BUFFER, GL_R16UI, s_palette_stream_buffer->m_buffer);
	}
	else
	{
		s_depaletizer = std::make_unique<Depalettizer>();
	}
	s_scaler = std::make_unique<TextureScaler>();
}

void TextureCache::CompileShaders()
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
		"	texcol = round(texcol * colmat[5]) * colmat[6];\n"
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
	
	ProgramShaderCache::CompileShader(s_ColorCopyProgram, StringFromFormat(VProgram, prefix, prefix).c_str(), pColorCopyProg, GProgram);
	ProgramShaderCache::CompileShader(s_ColorMatrixProgram, StringFromFormat(VProgram, prefix, prefix).c_str(), pColorMatrixProg, GProgram);
	ProgramShaderCache::CompileShader(s_DepthMatrixProgram, StringFromFormat(VProgram, prefix, prefix).c_str(), StringFromFormat(pDepthMatrixProg, depth_layer).c_str(), GProgram);

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
		ProgramShaderCache::CompileShader(
			s_palette_pixel_shader[GX_TL_IA8],
			StringFromFormat(VProgram, prefix, prefix).c_str(),
			("#define DECODE DecodePixel_IA8" + palette_shader).c_str(),
			GProgram);
		s_palette_buffer_offset_uniform[GX_TL_IA8] = glGetUniformLocation(s_palette_pixel_shader[GX_TL_IA8].glprogid, "texture_buffer_offset");
		s_palette_multiplier_uniform[GX_TL_IA8] = glGetUniformLocation(s_palette_pixel_shader[GX_TL_IA8].glprogid, "multiplier");
		s_palette_copy_position_uniform[GX_TL_IA8] = glGetUniformLocation(s_palette_pixel_shader[GX_TL_IA8].glprogid, "copy_position");

		ProgramShaderCache::CompileShader(
			s_palette_pixel_shader[GX_TL_RGB565],
			StringFromFormat(VProgram, prefix, prefix).c_str(),
			("#define DECODE DecodePixel_RGB565" + palette_shader).c_str(),
			GProgram);
		s_palette_buffer_offset_uniform[GX_TL_RGB565] = glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB565].glprogid, "texture_buffer_offset");
		s_palette_multiplier_uniform[GX_TL_RGB565] = glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB565].glprogid, "multiplier");
		s_palette_copy_position_uniform[GX_TL_RGB565] = glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB565].glprogid, "copy_position");

		ProgramShaderCache::CompileShader(
			s_palette_pixel_shader[GX_TL_RGB5A3],
			StringFromFormat(VProgram, prefix, prefix).c_str(),
			("#define DECODE DecodePixel_RGB5A3" + palette_shader).c_str(),
			GProgram);
		s_palette_buffer_offset_uniform[GX_TL_RGB5A3] = glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB5A3].glprogid, "texture_buffer_offset");
		s_palette_multiplier_uniform[GX_TL_RGB5A3] = glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB5A3].glprogid, "multiplier");
		s_palette_copy_position_uniform[GX_TL_RGB5A3] = glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB5A3].glprogid, "copy_position");
	}
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
	if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
	{
		s_palette_stream_buffer.reset();
		glDeleteTextures(1, &s_palette_resolv_texture);
	}
	else
	{
		s_depaletizer.reset();
	}
	s_scaler.reset();
}

void TextureCache::LoadLut(u32 lutFmt, void* addr, u32 size)
{
	if (lutFmt == m_last_lutFmt && addr == m_last_addr && size == m_last_size)
	{
		u64 hash = GetHash64(reinterpret_cast<u8*>(addr), size, g_ActiveConfig.iSafeTextureCache_ColorSamples);
		if (hash == m_last_hash)
		{
			return;
		}
		m_last_hash = hash;
	}
	m_last_lutFmt = lutFmt;
	m_last_addr = addr;
	m_last_size = size;
	if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
	{
		s_last_TlutFormat = (TlutFormat)lutFmt;
		s_last_pallet_Buffer = s_palette_stream_buffer->Map(size);
		memcpy(s_last_pallet_Buffer.first, addr, size);
		s_palette_stream_buffer->Unmap(size);
	}
	else
	{
		s_depaletizer->UploadPalette(lutFmt, addr, size);
	}
}

void TextureCache::DisableStage(u32 stage)
{
}

void TextureCache::SetStage()
{
	// -1 is the initial value as we don't know which testure should be bound
	if (s_ActiveTexture != (u32)-1)
		glActiveTexture(GL_TEXTURE0 + s_ActiveTexture);
}

}
