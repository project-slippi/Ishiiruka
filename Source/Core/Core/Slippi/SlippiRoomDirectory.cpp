#include "SlippiRoomDirectory.h"

#include <curl/curl.h>
#include <json.hpp>

#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"

using json = nlohmann::json;

namespace
{
size_t receive(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	static_cast<std::string *>(userdata)->append(ptr, size * nmemb);
	return size * nmemb;
}

// Rooms directory backed by Supabase. Each operation is a Postgres function called through
// Supabase's REST API with a publishable key, so the key only allows calling those functions
class SupabaseRoomDirectory : public SlippiRoomDirectory
{
  public:
	SupabaseRoomDirectory(std::string url, std::string key)
	    : m_url(std::move(url))
	    , m_key(std::move(key))
	{
	}

	Registration Register(const RoomInfo &info) override
	{
		json args = {
		    {"p_port", info.port},
		    {"p_listed", info.listed},
		    {"p_password", info.password.empty() ? json(nullptr) : json(info.password)},
		    {"p_host_name", info.hostName},
		    {"p_host_code", info.hostCode},
		    {"p_mode", info.mode},
		    {"p_stage_mode", info.stageMode},
		    {"p_capacity", info.capacity},
		    {"p_utc_offset", info.utcOffset},
		};

		Registration reg;
		json resp;
		if (!call("room_create", args, resp) || !resp.is_array() || resp.empty())
			return reg;

		try
		{
			reg.code = resp[0].value("code", "");
			reg.hostToken = resp[0].value("host_token", "");
		}
		catch (const json::exception &e)
		{
			logBadReply("room_create", e);
			return {};
		}
		return reg;
	}

	ActivityStatus Activity(const Registration &reg, u8 memberCount, u16 port) override
	{
		json args = {{"p_code", reg.code},
		             {"p_host_token", reg.hostToken},
		             {"p_member_count", memberCount},
		             {"p_port", port ? json(port) : json(nullptr)}};
		json resp;
		if (!call("room_activity", args, resp) || !resp.is_string())
			return ActivityStatus::UNAVAILABLE;

		std::string status = resp.get<std::string>();
		if (status == "gone")
			return ActivityStatus::GONE;
		if (status == "replaced")
			return ActivityStatus::REPLACED;
		return ActivityStatus::OK;
	}

	TakeOverResult TakeOver(const std::string &code, int generation, const RoomInfo &info) override
	{
		json args = {
		    {"p_code", code},
		    {"p_generation", generation},
		    {"p_port", info.port},
		    {"p_host_name", info.hostName},
		    {"p_host_code", info.hostCode},
		};

		TakeOverResult result;
		json resp;
		if (!call("room_take_over", args, resp) || !resp.is_array() || resp.empty())
			return result;

		try
		{
			std::string status = resp[0].value("status", "");
			if (status == "ok")
			{
				result.status = TakeOverStatus::OK;
				result.reg.code = code;
				result.reg.hostToken = resp[0].value("host_token", "");
				result.generation = resp[0].value("generation", 0);
			}
			else if (status == "taken")
			{
				result.status = TakeOverStatus::TAKEN;
				result.generation = resp[0].value("generation", 0);
			}
			else if (status == "gone")
			{
				result.status = TakeOverStatus::GONE;
			}
		}
		catch (const json::exception &e)
		{
			logBadReply("room_take_over", e);
			return {};
		}
		return result;
	}

	void Unregister(const Registration &reg) override
	{
		json args = {{"p_code", reg.code}, {"p_host_token", reg.hostToken}};
		json resp;
		call("room_close", args, resp);
	}

	bool JoinRequests(const Registration &reg, s64 afterId, std::vector<JoinRequest> &out) override
	{
		json args = {{"p_code", reg.code}, {"p_host_token", reg.hostToken}, {"p_after", afterId}};
		json resp;
		if (!call("room_join_requests", args, resp) || !resp.is_array())
			return false;

		out.clear();
		try
		{
			for (const json &el : resp)
			{
				JoinRequest request;
				request.id = el.value("id", static_cast<s64>(0));
				request.address = el.value("address", "");
				out.push_back(request);
			}
		}
		catch (const json::exception &e)
		{
			out.clear();
			logBadReply("room_join_requests", e);
			return false;
		}
		return true;
	}

	JoinResult Join(const std::string &code, const std::string &password, u16 port) override
	{
		json args = {{"p_code", code},
		             {"p_password", password.empty() ? json(nullptr) : json(password)},
		             {"p_port", port ? json(port) : json(nullptr)}};

		JoinResult result;
		json resp;
		if (!call("room_join", args, resp) || !resp.is_array() || resp.empty())
			return result;

		try
		{
			std::string status = resp[0].value("status", "");
			if (status == "ok")
			{
				result.status = JoinStatus::OK;
				result.address = resp[0].value("address", "");
				result.hostName = resp[0].value("host_name", "");
				result.hostCode = resp[0].value("host_code", "");
				result.generation = resp[0].value("generation", 0);
			}
			else if (status == "not_found")
				result.status = JoinStatus::NOT_FOUND;
			else if (status == "wrong_password")
				result.status = JoinStatus::WRONG_PASSWORD;
			else if (status == "full")
				result.status = JoinStatus::FULL;
			else if (status == "locked")
				result.status = JoinStatus::LOCKED;
		}
		catch (const json::exception &e)
		{
			logBadReply("room_join", e);
			return {};
		}

		return result;
	}

	bool Exists(const std::string &code, bool &exists) override
	{
		json resp;
		if (!call("room_exists", {{"p_code", code}}, resp) || !resp.is_boolean())
			return false;

		exists = resp.get<bool>();
		return true;
	}

	bool List(std::vector<Listing> &out) override
	{
		json resp;
		if (!call("room_list", json::object(), resp) || !resp.is_array())
			return false;

		out.clear();
		try
		{
			for (const json &el : resp)
			{
				Listing l;
				l.code = el.value("code", "");
				l.hostName = el.value("host_name", "");
				l.mode = el.value("mode", 0);
				l.stageMode = el.value("stage_mode", 0);
				l.capacity = el.value("capacity", 0);
				l.memberCount = el.value("member_count", 0);

				// Rooms from countries outside the list's regions have none
				auto region = el.find("region");
				if (region != el.end() && region->is_number())
					l.region = region->get<u8>();
				out.push_back(l);
			}
		}
		catch (const json::exception &e)
		{
			out.clear();
			logBadReply("room_list", e);
			return false;
		}
		return true;
	}

  private:
	// A reply with a field of the wrong type throws while it's read, and counts as a failed call
	static void logBadReply(const char *fn, const json::exception &e)
	{
		ERROR_LOG(SLIPPI_ONLINE, "[Rooms] Bad reply from directory call %s: %s", fn, e.what());
	}

	bool call(const std::string &fn, const json &args, json &resp)
	{
		CURL *curl = curl_easy_init();
		if (!curl)
			return false;

		std::string endpoint = m_url + "/rest/v1/rpc/" + fn;
		std::string body = args.dump();
		std::string received;

		struct curl_slist *headers = nullptr;
		headers = curl_slist_append(headers, "Content-Type: application/json");
		headers = curl_slist_append(headers, ("apikey: " + m_key).c_str());

		curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &receive);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &received);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000);

		// Called from worker threads, where a timeout must not use signals
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

		// The host's address is taken from this connection, and rooms connect over IPv4
		curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

		CURLcode res = curl_easy_perform(curl);
		long status = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);

		if (res != CURLE_OK || status < 200 || status >= 300)
		{
			ERROR_LOG(SLIPPI_ONLINE, "[Rooms] Directory call %s failed, curl: %d, status: %ld", fn.c_str(), res,
			          status);
			return false;
		}

		resp = json::parse(received, nullptr, false);
		return !resp.is_discarded();
	}

	std::string m_url;
	std::string m_key;
};

// Used when no directory is configured. Rooms still work locally, they just can't be joined
class NoRoomDirectory : public SlippiRoomDirectory
{
  public:
	Registration Register(const RoomInfo &info) override { return {}; }
	ActivityStatus Activity(const Registration &reg, u8 memberCount, u16 port) override
	{
		return ActivityStatus::UNAVAILABLE;
	}
	bool JoinRequests(const Registration &reg, s64 afterId, std::vector<JoinRequest> &out) override { return false; }
	TakeOverResult TakeOver(const std::string &code, int generation, const RoomInfo &info) override { return {}; }
	void Unregister(const Registration &reg) override {}
	JoinResult Join(const std::string &code, const std::string &password, u16 port) override { return {}; }
	bool List(std::vector<Listing> &out) override { return false; }
	bool Exists(const std::string &code, bool &exists) override { return false; }
};
} // namespace

std::unique_ptr<SlippiRoomDirectory> SlippiRoomDirectory::Create()
{
	const SConfig &config = SConfig::GetInstance();
	if (config.m_slippiRoomsDirectoryUrl.empty() || config.m_slippiRoomsDirectoryKey.empty())
	{
		WARN_LOG(SLIPPI_ONLINE, "[Rooms] No rooms directory is configured");
		return std::make_unique<NoRoomDirectory>();
	}

	return std::make_unique<SupabaseRoomDirectory>(config.m_slippiRoomsDirectoryUrl, config.m_slippiRoomsDirectoryKey);
}
