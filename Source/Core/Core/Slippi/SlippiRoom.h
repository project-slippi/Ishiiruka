#pragma once

#include <random>
#include <string>
#include <vector>

#include <json.hpp>

#include "Common/CommonTypes.h"
#include "Core/Slippi/SlippiExiTypes.h"

// A room of players taking turns on one setup. Winner stays on, the loser goes to the back of the
// queue, and beating everyone in the queue earns a crown.
//
// The host's copy runs the room. Every member keeps a copy of the host's, read from what the host
// sends, which is only shown until the member takes over as host
class SlippiRoom
{
  public:
	enum Side : s8
	{
		SIDE_NONE = -1,
		SIDE_WINNER = 0,
		SIDE_CHALLENGER = 1,
	};

	enum Phase : u8
	{
		PHASE_WAITING = 0,
		PHASE_STRIKING = 1,
		PHASE_CHOOSING = 2,
		PHASE_PICKING = 3,
		PHASE_PLAYING = 4,
	};

	enum Action : u8
	{
		ACTION_JOIN_QUEUE = 0,
		ACTION_LEAVE_QUEUE = 1,
		ACTION_STRIKE = 2,
		ACTION_CHOOSE = 3,
		ACTION_PICK = 4,
		ACTION_SET_COLOR = 5,
		ACTION_MATCH_ENDED = 6,
		ACTION_ADD_TEST_PLAYER = 7,
		ACTION_FINISH_SET = 8,
		ACTION_LEAVE_ROOM = 9,
	};

	enum MatchResult : s8
	{
		RESULT_DRAW = -1,
		RESULT_LOST = 0,
		RESULT_WON = 1,
	};

	// What happened to the previous host when a member takes over
	enum HostChange
	{
		HOST_LEFT,        // Left on purpose and gave the room up
		HOST_LOST,        // Dropped, and gets the room back if they return in time
		HOST_HANDED_BACK, // Handed the room back to the host who was lost, and stays on
	};

	static const int MAX_MEMBERS = ROOM_MAX_MEMBERS;
	static const int STAGE_COUNT = ROOM_STAGE_COUNT;
	static const u8 CHAR_RANDOM = 26;
	static const u8 MODE_COUNT = 5;
	static const u8 STAGE_MODE_COUNT = 2;

	static const int SET_DELAY_MS = 2000;
	static const int CROWN_DELAY_MS = 4000;
	static const int TEST_PLAYER_DELAY_MS = 1000;
	static const int TURN_SECONDS = 30;
	static const int GRACE_SECONDS = 3;

	// A match that ended without a result is replayed after this long. Long enough for a player who
	// dropped to be noticed first, so they forfeit the set instead
	static const int REPLAY_DELAY_MS = 10000;

	// Players who drop keep their crowns this long, and a lost host gets the room back
	static const int REJOIN_WINDOW_MS = 2 * 60 * 1000;

	SlippiRoom();
	SlippiRoom(const SlippiExiTypes::CreateRoomQuery &query);

	// Returns the new member's index, -1 if the room is full
	int AddMember(const std::string &name, const std::string &connectCode, u8 charId = CHAR_RANDOM, u8 charColor = 0);
	void RemoveMember(int member);
	void DropMember(int member);
	void AddTestPlayer();
	int FindMember(const std::string &connectCode) const;
	int MemberCount();
	bool IsFull();
	const std::string &MemberCode(int member) { return members[member].connectCode; }
	bool IsTestPlayer(int member) { return members[member].isTestPlayer; }

	// Hosting. Members are kept in the order they joined, so the next host is the member who has been
	// in the room longest
	void TakeOver(const std::string &newHostCode, HostChange change);
	std::vector<std::string> Successors();
	bool IsReturningHost(const std::string &connectCode);
	const std::string &HostCode() { return hostCode; }
	void SetHostCode(const std::string &code) { hostCode = code; }
	int Generation() { return generation; }
	void SetGeneration(int gen) { generation = gen; }

	// Counts joins, leaves and finished sets, which the host reports to the directory
	u32 ActivityCount() { return activityCount; }

	// Watching. Players say where their match can be reached, and watchers where they watch from so
	// the players can punch through to them
	void SetMatchAddress(int member, const std::string &address);
	void SetWatchAddress(int member, const std::string &address);
	std::string SideMatchAddress(Side side) const;
	std::string SideName(Side side) const;
	std::vector<std::string> WatchAddresses() const;

	void HandleAction(int member, u8 action, u8 value0, u8 value1);
	void ReportMatchResult(int member, MatchResult result);
	void Update();

	void SetCode(const std::string &code) { roomCode = code; }
	const std::string &Code() const { return roomCode; }
	const std::string &Password() const { return password; }
	bool IsListed() { return visibility == 0; }
	u8 Mode() { return mode; }
	u8 StageMode() { return stageMode; }
	u8 Capacity() { return capacity; }

	// What the host sends its members, and how a member's copy is made from it
	nlohmann::json ToJson();
	void FromJson(const nlohmann::json &j);

	SlippiExiTypes::GetRoomStateResponse GetState(int localMember);

  protected:
	struct Member
	{
		std::string name;
		std::string connectCode;
		u8 charId = CHAR_RANDOM;
		u8 charColor = 0;
		u8 crowns = 0;
		bool isTestPlayer = false;
		std::string matchAddress; // Where watchers connect while this member plays
		std::string watchAddress; // Where this member watches from
	};

	// A player who dropped rather than leaving on purpose
	struct Dropped
	{
		std::string connectCode;
		u8 crowns = 0;
		bool wasQueued = false;
		u32 droppedMs = 0;
	};

	int queuePos(int member);
	bool isOnSide(int member);
	void joinQueue(int member);
	void leaveQueue(int member);
	void fillSides();
	void startSet();
	void finishSet(Side winner);
	bool hasBeatenEveryone();
	Side turnSide();
	int turnSeconds();
	int firstOpenStage();
	void strike(int idx);
	void choose(int idx);
	void pick(Side side, u8 charId, u8 charColor);
	void startMatch();
	void startTurn();
	void takeTestPlayerTurn(Side side);
	u8 maxColors(u8 charId);
	void expireDropped();

	u8 visibility = 0;
	u8 mode = 0;
	u8 capacity = 0;
	u8 stageMode = 0;
	std::string roomCode;
	std::string password;

	std::vector<Member> members;
	std::vector<Dropped> dropped;
	std::string hostCode;
	int generation = 0;
	std::string returningHostCode; // The host who was lost, while they can still get the room back
	u32 returningHostMs = 0;
	u32 activityCount = 0;

	std::vector<int> queue;
	int sides[2] = {-1, -1};
	u8 streak = 0;
	u32 beaten = 0; // Bit per member the current winner has beaten
	int crowned = -1;
	Phase phase = PHASE_WAITING;
	bool struck[STAGE_COUNT] = {};
	int stageIdx = 0;
	bool hasPicked[2] = {};
	u8 playChar[2] = {};
	u8 playColor[2] = {};

	u32 phaseStartMs = 0;
	u32 turnStartMs = 0;
	bool isMatchOver = false; // The match ended without a result and waits to be replayed
	u32 matchOverMs = 0;

	// A member's copy shows the host's timer rather than timing turns itself
	bool isCopy = false;
	int copyTurnSeconds = -1;

	std::default_random_engine generator;
};
