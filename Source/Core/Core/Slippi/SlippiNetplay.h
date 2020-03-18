// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/FifoQueue.h"
#include "Common/Timer.h"
#include "Common/TraversalClient.h"
#include "Core/NetPlayProto.h"
#include "Core/Slippi/SlippiPad.h"
#include "InputCommon/GCPadStatus.h"
#include <SFML/Network/Packet.hpp>
#include <array>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <Qos2.h>
#endif

#define SLIPPI_ONLINE_LOCKSTEP_INTERVAL 30 // Number of frames to wait before attempting to time-sync
#define SLIPPI_PING_DISPLAY_INTERVAL 60

struct SlippiRemotePadOutput
{
	int32_t latestFrame;
	std::vector<u8> data;
};

class SlippiPlayerSelections
{
  public:
	u8 characterId;
	u8 characterColor;
	bool isCharacterSelected;

	u16 stageId;
	bool isStageSelected;

	void Merge(SlippiPlayerSelections &s)
	{
		if (s.isCharacterSelected)
		{
			this->characterId = s.characterId;
			this->characterColor = s.characterColor;
			this->isCharacterSelected = true;
		}

		if (s.isStageSelected)
		{
			this->stageId = s.stageId;
			this->isStageSelected = true;
		}
	}

	void Reset()
	{
		characterId = 0;
		characterColor = 0;
		isCharacterSelected = false;

		stageId = 0;
		isStageSelected = false;
	}
};

class SlippiMatchInfo
{
  public:
	SlippiPlayerSelections localPlayerSelections;
	SlippiPlayerSelections remotePlayerSelections;

	void Reset()
	{
		localPlayerSelections.Reset();
		remotePlayerSelections.Reset();
	}
};

class SlippiNetplayClient
{
  public:
	void ThreadFunc();
	void SendAsync(std::unique_ptr<sf::Packet> packet);

	SlippiNetplayClient(); // Make a dummy client
	SlippiNetplayClient(const std::string &address, const u16 port, bool isHost);
	~SlippiNetplayClient();

	// Slippi Online
	enum class SlippiConnectStatus
	{
		NET_CONNECT_STATUS_UNSET,
		NET_CONNECT_STATUS_INITIATED,
		NET_CONNECT_STATUS_CONNECTED,
		NET_CONNECT_STATUS_FAILED
	};

	bool IsHost();
	bool IsSlippiConnection();
	SlippiConnectStatus GetSlippiConnectStatus();
	void StartSlippiGame();
	void SendSlippiPad(std::unique_ptr<SlippiPad> pad);
	void SetMatchSelections(SlippiPlayerSelections &s);
	std::unique_ptr<SlippiRemotePadOutput> GetSlippiRemotePad(int32_t curFrame);
	SlippiMatchInfo *GetMatchInfo();
	u64 GetSlippiPing();
	int32_t GetSlippiLatestRemoteFrame();
	s32 CalcTimeOffsetUs();

#ifdef USE_UPNP
	void TryPortmapping(u16 port);
#endif

  protected:
	struct
	{
		std::recursive_mutex game;
		// lock order
		std::recursive_mutex players;
		std::recursive_mutex async_queue_write;
	} m_crit;

	Common::FifoQueue<std::unique_ptr<sf::Packet>, false> m_async_queue;

	ENetHost *m_client = nullptr;
	ENetPeer *m_server = nullptr;
	std::thread m_thread;

	std::string m_selected_game;
	Common::Flag m_is_running{false};
	Common::Flag m_do_loop{true};

	unsigned int m_minimum_buffer_size = 6;

	u32 m_current_game = 0;

	// Slippi Stuff
	struct FrameTiming
	{
		int32_t frame;
		u64 timeUs;
	};

	struct
	{
		// TODO: Should the buffer size be dynamic based on time sync interval or not?
		int idx;
		std::vector<s32> buf;
	} frameOffsetData;

	bool isSlippiConnection = false;
	bool isHost = false;
	int32_t lastFrameAcked;
	std::shared_ptr<FrameTiming> lastFrameTiming;
	u64 pingUs;
	std::deque<std::unique_ptr<SlippiPad>> localPadQueue;  // most recent inputs at start of deque
	std::deque<std::unique_ptr<SlippiPad>> remotePadQueue; // most recent inputs at start of deque
	std::map<int32_t, u64> ackTimers;
	SlippiConnectStatus slippiConnectStatus = SlippiConnectStatus::NET_CONNECT_STATUS_UNSET;
	SlippiMatchInfo matchInfo;

	bool m_is_recording = false;

	void writeToPacket(sf::Packet &packet, SlippiPlayerSelections &s);
	std::unique_ptr<SlippiPlayerSelections> readSelectionsFromPacket(sf::Packet &packet);

  private:
	enum class ConnectionState
	{
		WaitingForTraversalClientConnection,
		WaitingForTraversalClientConnectReady,
		Connecting,
		WaitingForHelloResponse,
		Connected,
		Failure
	};

	unsigned int OnData(sf::Packet &packet);
	void Send(sf::Packet &packet);
	void Disconnect();

	bool m_is_connected = false;
	ConnectionState m_connection_state = ConnectionState::Failure;

#ifdef _WIN32
	HANDLE m_qos_handle;
	QOS_FLOWID m_qos_flow_id;
#endif

	u32 m_timebase_frame = 0;

#ifdef USE_UPNP
	static void mapPortThread(const u16 port);
	static void unmapPortThread();

	static bool initUPnP();
	static bool UPnPMapPort(const std::string &addr, const u16 port);
	static bool UPnPUnmapPort(const u16 port);

	static struct UPNPUrls m_upnp_urls;
	static struct IGDdatas m_upnp_data;
	static std::string m_upnp_ourip;
	static u16 m_upnp_mapped;
	static bool m_upnp_inited;
	static bool m_upnp_error;
	static std::thread m_upnp_thread;
#endif
};
