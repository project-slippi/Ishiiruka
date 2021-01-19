#include "SlippiGameReporter.h"

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"

#include "Common/Common.h"
#include "Core/ConfigManager.h"

#include <codecvt>
#include <locale>

#include <json.hpp>
using json = nlohmann::json;

static size_t receive(char *ptr, size_t size, size_t nmemb, void *rcvBuf)
{
	size_t len = size * nmemb;
	INFO_LOG(SLIPPI_ONLINE, "[User] Received data: %d", len);

	std::string *buf = (std::string *)rcvBuf;

	buf->insert(buf->end(), ptr, ptr + len);

	return len;
}

SlippiGameReporter::SlippiGameReporter(SlippiUser *user)
{
	CURL *curl = curl_easy_init();
	if (curl)
	{
		// curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &receive);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000);

		// Set up HTTP Headers
		m_curlHeaderList = curl_slist_append(m_curlHeaderList, "Content-Type: application/json");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, m_curlHeaderList);

#ifdef _WIN32
		// ALPN support is enabled by default but requires Windows >= 8.1.
		curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, false);
#endif

		m_curl = curl;
	}

	m_user = user;

	runThread = true;
	reportingThread = std::thread(&SlippiGameReporter::ReportThreadHandler, this);
}

SlippiGameReporter::~SlippiGameReporter()
{
	runThread = false;
	cv.notify_one();
	if (reportingThread.joinable())
		reportingThread.join();

	if (m_curl)
	{
		curl_slist_free_all(m_curlHeaderList);
		curl_easy_cleanup(m_curl);
	}
}

void SlippiGameReporter::StartReport(GameReport report)
{
	gameReportQueue.Push(report);
	cv.notify_one();
}

void SlippiGameReporter::StartNewSession(std::vector<std::string> playerUids)
{
	this->playerUids = playerUids;
	gameIndex = 1;
}

void SlippiGameReporter::ReportThreadHandler()
{
	std::unique_lock<std::mutex> lck(mtx);

	while (runThread)
	{
		// Wait for report to come in
		cv.wait(lck);

		// Process all messages
		while (!gameReportQueue.Empty())
		{
			auto report = gameReportQueue.Front();
			gameReportQueue.Pop();

			auto userInfo = m_user->GetUserInfo();

			WARN_LOG(SLIPPI_ONLINE, "Checking game report for game %d. Length: %d...", gameIndex,
			         report.durationFrames);

			// Prepare report
			json request;
			request["uid"] = userInfo.uid;
			request["playKey"] = userInfo.playKey;
			request["gameIndex"] = gameIndex;
			request["gameDurationFrames"] = report.durationFrames;

			json players = json::array();
			for (int i = 0; i < report.players.size(); i++)
			{
				json p;
				p["uid"] = playerUids[i];
				p["damageDone"] = report.players[i].damageDone;
				p["stocksRemaining"] = report.players[i].stocksRemaining;

				players[i] = p;
			}

			request["players"] = players;

			auto requestString = request.dump();

			// Send report
			curl_easy_setopt(m_curl, CURLOPT_POST, true);
			curl_easy_setopt(m_curl, CURLOPT_URL, REPORT_URL.c_str());
			curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, requestString.c_str());
			curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, requestString.length());
			CURLcode res = curl_easy_perform(m_curl);

			if (res != 0)
			{
				ERROR_LOG(SLIPPI_ONLINE, "[GameReport] Got error executing request. Err code: %d", res);
			}

			gameIndex++;
			Common::SleepCurrentThread(0);
		}
	}
}