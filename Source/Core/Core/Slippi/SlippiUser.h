#pragma once

#include "Common/CommonTypes.h"
#include <atomic>
#include <curl/curl.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// This class is currently a shim for the Rust user interface. We're doing it this way
// to begin migrating things over without needing to do larger invasive changes.
//
// The remaining methods on here are simply layers that direct the call over to the Rust
// side. A quirk of this is that we're using the EXI device pointer, so this class absolutely
// cannot outlive the EXI device - but we control that and just need to do our due diligence
// when making changes.
class SlippiUser
{
  public:
	enum SlippiRank
	{
		RANK_UNRANKED,
		RANK_BRONZE_1,
		RANK_BRONZE_2,
		RANK_BRONZE_3,
		RANK_SILVER_1,
		RANK_SILVER_2,
		RANK_SILVER_3,
		RANK_GOLD_1,
		RANK_GOLD_2,
		RANK_GOLD_3,
		RANK_PLATINUM_1,
		RANK_PLATINUM_2,
		RANK_PLATINUM_3,
		RANK_DIAMOND_1,
		RANK_DIAMOND_2,
		RANK_DIAMOND_3,
		RANK_MASTER_1,
		RANK_MASTER_2,
		RANK_MASTER_3,
		RANK_GRANDMASTER,
	};

	// This type is filled in with data from the Rust side.
	// Eventually, this entire class will disappear.
	struct UserInfo
	{
		std::string uid = "";
		std::string playKey = "";
		std::string displayName = "";
		std::string connectCode = "";
		std::string latestVersion = "";

		int port;

		std::vector<std::string> chatMessages;
	};

	struct RankInfo
	{
		SlippiRank rank;
		float ratingOrdinal;
		u8 globalPlacing;
		u8 regionalPlacing;
		u8 ratingUpdateCount;
		float ratingChange;
		int rankChange;
	};

	struct RankInfo
	{
		SlippiRank rank;
		float ratingOrdinal;
		u8 globalPlacing;
		u8 regionalPlacing;
		u8 ratingUpdateCount;
		float ratingChange;
		int rankChange;
	};

	SlippiUser(uintptr_t rs_exi_device_ptr);
	~SlippiUser();

	bool AttemptLogin();
	void OpenLogInPage();
	bool UpdateApp();
	void ListenForLogIn();
	void LogOut();
	void OverwriteLatestVersion(std::string version);
	UserInfo GetUserInfo();
	std::vector<std::string> GetUserChatMessages();
	std::vector<std::string> GetDefaultChatMessages();
	bool IsLoggedIn();
	void FileListenThread();

	RankInfo FetchUserRank(std::string connectCode);
	RankInfo GetRankInfo();
	void InitUserRank();

	SlippiRank GetRank(float ratingOrdinal, int globalPlacing, int regionalPlacing, int ratingUpdateCount);

	const static std::vector<std::string> defaultChatMessages;

  protected:
	// A pointer to a "shadow" EXI Device that lives on the Rust side of things.
	// Do *not* do any cleanup of this! The EXI device will handle it.
	uintptr_t slprs_exi_device_ptr;
};
