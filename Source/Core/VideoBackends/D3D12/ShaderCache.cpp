// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#include <unordered_map>

#include "Common/LinearDiskCache.h"

#include "Core/ConfigManager.h"
#include "Core/Host.h"

#include "VideoBackends/D3D12/D3DBlob.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DShader.h"
#include "VideoBackends/D3D12/ShaderCache.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/HLSLCompiler.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/ObjectUsageProfiler.h"

namespace DX12
{

// Primitive topology type is always triangle, unless the GS stage is used. This is consumed
// by the PSO created in Renderer::ApplyState.
static D3D12_PRIMITIVE_TOPOLOGY_TYPE s_current_primitive_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
struct ByteCodeCacheEntry
{
	D3D12_SHADER_BYTECODE m_shader_bytecode;
	bool m_compiled;
	std::atomic_flag m_initialized;

	ByteCodeCacheEntry() : m_compiled(false), m_shader_bytecode({})
	{
		m_initialized.clear();
	}
	void Release()
	{
		if (m_shader_bytecode.pShaderBytecode)
			delete[] m_shader_bytecode.pShaderBytecode;
		m_shader_bytecode.pShaderBytecode = nullptr;
	}
};

static ByteCodeCacheEntry s_pass_entry;

using GsBytecodeCache = ObjectUsageProfiler<GeometryShaderUid, pKey_t, ByteCodeCacheEntry, GeometryShaderUid::ShaderUidHasher>;
using PsBytecodeCache = ObjectUsageProfiler<PixelShaderUid, pKey_t, ByteCodeCacheEntry, PixelShaderUid::ShaderUidHasher>;
using VsBytecodeCache = ObjectUsageProfiler<VertexShaderUid, pKey_t, ByteCodeCacheEntry, VertexShaderUid::ShaderUidHasher>;
using TsBytecodeCache = ObjectUsageProfiler<TessellationShaderUid, pKey_t, std::pair<ByteCodeCacheEntry, ByteCodeCacheEntry>, TessellationShaderUid::ShaderUidHasher>;

GsBytecodeCache* gs_bytecode_cache;
PsBytecodeCache* ps_bytecode_cache;
VsBytecodeCache* vs_bytecode_cache;
TsBytecodeCache* ts_bytecode_cache;

static std::vector<D3DBlob*> s_shader_blob_list;

static LinearDiskCache<TessellationShaderUid, u8> s_hs_disk_cache;
static LinearDiskCache<TessellationShaderUid, u8> s_ds_disk_cache;
static LinearDiskCache<GeometryShaderUid, u8> s_gs_disk_cache;
static LinearDiskCache<PixelShaderUid, u8> s_ps_disk_cache;
static LinearDiskCache<VertexShaderUid, u8> s_vs_disk_cache;

static ByteCodeCacheEntry* s_last_domain_shader_bytecode;
static ByteCodeCacheEntry* s_last_hull_shader_bytecode;
static ByteCodeCacheEntry* s_last_geometry_shader_bytecode;
static ByteCodeCacheEntry* s_last_pixel_shader_bytecode;
static ByteCodeCacheEntry* s_last_vertex_shader_bytecode;

static GeometryShaderUid s_last_geometry_shader_uid;
static PixelShaderUid s_last_pixel_shader_uid;
static VertexShaderUid s_last_vertex_shader_uid;
static TessellationShaderUid s_last_tessellation_shader_uid;

static GeometryShaderUid s_last_cpu_geometry_shader_uid;
static PixelShaderUid s_last_cpu_pixel_shader_uid;
static VertexShaderUid s_last_cpu_vertex_shader_uid;
static TessellationShaderUid s_last_cpu_tessellation_shader_uid;

static HLSLAsyncCompiler *s_compiler;
static Common::SpinLock<true> s_shaders_lock;

template<typename UidType, typename ShaderCacheType, ShaderCacheType** cache>
class ShaderCacheInserter final : public LinearDiskCacheReader<UidType, u8>
{
public:
	void Read(const UidType &key, const u8* value, u32 value_size)
	{
		D3DBlob* blob = new D3DBlob(value_size, value);
		ShaderCache::InsertByteCode<UidType, ShaderCacheType>(key, *cache, blob);
	}
};

class DShaderCacheInserter final : public LinearDiskCacheReader<TessellationShaderUid, u8>
{
public:
	void Read(const TessellationShaderUid &key, const u8* value, u32 value_size)
	{
		D3DBlob* blob = new D3DBlob(value_size, value);
		ShaderCache::InsertDSByteCode(key, blob);
	}
};

class HShaderCacheInserter final : public LinearDiskCacheReader<TessellationShaderUid, u8>
{
public:
	void Read(const TessellationShaderUid &key, const u8* value, u32 value_size)
	{
		D3DBlob* blob = new D3DBlob(value_size, value);
		ShaderCache::InsertHSByteCode(key, blob);
	}
};

void ShaderCache::Init()
{
	s_compiler = &HLSLAsyncCompiler::getInstance();
	s_shaders_lock.unlock();
	s_pass_entry.m_compiled = true;
	s_pass_entry.m_initialized.test_and_set();
	// This class intentionally shares its shader cache files with DX11, as the shaders are (right now) identical.
	// Reduces unnecessary compilation when switching between APIs.
	s_last_domain_shader_bytecode = &s_pass_entry;
	s_last_hull_shader_bytecode = &s_pass_entry;
	s_last_geometry_shader_bytecode = &s_pass_entry;
	s_last_pixel_shader_bytecode = nullptr;
	s_last_vertex_shader_bytecode = nullptr;

	s_last_geometry_shader_uid = {};
	s_last_pixel_shader_uid = {};
	s_last_vertex_shader_uid = {};
	s_last_tessellation_shader_uid = {};

	s_last_cpu_geometry_shader_uid = {};
	s_last_cpu_pixel_shader_uid = {};
	s_last_cpu_vertex_shader_uid = {};
	s_last_cpu_tessellation_shader_uid = {};

	// Ensure shader cache directory exists..
	std::string shader_cache_path = File::GetUserPath(D_SHADERCACHE_IDX);

	if (!File::Exists(shader_cache_path))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

	std::string title_unique_id = SConfig::GetInstance().m_strGameID;

	std::string ds_cache_filename = StringFromFormat("%sIDX11-%s-ds.cache", shader_cache_path.c_str(), title_unique_id.c_str());
	std::string hs_cache_filename = StringFromFormat("%sIDX11-%s-hs.cache", shader_cache_path.c_str(), title_unique_id.c_str());
	std::string gs_cache_filename = StringFromFormat("%sIDX11-%s-gs.cache", shader_cache_path.c_str(), title_unique_id.c_str());
	std::string ps_cache_filename = StringFromFormat("%sIDX11-%s-ps.cache", shader_cache_path.c_str(), title_unique_id.c_str());
	std::string vs_cache_filename = StringFromFormat("%sIDX11-%s-vs.cache", shader_cache_path.c_str(), title_unique_id.c_str());

	pKey_t gameid = (pKey_t)GetMurmurHash3(reinterpret_cast<const u8*>(SConfig::GetInstance().m_strGameID.data()), (u32)SConfig::GetInstance().m_strGameID.size(), 0);

	vs_bytecode_cache = VsBytecodeCache::Create(
		gameid,
		VERTEXSHADERGEN_UID_VERSION,
		"Ishiiruka.vs",
		StringFromFormat("%s.vs", title_unique_id.c_str())
	);

	ps_bytecode_cache = PsBytecodeCache::Create(
		gameid,
		PIXELSHADERGEN_UID_VERSION,
		"Ishiiruka.ps",
		StringFromFormat("%s.ps", title_unique_id.c_str())
	);

	gs_bytecode_cache = GsBytecodeCache::Create(
		gameid,
		GEOMETRYSHADERGEN_UID_VERSION,
		"Ishiiruka.gs",
		StringFromFormat("%s.gs", title_unique_id.c_str())
	);

	ts_bytecode_cache = TsBytecodeCache::Create(
		gameid,
		TESSELLATIONSHADERGEN_UID_VERSION,
		"Ishiiruka.ts",
		StringFromFormat("%s.ts", title_unique_id.c_str())
	);

	DShaderCacheInserter ds_inserter;
	s_ds_disk_cache.OpenAndRead(ds_cache_filename, ds_inserter);

	HShaderCacheInserter hs_inserter;
	s_hs_disk_cache.OpenAndRead(hs_cache_filename, hs_inserter);

	ShaderCacheInserter<GeometryShaderUid, GsBytecodeCache, &gs_bytecode_cache> gs_inserter;
	s_gs_disk_cache.OpenAndRead(gs_cache_filename, gs_inserter);

	ShaderCacheInserter<PixelShaderUid, PsBytecodeCache, &ps_bytecode_cache> ps_inserter;
	s_ps_disk_cache.OpenAndRead(ps_cache_filename, ps_inserter);

	ShaderCacheInserter<VertexShaderUid, VsBytecodeCache, &vs_bytecode_cache> vs_inserter;
	s_vs_disk_cache.OpenAndRead(vs_cache_filename, vs_inserter);

	// Clear out disk cache when debugging shaders to ensure stale ones don't stick around..
	SETSTAT(stats.numGeometryShadersAlive, static_cast<int>(gs_bytecode_cache->size()));
	SETSTAT(stats.numGeometryShadersCreated, 0);
	SETSTAT(stats.numPixelShadersAlive, static_cast<int>(ps_bytecode_cache->size()));
	SETSTAT(stats.numPixelShadersCreated, 0);
	SETSTAT(stats.numVertexShadersAlive, static_cast<int>(vs_bytecode_cache->size()));
	SETSTAT(stats.numVertexShadersCreated, 0);
	if (g_ActiveConfig.bCompileShaderOnStartup)
	{
		size_t shader_count = 0;
		ps_bytecode_cache->ForEachMostUsedByCategory(gameid,
			[&](const PixelShaderUid& it, size_t total)
		{
			PixelShaderUid item = it;
			item.ClearHASH();
			item.CalculateUIDHash();
			HandlePSUIDChange(item, true);
			shader_count++;
			if ((shader_count & 7) == 0)
			{
				Host_UpdateTitle(StringFromFormat("Compiling Pixel Shaders %i %% (%i/%i)", (shader_count * 100) / total, shader_count, total));
				s_compiler->WaitForFinish();
			}
		},
			[](ByteCodeCacheEntry& entry)
		{
			return !entry.m_shader_bytecode.pShaderBytecode;
		}
		, true);
		shader_count = 0;
		vs_bytecode_cache->ForEachMostUsedByCategory(gameid,
			[&](const VertexShaderUid& it, size_t total)
		{
			VertexShaderUid item = it;
			item.ClearHASH();
			item.CalculateUIDHash();
			HandleVSUIDChange(item, true);
			shader_count++;
			if ((shader_count & 31) == 0)
			{
				Host_UpdateTitle(StringFromFormat("Compiling Vertex Shaders %i %% (%i/%i)", (shader_count * 100) / total, shader_count, total));
				s_compiler->WaitForFinish();
			}
		},
			[](ByteCodeCacheEntry& entry)
		{
			return !entry.m_shader_bytecode.pShaderBytecode;
		}
		, true);
		shader_count = 0;
		gs_bytecode_cache->ForEachMostUsedByCategory(gameid,
			[&](const GeometryShaderUid& it, size_t total)
		{
			GeometryShaderUid item = it;
			item.ClearHASH();
			item.CalculateUIDHash();
			HandleGSUIDChange(item, true);
			shader_count++;
			if ((shader_count & 7) == 0)
			{
				Host_UpdateTitle(StringFromFormat("Compiling Geometry Shaders %i %% (%i/%i)", (shader_count * 100) / total, shader_count, total));
				s_compiler->WaitForFinish();
			}
		},
			[](ByteCodeCacheEntry& entry)
		{
			return !entry.m_shader_bytecode.pShaderBytecode;
		}
		, true);
		shader_count = 0;
		ts_bytecode_cache->ForEachMostUsedByCategory(gameid,
			[&](const TessellationShaderUid& it, size_t total)
		{
			TessellationShaderUid item = it;
			item.ClearHASH();
			item.CalculateUIDHash();
			HandleTSUIDChange(item, true);
			shader_count++;
			if ((shader_count & 31) == 0)
			{
				Host_UpdateTitle(StringFromFormat("Compiling Tessellation Shaders %i %% (%i/%i)", (shader_count * 100) / total, shader_count, total));
				s_compiler->WaitForFinish();
			}
		},
			[](std::pair<ByteCodeCacheEntry, ByteCodeCacheEntry>& entry)
		{
			return !entry.first.m_shader_bytecode.pShaderBytecode;
		}
		, true);
		s_compiler->WaitForFinish();
	}
}

void ShaderCache::Clear()
{

}

void ShaderCache::Shutdown()
{
	if (s_compiler)
	{
		s_compiler->WaitForFinish();
	}

	for (auto& iter : s_shader_blob_list)
		SAFE_RELEASE(iter);

	s_shader_blob_list.clear();

	vs_bytecode_cache->Persist();
	delete vs_bytecode_cache;
	vs_bytecode_cache = nullptr;

	gs_bytecode_cache->Persist();
	delete gs_bytecode_cache;
	gs_bytecode_cache = nullptr;

	ts_bytecode_cache->Persist();
	delete ts_bytecode_cache;
	ts_bytecode_cache = nullptr;

	ps_bytecode_cache->Persist();
	delete ps_bytecode_cache;
	ps_bytecode_cache = nullptr;

	s_ds_disk_cache.Sync();
	s_ds_disk_cache.Close();
	s_hs_disk_cache.Sync();
	s_hs_disk_cache.Close();
	s_gs_disk_cache.Sync();
	s_gs_disk_cache.Close();
	s_ps_disk_cache.Sync();
	s_ps_disk_cache.Close();
	s_vs_disk_cache.Sync();
	s_vs_disk_cache.Close();
}

static void PushByteCode(ByteCodeCacheEntry* entry, D3DBlob* shaderBuffer)
{
	s_shader_blob_list.push_back(shaderBuffer);
	entry->m_shader_bytecode.pShaderBytecode = shaderBuffer->Data();
	entry->m_shader_bytecode.BytecodeLength = shaderBuffer->Size();
	entry->m_compiled = true;
}

void ShaderCache::SetCurrentPrimitiveTopology(u32 gs_primitive_type)
{
	switch (gs_primitive_type)
	{
	case PRIMITIVE_TRIANGLES:
		s_current_primitive_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		break;
	case PRIMITIVE_LINES:
		s_current_primitive_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		break;
	case PRIMITIVE_POINTS:
		s_current_primitive_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		break;
	default:
		CHECK(0, "Invalid primitive type.");
		break;
	}
}

void ShaderCache::HandleGSUIDChange(
	const GeometryShaderUid &gs_uid,
	bool on_gpu_thread)
{
	if (gs_uid.GetUidData().IsPassthrough())
	{
		s_last_geometry_shader_bytecode = &s_pass_entry;
		return;
	}

	s_shaders_lock.lock();
	ByteCodeCacheEntry* entry = &gs_bytecode_cache->GetOrAdd(gs_uid);
	s_shaders_lock.unlock();
	if (on_gpu_thread)
	{
		s_last_geometry_shader_bytecode = entry;
	}

	if (entry->m_initialized.test_and_set())
	{
		return;
	}

	// Need to compile a new shader
	ShaderCompilerWorkUnit *wunit = s_compiler->NewUnit(GEOMETRYSHADERGEN_BUFFERSIZE);
	wunit->GenerateCodeHandler = [gs_uid](ShaderCompilerWorkUnit* wunit)
	{
		ShaderCode code;
		code.SetBuffer(wunit->code.data());
		GenerateGeometryShaderCode(code, gs_uid.GetUidData(), API_D3D11);
		wunit->codesize = (u32)code.BufferSize();
	};

	wunit->entrypoint = "main";
	wunit->flags = D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_OPTIMIZATION_LEVEL3;
	wunit->target = D3D::GeometryShaderVersionString();

	wunit->ResultHandler = [gs_uid, entry](ShaderCompilerWorkUnit* wunit)
	{
		if (SUCCEEDED(wunit->cresult))
		{
			D3DBlob* shaderBuffer = new D3DBlob(wunit->shaderbytecode);
			s_gs_disk_cache.Append(gs_uid, shaderBuffer->Data(), shaderBuffer->Size());
			PushByteCode(entry, shaderBuffer);
			wunit->shaderbytecode->Release();
			wunit->shaderbytecode = nullptr;
			SETSTAT(stats.numGeometryShadersAlive, static_cast<int>(ps_bytecode_cache->size()));
			INCSTAT(stats.numGeometryShadersCreated);
		}
		else
		{
			static int num_failures = 0;
			std::string filename = StringFromFormat("%sbad_gs_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
			std::ofstream file;
			OpenFStream(file, filename, std::ios_base::out);
			file << ((const char *)wunit->code.data());
			file << ((const char *)wunit->error->GetBufferPointer());
			file.close();

			PanicAlert("Failed to compile geometry shader!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%s):\n%s",
				filename,
				D3D::GeometryShaderVersionString(),
				(char*)wunit->error->GetBufferPointer());
		}
	};
	s_compiler->CompileShaderAsync(wunit);
}

void ShaderCache::HandlePSUIDChange(
	const PixelShaderUid &ps_uid,
	bool on_gpu_thread)
{
	s_shaders_lock.lock();
	ByteCodeCacheEntry* entry = &ps_bytecode_cache->GetOrAdd(ps_uid);
	s_shaders_lock.unlock();
	if (on_gpu_thread)
	{
		s_last_pixel_shader_bytecode = entry;
	}
	if (entry->m_initialized.test_and_set())
	{
		return;
	}
	// Need to compile a new shader
	ShaderCompilerWorkUnit *wunit = s_compiler->NewUnit(PIXELSHADERGEN_BUFFERSIZE);
	wunit->GenerateCodeHandler = [ps_uid](ShaderCompilerWorkUnit* wunit)
	{
		ShaderCode code;
		code.SetBuffer(wunit->code.data());
		GeneratePixelShaderCodeD3D11(code, ps_uid.GetUidData());
		wunit->codesize = (u32)code.BufferSize();
	};

	wunit->entrypoint = "main";
	wunit->flags = D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_OPTIMIZATION_LEVEL3;
	wunit->target = D3D::PixelShaderVersionString();
	wunit->ResultHandler = [ps_uid, entry](ShaderCompilerWorkUnit* wunit)
	{
		if (SUCCEEDED(wunit->cresult))
		{
			D3DBlob* shaderBuffer = new D3DBlob(wunit->shaderbytecode);
			s_ps_disk_cache.Append(ps_uid, shaderBuffer->Data(), shaderBuffer->Size());
			PushByteCode(entry, shaderBuffer);
			wunit->shaderbytecode->Release();
			wunit->shaderbytecode = nullptr;
		}
		else
		{
			static int num_failures = 0;
			std::string filename = StringFromFormat("%sbad_ps_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
			std::ofstream file;
			OpenFStream(file, filename, std::ios_base::out);
			file << ((const char *)wunit->code.data());
			file << ((const char *)wunit->error->GetBufferPointer());
			file.close();

			PanicAlert("Failed to compile pixel shader!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%s):\n%s",
				filename,
				D3D::PixelShaderVersionString(),
				(char*)wunit->error->GetBufferPointer());
		}
	};
	s_compiler->CompileShaderAsync(wunit);
}

void ShaderCache::HandleVSUIDChange(
	const VertexShaderUid& vs_uid,
	bool on_gpu_thread)
{
	s_shaders_lock.lock();
	ByteCodeCacheEntry* entry = &vs_bytecode_cache->GetOrAdd(vs_uid);
	s_shaders_lock.unlock();
	if (on_gpu_thread)
	{
		s_last_vertex_shader_bytecode = entry;
	}
	// Compile only when we have a new instance
	if (entry->m_initialized.test_and_set())
	{
		return;
	}
	ShaderCompilerWorkUnit *wunit = s_compiler->NewUnit(VERTEXSHADERGEN_BUFFERSIZE);
	wunit->GenerateCodeHandler = [vs_uid](ShaderCompilerWorkUnit* wunit)
	{
		ShaderCode code;
		code.SetBuffer(wunit->code.data());
		GenerateVertexShaderCodeD3D11(code, vs_uid.GetUidData());
		wunit->codesize = (u32)code.BufferSize();
	};

	wunit->entrypoint = "main";
	wunit->flags = D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;
	wunit->target = D3D::VertexShaderVersionString();
	wunit->ResultHandler = [vs_uid, entry](ShaderCompilerWorkUnit* wunit)
	{
		if (SUCCEEDED(wunit->cresult))
		{
			D3DBlob* shaderBuffer = new D3DBlob(wunit->shaderbytecode);
			s_vs_disk_cache.Append(vs_uid, shaderBuffer->Data(), shaderBuffer->Size());
			PushByteCode(entry, shaderBuffer);
			wunit->shaderbytecode->Release();
			wunit->shaderbytecode = nullptr;
		}
		else
		{
			static int num_failures = 0;
			std::string filename = StringFromFormat("%sbad_vs_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
			std::ofstream file;
			OpenFStream(file, filename, std::ios_base::out);
			file << ((const char*)wunit->code.data());
			file.close();

			PanicAlert("Failed to compile vertex shader!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%s):\n%s",
				filename,
				D3D::VertexShaderVersionString(),
				(char*)wunit->error->GetBufferPointer());
		}
	};
	s_compiler->CompileShaderAsync(wunit);
}

void ShaderCache::HandleTSUIDChange(
	const TessellationShaderUid& ts_uid,
	bool on_gpu_thread)
{
	s_shaders_lock.lock();
	std::pair<ByteCodeCacheEntry, ByteCodeCacheEntry>& entry = ts_bytecode_cache->GetOrAdd(ts_uid);
	s_shaders_lock.unlock();
	ByteCodeCacheEntry* dentry = &entry.first;
	ByteCodeCacheEntry* hentry = &entry.second;
	if (on_gpu_thread)
	{
		if (dentry->m_compiled && hentry->m_compiled)
		{
			s_last_domain_shader_bytecode = dentry;
			s_last_hull_shader_bytecode = hentry;
		}
		else
		{
			s_last_tessellation_shader_uid = {};
			s_last_domain_shader_bytecode = &s_pass_entry;
			s_last_hull_shader_bytecode = &s_pass_entry;
		}
	}
	if (dentry->m_initialized.test_and_set())
	{
		return;
	}
	hentry->m_initialized.test_and_set();

	// Need to compile a new shader
	ShaderCode code;
	ShaderCompilerWorkUnit *wunit = s_compiler->NewUnit(TESSELLATIONSHADERGEN_BUFFERSIZE);
	ShaderCompilerWorkUnit *wunitd = s_compiler->NewUnit(TESSELLATIONSHADERGEN_BUFFERSIZE);
	code.SetBuffer(wunit->code.data());
	GenerateTessellationShaderCode(code, API_D3D11, ts_uid.GetUidData());
	memcpy(wunitd->code.data(), wunit->code.data(), code.BufferSize());

	wunit->codesize = (u32)code.BufferSize();
	wunit->entrypoint = "HS_TFO";
	wunit->flags = D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_SKIP_OPTIMIZATION;
	wunit->target = D3D::HullShaderVersionString();

	wunitd->codesize = (u32)code.BufferSize();
	wunitd->entrypoint = "DS_TFO";
	wunitd->flags = D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_OPTIMIZATION_LEVEL3;
	wunitd->target = D3D::DomainShaderVersionString();

	wunitd->ResultHandler = [ts_uid, dentry](ShaderCompilerWorkUnit* wunit)
	{
		if (SUCCEEDED(wunit->cresult))
		{
			D3DBlob* shaderBuffer = new D3DBlob(wunit->shaderbytecode);
			s_ds_disk_cache.Append(ts_uid, shaderBuffer->Data(), shaderBuffer->Size());
			PushByteCode(dentry, shaderBuffer);
			wunit->shaderbytecode->Release();
			wunit->shaderbytecode = nullptr;
		}
		else
		{
			static int num_failures = 0;
			std::string filename = StringFromFormat("%sbad_ds_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
			std::ofstream file;
			OpenFStream(file, filename, std::ios_base::out);
			file << ((const char *)wunit->code.data());
			file << ((const char *)wunit->error->GetBufferPointer());
			file.close();

			PanicAlert("Failed to compile domain shader!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%s):\n%s",
				filename,
				D3D::DomainShaderVersionString(),
				(char*)wunit->error->GetBufferPointer());
		}
	};

	wunit->ResultHandler = [ts_uid, hentry](ShaderCompilerWorkUnit* wunit)
	{
		if (SUCCEEDED(wunit->cresult))
		{
			D3DBlob* shaderBuffer = new D3DBlob(wunit->shaderbytecode);
			s_hs_disk_cache.Append(ts_uid, shaderBuffer->Data(), shaderBuffer->Size());
			PushByteCode(hentry, shaderBuffer);
			wunit->shaderbytecode->Release();
			wunit->shaderbytecode = nullptr;
		}
		else
		{
			static int num_failures = 0;
			std::string filename = StringFromFormat("%sbad_hs_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
			std::ofstream file;
			OpenFStream(file, filename, std::ios_base::out);
			file << ((const char *)wunit->code.data());
			file << ((const char *)wunit->error->GetBufferPointer());
			file.close();

			PanicAlert("Failed to compile hull shader!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%s):\n%s",
				filename,
				D3D::HullShaderVersionString(),
				(char*)wunit->error->GetBufferPointer());
		}
	};

	s_compiler->CompileShaderAsync(wunit);
	s_compiler->CompileShaderAsync(wunitd);
}

void ShaderCache::PrepareShaders(PIXEL_SHADER_RENDER_MODE render_mode,
	u32 gs_primitive_type,
	u32 components,
	const XFMemory &xfr,
	const BPMemory &bpm, bool on_gpu_thread)
{
	SetCurrentPrimitiveTopology(gs_primitive_type);
	GeometryShaderUid gs_uid;
	GetGeometryShaderUid(gs_uid, gs_primitive_type, xfr, components);
	PixelShaderUid ps_uid;
	GetPixelShaderUID(ps_uid, render_mode, components, xfr, bpm);
	VertexShaderUid vs_uid;
	GetVertexShaderUID(vs_uid, components, xfr, bpm);
	TessellationShaderUid ts_uid;
	bool tessellationenabled = false;
	if (gs_primitive_type == PrimitiveType::PRIMITIVE_TRIANGLES
		&& g_ActiveConfig.TessellationEnabled()
		&& xfr.projection.type == GX_PERSPECTIVE
		&& (g_ActiveConfig.bForcedLighting || g_ActiveConfig.PixelLightingEnabled(xfr, components)))
	{
		GetTessellationShaderUID(ts_uid, xfr, bpm, components);
		tessellationenabled = true;
	}

	bool gs_changed = false;
	bool ps_changed = false;
	bool vs_changed = false;
	bool ts_changed = false;

	if (on_gpu_thread)
	{
		s_compiler->ProcCompilationResults();
		gs_changed = gs_uid != s_last_geometry_shader_uid;
		ps_changed = ps_uid != s_last_pixel_shader_uid;
		vs_changed = vs_uid != s_last_vertex_shader_uid;
		ts_changed = tessellationenabled && ts_uid != s_last_tessellation_shader_uid;
	}
	else
	{
		gs_changed = gs_uid != s_last_cpu_geometry_shader_uid;
		ps_changed = ps_uid != s_last_cpu_pixel_shader_uid;
		vs_changed = vs_uid != s_last_cpu_vertex_shader_uid;
		ts_changed = tessellationenabled && ts_uid != s_last_cpu_tessellation_shader_uid;
	}

	if (!gs_changed && !ps_changed && !vs_changed && !ts_changed)
	{
		return;
	}

	if (on_gpu_thread)
	{
		if (gs_changed)
		{
			s_last_geometry_shader_uid = gs_uid;
		}

		if (ps_changed)
		{
			s_last_pixel_shader_uid = ps_uid;
		}

		if (vs_changed)
		{
			s_last_vertex_shader_uid = vs_uid;
		}
		if (ts_changed)
		{
			s_last_tessellation_shader_uid = ts_uid;
		}
		// A Uid has changed, so the PSO will need to be reset at next ApplyState.
		D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PSO, true);
	}
	else
	{
		if (gs_changed)
		{
			s_last_cpu_geometry_shader_uid = gs_uid;
		}

		if (ps_changed)
		{
			s_last_cpu_pixel_shader_uid = ps_uid;
		}

		if (vs_changed)
		{
			s_last_cpu_vertex_shader_uid = vs_uid;
		}

		if (ts_changed)
		{
			s_last_cpu_tessellation_shader_uid = ts_uid;
		}
	}

	if (vs_changed)
	{
		HandleVSUIDChange(vs_uid, on_gpu_thread);
	}

	if (ts_changed)
	{
		HandleTSUIDChange(ts_uid, on_gpu_thread);
	}
	else
	{
		if (on_gpu_thread)
		{
			s_last_domain_shader_bytecode = &s_pass_entry;
			s_last_hull_shader_bytecode = &s_pass_entry;
		}
	}

	if (gs_changed)
	{
		HandleGSUIDChange(gs_uid, on_gpu_thread);
	}

	if (ps_changed)
	{
		HandlePSUIDChange(ps_uid, on_gpu_thread);
	}
}

bool ShaderCache::TestShaders()
{
	if (s_last_geometry_shader_bytecode == nullptr
		|| s_last_pixel_shader_bytecode == nullptr
		|| s_last_vertex_shader_bytecode == nullptr)
	{
		return false;
	}
	int count = 0;
	while (!(s_last_geometry_shader_bytecode->m_compiled
		&& s_last_pixel_shader_bytecode->m_compiled
		&& s_last_vertex_shader_bytecode->m_compiled))
	{
		s_compiler->ProcCompilationResults();
		if (g_ActiveConfig.bFullAsyncShaderCompilation)
		{
			break;
		}
		Common::cYield(count++);
	}
	return s_last_geometry_shader_bytecode->m_compiled
		&& s_last_pixel_shader_bytecode->m_compiled
		&& s_last_vertex_shader_bytecode->m_compiled;
}

void ShaderCache::InsertDSByteCode(const TessellationShaderUid & uid, D3DBlob * bytecode_blob)
{
	s_shader_blob_list.push_back(bytecode_blob);
	s_shaders_lock.lock();
	ByteCodeCacheEntry* entry = &ts_bytecode_cache->GetOrAdd(uid).first;
	s_shaders_lock.unlock();
	entry->m_shader_bytecode.pShaderBytecode = bytecode_blob->Data();
	entry->m_shader_bytecode.BytecodeLength = bytecode_blob->Size();
	entry->m_compiled = true;
	entry->m_initialized.test_and_set();
}

void ShaderCache::InsertHSByteCode(const TessellationShaderUid & uid, D3DBlob * bytecode_blob)
{
	s_shader_blob_list.push_back(bytecode_blob);
	s_shaders_lock.lock();
	ByteCodeCacheEntry* entry = &ts_bytecode_cache->GetOrAdd(uid).second;
	s_shaders_lock.unlock();
	entry->m_shader_bytecode.pShaderBytecode = bytecode_blob->Data();
	entry->m_shader_bytecode.BytecodeLength = bytecode_blob->Size();
	entry->m_compiled = true;
	entry->m_initialized.test_and_set();
}

template<typename UidType, typename ShaderCacheType>
void ShaderCache::InsertByteCode(const UidType& uid, ShaderCacheType* shader_cache, D3DBlob* bytecode_blob)
{
	s_shader_blob_list.push_back(bytecode_blob);
	s_shaders_lock.lock();
	ByteCodeCacheEntry* entry = entry = &shader_cache->GetOrAdd(uid);
	s_shaders_lock.unlock();
	entry->m_shader_bytecode.pShaderBytecode = bytecode_blob->Data();
	entry->m_shader_bytecode.BytecodeLength = bytecode_blob->Size();
	entry->m_compiled = true;
	entry->m_initialized.test_and_set();
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE ShaderCache::GetCurrentPrimitiveTopology()
{
	return s_current_primitive_topology;
}

D3D12_SHADER_BYTECODE ShaderCache::GetActiveDomainShaderBytecode()
{
	return s_current_primitive_topology == D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE
		&& s_last_hull_shader_bytecode->m_compiled
		&& s_last_domain_shader_bytecode->m_compiled ? s_last_domain_shader_bytecode->m_shader_bytecode : s_pass_entry.m_shader_bytecode;
}
D3D12_SHADER_BYTECODE ShaderCache::GetActiveHullShaderBytecode()
{
	return s_current_primitive_topology == D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE
		&& s_last_hull_shader_bytecode->m_compiled
		&& s_last_domain_shader_bytecode->m_compiled ? s_last_hull_shader_bytecode->m_shader_bytecode : s_pass_entry.m_shader_bytecode;
}
D3D12_SHADER_BYTECODE ShaderCache::GetActiveGeometryShaderBytecode()
{
	return s_last_geometry_shader_bytecode->m_shader_bytecode;
}
D3D12_SHADER_BYTECODE ShaderCache::GetActivePixelShaderBytecode()
{
	return s_last_pixel_shader_bytecode->m_shader_bytecode;
}
D3D12_SHADER_BYTECODE ShaderCache::GetActiveVertexShaderBytecode()
{
	return s_last_vertex_shader_bytecode->m_shader_bytecode;
}

const GeometryShaderUid* ShaderCache::GetActiveGeometryShaderUid()
{
	return &s_last_geometry_shader_uid;
}
const PixelShaderUid* ShaderCache::GetActivePixelShaderUid()
{
	return &s_last_pixel_shader_uid;
}
const VertexShaderUid* ShaderCache::GetActiveVertexShaderUid()
{
	return &s_last_vertex_shader_uid;
}
const TessellationShaderUid* ShaderCache::GetActiveTessellationShaderUid()
{
	return &s_last_tessellation_shader_uid;
}

static const D3D12_SHADER_BYTECODE empty = { 0 };

D3D12_SHADER_BYTECODE ShaderCache::GetDomainShaderFromUid(const TessellationShaderUid* uid)
{
	auto it = ts_bytecode_cache->GetInfoIfexists(*uid);
	if (it != nullptr)
		return it->first.m_shader_bytecode;

	return empty;
}
D3D12_SHADER_BYTECODE ShaderCache::GetHullShaderFromUid(const TessellationShaderUid* uid)
{
	auto it = ts_bytecode_cache->GetInfoIfexists(*uid);
	if (it != nullptr)
		return it->second.m_shader_bytecode;

	return empty;
}
D3D12_SHADER_BYTECODE ShaderCache::GetGeometryShaderFromUid(const GeometryShaderUid* uid)
{
	auto it = gs_bytecode_cache->GetInfoIfexists(*uid);
	if (it != nullptr)
		return it->m_shader_bytecode;

	return empty;
}
D3D12_SHADER_BYTECODE ShaderCache::GetPixelShaderFromUid(const PixelShaderUid* uid)
{
	auto it = ps_bytecode_cache->GetInfoIfexists(*uid);
	if (it != nullptr)
		return it->m_shader_bytecode;

	return empty;
}
D3D12_SHADER_BYTECODE ShaderCache::GetVertexShaderFromUid(const VertexShaderUid* uid)
{
	auto it = vs_bytecode_cache->GetInfoIfexists(*uid);
	if (it != nullptr)
		return it->m_shader_bytecode;

	return empty;
}

}