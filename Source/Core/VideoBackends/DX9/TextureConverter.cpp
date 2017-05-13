// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Fast image conversion using HLSL shaders.
// This kind of stuff would be a LOT nicer with OpenCL opr DirectCompute.
#include "Math.h"

#include "Common/Align.h"
#include "Common/FileUtil.h"

#include "Core/HW/Memmap.h"

#include "VideoBackends/DX9/FramebufferManager.h"
#include "VideoBackends/DX9/PixelShaderCache.h"
#include "VideoBackends/DX9/Render.h"
#include "VideoBackends/DX9/TextureCache.h"
#include "VideoBackends/DX9/TextureConverter.h"
#include "VideoBackends/DX9/VertexShaderCache.h"

#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/TextureConversionShader.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace DX9
{

namespace TextureConverter
{
struct XFBReadBuffer
{
	LPDIRECT3DTEXTURE9 XFBMEMTexture;
	LPDIRECT3DTEXTURE9 XFBTexture;
	LPDIRECT3DSURFACE9 ReadSurface;
	LPDIRECT3DSURFACE9 WriteSurface;
	int Width;
	int Height;
	void Clear()
	{
		XFBTexture = nullptr;
		XFBMEMTexture = nullptr;
		ReadSurface = nullptr;
		WriteSurface = nullptr;
		Width = 0;
		Height = 0;
	}
	void Release()
	{
		if (ReadSurface)
			ReadSurface->Release();
		if (WriteSurface)
			WriteSurface->Release();
		if (XFBTexture)
			XFBTexture->Release();
		if (XFBMEMTexture)
			XFBMEMTexture->Release();
		Clear();
	}
};

struct TransformBuffer
{
	LPDIRECT3DTEXTURE9 FBTexture;
	LPDIRECT3DSURFACE9 RenderSurface;
	LPDIRECT3DSURFACE9 ReadSurface;
	int Width;
	int Height;
	u32 hits;
	void Clear()
	{
		FBTexture = nullptr;
		RenderSurface = nullptr;
		ReadSurface = nullptr;
		Width = 0;
		Height = 0;
		hits = 0;
	}
	void Release()
	{
		if (RenderSurface != nullptr)
			RenderSurface->Release();

		if (ReadSurface != nullptr)
			ReadSurface->Release();

		if (FBTexture != nullptr)
			FBTexture->Release();
		Clear();
	}
};
const u32 NUM_TRANSFORM_BUFFERS = 16;
static TransformBuffer TrnBuffers[NUM_TRANSFORM_BUFFERS];
static u32 WorkingBuffers = 0;

const u32 NUM_XFBREAD_BUFFER = 4;
static XFBReadBuffer XReadBuffers[NUM_XFBREAD_BUFFER];
static u32 xfreadBuffers = 0;

static LPDIRECT3DPIXELSHADER9 s_rgbToYuyvProgram = nullptr;
static LPDIRECT3DPIXELSHADER9 s_yuyvToRgbProgram = nullptr;

struct EncodingProgram
{
	LPDIRECT3DPIXELSHADER9 program;
	bool failed;
};
static std::map<EFBCopyFormat, EncodingProgram> s_encoding_programs;

void CreateRgbToYuyvProgram()
{
	// Output is BGRA because that is slightly faster than RGBA.
	char* FProgram = new char[2048];
	sprintf(FProgram, "uniform float4 blkDims : register(c%d);\n"
		"uniform float4 textureDims : register(c%d);\n"
		"uniform sampler samp0 : register(s0);\n"
		"void main(\n"
		"  out float4 ocol0 : COLOR0,\n"
		"  in float2 uv0 : TEXCOORD0,\n"
		"  in float uv2 : TEXCOORD1)\n"
		"{\n"
		"  float2 uv1 = float2((uv0.x + 1.0f)/ blkDims.z, uv0.y / blkDims.w);\n"
		"  float3 c0 = tex2D(samp0, uv0.xy / blkDims.zw).rgb;\n"
		"  float3 c1 = tex2D(samp0, uv1).rgb;\n"
		"  c0 = pow(c0,uv2.xxx);\n"
		"  c1 = pow(c1,uv2.xxx);\n"
		"  float3 y_const = float3(0.257f,0.504f,0.098f);\n"
		"  float3 u_const = float3(-0.148f,-0.291f,0.439f);\n"
		"  float3 v_const = float3(0.439f,-0.368f,-0.071f);\n"
		"  float4 const3 = float4(0.0625f,0.5f,0.0625f,0.5f);\n"
		"  float3 c01 = (c0 + c1) * 0.5f;\n"
		"  ocol0 = float4(dot(c1,y_const),dot(c01,u_const),dot(c0,y_const),dot(c01, v_const)) + const3;\n"
		"}\n", C_COLORMATRIX, C_COLORMATRIX + 1);

	s_rgbToYuyvProgram = D3D::CompileAndCreatePixelShader(FProgram, (int)strlen(FProgram));
	if (!s_rgbToYuyvProgram)
	{
		ERROR_LOG(VIDEO, "Failed to create RGB to YUYV fragment program");
	}
	delete[] FProgram;
}

void CreateYuyvToRgbProgram()
{
	char* FProgram = new char[2048];
	sprintf(FProgram, "uniform float4 blkDims : register(c%d);\n"
		"uniform float4 textureDims : register(c%d);\n"
		"uniform sampler samp0 : register(s0);\n"
		"void main(\n"
		"  out float4 ocol0 : COLOR0,\n"
		"  in float2 uv0 : TEXCOORD0)\n"
		"{\n"
		"  float4 c0 = tex2D(samp0, uv0 / blkDims.zw).rgba;\n"
		"  float f = step(0.5, frac(uv0.x));\n"
		"  float y = lerp(c0.b, c0.r, f);\n"
		"  float yComp = 1.164f * (y - 0.0625f);\n"
		"  float uComp = c0.g - 0.5f;\n"
		"  float vComp = c0.a - 0.5f;\n"

		"  ocol0 = float4(yComp + (1.596f * vComp),\n"
		"                 yComp - (0.813f * vComp) - (0.391f * uComp),\n"
		"                 yComp + (2.018f * uComp),\n"
		"                 1.0f);\n"
		"}\n", C_COLORMATRIX, C_COLORMATRIX + 1);
	s_yuyvToRgbProgram = D3D::CompileAndCreatePixelShader(FProgram, (int)strlen(FProgram));
	if (!s_yuyvToRgbProgram)
	{
		ERROR_LOG(VIDEO, "Failed to create YUYV to RGB fragment program");
	}
	delete[] FProgram;
}

LPDIRECT3DPIXELSHADER9 GetOrCreateEncodingShader(const EFBCopyFormat& format)
{
	auto iter = s_encoding_programs.find(format);
	if (iter != s_encoding_programs.end() && !iter->second.failed)
		return iter->second.program;

	if (iter->second.failed)
	{
		return nullptr;
	}

	const char* shader = TextureConversionShaderLegacy::GenerateEncodingShader(format);

#if defined(_DEBUG) || defined(DEBUGFAST)
	if (g_ActiveConfig.iLog & CONF_SAVESHADERS && shader)
	{
		static int counter = 0;
		char szTemp[MAX_PATH];
		sprintf(szTemp, "%senc_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), counter++);

		SaveData(szTemp, shader);
	}
#endif
	EncodingProgram program;
	program.program = D3D::CompileAndCreatePixelShader(shader, (int)strlen(shader));
	program.failed = !program.program;
	if (program.failed)
	{
		ERROR_LOG(VIDEO, "Failed to create encoding fragment program");
	}

	return s_encoding_programs.emplace(format, program).first->second.program;
}

void Init()
{
	CreateRgbToYuyvProgram();
	CreateYuyvToRgbProgram();
	xfreadBuffers = 0;
	WorkingBuffers = 0;
	for (unsigned int i = 0; i < NUM_TRANSFORM_BUFFERS; i++)
	{
		TrnBuffers[i].Clear();
	}
	for (unsigned int i = 0; i < NUM_XFBREAD_BUFFER; i++)
	{
		XReadBuffers[i].Clear();
	}
}

void Shutdown()
{
	if (s_rgbToYuyvProgram)
		s_rgbToYuyvProgram->Release();
	s_rgbToYuyvProgram = nullptr;
	if (s_yuyvToRgbProgram)
		s_yuyvToRgbProgram->Release();
	s_yuyvToRgbProgram = nullptr;

	for (auto& program : s_encoding_programs)
	{
		if(program.second.program)
			program.second.program->Release();
	}
	s_encoding_programs.clear();

	for (unsigned int i = 0; i < NUM_TRANSFORM_BUFFERS; i++)
	{
		TrnBuffers[i].Release();
	}
	for (unsigned int i = 0; i < NUM_XFBREAD_BUFFER; i++)
	{
		XReadBuffers[i].Release();
	}
	xfreadBuffers = 0;
	WorkingBuffers = 0;
}

void EncodeToRamUsingShader(LPDIRECT3DPIXELSHADER9 shader, LPDIRECT3DTEXTURE9 srcTexture, const TargetRectangle& sourceRc,
	u8* destAddr, int dst_line_size, int dstHeight, int writeStride, int readStride, bool linearFilter, float Gamma)
{
	HRESULT hr;
	u32 index = 0;
	u32 dstWidth = (dst_line_size / 4);
	while (index < WorkingBuffers && (TrnBuffers[index].Width != dstWidth || TrnBuffers[index].Height != dstHeight))
		index++;
	LPDIRECT3DSURFACE9  s_texConvReadSurface = nullptr;
	LPDIRECT3DSURFACE9 Rendersurf = nullptr;
	TransformBuffer &currentbuffer = TrnBuffers[0];
	if (index >= WorkingBuffers)
	{
		if (WorkingBuffers < NUM_TRANSFORM_BUFFERS)
			WorkingBuffers++;
		if (index >= WorkingBuffers)
		{
			index = 0;
			u32 hits = TrnBuffers[index].hits;
			for (u32 i = 0; i < WorkingBuffers; i++)
			{
				if (TrnBuffers[i].hits < hits)
				{
					index = i;
					hits = TrnBuffers[index].hits;
				}
				TrnBuffers[i].hits = 0;
			}
		}
		currentbuffer = TrnBuffers[index];
		currentbuffer.Release();
		currentbuffer.Width = dstWidth;
		currentbuffer.Height = dstHeight;
		D3D::dev->CreateTexture(dstWidth, dstHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
			D3DPOOL_DEFAULT, &currentbuffer.FBTexture, nullptr);
		currentbuffer.FBTexture->GetSurfaceLevel(0, &currentbuffer.RenderSurface);
		D3D::dev->CreateOffscreenPlainSurface(dstWidth, dstHeight, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &currentbuffer.ReadSurface, nullptr);
	}
	else
	{
		currentbuffer = TrnBuffers[index];
		currentbuffer.hits++;
	}

	s_texConvReadSurface = currentbuffer.ReadSurface;
	Rendersurf = currentbuffer.RenderSurface;

	hr = D3D::dev->SetDepthStencilSurface(nullptr);
	hr = D3D::dev->SetRenderTarget(0, Rendersurf);

	if (linearFilter || g_ActiveConfig.iEFBScale != SCALE_1X)
	{
		D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	}
	else
	{
		D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	}

	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width = dstWidth;
	vp.Height = dstHeight;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	hr = D3D::dev->SetViewport(&vp);
	RECT SrcRect;
	SrcRect.top = sourceRc.top;
	SrcRect.left = sourceRc.left;
	SrcRect.right = sourceRc.right;
	SrcRect.bottom = sourceRc.bottom;
	RECT DstRect;
	DstRect.top = 0;
	DstRect.left = 0;
	DstRect.right = dstWidth;
	DstRect.bottom = dstHeight;


	// Draw...
	D3D::drawShadedTexQuad(srcTexture, &SrcRect, 1, 1, dstWidth, dstHeight, shader, VertexShaderCache::GetSimpleVertexShader(0), Gamma);
	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);
	// .. and then read back the results.
	// TODO: make this less slow.

	hr = D3D::dev->GetRenderTargetData(Rendersurf, s_texConvReadSurface);

	D3DLOCKED_RECT drect;
	hr = s_texConvReadSurface->LockRect(&drect, &DstRect, D3DLOCK_READONLY);

	u8* Source = (u8*)drect.pBits;
	int readHeight = readStride / dst_line_size;
	int readLoops = dstHeight / readHeight;
	int dstSize = dst_line_size * dstHeight;
	if ((writeStride != readStride) && (readLoops > 1))
	{
		for (int i = 0; i < readLoops; i++)
		{
			memcpy(destAddr, Source, readStride);
			Source += readStride;
			destAddr += writeStride;
		}
	}
	else
	{
		memcpy(destAddr, Source, dstSize);
	}
	hr = s_texConvReadSurface->UnlockRect();
}
// returns size of the encoded data (in bytes)
void EncodeToRamFromTexture(u8* dest_ptr, const EFBCopyFormat& format, u32 native_width,
	u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
	bool is_depth_copy, const EFBRectangle& src_rect, bool scale_by_half)
{
	LPDIRECT3DPIXELSHADER9 texconv_shader = GetOrCreateEncodingShader(format);
	if (!texconv_shader)
		return;
	const LPDIRECT3DTEXTURE9 read_texture = is_depth_copy ?
		FramebufferManager::GetEFBDepthTexture() :
		FramebufferManager::GetEFBColorTexture();

	const u16 blkW = TexDecoder_GetBlockWidthInTexels(format.copy_format);
	const u16 blkH = TexDecoder_GetBlockHeightInTexels(format.copy_format);
	const u16 samples = TextureConversionShader::GetEncodedSampleCount(format.copy_format);

	// only copy on cache line boundaries
	// extra pixels are copied but not displayed in the resulting texture
	s32 expandedWidth = static_cast<s32>(Common::AlignUpSizePow2(native_width, blkW));
	s32 expandedHeight = num_blocks_y * blkH;

	float sampleStride = scale_by_half ? 2.f : 1.f;
	TextureConversionShaderLegacy::SetShaderParameters(
		(float)expandedWidth,
		(float)g_renderer->EFBToScaledY(expandedHeight), // TODO: Why do we scale this?
		(float)g_renderer->EFBToScaledX(src_rect.left),
		(float)g_renderer->EFBToScaledY(src_rect.top),
		g_renderer->EFBToScaledXf(sampleStride),
		g_renderer->EFBToScaledYf(sampleStride),
		(float)g_renderer->GetTargetWidth(),
		(float)g_renderer->GetTargetHeight());
	D3D::dev->SetPixelShaderConstantF(C_COLORMATRIX, PixelShaderManager::GetBuffer(), 2);
	TargetRectangle scaledSource;
	scaledSource.top = 0;
	scaledSource.bottom = expandedHeight;
	scaledSource.left = 0;
	scaledSource.right = expandedWidth / samples;
	EncodeToRamUsingShader(
		texconv_shader,
		read_texture,
		scaledSource,
		dest_ptr, scaledSource.right * 4, expandedHeight, memory_stride, bytes_per_row, scale_by_half, 1.0f);
}

void EncodeToRamYUYV(LPDIRECT3DTEXTURE9 srcTexture, const TargetRectangle& sourceRc, u8* destAddr, u32 dstwidth, u32 dstStride, u32 dstHeight, float Gamma)
{
	TextureConversionShaderLegacy::SetShaderParameters(
		(float)dstwidth,
		(float)dstHeight,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		(float)g_renderer->GetTargetWidth(),
		(float)g_renderer->GetTargetHeight());
	D3D::dev->SetPixelShaderConstantF(C_COLORMATRIX, PixelShaderManager::GetBuffer(), 2);
	g_renderer->ResetAPIState();
	EncodeToRamUsingShader(s_rgbToYuyvProgram, srcTexture, sourceRc, destAddr,
		dstwidth * 2, dstHeight, dstStride, dstStride, false, Gamma);
	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
	g_renderer->RestoreAPIState();
}


// Should be scale free.
void DecodeToTexture(u32 xfbAddr, int srcWidth, int srcHeight, LPDIRECT3DTEXTURE9 destTexture)
{
	u8* srcAddr = Memory::GetPointer(xfbAddr);
	if (!srcAddr)
	{
		WARN_LOG(VIDEO, "Tried to decode from invalid memory address");
		return;
	}

	int srcFmtWidth = srcWidth / 2;
	XFBReadBuffer &ReadBuffer = XReadBuffers[xfreadBuffers];
	if (ReadBuffer.Width != srcFmtWidth || ReadBuffer.Height != srcHeight)
	{
		ReadBuffer.Release();
		ReadBuffer.Width = srcFmtWidth;
		ReadBuffer.Height = srcHeight;
		ReadBuffer.XFBMEMTexture = D3D::CreateTexture2D(srcFmtWidth, srcHeight, D3DFMT_A8R8G8B8, 1, D3DPOOL_SYSTEMMEM);
		ReadBuffer.XFBTexture = D3D::CreateTexture2D(srcFmtWidth, srcHeight, D3DFMT_A8R8G8B8);
		ReadBuffer.XFBMEMTexture->GetSurfaceLevel(0, &ReadBuffer.ReadSurface);
		ReadBuffer.XFBTexture->GetSurfaceLevel(0, &ReadBuffer.WriteSurface);
	}
	D3D::ReplaceTexture2D(ReadBuffer.XFBMEMTexture, srcAddr, srcFmtWidth, srcHeight, srcFmtWidth, D3DFMT_A8R8G8B8, false);
	RECT srcr{ 0, 0, srcFmtWidth, srcHeight };
	POINT dstp{ 0, 0 };
	D3D::dev->UpdateSurface(ReadBuffer.ReadSurface, &srcr, ReadBuffer.WriteSurface, &dstp);
	g_renderer->ResetAPIState(); // reset any game specific settings

	LPDIRECT3DSURFACE9 Rendersurf = nullptr;
	destTexture->GetSurfaceLevel(0, &Rendersurf);
	D3D::dev->SetDepthStencilSurface(nullptr);
	D3D::dev->SetRenderTarget(0, Rendersurf);

	D3DVIEWPORT9 vp;

	// Stretch picture with increased internal resolution
	vp.X = 0;
	vp.Y = 0;
	vp.Width = srcWidth;
	vp.Height = srcHeight;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);
	RECT destrect;
	destrect.bottom = srcHeight;
	destrect.left = 0;
	destrect.right = srcWidth;
	destrect.top = 0;

	RECT sourcerect;
	sourcerect.bottom = srcHeight;
	sourcerect.left = 0;
	sourcerect.right = srcFmtWidth;
	sourcerect.top = 0;

	D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	D3D::ChangeSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);

	TextureConversionShaderLegacy::SetShaderParameters(
		(float)srcFmtWidth,
		(float)srcHeight,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		(float)srcFmtWidth,
		(float)srcHeight);
	D3D::dev->SetPixelShaderConstantF(C_COLORMATRIX, PixelShaderManager::GetBuffer(), 2);
	D3D::drawShadedTexQuad(
		ReadBuffer.XFBTexture,
		&sourcerect,
		1,
		1,
		srcWidth,
		srcHeight,
		s_yuyvToRgbProgram,
		VertexShaderCache::GetSimpleVertexShader(0));


	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);
	D3D::RefreshSamplerState(0, D3DSAMP_MAGFILTER);
	D3D::SetTexture(0, nullptr);
	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
	g_renderer->RestoreAPIState();
	Rendersurf->Release();
	xfreadBuffers = (xfreadBuffers + 1) % NUM_XFBREAD_BUFFER;
}

}  // namespace

}  // namespace DX9
