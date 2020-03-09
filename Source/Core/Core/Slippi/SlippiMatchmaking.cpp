#include "SlippiMatchmaking.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

size_t receive(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	size_t len = size * nmemb;
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

	m_state = ProcessState::IDLE;
}

SlippiMatchmaking::~SlippiMatchmaking()
{
	if (m_curl)
	{
		curl_easy_cleanup(m_curl);
	}

	m_state = ProcessState::ERROR_ENCOUNTERED;

	if (m_matchmakeThread.joinable())
		m_matchmakeThread.detach();
}

void SlippiMatchmaking::FindMatch()
{
	if (!m_curl)
		return;

	m_state = ProcessState::INITIALIZING;
	m_matchmakeThread = std::thread(&SlippiMatchmaking::MatchmakeThread, this);
}

SlippiMatchmaking::ProcessState SlippiMatchmaking::GetMatchmakeState()
{
	return m_state;
}

bool SlippiMatchmaking::IsSearching()
{
	return searchingStates.count(m_state) != 0;
}

std::unique_ptr<SlippiNetplayClient> SlippiMatchmaking::GetNetplayClient()
{
	return std::move(m_netplayClient);
}

void SlippiMatchmaking::MatchmakeThread()
{
	while (IsSearching())
	{
		switch (m_state)
		{
		case ProcessState::INITIALIZING:
			startMatchmaking();
			break;
		case ProcessState::MATCHMAKING:
			handleMatchmaking();
			break;
		case ProcessState::OPPONENT_CONNECTING:
			handleConnecting();
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

	curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, NULL);
	curl_easy_setopt(m_curl, CURLOPT_POST, true);
	curl_easy_setopt(m_curl, CURLOPT_URL, URL_START.c_str());
	curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &findReceiveBuf);
	CURLcode res = curl_easy_perform(m_curl);

	if (res != 0)
	{
		ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Error trying to request matchmaking. Err code: %d", res);
		m_state = ProcessState::ERROR_ENCOUNTERED;
		return;
	}

	if (findReceiveBuf.size() < 20)
	{
		ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Ticket length less than 20?");
		m_state = ProcessState::ERROR_ENCOUNTERED;
		return;
	}

	m_ticketId.insert(m_ticketId.end(), findReceiveBuf.begin(), findReceiveBuf.end());

	m_state = ProcessState::MATCHMAKING;
	ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Request ticket success: %s", m_ticketId.c_str());
}

void SlippiMatchmaking::handleMatchmaking()
{
	Common::SleepCurrentThread(1000);
	if (m_state != ProcessState::MATCHMAKING)
	{
		// If the destructor was called during the sleep, state might have changed
		return;
	}

	getReceiveBuf.clear();

	curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, NULL);
	curl_easy_setopt(m_curl, CURLOPT_POST, false);
	curl_easy_setopt(m_curl, CURLOPT_URL, (URL_START + "/" + m_ticketId).c_str());
	curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &getReceiveBuf);
	CURLcode res = curl_easy_perform(m_curl);
	if (res != 0)
	{
		ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Error trying to get ticket info. Err code: %d", res);
		m_state = ProcessState::ERROR_ENCOUNTERED;
		return;
	}

	std::string resp;
	resp.insert(resp.end(), getReceiveBuf.begin(), getReceiveBuf.end());

	// TODO: Handle 404

	if (resp == "N/A")
	{
		// Here our ticket doesn't yet have an assignment... Try again
		ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] No assignment found yet");
		return;
	}

	deleteReceiveBuf.clear();

	// Delete ticket now that we've been assigned
	curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, "DELETE");
	curl_easy_setopt(m_curl, CURLOPT_POST, false);
	curl_easy_setopt(m_curl, CURLOPT_URL, (URL_START + "/" + m_ticketId).c_str());
	curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &deleteReceiveBuf);
	res = curl_easy_perform(m_curl);
	if (res != 0)
	{
		ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Failed to delete ticket. Err code: %d", res);
	}

	std::vector<std::string> splitResp;
	SplitString(resp, '\t', splitResp);

	m_netplayClient = nullptr;
	m_oppIp = splitResp[0];
	m_isHost = splitResp[1] == "true";

	m_state = ProcessState::OPPONENT_CONNECTING;
	ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Opponent found. IP: %s, isHost: %t", m_oppIp.c_str(), m_isHost);
}

void SlippiMatchmaking::handleConnecting()
{
	std::vector<std::string> ipParts;
	SplitString(m_oppIp, ':', ipParts);

	if (!m_netplayClient)
	{
		m_netplayClient = std::make_unique<SlippiNetplayClient>(ipParts[0], std::stoi(ipParts[1]), m_isHost);
	}

	auto status = m_netplayClient->GetSlippiConnectStatus();
	if (status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_INITIATED)
	{
		ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Connection not yet successful");
		Common::SleepCurrentThread(1000);
		return;
	}
	else if (status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_FAILED)
	{
		// Return to the start to get a new ticket to find someone else we can hopefully connect with
		m_netplayClient = nullptr;
		m_state = ProcessState::INITIALIZING;
		return;
	}

	// Connection success, our work is done
	m_state = ProcessState::CONNECTION_SUCCESS;
}
