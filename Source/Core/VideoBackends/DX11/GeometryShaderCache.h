// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <unordered_map>

#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/ObjectUsageProfiler.h"

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

	static ID3D11GeometryShader* GetClearGeometryShader();
	static ID3D11GeometryShader* GetCopyGeometryShader();

	static ID3D11GeometryShader* GetActiveShader()
	{
		return s_last_entry->shader.get();
	}
	static D3D::BufferDescriptor GetConstantBuffer();

private:
	struct GSCacheEntry
	{
		D3D::GeometryShaderPtr shader;
		bool compiled;
		std::atomic_flag initialized;

		GSCacheEntry() : shader(nullptr), compiled(false)
		{
			initialized.clear();
		}
		void Destroy()
		{
			shader.reset();
		}
	};
	static inline void PushByteCode(const void* bytecode, unsigned int bytecodelen, GSCacheEntry* entry);
	typedef ObjectUsageProfiler<GeometryShaderUid, pKey_t, GSCacheEntry, GeometryShaderUid::ShaderUidHasher> GSCache;

	static GSCache* s_geometry_shaders;
	static const GSCacheEntry* s_last_entry;
	static GeometryShaderUid s_last_uid;
	static GeometryShaderUid s_external_last_uid;
	static const GSCacheEntry s_pass_entry;
	static void CompileGShader(const GeometryShaderUid& uid, bool ongputhread);
};

}  // namespace DX11
