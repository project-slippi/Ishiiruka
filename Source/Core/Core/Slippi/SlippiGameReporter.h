#pragma once

#include "Common/CommonTypes.h"
#include "Common/FifoQueue.h"
#include "Core/Slippi/SlippiUser.h"
#include <atomic>
#include <condition_variable> // std::condition_variable
#include <curl/curl.h>
#include <mutex> // std::mutex, std::unique_lock
#include <string>
#include <thread>
#include <vector>

#define SLIPPI_REPORT_URL "https://rankings-dot-slippi.uc.r.appspot.com/report"
#define EX_REPORT_URL "https://lylat.gg/reports"

class SlippiGameReporter
{
  public:
	struct PlayerReport
	{
		float damageDone;
		u8 stocksRemaining;
	};
	struct GameReport
	{
		u32 durationFrames = 0;
		std::vector<PlayerReport> players;
	};

	SlippiGameReporter(SlippiUser *user, bool useSlippiUrl);
	~SlippiGameReporter();

	void StartReport(GameReport report);
	void StartNewSession(std::vector<std::string> playerUids);
	void ReportThreadHandler();

  protected:
	std::string reportUrl = SLIPPI_REPORT_URL;
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
