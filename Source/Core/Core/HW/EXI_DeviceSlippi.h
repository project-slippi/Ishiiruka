// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <SlippiGame.h>
#include <string>
#include <unordered_map>
#include <deque>
#include <ctime>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Core/HW/EXI_Device.h"

// Acts
class CEXISlippi : public IEXIDevice
{
public:
	CEXISlippi();
  virtual ~CEXISlippi();

	void DMAWrite(u32 _uAddr, u32 _uSize) override;
	void ImmWrite(u32 data, u32 size) override;
	u32 ImmRead(u32 size) override;

	bool IsPresent() const override;

private:
	enum {
		CMD_UNKNOWN = 0x0,
		CMD_RECEIVE_COMMANDS = 0x35,
		CMD_RECEIVE_GAME_INFO = 0x36,
		CMD_RECEIVE_POST_FRAME_UPDATE = 0x38,
		CMD_RECEIVE_GAME_END = 0x39,
		CMD_PREPARE_REPLAY = 0x75,
		CMD_READ_FRAME = 0x76,
		CMD_GET_LOCATION = 0x77
	};

	std::unordered_map<u8, u32> payloadSizes = {
		// The actual size of this command will be sent in one byte
		// after the command is received. The other receive command IDs
		// and sizes will be received immediately following
		{ CMD_RECEIVE_COMMANDS, 1},

		// The following are all commands used to play back a replay and
		// have fixed sizes
		{ CMD_PREPARE_REPLAY, 0 },
		{ CMD_READ_FRAME, 6 },
		{ CMD_GET_LOCATION, 6 }
	};

	// .slp File creation stuff
	u32 writtenByteCount = 0;
	
	// vars for metadata generation
	time_t gameStartTime;
	int32_t lastFrame;
	std::unordered_map<u8, std::unordered_map<u8, u32>> characterUsage;

	void updateMetadataFields(u8* payload, u32 length);
	void configureCommands(u8* payload, u8 length);
	void writeToFile(u8* payload, u32 length, std::string fileOption);
	std::vector<u8> generateMetadata();
	void createNewFile();
	void closeFile();
	std::string generateFileName();

	File::IOFile m_file;
	u32 m_payload_loc = 0;
	u8 m_payload_type = CMD_UNKNOWN;
	std::vector<u8> m_payload;

	// replay playback stuff
	void loadFile(std::string path);
	void prepareGameInfo();
	void prepareFrameData(u8* payload);
	void prepareLocationData(u8* payload);

	std::unordered_map<u8, std::string> getNetplayNames();

	std::deque<u32> m_read_queue;
	Slippi::SlippiGame* m_current_game = nullptr;

protected:
	void TransferByte(u8& byte) override;
};
