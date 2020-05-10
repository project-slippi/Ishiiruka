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

	enum ProcessState
	{
		IDLE,
		INITIALIZING,
		MATCHMAKING,
		OPPONENT_CONNECTING,
		CONNECTION_SUCCESS,
		ERROR_ENCOUNTERED,
	};

	void FindMatch();
	void MatchmakeThread();
	ProcessState GetMatchmakeState();
	bool IsSearching();
	std::unique_ptr<SlippiNetplayClient> GetNetplayClient();

  protected:
	const std::string MM_HOST = "35.197.121.196";
	const u16 MM_PORT = 43113;

	ENetHost *m_client;
	ENetPeer *m_server;

	bool isMmConnected = false;

	CURL *m_curl = nullptr;
	struct curl_slist *m_curlHeaderList = nullptr;

	std::thread m_matchmakeThread;

	ProcessState m_state;

	SlippiUser *m_user;

	int m_hostPort;
	std::string m_ticketId;
	std::string m_oppIp;
	bool m_isDecider;

	std::unique_ptr<SlippiNetplayClient> m_netplayClient;

	std::vector<char> findReceiveBuf;
	std::vector<char> getReceiveBuf;
	std::vector<char> deleteReceiveBuf;

	const std::unordered_map<ProcessState, bool> searchingStates = {
	    {ProcessState::INITIALIZING, true},
	    {ProcessState::MATCHMAKING, true},
	    {ProcessState::OPPONENT_CONNECTING, true},
	};

	void disconnectFromServer();
	void terminateMmConnection();
	void sendMessage(json msg);
	int receiveMessage(json &msg, int maxAttempts);

	int deciderChoose(SlippiNetplayClient *n1, SlippiNetplayClient *n2);
	int getDecided(SlippiNetplayClient *n1, SlippiNetplayClient *n2);

	void startMatchmaking();
	void handleMatchmaking();
	void handleConnecting();
};
