// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/NetPlayClient.h"
#include <algorithm>
#include <fstream>
#include <mbedtls/md5.h>
#include <memory>
#include <thread>
#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/ENetUtil.h"
#include "Common/MD5.h"
#include "Common/MsgHandler.h"
#include "Common/Timer.h"
#include "Core/Core.h"
#include "Core/ConfigManager.h"
#include "Core/HW/EXI_DeviceIPL.h"
#include "Core/HW/SI.h"
#include "Core/HW/SI_DeviceGCController.h"
#include "Core/HW/Sram.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_emu.h"
#include "Core/Movie.h"
#include "InputCommon/GCAdapter.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoConfig.h"

static std::mutex crit_netplay_client;
NetPlayClient* netplay_client = nullptr;
NetSettings g_NetPlaySettings;

// called from ---GUI--- thread
NetPlayClient::~NetPlayClient()
{
	// not perfect
	if (m_is_running.IsSet())
		StopGame();

	if (m_is_connected)
	{
		m_do_loop.Clear();
		m_thread.join();
	}

	if (m_server)
	{
		Disconnect();
	}

	if (g_MainNetHost.get() == m_client)
	{
		g_MainNetHost.release();
	}
	if (m_client)
	{
		enet_host_destroy(m_client);
		m_client = nullptr;
	}

	if (m_traversal_client)
	{
		ReleaseTraversalClient();
	}
}

// called from ---GUI--- thread
NetPlayClient::NetPlayClient(const std::string& address, const u16 port, NetPlayUI* dialog,
	const std::string& name, bool traversal,
	const std::string& centralServer, u16 centralPort)
	: dialog(dialog), m_player_name(name)
#ifdef _WIN32
	, m_qos_handle(nullptr), m_qos_flow_id(0)
#endif
{
	ClearBuffers();

	if (!traversal)
	{
		// Direct Connection
		m_client = enet_host_create(nullptr, 1, 3, 0, 0);

		if (m_client == nullptr)
		{
			PanicAlertT("Couldn't Create Client");
		}

		ENetAddress addr;
		enet_address_set_host(&addr, address.c_str());
		addr.port = port;

		m_server = enet_host_connect(m_client, &addr, 3, 0);

		if (m_server == nullptr)
		{
			PanicAlertT("Couldn't create peer.");
		}

		ENetEvent netEvent;
		int net = enet_host_service(m_client, &netEvent, 5000);
		if (net > 0 && netEvent.type == ENET_EVENT_TYPE_CONNECT)
		{
			if (Connect())
			{
				m_client->intercept = ENetUtil::InterceptCallback;
				m_thread = std::thread(&NetPlayClient::ThreadFunc, this);
			}
		}
		else
		{
			PanicAlertT("Failed to Connect!");
		}
	}
	else
	{
		if (address.size() > NETPLAY_CODE_SIZE)
		{
			PanicAlertT("Host code size is to large.\nPlease recheck that you have the correct code");
			return;
		}

		if (!EnsureTraversalClient(centralServer, centralPort))
			return;
		m_client = g_MainNetHost.get();

		m_traversal_client = g_TraversalClient.get();

		// If we were disconnected in the background, reconnect.
		if (m_traversal_client->m_State == TraversalClient::Failure)
			m_traversal_client->ReconnectToServer();
		m_traversal_client->m_Client = this;
		m_host_spec = address;
		m_connection_state = ConnectionState::WaitingForTraversalClientConnection;
		OnTraversalStateChanged();
		m_connecting = true;

		Common::Timer connect_timer;
		connect_timer.Start();

		while (m_connecting)
		{
			ENetEvent netEvent;
			if (m_traversal_client)
				m_traversal_client->HandleResends();

			while (enet_host_service(m_client, &netEvent, 4) > 0)
			{
				sf::Packet rpac;
				switch (netEvent.type)
				{
				case ENET_EVENT_TYPE_CONNECT:
					m_server = netEvent.peer;
					if (Connect())
					{
						m_connection_state = ConnectionState::Connected;
						m_thread = std::thread(&NetPlayClient::ThreadFunc, this);
					}
					return;
				default:
					break;
				}
			}
			if (connect_timer.GetTimeElapsed() > 5000)
				break;
		}
		PanicAlertT("Failed To Connect!");
	}
}

bool NetPlayClient::Connect()
{
	// send connect message
	sf::Packet spac;
	spac << scm_rev_git_str;
	spac << netplay_dolphin_ver;
	spac << m_player_name;
	Send(spac);
	enet_host_flush(m_client);
	sf::Packet rpac;
	// TODO: make this not hang
	ENetEvent netEvent;
	if (enet_host_service(m_client, &netEvent, 5000) > 0 && netEvent.type == ENET_EVENT_TYPE_RECEIVE)
	{
		rpac.append(netEvent.packet->data, netEvent.packet->dataLength);
		enet_packet_destroy(netEvent.packet);
	}
	else
	{
		return false;
	}

	MessageId error;
	rpac >> error;

	// got error message
	if (error)
	{
		switch (error)
		{
		case CON_ERR_SERVER_FULL:
			PanicAlertT("The server is full!");
			break;
		case CON_ERR_VERSION_MISMATCH:
			PanicAlertT("The server and client's NetPlay versions are incompatible!");
			break;
		case CON_ERR_GAME_RUNNING:
			PanicAlertT("The server responded: the game is currently running!");
			break;
		default:
			PanicAlertT("The server sent an unknown error message!");
			break;
		}

		Disconnect();
		return false;
	}
	else
	{
		rpac >> m_pid;

		Player player;
		player.name = m_player_name;
		player.pid = m_pid;
		player.revision = netplay_dolphin_ver;
		player.buffer = 0 /* will be raised once we get the packet */;

		// add self to player list
		m_players[m_pid] = player;
		local_player = &m_players[m_pid];

		dialog->Update();

		m_is_connected = true;

		return true;
	}
}

// called from ---GUI--- and ---NETPLAY--- thread
void NetPlayClient::SetLocalPlayerBuffer(u32 buffer)
{
	std::lock_guard<std::recursive_mutex> lkp(m_crit.players);

	local_player->buffer = buffer;
	if(local_player->buffer < m_minimum_buffer_size)
		local_player->buffer = m_minimum_buffer_size;

	auto spac = std::make_unique<sf::Packet>();
	*spac << static_cast<MessageId>(NP_MSG_PAD_BUFFER_PLAYER);
	*spac << local_player->buffer;
	SendAsync(std::move(spac));

	dialog->OnPlayerPadBufferChanged(local_player->buffer);
}

// called from ---NETPLAY--- thread
unsigned int NetPlayClient::OnData(sf::Packet& packet)
{
	MessageId mid;
	packet >> mid;

	switch (mid)
	{
	case NP_MSG_PLAYER_JOIN:
	{
		Player player;
		packet >> player.pid;
		packet >> player.name;
		packet >> player.revision;

		{
			std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
			m_players[player.pid] = player;
		}

		dialog->Update();
	}
	break;

	case NP_MSG_PLAYER_LEAVE:
	{
		PlayerId pid;
		packet >> pid;

		{
			std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
			m_players.erase(m_players.find(pid));
		}

		dialog->Update();
	}
	break;

	case NP_MSG_CHAT_MESSAGE:
	{
		PlayerId pid;
		packet >> pid;
		std::string msg;
		packet >> msg;

		// don't need lock to read in this thread
		const Player& player = m_players[pid];

		// add to gui
		std::ostringstream ss;
		ss << "[" << player.name << FindPlayerPadName(&player) << "]: " << msg;

		dialog->AppendChat(ss.str(), false);
	}
	break;

    case NP_MSG_REPORT_FRAME_TIME:
	{
		PlayerId pid;
		packet >> pid;

		float ftime;
		packet >> ftime;

		m_players[pid].frame_time = ftime;
	}
	break;

	case NP_MSG_PAD_MAPPING:
	{
		bool assigned = false;
		for (PadMapping& mapping : m_pad_map)
		{
			packet >> mapping;
			if (!assigned && mapping == this->m_pid)
				assigned = true;
		}
		dialog->SetSpectating(!assigned);

		UpdateDevices();

		dialog->Update();
	}
	break;

	case NP_MSG_WIIMOTE_MAPPING:
	{
		for (PadMapping& mapping : m_wiimote_map)
		{
			packet >> mapping;
		}

		dialog->Update();
	}
	break;

	case NP_MSG_PAD_DATA:
	{
		PadMapping map = 0;
		GCPadStatus pad;
		packet >> map >> pad.button >> pad.analogA >> pad.analogB >> pad.stickX >> pad.stickY >>
			pad.substickX >> pad.substickY >> pad.triggerLeft >> pad.triggerRight;

		// Trusting server for good map value (>=0 && <4)
		// add to pad buffer
		m_pad_buffer.at(map).Push(pad);
		m_gc_pad_event.Set();
	}
	break;

	case NP_MSG_WIIMOTE_DATA:
	{
		PadMapping map = 0;
		NetWiimote nw;
		u8 size;
		packet >> map >> size;

		nw.resize(size);

		for (unsigned int i = 0; i < size; ++i)
			packet >> nw[i];

		// Trusting server for good map value (>=0 && <4)
		// add to Wiimote buffer
		m_wiimote_buffer.at(map).Push(nw);
		m_wii_pad_event.Set();
	}
	break;

	case NP_MSG_PAD_BUFFER_MINIMUM:
	{
		u32 size = 0;
		packet >> size;

		m_minimum_buffer_size = size;
		dialog->OnMinimumPadBufferChanged(size);

		if(local_player->buffer < m_minimum_buffer_size)
			SetLocalPlayerBuffer(m_minimum_buffer_size);
	}
	break;

	case NP_MSG_PAD_BUFFER_PLAYER:
	{
		PlayerId pid;
		packet >> pid;

		{
			std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
			packet >> m_players[pid].buffer;
		}
	}
	break;

	case NP_MSG_CHANGE_GAME:
	{
		{
			std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
			packet >> m_selected_game;
		}

		// update gui
		dialog->OnMsgChangeGame(m_selected_game);

		sf::Packet spac;
		spac << static_cast<MessageId>(NP_MSG_GAME_STATUS);

		PlayerGameStatus status = dialog->FindGame(m_selected_game).empty() ?
			PlayerGameStatus::NotFound :
			PlayerGameStatus::Ok;

		spac << static_cast<u32>(status);
		Send(spac);
	}
	break;

	case NP_MSG_GAME_STATUS:
	{
		PlayerId pid;
		packet >> pid;

		{
			std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
			Player& player = m_players[pid];
			u32 status;
			packet >> status;
			player.game_status = static_cast<PlayerGameStatus>(status);
		}

		dialog->Update();
	}
	break;

	case NP_MSG_START_GAME:
	{
		{
			std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
			packet >> m_current_game;
			packet >> g_NetPlaySettings.m_CPUthread;
			packet >> g_NetPlaySettings.m_CPUcore;
			packet >> g_NetPlaySettings.m_EnableCheats;
			packet >> g_NetPlaySettings.m_SelectedLanguage;
			packet >> g_NetPlaySettings.m_OverrideGCLanguage;
			packet >> g_NetPlaySettings.m_ProgressiveScan;
			packet >> g_NetPlaySettings.m_PAL60;
			packet >> g_NetPlaySettings.m_DSPEnableJIT;
			packet >> g_NetPlaySettings.m_DSPHLE;
			packet >> g_NetPlaySettings.m_WriteToMemcard;
			packet >> g_NetPlaySettings.m_OCEnable;
			packet >> g_NetPlaySettings.m_OCFactor;

			int tmp;
			packet >> tmp;
			g_NetPlaySettings.m_EXIDevice[0] = (TEXIDevices)tmp;
			packet >> tmp;
			g_NetPlaySettings.m_EXIDevice[1] = (TEXIDevices)tmp;

            packet >> tmp;
            g_NetPlaySettings.m_LagReduction = (MeleeLagReductionCode)tmp;

            packet >> g_NetPlaySettings.m_MeleeForceWidescreen;

			u32 time_low, time_high;
			packet >> time_low;
			packet >> time_high;
			g_netplay_initial_rtc = time_low | ((u64)time_high << 32);
		}

		dialog->OnMsgStartGame();
	}
	break;

	case NP_MSG_STOP_GAME:
	case NP_MSG_DISABLE_GAME:
	{
		StopGame();
		dialog->OnMsgStopGame();
	}
	break;

	case NP_MSG_PING:
	{
		u32 ping_key = 0;
		packet >> ping_key;

		sf::Packet spac;
		spac << (MessageId)NP_MSG_PONG;
		spac << ping_key;

		Send(spac);
	}
	break;

	case NP_MSG_PLAYER_PING_DATA:
	{
		PlayerId pid;
		packet >> pid;

		{
			std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
			Player& player = m_players[pid];
			packet >> player.ping;
		}

		DisplayPlayersPing();
		dialog->Update();
	}
	break;

	case NP_MSG_DESYNC_DETECTED:
	{
		int pid_to_blame;
		u32 frame;
		packet >> pid_to_blame;
		packet >> frame;

		std::string player = "??";
		std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
		{
			auto it = m_players.find(pid_to_blame);
			if (it != m_players.end())
				player = it->second.name;
		}
		dialog->OnDesync(frame, player);
	}
	break;

	case NP_MSG_SYNC_GC_SRAM:
	{
		u8 sram[sizeof(g_SRAM.p_SRAM)];
		for (size_t i = 0; i < sizeof(g_SRAM.p_SRAM); ++i)
		{
			packet >> sram[i];
		}

		{
			std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
			memcpy(g_SRAM.p_SRAM, sram, sizeof(g_SRAM.p_SRAM));
			g_SRAM_netplay_initialized = true;
		}
	}
	break;

	case NP_MSG_COMPUTE_MD5:
	{
		std::string file_identifier;
		packet >> file_identifier;

		ComputeMD5(file_identifier);
	}
	break;

	case NP_MSG_MD5_PROGRESS:
	{
		PlayerId pid;
		int progress;
		packet >> pid;
		packet >> progress;

		dialog->SetMD5Progress(pid, progress);
	}
	break;

	case NP_MSG_MD5_RESULT:
	{
		PlayerId pid;
		std::string result;
		packet >> pid;
		packet >> result;

		dialog->SetMD5Result(pid, result);
	}
	break;

	case NP_MSG_MD5_ERROR:
	{
		PlayerId pid;
		std::string error;
		packet >> pid;
		packet >> error;

		dialog->SetMD5Result(pid, error);
	}
	break;

	case NP_MSG_MD5_ABORT:
	{
		m_should_compute_MD5 = false;
		dialog->AbortMD5();
	}
	break;

	default:
		PanicAlertT("Unknown message received with id : %d", mid);
		break;
	}

	return 0;
}

void NetPlayClient::Send(sf::Packet& packet)
{
	ENetPacket* epac =
		enet_packet_create(packet.getData(), packet.getDataSize(), ENET_PACKET_FLAG_RELIABLE);
	enet_peer_send(m_server, 0, epac);
}

void NetPlayClient::DisplayPlayersPing()
{
	if (!g_ActiveConfig.bShowNetPlayPing)
		return;

	OSD::AddTypedMessage(OSD::MessageType::NetPlayPing,
		StringFromFormat("Ping: %u", GetPlayersMaxPing()), OSD::Duration::SHORT,
		OSD::Color::CYAN);
}

u32 NetPlayClient::GetPlayersMaxPing() const
{
	return std::max_element(
		m_players.begin(), m_players.end(),
		[](const auto& a, const auto& b) { return a.second.ping < b.second.ping; })
		->second.ping;
}

void NetPlayClient::Disconnect()
{
	ENetEvent netEvent;
	m_connecting = false;
	m_connection_state = ConnectionState::Failure;
	if (m_server)
		enet_peer_disconnect(m_server, 0);
	else
		return;

	while (enet_host_service(m_client, &netEvent, 3000) > 0)
	{
		switch (netEvent.type)
		{
		case ENET_EVENT_TYPE_RECEIVE:
			enet_packet_destroy(netEvent.packet);
			break;
		case ENET_EVENT_TYPE_DISCONNECT:
			m_server = nullptr;
			return;
		default:
			break;
		}
	}
	// didn't disconnect gracefully force disconnect
	enet_peer_reset(m_server);
	m_server = nullptr;
}

void NetPlayClient::SendAsync(std::unique_ptr<sf::Packet> packet)
{
	{
		std::lock_guard<std::recursive_mutex> lkq(m_crit.async_queue_write);
		m_async_queue.Push(std::move(packet));
	}
	ENetUtil::WakeupThread(m_client);
}

// called from ---NETPLAY--- thread
void NetPlayClient::ThreadFunc()
{
    bool qos_success = false;
#ifdef _WIN32
	QOS_VERSION ver = { 1, 0 };

	if(SConfig::GetInstance().bQoSEnabled && QOSCreateHandle(&ver, &m_qos_handle))
	{
		// from win32.c
		struct sockaddr_in sin = { 0 };

		sin.sin_family = AF_INET;
		sin.sin_port = ENET_HOST_TO_NET_16(m_server->host->address.port);
		sin.sin_addr.s_addr = m_server->host->address.host;

		if (QOSAddSocketToFlow(m_qos_handle, m_server->host->socket, reinterpret_cast<PSOCKADDR>(&sin),
			// this is 0x38
			QOSTrafficTypeControl,
			QOS_NON_ADAPTIVE_FLOW,
			&m_qos_flow_id))
		{
			DWORD dscp = 0x2e;

			// this will fail if we're not admin
			// sets DSCP to the same as linux (0x2e)
			QOSSetFlow(m_qos_handle,
				m_qos_flow_id,
				QOSSetOutgoingDSCPValue,
				sizeof(DWORD),
				&dscp,
				0,
				nullptr);

			qos_success = true;
		}
	}
#else
	if(SConfig::GetInstance().bQoSEnabled)
	{
#ifdef __linux__
		// highest priority
		int priority = 7;
		setsockopt(m_server->host->socket, SOL_SOCKET, SO_PRIORITY, &priority, sizeof(priority));
#endif

		// https://www.tucny.com/Home/dscp-tos
		// ef is better than cs7
		int tos_val = 0xb8;
		qos_success = setsockopt(m_server->host->socket, IPPROTO_IP, IP_TOS, &tos_val, sizeof(tos_val)) == 0;
	}
#endif

	if(SConfig::GetInstance().bQoSEnabled)
	{
		if(qos_success)
			dialog->AppendChat("QoS was successfully enabled, netplay packets should be prioritized over normal packets", false);
		else
			dialog->AppendChat("QoS couldn't be enabled, other network activity might interfere with netplay", false);
	}

	while (m_do_loop.IsSet())
	{
		ENetEvent netEvent;
		int net;
		if (m_traversal_client)
			m_traversal_client->HandleResends();
		net = enet_host_service(m_client, &netEvent, 250);
		while (!m_async_queue.Empty())
		{
			Send(*(m_async_queue.Front().get()));
			m_async_queue.Pop();
		}
		if (net > 0)
		{
			sf::Packet rpac;
			switch (netEvent.type)
			{
			case ENET_EVENT_TYPE_RECEIVE:
				rpac.append(netEvent.packet->data, netEvent.packet->dataLength);
				OnData(rpac);

				enet_packet_destroy(netEvent.packet);
				break;
			case ENET_EVENT_TYPE_DISCONNECT:
				dialog->OnConnectionLost();

				if (m_is_running.IsSet())
					StopGame();

				break;
			default:
				break;
			}
		}
	}

#ifdef _WIN32
	if(m_qos_handle != 0)
	{
		if(m_qos_flow_id != 0)
			QOSRemoveSocketFromFlow(m_qos_handle, m_server->host->socket, m_qos_flow_id, 0);
		QOSCloseHandle(m_qos_handle);
	}
#endif

	Disconnect();
	return;
}

// called from ---GUI--- thread
void NetPlayClient::GetPlayerList(std::string& list, std::vector<int>& pid_list)
{
	std::lock_guard<std::recursive_mutex> lkp(m_crit.players);

	std::ostringstream ss;

	const auto enumerate_player_controller_mappings = [&ss](const PadMappingArray& mappings,
		const Player& player) {
		for (size_t i = 0; i < mappings.size(); i++)
		{
			if (mappings[i] == player.pid)
				ss << i + 1;
			else
				ss << '-';
		}
	};

	for (const auto& entry : m_players)
	{
		const Player& player = entry.second;
		ss << player.name << "[" << static_cast<int>(player.pid) << "] : " << player.revision << " | ";

		enumerate_player_controller_mappings(m_pad_map, player);
		enumerate_player_controller_mappings(m_wiimote_map, player);

		ss << " |\nPing: " << player.ping << "ms\n";

        std::string frame_time_str = std::to_string(player.frame_time);
        frame_time_str = frame_time_str.substr(0, 3);

        while(frame_time_str.find(",") != std::string::npos)
            frame_time_str[frame_time_str.find(",")] = '.';

        int percent_of_full_frame = SConfig::GetInstance().bNTSC ? (int)(player.frame_time / (1000.0 / 60.0) * 100) : (int)(player.frame_time / (1000.0 / 50.0) * 100);

        ss << "Frame time: " << (player.frame_time == 0 ? "(unknown)" : frame_time_str + " ms (" + std::to_string(percent_of_full_frame) + "% of max)") << "\n";

		ss << "Buffer: " << player.buffer << "\n";
		ss << "Status: ";

		switch (player.game_status)
		{
		case PlayerGameStatus::Ok:
			ss << "ready";
			break;

		case PlayerGameStatus::NotFound:
			ss << "game missing";
			break;

		default:
			ss << "unknown";
			break;
		}

		ss << "\n\n";

		pid_list.push_back(player.pid);
	}

	list = ss.str();
}

// called from ---GUI--- thread
std::vector<const Player*> NetPlayClient::GetPlayers()
{
	std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
	std::vector<const Player*> players;

	for (const auto& pair : m_players)
		players.push_back(&pair.second);

	return players;
}

// called from ---GUI--- thread
void NetPlayClient::SendChatMessage(const std::string& msg)
{
	auto spac = std::make_unique<sf::Packet>();
	*spac << static_cast<MessageId>(NP_MSG_CHAT_MESSAGE);
	*spac << msg;

	SendAsync(std::move(spac));
}

// called from ---CPU--- thread
void NetPlayClient::ReportFrameTimeToServer(float frame_time)
{
	auto spac = std::make_unique<sf::Packet>();
	*spac << static_cast<MessageId>(NP_MSG_REPORT_FRAME_TIME);
	*spac << frame_time;

	SendAsync(std::move(spac));

    local_player->frame_time = frame_time;
}

// called from ---CPU--- thread
void NetPlayClient::SendPadState(const int in_game_pad, const GCPadStatus& pad)
{
	auto spac = std::make_unique<sf::Packet>();
	*spac << static_cast<MessageId>(NP_MSG_PAD_DATA);
	*spac << static_cast<PadMapping>(in_game_pad);
	*spac << pad.button << pad.analogA << pad.analogB << pad.stickX << pad.stickY << pad.substickX
		<< pad.substickY << pad.triggerLeft << pad.triggerRight;

	SendAsync(std::move(spac));
}

// called from ---CPU--- thread
void NetPlayClient::SendWiimoteState(const int in_game_pad, const NetWiimote& nw)
{
	auto spac = std::make_unique<sf::Packet>();
	*spac << static_cast<MessageId>(NP_MSG_WIIMOTE_DATA);
	*spac << static_cast<PadMapping>(in_game_pad);
	*spac << static_cast<u8>(nw.size());
	for (auto it : nw)
	{
		*spac << it;
	}

	SendAsync(std::move(spac));
}

// called from ---GUI--- thread
void NetPlayClient::SendStartGamePacket()
{
	auto spac = std::make_unique<sf::Packet>();
	*spac << static_cast<MessageId>(NP_MSG_START_GAME);
	*spac << m_current_game;

	SendAsync(std::move(spac));
}

// called from ---GUI--- thread
void NetPlayClient::SendStopGamePacket()
{
	auto spac = std::make_unique<sf::Packet>();
	*spac << static_cast<MessageId>(NP_MSG_STOP_GAME);

	SendAsync(std::move(spac));
}

// called from ---GUI--- thread
bool NetPlayClient::StartGame(const std::string& path)
{
	std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
	SendStartGamePacket();

	if (m_is_running.IsSet())
	{
		PanicAlertT("Game is already running!");
		return false;
	}

	m_timebase_frame = 0;

	m_is_running.Set();
	NetPlay_Enable(this);

	ClearBuffers();

	if (dialog->IsRecording())
	{
		if (Movie::IsReadOnly())
			Movie::SetReadOnly(false);

		u8 controllers_mask = 0;
		for (unsigned int i = 0; i < 4; ++i)
		{
			if (m_pad_map[i] > 0)
				controllers_mask |= (1 << i);
			if (m_wiimote_map[i] > 0)
				controllers_mask |= (1 << (i + 4));
		}
		Movie::BeginRecordingInput(controllers_mask);
	}

	// boot game

	dialog->BootGame(path);

	if (SConfig::GetInstance().bWii)
	{
		for (unsigned int i = 0; i < 4; ++i)
			WiimoteReal::ChangeWiimoteSource(i,
				m_wiimote_map[i] > 0 ? WIIMOTE_SRC_EMU : WIIMOTE_SRC_NONE);
	}

	UpdateDevices();

	return true;
}

// called from ---GUI--- thread
bool NetPlayClient::ChangeGame(const std::string&)
{
	return true;
}

// called from ---NETPLAY--- thread
void NetPlayClient::UpdateDevices()
{
	u8 local_pad = 0;
	u8 pad = 0;

	for (auto player_id : m_pad_map)
	{
		// Use local controller types for local controllers if they are compatible
		// Only GCController-like controllers are supported, GBA and similar
		// exotic devices are not supported on netplay.
		if (player_id == local_player->pid)
		{
			if (SIDevice_IsGCController(SConfig::GetInstance().m_SIDevice[local_pad]))
			{
				SerialInterface::AddDevice(SConfig::GetInstance().m_SIDevice[local_pad], pad);
			}
			else
			{
				SerialInterface::AddDevice(SIDEVICE_GC_CONTROLLER, pad);
			}
			local_pad++;
		}
		else if (player_id > 0)
		{
			SerialInterface::AddDevice(SIDEVICE_GC_CONTROLLER, pad);
		}
		else
		{
			SerialInterface::AddDevice(SIDEVICE_NONE, pad);
		}
		pad++;
	}
}

// called from ---NETPLAY--- thread
void NetPlayClient::ClearBuffers()
{
	// clear pad buffers, Clear method isn't thread safe
	for (unsigned int i = 0; i < 4; ++i)
	{
		while (m_pad_buffer[i].Size())
			m_pad_buffer[i].Pop();

		while (m_wiimote_buffer[i].Size())
			m_wiimote_buffer[i].Pop();
	}
}

// called from ---NETPLAY--- thread
void NetPlayClient::OnTraversalStateChanged()
{
	if (m_connection_state == ConnectionState::WaitingForTraversalClientConnection &&
		m_traversal_client->m_State == TraversalClient::Connected)
	{
		m_connection_state = ConnectionState::WaitingForTraversalClientConnectReady;
		m_traversal_client->ConnectToClient(m_host_spec);
	}
	else if (m_connection_state != ConnectionState::Failure &&
		m_traversal_client->m_State == TraversalClient::Failure)
	{
		Disconnect();
		dialog->OnTraversalError(m_traversal_client->m_FailureReason);
	}
}

// called from ---NETPLAY--- thread
void NetPlayClient::OnConnectReady(ENetAddress addr)
{
	if (m_connection_state == ConnectionState::WaitingForTraversalClientConnectReady)
	{
		m_connection_state = ConnectionState::Connecting;
		enet_host_connect(m_client, &addr, 0, 0);
	}
}

// called from ---NETPLAY--- thread
void NetPlayClient::OnConnectFailed(u8 reason)
{
	m_connecting = false;
	m_connection_state = ConnectionState::Failure;
	switch (reason)
	{
	case TraversalConnectFailedClientDidntRespond:
		PanicAlertT("Traversal server timed out connecting to the host");
		break;
	case TraversalConnectFailedClientFailure:
		PanicAlertT("Server rejected traversal attempt");
		break;
	case TraversalConnectFailedNoSuchClient:
		PanicAlertT("Invalid host");
		break;
	default:
		PanicAlertT("Unknown error %x", reason);
		break;
	}
}

// called from ---CPU--- thread
bool NetPlayClient::GetNetPads(const int pad_nb, GCPadStatus* pad_status)
{
	SendNetPad(pad_nb);

	// Now, we either use the data pushed earlier, or wait for the
	// other clients to send it to us
	while (m_pad_buffer[pad_nb].Size() == 0)
	{
		if (!m_is_running.IsSet())
		{
			return false;
		}

		m_gc_pad_event.Wait();
	}

	m_pad_buffer[pad_nb].Pop(*pad_status);

	if (Movie::IsRecordingInput())
	{
		Movie::RecordInput(pad_status, pad_nb);
		Movie::InputUpdate();
	}
	else
	{
		Movie::CheckPadStatus(pad_status, pad_nb);
	}

	return true;
}

// called from ---GUI--- thread
void NetPlayClient::SendSpectatorSetting(bool spectator) {
	auto spac = std::make_unique<sf::Packet>();
	*spac << static_cast<MessageId>(NP_MSG_PAD_SPECTATOR);
	*spac << spectator;
	SendAsync(std::move(spac));
}

// called from ---CPU--- thread
void NetPlayClient::SendNetPad(int pad_nb)
{
	GCPadStatus status = {0};
	status.stickX = status.stickY =
	status.substickX = status.substickY =
	/* these are all the same */ GCPadStatus::MAIN_STICK_CENTER_X;

	// this is the old behavior
	// just a small lag decrease
	if(IsFirstInGamePad(pad_nb))
	{
		for(int i = 0; i < NumLocalPads(); i++)
		{
			int ingame_pad = LocalPadToInGamePad(i);

			if(m_pad_buffer[ingame_pad].Size() <= BufferSizeForPort(ingame_pad) / (SConfig::GetInstance().iPollingMethod == POLLING_ONSIREAD ? buffer_accuracy : 1))
			{
                if(!OSD::Chat::toggled)
                {
                    switch (SConfig::GetInstance().m_SIDevice[i])
                    {
                    case SIDEVICE_WIIU_ADAPTER:
                        status = GCAdapter::Input(i);
                        break;
                    case SIDEVICE_GC_CONTROLLER:
                    default:
                        status = Pad::GetStatus(i);
                        break;
                    }
                }

				while (m_pad_buffer[ingame_pad].Size() <= BufferSizeForPort(ingame_pad) / (SConfig::GetInstance().iPollingMethod == POLLING_ONSIREAD ? buffer_accuracy : 1))
				{
					m_pad_buffer[ingame_pad].Push(status);
					SendPadState(ingame_pad, status);
				}
			}
		}
	}
	// this is only to make sure that the buffer won't be empty
	else
	{
		int local_pad = InGamePadToLocalPad(pad_nb);
		if(local_pad != 4)
		{
			if(m_pad_buffer[pad_nb].Size() <= BufferSizeForPort(pad_nb) / (SConfig::GetInstance().iPollingMethod == POLLING_ONSIREAD ? buffer_accuracy : 1))
			{
                if(!OSD::Chat::toggled)
                {
                    switch (SConfig::GetInstance().m_SIDevice[local_pad])
                    {
                    case SIDEVICE_WIIU_ADAPTER:
                        status = GCAdapter::Input(local_pad);
                        break;
                    case SIDEVICE_GC_CONTROLLER:
                    default:
                        status = Pad::GetStatus(local_pad);
                        break;
                    }
                }

				while (m_pad_buffer[pad_nb].Size() <= BufferSizeForPort(pad_nb) / (SConfig::GetInstance().iPollingMethod == POLLING_ONSIREAD ? buffer_accuracy : 1))
				{
					m_pad_buffer[pad_nb].Push(status);
					SendPadState(pad_nb, status);
				}
			}
		}
	}
}

// called from ---CPU--- thread
bool NetPlayClient::WiimoteUpdate(int _number, u8* data, const u8 size, u8 reporting_mode)
{
	NetWiimote nw;
	{
		std::lock_guard<std::recursive_mutex> lkp(m_crit.players);

		// Only send data, if this Wiimote is mapped to this player
		if (m_wiimote_map[_number] == local_player->pid)
		{
			nw.assign(data, data + size);
			do
			{
				// add to buffer
				m_wiimote_buffer[_number].Push(nw);

				SendWiimoteState(_number, nw);
			} while (m_wiimote_buffer[_number].Size() <=
				m_minimum_buffer_size * 200 /
				120);  // TODO: add a seperate setting for wiimote buffer?
		}

	}  // unlock players

	while (m_wiimote_buffer[_number].Size() == 0)
	{
		if (!m_is_running.IsSet())
		{
			return false;
		}

		// wait for receiving thread to push some data
		m_wii_pad_event.Wait();
	}

	m_wiimote_buffer[_number].Pop(nw);

	// If the reporting mode has changed, we just need to pop through the buffer,
	// until we reach a good input
	if (nw[1] != reporting_mode)
	{
		u32 tries = 0;
		while (nw[1] != reporting_mode)
		{
			while (m_wiimote_buffer[_number].Size() == 0)
			{
				if (!m_is_running.IsSet())
				{
					return false;
				}

				// wait for receiving thread to push some data
				m_wii_pad_event.Wait();
			}

			m_wiimote_buffer[_number].Pop(nw);

			++tries;
			if (tries > m_minimum_buffer_size * 200 / 120)
				break;
		}

		// If it still mismatches, it surely desynced
		if (nw[1] != reporting_mode)
		{
			PanicAlertT("Netplay has desynced. There is no way to recover from this.");
			return false;
		}
	}

	memcpy(data, nw.data(), size);
	return true;
}

// called from ---GUI--- thread and ---NETPLAY--- thread (client side)
bool NetPlayClient::StopGame()
{
	m_is_running.Clear();

	// stop waiting for input
	m_gc_pad_event.Set();
	m_wii_pad_event.Set();

	NetPlay_Disable();

	// stop game
	dialog->StopGame();

    for(auto& player : m_players)
        player.second.frame_time = 0;

	return true;
}

// called from ---GUI--- thread
void NetPlayClient::Stop()
{
	if (!m_is_running.IsSet())
		return;

	m_is_running.Clear();

	// stop waiting for input
	m_gc_pad_event.Set();
	m_wii_pad_event.Set();

	// Tell the server to stop if we have a pad mapped in game.
	if (LocalPlayerHasControllerMapped())
		SendStopGamePacket();
}

// called from ---GUI--- thread
bool NetPlayClient::LocalPlayerHasControllerMapped() const
{
	const auto mapping_matches_player_id = [this](const PadMapping& mapping) {
		return mapping == local_player->pid;
	};

	return std::any_of(m_pad_map.begin(), m_pad_map.end(), mapping_matches_player_id) ||
		std::any_of(m_wiimote_map.begin(), m_wiimote_map.end(), mapping_matches_player_id);
}

bool NetPlayClient::IsFirstInGamePad(int ingame_pad) const
{
	return std::none_of(m_pad_map.begin(), m_pad_map.begin() + ingame_pad,
		[](auto mapping) { return mapping > 0; });
}

int NetPlayClient::NumLocalPads() const
{
	return static_cast<int>(std::count_if(m_pad_map.begin(), m_pad_map.end(), [this](auto mapping) {
		return mapping == local_player->pid;
	}));
}

int NetPlayClient::InGamePadToLocalPad(int ingame_pad)
{
	// not our pad
	if (m_pad_map[ingame_pad] != local_player->pid)
		return 4;

	int local_pad = 0;
	int pad = 0;

	for (; pad < ingame_pad; pad++)
	{
		if (m_pad_map[pad] == local_player->pid)
			local_pad++;
	}

	return local_pad;
}

int NetPlayClient::LocalPadToInGamePad(int local_pad)
{
	// Figure out which in-game pad maps to which local pad.
	// The logic we have here is that the local slots always
	// go in order.
	int local_pad_count = -1;
	int ingame_pad = 0;
	for (; ingame_pad < 4; ingame_pad++)
	{
		if (m_pad_map[ingame_pad] == local_player->pid)
			local_pad_count++;

		if (local_pad_count == local_pad)
			break;
	}

	return ingame_pad;
}

void NetPlayClient::SendTimeBase()
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);

	u64 timebase = SystemTimers::GetFakeTimeBase();

	auto spac = std::make_unique<sf::Packet>();
	*spac << static_cast<MessageId>(NP_MSG_TIMEBASE);
	*spac << static_cast<u32>(timebase);
	*spac << static_cast<u32>(timebase << 32);
	*spac << netplay_client->m_timebase_frame++;

	netplay_client->SendAsync(std::move(spac));
}

bool NetPlayClient::DoAllPlayersHaveGame()
{
	std::lock_guard<std::recursive_mutex> lkp(m_crit.players);

	return std::all_of(std::begin(m_players), std::end(m_players),
		[](auto entry) { return entry.second.game_status == PlayerGameStatus::Ok; });
}

void NetPlayClient::ComputeMD5(const std::string& file_identifier)
{
	if (m_should_compute_MD5)
		return;

	dialog->ShowMD5Dialog(file_identifier);
	m_should_compute_MD5 = true;

	std::string file;
	if (file_identifier == WII_SDCARD)
		file = File::GetUserPath(F_WIISDCARD_IDX);
	else
		file = dialog->FindGame(file_identifier);

	if (file.empty() || !File::Exists(file))
	{
		sf::Packet spac;
		spac << static_cast<MessageId>(NP_MSG_MD5_ERROR);
		spac << "file not found";
		Send(spac);
		return;
	}

	m_MD5_thread = std::thread([this, file]() {
		std::string sum = MD5::MD5Sum(file, [&](int progress) {
			sf::Packet spac;
			spac << static_cast<MessageId>(NP_MSG_MD5_PROGRESS);
			spac << progress;
			Send(spac);

			return m_should_compute_MD5;
		});

		sf::Packet spac;
		spac << static_cast<MessageId>(NP_MSG_MD5_RESULT);
		spac << sum;
		Send(spac);
	});
	m_MD5_thread.detach();
}

// stuff hacked into dolphin

// called from ---CPU--- thread
// Actual Core function which is called on every frame
bool CSIDevice_GCController::NetPlay_GetInput(int numPAD, GCPadStatus* PadStatus)
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);

	if (netplay_client)
		return netplay_client->GetNetPads(numPAD, PadStatus);
	else
		return false;
}

bool WiimoteEmu::Wiimote::NetPlay_GetWiimoteData(int wiimote, u8* data, u8 size, u8 reporting_mode)
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);

	if (netplay_client)
		return netplay_client->WiimoteUpdate(wiimote, data, size, reporting_mode);
	else
		return false;
}

// called from ---CPU--- thread
// so all players' games get the same time
//
// also called from ---GUI--- thread when starting input recording
u64 CEXIIPL::NetPlay_GetEmulatedTime()
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);

	if (netplay_client)
		return g_netplay_initial_rtc;
	else
		return 0;
}

// called from ---CPU--- thread
// return the local pad num that should rumble given a ingame pad num
int CSIDevice_GCController::NetPlay_InGamePadToLocalPad(int numPAD)
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);

	if (netplay_client)
		return netplay_client->InGamePadToLocalPad(numPAD);
	else
		return numPAD;
}

bool NetPlay::IsNetPlayRunning()
{
	return netplay_client != nullptr;
}

void NetPlay_Enable(NetPlayClient* const np)
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);
	netplay_client = np;
}

void NetPlay_Disable()
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);
	netplay_client = nullptr;
}
