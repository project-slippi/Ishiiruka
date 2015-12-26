// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <unordered_map>

#include "VideoCommon/GeometryShaderGen.h"

namespace DX12
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
	static bool InsertByteCode(const GeometryShaderUid &uid, const void* bytecode, unsigned int bytecodelen);

	static D3D12_SHADER_BYTECODE GeometryShaderCache::GetClearGeometryShader12();
	static D3D12_SHADER_BYTECODE GeometryShaderCache::GetCopyGeometryShader12();

	static D3D12_SHADER_BYTECODE GetActiveShader12() { return s_last_entry->shader12; }
	static GeometryShaderUid GetActiveShaderUid12() { return s_last_uid; }
	static D3D12_SHADER_BYTECODE GetShaderFromUid(GeometryShaderUid uid)
	{
		// This function is only called when repopulating the PSO cache from disk.
		// In this case, we know the shader already exists on disk, and has been loaded
		// into memory, thus we don't need to handle the failure case.

		GSCache::iterator iter;
		iter = s_geometry_shaders.find(uid);
		if (iter != s_geometry_shaders.end())
		{
			const GSCacheEntry &entry = iter->second;
			return entry.shader12;
		}

		return D3D12_SHADER_BYTECODE();
	}


	static void GetConstantBuffer12(); // This call on D3D12 actually sets the constant buffer, no need to return it.

	static D3D12_PRIMITIVE_TOPOLOGY_TYPE GetCurrentPrimitiveTopology();

private:
	struct GSCacheEntry
	{
		D3D12_SHADER_BYTECODE shader12;
		bool compiled;
		std::atomic_flag initialized;
		std::string code;

		GSCacheEntry() : shader12({}), compiled(false) { initialized.clear(); }
		void Destroy() { SAFE_DELETE(shader12.pShaderBytecode); }
	};

	static inline void PushByteCode(const void* bytecode, unsigned int bytecodelen, GSCacheEntry* entry);
	typedef std::unordered_map<GeometryShaderUid, GSCacheEntry, GeometryShaderUid::ShaderUidHasher> GSCache;

	static GSCache s_geometry_shaders;
	static const GSCacheEntry* s_last_entry;
	static GeometryShaderUid s_last_uid;
	static GeometryShaderUid s_external_last_uid;
	static GSCacheEntry s_pass_entry;

	static UidChecker<GeometryShaderUid, ShaderCode> geometry_uid_checker;
};

}  // namespace DX12
