// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <unordered_map>

#include "VideoCommon/HullDomainShaderGen.h"

namespace DX11
{

	class HullDomainShaderCache
	{
	public:
		static void Init();
		static void Clear();
		static void Shutdown();
		static void SetShader(const XFMemory &xfr, const  PrimitiveType primitiveType, const u32 components);
		static std::tuple<ID3D11Buffer*, UINT, UINT> GetConstantBuffer();
		static ID3D11HullShader* GetActiveHullShader() { return s_last_entry != nullptr ? s_last_entry->hullshader.get() : nullptr; }
		static ID3D11DomainShader* GetActiveDomainShader() { return s_last_entry != nullptr ? s_last_entry->domainshader.get() : nullptr; }

	private:
		struct HDCacheEntry
		{
			D3D::HullShaderPtr hullshader;
			D3D::DomainShaderPtr domainshader;
			std::string code;
			bool compiled;
			HDCacheEntry() : hullshader(nullptr), domainshader(nullptr), compiled(false) {}
			void Destroy()
			{
				hullshader.reset();
				domainshader.reset();
			}
		};
		typedef std::unordered_map<HullDomainShaderUid, HDCacheEntry, HullDomainShaderUid::ShaderUidHasher> HDCache;

		static HDCache s_hulldomain_shaders;
		static const HDCacheEntry* s_last_entry;
		static HullDomainShaderUid s_last_uid;
		static HullDomainShaderUid s_external_last_uid;
	};

}  // namespace DX11
