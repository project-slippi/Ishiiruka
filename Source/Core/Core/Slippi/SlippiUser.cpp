#include "Common/Common.h"
#include "Common/Logging/Log.h"

#include "SlippiUser.h"

#include "SlippiRustExtensions.h"

// Takes a RustChatMessages pointer and extracts messages from them, then
// frees the underlying memory safely.
std::vector<std::string> ConvertChatMessagesFromRust(RustChatMessages *rsMessages)
{
	std::vector<std::string> chatMessages;

	for (int i = 0; i < rsMessages->len; i++)
	{
		std::string message = std::string(rsMessages->data[i]);
		chatMessages.push_back(message);
	}

	slprs_user_free_messages(rsMessages);

	return chatMessages;
}

SlippiUser::SlippiUser(uintptr_t rs_exi_device_ptr)
{
	slprs_exi_device_ptr = rs_exi_device_ptr;

	InitUserRank();
}

SlippiUser::~SlippiUser() {}

void SlippiUser::InitUserRank() {
	userRank.rank = SlippiUser::SlippiRank (0);
	userRank.ratingOrdinal = 0.0f;
	userRank.globalPlacing = 0;
	userRank.regionalPlacing = 0;
	userRank.ratingUpdateCount = 0;
	userRank.ratingChange = 0.0f;
	userRank.rankChange = 0;
}

SlippiUser::RankInfo SlippiUser::GetRankInfo() {
	return userRank;
}

SlippiUser::SlippiRank SlippiUser::GetRank(float ratingOrdinal, int globalPlacing, int regionalPlacing, int ratingUpdateCount)
{
	if (ratingUpdateCount < 5)
		return RANK_UNRANKED;
	if (ratingOrdinal > 0 && ratingOrdinal <= 765.42)
		return RANK_BRONZE_1;
	if (ratingOrdinal > 765.43 && ratingOrdinal <= 913.71)
		return RANK_BRONZE_2;
	if (ratingOrdinal > 913.72 && ratingOrdinal <= 1054.86)
		return RANK_BRONZE_3;
	if (ratingOrdinal > 1054.87 && ratingOrdinal <= 1188.87)
		return RANK_SILVER_1;
	if (ratingOrdinal > 1188.88 && ratingOrdinal <= 1315.74)
		return RANK_SILVER_2;
	if (ratingOrdinal > 1315.75 && ratingOrdinal <= 1435.47)
		return RANK_SILVER_3;
	if (ratingOrdinal > 1435.48 && ratingOrdinal <= 1548.06)
		return RANK_GOLD_1;
	if (ratingOrdinal > 1548.07 && ratingOrdinal <= 1653.51)
		return RANK_GOLD_2;
	if (ratingOrdinal > 1653.52 && ratingOrdinal <= 1751.82)
		return RANK_GOLD_3;
	if (ratingOrdinal > 1751.83 && ratingOrdinal <= 1842.99)
		return RANK_PLATINUM_1;
	if (ratingOrdinal > 1843 && ratingOrdinal <= 1927.02)
		return RANK_PLATINUM_2;
	if (ratingOrdinal > 1927.03 && ratingOrdinal <= 2003.91)
		return RANK_PLATINUM_3;
	if (ratingOrdinal > 2003.92 && ratingOrdinal <= 2073.66)
		return RANK_DIAMOND_1;
	if (ratingOrdinal > 2073.67 && ratingOrdinal <= 2136.27)
		return RANK_DIAMOND_2;
	if (ratingOrdinal > 2136.28 && ratingOrdinal <= 2191.74)
		return RANK_DIAMOND_3;
	if (ratingOrdinal >= 2191.75 && globalPlacing && regionalPlacing)
		return RANK_GRANDMASTER;
	if (ratingOrdinal > 2191.75 && ratingOrdinal <= 2274.99)
		return RANK_MASTER_1;
	if (ratingOrdinal > 2275 && ratingOrdinal <= 2350)
		return RANK_MASTER_2;
	if (ratingOrdinal > 2350)
		return RANK_MASTER_3;
	return RANK_UNRANKED;
}

SlippiUser::RankInfo SlippiUser::FetchUserRank(std::string connectCode)
{
	RankInfo info;

	const char *query = 
		"fragment profileFields on NetplayProfile {\n"
		"  id\n"
		"  ratingOrdinal\n"
		"  ratingUpdateCount\n"
		"  wins\n"
		"  losses\n"
		"  dailyGlobalPlacement\n"
		"  dailyRegionalPlacement\n"
		"  continent\n  characters {\n"
		"    id\n"
		"    character\n"
		"    gameCount\n"
		"    __typename\n"
		"  }\n"
		"  __typename\n"
		"}\n"
		"\n"
		"fragment userProfilePage on User {\n"
		"  fbUid\n"
		"  displayName\n"
		"  connectCode {\n"
		"    code\n"
		"    __typename\n"
		"  }\n"
		"  status\n"
		"  activeSubscription {\n"
		"    level\n"
		"    hasGiftSub\n"
		"    __typename\n"
		"  }\n"
		"  rankedNetplayProfile {\n"
		"    ...profileFields\n"
		"    __typename\n"
		"  }\n"
		"  netplayProfiles {\n"
		"    ...profileFields\n"
		"    season {\n"
		"      id\n"
		"      startedAt\n"
		"      endedAt\n"
		"      name\n"
		"      status\n"
		"      __typename\n"
		"    }\n"
		"    __typename\n"
		"  }\n"
		"  __typename\n"
		"}\n"
		"\n"
		"query AccountManagementPageQuery($cc: String!, $uid: String!) {\n"
		"  getUser(fbUid: $uid) {\n"
		"    ...userProfilePage\n"
		"    __typename\n"
		"  }\n"
		"  getConnectCode(code: $cc) {\n"
		"    user {\n"
		"      ...userProfilePage\n"
		"      __typename\n"
		"    }\n"
		"    __typename\n"
		"  }\n"
		"}\n";

	std::string url = "https://gql-gateway-dot-slippi.uc.r.appspot.com/graphql";
	json body = {{"operationName", "AccountManagementPageQuery"},
		{"variables", {{"cc", connectCode}, {"uid", connectCode}}},
		{"query", query}};

	// INFO_LOG(SLIPPI_ONLINE, "Preparing request...");
	// Perform curl request
	std::string resp;
	curl_easy_setopt(m_curl, CURLOPT_URL, (url).c_str());
	curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &resp);
	curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, (body.dump()).c_str());
	curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, "POST");

	CURLcode res = curl_easy_perform(m_curl);

	// INFO_LOG(SLIPPI_ONLINE, "Request sent");
	if (res != 0)
	{
	    ERROR_LOG(SLIPPI, "[User] Error fetching user info from server, code: %d", res);
	    //return info;
	}

	long responseCode;
	curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &responseCode);
	if (responseCode != 200)
	{
	    ERROR_LOG(SLIPPI, "[User] Server responded with non-success status: %d", responseCode);
	    //return info;
	}

	auto r = json::parse(resp);
	auto rankedObject = r["data"]["getConnectCode"]["user"]["rankedNetplayProfile"];

	float ratingOrdinal = rankedObject["ratingOrdinal"];
	INFO_LOG(SLIPPI_ONLINE, "Rating: %0000.00f", ratingOrdinal);

	u8 global = (rankedObject["dailyGlobalPlacement"]).is_null() ? 0 : rankedObject["dailyGlobalPlacement"];
	INFO_LOG(SLIPPI_ONLINE, "Global Placing: %d", global);

	u8 regional = (rankedObject["dailyRegionalPlacement"]).is_null() ? 0 : rankedObject["dailyRegionalPlacement"];
	INFO_LOG(SLIPPI_ONLINE, "Regional Placing: %d", regional);

	u8 ratingUpdateCount = (rankedObject["ratingUpdateCount"]).is_null() ? 0 : rankedObject["ratingUpdateCount"];
	INFO_LOG(SLIPPI_ONLINE, "Rating Update Count: %d", ratingUpdateCount);

	SlippiRank rank = GetRank(ratingOrdinal, global, regional, ratingUpdateCount);
	INFO_LOG(SLIPPI_ONLINE, "Rank: %d", rank);

	float ratingChange = (userRank.ratingOrdinal > 0.001f) ? ratingOrdinal - userRank.ratingOrdinal : 0;
	INFO_LOG(SLIPPI_ONLINE, "Rating Change: %0.1f", ratingChange);

	int rankChange = (userRank.rank > 0.001f) ? rank - userRank.rank : 0;
	INFO_LOG(SLIPPI_ONLINE, "userRank: %d", userRank.rank);
	INFO_LOG(SLIPPI_ONLINE, "Rank Change: %d", rankChange);

	info.rank = rank;
	info.ratingOrdinal = ratingOrdinal;
	info.globalPlacing = global;
	info.regionalPlacing = regional;
	info.ratingUpdateCount = ratingUpdateCount;
	info.ratingChange = ratingChange;
	info.rankChange = rankChange;

	// Set user rank
	userRank = info;

	return info;
}

bool SlippiUser::AttemptLogin()
{
	return slprs_user_attempt_login(slprs_exi_device_ptr);
}

void SlippiUser::OpenLogInPage()
{
	slprs_user_open_login_page(slprs_exi_device_ptr);
}

void SlippiUser::ListenForLogIn()
{
	slprs_user_listen_for_login(slprs_exi_device_ptr);
}

bool SlippiUser::UpdateApp()
{
	return slprs_user_update_app(slprs_exi_device_ptr);
}

void SlippiUser::LogOut()
{
	slprs_user_logout(slprs_exi_device_ptr);
}

void SlippiUser::OverwriteLatestVersion(std::string version)
{
	slprs_user_overwrite_latest_version(slprs_exi_device_ptr, version.c_str());
}

SlippiUser::UserInfo SlippiUser::GetUserInfo()
{
	SlippiUser::UserInfo userInfo;

	RustUserInfo *info = slprs_user_get_info(slprs_exi_device_ptr);
	userInfo.uid = std::string(info->uid);
	userInfo.playKey = std::string(info->play_key);
	userInfo.displayName = std::string(info->display_name);
	userInfo.connectCode = std::string(info->connect_code);
	userInfo.latestVersion = std::string(info->latest_version);
	slprs_user_free_info(info);

	return userInfo;
}

std::vector<std::string> SlippiUser::GetDefaultChatMessages()
{
	RustChatMessages *chatMessages = slprs_user_get_default_messages(slprs_exi_device_ptr);
	return ConvertChatMessagesFromRust(chatMessages);
}

std::vector<std::string> SlippiUser::GetUserChatMessages()
{
	RustChatMessages *chatMessages = slprs_user_get_messages(slprs_exi_device_ptr);
	return ConvertChatMessagesFromRust(chatMessages);
}

bool SlippiUser::IsLoggedIn()
{
	return slprs_user_get_is_logged_in(slprs_exi_device_ptr);
}
