#include "SlippiGame.h"

namespace Slippi {

	//**********************************************************************
	//*                         Event Handlers
	//**********************************************************************
	//The read operators will read a value and increment the index so the next read will read in the correct location
	uint8_t readByte(uint8_t* a, int& idx) {
		return a[idx++];
	}

	uint16_t readHalf(uint8_t* a, int& idx) {
		uint16_t value = a[idx] << 8 | a[idx + 1];
		idx += 2;
		return value;
	}

	uint32_t readWord(uint8_t* a, int& idx) {
		uint32_t value = a[idx] << 24 | a[idx + 1] << 16 | a[idx + 2] << 8 | a[idx + 3];
		idx += 4;
		return value;
	}

	float readFloat(uint8_t* a, int& idx) {
		uint32_t bytes = readWord(a, idx);
		return *(float*)(&bytes);
	}

	void handleGameInit(Game* game) {
		int idx = 0;

		// Read version number
		for (int i = 0; i < 4; i++) {
			game->version[i] = readByte(data, idx);
		}

		// Read entire game info header
		for (int i = 0; i < GAME_INFO_HEADER_SIZE; i++) {
			game->settings.header[i] = readWord(data, idx);
		}

		// Load random seed
		game->settings.randomSeed = readWord(data, idx);

		// Read UCF toggle bytes
		bool shouldRead = game->version[0] >= 1;
		for (int i = 0; i < UCF_TOGGLE_SIZE; i++) {
			uint32_t value = shouldRead ? readWord(data, idx) : 0;
			game->settings.ucfToggles[i] = value;
		}

		// Pull header data into struct
		int player1Pos = 24; // This is the index of the first players character info
		std::array<uint32_t, Slippi::GAME_INFO_HEADER_SIZE> gameInfoHeader = game->settings.header;
		for (int i = 0; i < 4; i++) {
			// this is the position in the array that this player's character info is stored
			int pos = player1Pos + (9 * i);

			uint32_t playerInfo = gameInfoHeader[pos];
			uint8_t playerType = (playerInfo & 0x00FF0000) >> 16;
			if (playerType == 0x3) {
				// Player type 3 is an empty slot
				continue;
			}

			PlayerSettings* p = new PlayerSettings();
			p->controllerPort = i;
			p->characterId = playerInfo >> 24;
			p->playerType = playerType;
			p->characterColor = playerInfo & 0xFF;

			//Add player settings to result
			game->settings.players[p->controllerPort] = *p;
		}

		game->settings.stage = gameInfoHeader[3] & 0xFFFF;
	}

	void handlePreFrameUpdate(Game* game) {
		int idx = 0;

		//Check frame count
		int32_t frameCount = readWord(data, idx);
		game->frameCount = frameCount;

		FrameData* frame = new FrameData();
		if (game->frameData.count(frameCount)) {
			// If this frame already exists, this is probably another player
			// in this frame, so let's fetch it.
			frame = &game->frameData[frameCount];
		}

		frame->frame = frameCount;

		PlayerFrameData* p = new PlayerFrameData();

		uint8_t playerSlot = readByte(data, idx);
		uint8_t isFollower = readByte(data, idx);

		//Load random seed for player frame update
		p->randomSeed = readWord(data, idx);

		//Load player data
		p->animation = readHalf(data, idx);
		p->locationX = readFloat(data, idx);
		p->locationY = readFloat(data, idx);
		p->facingDirection = readFloat(data, idx);

		//Controller information
		p->joystickX = readFloat(data, idx);
		p->joystickY = readFloat(data, idx);
		p->cstickX = readFloat(data, idx);
		p->cstickY = readFloat(data, idx);
		p->trigger = readFloat(data, idx);
		p->buttons = readWord(data, idx);

		//Raw controller information
		p->physicalButtons = readHalf(data, idx);
		p->lTrigger = readFloat(data, idx);
		p->rTrigger = readFloat(data, idx);

		if (asmEvents[EVENT_PRE_FRAME_UPDATE] >= 59) {
			p->joystickXRaw = readByte(data, idx);
		}
		
		// Add player data to frame
		std::unordered_map<uint8_t, PlayerFrameData>* target;
		target = isFollower ? &frame->followers : &frame->players;

		// Set the player data for the player or follower
		target->operator[](playerSlot) = *p;

		// Add frame to game
		game->frameData[frameCount] = *frame;

		// Check if a player started as sheik and update
		if (frameCount == GAME_FIRST_FRAME && p->internalCharacterId == GAME_SHEIK_INTERNAL_ID) {
			game->settings.players[playerSlot].characterId = GAME_SHEIK_EXTERNAL_ID;
		}
	}

	void handlePostFrameUpdate(Game* game) {
		int idx = 0;

		//Check frame count
		int32_t frameCount = readWord(data, idx);

		FrameData* frame = new FrameData();
		if (game->frameData.count(frameCount)) {
			// If this frame already exists, this is probably another player
			// in this frame, so let's fetch it.
			frame = &game->frameData[frameCount];
		}

		uint8_t playerSlot = readByte(data, idx);
		uint8_t isFollower = readByte(data, idx);

		PlayerFrameData* p = isFollower ? &frame->followers[playerSlot] : &frame->players[playerSlot];

		p->internalCharacterId = readByte(data, idx);

		// Check if a player started as sheik and update
		if (frameCount == GAME_FIRST_FRAME && p->internalCharacterId == GAME_SHEIK_INTERNAL_ID) {
			game->settings.players[playerSlot].characterId = GAME_SHEIK_EXTERNAL_ID;
		}
	}

	void handleGameEnd(Game* game) {
		int idx = 0;

		game->winCondition = readByte(data, idx);
	}

	// This function gets the position where the raw data starts
	int getRawDataPosition(std::ifstream* f) {
		char buffer[2];
		f->seekg(0, std::ios::beg);
		f->read(buffer, 2);

		if (buffer[0] == 0x36) {
			return 0;
		}

		if (buffer[0] != '{') {
			// TODO: Do something here to cause an error
			return 0;
		}

		// TODO: Read ubjson file to find the "raw" element and return the start of it
		// TODO: For now since raw is the first element the data will always start at 15
		return 15;
	}

	uint32_t getRawDataLength(std::ifstream* f, int position, int fileSize) {
		if (position == 0) {
			return fileSize;
		}

		char buffer[4];
		f->seekg(position - 4, std::ios::beg);
		f->read(buffer, 4);

		uint8_t* byteBuf = (uint8_t*)&buffer[0];
		uint32_t length = byteBuf[0] << 24 | byteBuf[1] << 16 | byteBuf[2] << 8 | byteBuf[3];
		return length;
	}

	std::unordered_map<uint8_t, uint32_t> getMessageSizes(std::ifstream* f, int position) {
		char buffer[2];
		f->seekg(position, std::ios::beg);
		f->read(buffer, 2);
		if (buffer[0] != EVENT_PAYLOAD_SIZES) {
			return {};
		}

		int payloadLength = buffer[1];
		std::unordered_map<uint8_t, uint32_t> messageSizes = {
			{ EVENT_PAYLOAD_SIZES, payloadLength }
		};
		
		std::vector<char> messageSizesBuffer(payloadLength - 1);
		f->read(&messageSizesBuffer[0], payloadLength - 1);
		for (int i = 0; i < payloadLength - 1; i += 3) {
			uint8_t command = messageSizesBuffer[i];
			uint16_t size = messageSizesBuffer[i + 1] << 8 | messageSizesBuffer[i + 2];
			messageSizes[command] = size;
		}

		return messageSizes;
	}

	SlippiGame* SlippiGame::processFile(uint8_t* rawData, uint64_t rawDataLength) {
		SlippiGame* result = new SlippiGame();
		result->game = new Game();

		// Iterate through the data and process frames
		for (int i = 0; i < rawDataLength; i++) {
			int code = rawData[i];
			int msgLength = asmEvents[code];
			if (!msgLength) {
				return nullptr;
			}

			data = &rawData[i + 1];
			switch (code) {
			case EVENT_GAME_INIT:
				handleGameInit(result->game);
				break;
			case EVENT_PRE_FRAME_UPDATE:
				handlePreFrameUpdate(result->game);
				break;
			case EVENT_POST_FRAME_UPDATE:
				handlePostFrameUpdate(result->game);
				break;
			case EVENT_GAME_END:
				handleGameEnd(result->game);
				break;
			}
			i += msgLength;
		}

		return result;
	}

	SlippiGame* SlippiGame::FromFile(std::string path) {
		std::ifstream file(path, std::ios::in|std::ios::binary|std::ios::ate);
		if (!file.is_open()) {
			return nullptr;
		}

		int fileLength = (int)file.tellg();
		int rawDataPos = getRawDataPosition(&file);
		uint32_t rawDataLength = getRawDataLength(&file, rawDataPos, fileLength);
		asmEvents = getMessageSizes(&file, rawDataPos);

		std::vector<char> rawData(rawDataLength);
		file.seekg(rawDataPos, std::ios::beg);
		file.read(&rawData[0], rawDataLength);

		SlippiGame* result = processFile((uint8_t*)&rawData[0], rawDataLength);

		return result;
	}

	bool SlippiGame::DoesFrameExist(int32_t frame) {
		return (bool)game->frameData.count(frame);
	}

	FrameData* SlippiGame::GetFrame(int32_t frame) {
		// Get the frame we want
		return &game->frameData.at(frame);
	}

	GameSettings* SlippiGame::GetSettings() {
		return &game->settings;
	}

	bool SlippiGame::DoesPlayerExist(int8_t port) {
		return game->settings.players.find(port) != game->settings.players.end();
	}
}