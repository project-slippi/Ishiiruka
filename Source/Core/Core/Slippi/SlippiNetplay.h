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
#include "InputCommon/GCAdapter.h"
#include "InputCommon/GCPadStatus.h"
#include <SFML/Network/Packet.hpp>
#include <array>
#include <deque>
#include <set>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#ifdef _WIN32
#include <Qos2.h>
#endif

#define SLIPPI_ONLINE_LOCKSTEP_INTERVAL 30 // Number of frames to wait before attempting to time-sync
#define SLIPPI_PING_DISPLAY_INTERVAL 60
#define SLIPPI_REMOTE_PLAYER_MAX 3
#define SLIPPI_REMOTE_PLAYER_COUNT 3

struct SlippiRemotePadOutput
{
	int32_t latestFrame;
	u8 playerIdx;
	std::vector<u8> data;
};

class SlippiPlayerSelections
{
  public:
	u8 playerIdx = 0;
	u8 characterId = 0;
	u8 characterColor = 0;
	u8 teamId = 0;

	bool isCharacterSelected = false;

	u16 stageId = 0;
	bool isStageSelected = false;

	u32 rngOffset = 0;

	int messageId;

	void Merge(SlippiPlayerSelections &s)
	{
		this->rngOffset = s.rngOffset;

		if (s.isStageSelected)
		{
			this->stageId = s.stageId;
			this->isStageSelected = true;
		}

		if (s.isCharacterSelected)
		{
			this->characterId = s.characterId;
			this->characterColor = s.characterColor;
			this->teamId = s.teamId;
			this->isCharacterSelected = true;
		}
	}

	void Reset()
	{
		characterId = 0;
		characterColor = 0;
		isCharacterSelected = false;
		teamId = 0;

		stageId = 0;
		isStageSelected = false;

		rngOffset = 0;
	}
};

class SlippiMatchInfo
{
  public:
	SlippiPlayerSelections localPlayerSelections;
	SlippiPlayerSelections remotePlayerSelections[SLIPPI_REMOTE_PLAYER_MAX];

	void Reset()
	{
		localPlayerSelections.Reset();
		for (int i = 0; i < SLIPPI_REMOTE_PLAYER_MAX; i++)
		{
			remotePlayerSelections[i].Reset();
		}
	}
};

class SlippiNetplayClient
{
  public:
	void ThreadFunc();
	void SendAsync(std::unique_ptr<sf::Packet> packet);

	SlippiNetplayClient(bool isDecider); // Make a dummy client
	SlippiNetplayClient(std::vector<std::string> addrs, std::vector<u16> ports, const u8 remotePlayerCount,
	                    const u16 localPort, bool isDecider, u8 playerIdx);
	~SlippiNetplayClient();

	// Slippi Online
	enum class SlippiConnectStatus
	{
		NET_CONNECT_STATUS_UNSET,
		NET_CONNECT_STATUS_INITIATED,
		NET_CONNECT_STATUS_CONNECTED,
		NET_CONNECT_STATUS_FAILED,
		NET_CONNECT_STATUS_DISCONNECTED,
	};

	bool IsDecider();
	bool IsConnectionSelected();
	u8 LocalPlayerPort();
	SlippiConnectStatus GetSlippiConnectStatus();
	std::vector<int> GetFailedConnections();
	void StartSlippiGame(u8 delay);
	void SendConnectionSelected();
	void SendSlippiPad(std::unique_ptr<SlippiPad> pad);
	void SetMatchSelections(SlippiPlayerSelections &s);

	// 1. Si aucun RemotePad enregistré, renvoie un avec frame 0 et contenu de pad nul
	// 2. Sinon renvoie une copie frame+pad du premier pad dans la queue
	// 3. Enlève les pads antérieurs à la frame actuelle sauf le dernier pad connu
	std::unique_ptr<SlippiRemotePadOutput> GetSlippiRemotePad(int32_t curFrame, int index);
	void DropOldRemoteInputs(int32_t curFrame);

	SlippiMatchInfo *GetMatchInfo();
	int32_t GetSlippiLatestRemoteFrame();
	SlippiPlayerSelections GetSlippiRemoteChatMessage();
	u8 GetSlippiRemoteSentChatMessage();
	s32 CalcTimeOffsetUs();

	void WriteChatMessageToPacket(sf::Packet &packet, int messageId, u8 playerIdx);
	std::unique_ptr<SlippiPlayerSelections> ReadChatMessageFromPacket(sf::Packet &packet);

	std::unique_ptr<SlippiPlayerSelections> remoteChatMessageSelection =
	    nullptr;                    // most recent chat message player selection (message + player index)
	u8 remoteSentChatMessageId = 0; // most recent chat message id that current player sent

	void DecrementInputStabilizerFrameCounts();

	void KristalInputCallback(const GCPadStatus &pad, std::chrono::high_resolution_clock::time_point tp, int chan);
	// TODO Check whether we can't just use the first InputStabilizer since we only want timing information
	// Will have to handle that for HID controller support

	// TODO Extract to own class
	struct KristalPad
	{
		float subframe;
		u8 version;
		u8 pad[SLIPPI_PAD_DATA_SIZE];

		// Latest pad in back of set i.e superior
		bool operator<(const KristalPad &rhs) const
		{
			if ((int)subframe != (int)rhs.subframe) // If not from the same frame
				return subframe < rhs.subframe;     // Check higher frame
			if (version != rhs.version)             // If from the same frame and not same version
				return version < rhs.version;       // Check higher version
			return subframe < rhs.subframe;         // If same frame, same version, check higher subframe
		}
	};

	std::pair<bool, KristalPad> GetKristalInput(u32 frame, u8 playerIdx);

	// Debug
	bool newXReady = false;
	 // /Debug

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
	std::vector<ENetPeer *> m_server;
	std::thread m_thread;
	u8 m_remotePlayerCount = 0;

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

	struct FrameOffsetData
	{
		// TODO: Should the buffer size be dynamic based on time sync interval or not?
		int idx;
		std::vector<s32> buf;
	};

	bool isConnectionSelected = false;
	bool isDecider = false;
	bool hasGameStarted = false;
	u8 playerIdx = 0;

	std::deque<std::unique_ptr<SlippiPad>> localPadQueue; // most recent inputs at start of deque
	std::deque<std::unique_ptr<SlippiPad>>
	    remotePadQueue[SLIPPI_REMOTE_PLAYER_MAX]; // most recent inputs at start of deque

	u64 pingUs[SLIPPI_REMOTE_PLAYER_MAX];
	int32_t lastFrameAcked[SLIPPI_REMOTE_PLAYER_MAX];
	FrameOffsetData frameOffsetData[SLIPPI_REMOTE_PLAYER_MAX];
	FrameTiming lastFrameTiming[SLIPPI_REMOTE_PLAYER_MAX];
	std::array<Common::FifoQueue<FrameTiming, false>, SLIPPI_REMOTE_PLAYER_MAX> ackTimers;

	SlippiConnectStatus slippiConnectStatus = SlippiConnectStatus::NET_CONNECT_STATUS_UNSET;
	std::vector<int> failedConnections;
	SlippiMatchInfo matchInfo;

	bool m_is_recording = false;

	bool shouldInitializeInputStabilizers = false;

	std::set<KristalPad> subframePadSets[SLIPPI_REMOTE_PLAYER_MAX];
	std::mutex subframePadSetLocks[SLIPPI_REMOTE_PLAYER_MAX];

	void writeToPacket(sf::Packet &packet, SlippiPlayerSelections &s);
	std::unique_ptr<SlippiPlayerSelections> readSelectionsFromPacket(sf::Packet &packet);

  private:
	u8 PlayerIdxFromPort(u8 port);
	unsigned int OnData(sf::Packet &packet, ENetPeer *peer);
	void Send(sf::Packet &packet);
	void Disconnect();

	bool m_is_connected = false;

#ifdef _WIN32
	HANDLE m_qos_handle;
	QOS_FLOWID m_qos_flow_id;
#endif

	u32 m_timebase_frame = 0;

	u8 m_last_adapter_chan_used_in_kristal_callback = 0;
};

extern SlippiNetplayClient *SLIPPI_NETPLAY; // singleton static pointer

static bool IsOnline()
{
	return SLIPPI_NETPLAY != nullptr;
}