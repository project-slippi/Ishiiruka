// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#include <unordered_map>

#include "Common/LinearDiskCache.h"

#include "Core/ConfigManager.h"

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
	void Release() {
		if (m_shader_bytecode.pShaderBytecode)
			delete[] m_shader_bytecode.pShaderBytecode;
		m_shader_bytecode.pShaderBytecode = nullptr;
	}
};

static ByteCodeCacheEntry s_pass_entry;

using GsBytecodeCache = std::unordered_map<GeometryShaderUid, ByteCodeCacheEntry, GeometryShaderUid::ShaderUidHasher>;
using PsBytecodeCache = std::unordered_map<PixelShaderUid, ByteCodeCacheEntry, PixelShaderUid::ShaderUidHasher>;
using VsBytecodeCache = std::unordered_map<VertexShaderUid, ByteCodeCacheEntry, VertexShaderUid::ShaderUidHasher>;
using TsBytecodeCache = std::unordered_map<TessellationShaderUid, ByteCodeCacheEntry, TessellationShaderUid::ShaderUidHasher>;

TsBytecodeCache ds_bytecode_cache;
TsBytecodeCache hs_bytecode_cache;
GsBytecodeCache gs_bytecode_cache;
PsBytecodeCache ps_bytecode_cache;
VsBytecodeCache vs_bytecode_cache;

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

static ObjectUsageProfiler<VertexShaderUid, pKey_t, VertexShaderUid::ShaderUidHasher>* s_vs_profiler = nullptr;
static ObjectUsageProfiler<PixelShaderUid, pKey_t, PixelShaderUid::ShaderUidHasher>* s_ps_profiler = nullptr;
static ObjectUsageProfiler<GeometryShaderUid, pKey_t, GeometryShaderUid::ShaderUidHasher>* s_gs_profiler = nullptr;
static ObjectUsageProfiler<TessellationShaderUid, pKey_t, TessellationShaderUid::ShaderUidHasher>* s_hds_profiler = nullptr;

template<typename UidType, typename ShaderCacheType, ShaderCacheType* cache>
class ShaderCacheInserter final : public LinearDiskCacheReader<UidType, u8>
{
public:
	void Read(const UidType &key, const u8* value, u32 value_size)
	{
		D3DBlob* blob = new D3DBlob(value_size, value);
		ShaderCache::InsertByteCode<UidType, ShaderCacheType>(key, cache, blob);
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

	std::string title_unique_id = SConfig::GetInstance().m_strUniqueID.c_str();
	pKey_t gameid = (pKey_t)GetMurmurHash3(reinterpret_cast<const u8*>(title_unique_id.data()), (u32)title_unique_id.size(), 0);

	std::string ds_cache_filename = StringFromFormat("%sIDX11-%s-ds.cache", shader_cache_path.c_str(), title_unique_id.c_str());
	std::string hs_cache_filename = StringFromFormat("%sIDX11-%s-hs.cache", shader_cache_path.c_str(), title_unique_id.c_str());
	std::string gs_cache_filename = StringFromFormat("%sIDX11-%s-gs.cache", shader_cache_path.c_str(), title_unique_id.c_str());
	std::string ps_cache_filename = StringFromFormat("%sIDX11-%s-ps.cache", shader_cache_path.c_str(), title_unique_id.c_str());
	std::string vs_cache_filename = StringFromFormat("%sIDX11-%s-vs.cache", shader_cache_path.c_str(), title_unique_id.c_str());

	std::string profile_vs_filename = StringFromFormat("%s\\Ishiiruka.vs.usage", File::GetExeDirectory().c_str());
	bool profile_vs_exists = File::Exists(profile_vs_filename);
	if (g_ActiveConfig.bShaderUsageProfiling || profile_vs_exists)
	{
		s_vs_profiler = new ObjectUsageProfiler<VertexShaderUid, pKey_t, VertexShaderUid::ShaderUidHasher>(VERTEXSHADERGEN_UID_VERSION);
	}
	if (profile_vs_exists)
	{
		s_vs_profiler->ReadFromFile(profile_vs_filename);
	}
	if (s_vs_profiler)
	{
		s_vs_profiler->SetCategory(gameid);
	}

	std::string profile_ps_filename = StringFromFormat("%s\\Ishiiruka.ps.usage", File::GetExeDirectory().c_str());
	bool profile_ps_exists = File::Exists(profile_ps_filename);
	if (g_ActiveConfig.bShaderUsageProfiling || profile_ps_exists)
	{
		s_ps_profiler = new ObjectUsageProfiler<PixelShaderUid, pKey_t, PixelShaderUid::ShaderUidHasher>(PIXELSHADERGEN_UID_VERSION);
	}
	if (profile_ps_exists)
	{
		s_ps_profiler->ReadFromFile(profile_ps_filename);
	}
	if (s_ps_profiler)
	{
		s_ps_profiler->SetCategory(gameid);
	}

	std::string profile_gs_filename = StringFromFormat("%s\\Ishiiruka.gs.usage", File::GetExeDirectory().c_str());
	bool profile_gs_exists = File::Exists(profile_gs_filename);
	if (g_ActiveConfig.bShaderUsageProfiling || profile_gs_exists)
	{
		s_gs_profiler = new ObjectUsageProfiler<GeometryShaderUid, pKey_t, GeometryShaderUid::ShaderUidHasher>(GEOMETRYSHADERGEN_UID_VERSION);
	}
	if (profile_gs_exists)
	{
		s_gs_profiler->ReadFromFile(profile_gs_filename);
	}
	if (s_gs_profiler)
	{
		s_gs_profiler->SetCategory(gameid);
	}

	std::string profile_hds_filename = StringFromFormat("%s\\Ishiiruka.ts.usage", File::GetExeDirectory().c_str());
	bool profile_hds_exists = File::Exists(profile_hds_filename);
	if (g_ActiveConfig.bShaderUsageProfiling || profile_hds_exists)
	{
		s_hds_profiler = new ObjectUsageProfiler<TessellationShaderUid, pKey_t, TessellationShaderUid::ShaderUidHasher>(TESSELLATIONSHADERGEN_UID_VERSION);
	}
	if (profile_hds_exists)
	{
		s_hds_profiler->ReadFromFile(profile_hds_filename);
	}
	if (s_hds_profiler)
	{
		s_hds_profiler->SetCategory(gameid);
	}

	ShaderCacheInserter<TessellationShaderUid, TsBytecodeCache, &ds_bytecode_cache> ds_inserter;
	s_ds_disk_cache.OpenAndRead(ds_cache_filename, ds_inserter);

	ShaderCacheInserter<TessellationShaderUid, TsBytecodeCache, &hs_bytecode_cache> hs_inserter;
	s_hs_disk_cache.OpenAndRead(hs_cache_filename, hs_inserter);

	ShaderCacheInserter<GeometryShaderUid, GsBytecodeCache, &gs_bytecode_cache> gs_inserter;
	s_gs_disk_cache.OpenAndRead(gs_cache_filename, gs_inserter);

	ShaderCacheInserter<PixelShaderUid, PsBytecodeCache, &ps_bytecode_cache> ps_inserter;
	s_ps_disk_cache.OpenAndRead(ps_cache_filename, ps_inserter);

	ShaderCacheInserter<VertexShaderUid, VsBytecodeCache, &vs_bytecode_cache> vs_inserter;
	s_vs_disk_cache.OpenAndRead(vs_cache_filename, vs_inserter);

	// Clear out disk cache when debugging shaders to ensure stale ones don't stick around..
	SETSTAT(stats.numGeometryShadersAlive, static_cast<int>(gs_bytecode_cache.size()));
	SETSTAT(stats.numGeometryShadersCreated, 0);
	SETSTAT(stats.numPixelShadersAlive, static_cast<int>(ps_bytecode_cache.size()));
	SETSTAT(stats.numPixelShadersCreated, 0);
	SETSTAT(stats.numVertexShadersAlive, static_cast<int>(vs_bytecode_cache.size()));
	SETSTAT(stats.numVertexShadersCreated, 0);
	if (g_ActiveConfig.bCompileShaderOnStartup)
	{
		if (profile_ps_exists)
		{
			std::vector<PixelShaderUid> shaders;
			s_ps_profiler->GetMostUsedByCategory(gameid, shaders, true);
			size_t shader_count = 0;
			for (const PixelShaderUid& item : shaders)
			{
				if (ps_bytecode_cache.find(item) == ps_bytecode_cache.end())
				{
					HandlePSUIDChange(item, true);
				}
				shader_count++;
				if ((shader_count & 31) == 0)
				{
					s_compiler->WaitForFinish();
				}
			}
			s_compiler->WaitForFinish();
		}

		if (profile_vs_exists)
		{
			std::vector<VertexShaderUid> shaders;
			s_vs_profiler->GetMostUsedByCategory(gameid, shaders, true);
			size_t shader_count = 0;
			for (const VertexShaderUid& item : shaders)
			{
				if (vs_bytecode_cache.find(item) == vs_bytecode_cache.end())
				{
					HandleVSUIDChange(item, true);
				}
				shader_count++;
				if ((shader_count & 31) == 0)
				{
					s_compiler->WaitForFinish();
				}
			}
			s_compiler->WaitForFinish();
		}

		if (profile_gs_exists)
		{
			std::vector<GeometryShaderUid> shaders;
			s_gs_profiler->GetMostUsedByCategory(gameid, shaders, true);
			size_t shader_count = 0;
			for (const GeometryShaderUid& item : shaders)
			{
				if (gs_bytecode_cache.find(item) == gs_bytecode_cache.end())
				{
					HandleGSUIDChange(item, true);
				}
				shader_count++;
				if ((shader_count & 31) == 0)
				{
					s_compiler->WaitForFinish();
				}
			}
			s_compiler->WaitForFinish();
		}

		if (profile_hds_exists)
		{
			std::vector<TessellationShaderUid> shaders;
			s_hds_profiler->GetMostUsedByCategory(gameid, shaders, true);
			size_t shader_count = 0;
			for (const TessellationShaderUid& item : shaders)
			{
				if (hs_bytecode_cache.find(item) == hs_bytecode_cache.end())
				{
					HandleTSUIDChange(item, true);
				}
				shader_count++;
				if ((shader_count & 31) == 0)
				{
					s_compiler->WaitForFinish();
				}
			}
			s_compiler->WaitForFinish();
		}
	}
}

void ShaderCache::Clear()
{

}

void ShaderCache::Shutdown()
{
	if (s_vs_profiler)
	{
		std::string profile_filename = StringFromFormat("%s\\Ishiiruka.vs.usage", File::GetExeDirectory().c_str());
		s_vs_profiler->PersistToFile(profile_filename);
		delete s_vs_profiler;
		s_vs_profiler = nullptr;
	}
	if (s_ps_profiler)
	{
		std::string profile_filename = StringFromFormat("%s\\Ishiiruka.ps.usage", File::GetExeDirectory().c_str());
		s_ps_profiler->PersistToFile(profile_filename);
		delete s_ps_profiler;
		s_ps_profiler = nullptr;
	}

	if (s_compiler)
	{
		s_compiler->WaitForFinish();
	}

	for (auto& iter : s_shader_blob_list)
		SAFE_RELEASE(iter);

	s_shader_blob_list.clear();
	ds_bytecode_cache.clear();
	hs_bytecode_cache.clear();
	gs_bytecode_cache.clear();
	ps_bytecode_cache.clear();
	vs_bytecode_cache.clear();

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
	ByteCodeCacheEntry* entry = &gs_bytecode_cache[gs_uid];
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
			SETSTAT(stats.numGeometryShadersAlive, static_cast<int>(ps_bytecode_cache.size()));
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
	ByteCodeCacheEntry* entry = &ps_bytecode_cache[ps_uid];
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
	ByteCodeCacheEntry* entry = &vs_bytecode_cache[vs_uid];
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
	ByteCodeCacheEntry* dentry = &ds_bytecode_cache[ts_uid];
	ByteCodeCacheEntry* hentry = &hs_bytecode_cache[ts_uid];
	s_shaders_lock.unlock();
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
		if (g_ActiveConfig.bShaderUsageProfiling)
			s_vs_profiler->RegisterUsage(vs_uid);
		HandleVSUIDChange(vs_uid, on_gpu_thread);
	}

	if (ts_changed)
	{
		if (g_ActiveConfig.bShaderUsageProfiling)
			s_hds_profiler->RegisterUsage(ts_uid);
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
		if (g_ActiveConfig.bShaderUsageProfiling)
			s_gs_profiler->RegisterUsage(gs_uid);
		HandleGSUIDChange(gs_uid, on_gpu_thread);
	}

	if (ps_changed)
	{
		if (g_ActiveConfig.bShaderUsageProfiling)
			s_ps_profiler->RegisterUsage(ps_uid);
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

template<typename UidType, typename ShaderCacheType>
void ShaderCache::InsertByteCode(const UidType& uid, ShaderCacheType* shader_cache, D3DBlob* bytecode_blob)
{
	s_shader_blob_list.push_back(bytecode_blob);
	s_shaders_lock.lock();
	ByteCodeCacheEntry* entry = &(*shader_cache)[uid];
	s_shaders_lock.unlock();
	entry->m_shader_bytecode.pShaderBytecode = bytecode_blob->Data();
	entry->m_shader_bytecode.BytecodeLength = bytecode_blob->Size();
	entry->m_compiled = true;
	entry->m_initialized.test_and_set();
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE ShaderCache::GetCurrentPrimitiveTopology() { return s_current_primitive_topology; }

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
D3D12_SHADER_BYTECODE ShaderCache::GetActiveGeometryShaderBytecode() { return s_last_geometry_shader_bytecode->m_shader_bytecode; }
D3D12_SHADER_BYTECODE ShaderCache::GetActivePixelShaderBytecode() { return s_last_pixel_shader_bytecode->m_shader_bytecode; }
D3D12_SHADER_BYTECODE ShaderCache::GetActiveVertexShaderBytecode() { return s_last_vertex_shader_bytecode->m_shader_bytecode; }

const GeometryShaderUid* ShaderCache::GetActiveGeometryShaderUid() { return &s_last_geometry_shader_uid; }
const PixelShaderUid* ShaderCache::GetActivePixelShaderUid() { return &s_last_pixel_shader_uid; }
const VertexShaderUid* ShaderCache::GetActiveVertexShaderUid() { return &s_last_vertex_shader_uid; }
const TessellationShaderUid* ShaderCache::GetActiveTessellationShaderUid() { return &s_last_tessellation_shader_uid; }

static const D3D12_SHADER_BYTECODE empty = { 0 };

D3D12_SHADER_BYTECODE ShaderCache::GetDomainShaderFromUid(const TessellationShaderUid* uid)
{
	auto it = ds_bytecode_cache.find(*uid);
	if (it != ds_bytecode_cache.end())
		return it->second.m_shader_bytecode;

	return empty;
}
D3D12_SHADER_BYTECODE ShaderCache::GetHullShaderFromUid(const TessellationShaderUid* uid)
{
	auto it = hs_bytecode_cache.find(*uid);
	if (it != hs_bytecode_cache.end())
		return it->second.m_shader_bytecode;

	return empty;
}
D3D12_SHADER_BYTECODE ShaderCache::GetGeometryShaderFromUid(const GeometryShaderUid* uid)
{
	auto it = gs_bytecode_cache.find(*uid);
	if (it != gs_bytecode_cache.end())
		return it->second.m_shader_bytecode;

	return empty;
}
D3D12_SHADER_BYTECODE ShaderCache::GetPixelShaderFromUid(const PixelShaderUid* uid)
{
	auto it = ps_bytecode_cache.find(*uid);
	if (it != ps_bytecode_cache.end())
		return it->second.m_shader_bytecode;

	return empty;
}
D3D12_SHADER_BYTECODE ShaderCache::GetVertexShaderFromUid(const VertexShaderUid* uid)
{
	auto it = vs_bytecode_cache.find(*uid);
	if (it != vs_bytecode_cache.end())
		return it->second.m_shader_bytecode;

	return empty;
}

}