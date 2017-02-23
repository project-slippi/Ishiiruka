// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include <atomic>
#include <unordered_map>
#include "VideoBackends/DX11/D3DBase.h"
#include "VideoBackends/DX11/D3DBlob.h"

#include "VideoCommon/ObjectUsageProfiler.h"
#include "VideoCommon/VertexShaderGen.h"

namespace DX11 {

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
	static ID3D11VertexShader* GetActiveShader()
	{
		return s_last_entry->shader.get();
	}
	static D3DBlob const& GetActiveShaderBytecode()
	{
		return s_last_entry->bytecode;
	}
	static D3D::BufferDescriptor GetConstantBuffer();

	static ID3D11VertexShader* GetSimpleVertexShader();
	static ID3D11VertexShader* GetClearVertexShader();
	static ID3D11InputLayout* GetSimpleInputLayout();
	static ID3D11InputLayout* GetClearInputLayout();

	static void InsertByteCode(const VertexShaderUid &uid, D3DBlob&& bcodeblob);

private:
	struct VSCacheEntry
	{
		D3D::VertexShaderPtr shader;
		D3DBlob bytecode; // needed to initialize the input layout

		std::string code;
		bool compiled;
		std::atomic_flag initialized;
		VSCacheEntry() : compiled(false)
		{
			initialized.clear();
		}
		void SetByteCode(D3DBlob&& blob)
		{
			bytecode = std::move(blob);

		}
		void Destroy()
		{
			shader.reset();
			bytecode = nullptr;
		}
	};
	static inline void PushByteCode(D3DBlob&& bcodeblob, VSCacheEntry* entry);
	typedef ObjectUsageProfiler<VertexShaderUid, pKey_t, VSCacheEntry, VertexShaderUid::ShaderUidHasher> VSCache;

	static VSCache* s_vshaders;
	static const VSCacheEntry* s_last_entry;
	static VertexShaderUid s_last_uid;
	static VertexShaderUid s_external_last_uid;
	static void CompileVShader(const VertexShaderUid& uid, bool ongputhread);
};

}  // namespace DX11