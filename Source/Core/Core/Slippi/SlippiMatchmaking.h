#pragma once

#include "Common/CommonTypes.h"
#include "Common/Thread.h"
#include "Core/Slippi/SlippiNetplay.h"
#include "Core/Slippi/SlippiUser.h"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <json.hpp>
#include <unordered_map>
#include <vector>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

class SlippiMatchmaking
{
  public:
	SlippiMatchmaking(SlippiUser *user);
	~SlippiMatchmaking();

	enum OnlinePlayMode
	{
		RANKED = 0,
		UNRANKED = 1,
		DIRECT = 2,
		TEAMS = 3,
	};

	enum Regions
	{
		EC = 0,
		WC = 1,
		EU = 2,
		SA = 3,
		LA = 4,
		AU = 5,
		AS = 6,
	};

	std::unordered_map<u8, std::string> RegionMap = {
        {EC, "EC"},
        {WC, "WC"},
        {EU, "EU"},
        {SA, "SA"},
        {LA, "LA"},
        {AU, "AU"},
        {AS, "AS"},
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

	struct MatchSearchSettings
	{
		OnlinePlayMode mode = OnlinePlayMode::RANKED;
		OnlinePlayMode submode = OnlinePlayMode::RANKED;
		std::string connectCode = "";
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
	std::vector<u16> GetStages();
	u8 RemotePlayerCount();
	static bool IsFixedRulesMode(OnlinePlayMode mode);

  protected:
	const std::string MM_HOST_DEV = "104.154.50.102"; // mm
	const std::string MM_HOST_PROD = "35.184.161.98"; // mm-2
	const u16 MM_PORT = 43113;

	std::string MM_HOST = "";

	ENetHost *m_client;
	ENetPeer *m_server;

	std::default_random_engine generator;

	bool isMmConnected = false;

	std::thread m_matchmakeThread;

	MatchSearchSettings m_searchSettings;

	ProcessState m_state;
	std::string m_errorMsg = "";

	SlippiUser *m_user;

	int m_isSwapAttempt = false;

	int m_hostPort;
	int m_localPlayerIndex;
	std::vector<std::string> m_remoteIps;
	std::vector<SlippiUser::UserInfo> m_playerInfo;
	std::vector<u16> m_allowedStages;
	bool m_joinedLobby;
	bool m_isHost;

	std::unique_ptr<SlippiNetplayClient> m_netplayClient;

	const std::unordered_map<ProcessState, bool> searchingStates = {
	    {ProcessState::INITIALIZING, true},
	    {ProcessState::MATCHMAKING, true},
	    {ProcessState::OPPONENT_CONNECTING, true},
	};

	void disconnectFromServer();
	void terminateMmConnection();
	void sendMessage(json msg);
	int receiveMessage(json &msg, int maxAttempts);

	void sendHolePunchMsg(std::string remoteIp, u16 remotePort, u16 localPort);

	void startMatchmaking();
	void handleMatchmaking();
	void handleConnecting();
};
