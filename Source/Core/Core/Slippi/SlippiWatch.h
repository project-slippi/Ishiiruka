#pragma once

#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <enet/enet.h>

#include "Common/CommonTypes.h"
#include "Core/Slippi/SlippiPad.h"

// Watching a room's match as a delay-based peer. The two players are connected to each other by
// Slippi's servers; a watcher connects to both of them as an extra peer, collects their pads into
// one timeline and hands Melee whole frames out of it.
//
// Both of them, since each client only sends its own pads and selections. And it never predicts:
// Melee is only given a frame once both players' inputs for it are in hand, so there is nothing to
// roll back. A late network pauses the picture, and a watcher costs the players nothing because it
// sends no acks
class SlippiWatchClient
{
  public:
	// Both players' addresses
	SlippiWatchClient(const std::vector<std::string> &addrs, const std::vector<u16> &ports);
	~SlippiWatchClient();

	SlippiWatchClient(const SlippiWatchClient &) = delete;
	void operator=(const SlippiWatchClient &) = delete;

	enum class Status
	{
		CONNECTING,
		WATCHING,
		FAILED,
		OVER, // The players finished, or went away
	};

	Status GetStatus() const { return m_status.load(std::memory_order_acquire); }

	// Where a STUN server saw this watcher's socket as "host:port", or empty until it answers. The
	// players punch toward it, since their routers drop a first packet from a stranger
	std::string PublicAddress() const;

	// The newest frame held for every player with no holes behind it, which is how far Melee may
	// safely be advanced
	s32 LatestFrame() const { return m_contiguous.load(std::memory_order_acquire); }

	// One player's inputs for one frame. False when it isn't held, and then Melee must not advance
	bool GetPad(s32 frame, u8 playerIdx, u8 *out) const;

	// What the two of them picked, as they told us
	struct Picks
	{
		bool known = false;
		u8 character[2] = {0, 0};
		u8 color[2] = {0, 0};
		u16 stage = 0;
		u32 seed = 0;

		// Which of the dialed addresses each player answered on. Only the connection knows which of
		// them Slippi made player 0
		u8 slot[2] = {0, 1};
	};
	Picks GetPicks() const;

	// Enough to start the match: both players' picks, and at least one frame from both
	bool Ready() const;

	// The watcher is port 2, which isn't in the match, so both players come in through Melee's
	// remote input path the same way and the watcher's own controller moves nothing
	static const u8 WATCHER_PORT = 2;

  private:
	void ThreadFunc();
	void OnPacket(const u8 *data, size_t len, ENetPeer *from);
	void AskForMissing(s32 from);
	void Advance();

	ENetHost *m_host = nullptr;
	std::vector<std::pair<ENetAddress, u8>> m_targets; // Each player's address and slot
	std::vector<ENetPeer *> m_players;
	size_t m_needed = 0;
	u64 m_dialedAtUs = 0;
	std::thread m_thread;
	std::atomic<bool> m_run{true};
	std::atomic<Status> m_status{Status::CONNECTING};

	// Indexed by frame minus Slippi::GAME_FIRST_FRAME, which is -123 rather than 0. Per player,
	// since they arrive from two places and one can be ahead
	struct Frame
	{
		std::array<u8, SLIPPI_PAD_DATA_SIZE> pad{};
		bool have = false;
	};

	mutable std::mutex m_lock;
	std::string m_publicAddr;
	std::vector<Frame> m_line[2];
	std::atomic<s32> m_contiguous{0};
	bool m_baselined = false; // Found where the timeline starts
	s32 m_heard[2] = {0, 0};  // Newest frame seen from each, holes and all
	Picks m_picks;

	// Each player's stage choice, resolved in port order the way the players resolve it
	u16 m_stageOf[2] = {0, 0};
	bool m_stageSet[2] = {false, false};
	bool m_toldPicks[2] = {false, false};

	// Which player slot each peer was dialed for
	std::vector<std::pair<ENetPeer *, u8>> m_peerSlot;

	// The newest frame both players had filled in, and when it last moved
	s32 m_progressFrame = 0;
	u64 m_progressAtUs = 0;

	bool m_isEnetInitialized = false;
};
