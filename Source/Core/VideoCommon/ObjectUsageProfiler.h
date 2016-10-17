// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


// Copyright 2016 Rodolfo Bogado
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the owner nor the names of its contributors may
//       be used to endorse or promote products derived from this software
//       without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#pragma once
#include <algorithm>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

typedef uint64_t pKey_t;

template <typename Tobj, typename TCaterogry, typename TInfo, typename TobjHasher> class ObjectUsageProfiler
{
public:
	ObjectUsageProfiler(pKey_t version) : m_version(version)
	{};
	void SetCategory(const TCaterogry& category)
	{
		if (m_categories.find(category) == m_categories.end())
		{
			m_categories[category] = { m_categories.size() + 1, 0 };
		}
		if (m_categories[category].usage_count < LLONG_MAX)
		{
			m_categories[category].usage_count++;
		}
		m_category_id = m_categories[category].Id;
		m_category_index = m_category_id / (sizeof(pKey_t) * 8);
		pKey_t max_category_index = std::max(m_max_category_index, m_category_index + 1);
		if (max_category_index > m_max_category_index)
		{
			m_max_category_index = max_category_index;
			for (auto& item : m_objects)
			{
				item.second.category_mask.resize(m_max_category_index);
			}
		}
		m_category_mask = pKey_t(1) << (m_category_id % (sizeof(pKey_t) * 8));
	}

	TInfo& GetOrAdd(const Tobj& obj)
	{
		ObjectMetadata& item = m_objects[obj];

		if (item.usage_count < LLONG_MAX)
		{
			item.usage_count++;
		}
		if (m_categories.size() == 1)
		{
			return item.info;
		}
		if (item.category_mask.size() < m_max_category_index)
		{
			item.category_mask.resize(m_max_category_index);
		}
		if ((item.category_mask[m_category_index] & m_category_mask) == 0)
		{
			item.category_mask[m_category_index] |= m_category_mask;
			if (item.category_count < LLONG_MAX)
			{
				item.category_count++;
			}
		}
		return item.info;
	}

	void ForEach(const std::function<void(TInfo&)>& func)
	{
		for (auto& item : m_objects)
		{
			func(item.second.info);
		}
	}

	void Clear(const std::function<void(TInfo&)>& eachfunc = {})
	{
		if (eachfunc)
		{
			for (auto& item : m_objects)
			{
				eachfunc(item.second.info);
			}
		}
		m_objects.clear();
		m_categories.clear();
		m_category_id = 0;
		m_category_index = 0;
		m_max_category_index = 0;
		m_category_mask = 0;
		m_version = 0;
	}

	void Clear(const std::function<void(const Tobj&, TInfo&)>& eachfunc = {})
	{
		if (eachfunc)
		{
			for (auto& item : m_objects)
			{
				eachfunc(item.first, item.second.info);
			}
		}
		m_objects.clear();
		m_categories.clear();
		m_category_id = 0;
		m_category_index = 0;
		m_max_category_index = 0;
		m_category_mask = 0;
		m_version = 0;
	}

	const TInfo* GetInfoIfexists(const Tobj& obj) const
	{
		auto it = m_objects.find(obj);
		if (it != m_objects.end())
		{
			return &it->second.info;
		}
		return nullptr;
	}

	size_t size() const
	{
		return m_objects.size();
	}

	void ForEachMostUsed(const std::function<void(const Tobj&)>& outfunc, const std::function<bool(TInfo&)>& filter = {}, pKey_t max_count = LLONG_MAX)
	{
		std::vector<std::pair<const Tobj, ObjectMetadata>*> elements;
		for (auto& item : m_objects)
		{
			if (filter && !filter(item.second.info))
			{
				continue;
			}
			elements.push_back(&item);
		}
		greater comparer;
		std::sort(elements.begin(), elements.end(), comparer);
		max_count = std::min(elements.size(), max_count);
		for (size_t i = 0; i < max_count; i++)
		{
			outfunc(elements[i]->first);
		}
	}

	void ForEachMostUsedByCategory(const TCaterogry& category, const std::function<void(const Tobj&, size_t total)>& outfunc, const std::function<bool(TInfo&)>& filter = {}, bool include_globals = false, size_t global_limit = 3, size_t max_count = LLONG_MAX)
	{
		if (m_categories.find(category) == m_categories.end() && !include_globals)
		{
			return;
		}
		include_globals = include_globals && (m_categories.find(category) == m_categories.end());
		pKey_t category_id = m_categories[category].Id;
		pKey_t category_index = category_id / (sizeof(pKey_t) * 8);
		pKey_t category_mask = pKey_t(1) << (category_id % (sizeof(pKey_t) * 8));
		std::vector<std::pair<const Tobj, ObjectMetadata>*> elements;
		for (auto& item : m_objects)
		{
			if (filter && !filter(item.second.info))
			{
				continue;
			}
			if (include_globals && item.second.category_count > global_limit)
			{
				elements.push_back(&item);
			}
			if (item.second.category_mask.size() <= category_index)
			{
				continue;
			}
			if ((item.second.category_mask[category_index] & category_mask) != 0)
			{
				elements.push_back(&item);
			}
		}
		greater comparer;
		std::sort(elements.begin(), elements.end(), comparer);
		max_count = std::min(elements.size(), max_count);
		for (size_t i = 0; i < max_count; i++)
		{
			outfunc(elements[i]->first, max_count);
		}
	}

	void PersistToFile(const std::string& path, bool allcategories = false)
	{
		std::ofstream out(path, std::ofstream::binary);
		if (!out.is_open())
		{
			return;
		}
		bwrite(out, m_version);
		size_t category_count = allcategories ? m_categories.size() : 1;
		bwrite(out, category_count);
		for (auto& item : m_categories)
		{
			if (allcategories || item.second.Id == m_category_id)
			{
				bwrite(out, item.first);
				bwrite(out, item.second);
			}
		}
		size_t object_count = 0;
		if (allcategories || m_categories.size() == 1)
		{
			object_count = m_objects.size();
		}
		else
		{
			for (auto& item : m_objects)
			{
				if ((item.second.category_mask[m_category_index] & m_category_mask) != 0)
				{
					object_count++;
				}
			}
		}
		bwrite(out, object_count);
		for (auto& item : m_objects)
		{
			if (allcategories || m_categories.size() == 1 || (item.second.category_mask[m_category_index] & m_category_mask) != 0)
			{
				bwrite(out, item.first);
				if (allcategories)
				{
					bwrite(out, item.second.category_count);
				}
				bwrite(out, item.second.usage_count);
				if (allcategories)
				{
					size_t mask_size = item.second.category_mask.size();
					bwrite(out, mask_size);
					if (mask_size > 0)
					{
						out.write(reinterpret_cast<char*>(item.second.category_mask.data()), sizeof(size_t) * mask_size);
					}
				}
			}
		}
	}

	void Persist()
	{
		if (m_storage.length() > 0)
		{
			PersistToFile(m_storage);
		}
	}

	void ReadFromFile(const std::string& path, bool multicategory = false)
	{
		std::ifstream input(path, std::ifstream::binary);
		if (!input.is_open())
		{
			return;
		}
		pKey_t version = 0;
		bread(input, version);
		if (version != m_version)
		{
			return;
		}
		pKey_t category_count = 0;
		bread(input, category_count);
		if (!multicategory)
		{
			category_count = m_categories.size() + 1;
		}
		pKey_t category_index = (category_count + 1) / (sizeof(pKey_t) * 8);
		pKey_t category_mask = pKey_t(1) << (category_count % (sizeof(pKey_t) * 8));
		if (m_max_category_index < category_index + 1)
		{
			m_max_category_index = category_index + 1;
			for (auto& item : m_objects)
			{
				item.second.category_mask.resize(m_max_category_index);
			}
		}
		for (size_t i = 0; i < (multicategory ? category_count : 1); i++)
		{
			TCaterogry key;
			bread(input, key);
			CategoryMetadata data;
			bread(input, data);
			if (!multicategory)
			{
				data.Id = category_count;
			}
			m_categories.emplace(std::move(key), std::move(data));
		}
		pKey_t object_count = 0;
		bread(input, object_count);
		for (size_t i = 0; i < object_count; i++)
		{
			Tobj key;
			bread(input, key);
			ObjectMetadata& data = m_objects[key];
			if (multicategory)
			{
				bread(input, data.category_count);
			}
			else
			{
				data.category_count = 0;
			}
			bread(input, data.usage_count);
			if (multicategory)
			{
				data.category_mask.resize(m_max_category_index);
				pKey_t mask_size = 0;
				bread(input, mask_size);
				if (mask_size)
				{
					input.read(reinterpret_cast<char*>(data.category_mask.data()), sizeof(pKey_t) * mask_size);
				}
			}
			if (!multicategory)
			{
				data.category_mask.resize(m_max_category_index);
				if ((data.category_mask[category_index] & category_mask) == 0)
				{
					data.category_count++;
					data.category_mask[category_index] |= category_mask;
				}
			}
		}
	}

	void SetStorage(const std::string& storagefile)
	{
		m_storage = storagefile;
	}

	static ObjectUsageProfiler<Tobj, TCaterogry, TInfo, TobjHasher>* Create(TCaterogry catid, pKey_t version, const std::string& global_filename, const std::string filename)
	{
		if (!File::Exists(File::GetUserPath(D_SHADERUIDCACHE_IDX)))
			File::CreateDir(File::GetUserPath(D_SHADERUIDCACHE_IDX));
		std::string global_profile_filename = StringFromFormat("%s%s.usage", File::GetUserPath(D_SHADERUIDCACHE_IDX).c_str(), global_filename.c_str());
		std::string profile_filename = StringFromFormat("%s%s.usage",
			File::GetUserPath(D_SHADERUIDCACHE_IDX).c_str(),
			filename.c_str());
		bool profile_exists = File::Exists(profile_filename);
		bool global_profile_exists = File::Exists(global_profile_filename);
		ObjectUsageProfiler<Tobj, TCaterogry, TInfo, TobjHasher>* output = new ObjectUsageProfiler<Tobj, TCaterogry, TInfo, TobjHasher>(version);
		if (profile_exists)
		{
			output->ReadFromFile(profile_filename);
		}
		else if (global_profile_exists)
		{
			output->ReadFromFile(global_profile_filename, true);
		}
		output->SetCategory(catid);
		output->SetStorage(profile_filename);
		return output;
	}

private:
	struct ObjectMetadata
	{
		pKey_t category_count;
		pKey_t usage_count;
		std::vector<pKey_t> category_mask;
		TInfo info;
		ObjectMetadata() : category_count(0), usage_count(0)
		{}
	};
	struct greater
	{
		bool operator()(std::pair<const Tobj, ObjectMetadata>* const &first, std::pair<const Tobj, ObjectMetadata>* const &second) const
		{
			if (first->second.category_count > second->second.category_count)
			{
				return true;
			}
			if (first->second.category_count == second->second.category_count)
			{
				return  first->second.usage_count > second->second.usage_count;
			}
			return false;
		}
	};
	struct CategoryMetadata
	{
		pKey_t Id;
		pKey_t usage_count;
	};
	std::unordered_map<Tobj, ObjectMetadata, TobjHasher> m_objects;
	std::map<TCaterogry, CategoryMetadata> m_categories;
	pKey_t m_category_id = {};
	pKey_t m_category_index = {};
	pKey_t m_max_category_index = {};
	pKey_t m_category_mask = {};
	pKey_t m_version = {};
	std::string m_storage;
	template<typename T>
	inline void bwrite(std::ofstream& out, const T& t)
	{
		out.write(reinterpret_cast<const char*>(&t), sizeof(T));
	}

	template<typename T>
	inline void bread(std::ifstream& in, T& t)
	{
		in.read(reinterpret_cast<char*>(&t), sizeof(T));
	}
};
