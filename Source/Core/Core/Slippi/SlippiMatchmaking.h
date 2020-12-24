#pragma once

#include "Common/CommonTypes.h"
#include "Common/Thread.h"
#include "Core/Slippi/SlippiNetplay.h"
#include "Core/Slippi/SlippiUser.h"

#include <curl/curl.h>
#include <enet/enet.h>
#include <unordered_map>
#include <vector>

#include <json.hpp>
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
		std::string connectCode = "";
	};

	void FindMatch(MatchSearchSettings settings);
	void MatchmakeThread();
	ProcessState GetMatchmakeState();
	bool IsSearching();
	std::unique_ptr<SlippiNetplayClient> GetNetplayClient();
	std::string GetErrorMessage();
	SlippiUser::UserInfo GetOpponent();

  protected:
	std::string MM_HOST = "";

	ENetHost *m_client;
	ENetPeer *m_server;

	std::default_random_engine generator;

	bool isMmConnected = false;

	std::thread m_matchmakeThread;

	MatchSearchSettings m_searchSettings;

	ProcessState m_state;
	std::string m_errorMsg = "";
  std::string m_api_url = "http://localhost/";

	SlippiUser *m_user;

	int m_isSwapAttempt = false;

	int m_hostPort;
	std::string m_oppIp;
  std::string m_ticket;
	bool m_isHost;
	SlippiUser::UserInfo m_oppUser;

	std::unique_ptr<SlippiNetplayClient> m_netplayClient;

	const std::unordered_map<ProcessState, bool> searchingStates = {
	    {ProcessState::INITIALIZING, true},
	    {ProcessState::MATCHMAKING, true},
	    {ProcessState::OPPONENT_CONNECTING, true},
	};

  // Used by libcurl
  struct CurlString {
    const char *readptr;
    size_t sizeleft;
  };

  static size_t WriteCallback(void *ptr, size_t size, size_t nmemb, std::string *data);
	std::string sendMessage(json msg);

	void sendHolePunchMsg(std::string remoteIp, u16 remotePort, u16 localPort);

	void startMatchmaking();
	void getTicketStatus();
	void handleConnecting();
};
