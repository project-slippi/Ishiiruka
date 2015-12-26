// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/Memmap.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DShader.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/PSTextureEncoder.h"
#include "VideoBackends/D3D12/Render.h"
#include "VideoBackends/D3D12/TextureCache.h"
#include "VideoBackends/D3D12/VertexShaderCache.h"

#include "VideoCommon/TextureConversionShader.h"

namespace DX12
{

struct EFBEncodeParams
{
	DWORD SrcLeft;
	DWORD SrcTop;
	DWORD DestWidth;
	DWORD ScaleFactor;
};

PSTextureEncoder::PSTextureEncoder()
	: m_ready(false), m_out12(nullptr), m_outStage12(nullptr), m_encodeParams12(nullptr), m_encodeParams12data(nullptr)
{
}

void PSTextureEncoder::Init()
{
	m_ready = false;

	// Create output texture RGBA format
	D3D12_RESOURCE_DESC t2dd12 = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_B8G8R8A8_UNORM,
		EFB_WIDTH * 4,
		EFB_HEIGHT / 4,
		1,
		0,
		1,
		0,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
		);

	D3D12_CLEAR_VALUE optimized_clear_value = { DXGI_FORMAT_B8G8R8A8_UNORM, { 0.0f, 0.0f, 0.0f, 1.0f } };

	CheckHR(D3D::device12->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &t2dd12, D3D12_RESOURCE_STATE_COPY_SOURCE, &optimized_clear_value, IID_PPV_ARGS(&m_out12)));
	D3D::SetDebugObjectName12(m_out12, "efb encoder output texture");

	// Create output render target view
	D3D12_RENDER_TARGET_VIEW_DESC rtvd12 = {
		DXGI_FORMAT_B8G8R8A8_UNORM,    // DXGI_FORMAT Format;
		D3D12_RTV_DIMENSION_TEXTURE2D  // D3D12_RTV_DIMENSION ViewDimension;
	};
	rtvd12.Texture2D.MipSlice = 0;

	D3D::rtv_descriptor_heap_mgr->Allocate(&m_outRTV12cpu);
	D3D::device12->CreateRenderTargetView(m_out12, &rtvd12, m_outRTV12cpu);

	// Create output staging buffer
	CheckHR(
		D3D::device12->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT + ((t2dd12.Width * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)) * t2dd12.Height),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_outStage12)
			)
		);

	D3D::SetDebugObjectName12(m_outStage12, "efb encoder output staging buffer");

	CheckHR(m_outStage12->Map(0, nullptr, &m_outStage12data));

	// Create constant buffer for uploading data to shaders
	UINT32 paddedSize = (sizeof(EFBEncodeParams) + 0xff) & ~0xff;

	CheckHR(
		D3D::device12->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(paddedSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_encodeParams12)
			)
		);

	D3D::SetDebugObjectName12(m_encodeParams12, "efb encoder params buffer");

	CheckHR(m_encodeParams12->Map(0, nullptr, &m_encodeParams12data));

	// Don't need to create a descriptor for the constant buffer, as we'll be creating it on the fly as a root descriptor.

	m_ready = true;
}

void PSTextureEncoder::Shutdown()
{
	m_ready = false;

	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_encodeParams12);
	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_out12);
	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_outStage12);

	for (auto& it : m_staticShaders12blobs)
	{
		SAFE_RELEASE(it);
	}

	m_staticShaders12.clear();
	m_staticShaders12blobs.clear();
}

void PSTextureEncoder::Encode(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
	PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
	bool isIntensity, bool scaleByHalf)
{
	if (!m_ready) // Make sure we initialized OK
		return;

	{
#ifdef USE_D3D12_FREQUENT_EXECUTION
		D3D::command_list_mgr->CPUAccessNotify();
#endif

		const u32 words_per_row = bytes_per_row / sizeof(u32);

		D3D12_VIEWPORT vp12 = { 0.f, 0.f, FLOAT(words_per_row), FLOAT(num_blocks_y), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
		D3D::current_command_list->RSSetViewports(1, &vp12);

		constexpr EFBRectangle fullSrcRect(0, 0, EFB_WIDTH, EFB_HEIGHT);

		TargetRectangle targetRect = g_renderer->ConvertEFBRectangle(fullSrcRect);

		D3DTexture2D* pEFB = (srcFormat == PEControl::Z24) ?
			FramebufferManager::GetResolvedEFBDepthTexture() :
			// FIXME: Instead of resolving EFB, it would be better to pick out a
			// single sample from each pixel. The game may break if it isn't
			// expecting the blurred edges around multisampled shapes.
			FramebufferManager::GetResolvedEFBColorTexture();

		// GetResolvedEFBDepthTexture will set the render targets, when MSAA is enabled 
		// (since it needs to do a manual depth resolve). So make sure to set the RTs
		// afterwards.

		D3D::ResourceBarrier(D3D::current_command_list, m_out12, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, 0);
		D3D::current_command_list->OMSetRenderTargets(1, &m_outRTV12cpu, FALSE, nullptr);

		EFBEncodeParams params;
		params.SrcLeft = srcRect.left;
		params.SrcTop = srcRect.top;
		params.DestWidth = native_width;
		params.ScaleFactor = scaleByHalf ? 2 : 1;

		memcpy(m_encodeParams12data, &params, sizeof(params));
		D3D::current_command_list->SetGraphicsRootConstantBufferView(DESCRIPTOR_TABLE_PS_CBVONE, m_encodeParams12->GetGPUVirtualAddress());
		D3D::command_list_mgr->m_dirty_ps_cbv = true;

		// Use linear filtering if (bScaleByHalf), use point filtering otherwise
		if (scaleByHalf)
			D3D::SetLinearCopySampler();
		else
			D3D::SetPointCopySampler();

		D3D::DrawShadedTexQuad(pEFB,
			targetRect.AsRECT(),
			Renderer::GetTargetWidth(),
			Renderer::GetTargetHeight(),
			SetStaticShader12(format, srcFormat, isIntensity, scaleByHalf),
			VertexShaderCache::GetSimpleVertexShader12(),
			VertexShaderCache::GetSimpleInputLayout12(),
			D3D12_SHADER_BYTECODE(),
			1.0f,
			0,
			DXGI_FORMAT_B8G8R8A8_UNORM,
			false,
			false /* Render target is not multisampled */
			);

		// Copy to staging buffer
		D3D12_BOX srcBox12 = CD3DX12_BOX(0, 0, 0, words_per_row, num_blocks_y, 1);

		D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
		dstLocation.pResource = m_outStage12;
		dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dstLocation.PlacedFootprint.Offset = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
		dstLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		dstLocation.PlacedFootprint.Footprint.Width = EFB_WIDTH * 4;
		dstLocation.PlacedFootprint.Footprint.Height = EFB_HEIGHT / 4;
		dstLocation.PlacedFootprint.Footprint.Depth = 1;
		dstLocation.PlacedFootprint.Footprint.RowPitch = dstLocation.PlacedFootprint.Footprint.Width * 4 /* width * 32bpp */;
		dstLocation.PlacedFootprint.Footprint.RowPitch = (dstLocation.PlacedFootprint.Footprint.RowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);

		D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
		srcLocation.pResource = m_out12;
		srcLocation.SubresourceIndex = 0;
		srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

		D3D::ResourceBarrier(D3D::current_command_list, m_out12, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
		D3D::current_command_list->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, &srcBox12);

		D3D::command_list_mgr->ExecuteQueuedWork(true);
		
		// Transfer staging buffer to GameCube/Wii RAM

		u8* src12 = (u8*)m_outStage12data + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
		u32 readStride = std::min(bytes_per_row, dstLocation.PlacedFootprint.Footprint.RowPitch);
		for (unsigned int y = 0; y < num_blocks_y; ++y)
		{
			memcpy(dst, src12, readStride);

			dst += memory_stride;

			src12 += dstLocation.PlacedFootprint.Footprint.RowPitch;
		}
	}

	// Restores proper viewport/scissor settings.
	g_renderer->RestoreAPIState();
	
	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE );
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());
}

D3D12_SHADER_BYTECODE PSTextureEncoder::SetStaticShader12(unsigned int dstFormat, PEControl::PixelFormat srcFormat,
	bool isIntensity, bool scaleByHalf)
{
	size_t fetchNum = static_cast<size_t>(srcFormat);
	size_t scaledFetchNum = scaleByHalf ? 1 : 0;
	size_t intensityNum = isIntensity ? 1 : 0;
	size_t generatorNum = dstFormat;

	ComboKey key = MakeComboKey(dstFormat, srcFormat, isIntensity, scaleByHalf);

	ComboMap12::iterator it = m_staticShaders12.find(key);
	if (it == m_staticShaders12.end())
	{
		INFO_LOG(VIDEO, "Compiling efb encoding shader for dstFormat 0x%X, srcFormat %d, isIntensity %d, scaleByHalf %d",
			dstFormat, static_cast<int>(srcFormat), isIntensity ? 1 : 0, scaleByHalf ? 1 : 0);

		u32 format = dstFormat;

		if (srcFormat == PEControl::Z24)
		{
			format |= _GX_TF_ZTF;
			if (dstFormat == 11)
				format = GX_TF_Z16;
			else if (format < GX_TF_Z8 || format > GX_TF_Z24X8)
				format |= _GX_TF_CTF;
		}
		else
		{
			if (dstFormat > GX_TF_RGBA8 || (dstFormat < GX_TF_RGB565 && !isIntensity))
				format |= _GX_TF_CTF;
		}

		D3DBlob* bytecode = nullptr;
		const char* shader = TextureConversionShader::GenerateEncodingShader(format, API_D3D11);
		if (!D3D::CompilePixelShader(shader, &bytecode))
		{
			WARN_LOG(VIDEO, "EFB encoder shader for dstFormat 0x%X, srcFormat %d, isIntensity %d, scaleByHalf %d failed to compile",
				dstFormat, static_cast<int>(srcFormat), isIntensity ? 1 : 0, scaleByHalf ? 1 : 0);
			m_staticShaders12[key] = {};
			return {};
		}

		D3D12_SHADER_BYTECODE newShader = {
			bytecode->Data(),
			bytecode->Size()
		};

		it = m_staticShaders12.insert(std::make_pair(key, newShader)).first;
		
		// Keep track of the D3DBlobs, so we can free them upon shutdown.
		m_staticShaders12blobs.push_back(bytecode);
	}

	return it->second;
}

}
