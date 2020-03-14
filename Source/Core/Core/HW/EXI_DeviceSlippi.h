// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <SlippiGame.h>
#include <ctime>
#include <deque>
#include <future>
#include <mutex>
#include <open-vcdiff/src/google/vcdecoder.h>
#include <open-vcdiff/src/google/vcencoder.h>
#include <string>
#include <unordered_map>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Core/HW/EXI_Device.h"
#include "Core/Slippi/SlippiMatchmaking.h"
#include "Core/Slippi/SlippiNetplay.h"
#include "Core/Slippi/SlippiReplayComm.h"
#include "Core/Slippi/SlippiSavestate.h"

#define MELEE_HEAP_START 0x00bd5c40
#define MELEE_HEAP_SIZE 0x5D7960

#define ROLLBACK_MAX_FRAMES 5

// Acts
class CEXISlippi : public IEXIDevice
{
  public:
	CEXISlippi();
	virtual ~CEXISlippi();

	void DMAWrite(u32 _uAddr, u32 _uSize) override;
	void DMARead(u32 addr, u32 size) override;

	bool IsPresent() const override;
	std::thread m_savestateThread;
	std::thread m_seekThread;

  private:
	enum
	{
		CMD_UNKNOWN = 0x0,

		// Recording
		CMD_RECEIVE_COMMANDS = 0x35,
		CMD_RECEIVE_GAME_INFO = 0x36,
		CMD_RECEIVE_POST_FRAME_UPDATE = 0x38,
		CMD_RECEIVE_GAME_END = 0x39,

		// Playback
		CMD_PREPARE_REPLAY = 0x75,
		CMD_READ_FRAME = 0x76,
		CMD_GET_LOCATION = 0x77,
		CMD_IS_FILE_READY = 0x88,
		CMD_IS_STOCK_STEAL = 0x89,
		CMD_GET_GECKO_CODES = 0x8A,
		CMD_GET_FRAME_COUNT = 0x90,

		// Online
		CMD_ONLINE_INPUTS = 0xB0,
		CMD_CAPTURE_SAVESTATE = 0xB1,
		CMD_LOAD_SAVESTATE = 0xB2,
		CMD_GET_MATCH_STATE = 0xB3,
		CMD_FIND_OPPONENT = 0xB4,
		CMD_SET_MATCH_SELECTIONS = 0xB5,
	};

	enum
	{
		FRAME_RESP_WAIT = 0,
		FRAME_RESP_CONTINUE = 1,
		FRAME_RESP_TERMINATE = 2,
		FRAME_RESP_FASTFORWARD = 3,
	};

	std::unordered_map<u8, u32> payloadSizes = {
	    // The actual size of this command will be sent in one byte
	    // after the command is received. The other receive command IDs
	    // and sizes will be received immediately following
	    {CMD_RECEIVE_COMMANDS, 1},

	    // The following are all commands used to play back a replay and
	    // have fixed sizes
	    {CMD_PREPARE_REPLAY, 0},
	    {CMD_READ_FRAME, 4},
	    {CMD_IS_STOCK_STEAL, 5},
	    {CMD_GET_LOCATION, 6},
	    {CMD_IS_FILE_READY, 0},
	    {CMD_GET_GECKO_CODES, 0},
	    {CMD_GET_FRAME_COUNT, 0},

	    // The following are used for Slippi online and also have fixed sizes
	    {CMD_ONLINE_INPUTS, 17},
	    {CMD_CAPTURE_SAVESTATE, 32},
	    {CMD_LOAD_SAVESTATE, 32},
	    {CMD_GET_MATCH_STATE, 0},
	    {CMD_FIND_OPPONENT, 0},
	    {CMD_SET_MATCH_SELECTIONS, 6},
	};

	// Communication with Launcher
	SlippiReplayComm *replayComm;

	// .slp File creation stuff
	u32 writtenByteCount = 0;

	// vars for metadata generation
	time_t gameStartTime;
	int32_t lastFrame;
	std::unordered_map<u8, std::unordered_map<u8, u32>> characterUsage;

	void updateMetadataFields(u8 *payload, u32 length);
	void configureCommands(u8 *payload, u8 length);
	void writeToFile(u8 *payload, u32 length, std::string fileOption);
	std::vector<u8> generateMetadata();
	void createNewFile();
	void closeFile();
	std::string generateFileName();
	bool checkFrameFullyFetched(int32_t frameIndex);
	bool shouldFFWFrame(int32_t frameIndex);

	// std::ofstream log;

	File::IOFile m_file;
	std::vector<u8> m_payload;

	// online play stuff
	void handleOnlineInputs(u8 *payload);
	void prepareOpponentInputs(u8 *payload);
	void handleSendInputs(u8 *payload);
	void handleCaptureSavestate(u8 *payload);
	void handleLoadSavestate(u8 *payload);
	void startFindMatch();
	void prepareOnlineMatchState();
	void setMatchSelections(u8 *payload);
	bool shouldSkipOnlineFrame(int32_t frame);

	// replay playback stuff
	void prepareGameInfo();
	void prepareGeckoList();
	void prepareCharacterFrameData(int32_t frameIndex, u8 port, u8 isFollower);
	void prepareFrameData(u8 *payload);
	void prepareIsStockSteal(u8 *payload);
	void prepareFrameCount();
	void prepareSlippiPlayback(int32_t &frameIndex);
	void prepareIsFileReady();
	void processInitialState(std::vector<u8> &iState);
	void resetPlayback();
	void clearWatchSettingsStartEnd();

	void SavestateThread(void);
	void SeekThread(void);

	std::unordered_map<int32_t, std::shared_future<std::string>>
	    futureDiffs;        // State diffs keyed by frameIndex, processed async
	std::vector<u8> iState; // The initial state
	std::vector<u8> cState; // The current (latest) state

	bool shouldFFWToTarget = false;
	int mostRecentlyProcessedFrame = INT_MAX;

	open_vcdiff::VCDiffDecoder decoder;
	open_vcdiff::VCDiffEncoder *encoder = NULL;

	std::unordered_map<u8, std::string> getNetplayNames();

	std::vector<uint8_t> geckoList;

	bool isSoftFFW = false;
	bool isHardFFW = false;
	int32_t lastFFWFrame = INT_MIN;
	std::vector<u8> m_read_queue;
	Slippi::SlippiGame *m_current_game = nullptr;

  protected:
	void TransferByte(u8 &byte) override;

  private:
	SlippiPlayerSelections localSelections;

	std::unique_ptr<SlippiNetplayClient> slippi_netplay;
	std::unique_ptr<SlippiMatchmaking> matchmaking;

	std::map<u32, std::unique_ptr<SlippiSavestate>> activeSavestates;
	std::deque<std::unique_ptr<SlippiSavestate>> availableSavestates;
};
