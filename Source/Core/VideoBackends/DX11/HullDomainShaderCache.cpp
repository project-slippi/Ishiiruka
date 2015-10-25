// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/FileUtil.h"
#include "Common/LinearDiskCache.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"

#include "VideoBackends/DX11/D3DBase.h"
#include "VideoBackends/DX11/D3DPtr.h"
#include "VideoBackends/DX11/D3DShader.h"
#include "VideoBackends/DX11/D3DUtil.h"
#include "VideoBackends/DX11/FramebufferManager.h"
#include "VideoBackends/DX11/HullDomainShaderCache.h"
#include "VideoBackends/DX11/Globals.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/HullDomainShaderManager.h"
#include "VideoCommon/HLSLCompiler.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{

	HullDomainShaderCache::HDCache HullDomainShaderCache::s_hulldomain_shaders;
	const HullDomainShaderCache::HDCacheEntry* HullDomainShaderCache::s_last_entry;
	HullDomainShaderUid HullDomainShaderCache::s_last_uid;
	HullDomainShaderUid HullDomainShaderCache::s_external_last_uid;
	static HLSLAsyncCompiler *s_compiler;

	std::unique_ptr<D3D::ConstantStreamBuffer> hdscbuf;
	static UINT s_hdscbuf_offset = 0;
	static UINT s_hdscbuf_size = 0;

	std::tuple<ID3D11Buffer*, UINT, UINT>  HullDomainShaderCache::GetConstantBuffer()
	{
		if (HullDomainShaderManager::IsDirty())
		{
			s_hdscbuf_size = sizeof(HullDomainShaderConstants);
			s_hdscbuf_offset = hdscbuf->AppendData((void*)&HullDomainShaderManager::constants, s_hdscbuf_size);
			HullDomainShaderManager::Clear();
			ADDSTAT(stats.thisFrame.bytesUniformStreamed, s_hdscbuf_size);
			s_hdscbuf_size = (UINT)(((s_hdscbuf_size + 255) & (~255)) / 16);
		}
		return std::tuple<ID3D11Buffer*, UINT, UINT>(hdscbuf->GetBuffer(), s_hdscbuf_offset, s_hdscbuf_size);
	}

	void HullDomainShaderCache::Init()
	{
		bool use_partial_buffer_update = D3D::SupportPartialContantBufferUpdate();
		u32 gbsize = use_partial_buffer_update ? 4 * 1024 * 1024 : ROUND_UP(sizeof(HullDomainShaderConstants), 16); // must be a multiple of 16
		hdscbuf.reset(new D3D::ConstantStreamBuffer(gbsize));
		ID3D11Buffer* buf = hdscbuf->GetBuffer();
		CHECK(buf != nullptr, "Create Hull Domain shader constant buffer (size=%u)", gbsize);
		D3D::SetDebugObjectName(buf, "Hull Domain shader constant buffer used to emulate the GX pipeline");
		Clear();
	}

	// ONLY to be used during shutdown.
	void HullDomainShaderCache::Clear()
	{
		for (auto& iter : s_hulldomain_shaders)
			iter.second.Destroy();
		s_hulldomain_shaders.clear();
		s_last_entry = nullptr;
	}

	void HullDomainShaderCache::Shutdown()
	{
		hdscbuf.reset();
		Clear();
	}

	void HullDomainShaderCache::SetShader(const XFMemory &xfr, const PrimitiveType primitiveType, const u32 components)
	{
		if (primitiveType != PrimitiveType::PRIMITIVE_TRIANGLES || !g_ActiveConfig.TessellationEnabled() || !g_ActiveConfig.PixelLightingEnabled(xfr, components))
		{
			s_last_entry = nullptr;
			return;
		}
		HullDomainShaderUid uid;
		GetHullDomainShaderUid(uid, API_D3D11, xfr, components);
		// Check if the shader is already set
		if (s_last_entry)
		{
			if (uid == s_last_uid)
			{
				return;
			}
		}
		s_last_uid = uid;
		GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
		HDCacheEntry* entry = &s_hulldomain_shaders[uid];
		s_last_entry = entry;
		if (!entry->compiled)
		{
			// Need to compile a new shader
			ShaderCode code;
			GenerateHullDomainShaderCode(code, API_D3D11, xfr, components);
			// Fail Silently if tessellation configuration is not supported
			entry->hullshader = D3D::CompileAndCreateHullShader(code.GetBuffer(), nullptr, "HS_TFO", false);
			entry->domainshader = D3D::CompileAndCreateDomainShader(code.GetBuffer(), nullptr, "DS_TFO", false);
			entry->compiled = true;
		}
	}
}  // DX11
