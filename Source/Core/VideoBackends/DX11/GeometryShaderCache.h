// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <unordered_map>

#include "VideoCommon/GeometryShaderGen.h"

namespace DX11
{

	class GeometryShaderCache
	{
	public:
		static void Init();
		static void Clear();
		static void Shutdown();
		static void PrepareShader(
			u32 primitive_type,
			const XFMemory &xfr,
			const u32 components,
			bool ongputhread);
		static bool TestShader();		
		static void InsertByteCode(const GeometryShaderUid &uid, const void* bytecode, unsigned int bytecodelen);

		static ID3D11GeometryShader* GeometryShaderCache::GetClearGeometryShader();
		static ID3D11GeometryShader* GeometryShaderCache::GetCopyGeometryShader();

		static ID3D11GeometryShader* GetActiveShader() { return s_last_entry->shader.get(); }
		static std::tuple<ID3D11Buffer*, UINT, UINT> GetConstantBuffer();

	private:
		struct GSCacheEntry
		{
			D3D::GeometryShaderPtr shader;
			bool compiled;
			std::atomic_flag initialized;
			std::string code;

			GSCacheEntry() : shader(nullptr), compiled(false) { initialized.clear(); }
			void Destroy() { shader.reset(); }
		};
		static inline void PushByteCode(const void* bytecode, unsigned int bytecodelen, GSCacheEntry* entry);
		typedef std::unordered_map<GeometryShaderUid, GSCacheEntry, GeometryShaderUid::ShaderUidHasher> GSCache;

		static GSCache s_geometry_shaders;
		static const GSCacheEntry* s_last_entry;
		static GeometryShaderUid s_last_uid;
		static GeometryShaderUid s_external_last_uid;
		static const GSCacheEntry s_pass_entry;

		static UidChecker<GeometryShaderUid, ShaderCode> geometry_uid_checker;
	};

}  // namespace DX11
