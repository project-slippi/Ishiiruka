// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/Memmap.h"
#include "VideoCommon/HLSLCompiler.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoBackends/DX11/D3DPtr.h"
#include "VideoBackends/DX11/D3DBase.h"
#include "VideoBackends/DX11/D3DShader.h"
#include "VideoBackends/DX11/D3DUtil.h"
#include "VideoBackends/DX11/FramebufferManager.h"
#include "VideoBackends/DX11/D3DState.h"
#include "VideoBackends/DX11/PSTextureEncoder.h"
#include "VideoBackends/DX11/Render.h"
#include "VideoBackends/DX11/TextureCache.h"
#include "VideoBackends/DX11/VertexShaderCache.h"

#include "VideoCommon/TextureConversionShader.h"

namespace DX11
{

struct EFBEncodeParams
{
	DWORD SrcLeft;
	DWORD SrcTop;
	DWORD DestWidth;
	DWORD ScaleFactor;
};

PSTextureEncoder::PSTextureEncoder()
	: m_ready(false), m_out(nullptr), m_outRTV(nullptr), m_outStage(nullptr),
	m_encodeParams(nullptr)
{}

void PSTextureEncoder::Init()
{
	m_ready = false;

	HRESULT hr;

	// Create output texture RGBA format
	D3D11_TEXTURE2D_DESC t2dd = CD3D11_TEXTURE2D_DESC(
		DXGI_FORMAT_B8G8R8A8_UNORM,
		EFB_WIDTH * 4, EFB_HEIGHT / 4, 1, 1, D3D11_BIND_RENDER_TARGET);
	hr = D3D::device->CreateTexture2D(&t2dd, nullptr, D3D::ToAddr(m_out));
	CHECK(SUCCEEDED(hr), "create efb encode output texture");
	D3D::SetDebugObjectName(m_out.get(), "efb encoder output texture");

	// Create output render target view
	D3D11_RENDER_TARGET_VIEW_DESC rtvd = CD3D11_RENDER_TARGET_VIEW_DESC(m_out.get(),
		D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_B8G8R8A8_UNORM);
	hr = D3D::device->CreateRenderTargetView(m_out.get(), &rtvd, D3D::ToAddr(m_outRTV));
	CHECK(SUCCEEDED(hr), "create efb encode output render target view");
	D3D::SetDebugObjectName(m_outRTV.get(), "efb encoder output rtv");

	// Create output staging buffer
	t2dd.Usage = D3D11_USAGE_STAGING;
	t2dd.BindFlags = 0;
	t2dd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	hr = D3D::device->CreateTexture2D(&t2dd, nullptr, D3D::ToAddr(m_outStage));
	CHECK(SUCCEEDED(hr), "create efb encode output staging buffer");
	D3D::SetDebugObjectName(m_outStage.get(), "efb encoder output staging buffer");

	// Create constant buffer for uploading data to shaders
	D3D11_BUFFER_DESC bd = CD3D11_BUFFER_DESC(sizeof(EFBEncodeParams),
		D3D11_BIND_CONSTANT_BUFFER);
	hr = D3D::device->CreateBuffer(&bd, nullptr, D3D::ToAddr(m_encodeParams));
	CHECK(SUCCEEDED(hr), "create efb encode params buffer");
	D3D::SetDebugObjectName(m_encodeParams.get(), "efb encoder params buffer");

	m_ready = true;
	// Warm up with shader cache
	std::string cache_filename = StringFromFormat("%sdx11-ENCODER-ps.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str());
	m_shaderCache.OpenAndRead(cache_filename, ShaderCacheInserter(*this));
}

void PSTextureEncoder::Shutdown()
{
	m_ready = false;

	m_encoding_shaders.clear();

	m_encodeParams.reset();
	m_outStage.reset();
	m_outRTV.reset();
	m_out.reset();
}

void PSTextureEncoder::Encode(u8* dst, const EFBCopyFormat& format, u32 native_width,
	u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
	bool is_depth_copy, const EFBRectangle& src_rect, bool scale_by_half)
{
	if (!m_ready) // Make sure we initialized OK
		return;
	auto shader = GetEncodingPixelShader(format);
	if (shader == nullptr)
	{
		return;
	}
	HRESULT hr;

	// Resolve MSAA targets before copying.
	ID3D11ShaderResourceView* pEFB = is_depth_copy ?
		FramebufferManager::GetResolvedEFBDepthTexture()->GetSRV() :
		// FIXME: Instead of resolving EFB, it would be better to pick out a
		// single sample from each pixel. The game may break if it isn't
		// expecting the blurred edges around multisampled shapes.
		FramebufferManager::GetResolvedEFBColorTexture()->GetSRV();

	// Reset API
	g_renderer->ResetAPIState();

	// Set up all the state for EFB encoding
	{
		const u32 words_per_row = bytes_per_row / sizeof(u32);

		D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, FLOAT(words_per_row), FLOAT(num_blocks_y));
		D3D::context->RSSetViewports(1, &vp);

		constexpr EFBRectangle fullSrcRect(0, 0, EFB_WIDTH, EFB_HEIGHT);
		TargetRectangle targetRect = g_renderer->ConvertEFBRectangle(fullSrcRect);
		ID3D11RenderTargetView* rtv = m_outRTV.get();
		D3D::context->OMSetRenderTargets(1, &rtv, nullptr);

		EFBEncodeParams params;
		params.SrcLeft = src_rect.left;
		params.SrcTop = src_rect.top;
		params.DestWidth = native_width;
		params.ScaleFactor = scale_by_half ? 2 : 1;
		D3D::context->UpdateSubresource(m_encodeParams.get(), 0, nullptr, &params, 0, 0);
		D3D::stateman->SetPixelConstants(m_encodeParams.get());

		// Use linear filtering if (bScaleByHalf), use point filtering otherwise
		if (scale_by_half || g_ActiveConfig.iEFBScale != SCALE_1X)
			D3D::SetLinearCopySampler();
		else
			D3D::SetPointCopySampler();

		D3D::drawShadedTexQuad(pEFB,
			targetRect.AsRECT(),
			g_renderer->GetTargetWidth(),
			g_renderer->GetTargetHeight(),
			shader,
			VertexShaderCache::GetSimpleVertexShader(),
			VertexShaderCache::GetSimpleInputLayout());

		// Copy to staging buffer
		D3D11_BOX srcBox = CD3D11_BOX(0, 0, 0, words_per_row, num_blocks_y, 1);
		D3D::context->CopySubresourceRegion(m_outStage.get(), 0, 0, 0, 0, m_out.get(), 0, &srcBox);

		// Transfer staging buffer to GameCube/Wii RAM
		D3D11_MAPPED_SUBRESOURCE map = { 0 };
		hr = D3D::context->Map(m_outStage.get(), 0, D3D11_MAP_READ, 0, &map);
		CHECK(SUCCEEDED(hr), "map staging buffer (0x%x)", hr);

		u8* src = (u8*)map.pData;
		u32 readStride = std::min(bytes_per_row, map.RowPitch);
		for (unsigned int y = 0; y < num_blocks_y; ++y)
		{
			memcpy(dst, src, readStride);
			dst += memory_stride;
			src += map.RowPitch;
		}

		D3D::context->Unmap(m_outStage.get(), 0);
	}

	// Restore API
	g_renderer->RestoreAPIState();
	D3D::context->OMSetRenderTargets(1,
		&FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());
}

ID3D11PixelShader* PSTextureEncoder::GetEncodingPixelShader(const EFBCopyFormat& format)
{
	auto iter = m_encoding_shaders.find(format);
	if (iter != m_encoding_shaders.end())
		return iter->second.get();

	D3DBlob bytecode;
	const char* shader = TextureConversionShader::GenerateEncodingShader(format, API_D3D11);
	if (!D3D::CompileShader(D3D::ShaderType::Pixel, shader, bytecode))
	{
		PanicAlert("Failed to compile texture encoding shader.");
		m_encoding_shaders[format] = nullptr;
		return nullptr;
	}
	m_shaderCache.Append(format, bytecode.Data(), (u32)bytecode.Size());
	return InsertShader(format, bytecode.Data(), (u32)bytecode.Size());
}

ID3D11PixelShader* PSTextureEncoder::InsertShader(const EFBCopyFormat &key, u8 const *data, u32 sz)
{
	ID3D11PixelShader* newShader;
	HRESULT hr = D3D::device->CreatePixelShader(data, sz, nullptr, &newShader);
	CHECK(SUCCEEDED(hr), "create efb encoder pixel shader");

	m_encoding_shaders.emplace(key, D3D::PixelShaderPtr(newShader));
	return newShader;
}

}
