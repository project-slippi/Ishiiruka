#include "SlippiMatchmaking.h"
#include "Common/Logging/Log.h"

size_t receive(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	int len = size * nmemb;
	ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Received data: %d", len);

	std::vector<char> *buf = (std::vector<char> *)userdata;

	buf->insert(buf->end(), ptr, ptr + len);

	return len;
}

SlippiMatchmaking::SlippiMatchmaking()
{
	CURL *curl = curl_easy_init();
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &receive);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000);

#ifdef _WIN32
		// ALPN support is enabled by default but requires Windows >= 8.1.
		curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, false);
#endif

		m_curl = curl;
	}
}

SlippiMatchmaking::~SlippiMatchmaking()
{
	if (m_curl)
	{
		curl_easy_cleanup(m_curl);
	}
}

void SlippiMatchmaking::FindMatch()
{
	if (!m_curl)
		return;

	m_state = ProcessState::UNCONNECTED;
	m_matchmakeThread = std::thread(&SlippiMatchmaking::MatchmakeThread, this);
}

void SlippiMatchmaking::MatchmakeThread()
{
	while (m_state != ProcessState::GAME_READY && m_state != ProcessState::ERROR_ENCOUNTERED)
	{
		switch (m_state)
		{
		case ProcessState::UNCONNECTED:
			startMatchmaking();
			break;
		case ProcessState::MATCHMAKING:
			Common::SleepCurrentThread(2000);
			handleMatchmaking();
			break;
		case ProcessState::OPPONENT_FOUND:
			break;
		case ProcessState::OPPONENT_CONNECTING:
			break;
		}
	}
}

void SlippiMatchmaking::startMatchmaking()
{
	ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Trying to find match...");

	// Reset variables for
	m_ticketId.clear();
	findReceiveBuf.clear();

  curl_easy_setopt(m_curl, CURLOPT_URL, URL_START);
	curl_easy_setopt(m_curl, CURLOPT_POST, true);
	curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &findReceiveBuf);
	CURLcode res = curl_easy_perform(m_curl);

	if (res != 0)
	{
		ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Error trying to request matchmaking. Err code: %d", res);
		m_state = ProcessState::ERROR_ENCOUNTERED;
	}
}

void SlippiMatchmaking::handleMatchmaking()
{
	if (findReceiveBuf.size() < 20)
		return;

	if (m_ticketId.empty())
		m_ticketId.insert(m_ticketId.end(), findReceiveBuf.begin(), findReceiveBuf.end());


}
