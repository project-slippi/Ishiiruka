#pragma once

#include "Common/CommonTypes.h"
#include "Common/FifoQueue.h"
#include "Core/Slippi/SlippiUser.h"
#include "Core/Slippi/SlippiMatchmaking.h"
#include <atomic>
#include <condition_variable> // std::condition_variable
#include <curl/curl.h>
#include <mutex> // std::mutex, std::unique_lock
#include <string>
#include <thread>
#include <vector>

class SlippiGameReporter
{
  public:
	struct PlayerReport
	{
		std::string uid;
		u8 slotType;
		float damageDone;
		u8 stocksRemaining;
		u8 charId;
		u8 colorId;
		int startingStocks;
		int startingPercent;
	};
	struct GameReport
	{
		SlippiMatchmaking::OnlinePlayMode onlineMode;
		std::string matchId;
		u32 durationFrames = 0;
		u32 gameIndex = 0;
		u32 tiebreakIndex = 0;
		s8 winnerIdx = 0;
		u8 gameEndMethod = 0;
		s8 lrasInitiator = 0;
		int stageId;
		std::vector<PlayerReport> players;
	};

	SlippiGameReporter(SlippiUser *user);
	~SlippiGameReporter();

	void StartReport(GameReport report);
	void ReportAbandonment(std::string matchId);
	void StartNewSession();
	void ReportThreadHandler();

  protected:
	const std::string REPORT_URL = "https://rankings-dot-slippi.uc.r.appspot.com/report";
	const std::string ABANDON_URL = "https://rankings-dot-slippi.uc.r.appspot.com/abandon";
	CURL *m_curl = nullptr;
	struct curl_slist *m_curlHeaderList = nullptr;

	u32 gameIndex = 1;
	std::vector<std::string> playerUids;

	SlippiUser *m_user;
	Common::FifoQueue<GameReport, false> gameReportQueue;
	std::thread reportingThread;
	std::mutex mtx;
	std::condition_variable cv;
	std::atomic<bool> runThread;
};
