// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <map>

#include "VideoCommon/PixelShaderGen.h"

enum DSTALPHA_MODE;

namespace DX12
{

class PixelShaderCache
{
public:
	static void Init();
	static void Clear();
	static void Shutdown();
	static void PrepareShader(
		DSTALPHA_MODE dstAlphaMode,
		u32 componets,
		const XFMemory &xfr,
		const BPMemory &bpm, bool ongputhread);
	static bool TestShader();
	static bool InsertByteCode(const PixelShaderUid &uid, const void* bytecode, unsigned int bytecodelen);

	static D3D12_SHADER_BYTECODE GetActiveShader12() { return s_last_entry->shaderDesc; }
	static PixelShaderUid GetActiveShaderUid12() { return s_last_uid; }
	static D3D12_SHADER_BYTECODE GetShaderFromUid(PixelShaderUid uid)
	{
		// This function is only called when repopulating the PSO cache from disk.
		// In this case, we know the shader already exists on disk, and has been loaded
		// into memory, thus we don't need to handle the failure case.

		PSCache::iterator iter;
		iter = s_pixel_shaders.find(uid);
		if (iter != s_pixel_shaders.end())
		{
			const PSCacheEntry &entry = iter->second;
			return entry.shaderDesc;
		}

		return D3D12_SHADER_BYTECODE();
	}

	static void GetConstantBuffer12(); // Does not return a buffer, but actually binds the constant data.

	static D3D12_SHADER_BYTECODE GetColorMatrixProgram12(bool multisampled);
	static D3D12_SHADER_BYTECODE GetColorCopyProgram12(bool multisampled);
	static D3D12_SHADER_BYTECODE GetDepthMatrixProgram12(bool multisampled);
	static D3D12_SHADER_BYTECODE GetDepthCopyProgram12(bool multisampled);
	static D3D12_SHADER_BYTECODE GetClearProgram12();
	static D3D12_SHADER_BYTECODE GetAnaglyphProgram12();
	static D3D12_SHADER_BYTECODE ReinterpRGBA6ToRGB812(bool multisampled);
	static D3D12_SHADER_BYTECODE ReinterpRGB8ToRGBA612(bool multisampled);

	static void InvalidateMSAAShaders();

private:
	struct PSCacheEntry
	{
		D3D12_SHADER_BYTECODE shaderDesc;
		bool compiled;
		std::atomic_flag initialized;
		std::string code;

		PSCacheEntry() : compiled(false), shaderDesc({})
		{
			initialized.clear();
		}
		void Destroy() {
			SAFE_DELETE(shaderDesc.pShaderBytecode);
		}
	};
	static void PushByteCode(PSCacheEntry* entry, const void* bytecode, unsigned int bytecodelen);
	typedef std::unordered_map<PixelShaderUid, PSCacheEntry, PixelShaderUid::ShaderUidHasher> PSCache;

	static PSCache s_pixel_shaders;
	static const PSCacheEntry* s_last_entry;
	static PixelShaderUid s_last_uid;
	static PixelShaderUid s_external_last_uid;
	static UidChecker<PixelShaderUid,ShaderCode> s_pixel_uid_checker;
};

}  // namespace DX12
