#pragma once

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <enet/enet.h>
#include <json.hpp>

#include "Common/CommonTypes.h"
#include "Core/Slippi/SlippiExiTypes.h"
#include "Core/Slippi/SlippiRoom.h"
#include "Core/Slippi/SlippiRoomDirectory.h"

// Being in a room, either as the host running it or as a member who joined it. Members connect
// straight to the host, which sends everyone the room whenever it changes and applies what each
// member does. When the host leaves or is lost, the member who has been in the room longest takes
// over and the others reconnect to them through the room's code. Matches and spectating connect
// separately.
//
// All networking runs on threads the game never waits for, so leaving a room or changing hosts
// never stalls emulation. Each session shares its state with its threads, which clean up on their
// own once the session is gone
class SlippiRoomSession
{
  public:
	enum ConnectionStatus : u8
	{
		STATUS_HOSTING = 0,
		STATUS_JOINING = 1,
		STATUS_JOINED = 2,
		STATUS_FAILED = 3,
		STATUS_RECONNECTING = 4, // The host changed and the room is being rejoined
	};

	enum ConnectionError : u8
	{
		CONNECT_OK = 0,
		CONNECT_UNAVAILABLE = 1, // The rooms directory couldn't be reached
		CONNECT_NOT_FOUND = 2,
		CONNECT_WRONG_PASSWORD = 3,
		CONNECT_FULL = 4,
		CONNECT_LOCKED = 5, // Too many wrong codes or passwords
		CONNECT_UNREACHABLE = 6,
		CONNECT_DISCONNECTED = 7,
		CONNECT_REJECTED = 8,
		CONNECT_IDLE = 9, // The room closed after an hour without activity
	};

	static const int PROTOCOL_VERSION = 2;

	struct Identity
	{
		std::string name;
		std::string connectCode;
	};

	virtual ~SlippiRoomSession() = default;

	virtual void HandleLocalAction(u8 action, u8 value0, u8 value1) = 0;
	virtual void ReportMatchResult(SlippiRoom::MatchResult result) = 0;
	virtual SlippiExiTypes::GetRoomStateResponse GetState() = 0;
	virtual void AddTestPlayer() {}

	// Watching: where this player's match can be reached, and where this watcher watches from
	virtual void SetMatchAddress(const std::string &address) = 0;
	virtual void SetWatchAddress(const std::string &address) = 0;

	// The room as this session sees it
	virtual SlippiRoom CopyRoom() = 0;

	// Leaving on purpose. The session can be destroyed right after
	virtual void Leave() = 0;

	// Once this session has changed role, such as a member taking over as host, returns the session
	// that replaces it
	virtual std::unique_ptr<SlippiRoomSession> TakeNext() = 0;

  protected:
	// How a member finds the room again after its host changed. The first candidate takes over, and
	// the others wait their turn in order in case the ones before them are gone too
	struct Reconnect
	{
		SlippiRoom room; // The last copy, shown while reconnecting
		std::vector<std::string> candidates;
		SlippiRoom::HostChange change = SlippiRoom::HOST_LOST;
		int knownGeneration = 0;
	};

	static void sendMessage(ENetPeer *peer, const nlohmann::json &msg);
};

class SlippiRoomHost : public SlippiRoomSession
{
  public:
	// Creates a new room
	SlippiRoomHost(const Identity &identity, const SlippiExiTypes::CreateRoomQuery &query);

	// Takes a room over, starting from this member's copy of it
	SlippiRoomHost(const Identity &identity, const SlippiRoom &copy, SlippiRoom::HostChange change);

	~SlippiRoomHost();

	void HandleLocalAction(u8 action, u8 value0, u8 value1) override;
	void ReportMatchResult(SlippiRoom::MatchResult result) override;
	SlippiExiTypes::GetRoomStateResponse GetState() override;
	void AddTestPlayer() override;
	void SetMatchAddress(const std::string &address) override;
	void SetWatchAddress(const std::string &address) override;
	SlippiRoom CopyRoom() override;
	void Leave() override;
	std::unique_ptr<SlippiRoomSession> TakeNext() override;

  private:
	struct Shared
	{
		std::mutex lock;
		std::atomic<bool> running{true};
		std::atomic<bool> leaving{false};
		SlippiRoom room;
		ConnectionStatus status = STATUS_HOSTING;
		ConnectionError error = CONNECT_OK;

		// Set when this host steps down to a member, such as when handing the room back
		std::unique_ptr<Reconnect> next;

		// Directory
		SlippiRoomDirectory::Registration reg;
		bool registered = false;
		bool unavailable = false;
		bool activityWanted = false;
		u8 memberCount = 1;
		bool closeRoom = false;
		bool replaced = false; // Another member took the room over
		bool gone = false;     // The directory no longer has the room

		// Where members connect: the room's port on the router, as a STUN server saw it. 0 when none
		// answered, and then only a port UPnP opened lets anyone in
		u16 publicPort = 0;
		bool isPortReady = false;
		bool isPortChanged = false;
		std::deque<std::string> punches; // Joiners to punch through to
		std::atomic<bool> netDone{false};
	};

	class NetThread;

	void start(SlippiRoomDirectory::RoomInfo info, bool isTakeOver);
	static void runNetThread(std::shared_ptr<Shared> shared, Identity identity, ENetHost *host, bool tookOver);
	static void directoryThread(std::shared_ptr<Shared> shared, SlippiRoomDirectory::RoomInfo info,
	                            std::string takeOverCode, int takeOverGeneration);

	Identity m_identity;
	std::shared_ptr<Shared> m_shared;
};

class SlippiRoomMember : public SlippiRoomSession
{
  public:
	// Joins a room by its code
	SlippiRoomMember(const Identity &identity, const SlippiExiTypes::JoinRoomQuery &query);

	// Finds a room again after its host changed
	SlippiRoomMember(const Identity &identity, const Reconnect &reconnect);

	~SlippiRoomMember();

	void HandleLocalAction(u8 action, u8 value0, u8 value1) override;
	void ReportMatchResult(SlippiRoom::MatchResult result) override;
	SlippiExiTypes::GetRoomStateResponse GetState() override;
	void SetMatchAddress(const std::string &address) override;
	void SetWatchAddress(const std::string &address) override;
	SlippiRoom CopyRoom() override;
	void Leave() override;
	std::unique_ptr<SlippiRoomSession> TakeNext() override;

  private:
	struct Shared
	{
		std::mutex lock;
		std::atomic<bool> running{true};
		std::atomic<bool> leaving{false};
		std::string code;
		std::string password;
		SlippiRoom room;
		int localMember = 0;
		ConnectionStatus status = STATUS_JOINING;
		ConnectionError error = CONNECT_OK;
		std::deque<nlohmann::json> outgoing;

		// Set when this member takes over as host
		bool promote = false;
		SlippiRoom::HostChange promoteChange = SlippiRoom::HOST_LOST;
	};

	class NetThread;

	static void runNetThread(std::shared_ptr<Shared> shared, Identity identity, u8 lastChar, u8 lastColor,
	                         std::shared_ptr<Reconnect> reconnect);

	Identity m_identity;
	std::shared_ptr<Shared> m_shared;
};
