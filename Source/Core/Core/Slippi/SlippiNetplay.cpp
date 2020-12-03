// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Slippi/SlippiNetplay.h"
#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/ENetUtil.h"
#include "Common/MD5.h"
#include "Common/MsgHandler.h"
#include "Common/Timer.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
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
#include <SlippiGame.h>
#include <algorithm>
#include <fstream>
#include <mbedtls/md5.h>
#include <memory>
#include <thread>

static std::mutex pad_mutex;
static std::mutex ack_mutex;

// called from ---GUI--- thread
SlippiNetplayClient::~SlippiNetplayClient()
{
	m_do_loop.Clear();
	if (m_thread.joinable())
		m_thread.join();

	if (!m_server.empty())
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

	WARN_LOG(SLIPPI_ONLINE, "Netplay client cleanup complete");
}

// called from ---SLIPPI EXI--- thread
SlippiNetplayClient::SlippiNetplayClient(const std::string &address1, const u16 remotePort1,
                                         const std::string &address2, const u16 remotePort2, const u16 localPort,
                                         bool isDecider, u8 playerIdx)
#ifdef _WIN32
    : m_qos_handle(nullptr)
    , m_qos_flow_id(0)
#endif
{
	WARN_LOG(SLIPPI_ONLINE, "Initializing Slippi Netplay for port: %d, with host: %s", localPort,
	         isDecider ? "true" : "false");

	this->isDecider = isDecider;
	this->playerIdx = playerIdx;

	int j = 1;
	for (int i = 0; i < 3; i++)
	{
		if (j == playerIdx)
			j++;
		this->matchInfo.remotePlayerSelections[i] = SlippiPlayerSelections();
		this->matchInfo.remotePlayerSelections[i].playerIdx = j;
		j++;
	}

	// Local address
	ENetAddress *localAddr = nullptr;
	ENetAddress localAddrDef;

	// It is important to be able to set the local port to listen on even in a client connection because
	// not doing so will break hole punching, the host is expecting traffic to come from a specific ip/port
	// and if the port does not match what it is expecting, it will not get through the NAT on some routers
	if (localPort > 0)
	{
		INFO_LOG(SLIPPI_ONLINE, "Setting up local address");

		localAddrDef.host = ENET_HOST_ANY;
		localAddrDef.port = localPort;

		localAddr = &localAddrDef;
	}

	// TODO: Figure out how to use a local port when not hosting without accepting incoming connections
	m_client = enet_host_create(localAddr, 4, 10, 0, 0);

	if (m_client == nullptr)
	{
		PanicAlertT("Couldn't Create Client");
	}

	for (int i = 0; i < 2; i++)
	{
		std::string address = "";
		u16 remotePort = 0;
		if (i == 0)
		{
			address = address1;
			remotePort = remotePort1;
		}
		else
		{
			address = address2;
			remotePort = remotePort2;
		}

		ENetAddress addr;
		enet_address_set_host(&addr, address.c_str());
		addr.port = remotePort;
		INFO_LOG(SLIPPI_ONLINE, "Set ENet host, addr = %s, port = %d", addr.host, addr.port);

		ENetPeer *peer = enet_host_connect(m_client, &addr, 3, 0);
		m_server.push_back(peer);

		if (peer == nullptr)
		{
			PanicAlertT("Couldn't create peer.");
		}
		else
		{
			INFO_LOG(SLIPPI_ONLINE, "Connected to ENet host, addr = %x, port = %d", peer->address.host,
			         peer->address.port);
		}
	}

	slippiConnectStatus = SlippiConnectStatus::NET_CONNECT_STATUS_INITIATED;

	m_thread = std::thread(&SlippiNetplayClient::ThreadFunc, this);
}

// Make a dummy client
SlippiNetplayClient::SlippiNetplayClient(bool isDecider)
{
	this->isDecider = isDecider;
	slippiConnectStatus = SlippiConnectStatus::NET_CONNECT_STATUS_FAILED;
}

// called from ---NETPLAY--- thread
unsigned int SlippiNetplayClient::OnData(sf::Packet &packet)
{
	MessageId mid = 0;
	if (!(packet >> mid))
	{
		ERROR_LOG(SLIPPI_ONLINE, "Received empty netplay packet");
		return 0;
	}

	switch (mid)
	{
	case NP_MSG_SLIPPI_PAD:
	{
		int32_t frame;
		if (!(packet >> frame))
		{
			ERROR_LOG(SLIPPI_ONLINE, "Netplay packet too small to read frame count");
			break;
		}
		u8 playerIdx;
		if (!(packet >> playerIdx))
		{
			ERROR_LOG(SLIPPI_ONLINE, "Netplay packet too small to read player index");
			break;
		}

		// Pad received, try to guess what our local time was when the frame was sent by our opponent
		// before we initialized
		// We can compare this to when we sent a pad for last frame to figure out how far/behind we
		// are with respect to the opponent

		u64 curTime = Common::Timer::GetTimeUs();

		auto timing = lastFrameTiming;
		if (!hasGameStarted)
		{
			// Handle case where opponent starts sending inputs before our game has reached frame 1. This will
			// continuously say frame 0 is now to prevent opp from getting too far ahead
			timing.frame = 0;
			timing.timeUs = curTime;
		}

		s64 opponentSendTimeUs = curTime - (pingUs / 2);
		s64 frameDiffOffsetUs = 16683 * (timing.frame - frame);
		s64 timeOffsetUs = opponentSendTimeUs - timing.timeUs + frameDiffOffsetUs;

		INFO_LOG(SLIPPI_ONLINE, "[Offset] Opp Frame: %d, My Frame: %d. Time offset: %lld", frame, timing.frame,
		         timeOffsetUs);

		// Add this offset to circular buffer for use later
		if (frameOffsetData.buf.size() < SLIPPI_ONLINE_LOCKSTEP_INTERVAL)
			frameOffsetData.buf.push_back((s32)timeOffsetUs);
		else
			frameOffsetData.buf[frameOffsetData.idx] = (s32)timeOffsetUs;

		frameOffsetData.idx = (frameOffsetData.idx + 1) % SLIPPI_ONLINE_LOCKSTEP_INTERVAL;

		{
			std::lock_guard<std::mutex> lk(pad_mutex); // TODO: Is this the correct lock?

			auto packetData = (u8 *)packet.getData();

			INFO_LOG(SLIPPI_ONLINE, "Receiving a packet of inputs from player %d [%d]...", playerIdx, frame);

			int32_t headFrame = remotePadQueue.empty() ? 0 : remotePadQueue.front()->frame;
			int inputsToCopy = frame - headFrame;

			//// Check that the packet actually contains the data it claims to
			// if((5 + inputsToCopy * SLIPPI_PAD_DATA_SIZE) > (int)packet.getDataSize())
			//{
			//	ERROR_LOG(SLIPPI_ONLINE, "Netplay packet too small to read pad buffer");
			//	break;
			//}

			for (int i = inputsToCopy - 1; i >= 0; i--)
			{
				auto pad = std::make_unique<SlippiPad>(frame - i, playerIdx, &packetData[6 + i * SLIPPI_PAD_DATA_SIZE]);

				INFO_LOG(SLIPPI_ONLINE, "Rcv [%d] -> %02X %02X %02X %02X %02X %02X %02X %02X", pad->frame,
				         pad->padBuf[0], pad->padBuf[1], pad->padBuf[2], pad->padBuf[3], pad->padBuf[4], pad->padBuf[5],
				         pad->padBuf[6], pad->padBuf[7]);

				remotePadQueue.push_front(std::move(pad));
			}
		}

		// Send Ack
		sf::Packet spac;
		spac << (MessageId)NP_MSG_SLIPPI_PAD_ACK;
		spac << frame;
		Send(spac);
	}
	break;

	case NP_MSG_SLIPPI_PAD_ACK:
	{
		std::lock_guard<std::mutex> lk(ack_mutex); // Trying to fix rare crash on ackTimers.count

		// Store last frame acked
		int32_t frame;
		if (!(packet >> frame))
		{
			ERROR_LOG(SLIPPI_ONLINE, "Ack packet too small to read frame");
			break;
		}

		lastFrameAcked = frame > lastFrameAcked ? frame : lastFrameAcked;

		// Remove old timings
		while (!ackTimers.Empty() && ackTimers.Front().frame < frame)
		{
			ackTimers.Pop();
		}

		// Don't get a ping if we do not have the right ack frame
		if (ackTimers.Empty() || ackTimers.Front().frame != frame)
		{
			break;
		}

		auto sendTime = ackTimers.Front().timeUs;
		ackTimers.Pop();

		pingUs = Common::Timer::GetTimeUs() - sendTime;
		if (g_ActiveConfig.bShowNetPlayPing && frame % SLIPPI_PING_DISPLAY_INTERVAL == 0)
		{
			OSD::AddTypedMessage(OSD::MessageType::NetPlayPing, StringFromFormat("Ping: %u", pingUs / 1000),
			                     OSD::Duration::NORMAL, OSD::Color::CYAN);
		}
	}
	break;

	case NP_MSG_SLIPPI_MATCH_SELECTIONS:
	{
		auto s = readSelectionsFromPacket(packet);
		INFO_LOG(SLIPPI_ONLINE, "[Netplay] Received selections from opponent with player idx %d", s->playerIdx);
		for (int i = 0; i < 3; i++) {
			if (matchInfo.remotePlayerSelections[i].playerIdx == s->playerIdx)
			{
				matchInfo.remotePlayerSelections[i].Merge(*s);
				break;
			}
		}

		// This might be a good place to reset some logic? Game can't start until we receive this msg
		// so this should ensure that everything is initialized before the game starts
		// TODO: This could cause issues in the case of a desync? If this is ever received mid-game, bad things
		// TODO: will happen. Consider improving this
		hasGameStarted = false;
	}
	break;

	case NP_MSG_SLIPPI_CONN_SELECTED:
	{
		// Currently this is unused but the intent is to support two-way simultaneous connection attempts
		isConnectionSelected = true;
	}
	break;

	default:
		WARN_LOG(SLIPPI_ONLINE, "Unknown message received with id : %d", mid);
		break;
	}

	return 0;
}

void SlippiNetplayClient::writeToPacket(sf::Packet &packet, SlippiPlayerSelections &s)
{
	packet << static_cast<MessageId>(NP_MSG_SLIPPI_MATCH_SELECTIONS);
	packet << s.characterId << s.characterColor << s.isCharacterSelected;
	packet << s.playerIdx;
	packet << s.stageId << s.isStageSelected;
	packet << s.rngOffset;
}

std::unique_ptr<SlippiPlayerSelections> SlippiNetplayClient::readSelectionsFromPacket(sf::Packet &packet)
{
	auto s = std::make_unique<SlippiPlayerSelections>();

	packet >> s->characterId;
	packet >> s->characterColor;
	packet >> s->isCharacterSelected;

	packet >> s->playerIdx;

	packet >> s->stageId;
	packet >> s->isStageSelected;

	packet >> s->rngOffset;

	return std::move(s);
}

void SlippiNetplayClient::Send(sf::Packet &packet)
{
	enet_uint32 flags = ENET_PACKET_FLAG_RELIABLE;
	u8 channelId = 0;

	// Echo the packet to each remote peer.
	for (int i = 0; i < m_server.size(); i++)
	{
		MessageId mid = ((u8 *)packet.getData())[0];
		if (mid == NP_MSG_SLIPPI_PAD || mid == NP_MSG_SLIPPI_PAD_ACK || mid == NP_MSG_SLIPPI_CONN_READY || mid == NP_MSG_SLIPPI_GAME_READY)
		{
			// Slippi communications do not need reliable connection and do not need to
			// be received in order. Channel is changed so that other reliable communications
			// do not block anything. This may not be necessary if order is not maintained?
			flags = ENET_PACKET_FLAG_UNSEQUENCED;
			channelId = 1;
		}

		ENetPacket *epac = enet_packet_create(packet.getData(), packet.getDataSize(), flags);
		enet_peer_send(m_server[i], channelId, epac);
	}
}

void SlippiNetplayClient::Disconnect()
{
	ENetEvent netEvent;
	slippiConnectStatus = SlippiConnectStatus::NET_CONNECT_STATUS_DISCONNECTED;
	if (!m_server.empty())
		for (int i = 0; i < m_server.size(); i++)
		{
			enet_peer_disconnect(m_server[i], 0);
		}
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
			m_server.clear();
			return;
		default:
			break;
		}
	}
	// didn't disconnect gracefully force disconnect
	for (int i = 0; i < m_server.size(); i++)
	{
		enet_peer_reset(m_server[i]);
	}
	m_server.clear();
}

void SlippiNetplayClient::SendAsync(std::unique_ptr<sf::Packet> packet)
{
	{
		std::lock_guard<std::recursive_mutex> lkq(m_crit.async_queue_write);
		m_async_queue.Push(std::move(packet));
	}
	ENetUtil::WakeupThread(m_client);
}

// called from ---NETPLAY--- thread
void SlippiNetplayClient::ThreadFunc()
{
	// Let client die 1 second before host such that after a swap, the client won't be connected to
	int attemptCountLimit = 30;

	int attemptCount = 0;
	int connections = 0;
	while (slippiConnectStatus == SlippiConnectStatus::NET_CONNECT_STATUS_INITIATED)
	{
		// This will confirm that connection went through successfully
		ENetEvent netEvent;
		int net = enet_host_service(m_client, &netEvent, 500);
		if (net > 0 && netEvent.type == ENET_EVENT_TYPE_CONNECT)
		{
			// TODO: Confirm gecko codes match?
			if (netEvent.peer)
			{
				INFO_LOG(SLIPPI_ONLINE, "[Netplay] got event with peer port %d", netEvent.peer->address.port);
				for (int i = 0; i < m_server.size(); i++)
				{
					INFO_LOG(SLIPPI_ONLINE, "[Netplay] Comparing connection address: %s:%d - %s:%d",
					         m_server[i]->address.host, m_server[i]->address.port,
					         netEvent.peer->address.host, netEvent.peer->address.port);
					if (m_server[i]->address.host == netEvent.peer->address.host &&
					    m_server[i]->address.port == netEvent.peer->address.port)
					{
						INFO_LOG(SLIPPI_ONLINE, "[Netplay] Overwriting ENetPeer for address: %s:%d",
						         netEvent.peer->address.host, netEvent.peer->address.port);
						m_server[i] = netEvent.peer;
						connections++;
						break;
					}
				}
			}

			if (connections == 2)
			{
				m_client->intercept = ENetUtil::InterceptCallback;
				INFO_LOG(SLIPPI_ONLINE, "Slippi online connection successful!");
				break;
			}
		}

		for (int i = 0; i < 2; i++)
		{
			INFO_LOG(SLIPPI_ONLINE, "m_client peer %d state: %d", i, m_client->peers[i].state);
		}
		WARN_LOG(SLIPPI_ONLINE, "[Netplay] Not yet connected. Res: %d, Type: %d", net, netEvent.type);

		// Time out after enough time has passed
		attemptCount++;
		if (attemptCount >= attemptCountLimit || !m_do_loop.IsSet())
		{
			slippiConnectStatus = SlippiConnectStatus::NET_CONNECT_STATUS_FAILED;
			INFO_LOG(SLIPPI_ONLINE, "Slippi online connection failed");
			return;
		}
	}

	if (playerIdx == 1)
	{
		// Wait for acks, then send the start packet
		bool acks[3] = { false, false, false };
		bool sentStart[3] = {false, false, false};
		while (!sentStart[0] || !sentStart[1])
		{
			ENetEvent netEvent;
			int net;
			net = enet_host_service(m_client, &netEvent, 250);

			sf::Packet rpac;
			sf::Packet fwdPac;
			MessageId mid = 0;
			u8 pIdx = 0;
			switch (netEvent.type)
			{
			case ENET_EVENT_TYPE_RECEIVE:
				rpac.append(netEvent.packet->data, netEvent.packet->dataLength);
				rpac >> mid;
				rpac >> pIdx;
				INFO_LOG(SLIPPI_ONLINE, "Got ready waiting packet for player %d with mid %d", pIdx, mid);
				if (mid == NP_MSG_SLIPPI_CONN_READY)
				{
					acks[pIdx-2] = true;
				}
				else if (mid == NP_MSG_SLIPPI_CLIENT_READY)
				{
					sentStart[pIdx - 2] = true;
				}
				else
				{
					fwdPac.append(netEvent.packet->data, netEvent.packet->dataLength);
					OnData(fwdPac);

					enet_packet_destroy(netEvent.packet);
					break;
				}

				if (acks[0] && acks[1])
				{
					sf::Packet spac;
					spac << (MessageId)NP_MSG_SLIPPI_GAME_READY;
					Send(spac);
				}

				enet_packet_destroy(netEvent.packet);
				break;
			default:
				break;
			}
		}
	}
	else
	{
		// send the ack packet, then wait for start
		bool gameReady = false;
		while (!gameReady)
		{
			sf::Packet spac;
			spac << (MessageId)NP_MSG_SLIPPI_CONN_READY;
			spac << playerIdx;
			Send(spac);

			ENetEvent netEvent;
			int net;
			net = enet_host_service(m_client, &netEvent, 250);

			sf::Packet rpac;
			sf::Packet fwdPac;
			MessageId mid;
			switch (netEvent.type)
			{
			case ENET_EVENT_TYPE_RECEIVE:
				rpac.append(netEvent.packet->data, netEvent.packet->dataLength);
				rpac >> mid;
				if (mid == NP_MSG_SLIPPI_GAME_READY)
				{
					INFO_LOG(SLIPPI_ONLINE, "Got game ready packet");
					gameReady = true;

					sf::Packet spac;
					spac << (MessageId)NP_MSG_SLIPPI_CLIENT_READY;
					spac << playerIdx;
					Send(spac);
				}
				else
				{
					INFO_LOG(SLIPPI_ONLINE, "Got packet with mid %d while waiting for game ready", mid);
					fwdPac.append(netEvent.packet->data, netEvent.packet->dataLength);
					OnData(fwdPac);

					enet_packet_destroy(netEvent.packet);
					break;
				}

				enet_packet_destroy(netEvent.packet);
				break;
			default:
				break;
			}
		}
	}

	slippiConnectStatus = SlippiConnectStatus::NET_CONNECT_STATUS_CONNECTED;

	INFO_LOG(SLIPPI_ONLINE, "Successfully initialized %d connections", m_server.size());
	for (int i = 0; i < m_server.size(); i++)
	{
		INFO_LOG(SLIPPI_ONLINE, "Connection %d: %d, %d", i, m_server[i]->address.host, m_server[i]->address.port);
	}

	bool qos_success = false;
#ifdef _WIN32
	QOS_VERSION ver = {1, 0};

	if (SConfig::GetInstance().bQoSEnabled && QOSCreateHandle(&ver, &m_qos_handle))
	{
		for (int i = 0; i < m_server.size(); i++)
		{
			// from win32.c
			struct sockaddr_in sin = {0};

			sin.sin_family = AF_INET;
			sin.sin_port = ENET_HOST_TO_NET_16(m_server[i]->host->address.port);
			sin.sin_addr.s_addr = m_server[i]->host->address.host;

			if (QOSAddSocketToFlow(m_qos_handle, m_server[i]->host->socket, reinterpret_cast<PSOCKADDR>(&sin),
			    // this is 0x38
			    QOSTrafficTypeControl, QOS_NON_ADAPTIVE_FLOW, &m_qos_flow_id))
			{
				DWORD dscp = 0x2e;

				// this will fail if we're not admin
				// sets DSCP to the same as linux (0x2e)
				QOSSetFlow(m_qos_handle, m_qos_flow_id, QOSSetOutgoingDSCPValue, sizeof(DWORD), &dscp, 0, nullptr);

				qos_success = true;
			}
		}
	}
#else
	if (SConfig::GetInstance().bQoSEnabled)
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

	while (m_do_loop.IsSet())
	{
		ENetEvent netEvent;
		int net;
		net = enet_host_service(m_client, &netEvent, 250);
		while (!m_async_queue.Empty())
		{
			Send(*(m_async_queue.Front().get()));
			m_async_queue.Pop();
		}
		if (net > 0)
		{
			sf::Packet rpac;
			bool sameClient = false;
			switch (netEvent.type)
			{
			case ENET_EVENT_TYPE_RECEIVE:
				rpac.append(netEvent.packet->data, netEvent.packet->dataLength);
				OnData(rpac);

				enet_packet_destroy(netEvent.packet);
				break;
			case ENET_EVENT_TYPE_DISCONNECT:
				for (int i = 0; i < m_server.size(); i++)
				{
					if (netEvent.peer == m_server[i])
					{
						sameClient = true;
					}
				}
				ERROR_LOG(SLIPPI_ONLINE, "[Netplay] Disconnected Event detected: %s",
				          sameClient ? "same client" : "diff client");

				// If the disconnect event doesn't come from the client we are actually listening to,
				// it can be safely ignored
				if (sameClient)
				{
					m_do_loop.Clear(); // Stop the loop, will trigger a disconnect
				}
				break;
			default:
				break;
			}
		}
	}

#ifdef _WIN32
	if (m_qos_handle != 0)
	{
		if (m_qos_flow_id != 0)
		{
			for (int i = 0; i < m_server.size(); i++)
			{
				QOSRemoveSocketFromFlow(m_qos_handle, m_server[i]->host->socket, m_qos_flow_id, 0);
			}
		}

		QOSCloseHandle(m_qos_handle);
	}
#endif

	Disconnect();
	return;
}

bool SlippiNetplayClient::IsDecider()
{
	return isDecider;
}

bool SlippiNetplayClient::IsConnectionSelected()
{
	return isConnectionSelected;
}

SlippiNetplayClient::SlippiConnectStatus SlippiNetplayClient::GetSlippiConnectStatus()
{
	return slippiConnectStatus;
}

void SlippiNetplayClient::StartSlippiGame()
{
	// Reset variables to start a new game
	lastFrameAcked = 0;

	FrameTiming timing;
	timing.frame = 0;
	timing.timeUs = Common::Timer::GetTimeUs();
	lastFrameTiming = timing;
	hasGameStarted = false;

	localPadQueue.clear();

	remotePadQueue.clear();
	for (s32 i = 1; i <= 2; i++)
	{
		std::unique_ptr<SlippiPad> pad = std::make_unique<SlippiPad>(i);
		remotePadQueue.push_front(std::move(pad));
	}

	// Reset match info for next game
	matchInfo.Reset();

	// Reset ack timers
	ackTimers.Clear();
}

void SlippiNetplayClient::SendConnectionSelected()
{
	isConnectionSelected = true;

	auto spac = std::make_unique<sf::Packet>();
	*spac << static_cast<MessageId>(NP_MSG_SLIPPI_CONN_SELECTED);
	SendAsync(std::move(spac));
}

void SlippiNetplayClient::SendSlippiPad(std::unique_ptr<SlippiPad> pad)
{
	auto status = slippiConnectStatus;
	bool connectionFailed = status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_FAILED;
	bool connectionDisconnected = status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_DISCONNECTED;
	if (connectionFailed || connectionDisconnected)
	{
		return;
	}

	// if (pad && isDecider)
	//{
	//  ERROR_LOG(SLIPPI_ONLINE, "[%d] %X %X %X %X %X %X %X %X", pad->frame, pad->padBuf[0], pad->padBuf[1],
	//  pad->padBuf[2], pad->padBuf[3], pad->padBuf[4], pad->padBuf[5], pad->padBuf[6], pad->padBuf[7]);
	//}

	if (pad)
	{
		// Add latest local pad report to queue
		localPadQueue.push_front(std::move(pad));
	}

	// Remove pad reports that have been received and acked
	while (!localPadQueue.empty() && localPadQueue.back()->frame < lastFrameAcked)
	{
		localPadQueue.pop_back();
	}

	if (localPadQueue.empty())
	{
		// If pad queue is empty now, there's no reason to send anything
		return;
	}

	auto frame = localPadQueue.front()->frame;

	auto spac = std::make_unique<sf::Packet>();
	*spac << static_cast<MessageId>(NP_MSG_SLIPPI_PAD);
	*spac << frame;
	*spac << this->playerIdx;

	INFO_LOG(SLIPPI_ONLINE, "Sending a packet of inputs [%d]...", frame);
	for (auto it = localPadQueue.begin(); it != localPadQueue.end(); ++it)
	{
		INFO_LOG(SLIPPI_ONLINE, "Send [%d] -> %02X %02X %02X %02X %02X %02X %02X %02X", (*it)->frame, (*it)->padBuf[0],
		         (*it)->padBuf[1], (*it)->padBuf[2], (*it)->padBuf[3], (*it)->padBuf[4], (*it)->padBuf[5],
		         (*it)->padBuf[6], (*it)->padBuf[7]);
		spac->append((*it)->padBuf, SLIPPI_PAD_DATA_SIZE); // only transfer 8 bytes per pad
	}

	SendAsync(std::move(spac));

	u64 time = Common::Timer::GetTimeUs();

	hasGameStarted = true;

	FrameTiming timing;
	timing.frame = frame;
	timing.timeUs = time;
	lastFrameTiming = timing;

	// Add send time to ack timers
	FrameTiming sendTime;
	sendTime.frame = frame;
	sendTime.timeUs = time;
	ackTimers.Push(sendTime);
}

void SlippiNetplayClient::SetMatchSelections(SlippiPlayerSelections &s)
{
	matchInfo.localPlayerSelections.Merge(s);
	matchInfo.localPlayerSelections.playerIdx = playerIdx;

	// Send packet containing selections
	auto spac = std::make_unique<sf::Packet>();
	writeToPacket(*spac, matchInfo.localPlayerSelections);
	SendAsync(std::move(spac));
}

std::unique_ptr<SlippiRemotePadOutput> SlippiNetplayClient::GetSlippiRemotePad(int32_t curFrame)
{
	std::lock_guard<std::mutex> lk(pad_mutex); // TODO: Is this the correct lock?

	std::unique_ptr<SlippiRemotePadOutput> padOutput = std::make_unique<SlippiRemotePadOutput>();

	if (remotePadQueue.empty())
	{
		auto emptyPad = std::make_unique<SlippiPad>(0);

		padOutput->latestFrame = emptyPad->frame;

		auto emptyIt = std::begin(emptyPad->padBuf);
		padOutput->data.insert(padOutput->data.end(), emptyIt, emptyIt + SLIPPI_PAD_FULL_SIZE);

		return std::move(padOutput);
	}

	padOutput->latestFrame = remotePadQueue.front()->frame;

	// Copy the entire remaining remote buffer
	for (auto it = remotePadQueue.begin(); it != remotePadQueue.end(); ++it)
	{
		auto padIt = std::begin((*it)->padBuf);
		padOutput->data.insert(padOutput->data.end(), padIt, padIt + SLIPPI_PAD_FULL_SIZE);
	}

	// Remove pad reports that should no longer be needed
	while (remotePadQueue.size() > 1 && remotePadQueue.back()->frame < curFrame)
	{
		remotePadQueue.pop_back();
	}

	return std::move(padOutput);
}

SlippiMatchInfo *SlippiNetplayClient::GetMatchInfo()
{
	return &matchInfo;
}

u64 SlippiNetplayClient::GetSlippiPing()
{
	return pingUs;
}

int32_t SlippiNetplayClient::GetSlippiLatestRemoteFrame()
{
	std::lock_guard<std::mutex> lk(pad_mutex); // TODO: Is this the correct lock?

	if (remotePadQueue.empty())
	{
		return 0;
	}

	return remotePadQueue.front()->frame;
}

s32 SlippiNetplayClient::CalcTimeOffsetUs()
{
	if (frameOffsetData.buf.empty())
	{
		return 0;
	}

	std::vector<s32> buf;
	std::copy(frameOffsetData.buf.begin(), frameOffsetData.buf.end(), std::back_inserter(buf));

	// TODO: Does this work?
	std::sort(buf.begin(), buf.end());

	int bufSize = (int)buf.size();
	int offset = (int)((1.0f / 3.0f) * bufSize);
	int end = bufSize - offset;

	int sum = 0;
	for (int i = offset; i < end; i++)
	{
		sum += buf[i];
	}

	int count = end - offset;
	if (count <= 0)
	{
		return 0; // What do I return here?
	}

	return sum / count;
}
