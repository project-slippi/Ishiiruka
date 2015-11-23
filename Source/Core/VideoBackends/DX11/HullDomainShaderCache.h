// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <unordered_map>

#include "VideoCommon/TessellationShaderGen.h"

namespace DX11
{

	class HullDomainShaderCache
	{
	public:
		static void Init();
		static void Clear();
		static void Shutdown();
		static void PrepareShader(
			const XFMemory &xfr,
			const BPMemory &bpm,
			const PrimitiveType primitiveType,
			const u32 components,
			bool ongputhread);
		static bool TestShader();
		static void InsertByteCode(
			const TessellationShaderUid &uid,
			const void* bytecode,
			unsigned int bytecodelen, bool isdomain);
		static D3D::BufferDescriptor GetConstantBuffer();
		static ID3D11HullShader* GetActiveHullShader() { return s_last_entry != nullptr && s_last_entry->dcompiled && s_last_entry->hcompiled ? s_last_entry->hullshader.get() : nullptr; }
		static ID3D11DomainShader* GetActiveDomainShader() { return s_last_entry != nullptr && s_last_entry->dcompiled && s_last_entry->hcompiled ? s_last_entry->domainshader.get() : nullptr; }

	private:
		struct HDCacheEntry
		{
			D3D::HullShaderPtr hullshader;
			D3D::DomainShaderPtr domainshader;
			std::string code;
			bool hcompiled;
			bool dcompiled;
			std::atomic_flag initialized;
			HDCacheEntry() : hullshader(nullptr), domainshader(nullptr), hcompiled(false), dcompiled(false) { initialized.clear(); }
			void Destroy()
			{
				hullshader.reset();
				domainshader.reset();
			}
		};
		static inline void PushByteCode(
			const void* bytecode,
			unsigned int bytecodelen,
			HDCacheEntry* entry, bool isdomain);
		typedef std::unordered_map<TessellationShaderUid, HDCacheEntry, TessellationShaderUid::ShaderUidHasher> HDCache;

		static HDCache s_hulldomain_shaders;
		static const HDCacheEntry* s_last_entry;
		static TessellationShaderUid s_last_uid;
		static TessellationShaderUid s_external_last_uid;

		static UidChecker<TessellationShaderUid, ShaderCode> HullDomain_uid_checker;
	};

}  // namespace DX11
