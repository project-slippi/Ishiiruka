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

#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoConfig.h"



NativeVertexFormat *g_nativeVertexFmt;
static VertexLoaderBase *s_CPULoaders[8];
static std::string LastGameCode;

typedef std::unordered_map<VertexLoaderUID, VertexLoaderBase*> VertexLoaderMap;
typedef std::map<PortableVertexDeclaration, std::unique_ptr<NativeVertexFormat>> NativeVertexLoaderMap;

namespace VertexLoaderManager
{
	static bool s_PrecompiledLoadersInitialized = false;
	
	static VertexLoaderMap s_VertexLoaderMap;
	static NativeVertexLoaderMap s_native_vertex_map;
	// TODO - change into array of pointers. Keep a map of all seen so far.
	
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

	std::string To_HexString(u32 in) {
		char hexString[2 * sizeof(u32) + 8];
		sprintf(hexString, "0x%08xu", in);
		return std::string(hexString);
	}

	void DumpLoadersCode()
	{
		std::vector<codeentry> entries;
		for (VertexLoaderMap::const_iterator iter = s_VertexLoaderMap.begin(); iter != s_VertexLoaderMap.end(); ++iter)
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
		std::string filename = StringFromFormat("%sG_%s_pvt.h", File::GetUserPath(D_DUMP_IDX).c_str(), LastGameCode.c_str());		
		std::string header;
		header.append("// Copyright 2013 Dolphin Emulator Project\n");
		header.append("// Licensed under GPLv2+\n");
		header.append("// Refer to the license.txt file included.\n");
		header.append("// Added for Ishiiruka by Tino\n");
		header.append("#pragma once\n");
		header.append("#include <map>\n");
		header.append("#include \"VideoCommon/NativeVertexFormat.h\"\n");
		header.append("class G_");
		header.append(LastGameCode);
		header.append("_pvt\n{\npublic:\n");
		header.append("static void Initialize(std::map<u64, TCompiledLoaderFunction> &pvlmap);\n");
		header.append("};\n");
		std::ofstream headerfile(filename);
		headerfile << header;
		headerfile.close();
		filename = StringFromFormat("%sG_%s_pvt.cpp", File::GetUserPath(D_DUMP_IDX).c_str(), LastGameCode.c_str());		
		sort(entries.begin(), entries.end());
		std::string sourcecode;
		sourcecode.append("#include \"VideoCommon/G_");
		sourcecode.append(LastGameCode);
		sourcecode.append("_pvt.h\"\n");
		sourcecode.append("#include \"VideoCommon/VertexLoader_Template.h\"\n\n");
		sourcecode.append("\n\nvoid G_");
		sourcecode.append(LastGameCode);
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
		for (VertexLoaderMap::const_iterator iter = s_VertexLoaderMap.begin(); iter != s_VertexLoaderMap.end(); ++iter)
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
		MarkAllAttrDirty();
		for (VertexLoaderBase*& vertexLoader : g_main_cp_state.vertex_loaders)
			vertexLoader = nullptr;
		LastGameCode = SConfig::GetInstance().m_strUniqueID;
	}

	void Shutdown()
	{
		if (s_VertexLoaderMap.size() > 0 && g_ActiveConfig.bDumpVertexLoaders)
			DumpLoadersCode();
		for (auto& p : s_VertexLoaderMap)
		{
			delete p.second;
		}
		s_VertexLoaderMap.clear();
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

	inline NativeVertexFormat* GetNativeVertexFormat(const PortableVertexDeclaration& format, u32 components)
	{
		auto& native = s_native_vertex_map[format];
		if (!native)
		{
			auto raw_pointer = g_vertex_manager->CreateNativeVertexFormat();
			native = std::unique_ptr<NativeVertexFormat>(raw_pointer);
			native->m_components = components;
			native->Initialize(format);
		}
		return native.get();
	}
	
	inline VertexLoaderBase *GetOrAddLoader(const TVtxDesc &VtxDesc, const VAT &VtxAttr)
	{
		VertexLoaderUID uid(VtxDesc, VtxAttr);
		VertexLoaderMap::iterator iter = s_VertexLoaderMap.find(uid);
		if (iter == s_VertexLoaderMap.end())
		{
			VertexLoaderBase *loader = VertexLoaderBase::CreateVertexLoader(VtxDesc, VtxAttr);
			loader->m_native_vertex_format = GetNativeVertexFormat(loader->m_native_vtx_decl, loader->m_native_components);
			VertexLoaderBase * fallback = loader->GetFallback();
			if (fallback)
			{
				fallback->m_native_vertex_format = GetNativeVertexFormat(fallback->m_native_vtx_decl, fallback->m_native_components);
			}
			s_VertexLoaderMap[uid] = loader;
			INCSTAT(stats.numVertexLoaders);
			return loader;
		}
		return iter->second;
	}

	void GetVertexSizeAndComponents(const VertexLoaderParameters &parameters, u32 &vertexsize, u32 &components)
	{
		if (parameters.needloaderrefresh)
		{
			s_CPULoaders[parameters.vtx_attr_group] = GetOrAddLoader(*parameters.VtxDesc, *parameters.VtxAttr);
		}
		vertexsize = s_CPULoaders[parameters.vtx_attr_group]->m_VertexSize;
		components = s_CPULoaders[parameters.vtx_attr_group]->m_native_components;
	}

	inline void UpdateLoader(const VertexLoaderParameters &parameters)
	{
		g_main_cp_state.vertex_loaders[parameters.vtx_attr_group] = GetOrAddLoader(*parameters.VtxDesc, *parameters.VtxAttr);
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
		if (g_nativeVertexFmt != nullptr && g_nativeVertexFmt != nativefmt)
		{
			VertexManagerBase::Flush();
		}
		VertexManagerBase::PrepareForAdditionalData(parameters.primitive, parameters.count, loader->m_native_stride);
		parameters.destination = VertexManagerBase::s_pCurBufferPointer;
		g_nativeVertexFmt = nativefmt;
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
