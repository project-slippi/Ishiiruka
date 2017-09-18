// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/EXI_DeviceSlippi.h"

#include <array>
#include <unordered_map>
#include <stdexcept>

#include "SlippiLib/SlippiGame.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "wx/datetime.h"

CEXISlippi::CEXISlippi() {
  INFO_LOG(EXPANSIONINTERFACE, "EXI SLIPPI Constructor called.");
}

CEXISlippi::~CEXISlippi() {
	closeFile();
}

void CEXISlippi::createNewFile() {
	if (m_file) {
		// If we already have a file, return
		return;
	}

	File::CreateDir("Slippi");
	std::string filepath = generateFileName();
	m_file = File::IOFile(filepath, "wb");
}

void CEXISlippi::closeFile() {
	if (!m_file) {
		// If we have no file or payload is not game end, do nothing
		return;
	}

	// If this is the end of the game end payload, reset the file so that we create a new one
	m_file.Close();
	m_file = nullptr;
}

void CEXISlippi::writeFileContents(u8* toWrite, u32 length) {
	if (!m_file) {
		// If no file, do nothing
		return;
	}

	bool result = m_file.WriteBytes(toWrite, length);

	if (!result) {
		ERROR_LOG(EXPANSIONINTERFACE, "Failed to write data to file.");
	}
}

std::string CEXISlippi::generateFileName()
{
	std::string str = wxDateTime::Now().Format(wxT("%Y%m%dT%H%M%S"));
	return StringFromFormat("Slippi/Game_%s.slp", str.c_str());
}

void CEXISlippi::loadFile(std::string path) {
	m_current_game = Slippi::SlippiGame::FromFile((std::string)path);
}

void CEXISlippi::prepareGameInfo() {
	// Since we are prepping new data, clear any existing data
	m_read_queue.clear();

	if (!m_current_game) {
		// Do nothing if we don't have a game loaded
		return;
	}

	Slippi::GameSettings* settings = m_current_game->GetSettings();

	// Build a word containing the stage and the presence of the characters
	u32 randomSeed = settings->randomSeed;
	m_read_queue.push_back(randomSeed);

	// This is kinda dumb but we need to handle the case where a player transforms
	// into sheik/zelda immediately. This info is not stored in the game info header
	// and so let's overwrite those values
	int player1Pos = 24; // This is the index of the first players character info
	std::array<uint32_t, Slippi::GAME_INFO_HEADER_SIZE> gameInfoHeader = settings->header;
	for (int i = 0; i < 4; i++) {
		// check if this player is actually in the game
		bool playerExists = m_current_game->DoesPlayerExist(i);
		if (!playerExists) {
			continue;
		}

		// check if the player is playing sheik or zelda
		uint8_t externalCharId = settings->players[i].characterId;
		if (externalCharId != 0x12 && externalCharId != 0x13) {
			continue;
		}

		// this is the position in the array that this player's character info is stored
		int pos = player1Pos + (9 * i);
		
		// here we have determined the player is playing sheik or zelda...
		// at this point let's overwrite the player's character with the one
		// that they are playing
		gameInfoHeader[pos] &= 0x00FFFFFF;
		gameInfoHeader[pos] |= externalCharId << 24;
	}

	// Write entire header to game
	for (int i = 0; i < Slippi::GAME_INFO_HEADER_SIZE; i++) {
		m_read_queue.push_back(gameInfoHeader[i]);
	}
}

void CEXISlippi::prepareFrameData(int32_t frameIndex, uint8_t port) {
	// Since we are prepping new data, clear any existing data
	m_read_queue.clear();

	if (!m_current_game) {
		// Do nothing if we don't have a game loaded
		return;
	}

	// Load the data from this frame into the read buffer
	uint8_t* a = (uint8_t*)&frameIndex;
  frameIndex = a[0] << 24 | a[1] << 16 | a[2] << 8 | a[3];
	bool frameExists = m_current_game->DoesFrameExist(frameIndex);
	if (!frameExists) {
	  return;
	}

	Slippi::FrameData* frame = m_current_game->GetFrame(frameIndex);

	// Add random seed to the front of the response regardless of player
  m_read_queue.push_back(*(u32*)&frame->randomSeed);

  // Check if player exists
  if (!frame->players.count(port)) {
    return;
  }

  // Get data for this player
	Slippi::PlayerFrameData data = frame->players.at(port);

  // Add all of the inputs in order
  m_read_queue.push_back(*(u32*)&data.joystickX);
  m_read_queue.push_back(*(u32*)&data.joystickY);
  m_read_queue.push_back(*(u32*)&data.cstickX);
  m_read_queue.push_back(*(u32*)&data.cstickY);
  m_read_queue.push_back(*(u32*)&data.trigger);
  m_read_queue.push_back(data.buttons);
}

void CEXISlippi::prepareLocationData(int32_t frameIndex, uint8_t port) {
	// Since we are prepping new data, clear any existing data
	m_read_queue.clear();

	if (!m_current_game) {
		// Do nothing if we don't have a game loaded
		return;
	}

	// Load the data from this frame into the read buffer
	uint8_t* a = (uint8_t*)&frameIndex;
  frameIndex = a[0] << 24 | a[1] << 16 | a[2] << 8 | a[3];
	bool frameExists = m_current_game->DoesFrameExist(frameIndex);
	if (!frameExists) {
	  return;
	}

	Slippi::FrameData* frame = m_current_game->GetFrame(frameIndex);
  if (!frame->players.count(port)) {
    return;
  }

  // Get data for this player
	Slippi::PlayerFrameData data = frame->players.at(port);

	// Add all of the inputs in order
  m_read_queue.push_back(*(u32*)&data.locationX);
  m_read_queue.push_back(*(u32*)&data.locationY);
  m_read_queue.push_back(*(u32*)&data.facingDirection);
}

void CEXISlippi::ImmWrite(u32 data, u32 size)
{
	//init();
	INFO_LOG(EXPANSIONINTERFACE, "EXI SLIPPI ImmWrite: %08x, size: %d", data, size);

	bool lookingForMessage = m_payload_type == CMD_UNKNOWN;
	if (lookingForMessage) {
		// If the size is not one, this can't be the start of a command
		if (size != 1) {
			return;
		}

		m_payload_type = data >> 24;

		// Attempt to get payload size for this command. If not found, don't do anything
		if (!payloadSizes.count(m_payload_type)) {
		  m_payload_type = CMD_UNKNOWN;
    	return;
		}
	}

	// Read and incremement our payload location
	m_payload_loc += size;

	// Add new data to payload
	for (u32 i = 0; i < size; i++) {
		int shiftAmount = 8 * (3 - i);
		u8 byte = 0xFF & (data >> shiftAmount);
		m_payload.push_back(byte);
	}

	// This section deals with saying we are done handling the payload
	// add one because we count the command as part of the total size
	u32 payloadSize = payloadSizes[m_payload_type];
	if (m_payload_loc >= payloadSize + 1) {
		// Handle payloads
		switch (m_payload_type) {
		case CMD_GAME_INIT:
			// Here we create a new file if one doesn't exist already
			createNewFile();
			writeFileContents(&m_payload[0], m_payload_loc);
			break;
		case CMD_GAME_START:
			writeFileContents(&m_payload[0], m_payload_loc);
			break;
		case CMD_FRAME_UPDATE:
			writeFileContents(&m_payload[0], m_payload_loc);
			break;
		case CMD_GAME_END:
			writeFileContents(&m_payload[0], m_payload_loc);
			closeFile();
			break;
		case CMD_PREPARE_REPLAY:
			loadFile("Slippi/CurrentGame.slp");
			prepareGameInfo();
			break;
		case CMD_READ_FRAME:
			prepareFrameData(*(int32_t*)&m_payload[1], *(uint8_t*)&m_payload[5]);
			break;
		case CMD_GET_LOCATION:
			prepareLocationData(*(int32_t*)&m_payload[1], *(uint8_t*)&m_payload[5]);
			break;
		}

		// reset payload loc and type so we look for next command
		m_payload_loc = 0;
		m_payload_type = CMD_UNKNOWN;
		m_payload.clear();
	}
}

u32 CEXISlippi::ImmRead(u32 size)
{
	if (m_read_queue.empty()) {
		INFO_LOG(EXPANSIONINTERFACE, "EXI SLIPPI ImmRead: Empty");
		return 0;
	}

	u32 value = m_read_queue.front();
	m_read_queue.pop_front();

	INFO_LOG(EXPANSIONINTERFACE, "EXI SLIPPI ImmRead %08x", value);

	return value;
}

bool CEXISlippi::IsPresent() const
{
	return true;
}

void CEXISlippi::TransferByte(u8& byte)
{
}