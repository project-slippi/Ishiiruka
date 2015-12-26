// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>

#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DBlob.h"

#include "VideoCommon/VertexShaderGen.h"

namespace DX12 {

class VertexShaderCache
{
public:
	static void Init();
	static void Clear();
	static void Shutdown();
	static void PrepareShader(u32 components,
		const XFMemory &xfr,
		const BPMemory &bpm, bool ongputhread);
	static bool TestShader();

	static D3D12_SHADER_BYTECODE GetActiveShader12() { return s_last_entry->shader12; }
	static VertexShaderUid GetActiveShaderUid12() { return s_last_uid; }
	static D3D12_SHADER_BYTECODE GetShaderFromUid(VertexShaderUid uid)
	{
		// This function is only called when repopulating the PSO cache from disk.
		// In this case, we know the shader already exists on disk, and has been loaded
		// into memory, thus we don't need to handle the failure case.

		VSCache::iterator iter;
		iter = s_vshaders.find(uid);
		if (iter != s_vshaders.end())
		{
			const VSCacheEntry &entry = iter->second;
			return entry.shader12;
		}

		return D3D12_SHADER_BYTECODE();
	}

	static void GetConstantBuffer12();

	static D3D12_SHADER_BYTECODE GetSimpleVertexShader12();
	static D3D12_SHADER_BYTECODE GetClearVertexShader12();
	static D3D12_INPUT_LAYOUT_DESC GetSimpleInputLayout12();
	static D3D12_INPUT_LAYOUT_DESC GetClearInputLayout12();

	static bool VertexShaderCache::InsertByteCode(const VertexShaderUid &uid, D3DBlob* bcodeblob);

private:
	struct VSCacheEntry
	{
		D3D12_SHADER_BYTECODE shader12;

		D3DBlob* bytecode; // needed to initialize the input layout

		std::string code;
		bool compiled;
		std::atomic_flag initialized;
		VSCacheEntry() :
			bytecode(nullptr), shader12({}), compiled(false)
		{
			initialized.clear();
		}
		void SetByteCode(D3DBlob* blob)
		{
			SAFE_RELEASE(bytecode);
			bytecode = blob;
			blob->AddRef();
		}
		void Destroy()
		{
			SAFE_RELEASE(bytecode);
		}
	};
	static void PushByteCode(VertexShaderCache::VSCacheEntry* entry, D3DBlob* bcodeblob);
	typedef std::unordered_map<VertexShaderUid, VSCacheEntry, VertexShaderUid::ShaderUidHasher> VSCache;

	static VSCache s_vshaders;
	static const VSCacheEntry* s_last_entry;
	static VertexShaderUid s_last_uid;
	static VertexShaderUid s_external_last_uid;
	static UidChecker<VertexShaderUid, ShaderCode> s_vertex_uid_checker;
};

}  // namespace DX12
