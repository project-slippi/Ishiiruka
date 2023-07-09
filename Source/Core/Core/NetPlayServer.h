// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <SFML/Network/Packet.hpp>
#include <map>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include "Common/FifoQueue.h"
#include "Common/Timer.h"
#include "Common/TraversalClient.h"
#include "Core/NetPlayProto.h"

#ifdef _WIN32
#include <Qos2.h>
#endif

enum class PlayerGameStatus;

class NetPlayUI;
enum class PlayerGameStatus;

class NetPlayServer : public TraversalClientClient
{
public:
	void ThreadFunc();
	void SendAsyncToClients(std::unique_ptr<sf::Packet> packet);

	NetPlayServer(const u16 port, bool traversal, const std::string& centralServer, u16 centralPort);
	~NetPlayServer();

	bool ChangeGame(const std::string& game);
	bool ComputeMD5(const std::string& file_identifier);
	bool AbortMD5();
	void SendChatMessage(const std::string& msg);

	void SetNetSettings(const NetSettings& settings);

	bool StartGame();

	PadMappingArray GetPadMapping() const;
	void SetPadMapping(const PadMappingArray& mappings);

	PadMappingArray GetWiimoteMapping() const;
	void SetWiimoteMapping(const PadMappingArray& mappings);

	void AdjustMinimumPadBufferSize(unsigned int size);

	void KickPlayer(PlayerId player);

	u16 GetPort();

	void SetNetPlayUI(NetPlayUI* dialog);
	std::unordered_set<std::string> GetInterfaceSet();
	std::string GetInterfaceHost(const std::string& inter);

	bool is_connected = false;

private:
	class Client
	{
	public:
		PlayerId pid;
		std::string name;
		std::string revision;
		PlayerGameStatus game_status;

		ENetPeer* socket;
		u32 ping;
        float frame_time = 0;
		u32 current_game;

		unsigned int buffer = 0;

#ifdef _WIN32
		HANDLE qos_handle;
		QOS_FLOWID qos_flow_id;
#endif

		bool operator==(const Client& other) const { return this == &other; }
	};

	void SendToClients(sf::Packet& packet, const PlayerId skip_pid = 0);
	void Send(ENetPeer* socket, sf::Packet& packet);
	unsigned int OnConnect(ENetPeer* socket);
	unsigned int OnDisconnect(Client& player);
	unsigned int OnData(sf::Packet& packet, Client& player);

	void OnTraversalStateChanged() override;
	void OnConnectReady(ENetAddress) override {}
	void OnConnectFailed(u8) override {}
	void UpdatePadMapping();
	void UpdateWiimoteMapping();
	std::vector<std::pair<std::string, std::string>> GetInterfaceListInternal();

	NetSettings m_settings;

	bool m_is_running = false;
	bool m_do_loop = false;
	Common::Timer m_ping_timer;
	u32 m_ping_key = 0;
	bool m_update_pings = false;
	u32 m_current_game = 0;
	unsigned int m_minimum_buffer_size = 0;
	PadMappingArray m_pad_map;
	PadMappingArray m_wiimote_map;

	std::map<PlayerId, Client> m_players;

	std::unordered_map<u32, std::vector<std::pair<PlayerId, u64>>> m_timebase_by_frame;
	bool m_desync_detected;

	struct
	{
		std::recursive_mutex game;
		// lock order
		std::recursive_mutex players;
		std::recursive_mutex async_queue_write;
	} m_crit;

	std::string m_selected_game;
	std::thread m_thread;
	Common::FifoQueue<std::unique_ptr<sf::Packet>, false> m_async_queue;

	ENetHost* m_server = nullptr;
	TraversalClient* m_traversal_client = nullptr;
	NetPlayUI* m_dialog = nullptr;
};
