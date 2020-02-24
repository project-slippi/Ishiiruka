#pragma once

#include "Common/CommonTypes.h"
#include "Common/Thread.h"

#include <vector>
#include <curl/curl.h>

class SlippiMatchmaking
{
public:
	SlippiMatchmaking();
	~SlippiMatchmaking();

  void FindMatch();
	void MatchmakeThread();

  enum ProcessState
	{
	  UNCONNECTED,
    MATCHMAKING,
    OPPONENT_FOUND,
    OPPONENT_CONNECTING,
    GAME_READY,
    ERROR_ENCOUNTERED,
  };

protected:
  const std::string URL_START = "http://35.197.121.196:43113/tickets";

  CURL *m_curl = nullptr;
  std::thread m_matchmakeThread;

  ProcessState m_state;

  std::string m_ticketId;
  
  std::vector<char> findReceiveBuf;
  std::vector<char> getReceiveBuf;

  void startMatchmaking();
  void handleMatchmaking();
};

