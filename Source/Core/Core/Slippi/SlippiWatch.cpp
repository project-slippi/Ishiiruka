#include "Core/Slippi/SlippiWatch.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <SFML/Network/Packet.hpp>
#include <SlippiLib/SlippiGame.h>

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Timer.h"
#include "Core/NetPlayProto.h"
#include "Core/Slippi/SlippiNetplay.h"
#include "Core/Slippi/SlippiStun.h"

// #define LOCAL_TESTING

namespace
{
// A match is a few minutes. Reserving twelve up front costs about 300KB and means the timeline
// never moves under a reader
const size_t FRAMES_RESERVED = 60 * 60 * 12;

// A watch that can't reach both players in this long gives up and says so
const u64 CONNECT_TIMEOUT_US = 20000000;

// Missing frames are asked for again after this long without any arriving
const u64 STALL_US = 1000000;

inline size_t slotFor(s32 frame)
{
	return static_cast<size_t>(frame - Slippi::GAME_FIRST_FRAME);
}
} // namespace

SlippiWatchClient::SlippiWatchClient(const std::vector<std::string> &addrs, const std::vector<u16> &ports)
{
	m_needed = std::min<size_t>(addrs.size(), 2);

	for (int i = 0; i < 2; i++)
		m_line[i].resize(FRAMES_RESERVED);
	m_contiguous.store(Slippi::GAME_FIRST_FRAME - 1, std::memory_order_release);
	m_heard[0] = m_heard[1] = Slippi::GAME_FIRST_FRAME - 1;

	if (enet_initialize() != 0)
	{
		ERROR_LOG(SLIPPI_ONLINE, "[Watch] Could not start ENet");
		m_status.store(Status::FAILED, std::memory_order_release);
		return;
	}
	m_isEnetInitialized = true;

	// Any port. STUN reports whatever the world sees, so it only has to be discoverable
	ENetAddress local;
	local.host = ENET_HOST_ANY;
	local.port = 0;

	m_host = enet_host_create(&local, 4, 3, 0, 0);
	if (!m_host)
	{
		ERROR_LOG(SLIPPI_ONLINE, "[Watch] Could not open a socket");
		m_status.store(Status::FAILED, std::memory_order_release);
		return;
	}

	m_host->intercept = SlippiStun::Intercept;

	for (size_t i = 0; i < addrs.size() && i < 2; i++)
	{
		std::string host = addrs[i];
#ifdef LOCAL_TESTING
		// Test clients on one computer can't reach each other through their shared public address
		host = "127.0.0.1";
#endif

		ENetAddress addr;
		if (enet_address_set_host(&addr, host.c_str()) != 0)
		{
			ERROR_LOG(SLIPPI_ONLINE, "[Watch] Could not read the address %s", host.c_str());
			continue;
		}
		addr.port = ports[i];
		m_targets.emplace_back(addr, static_cast<u8>(i));
	}

	if (m_targets.empty())
	{
		m_status.store(Status::FAILED, std::memory_order_release);
		return;
	}

	m_thread = std::thread(&SlippiWatchClient::ThreadFunc, this);
}

SlippiWatchClient::~SlippiWatchClient()
{
	m_run.store(false, std::memory_order_release);
	if (m_thread.joinable())
		m_thread.join();

	if (m_host)
	{
		for (auto *peer : m_players)
			enet_peer_disconnect(peer, 0);

		// Give the goodbyes a moment to go out
		ENetEvent ev;
		while (enet_host_service(m_host, &ev, 250) > 0)
		{
			if (ev.type == ENET_EVENT_TYPE_RECEIVE)
				enet_packet_destroy(ev.packet);
		}

		for (auto *peer : m_players)
			enet_peer_reset(peer);
		SlippiStun::Forget(m_host);
		enet_host_destroy(m_host);
		m_host = nullptr;
	}

	if (m_isEnetInitialized)
		enet_deinitialize();
}

std::string SlippiWatchClient::PublicAddress() const
{
	std::lock_guard<std::mutex> lk(m_lock);
	return m_publicAddr;
}

void SlippiWatchClient::ThreadFunc()
{
	Common::SetCurrentThreadName("Slippi watch");

	// Found before dialing, since finding it services the socket. The players punch toward it, and the
	// dial keeps retrying until their punch lands. Not finding it isn't fatal: without a router in the
	// way nothing is needed
	std::string publicIp;
	u16 publicPort = 0;
	if (SlippiStun::Discover(m_host, publicIp, publicPort))
	{
		std::string publicAddr = StringFromFormat("%s:%d", publicIp.c_str(), publicPort);
		INFO_LOG(SLIPPI_ONLINE, "[Watch] This end is %s from outside", publicAddr.c_str());
		std::lock_guard<std::mutex> lk(m_lock);
		m_publicAddr = publicAddr;
	}
	else
	{
		WARN_LOG(SLIPPI_ONLINE, "[Watch] STUN did not answer, the players can't punch through to us");
	}

	// The connect data marks us as a watcher rather than a player
	for (const auto &target : m_targets)
	{
		ENetPeer *peer = enet_host_connect(m_host, &target.first, 3, SLIPPI_CONNECT_SPECTATOR);
		if (!peer)
			continue;

		m_players.push_back(peer);
		m_peerSlot.emplace_back(peer, target.second);
		INFO_LOG(SLIPPI_ONLINE, "[Watch] Asking to watch player %d", static_cast<int>(target.second));
	}
	if (m_players.empty())
	{
		m_status.store(Status::FAILED, std::memory_order_release);
		return;
	}
	m_dialedAtUs = Common::Timer::GetTimeUs();

	size_t connected = 0;
	u64 lastWaitLogUs = 0;

	// Peers that actually answered. A dial that never answers also ends in a disconnect event, which
	// must not be taken for a player leaving
	std::vector<ENetPeer *> live;

	while (m_run.load(std::memory_order_acquire))
	{
		ENetEvent ev;
		int got = enet_host_service(m_host, &ev, 100);
		if (got < 0)
		{
			ERROR_LOG(SLIPPI_ONLINE, "[Watch] The socket failed");
			m_status.store(Status::FAILED, std::memory_order_release);
			return;
		}

		if (got > 0)
		{
			switch (ev.type)
			{
			case ENET_EVENT_TYPE_CONNECT:
			{
				connected++;
				live.push_back(ev.peer);
				INFO_LOG(SLIPPI_ONLINE, "[Watch] Connected to %d of %d", static_cast<int>(connected),
				         static_cast<int>(m_needed));

				// Everything from the start. Nothing for a match joined at the beginning, the whole thing so
				// far for one already under way
				sf::Packet ask;
				ask << static_cast<MessageId>(NP_MSG_SLIPPI_WATCH_FROM);
				ask << static_cast<s32>(Slippi::GAME_FIRST_FRAME);
				ENetPacket *epac = enet_packet_create(ask.getData(), ask.getDataSize(), ENET_PACKET_FLAG_RELIABLE);
				enet_peer_send(ev.peer, 0, epac);
				m_progressAtUs = Common::Timer::GetTimeUs();

				if (connected >= m_needed)
					m_status.store(Status::WATCHING, std::memory_order_release);
				break;
			}
			case ENET_EVENT_TYPE_RECEIVE:
				OnPacket(ev.packet->data, ev.packet->dataLength, ev.peer);
				enet_packet_destroy(ev.packet);
				break;

			case ENET_EVENT_TYPE_DISCONNECT:
				// A dial that never answered, giving up
				if (std::find(live.begin(), live.end(), ev.peer) == live.end())
				{
					m_players.erase(std::remove(m_players.begin(), m_players.end(), ev.peer), m_players.end());
					break;
				}

				// One of them going ends it, since half a match can't be simulated
				INFO_LOG(SLIPPI_ONLINE, "[Watch] A player went away, the watch is over");
				m_status.store(Status::OVER, std::memory_order_release);
				return;

			default:
				break;
			}
		}

		// A watch that never connects says so rather than waiting forever. Unreachable players are a
		// normal outcome behind a strict router
		if (m_status.load(std::memory_order_acquire) == Status::CONNECTING)
		{
			u64 waited = Common::Timer::GetTimeUs() - m_dialedAtUs;
			if (waited > CONNECT_TIMEOUT_US)
			{
				ERROR_LOG(SLIPPI_ONLINE, "[Watch] Gave up, reached %d of %d players", static_cast<int>(connected),
				          static_cast<int>(m_needed));
				m_status.store(Status::FAILED, std::memory_order_release);
				return;
			}
			if (waited - lastWaitLogUs > 5000000)
			{
				lastWaitLogUs = waited;
				INFO_LOG(SLIPPI_ONLINE, "[Watch] Still waiting, %d of %d answered", static_cast<int>(connected),
				         static_cast<int>(m_needed));
			}
		}

		// Newer frames in hand but none filling the gap for a while is a hole that won't fill itself,
		// since nothing resends the live stream. Asking sooner would repeat a burst still arriving
		if (m_status.load(std::memory_order_acquire) == Status::WATCHING)
		{
			s32 contiguous = m_contiguous.load(std::memory_order_acquire);
			s32 behind;
			{
				std::lock_guard<std::mutex> lk(m_lock);
				behind = std::max(m_heard[0], m_heard[1]) - contiguous;
			}

			u64 now = Common::Timer::GetTimeUs();
			if (contiguous != m_progressFrame)
			{
				m_progressFrame = contiguous;
				m_progressAtUs = now;
			}
			else if (behind > 0 && now - m_progressAtUs > STALL_US)
			{
				AskForMissing(contiguous + 1);
				m_progressAtUs = now;
			}
		}
	}
}

// Asks both players for everything from a frame on
void SlippiWatchClient::AskForMissing(s32 from)
{
	sf::Packet ask;
	ask << static_cast<MessageId>(NP_MSG_SLIPPI_WATCH_FROM);
	ask << from;

	for (auto *peer : m_players)
	{
		ENetPacket *epac = enet_packet_create(ask.getData(), ask.getDataSize(), ENET_PACKET_FLAG_RELIABLE);
		enet_peer_send(peer, 0, epac);
	}
	INFO_LOG(SLIPPI_ONLINE, "[Watch] Asking for frame %d onwards", from);
}

void SlippiWatchClient::OnPacket(const u8 *data, size_t len, ENetPeer *from)
{
	if (len < 1)
		return;

	sf::Packet packet;
	packet.append(data, len);

	MessageId mid = 0;
	if (!(packet >> mid))
		return;

	switch (mid)
	{
	case NP_MSG_SLIPPI_MATCH_SELECTIONS:
	{
		// In writeToPacket's order, read in full so the stream stays aligned
		u8 characterId = 0, characterColor = 0, playerIdx = 0, teamId = 0, altStage = 0;
		bool isCharacterSelected = false, isStageSelected = false;
		u16 stageId = 0;
		u32 rngOffset = 0;

		if (!(packet >> characterId >> characterColor >> isCharacterSelected))
			return;
		if (!(packet >> playerIdx))
			return;
		if (!(packet >> stageId >> isStageSelected))
			return;
		if (!(packet >> rngOffset))
			return;
		if (!(packet >> teamId))
			return;
		if (!(packet >> altStage))
			return;

		if (playerIdx > 1)
			return;

		std::lock_guard<std::mutex> lk(m_lock);
		if (isCharacterSelected)
		{
			m_picks.character[playerIdx] = characterId;
			m_picks.color[playerIdx] = characterColor;
			m_toldPicks[playerIdx] = true;

			for (const auto &ps : m_peerSlot)
			{
				if (ps.first == from)
				{
					m_picks.slot[playerIdx] = ps.second;
					break;
				}
			}
		}

		// The first player in port order who chose a stage, the way the players resolve it
		if (isStageSelected)
		{
			m_stageOf[playerIdx] = stageId;
			m_stageSet[playerIdx] = true;
			m_picks.stage = m_stageSet[0] ? m_stageOf[0] : m_stageOf[1];
		}

		// Player 0's seed only. Both generate one, but the match runs on the decider's, who is player 0
		if (playerIdx == 0 && rngOffset)
			m_picks.seed = rngOffset;
		m_picks.known = m_toldPicks[0] && m_toldPicks[1];

		INFO_LOG(SLIPPI_ONLINE, "[Watch] Player %d is %d (color %d), stage %d, seed %08x%s", playerIdx, characterId,
		         characterColor, stageId, rngOffset, isCharacterSelected ? "" : ", not chosen yet");
		break;
	}
	case NP_MSG_SLIPPI_PAD:
	{
		// The header frame is the newest in the packet and the pads run backwards from it
		s32 newest;
		u8 playerIdx;
		s32 checksumFrame;
		u32 checksum;
		if (!(packet >> newest >> playerIdx >> checksumFrame >> checksum))
			return;
		if (playerIdx > 1)
			return;

		const size_t HEADER_SIZE = 14;
		if (len <= HEADER_SIZE)
			return;
		const size_t count = (len - HEADER_SIZE) / SLIPPI_PAD_DATA_SIZE;

		std::lock_guard<std::mutex> lk(m_lock);
		for (size_t i = 0; i < count; i++)
		{
			s32 frame = newest - static_cast<s32>(i);
			if (frame < Slippi::GAME_FIRST_FRAME)
				break;
			size_t slot = slotFor(frame);
			if (slot >= m_line[playerIdx].size())
				continue;

			// A frame that arrives twice, live and in a catch-up burst, is the same frame
			if (m_line[playerIdx][slot].have)
				continue;
			memcpy(m_line[playerIdx][slot].pad.data(), data + HEADER_SIZE + i * SLIPPI_PAD_DATA_SIZE,
			       SLIPPI_PAD_DATA_SIZE);
			m_line[playerIdx][slot].have = true;
		}

		if (newest > m_heard[playerIdx])
			m_heard[playerIdx] = newest;

		Advance();
		break;
	}
	default:
		break;
	}
}

// Moves the timeline up to the newest frame held for both players with nothing missing behind it.
// Called with m_lock held
void SlippiWatchClient::Advance()
{
	const s32 ceiling = std::min(m_heard[0], m_heard[1]);

	// Where the timeline starts is found rather than assumed: players only keep what they've sent,
	// so a match under way may not reach back to the first frame
	if (!m_baselined && m_heard[0] > Slippi::GAME_FIRST_FRAME - 1 && m_heard[1] > Slippi::GAME_FIRST_FRAME - 1)
	{
		for (s32 f = Slippi::GAME_FIRST_FRAME; f <= ceiling; f++)
		{
			size_t slot = slotFor(f);
			if (slot >= m_line[0].size())
				break;
			if (m_line[0][slot].have && m_line[1][slot].have)
			{
				m_contiguous.store(f - 1, std::memory_order_release);
				m_baselined = true;
				INFO_LOG(SLIPPI_ONLINE, "[Watch] The timeline starts at frame %d", f);
				break;
			}
		}
		if (!m_baselined)
			return;
	}

	s32 at = m_contiguous.load(std::memory_order_relaxed);
	while (at < ceiling)
	{
		size_t slot = slotFor(at + 1);
		if (slot >= m_line[0].size())
			break;
		if (!m_line[0][slot].have || !m_line[1][slot].have)
			break;
		at++;
	}

	m_contiguous.store(at, std::memory_order_release);
}

bool SlippiWatchClient::GetPad(s32 frame, u8 playerIdx, u8 *out) const
{
	if (playerIdx > 1 || frame < Slippi::GAME_FIRST_FRAME)
		return false;

	std::lock_guard<std::mutex> lk(m_lock);
	size_t slot = slotFor(frame);
	if (slot >= m_line[playerIdx].size() || !m_line[playerIdx][slot].have)
		return false;

	memcpy(out, m_line[playerIdx][slot].pad.data(), SLIPPI_PAD_DATA_SIZE);
	return true;
}

bool SlippiWatchClient::Ready() const
{
	std::lock_guard<std::mutex> lk(m_lock);
	return m_picks.known && m_baselined;
}

SlippiWatchClient::Picks SlippiWatchClient::GetPicks() const
{
	std::lock_guard<std::mutex> lk(m_lock);
	return m_picks;
}
