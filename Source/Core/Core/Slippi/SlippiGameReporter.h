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
#include <map>

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
		int reportAttempts = 0;
		u32 durationFrames = 0;
		u32 gameIndex = 0;
		u32 tiebreakIndex = 0;
		s8 winnerIdx = 0;
		u8 gameEndMethod = 0;
		s8 lrasInitiator = 0;
		int stageId = 0;
		std::vector<PlayerReport> players;
	};

	SlippiGameReporter(SlippiUser *user);
	~SlippiGameReporter();

	void StartReport(GameReport report);
	void ReportAbandonment(std::string matchId);
	void ReportCompletion(std::string matchId, u8 endMode);
	void StartNewSession();
	void ReportThreadHandler();
	void PushReplayData(u8 *data, u32 length, std::string action);
	void UploadReplay(int idx, std::string url);

  protected:
	const std::string REPORT_URL = "https://rankings-dot-slippi.uc.r.appspot.com/report";
	const std::string ABANDON_URL = "https://rankings-dot-slippi.uc.r.appspot.com/abandon";
	const std::string COMPLETE_URL = "https://rankings-dot-slippi.uc.r.appspot.com/complete";
	CURL *m_curl = nullptr;
	struct curl_slist *m_curlHeaderList = nullptr;

	CURL *m_curl_upload = nullptr;
	struct curl_slist *m_curl_upload_headers = nullptr;

	char m_curl_err_buf[CURL_ERROR_SIZE];
	char m_curl_upload_err_buf[CURL_ERROR_SIZE];

	std::vector<std::string> playerUids;

	std::unordered_map<std::string, bool> knownDesyncIsos = {
	    {"23d6baef06bd65989585096915da20f2", true},
	    {"27a5668769a54cd3515af47b8d9982f3", true},
	    {"5805fa9f1407aedc8804d0472346fc5f", true},
	    {"9bb3e275e77bb1a160276f2330f93931", true},
	};

	SlippiUser *m_user;
	std::string m_iso_hash;
	Common::FifoQueue<GameReport, false> gameReportQueue;
	std::thread reportingThread;
	std::mutex mtx;
	std::condition_variable cv;
	std::atomic<bool> runThread;
	std::thread m_md5_thread;

	std::map<int, std::vector<u8>> m_replay_data;
	int m_replay_write_idx = 0;
	int m_replay_last_completed_idx = -1;
};
