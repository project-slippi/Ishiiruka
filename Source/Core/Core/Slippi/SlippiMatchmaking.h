#pragma once

#include "Common/CommonTypes.h"
#include "Common/Thread.h"
#include "Core/Slippi/SlippiNetplay.h"
#include "Core/Slippi/SlippiUser.h"

#ifndef _WIN32
#include <netdb.h>
#include <arpa/inet.h>
#endif

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <json.hpp>

// Defined in SlippiRustExtensions.h. Forward-declared here because not every project that
// includes this header has the Rust include path.
struct SlippiStunObservation;

using json = nlohmann::json;

// Drives a search for opponents. The conversation with the matchmaking service happens on the
// Rust side; this class owns the UDP socket that will be used for netplay, learns its public
// address through STUN, keeps its NAT mapping alive while queued, and connects to the opponents
// once a match arrives.
class SlippiMatchmaking
{
  public:
	SlippiMatchmaking(uintptr_t rs_exi_device_ptr, SlippiUser *user);
	~SlippiMatchmaking();

	enum OnlinePlayMode
	{
		RANKED = 0,
		UNRANKED = 1,
		DIRECT = 2,
		TEAMS = 3,
		PARTY = 4,
	};

	enum ProcessState
	{
		IDLE,
		INITIALIZING,
		MATCHMAKING,
		OPPONENT_CONNECTING,
		CONNECTION_SUCCESS,
		ERROR_ENCOUNTERED,
	};

	enum SlippiRank
	{
		Unranked,
		Bronze1,
		Bronze2,
		Bronze3,
		Silver1,
		Silver2,
		Silver3,
		Gold1,
		Gold2,
		Gold3,
		Platinum1,
		Platinum2,
		Platinum3,
		Diamond1,
		Diamond2,
		Diamond3,
		Master1,
		Master2,
		Master3,
		Grandmaster
	};

	struct MatchSearchSettings
	{
		OnlinePlayMode mode = OnlinePlayMode::RANKED;
		std::string connectCode = "";
	};

	struct MatchmakeResult
	{
		std::string id = "";
		std::vector<SlippiUser::UserInfo> players;
		std::vector<u16> stages;
		u32 items;
	};

	void FindMatch(MatchSearchSettings settings);
	void MatchmakeThread();
	ProcessState GetMatchmakeState();
	bool IsSearching();
	std::unique_ptr<SlippiNetplayClient> GetNetplayClient();
	std::string GetErrorMessage();
	int LocalPlayerIndex();
	std::vector<SlippiUser::UserInfo> GetPlayerInfo();
	std::string GetPlayerName(u8 port);
	SlippiRank GetPlayerRank(u8 port);
	std::vector<u16> GetStages();
	u8 RemotePlayerCount();
	MatchmakeResult GetMatchmakeResult();
	static bool IsFixedRulesMode(OnlinePlayMode mode);

  protected:
	// Public STUN servers used to learn the netplay socket's public address, tried in order
	// until two have answered. Two different servers are needed to tell a cone NAT, which
	// reuses one public port, from a symmetric NAT, which hands out a new one per
	// destination. The non-Google entries are for networks that block Google.
	const std::vector<std::pair<std::string, u16>> STUN_SERVERS = {
	    {"stun.l.google.com", 19302},
	    {"stun1.l.google.com", 19302},
	    {"stun.cloudflare.com", 3478},
	    {"global.stun.twilio.com", 3478},
	};

	// How often to send a packet from the netplay socket while queued so the NAT mapping the
	// opponent will use does not expire.
	const u32 STUN_KEEPALIVE_INTERVAL_MS = 15000;

	// Bound to the netplay port for the duration of the queue. Only its socket is used.
	ENetHost *m_client;
	ENetAddress m_stunAddr;
	bool m_hasStunAddr = false;
	u32 m_lastKeepaliveMs = 0;

	// Identifies the Rust-side session this object started, so tearing this object down
	// cannot cancel a newer search started by another instance.
	u64 m_sessionId = 0;

	std::default_random_engine generator;

	bool isMmTerminated = false;

	std::thread m_matchmakeThread;

	MatchSearchSettings m_searchSettings;

	ProcessState m_state;
	std::string m_errorMsg = "";

	SlippiUser *m_user;

	int m_isSwapAttempt = false;

	int m_hostPort;
	int m_localPlayerIndex;
	std::vector<std::string> m_remoteIps;
	MatchmakeResult m_mmResult;
	std::vector<SlippiUser::UserInfo> m_playerInfo;
	std::vector<u16> m_allowedStages;
	bool m_joinedLobby;
	bool m_isHost;

	std::unique_ptr<SlippiNetplayClient> m_netplayClient;

	// A pointer to a "shadow" EXI Device that lives on the Rust side of things.
	// Do *not* do any cleanup of this! The EXI device will handle it.
	uintptr_t slprs_exi_device_ptr;

	const std::unordered_map<ProcessState, bool> searchingStates = {
	    {ProcessState::INITIALIZING, true},
	    {ProcessState::MATCHMAKING, true},
	    {ProcessState::OPPONENT_CONNECTING, true},
	};

	void terminateMmConnection();

	bool stunBindingRequest(const ENetAddress &server, SlippiStunObservation &result);
	void sendStunKeepalive();

	void startMatchmaking();
	void handleMatchmaking();
	void handleConnecting();
};
