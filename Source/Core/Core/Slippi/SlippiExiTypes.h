#pragma once

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"

#define REPORT_PLAYER_COUNT 4
#define ROOM_MAX_MEMBERS 32
#define ROOM_STAGE_COUNT 6
#define ROOM_LIST_MAX 50 // As many as the directory lists

namespace SlippiExiTypes
{
// Using pragma pack here will remove any structure padding which is what EXI comms expect
// https://www.geeksforgeeks.org/how-to-avoid-structure-padding-in-c/
// Note that none of these classes should be used outside of the handler function, pragma pack
// is supposedly not very efficient?
#pragma pack(1)

struct ReportGameQueryPlayer
{
	u8 slotType;
	u8 stocksRemaining;
	float damageDone;
	u8 syncedStocksRemaining;
	u16 syncedCurrentHealth;
};

struct ReportGameQuery
{
	u8 command;
	u8 onlineMode;
	u32 frameLength;
	u32 gameIndex;
	u32 tiebreakIndex;
	s8 winnerIdx;
	u8 gameEndMethod;
	s8 lrasInitiator;
	u32 syncedTimer;
	ReportGameQueryPlayer players[REPORT_PLAYER_COUNT];
	u8 gameInfoBlock[312];
};

struct ReportSetCompletionQuery
{
	u8 command;
	u8 endMode;
};

struct ReportMatchStatusUpdateQuery
{
	u8 command;
	u8 statusIdx;
};

struct GpCompleteStepQuery
{
	u8 command;
	u8 step_idx;
	u8 char_selection;
	u8 char_color_selection;
	u8 stage_selections[2];
};

struct GpFetchStepQuery
{
	u8 command;
	u8 step_idx;
};

struct GpFetchStepResponse
{
	u8 is_found;
	u8 is_skip;
	u8 char_selection;
	u8 char_color_selection;
	u8 stage_selections[2];
};

struct OverwriteCharSelections
{
	u8 is_set;
	u8 char_id;
	u8 char_color_id;
};
struct OverwriteSelectionsQuery
{
	u8 command;
	u16 stage_id;
	OverwriteCharSelections chars[4];
};

struct PlayerSettings
{
	char chatMessages[16][51];
};
struct GetPlayerSettingsResponse
{
	PlayerSettings settings[4];
};

struct PlayMusicQuery
{
	u8 command;
	u32 offset;
	u32 size;
};

struct ChangeMusicVolumeQuery
{
	u8 command;
	u8 volume;
};

struct CreateRoomQuery
{
	u8 command;
	u8 visibility;
	u8 mode;
	u8 capacity;
	u8 stage_mode;
	u8 last_char;
	u8 last_color;
};

struct RoomActionQuery
{
	u8 command;
	u8 action;
	u8 value[2];
};

struct RoomMember
{
	char name[31];
	char connect_code[10];
	u8 char_id;
	u8 char_color;
	u8 crowns;
};

struct GetRoomStateResponse
{
	u8 is_active;
	u8 connection_status;
	u8 connection_error;
	u8 visibility;
	u8 mode;
	u8 capacity;
	u8 stage_mode;
	char code[5];     // Empty until the room is registered
	char password[5]; // Empty for public rooms
	u8 local_member;
	u8 member_count;
	RoomMember members[ROOM_MAX_MEMBERS];
	u8 queue_count;
	u8 queue[ROOM_MAX_MEMBERS];
	s8 sides[2];
	u8 streak;
	s8 crowned;
	u8 phase;
	u8 struck[ROOM_STAGE_COUNT];
	u8 stage_idx;
	u8 has_picked[2];
	u8 play_char[2];
	u8 play_color[2];
	u8 turn_seconds; // 0xFF when it's nobody's turn
	u8 host_member;  // 0xFF while nobody is hosting
	u8 watch_status; // Watching the room's match, see CEXISlippi::WatchStatus
	u8 match_over;   // The match ended without a result, and the room replays it shortly
};

struct JoinRoomQuery
{
	u8 command;
	char code[5];
	char password[5]; // Empty when joining a public room
	u8 last_char;
	u8 last_color;
};

struct RoomListing
{
	char code[5];
	char host_name[31];
	u8 mode;
	u8 stage_mode;
	u8 capacity;
	u8 member_count;
	u8 region; // 0xFF when unknown
};

struct GetRoomListResponse
{
	u8 status;
	u8 count;
	RoomListing rooms[ROOM_LIST_MAX];
	char rejoin_code[5]; // The last room, after a crash. Empty when there's nothing to rejoin
	char rejoin_password[5];
};

// Not sure if resetting is strictly needed, might be contained to the file
#pragma pack()

template <typename T> inline T Convert(u8 *payload)
{
	return *reinterpret_cast<T *>(payload);
}

// Here we define custom convert functions for any type that larger than u8 sized fields to convert from big-endian

template <> inline ReportGameQuery Convert(u8 *payload)
{
	auto q = *reinterpret_cast<ReportGameQuery *>(payload);
	q.frameLength = Common::FromBigEndian(q.frameLength);
	q.gameIndex = Common::FromBigEndian(q.gameIndex);
	q.tiebreakIndex = Common::FromBigEndian(q.tiebreakIndex);
	q.syncedTimer = Common::FromBigEndian(q.syncedTimer);
	for (int i = 0; i < REPORT_PLAYER_COUNT; i++)
	{
		auto *p = &q.players[i];
		p->damageDone = Common::FromBigEndian(p->damageDone);
		p->syncedCurrentHealth = Common::FromBigEndian(p->syncedCurrentHealth);
	}
	return q;
}

template <> inline OverwriteSelectionsQuery Convert(u8 *payload)
{
	auto q = *reinterpret_cast<OverwriteSelectionsQuery *>(payload);
	q.stage_id = Common::FromBigEndian(q.stage_id);
	return q;
}

template <> inline PlayMusicQuery Convert(u8* payload)
{
	auto q = *reinterpret_cast<PlayMusicQuery *>(payload);
	q.offset = Common::FromBigEndian(q.offset);
	q.size = Common::FromBigEndian(q.size);
	return q;
}
}; // namespace SlippiExiTypes