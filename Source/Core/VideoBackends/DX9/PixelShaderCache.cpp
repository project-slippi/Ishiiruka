// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Common/Hash.h"
#include "Common/FileUtil.h"
#include "Common/LinearDiskCache.h"

#include "Core/ConfigManager.h"
#include "Core/Host.h"

#include "VideoBackends/DX9/D3DBase.h"
#include "VideoBackends/DX9/D3DShader.h"
#include "VideoBackends/DX9/PixelShaderCache.h"

#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/XFMemory.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/HLSLCompiler.h"

namespace DX9
{


const PixelShaderCache::PSCacheEntry *PixelShaderCache::s_last_entry[PSRM_DEPTH_ONLY + 1];
PixelShaderUid PixelShaderCache::s_last_uid[PSRM_DEPTH_ONLY + 1];
PixelShaderUid PixelShaderCache::s_external_last_uid[PSRM_DEPTH_ONLY + 1];

static HLSLAsyncCompiler *s_compiler;
static Common::SpinLock<true> s_pixel_shaders_lock;
static LinearDiskCache<PixelShaderUid, u8> g_ps_disk_cache;
static std::set<u32> s_unique_shaders;
ObjectUsageProfiler<PixelShaderUid, pKey_t, PixelShaderCache::PSCacheEntry, PixelShaderUid::ShaderUidHasher>* PixelShaderCache::s_pshaders = nullptr;

#define MAX_SSAA_SHADERS 3
enum
{
	COPY_TYPE_DIRECT,
	COPY_TYPE_MATRIXCOLOR,
	NUM_COPY_TYPES
};
enum
{
	DEPTH_CONVERSION_TYPE_NONE,
	DEPTH_CONVERSION_TYPE_ON,
	NUM_DEPTH_CONVERSION_TYPES
};

static LPDIRECT3DPIXELSHADER9 s_copy_program[NUM_COPY_TYPES][NUM_DEPTH_CONVERSION_TYPES][MAX_SSAA_SHADERS];
static LPDIRECT3DPIXELSHADER9 s_clear_program = NULL;
static LPDIRECT3DPIXELSHADER9 s_rgba6_to_rgb8 = NULL;
static LPDIRECT3DPIXELSHADER9 s_rgb8_to_rgba6 = NULL;

class PixelShaderCacheInserter : public LinearDiskCacheReader<PixelShaderUid, u8>
{
public:
	void Read(const PixelShaderUid &key, const u8 *value, u32 value_size)
	{
		PixelShaderCache::InsertByteCode(key, value, value_size);
	}
};

LPDIRECT3DPIXELSHADER9 PixelShaderCache::GetColorMatrixProgram(int SSAAMode)
{
	return s_copy_program[COPY_TYPE_MATRIXCOLOR][DEPTH_CONVERSION_TYPE_NONE][SSAAMode % MAX_SSAA_SHADERS];
}

LPDIRECT3DPIXELSHADER9 PixelShaderCache::GetDepthMatrixProgram(int SSAAMode, bool depthConversion)
{
	return s_copy_program[COPY_TYPE_MATRIXCOLOR][depthConversion ? DEPTH_CONVERSION_TYPE_ON : DEPTH_CONVERSION_TYPE_NONE][SSAAMode % MAX_SSAA_SHADERS];
}

LPDIRECT3DPIXELSHADER9 PixelShaderCache::GetColorCopyProgram(int SSAAMode)
{
	return s_copy_program[COPY_TYPE_DIRECT][DEPTH_CONVERSION_TYPE_NONE][SSAAMode % MAX_SSAA_SHADERS];
}

LPDIRECT3DPIXELSHADER9 PixelShaderCache::GetClearProgram()
{
	return s_clear_program;
}

static LPDIRECT3DPIXELSHADER9 s_rgb8 = NULL;
static LPDIRECT3DPIXELSHADER9 s_rgba6 = NULL;

LPDIRECT3DPIXELSHADER9 PixelShaderCache::ReinterpRGBA6ToRGB8()
{
	const char code[] =
	{
		"uniform sampler samp0 : register(s0);\n"
		"void main(\n"
		"			out float4 ocol0 : COLOR0,\n"
		"			in float2 uv0 : TEXCOORD0){\n"
		"	ocol0 = tex2D(samp0,uv0);\n"
		"	float4 src6 = round(ocol0 * 63.f);\n"
		"	ocol0.r = floor(src6.r*4.f) + floor(src6.g/16.f);\n" // dst8r = (src6r<<2)|(src6g>>4);
		"	ocol0.g = frac(src6.g/16.f)*16.f*16.f + floor(src6.b/4.f);\n" // dst8g = ((src6g&0xF)<<4)|(src6b>>2);
		"	ocol0.b = frac(src6.b/4.f)*4.f*64.f + src6.a;\n" // dst8b = ((src6b&0x3)<<6)|src6a;
		"	ocol0.a = 255.f;\n"
		"	ocol0 /= 255.f;\n"
		"}\n"
	};

	if (!s_rgba6_to_rgb8)
		s_rgba6_to_rgb8 = D3D::CompileAndCreatePixelShader(code, (int)strlen(code));

	return s_rgba6_to_rgb8;
}

LPDIRECT3DPIXELSHADER9 PixelShaderCache::ReinterpRGB8ToRGBA6()
{
	/* old code here for reference
	const char code[] =
	{
	"uniform sampler samp0 : register(s0);\n"
	"void main(\n"
	"			out float4 ocol0 : COLOR0,\n"
	"			in float2 uv0 : TEXCOORD0){\n"
	"	ocol0 = tex2D(samp0,uv0);\n"
	"	float4 src8 = round(ocol0*255.f);\n"
	"	ocol0.r = floor(src8.r/4.f);\n" // dst6r = src8r>>2;
	"	ocol0.g = frac(src8.r/4.f)*4.f*16.f + floor(src8.g/16.f);\n" // dst6g = ((src8r&0x3)<<4)|(src8g>>4);
	"	ocol0.b = frac(src8.g/16.f)*16.f*4.f + floor(src8.b/64.f);\n" // dst6b = ((src8g&0xF)<<2)|(src8b>>6);
	"	ocol0.a = frac(src8.b/64.f)*64.f;\n" // dst6a = src8b&0x3F;
	"	ocol0 /= 63.f;\n"
	"}\n"
	};
	*/
	const char code[] =
	{
		"uniform sampler samp0 : register(s0);\n"
		"void main(\n"
		"out float4 ocol0 : COLOR0,\n"
		"in float2 uv0 : TEXCOORD0){\n"
		"float4 temp1 = float4(1.0f/4.0f,1.0f/16.0f,1.0f/64.0f,0.0f);\n"
		"float4 temp2 = float4(1.0f,64.0f,255.0f,1.0f/63.0f);\n"
		"float4 src8 = round(tex2D(samp0,uv0)*temp2.z) * temp1;\n"
		"ocol0 = (frac(src8.wxyz) * temp2.xyyy + floor(src8)) * temp2.w;\n"
		"}\n"
	};
	if (!s_rgb8_to_rgba6) s_rgb8_to_rgba6 = D3D::CompileAndCreatePixelShader(code, (int)strlen(code));
	return s_rgb8_to_rgba6;
}

#define WRITE p+=sprintf

static LPDIRECT3DPIXELSHADER9 CreateCopyShader(int copyMatrixType, int depthConversionType, int SSAAMode)
{
	//Used for Copy/resolve the color buffer
	//Color conversion Programs
	//Depth copy programs
	// this should create the same shaders as before (plus some extras added for DF16), just... more manageably than listing the full program for each combination
	char text[3072];

	text[sizeof(text) - 1] = 0x7C;  // canary

	char* p = text;
	WRITE(p, "// Copy/Color Matrix/Depth Matrix shader (matrix=%d, depth=%d, ssaa=%d)\n", copyMatrixType, depthConversionType, SSAAMode);

	WRITE(p, "uniform sampler samp0 : register(s0);\n");
	if (copyMatrixType == COPY_TYPE_MATRIXCOLOR)
		WRITE(p, "uniform float4 cColMatrix[7] : register(c%d);\n", C_COLORMATRIX);
	WRITE(p, "void main(\n"
		"out float4 ocol0 : COLOR0,\n");

	switch (SSAAMode % MAX_SSAA_SHADERS)
	{
	case 0: // 1 Sample
		WRITE(p, "in float2 uv0 : TEXCOORD0,\n"
			"in float uv1 : TEXCOORD1){\n"
			"float4 texcol = tex2D(samp0,uv0.xy);\n");
		break;
	case 1: // 4 Samples in 4x SSAA buffer		
	case 2: // 4 Samples in 9x SSAA buffer
		WRITE(p, "in float4 uv0 : TEXCOORD0,\n"
			"in float uv1 : TEXCOORD1,\n"
			"in float4 uv2 : TEXCOORD2,\n"
			"in float4 uv3 : TEXCOORD3){\n"
			"float4 texcol = (tex2D(samp0,uv0.xy) + tex2D(samp0,uv2.xy) + tex2D(samp0,uv2.wz) + tex2D(samp0,uv3.xy) + tex2D(samp0,uv3.wz))*0.2f;\n");
		break;
	}

	if (depthConversionType != DEPTH_CONVERSION_TYPE_NONE)
	{
		// Watch out for the fire fumes effect in Metroid it's really sensitive to this,
		// the lighting in RE0 is also way beyond sensitive since the "good value" is hardcoded and Dolphin is almost always off.
		WRITE(p, // 255.99998474121 = 16777215/16777216*256
			"	float workspace = (1.0 - texcol.x) * 255.99998474121;\n"

			"	texcol.x = floor(workspace);\n"         // x component

			"	workspace = workspace - texcol.x;\n"    // subtract x component out
			"	workspace = workspace * 256.0;\n"       // shift left 8 bits
			"	texcol.y = floor(workspace);\n"         // y component

			"	workspace = workspace - texcol.y;\n"    // subtract y component out
			"	workspace = workspace * 256.0;\n"       // shift left 8 bits
			"	texcol.z = floor(workspace);\n"         // z component

			"	texcol.w = texcol.x;\n"                 // duplicate x into w

			"	texcol = texcol / 255.0;\n"             // normalize components to [0.0..1.0]

			"	texcol.w = texcol.w * 15.0;\n"
			"	texcol.w = floor(texcol.w);\n"
			"	texcol.w = texcol.w / 15.0;\n"          // w component
		);
	}
	else
	{
		//Apply Gamma Correction
		WRITE(p, "texcol = pow(texcol,uv1.xxxx);\n");
	}

	if (copyMatrixType == COPY_TYPE_MATRIXCOLOR)
	{
		if (depthConversionType == DEPTH_CONVERSION_TYPE_NONE)
			WRITE(p, "texcol = floor(texcol * cColMatrix[5])*cColMatrix[6];\n");

		WRITE(p, "ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n");
	}
	else
		WRITE(p, "ocol0 = texcol;\n");

	WRITE(p, "}\n");
	if (text[sizeof(text) - 1] != 0x7C)
		PanicAlert("PixelShaderCache copy shader generator - buffer too small, canary has been eaten!");

	return D3D::CompileAndCreatePixelShader(text, (int)strlen(text));
}

void PixelShaderCache::Init()
{
	s_pixel_shaders_lock.unlock();
	for (u32 i = 0; i < PSRM_DEPTH_ONLY + 1; i++)
	{
		s_last_entry[i] = nullptr;
	}
	s_compiler = &HLSLAsyncCompiler::getInstance();

	//program used for clear screen
	{
		char pprog[3072];
		sprintf(pprog, "void main(\n"
			"out float4 ocol0 : COLOR0,\n"
			" in float4 incol0 : COLOR0){\n"
			"ocol0 = incol0;\n"
			"}\n");
		s_clear_program = D3D::CompileAndCreatePixelShader(pprog, (int)strlen(pprog));
	}

	int shaderModel = ((D3D::GetCaps().PixelShaderVersion >> 8) & 0xFF);
	int maxConstants = (shaderModel < 3) ? 32 : ((shaderModel < 4) ? 224 : 65536);

	// other screen copy/convert programs
	for (int copyMatrixType = 0; copyMatrixType < NUM_COPY_TYPES; copyMatrixType++)
	{
		for (int depthType = 0; depthType < NUM_DEPTH_CONVERSION_TYPES; depthType++)
		{
			for (int ssaaMode = 0; ssaaMode < MAX_SSAA_SHADERS; ssaaMode++)
			{
				if (ssaaMode && !s_copy_program[copyMatrixType][depthType][ssaaMode - 1]
					|| depthType && !s_copy_program[copyMatrixType][depthType - 1][ssaaMode]
					|| copyMatrixType && !s_copy_program[copyMatrixType - 1][depthType][ssaaMode])
				{
					// if it failed at a lower setting, it's going to fail here for the same reason it did there,
					// so skip this attempt to avoid duplicate error messages.
					s_copy_program[copyMatrixType][depthType][ssaaMode] = NULL;
				}
				else
				{
					s_copy_program[copyMatrixType][depthType][ssaaMode] = CreateCopyShader(copyMatrixType, depthType, ssaaMode);
				}
			}
		}
	}

	Clear();

	if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX).c_str());

	SETSTAT(stats.numPixelShadersCreated, 0);
	SETSTAT(stats.numPixelShadersAlive, 0);

	pKey_t gameid = (pKey_t)GetMurmurHash3(reinterpret_cast<const u8*>(SConfig::GetInstance().m_strGameID.data()), (u32)SConfig::GetInstance().m_strGameID.size(), 0);
	s_pshaders = ObjectUsageProfiler<PixelShaderUid, pKey_t, PSCacheEntry, PixelShaderUid::ShaderUidHasher>::Create(
		gameid,
		PIXELSHADERGEN_UID_VERSION,
		"Ishiiruka.ps.dx9",
		StringFromFormat("%s.ps.dx9", SConfig::GetInstance().m_strGameID.c_str())
	);

	std::string cache_filename = StringFromFormat("%sIDX9-%s-ps.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
		SConfig::GetInstance().m_strGameID.c_str());

	PixelShaderCacheInserter inserter;
	g_ps_disk_cache.OpenAndRead(cache_filename, inserter);

	if (g_ActiveConfig.bCompileShaderOnStartup)
	{
		std::vector<PixelShaderUid> shaders;
		size_t shader_count = 0;
		s_pshaders->ForEachMostUsedByCategory(gameid,
			[&](const PixelShaderUid& item, size_t total)
		{
			PixelShaderUid newitem = item;
			pixel_shader_uid_data& uid_data = newitem.GetUidData<pixel_shader_uid_data>();

			if (uid_data.render_mode == PSRM_DUAL_SOURCE_BLEND && !g_ActiveConfig.backend_info.bSupportsDualSourceBlend)
			{
				uid_data.render_mode = PSRM_DEFAULT;
				newitem.ClearHASH();
				newitem.CalculateUIDHash();
				CompilePShader(newitem, PIXEL_SHADER_RENDER_MODE::PSRM_DEFAULT, true);
				uid_data.render_mode = PSRM_ALPHA_PASS;
				uid_data.fog_proj = 0;
				uid_data.fog_RangeBaseEnabled = 0;
				newitem.ClearHASH();
				newitem.CalculateUIDHash();
				CompilePShader(newitem, PIXEL_SHADER_RENDER_MODE::PSRM_ALPHA_PASS, true);
			}
			else
			{
				newitem.ClearHASH();
				newitem.CalculateUIDHash();
				CompilePShader(newitem, PIXEL_SHADER_RENDER_MODE::PSRM_DEFAULT, true);
			}
			shader_count++;
			if ((shader_count & 7) == 0)
			{
				Host_UpdateTitle(StringFromFormat("Compiling Pixel Shaders %i %% (%i/%i)", (shader_count * 100) / total, shader_count, total));
				s_compiler->WaitForFinish();
			}
		},
			[](PSCacheEntry& entry)
		{
			return !entry.shader;
		}, true);
		s_compiler->WaitForFinish();
	}
}

// ONLY to be used during shutdown.
void PixelShaderCache::Clear()
{
	if (s_pshaders)
	{
		s_pixel_shaders_lock.lock();
		if (s_compiler)
			s_compiler->WaitForFinish();
		s_pshaders->Persist();
		s_pshaders->Clear([](auto& item)
		{
			item.Destroy();
		});
		s_pixel_shaders_lock.unlock();
	}
	for (u32 i = 0; i < PSRM_DEPTH_ONLY + 1; i++)
	{
		s_last_entry[i] = nullptr;
	}
}

void PixelShaderCache::Shutdown()
{
	if (s_compiler)
	{
		s_compiler->WaitForFinish();
	}
	for (int copyMatrixType = 0; copyMatrixType < NUM_COPY_TYPES; copyMatrixType++)
		for (int depthType = 0; depthType < NUM_DEPTH_CONVERSION_TYPES; depthType++)
			for (int ssaaMode = 0; ssaaMode < MAX_SSAA_SHADERS; ssaaMode++)
				if (s_copy_program[copyMatrixType][depthType][ssaaMode]
					&& (copyMatrixType == 0 || s_copy_program[copyMatrixType][depthType][ssaaMode] != s_copy_program[copyMatrixType - 1][depthType][ssaaMode]))
					s_copy_program[copyMatrixType][depthType][ssaaMode]->Release();

	for (int copyMatrixType = 0; copyMatrixType < NUM_COPY_TYPES; copyMatrixType++)
		for (int depthType = 0; depthType < NUM_DEPTH_CONVERSION_TYPES; depthType++)
			for (int ssaaMode = 0; ssaaMode < MAX_SSAA_SHADERS; ssaaMode++)
				s_copy_program[copyMatrixType][depthType][ssaaMode] = NULL;

	if (s_clear_program) s_clear_program->Release();
	s_clear_program = NULL;
	if (s_rgb8_to_rgba6) s_rgb8_to_rgba6->Release();
	s_rgb8_to_rgba6 = NULL;
	if (s_rgba6_to_rgb8) s_rgba6_to_rgb8->Release();
	s_rgba6_to_rgb8 = NULL;


	Clear();
	delete s_pshaders;
	s_pshaders = nullptr;
	g_ps_disk_cache.Sync();
	g_ps_disk_cache.Close();

	s_unique_shaders.clear();
}

void PixelShaderCache::CompilePShader(const PixelShaderUid& uid, PIXEL_SHADER_RENDER_MODE render_mode, bool ongputhread)
{
	const API_TYPE api = ((D3D::GetCaps().PixelShaderVersion >> 8) & 0xFF) < 3 ? API_D3D9_SM20 : API_D3D9_SM30;
	s_pixel_shaders_lock.lock();
	PSCacheEntry* entry = &s_pshaders->GetOrAdd(uid);
	s_pixel_shaders_lock.unlock();
	if (ongputhread)
	{
		s_last_entry[render_mode] = entry;
	}
	// Compile only when we have a new instance
	if (entry->initialized.test_and_set())
	{
		return;
	}
	// Need to compile a new shader

	ShaderCompilerWorkUnit *wunit = s_compiler->NewUnit(PIXELSHADERGEN_BUFFERSIZE);
	wunit->GenerateCodeHandler = [uid, api](ShaderCompilerWorkUnit* wunit)
	{
		ShaderCode code;
		code.SetBuffer(wunit->code.data());
		if (api == API_D3D9_SM20)
		{
			GeneratePixelShaderCodeD3D9SM2(code, uid.GetUidData());
		}
		else
		{
			GeneratePixelShaderCodeD3D9(code, uid.GetUidData());
		}
		wunit->codesize = (u32)code.BufferSize();
	};
	wunit->entrypoint = "main";
	wunit->flags = D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_OPTIMIZATION_LEVEL3;
	wunit->target = D3D::PixelShaderVersionString();
	wunit->ResultHandler = [uid, entry](ShaderCompilerWorkUnit* wunit)
	{
		if (SUCCEEDED(wunit->cresult))
		{
			ID3DBlob* shaderBuffer = wunit->shaderbytecode;
			const u8* bytecode = (const u8*)shaderBuffer->GetBufferPointer();
			u32 bytecodelen = (u32)shaderBuffer->GetBufferSize();
			g_ps_disk_cache.Append(uid, bytecode, bytecodelen);
			PushByteCode(uid, bytecode, bytecodelen, entry);
		}
		else
		{
			static int num_failures = 0;
			std::string filename = StringFromFormat("%sbad_ps_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
			std::ofstream file;
			OpenFStream(file, filename, std::ios_base::out);
			file << ((const char *)wunit->code.data());
			file << ((const char*)wunit->error->GetBufferPointer());
			file.close();

			PanicAlert("Failed to compile pixel shader!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%s):\n%s",
				filename,
				D3D::VertexShaderVersionString(),
				(const char*)wunit->error->GetBufferPointer());
		}
	};
	s_compiler->CompileShaderAsync(wunit);
}

void PixelShaderCache::PrepareShader(
	PIXEL_SHADER_RENDER_MODE render_mode,
	u32 components,
	const XFMemory &xfr,
	const BPMemory &bpm,
	bool ongputhread)
{
	PixelShaderUid uid;
	GetPixelShaderUID(uid, render_mode, components, xfr, bpm);
	if (ongputhread)
	{
		s_compiler->ProcCompilationResults();
		// Check if the shader is already set
		if (s_last_entry[render_mode])
		{
			if (uid == s_last_uid[render_mode])
			{
				return;
			}
		}
		s_last_uid[render_mode] = uid;
		GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
	}
	else
	{
		if (s_external_last_uid[render_mode] == uid)
		{
			return;
		}
		s_external_last_uid[render_mode] = uid;
	}
	CompilePShader(uid, render_mode, ongputhread);
}

bool PixelShaderCache::SetShader(PIXEL_SHADER_RENDER_MODE render_mode)
{
	const PSCacheEntry* entry = s_last_entry[render_mode];
	u32 count = 0;
	while (!entry->compiled)
	{
		s_compiler->ProcCompilationResults();
		if (g_ActiveConfig.bFullAsyncShaderCompilation)
		{
			break;
		}
		Common::cYield(count++);
	}
	if (entry->shader && entry->compiled)
	{
		D3D::SetPixelShader(entry->shader);
		return true;
	}
	return false;
}

void PixelShaderCache::PushByteCode(const PixelShaderUid &uid, const u8 *bytecode, int bytecodelen, PixelShaderCache::PSCacheEntry* entry)
{
	entry->shader = D3D::CreatePixelShaderFromByteCode(bytecode, bytecodelen);
	entry->compiled = true;
	if (entry->shader)
	{
		INCSTAT(stats.numPixelShadersCreated);
		SETSTAT(stats.numPixelShadersAlive, static_cast<int>(s_pshaders->size()));
	}
}

void PixelShaderCache::InsertByteCode(const PixelShaderUid &uid, const u8 *bytecode, int bytecodelen)
{
	PixelShaderCache::PSCacheEntry *entry = &s_pshaders->GetOrAdd(uid);
	entry->initialized.test_and_set();
	PushByteCode(uid, bytecode, bytecodelen, entry);
}
}  // namespace DX9
