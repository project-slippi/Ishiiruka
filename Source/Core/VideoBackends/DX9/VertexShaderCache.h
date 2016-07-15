// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include <atomic>
#include <unordered_map>
#include <string>

#include "VideoBackends/DX9/D3DBase.h"

#include "VideoCommon/ObjectUsageProfiler.h"
#include "VideoCommon/VertexShaderGen.h"

namespace DX9
{

class VertexShaderCache
{
private:
	struct VSCacheEntry
	{
		LPDIRECT3DVERTEXSHADER9 shader;
		bool compiled;
		std::atomic_flag initialized;
		VSCacheEntry() : shader(NULL), compiled(false)
		{
			initialized.clear();
		}
		void Destroy()
		{
			if (shader)
				shader->Release();
			shader = NULL;
		}
	};

	static inline void PushByteCode(const VertexShaderUid &uid, const u8 *bytecode, int bytecodelen, VSCacheEntry* entry);
	static ObjectUsageProfiler<VertexShaderUid, pKey_t, VSCacheEntry, VertexShaderUid::ShaderUidHasher>* s_vshaders;
	static const VSCacheEntry *s_last_entry;
	static VertexShaderUid s_last_uid;
	static VertexShaderUid s_external_last_uid;

	static void Clear();
	static void CompileVShader(const VertexShaderUid& uid, bool ongputhread);
public:
	static void Init();
	static void Shutdown();
	static void PrepareShader(u32 components, const XFMemory &xfr, const BPMemory &bpm, bool ongputhread);
	static bool TestShader();
	static LPDIRECT3DVERTEXSHADER9 GetSimpleVertexShader(int level);
	static LPDIRECT3DVERTEXSHADER9 GetClearVertexShader();
	static void InsertByteCode(const VertexShaderUid &uid, const u8 *bytecode, int bytecodelen);
};

}  // namespace DX9