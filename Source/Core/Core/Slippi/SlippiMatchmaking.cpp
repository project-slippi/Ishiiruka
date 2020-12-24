#include "SlippiMatchmaking.h"
#include "Common/Common.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include <string>
#include <vector>

SlippiMatchmaking::SlippiMatchmaking(SlippiUser *user)
{
	m_user = user;
	m_state = ProcessState::IDLE;
	m_errorMsg = "";

	m_client = nullptr;
	m_server = nullptr;

	generator = std::default_random_engine(Common::Timer::GetTimeMs());
}

SlippiMatchmaking::~SlippiMatchmaking()
{
	m_state = ProcessState::ERROR_ENCOUNTERED;
	m_errorMsg = "Matchmaking shut down";

	if (m_matchmakeThread.joinable())
		m_matchmakeThread.join();
}

void SlippiMatchmaking::FindMatch(MatchSearchSettings settings)
{
	isMmConnected = false;

	ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Starting matchmaking...");

	m_searchSettings = settings;

	m_errorMsg = "";
	m_state = ProcessState::INITIALIZING;
	m_matchmakeThread = std::thread(&SlippiMatchmaking::MatchmakeThread, this);
}

SlippiMatchmaking::ProcessState SlippiMatchmaking::GetMatchmakeState()
{
	return m_state;
}

std::string SlippiMatchmaking::GetErrorMessage()
{
	return m_errorMsg;
}

SlippiUser::UserInfo SlippiMatchmaking::GetOpponent()
{
	return m_oppUser;
}

bool SlippiMatchmaking::IsSearching()
{
	return searchingStates.count(m_state) != 0;
}

std::unique_ptr<SlippiNetplayClient> SlippiMatchmaking::GetNetplayClient()
{
	return std::move(m_netplayClient);
}

size_t SlippiMatchmaking::WriteCallback(void *ptr, size_t size, size_t nmemb, std::string *data)
{
	data->append((char *)ptr, size * nmemb);
	return size * nmemb;
}

std::string SlippiMatchmaking::sendMessage(json msg)
{
	std::string msgContents = msg.dump();
  struct CurlString requestBody;
  requestBody.readptr = msgContents.data();
  requestBody.sizeleft = msgContents.length();

  auto curl = curl_easy_init();
  if(curl == nullptr){
		ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Could not initilize libcurl");
		m_state = ProcessState::ERROR_ENCOUNTERED;
		m_errorMsg = "Connection error";
		return "";
  }
	std::string result_string;

	// Set the url
	curl_easy_setopt(curl, CURLOPT_URL, m_api_url.data());
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2);

	// Set the headers
	struct curl_slist *headers = NULL;
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);

  // Send data in POST
  curl_easy_setopt(curl, CURLOPT_READDATA, &requestBody);

	// Get result
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result_string);

	// Perform the request
	int returnCode = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	if(returnCode != 200)
	{
		ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Could not connect to MM server");
		m_state = ProcessState::ERROR_ENCOUNTERED;
		m_errorMsg = "Could not connect to MM server";
		return "{}";
	}

  return result_string;
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
			getTicketStatus();
			break;
		case ProcessState::OPPONENT_CONNECTING:
			handleConnecting();
			break;
		}
	}
}

// This will create a matchmaking ticket on the MM server.
//  The resulting ticket is stored into m_ticket and needs to be checked on later
void SlippiMatchmaking::startMatchmaking()
{
	auto userInfo = m_user->GetUserInfo();

	std::vector<u8> connectCodeBuf;
	connectCodeBuf.insert(connectCodeBuf.end(), m_searchSettings.connectCode.begin(),
	                      m_searchSettings.connectCode.end());

	// Send message to server to create ticket
	json request;
	request["type"] = "create-ticket";
	request["user"] = {{"uid", userInfo.uid}, {"playKey", userInfo.playKey}};
	request["search"] = {{"mode", m_searchSettings.mode}, {"connectCode", connectCodeBuf}};
	request["appVersion"] = scm_slippi_semver_str;
  json response = json::parse(sendMessage(request));

	if(m_state == ProcessState::ERROR_ENCOUNTERED)
	{
		return;
	}

	std::string err = response.value("error", "");
	if (err.length() > 0)
	{
		ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Received error from server for create ticket");
		m_state = ProcessState::ERROR_ENCOUNTERED;
		m_errorMsg = err;
		return;
	}
  m_ticket = response["ticket"];

	m_state = ProcessState::MATCHMAKING;
	ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Request ticket success");
}

void SlippiMatchmaking::getTicketStatus()
{
	// Deal with class shut down
	if (m_state != ProcessState::MATCHMAKING)
		return;

  auto userInfo = m_user->GetUserInfo();

  // Send message to server to create ticket
	json request;
	request["type"] = "ticket-status";
	request["user"] = {{"uid", userInfo.uid}, {"playKey", userInfo.playKey}};
	request["ticket"] = {m_ticket};
	request["appVersion"] = scm_slippi_semver_str;
  json response = json::parse(sendMessage(request));

  std::string err = response.value("error", "");
	std::string latestVersion = response.value("latestVersion", "");
	if (err.length() > 0)
	{
		if (latestVersion != "")
		{
			// Update version number when the mm server tells us our version is outdated
			m_user->OverwriteLatestVersion(latestVersion); // Force latest version for people whose file updates dont work
		}

		ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Received error from server for get ticket");
		m_state = ProcessState::ERROR_ENCOUNTERED;
		m_errorMsg = err;
		return;
	}

	m_netplayClient = nullptr;
	m_oppIp = response.value("oppAddress", "");
	m_isHost = response.value("isHost", false);

	// Clear old user
	SlippiUser::UserInfo emptyInfo;
	m_oppUser = emptyInfo;

	auto oppUser = response["oppUser"];
	if (oppUser.is_object())
	{
		m_oppUser.uid = oppUser.value("uid", "");
		m_oppUser.displayName = oppUser.value("displayName", "");
		m_oppUser.connectCode = oppUser.value("connectCode", "");
    m_state = ProcessState::OPPONENT_CONNECTING;
    ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Opponent found. isDecider: %s", m_isHost ? "true" : "false");
	}
  else
  {
    INFO_LOG(SLIPPI_ONLINE, "[Matchmaking] Connection not yet successful");
    m_state = ProcessState::MATCHMAKING;
    Common::SleepCurrentThread(1000);
  }
}

void SlippiMatchmaking::handleConnecting()
{
	std::vector<std::string> ipParts;
	SplitString(m_oppIp, ':', ipParts);

	// Is host is now used to specify who the decider is
	auto client = std::make_unique<SlippiNetplayClient>(ipParts[0], std::stoi(ipParts[1]), m_hostPort, m_isHost);

	while (!m_netplayClient)
	{
		auto status = client->GetSlippiConnectStatus();
		if (status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_INITIATED)
		{
			INFO_LOG(SLIPPI_ONLINE, "[Matchmaking] Connection not yet successful");
			Common::SleepCurrentThread(500);

			// Deal with class shut down
			if (m_state != ProcessState::OPPONENT_CONNECTING)
				return;

			continue;
		}
		else if (status != SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_CONNECTED)
		{
			ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Connection attempt failed, looking for someone else.");

			// Return to the start to get a new ticket to find someone else we can hopefully connect with
			m_netplayClient = nullptr;
			m_state = ProcessState::INITIALIZING;
			return;
		}

		ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Connection success!");

		// Successful connection
		m_netplayClient = std::move(client);
	}

	// Connection success, our work is done
	m_state = ProcessState::CONNECTION_SUCCESS;
}
