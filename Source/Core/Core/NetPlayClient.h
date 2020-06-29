// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <SFML/Network/Packet.hpp>
#include <array>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/FifoQueue.h"
#include "Common/TraversalClient.h"
#include "Core/NetPlayProto.h"
#include "InputCommon/GCPadStatus.h"

#ifdef _WIN32
#include <Qos2.h>
#endif

class NetPlayUI
{
public:
	virtual ~NetPlayUI() {}
	virtual void BootGame(const std::string& filename) = 0;
	virtual void StopGame() = 0;

	virtual void Update() = 0;
	virtual void AppendChat(const std::string& msg, bool from_self) = 0;

	virtual void OnMsgChangeGame(const std::string& filename) = 0;
	virtual void OnMsgStartGame() = 0;
	virtual void OnMsgStopGame() = 0;
	virtual void OnMinimumPadBufferChanged(u32 buffer) = 0;
	virtual void OnPlayerPadBufferChanged(u32 buffer) = 0;
	virtual void OnDesync(u32 frame, const std::string& player) = 0;
	virtual void OnConnectionLost() = 0;
	virtual void OnTraversalError(int error) = 0;
	virtual bool IsRecording() = 0;
	virtual bool IsSpectating() = 0;
	virtual void SetSpectating(bool spectating) = 0;
	virtual std::string FindGame(const std::string& game) = 0;
	virtual void ShowMD5Dialog(const std::string& file_identifier) = 0;
	virtual void SetMD5Progress(int pid, int progress) = 0;
	virtual void SetMD5Result(int pid, const std::string& result) = 0;
	virtual void AbortMD5() = 0;
};

enum class PlayerGameStatus
{
	Unknown,
	Ok,
	NotFound
};

class Player
{
public:
	PlayerId pid;
	std::string name;
	std::string revision;
	u32 ping;
    float frame_time = 0;
	PlayerGameStatus game_status;
	u32 buffer = 0;
};


class NetPlayClient : public TraversalClientClient
{
public:
	void ThreadFunc();
	void SendAsync(std::unique_ptr<sf::Packet> packet);

	NetPlayClient(const std::string& address, const u16 port, NetPlayUI* dialog,
		const std::string& name, bool traversal, const std::string& centralServer,
		u16 centralPort);
	~NetPlayClient();

	void GetPlayerList(std::string& list, std::vector<int>& pid_list);
	std::vector<const Player*> GetPlayers();

	// Called from the GUI thread.
	bool IsConnected() const { return m_is_connected; }
	bool StartGame(const std::string& path);
	bool StopGame();
	void Stop();
	bool ChangeGame(const std::string& game);
	void SendChatMessage(const std::string& msg);

    void ReportFrameTimeToServer(float frame_time);

	// Send and receive pads values
	bool WiimoteUpdate(int _number, u8* data, const u8 size, u8 reporting_mode);
	bool GetNetPads(int pad_nb, GCPadStatus* pad_status);
	void SendNetPad(int pad_nb);
	void SendSpectatorSetting(bool spectator);

	void OnTraversalStateChanged() override;
	void OnConnectReady(ENetAddress addr) override;
	void OnConnectFailed(u8 reason) override;

	bool IsFirstInGamePad(int ingame_pad) const;
	int NumLocalPads() const;

	int InGamePadToLocalPad(int ingame_pad);
	int LocalPadToInGamePad(int localPad);

	static void SendTimeBase();
	bool DoAllPlayersHaveGame();

	void SetLocalPlayerBuffer(u32 buffer);

	// the number of ticks in-between frames
	constexpr static int buffer_accuracy = 4;

	inline u32 BufferSizeForPort(int pad) const
	{
		if(m_pad_map[pad] <= 0)
			return 0;

		return std::max(m_minimum_buffer_size, m_players.at(m_pad_map.at(pad)).buffer);
	}

    // used for chat, not the best place for it
    inline std::string FindPlayerPadName(const Player* player) const
    {
        for(int i = 0; i < 4; i++)
        {
            if(m_pad_map[i] == player->pid)
                return " (port " + std::to_string(i + 1) + ")";
        }

        return "";
    }

	// used for slippi, not the best place for it
	inline int FindPlayerPad(const Player* player) const
	{
		for (int i = 0; i < 4; i++)
		{
			if (m_pad_map[i] == player->pid)
				return i;
		}

		return -1;
	}

    NetPlayUI* dialog = nullptr;
    Player* local_player = nullptr;
protected:
	void ClearBuffers();

	struct
	{
		std::recursive_mutex game;
		// lock order
		std::recursive_mutex players;
		std::recursive_mutex async_queue_write;
	} m_crit;

	Common::FifoQueue<std::unique_ptr<sf::Packet>, false> m_async_queue;

	std::array<Common::FifoQueue<GCPadStatus>, 4> m_pad_buffer;
	std::array<Common::FifoQueue<NetWiimote>, 4> m_wiimote_buffer;

	ENetHost* m_client = nullptr;
	ENetPeer* m_server = nullptr;
	std::thread m_thread;

	std::string m_selected_game;
	Common::Flag m_is_running{ false };
	Common::Flag m_do_loop{ true };

	unsigned int m_minimum_buffer_size = 6;

	u32 m_current_game = 0;

	PadMappingArray m_pad_map;
	PadMappingArray m_wiimote_map;

	bool m_is_recording = false;
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

	bool LocalPlayerHasControllerMapped() const;

	void SendStartGamePacket();
	void SendStopGamePacket();

	void UpdateDevices();
	void SendPadState(int in_game_pad, const GCPadStatus& np);
	void SendWiimoteState(int in_game_pad, const NetWiimote& nw);
	unsigned int OnData(sf::Packet& packet);
	void Send(sf::Packet& packet);
	void Disconnect();
	bool Connect();
	void ComputeMD5(const std::string& file_identifier);
	void DisplayPlayersPing();
	u32 GetPlayersMaxPing() const;

	bool m_is_connected = false;
	ConnectionState m_connection_state = ConnectionState::Failure;

	PlayerId m_pid = 0;
	std::map<PlayerId, Player> m_players;
	std::string m_host_spec;

	std::string m_player_name;
	bool m_connecting = false;
	TraversalClient* m_traversal_client = nullptr;
	std::thread m_MD5_thread;
	bool m_should_compute_MD5 = false;
	Common::Event m_gc_pad_event;
	Common::Event m_wii_pad_event;

#ifdef _WIN32
	HANDLE m_qos_handle;
	QOS_FLOWID m_qos_flow_id;
#endif

	u32 m_timebase_frame = 0;
};

void NetPlay_Enable(NetPlayClient* const np);
void NetPlay_Disable();

extern NetPlayClient* netplay_client;
