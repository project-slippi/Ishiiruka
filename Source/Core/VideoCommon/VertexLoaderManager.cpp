// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Modified for Ishiiruka by Tino

#include <map>
#include <memory>
#include <unordered_map>


#include "Core/ConfigManager.h"
#include "Core/HW/Memmap.h"

#include "Common/ThreadPool.h"
#include "Common/StringUtil.h"

#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoConfig.h"



static VertexLoaderBase *s_cpu_loaders[8];
static std::string last_game_code;

typedef std::unordered_map<VertexLoaderUID, std::unique_ptr<VertexLoaderBase>> VertexLoaderMap;

namespace VertexLoaderManager
{
static VertexLoaderMap s_vertex_loader_map;
static NativeVertexFormatMap s_native_vertex_map;
static NativeVertexFormat* s_current_vtx_fmt;
u32 g_current_components;
// TODO - change into array of pointers. Keep a map of all seen so far.
// Used in D3D12 backend, to populate input layouts used by cached-to-disk PSOs.
NativeVertexFormatMap* GetNativeVertexFormatMap()
{
	return &s_native_vertex_map;
}

NativeVertexFormat* GetCurrentVertexFormat()
{
	return s_current_vtx_fmt;
}

namespace
{
struct entry
{
	std::string text;
	u64 num_verts;
	bool operator < (const entry &other) const
	{
		return num_verts > other.num_verts;
	}
};

struct codeentry
{
	std::string name;
	std::string conf;
	u64 num_verts;
	std::string hash;
	bool operator < (const codeentry &other) const
	{
		return num_verts > other.num_verts;
	}
};
}

static std::string To_HexString(u32 in)
{
	char hexString[2 * sizeof(u32) + 8];
	sprintf(hexString, "0x%08xu", in);
	return std::string(hexString);
}

static void DumpLoadersCode()
{
	std::vector<codeentry> entries;
	for (VertexLoaderMap::const_iterator iter = s_vertex_loader_map.begin(); iter != s_vertex_loader_map.end(); ++iter)
	{
		if (!iter->second->IsPrecompiled())
		{
			codeentry e;
			e.conf.append(To_HexString(iter->first.GetElement(0)));
			e.conf.append(", ");
			e.conf.append(To_HexString(iter->first.GetElement(1)));
			e.conf.append(", ");
			e.conf.append(To_HexString(iter->first.GetElement(2)));
			e.conf.append(", ");
			e.conf.append(To_HexString(iter->first.GetElement(3)));
			e.name = iter->second->GetName();
			e.num_verts = iter->second->m_numLoadedVertices;
			e.hash = std::to_string(iter->first.GetHash());
			entries.push_back(e);
		}
	}
	if (entries.size() == 0)
	{
		return;
	}
	std::string filename = StringFromFormat("%sG_%s_pvt.h", File::GetUserPath(D_DUMP_IDX).c_str(), last_game_code.c_str());
	std::string header;
	header.append("// Copyright 2013 Dolphin Emulator Project\n");
	header.append("// Licensed under GPLv2+\n");
	header.append("// Refer to the license.txt file included.\n");
	header.append("// Added for Ishiiruka by Tino\n");
	header.append("#pragma once\n");
	header.append("#include <map>\n");
	header.append("#include \"VideoCommon/NativeVertexFormat.h\"\n");
	header.append("class G_");
	header.append(last_game_code);
	header.append("_pvt\n{\npublic:\n");
	header.append("static void Initialize(std::map<u64, TCompiledLoaderFunction> &pvlmap);\n");
	header.append("};\n");
	std::ofstream headerfile(filename);
	headerfile << header;
	headerfile.close();
	filename = StringFromFormat("%sG_%s_pvt.cpp", File::GetUserPath(D_DUMP_IDX).c_str(), last_game_code.c_str());
	sort(entries.begin(), entries.end());
	std::string sourcecode;
	sourcecode.append("#include \"VideoCommon/G_");
	sourcecode.append(last_game_code);
	sourcecode.append("_pvt.h\"\n");
	sourcecode.append("#include \"VideoCommon/VertexLoader_Template.h\"\n\n");
	sourcecode.append("\n\nvoid G_");
	sourcecode.append(last_game_code);
	sourcecode.append("_pvt::Initialize(std::map<u64, TCompiledLoaderFunction> &pvlmap)\n{\n");
	for (std::vector<codeentry>::const_iterator iter = entries.begin(); iter != entries.end(); ++iter)
	{
		sourcecode.append("\t// ");
		sourcecode.append(iter->name);
		sourcecode.append("\n// num_verts= ");
		sourcecode.append(std::to_string(iter->num_verts));
		sourcecode.append("#if _M_SSE >= 0x301\n");
		sourcecode.append("\tif (cpu_info.bSSSE3)\n");
		sourcecode.append("\t{\n");
		sourcecode.append("\t\tpvlmap[");
		sourcecode.append(iter->hash);
		sourcecode.append("] = ");
		sourcecode.append("TemplatedLoader");
		sourcecode.append("<0x301, ");
		sourcecode.append(iter->conf);
		sourcecode.append(">;\n");
		sourcecode.append("\t}\n\telse\n");
		sourcecode.append("#endif\n");
		sourcecode.append("\t{\n");
		sourcecode.append("\t\tpvlmap[");
		sourcecode.append(iter->hash);
		sourcecode.append("] = ");
		sourcecode.append("TemplatedLoader");
		sourcecode.append("<0, ");
		sourcecode.append(iter->conf);
		sourcecode.append(">;\n");
		sourcecode.append("\t}\n");
	}
	sourcecode.append("}\n");
	std::ofstream out(filename);
	out << sourcecode;
	out.close();
}

void AppendListToString(std::string *dest)
{
	std::vector<entry> entries;

	size_t total_size = 0;
	for (VertexLoaderMap::const_iterator iter = s_vertex_loader_map.begin(); iter != s_vertex_loader_map.end(); ++iter)
	{
		entry e;
		iter->second->AppendToString(&e.text);
		e.num_verts = iter->second->m_numLoadedVertices;
		entries.push_back(e);
		total_size += e.text.size() + 1;
	}
	sort(entries.begin(), entries.end());
	dest->reserve(dest->size() + total_size);
	for (std::vector<entry>::const_iterator iter = entries.begin(); iter != entries.end(); ++iter)
	{
		dest->append(iter->text);
	}
}

void Init()
{
	MarkAllDirty();
	for (VertexLoaderBase*& vertexLoader : g_main_cp_state.vertex_loaders)
		vertexLoader = nullptr;
	last_game_code = SConfig::GetInstance().m_strGameID;
}

void Shutdown()
{
	if (s_vertex_loader_map.size() > 0 && g_ActiveConfig.bDumpVertexLoaders)
		DumpLoadersCode();
	s_vertex_loader_map.clear();
	s_native_vertex_map.clear();
}

void UpdateVertexArrayPointers()
{
	// Anything to update?
	if (!g_main_cp_state.bases_dirty)
		return;

	// Some games such as Burnout 2 can put invalid addresses into
	// the array base registers. (see issue 8591)
	// But the vertex arrays with invalid addresses aren't actually enabled.
	// Note: Only array bases 0 through 11 are used by the Vertex loaders.
	//       12 through 15 are used for loading data into xfmem.
	for (int i = 0; i < 12; i++)
	{
		// Only update the array base if the vertex description states we are going to use it.
		if (g_main_cp_state.vtx_desc.GetVertexArrayStatus(i) >= 0x2)
			cached_arraybases[i] = Memory::GetPointer(g_main_cp_state.array_bases[i]);
	}

	g_main_cp_state.bases_dirty = false;
}

inline NativeVertexFormat* GetNativeVertexFormat(const PortableVertexDeclaration& format)
{
	auto& native = s_native_vertex_map[format];
	if (!native)
	{
		native = g_vertex_manager->CreateNativeVertexFormat(format);
	}
	return native.get();
}

void MarkAllDirty()
{
	g_main_cp_state.attr_dirty = 0xff;
	g_main_cp_state.bases_dirty = true;
	g_preprocess_cp_state.attr_dirty = 0xff;
	g_preprocess_cp_state.bases_dirty = true;
}

inline VertexLoaderBase *GetOrAddLoader(const TVtxDesc &VtxDesc, const VAT &VtxAttr)
{
	VertexLoaderUID uid(VtxDesc, VtxAttr);
	VertexLoaderMap::iterator iter = s_vertex_loader_map.find(uid);
	if (iter == s_vertex_loader_map.end())
	{
		s_vertex_loader_map[uid] = VertexLoaderBase::CreateVertexLoader(VtxDesc, VtxAttr);
		VertexLoaderBase* loader = s_vertex_loader_map[uid].get();
		loader->m_native_vertex_format = GetNativeVertexFormat(loader->m_native_vtx_decl);
		VertexLoaderBase * fallback = loader->GetFallback();
		if (fallback)
		{
			fallback->m_native_vertex_format = GetNativeVertexFormat(fallback->m_native_vtx_decl);
		}
		INCSTAT(stats.numVertexLoaders);
		return loader;
	}
	return iter->second.get();
}

void GetVertexSizeAndComponents(const VertexLoaderParameters &parameters, u32 &vertexsize, u32 &components)
{
	if (parameters.needloaderrefresh)
	{
		s_cpu_loaders[parameters.vtx_attr_group] = GetOrAddLoader(*parameters.VtxDesc, *parameters.VtxAttr);
	}
	vertexsize = s_cpu_loaders[parameters.vtx_attr_group]->m_VertexSize;
	components = s_cpu_loaders[parameters.vtx_attr_group]->m_native_components;
}

inline void UpdateLoader(const VertexLoaderParameters &parameters)
{
	g_main_cp_state.vertex_loaders[parameters.vtx_attr_group] = GetOrAddLoader(*parameters.VtxDesc, *parameters.VtxAttr);
	g_main_cp_state.last_id = parameters.vtx_attr_group;
}

bool ConvertVertices(VertexLoaderParameters &parameters, u32 &readsize, u32 &writesize)
{
	if (parameters.needloaderrefresh)
	{
		UpdateLoader(parameters);
	}
	auto loader = g_main_cp_state.vertex_loaders[parameters.vtx_attr_group];
	if (!loader->EnvironmentIsSupported())
	{
		loader = loader->GetFallback();
	}
	readsize = parameters.count * loader->m_VertexSize;
	if (parameters.buf_size < readsize)
		return false;
	if (parameters.skip_draw)
	{
		return true;
	}
	// Lookup pointers for any vertex arrays.
	UpdateVertexArrayPointers();
	NativeVertexFormat *nativefmt = loader->m_native_vertex_format;
	// Flush if our vertex format is different from the currently set.
	if (s_current_vtx_fmt != nullptr && s_current_vtx_fmt != nativefmt)
	{
		g_vertex_manager->Flush();
	}
	s_current_vtx_fmt = nativefmt;
	g_current_components = loader->m_native_components;
	g_vertex_manager->PrepareForAdditionalData(parameters.primitive, parameters.count, loader->m_native_stride);
	parameters.destination = g_vertex_manager->GetCurrentBufferPointer();
	s32 finalcount = loader->RunVertices(parameters);
	writesize = loader->m_native_stride * finalcount;
	IndexGenerator::AddIndices(parameters.primitive, finalcount);
	ADDSTAT(stats.thisFrame.numPrims, finalcount);
	INCSTAT(stats.thisFrame.numPrimitiveJoins);
	return true;
}

int GetVertexSize(const VertexLoaderParameters &parameters)
{
	if (parameters.needloaderrefresh)
	{
		UpdateLoader(parameters);
	}
	return g_main_cp_state.vertex_loaders[parameters.vtx_attr_group]->m_VertexSize;
}

}  // namespace
