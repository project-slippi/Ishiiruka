#include "SlippiMatchmaking.h"
#include "Common/Logging/Log.h"

size_t test(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Received data: %d", size);
	return size * nmemb;
}

SlippiMatchmaking::SlippiMatchmaking()
{
	CURL *curl = curl_easy_init();
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, "34.82.238.20:8080");
		curl_easy_setopt(curl, CURLOPT_POST, true);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &test);
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

size_t SlippiMatchmaking::receive(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Received data: %d", size);
	return size * nmemb;
}

void SlippiMatchmaking::FindMatch()
{
	if (!m_curl)
		return;

	//curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, report.c_str());
	//curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, report.size());
	curl_easy_perform(m_curl);
}
