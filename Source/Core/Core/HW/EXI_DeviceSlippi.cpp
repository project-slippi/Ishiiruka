// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/EXI_DeviceSlippi.h"
#include <SlippiGame.h>
#include <array>
#include <condition_variable>
#include <future>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>

#ifdef _WIN32
#include <share.h>
#endif

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Core/HW/Memmap.h"
#include "Core/NetPlayClient.h"
#include "Core/SlippiPlayback.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/SystemTimers.h"
#include "Core/State.h"

// Interval between savestates
#define FRAME_INTERVAL 900

#define START_FRAME -123

static int32_t currentPlaybackFrame = INT_MAX;
static int32_t latestFrame = INT_MAX;

#define SLEEP_TIME_MS 8

static std::mutex mtx;
static std::condition_variable condVar;

template <typename T> bool isFutureReady(std::future<T> &t)
{
	return t.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

std::vector<u8> uint16ToVector(u16 num)
{
	u8 byte0 = num >> 8;
	u8 byte1 = num & 0xFF;

	return std::vector<u8>({byte0, byte1});
}

std::vector<u8> uint32ToVector(u32 num)
{
	u8 byte0 = num >> 24;
	u8 byte1 = (num & 0xFF0000) >> 16;
	u8 byte2 = (num & 0xFF00) >> 8;
	u8 byte3 = num & 0xFF;

	return std::vector<u8>({byte0, byte1, byte2, byte3});
}

std::vector<u8> int32ToVector(int32_t num)
{
	u8 byte0 = num >> 24;
	u8 byte1 = (num & 0xFF0000) >> 16;
	u8 byte2 = (num & 0xFF00) >> 8;
	u8 byte3 = num & 0xFF;

	return std::vector<u8>({byte0, byte1, byte2, byte3});
}

void appendWordToBuffer(std::vector<u8> *buf, u32 word)
{
	auto wordVector = uint32ToVector(word);
	buf->insert(buf->end(), wordVector.begin(), wordVector.end());
}

void appendHalfToBuffer(std::vector<u8> *buf, u16 word)
{
	auto halfVector = uint16ToVector(word);
	buf->insert(buf->end(), halfVector.begin(), halfVector.end());
}

std::pair<int32_t, std::string> processDiff(std::vector<u8> &iState, std::vector<u8> &cState, int32_t frameNumber,
                                            open_vcdiff::VCDiffEncoder *&encoder)
{
	INFO_LOG(SLIPPI, "Saving at diff at frame: %d", frameNumber);
	std::string diff = std::string();
	if (encoder != NULL)
		encoder->Encode((char *)cState.data(), cState.size(), &diff);

	std::pair<int32_t, std::string> diffPair = std::make_pair(frameNumber, diff);
	return diffPair;
}

CEXISlippi::CEXISlippi()
{
	INFO_LOG(SLIPPI, "EXI SLIPPI Constructor called.");

	replayComm = new SlippiReplayComm();

	// Loggers will check 5 bytes, make sure we own that memory
	m_read_queue.reserve(5);

	// Spawn thread for savestates
	// maybe stick this into functions below so it doesn't always get spawned
	// only spin off and join when a replay is loaded, delete after replay is done, etc
	m_savestateThread = std::thread(&CEXISlippi::SavestateThread, this);
}

CEXISlippi::~CEXISlippi()
{
	u8 empty[1];

	// Closes file gracefully to prevent file corruption when emulation
	// suddenly stops. This would happen often on netplay when the opponent
	// would close the emulation before the file successfully finished writing
	writeToFile(&empty[0], 0, "close");
}

void CEXISlippi::configureCommands(u8 *payload, u8 length)
{
	for (int i = 1; i < length; i += 3)
	{
		// Go through the receive commands payload and set up other commands
		u8 commandByte = payload[i];
		u32 commandPayloadSize = payload[i + 1] << 8 | payload[i + 2];
		payloadSizes[commandByte] = commandPayloadSize;
	}
}

void CEXISlippi::updateMetadataFields(u8 *payload, u32 length)
{
	if (length <= 0 || payload[0] != CMD_RECEIVE_POST_FRAME_UPDATE)
	{
		// Only need to update if this is a post frame update
		return;
	}

	// Keep track of last frame
	lastFrame = payload[1] << 24 | payload[2] << 16 | payload[3] << 8 | payload[4];

	// Keep track of character usage
	u8 playerIndex = payload[5];
	u8 internalCharacterId = payload[7];
	if (!characterUsage.count(playerIndex) || !characterUsage[playerIndex].count(internalCharacterId))
	{
		characterUsage[playerIndex][internalCharacterId] = 0;
	}
	characterUsage[playerIndex][internalCharacterId] += 1;
}

std::unordered_map<u8, std::string> CEXISlippi::getNetplayNames()
{
	std::unordered_map<u8, std::string> names;

	if (netplay_client && netplay_client->IsConnected())
	{
		auto netplayPlayers = netplay_client->GetPlayers();
		for (auto it = netplayPlayers.begin(); it != netplayPlayers.end(); ++it)
		{
			auto player = *it;
			u8 portIndex = netplay_client->FindPlayerPad(player);
			if (portIndex < 0)
			{
				continue;
			}

			names[portIndex] = player->name;
		}
	}

	return names;
}

std::vector<u8> CEXISlippi::generateMetadata()
{
	std::vector<u8> metadata({'U', 8, 'm', 'e', 't', 'a', 'd', 'a', 't', 'a', '{'});

	// TODO: Abstract out UBJSON functions to make this cleaner

	// Add game start time
	uint8_t dateTimeStrLength = sizeof "2011-10-08T07:07:09Z";
	std::vector<char> dateTimeBuf(dateTimeStrLength);
	strftime(&dateTimeBuf[0], dateTimeStrLength, "%FT%TZ", gmtime(&gameStartTime));
	dateTimeBuf.pop_back(); // Removes the \0 from the back of string
	metadata.insert(metadata.end(), {'U', 7, 's', 't', 'a', 'r', 't', 'A', 't', 'S', 'U', (uint8_t)dateTimeBuf.size()});
	metadata.insert(metadata.end(), dateTimeBuf.begin(), dateTimeBuf.end());

	// Add game duration
	std::vector<u8> lastFrameToWrite = int32ToVector(lastFrame);
	metadata.insert(metadata.end(), {'U', 9, 'l', 'a', 's', 't', 'F', 'r', 'a', 'm', 'e', 'l'});
	metadata.insert(metadata.end(), lastFrameToWrite.begin(), lastFrameToWrite.end());

	// Add players elements to metadata, one per player index
	metadata.insert(metadata.end(), {'U', 7, 'p', 'l', 'a', 'y', 'e', 'r', 's', '{'});

	auto playerNames = getNetplayNames();

	for (auto it = characterUsage.begin(); it != characterUsage.end(); ++it)
	{
		auto playerIndex = it->first;
		auto playerCharacterUsage = it->second;

		metadata.push_back('U');
		std::string playerIndexStr = std::to_string(playerIndex);
		metadata.push_back((u8)playerIndexStr.length());
		metadata.insert(metadata.end(), playerIndexStr.begin(), playerIndexStr.end());
		metadata.push_back('{');

		// Add names element for this player
		metadata.insert(metadata.end(), {'U', 5, 'n', 'a', 'm', 'e', 's', '{'});

		if (playerNames.count(playerIndex))
		{
			auto playerName = playerNames[playerIndex];
			// Add netplay element for this player name
			metadata.insert(metadata.end(), {'U', 7, 'n', 'e', 't', 'p', 'l', 'a', 'y', 'S', 'U'});
			metadata.push_back((u8)playerName.length());
			metadata.insert(metadata.end(), playerName.begin(), playerName.end());
		}

		metadata.push_back('}'); // close names

		// Add character element for this player
		metadata.insert(metadata.end(), {'U', 10, 'c', 'h', 'a', 'r', 'a', 'c', 't', 'e', 'r', 's', '{'});
		for (auto it2 = playerCharacterUsage.begin(); it2 != playerCharacterUsage.end(); ++it2)
		{
			metadata.push_back('U');
			std::string internalCharIdStr = std::to_string(it2->first);
			metadata.push_back((u8)internalCharIdStr.length());
			metadata.insert(metadata.end(), internalCharIdStr.begin(), internalCharIdStr.end());

			metadata.push_back('l');
			std::vector<u8> frameCount = uint32ToVector(it2->second);
			metadata.insert(metadata.end(), frameCount.begin(), frameCount.end());
		}
		metadata.push_back('}'); // close characters

		metadata.push_back('}'); // close player
	}
	metadata.push_back('}');

	// Indicate this was played on dolphin
	metadata.insert(metadata.end(),
	                {'U', 8, 'p', 'l', 'a', 'y', 'e', 'd', 'O', 'n', 'S', 'U', 7, 'd', 'o', 'l', 'p', 'h', 'i', 'n'});

	metadata.push_back('}');
	return metadata;
}

void CEXISlippi::writeToFile(u8 *payload, u32 length, std::string fileOption)
{
	std::vector<u8> dataToWrite;
	if (fileOption == "create")
	{
		// If the game sends over option 1 that means a file should be created
		createNewFile();

		// Start ubjson file and prepare the "raw" element that game
		// data output will be dumped into. The size of the raw output will
		// be initialized to 0 until all of the data has been received
		std::vector<u8> headerBytes({'{', 'U', 3, 'r', 'a', 'w', '[', '$', 'U', '#', 'l', 0, 0, 0, 0});
		dataToWrite.insert(dataToWrite.end(), headerBytes.begin(), headerBytes.end());

		// Used to keep track of how many bytes have been written to the file
		writtenByteCount = 0;

		// Used to track character usage (sheik/zelda)
		characterUsage.clear();

		// Reset lastFrame
		lastFrame = Slippi::GAME_FIRST_FRAME;
	}

	// If no file, do nothing
	if (!m_file)
	{
		return;
	}

	// Update fields relevant to generating metadata at the end
	updateMetadataFields(payload, length);

	// Add the payload to data to write
	dataToWrite.insert(dataToWrite.end(), payload, payload + length);
	writtenByteCount += length;

	// If we are going to close the file, generate data to complete the UBJSON file
	if (fileOption == "close")
	{
		// This option indicates we are done sending over body
		std::vector<u8> closingBytes = generateMetadata();
		closingBytes.push_back('}');
		dataToWrite.insert(dataToWrite.end(), closingBytes.begin(), closingBytes.end());
	}

	// Write data to file
	bool result = m_file.WriteBytes(&dataToWrite[0], dataToWrite.size());
	if (!result)
	{
		ERROR_LOG(EXPANSIONINTERFACE, "Failed to write data to file.");
	}

	// If file should be closed, close it
	if (fileOption == "close")
	{
		// Write the number of bytes for the raw output
		std::vector<u8> sizeBytes = uint32ToVector(writtenByteCount);
		m_file.Seek(11, 0);
		m_file.WriteBytes(&sizeBytes[0], sizeBytes.size());

		// Close file
		closeFile();
	}
}

void CEXISlippi::createNewFile()
{
	if (m_file)
	{
		// If there's already a file open, close that one
		closeFile();
	}

	File::CreateDir("Slippi");
	std::string filepath = generateFileName();

	INFO_LOG(SLIPPI, "EXI_DeviceSlippi.cpp: Creating new replay file %s", filepath.c_str());

#ifdef _WIN32
	m_file = File::IOFile(filepath, "wb", _SH_DENYWR);
#else
	m_file = File::IOFile(filepath, "wb");
#endif
}

std::string CEXISlippi::generateFileName()
{
	// Add game start time
	uint8_t dateTimeStrLength = sizeof "20171015T095717";
	std::vector<char> dateTimeBuf(dateTimeStrLength);
	strftime(&dateTimeBuf[0], dateTimeStrLength, "%Y%m%dT%H%M%S", localtime(&gameStartTime));

	std::string str(&dateTimeBuf[0]);
	return StringFromFormat("Slippi/Game_%s.slp", str.c_str());
}

void CEXISlippi::closeFile()
{
	if (!m_file)
	{
		// If we have no file or payload is not game end, do nothing
		return;
	}

	// If this is the end of the game end payload, reset the file so that we create a new one
	m_file.Close();
	m_file = nullptr;
}

void CEXISlippi::prepareGameInfo()
{
	// Since we are prepping new data, clear any existing data
	m_read_queue.clear();

	if (!m_current_game)
	{
		// Do nothing if we don't have a game loaded
		return;
	}

	if (!m_current_game->AreSettingsLoaded())
	{
		m_read_queue.push_back(0);
		return;
	}

	// Return success code
	m_read_queue.push_back(1);

	Slippi::GameSettings *settings = m_current_game->GetSettings();

	// Start in Fast Forward if this is mirrored
	auto replayCommSettings = replayComm->getSettings();
	if (!isHardFFW)
		isHardFFW = replayCommSettings.mode == "mirror";
	lastFFWFrame = INT_MIN;

	// Build a word containing the stage and the presence of the characters
	u32 randomSeed = settings->randomSeed;
	appendWordToBuffer(&m_read_queue, randomSeed);

	// This is kinda dumb but we need to handle the case where a player transforms
	// into sheik/zelda immediately. This info is not stored in the game info header
	// and so let's overwrite those values
	int player1Pos = 24; // This is the index of the first players character info
	std::array<uint32_t, Slippi::GAME_INFO_HEADER_SIZE> gameInfoHeader = settings->header;
	for (int i = 0; i < 4; i++)
	{
		// check if this player is actually in the game
		bool playerExists = m_current_game->DoesPlayerExist(i);
		if (!playerExists)
		{
			continue;
		}

		// check if the player is playing sheik or zelda
		uint8_t externalCharId = settings->players[i].characterId;
		if (externalCharId != 0x12 && externalCharId != 0x13)
		{
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
	for (int i = 0; i < Slippi::GAME_INFO_HEADER_SIZE; i++)
	{
		appendWordToBuffer(&m_read_queue, gameInfoHeader[i]);
	}

	// Write UCF toggles
	std::array<uint32_t, Slippi::UCF_TOGGLE_SIZE> ucfToggles = settings->ucfToggles;
	for (int i = 0; i < Slippi::UCF_TOGGLE_SIZE; i++)
	{
		appendWordToBuffer(&m_read_queue, ucfToggles[i]);
	}

	// Write nametags
	for (int i = 0; i < 4; i++)
	{
		auto player = settings->players[i];
		for (int j = 0; j < Slippi::NAMETAG_SIZE; j++)
		{
			appendHalfToBuffer(&m_read_queue, player.nametag[j]);
		}
	}

	// Write PAL byte
	m_read_queue.push_back(settings->isPAL);

	// Get replay version numbers
	auto replayVersion = m_current_game->GetVersion();
	auto majorVersion = replayVersion[0];
	auto minorVersion = replayVersion[1];

	// Write PS pre-load byte
	auto shouldPreloadPs = majorVersion > 1 || (majorVersion == 1 && minorVersion > 2);
	m_read_queue.push_back(shouldPreloadPs);

	// Write PS Frozen byte
	m_read_queue.push_back(settings->isFrozenPS);

	// Set values for initializing saveState thread
	inReplay = true;
	hasProcessedSaveStates = false;
}

void CEXISlippi::prepareCharacterFrameData(int32_t frameIndex, u8 port, u8 isFollower)
{
	// Load the data from this frame into the read buffer
	Slippi::FrameData *frame = m_current_game->GetFrame(frameIndex);

	std::unordered_map<uint8_t, Slippi::PlayerFrameData> source;
	source = isFollower ? frame->followers : frame->players;

	// This must be updated if new data is added
	int characterDataLen = 49;

	// Check if player exists
	if (!source.count(port))
	{
		// If player does not exist, insert blank section
		m_read_queue.insert(m_read_queue.end(), characterDataLen, 0);
		return;
	}

	// Get data for this player
	Slippi::PlayerFrameData data = source[port];

	// log << frameIndex << "\t" << port << "\t" << data.locationX << "\t" << data.locationY << "\t" << data.animation
	// << "\n";

	// WARN_LOG(EXPANSIONINTERFACE, "[Frame %d] [Player %d] Positions: %f | %f", frameIndex, port, data.locationX,
	// data.locationY);

	// Add all of the inputs in order
	appendWordToBuffer(&m_read_queue, data.randomSeed);
	appendWordToBuffer(&m_read_queue, *(u32 *)&data.joystickX);
	appendWordToBuffer(&m_read_queue, *(u32 *)&data.joystickY);
	appendWordToBuffer(&m_read_queue, *(u32 *)&data.cstickX);
	appendWordToBuffer(&m_read_queue, *(u32 *)&data.cstickY);
	appendWordToBuffer(&m_read_queue, *(u32 *)&data.trigger);
	appendWordToBuffer(&m_read_queue, data.buttons);
	appendWordToBuffer(&m_read_queue, *(u32 *)&data.locationX);
	appendWordToBuffer(&m_read_queue, *(u32 *)&data.locationY);
	appendWordToBuffer(&m_read_queue, *(u32 *)&data.facingDirection);
	appendWordToBuffer(&m_read_queue, (u32)data.animation);
	m_read_queue.push_back(data.joystickXRaw);
	appendWordToBuffer(&m_read_queue, *(u32 *)&data.percent);
	// NOTE TO DEV: If you add data here, make sure to increase the size above
}

bool CEXISlippi::checkFrameFullyFetched(int32_t frameIndex)
{
	auto doesFrameExist = m_current_game->DoesFrameExist(frameIndex);
	if (!doesFrameExist)
		return false;

	Slippi::FrameData *frame = m_current_game->GetFrame(frameIndex);

	// This flag is set to true after a post frame update has been received. At that point
	// we know we have received all of the input data for the frame
	return frame->inputsFullyFetched;
}

void CEXISlippi::prepareFrameData(u8 *payload)
{
	// Since we are prepping new data, clear any existing data
	m_read_queue.clear();

	if (!m_current_game)
	{
		// Do nothing if we don't have a game loaded
		return;
	}

	// Parse input
	int32_t frameIndex = payload[0] << 24 | payload[1] << 16 | payload[2] << 8 | payload[3];

	// If loading from queue, move on to the next replay if we have past endFrame
	auto watchSettings = replayComm->current;
	if (frameIndex > watchSettings.endFrame)
	{
		INFO_LOG(SLIPPI, "Killing game because we are past endFrame");
		m_read_queue.push_back(FRAME_RESP_TERMINATE);
		return;
	}

	// If a new replay should be played, terminate the current game
	auto isNewReplay = replayComm->isNewReplay();
	if (isNewReplay)
	{
		m_read_queue.push_back(FRAME_RESP_TERMINATE);
		return;
	}

	auto isProcessingComplete = m_current_game->IsProcessingComplete();
	// Wait until frame exists in our data before reading it. We also wait until
	// next frame has been found to ensure we have actually received all of the
	// data from this frame. Don't wait until next frame is processing is complete
	// (this is the last frame, in that case)
	auto isFrameFound = m_current_game->DoesFrameExist(frameIndex);
	latestFrame = m_current_game->GetFrameCount();
	auto isNextFrameFound = latestFrame > frameIndex;
	auto isFrameComplete = checkFrameFullyFetched(frameIndex);
	auto isFrameReady = isFrameFound && (isProcessingComplete || isNextFrameFound || isFrameComplete);

	// If there is a startFrame configured, manage the fast-forward flag
	if (watchSettings.startFrame > Slippi::GAME_FIRST_FRAME)
	{
		if (frameIndex < watchSettings.startFrame)
		{
			isHardFFW = true;
		}
		else if (frameIndex == watchSettings.startFrame)
		{
			// TODO: This might disable fast forward on first frame when we dont want to?
			isHardFFW = false;
		}
	}

	// If RealTimeMode is enabled, let's trigger fast forwarding under certain conditions
	auto commSettings = replayComm->getSettings();
	auto isFarBehind = latestFrame - frameIndex > 2;
	auto isVeryFarBehind = latestFrame - frameIndex > 25;
	if (isFarBehind && commSettings.mode == "mirror" && commSettings.isRealTimeMode)
	{
		isSoftFFW = true;

		// Once isHardFFW has been turned on, do not turn it off with this condition, should
		// hard FFW to the latest point
		if (!isHardFFW)
			isHardFFW = isVeryFarBehind;
	}

	if (latestFrame == frameIndex)
	{
		// The reason to disable fast forwarding here is in hopes
		// of disabling it on the last frame that we have actually received.
		// Doing this will allow the rendering logic to run to display the
		// last frame instead of the frame previous to fast forwarding.
		// Not sure if this fully works with partial frames
		isSoftFFW = false;
		isHardFFW = false;
	}

	currentPlaybackFrame = frameIndex;

	// For normal replays, modify slippi seek/playback data as needed
	// TODO: maybe handle other modes too?
	if (commSettings.mode == "normal")
	{
		prepareSlippiPlayback(frameIndex);
	}

	bool shouldFFW = shouldFFWFrame(frameIndex);
	u8 requestResultCode = shouldFFW ? FRAME_RESP_FASTFORWARD : FRAME_RESP_CONTINUE;
	if (!isFrameReady)
	{
		// If processing is complete, the game has terminated early. Tell our playback
		// to end the game as well.
		auto shouldTerminateGame = isProcessingComplete;
		requestResultCode = shouldTerminateGame ? FRAME_RESP_TERMINATE : FRAME_RESP_WAIT;
		m_read_queue.push_back(requestResultCode);

		// Disable fast forward here too... this shouldn't be necessary but better
		// safe than sorry I guess
		isSoftFFW = false;
		isHardFFW = false;

		if (requestResultCode == FRAME_RESP_TERMINATE)
		{
			ERROR_LOG(EXPANSIONINTERFACE, "Game should terminate on frame %d [%X]", frameIndex, frameIndex);
		}

		return;
	}

	// WARN_LOG(EXPANSIONINTERFACE, "[Frame %d] Playback current behind by: %d frames.", frameIndex,
	//        latestFrame - frameIndex);

	// Keep track of last FFW frame, used for soft FFW's
	if (shouldFFW)
	{
		WARN_LOG(EXPANSIONINTERFACE, "[Frame %d] FFW frame, behind by: %d frames.", frameIndex,
		         latestFrame - frameIndex);
		lastFFWFrame = frameIndex;
	}

	// Return success code
	m_read_queue.push_back(requestResultCode);

	// Add frame rng seed to be restored at priority 0
	Slippi::FrameData *frame = m_current_game->GetFrame(frameIndex);
	u8 rngResult = frame->randomSeedExists ? 1 : 0;
	m_read_queue.push_back(rngResult);
	appendWordToBuffer(&m_read_queue, *(u32 *)&frame->randomSeed);

	// Add frame data for every character
	for (u8 port = 0; port < 4; port++)
	{
		prepareCharacterFrameData(frameIndex, port, 0);
		prepareCharacterFrameData(frameIndex, port, 1);
	}
}

bool CEXISlippi::shouldFFWFrame(int32_t frameIndex)
{
	if (!isSoftFFW && !isHardFFW)
	{
		// If no FFW at all, don't FFW this frame
		return false;
	}

	if (isHardFFW)
	{
		// For a hard FFW, always FFW until it's turned off
		return true;
	}

	// Here we have a soft FFW, we only want to turn on FFW for single frames once
	// every X frames to FFW in a more smooth manner
	return frameIndex - lastFFWFrame >= 15;
}

void CEXISlippi::prepareIsStockSteal(u8 *payload)
{
	// Since we are prepping new data, clear any existing data
	m_read_queue.clear();

	if (!m_current_game)
	{
		// Do nothing if we don't have a game loaded
		return;
	}

	// Parse args
	int32_t frameIndex = payload[0] << 24 | payload[1] << 16 | payload[2] << 8 | payload[3];
	u8 playerIndex = payload[4];

	// I'm not sure checking for the frame should be necessary. Theoretically this
	// should get called after the frame request so the frame should already exist
	auto isFrameFound = m_current_game->DoesFrameExist(frameIndex);
	if (!isFrameFound)
	{
		m_read_queue.push_back(0);
		return;
	}

	// Load the data from this frame into the read buffer
	Slippi::FrameData *frame = m_current_game->GetFrame(frameIndex);
	auto players = frame->players;

	u8 playerIsBack = players.count(playerIndex) ? 1 : 0;
	m_read_queue.push_back(playerIsBack);
}

void CEXISlippi::prepareFrameCount()
{
	m_read_queue.clear();

	if (!m_current_game)
	{
		// Do not start if replay file doesn't exist
		// TODO: maybe display error message?
		INFO_LOG(EXPANSIONINTERFACE, "EXI_DeviceSlippi.cpp: Replay file does not exist");
		m_read_queue.push_back(0);
		return;
	}

	if (m_current_game->IsProcessingComplete())
	{
		m_read_queue.push_back(0);
		return;
	}

	int bufferCount = 15;

	// Make sure we've loaded all the latest data, maybe this should be part of GetFrameCount
	auto latestFrame = m_current_game->GetFrameCount();
	auto frameCount = latestFrame - Slippi::GAME_FIRST_FRAME;
	auto frameCountPlusBuffer = frameCount + bufferCount;

	u8 result = frameCountPlusBuffer > 0xFF ? 0xFF : (u8)frameCountPlusBuffer;
	WARN_LOG(EXPANSIONINTERFACE, "EXI_DeviceSlippi.cpp: Fast forwarding by %d frames. (+%d)", result, bufferCount);

	m_read_queue.push_back(result);
}

void CEXISlippi::prepareIsFileReady()
{
	m_read_queue.clear();

	auto isNewReplay = replayComm->isNewReplay();
	if (!isNewReplay)
	{
		replayComm->nextReplay();
		m_read_queue.push_back(0);
		inReplay = false;
		return;
	}

	// Attempt to load game if there is a new replay file
	// this can come pack falsy if the replay file does not exist
	m_current_game = replayComm->loadGame();
	if (!m_current_game)
	{
		// Do not start if replay file doesn't exist
		// TODO: maybe display error message?
		INFO_LOG(SLIPPI, "EXI_DeviceSlippi.cpp: Replay file does not exist?");
		inReplay = false;
		m_read_queue.push_back(0);
		return;
	}

	INFO_LOG(SLIPPI, "EXI_DeviceSlippi.cpp: Replay file loaded successfully!?");
	// Start the playback!
	m_read_queue.push_back(1);
}

void CEXISlippi::DMAWrite(u32 _uAddr, u32 _uSize)
{
	u8 *memPtr = Memory::GetPointer(_uAddr);

	u32 bufLoc = 0;

	u8 byte = memPtr[0];
	if (byte == CMD_RECEIVE_COMMANDS)
	{
		time(&gameStartTime); // Store game start time
		u8 receiveCommandsLen = memPtr[1];
		configureCommands(&memPtr[1], receiveCommandsLen);
		writeToFile(&memPtr[0], receiveCommandsLen + 1, "create");
		bufLoc += receiveCommandsLen + 1;
	}

	INFO_LOG(EXPANSIONINTERFACE, "EXI SLIPPI DMAWrite: addr: 0x%08x size: %d, bufLoc:[%02x %02x %02x %02x %02x]",
	         _uAddr, _uSize, memPtr[bufLoc], memPtr[bufLoc + 1], memPtr[bufLoc + 2], memPtr[bufLoc + 3],
	         memPtr[bufLoc + 4]);

	while (bufLoc < _uSize)
	{
		byte = memPtr[bufLoc];
		if (!payloadSizes.count(byte))
		{
			// This should never happen. Do something else if it does?
			return;
		}

		u32 payloadLen = payloadSizes[byte];
		switch (byte)
		{
		case CMD_RECEIVE_GAME_END:
			writeToFile(&memPtr[bufLoc], payloadLen + 1, "close");
			break;
		case CMD_PREPARE_REPLAY:
			// log.open("log.txt");
			prepareGameInfo();
			break;
		case CMD_READ_FRAME:
			prepareFrameData(&memPtr[1]);
			break;
		case CMD_IS_STOCK_STEAL:
			prepareIsStockSteal(&memPtr[1]);
			break;
		case CMD_GET_FRAME_COUNT:
			prepareFrameCount();
			break;
		case CMD_IS_FILE_READY:
			prepareIsFileReady();
			break;
		default:
			writeToFile(&memPtr[bufLoc], payloadLen + 1, "");
			break;
		}

		bufLoc += payloadLen + 1;
	}
}

void CEXISlippi::DMARead(u32 addr, u32 size)
{
	if (m_read_queue.empty())
	{
		INFO_LOG(EXPANSIONINTERFACE, "EXI SLIPPI DMARead: Empty");
		return;
	}

	auto queueAddr = &m_read_queue[0];
	INFO_LOG(EXPANSIONINTERFACE, "EXI SLIPPI DMARead: addr: 0x%08x size: %d, startResp: [%02x %02x %02x %02x %02x]",
	         addr, size, queueAddr[0], queueAddr[1], queueAddr[2], queueAddr[3], queueAddr[4]);

	// Copy buffer data to memory
	Memory::CopyToEmu(addr, queueAddr, size);
}

bool CEXISlippi::IsPresent() const
{
	return true;
}

void CEXISlippi::TransferByte(u8 &byte) {}

void CEXISlippi::SavestateThread()
{
	// state diffs keyed by frameIndex;
	static std::unordered_map<int32_t, std::string> diffsByFrame;

	// diffs are processed async
	std::vector<std::future<std::pair<int32_t, std::string>>> futureDiffs;
	std::vector<u8> iState; // The initial state
	std::vector<u8> cState; // The current (latest) state
	Common::SetCurrentThreadName("Savestate thread");

	bool haveInitialState = false;
	bool hasRestartedReplay = false;
	int mostRecentlyProcessedFrame = INT_MAX;

	isHardFFW = true;
	// SConfig::GetInstance().m_EmulationSpeed = -1.0f;
	SConfig::GetInstance().m_OCEnable = true;
	SConfig::GetInstance().m_OCFactor = 4.0f;
	std::unique_lock<std::mutex> lock(mtx);
	ThreadPoolQueue pool(10);

	open_vcdiff::VCDiffDecoder decoder;
	open_vcdiff::VCDiffEncoder *encoder = NULL;
	std::tuple<int, size_t, std::vector<u8>> diffTuple;

	while (true)
	{
		bool shouldSeek = g_inSlippiPlayback && hasRestartedReplay &&
		                  (g_shouldJumpBack || g_shouldJumpForward || g_targetFrameNum != INT_MAX);
		if (shouldSeek)
		{
			seekTargetFrameNum(isHardFFW, iState, diffsByFrame, lock, decoder);
		} else {
			if (inReplay && currentPlaybackFrame != mostRecentlyProcessedFrame)
			{
				mostRecentlyProcessedFrame = currentPlaybackFrame;

				// Processing save states diffs during initial fast forward through replay
				if (!hasProcessedSaveStates)
				{
					processSaveState(haveInitialState, iState, cState, futureDiffs, lock, pool, encoder);
				}
			}

			// When done processing save states, load the replay from beginning and turn off ffw
			if (hasProcessedSaveStates && !hasRestartedReplay && haveInitialState)
			{
				restartReplay(futureDiffs, diffsByFrame, iState, cState, isHardFFW, hasRestartedReplay);
			}

			// Make sure our state is reset when playback is pending/finished
			// if (!inReplay)
			//{
			//	if (iState.size() != 0)
			//	{
			//		INFO_LOG(SLIPPI, "Cleared slot 0");
			//		std::vector<u8>().swap(iState);
			//	}
			//	if (cState.size() != 0)
			//	{
			//		INFO_LOG(SLIPPI, "Cleared slot 1");
			//		std::vector<u8>().swap(cState);
			//	}
			//	if (statesByFrame.size() != 0)
			//	{
			//		INFO_LOG(SLIPPI, "Cleared statesByFrame (%d entries)", statesByFrame.size());
			//		std::map<int32_t, std::string>().swap(statesByFrame);
			//	}

			//	if (currentPlaybackFrame != -INT_MAX)
			//		currentPlaybackFrame = -INT_MAX;
			//	if (latestStateFrame != -INT_MAX)
			//		latestStateFrame = -INT_MAX;

			//	if (haveInitialState)
			//		haveInitialState = false;

			//	if (hasProcessedSaveStates)
			//		hasProcessedSaveStates = false;

			//	if (!isHardFFW)
			//		isHardFFW = true;
			//}
			Common::SleepCurrentThread(SLEEP_TIME_MS);
		}
	}
}

void CEXISlippi::prepareSlippiPlayback(int32_t &frameIndex)
{
	if (hasProcessedSaveStates && g_inSlippiPlayback && currentPlaybackFrame == g_targetFrameNum)
	{
		INFO_LOG(SLIPPI, "Reached frame to seek to, unblock", frameIndex);
		condVar.notify_one();
	}
	// Mark save states as done processing at end of replay
	// Thread will then reload the game at the start of the replay using save state
	if (latestFrame == frameIndex && !hasProcessedSaveStates)
	{
		INFO_LOG(SLIPPI, "Found last frame: %d", frameIndex);
		hasProcessedSaveStates = true;
		condVar.notify_one();
	}

	// Unblock thread to save a state every interval
	if (!hasProcessedSaveStates && ((currentPlaybackFrame + 123) % FRAME_INTERVAL == 0))
		condVar.notify_one();
}

void CEXISlippi::processSaveState(bool &haveInitialState, std::vector<u8> &iState, std::vector<u8> &cState,
                                  std::vector<std::future<std::pair<int32_t, std::string>>> &futureDiffs,
                                  std::unique_lock<std::mutex> &lock, ThreadPoolQueue &pool,
                                  open_vcdiff::VCDiffEncoder *&encoder)
{
	// Only save frames every so often.
	// Block until we reach one of these points, or until we are done processing
	// TODO: this is kind of dumb but this ends up permablocking if it gets to eog
	// unless I include !hasProcessedSaveStates again for the condition variable.
	// Figure out a better pattern for all these if elses?
	while ((currentPlaybackFrame + 123) % FRAME_INTERVAL != 0 && !hasProcessedSaveStates)
	{
		INFO_LOG(SLIPPI, "block");
		condVar.wait(lock);
		INFO_LOG(SLIPPI, "unblock");
	}

	// Save an initial state at the beginning of the game
	if (currentPlaybackFrame == START_FRAME && !haveInitialState)
	{
		INFO_LOG(SLIPPI, "saving iState");
		State::SaveToBuffer(iState);
		haveInitialState = true;
		encoder = new open_vcdiff::VCDiffEncoder((char *)iState.data(), iState.size());
	}
	else
	{
		// Queue up processing new diff if it doesn't exist yet
		State::SaveToBuffer(cState);
		futureDiffs.push_back(pool.enqueue(processDiff, iState, cState, currentPlaybackFrame, encoder));
	}
}

void CEXISlippi::seekTargetFrameNum(bool &isHardFFW, std::vector<u8> &iState,
                                    std::unordered_map<int32_t, std::string> &diffsByFrame, std::unique_lock<std::mutex> &lock,
                                    open_vcdiff::VCDiffDecoder &decoder)
{
	bool paused = (Core::GetState() == Core::CORE_PAUSE);
	bool pause = Core::PauseAndLock(true);

	int jumpInterval = 300; // 5 seconds;

	if (g_shouldJumpForward)
	{
		g_targetFrameNum = currentPlaybackFrame + jumpInterval;
	}
	else if (g_shouldJumpBack)
	{
		g_targetFrameNum = currentPlaybackFrame - jumpInterval;
	}

	int closestStateFrame = g_targetFrameNum - ((g_targetFrameNum + 123) % FRAME_INTERVAL);
	while (closestStateFrame > latestFrame)
	{
		closestStateFrame -= FRAME_INTERVAL;
	}

	INFO_LOG(SLIPPI, "Seek started, moving to frame: %d", g_targetFrameNum);

	if (g_targetFrameNum < START_FRAME || closestStateFrame == START_FRAME)
	{
		isHardFFW = true;
		State::LoadFromBuffer(iState);
	}
	else
	{
		std::string stateString;
		decoder.Decode((char *)iState.data(), iState.size(), diffsByFrame[closestStateFrame], &stateString);
		std::vector<u8> stateToLoad(stateString.begin(), stateString.end());
		State::LoadFromBuffer(stateToLoad);
	}

	SConfig::GetInstance().m_OCEnable = true;
	SConfig::GetInstance().m_OCFactor = 4.0f;
	isHardFFW = true;

	Core::PauseAndLock(false, pause);
	// ffw to our targetFrame
	while (currentPlaybackFrame != g_targetFrameNum)
	{
		condVar.wait(lock);
	}

	pause = Core::PauseAndLock(true);

	isHardFFW = false;
	SConfig::GetInstance().m_OCFactor = 1.0f;
	SConfig::GetInstance().m_OCEnable = false;

	g_shouldJumpBack = false;
	g_shouldJumpForward = false;
	g_targetFrameNum = INT_MAX;

	if (!paused)
	{
		Core::PauseAndLock(false, pause);
	}
	INFO_LOG(SLIPPI, "Rewind/ffw completed");
}

void CEXISlippi::restartReplay(std::vector<std::future<std::pair<int32_t, std::string>>> &futureDiffs,
                               std::unordered_map<int32_t, std::string> &diffsByFrame, std::vector<u8> &iState,
                               std::vector<u8> &cState, bool &isHardFFW, bool &hasRestartedReplay)
{
	bool pause = Core::PauseAndLock(true);

	for (auto &&futureDiff : futureDiffs)
	{
		auto diffPair = futureDiff.get();
		auto frameIndex = diffPair.first;
		auto diff = diffPair.second;
		diffsByFrame[frameIndex] = diff;
	}

	SConfig::GetInstance().m_OCFactor = 1.0f;
	SConfig::GetInstance().m_OCEnable = false;
	INFO_LOG(SLIPPI, "Finished processing. Restarting replay");
	isHardFFW = false;
	State::LoadFromBuffer(iState);
	Core::PauseAndLock(false, pause);
	hasRestartedReplay = true;
	g_inSlippiPlayback = true;
	SConfig::GetInstance().bHideCursor = false;
}