// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <SlippiGame.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Core/HW/EXI_Device.h"
#include "Core/Slippi/SlippiGameFileLoader.h"
#include "Core/Slippi/SlippiMatchmaking.h"
#include "Core/Slippi/SlippiNetplay.h"
#include "Core/Slippi/SlippiReplayComm.h"
#include "Core/Slippi/SlippiSavestate.h"
#include "Core/Slippi/SlippiSpectate.h"
#include "Core/Slippi/SlippiUser.h"

#define ROLLBACK_MAX_FRAMES 7
#define MAX_NAME_LENGTH 15
#define CONNECT_CODE_LENGTH 8

extern bool g_needInputForFrame;

// Emulated Slippi device used to receive and respond to in-game messages
class CEXISlippi : public IEXIDevice
{
  public:
	CEXISlippi();
	virtual ~CEXISlippi();

	//* Primitive de communication ASM C++
	//* DMA = Direct memory access

	//* Répond aux demandes de l'ASM
	//* A read from the ASM, a write from the ASM's perspective
	void DMAWrite(u32 _uAddr, u32 _uSize) override;

	//* A write to the ASM, a read from the ASM's perspective
	//* Passe des choses à l'ASM - dont les gamepads mais quoi d'autre ?
	void DMARead(u32 addr, u32 size) override;
	
	bool IsPresent() const override;

  private:
	//* Messages entre ASM et C++
	//* + Payload size des messages
	enum
	{
		CMD_UNKNOWN = 0x0,

		// Recording
		CMD_RECEIVE_COMMANDS = 0x35,
		CMD_RECEIVE_GAME_INFO = 0x36,
		CMD_RECEIVE_POST_FRAME_UPDATE = 0x38,
		CMD_RECEIVE_GAME_END = 0x39,
		CMD_FRAME_BOOKEND = 0x3C,
		CMD_MENU_FRAME = 0x3E,

		// Playback
		CMD_PREPARE_REPLAY = 0x75,
		CMD_READ_FRAME = 0x76,
		CMD_GET_LOCATION = 0x77,
		CMD_IS_FILE_READY = 0x88,
		CMD_IS_STOCK_STEAL = 0x89,
		CMD_GET_GECKO_CODES = 0x8A,

		// Online
		CMD_ONLINE_INPUTS = 0xB0,
		CMD_CAPTURE_SAVESTATE = 0xB1,
		CMD_LOAD_SAVESTATE = 0xB2,
		CMD_GET_MATCH_STATE = 0xB3,
		CMD_FIND_OPPONENT = 0xB4,
		CMD_SET_MATCH_SELECTIONS = 0xB5,
		CMD_OPEN_LOGIN = 0xB6,
		CMD_LOGOUT = 0xB7,
		CMD_UPDATE = 0xB8,
		CMD_GET_ONLINE_STATUS = 0xB9,
		CMD_CLEANUP_CONNECTION = 0xBA,

		// Misc
		CMD_LOG_MESSAGE = 0xD0,
		CMD_FILE_LENGTH = 0xD1,
		CMD_FILE_LOAD = 0xD2,
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

	    // The following are used for Slippi online and also have fixed sizes
	    {CMD_ONLINE_INPUTS, 17},
	    {CMD_CAPTURE_SAVESTATE, 32},
	    {CMD_LOAD_SAVESTATE, 32},
	    {CMD_GET_MATCH_STATE, 0},
	    {CMD_FIND_OPPONENT, 19},
	    {CMD_SET_MATCH_SELECTIONS, 6},
	    {CMD_OPEN_LOGIN, 0},
	    {CMD_LOGOUT, 0},
	    {CMD_UPDATE, 0},
	    {CMD_GET_ONLINE_STATUS, 0},
	    {CMD_CLEANUP_CONNECTION, 0},

	    // Misc
	    {CMD_LOG_MESSAGE, 0xFFFF}, // Variable size... will only work if by itself
	    {CMD_FILE_LENGTH, 0x40},
	    {CMD_FILE_LOAD, 0x40},
	};

	struct WriteMessage
	{
		std::vector<u8> data;
		std::string operation;
	};

	// .slp File creation stuff
	u32 writtenByteCount = 0;

	// cout stuff
	bool outputCurrentFrame = false;
	bool shouldOutput = false;

	// vars for metadata generation
	time_t gameStartTime;
	s32 lastFrame;
	std::unordered_map<u8, std::unordered_map<u8, u32>> characterUsage;

	void updateMetadataFields(u8 *payload, u32 length);
	void configureCommands(u8 *payload, u8 length);
	void writeToFileAsync(u8 *payload, u32 length, std::string fileOption);
	void writeToFile(std::unique_ptr<WriteMessage> msg);
	std::vector<u8> generateMetadata();
	void createNewFile();
	void closeFile();
	std::string generateFileName();
	bool checkFrameFullyFetched(s32 frameIndex);
	bool shouldFFWFrame(s32 frameIndex);

	// std::ofstream log;

	File::IOFile m_file;
	std::vector<u8> m_payload;

	// online play stuff

	// Ensures randomly selected stage go through all 6 once then resets the pool
	u16 getRandomStage();
	
	// Wrapper for (not in netplay) or (disconnected according to the slippiNetplay)
	bool isDisconnected();

	/* CMD_ONLINE_INPUTS triggered
	 First four bits of payload = frame
	 Cas particulier frame 1 (initialisation)
	 Cas particulier should skip online frame
	
	handleSendInputs(payload);
	prepareOpponentInputs(payload);*/
	void handleOnlineInputs(u8 *payload);

	// If we haven't disconnected
	// Compute frame from payload - which payload ? Ours I assume ?
	// slippi_netplay->GetSlippiRemotePad(frame)
	// CALLS GetSlippiRemotePad o_O Why is it called prepare then
	void prepareOpponentInputs(u8 *payload);
	
	/*int32_t frame = payload[0] << 24 | payload[1] << 16 | payload[2] << 8 | payload[3];
	u8 delay = payload[4];
	auto pad = std::make_unique<SlippiPad>(frame + delay, &payload[5]);
	slippi_netplay->SendSlippiPad(std::move(pad));*/
	// On envoie nous même la frame que l'autre utilisera avec LEUR delay ? Bizarre... Le payload est fourni par Melee je suppose ?
	// Le pad aussi (pas illogique en fait)
	// Mais ça ne nous arrange guère
	void handleSendInputs(u8 *payload);


	void handleCaptureSavestate(u8 *payload);
	void handleLoadSavestate(u8 *payload);
	void startFindMatch(u8 *payload);
	void prepareOnlineMatchState();
	void setMatchSelections(u8 *payload);

	/*Non si connection hs
	Si pas d'info de l'adversaire depuis 7 frames, stall une frame
	Si on stall à cause de ça depuis 7 secondes, se déconnecter
	Toutes les 30 frames, timesync
	Timesync:
	L'un des joueurs est-il devant de plus de 0.6FL ?
	A voir comment ce offsetUs est il calculé
	auto offsetUs = slippi_netplay->CalcTimeOffsetUs()
	Si oui attendre 1 (ou jusqu'à 5, [n+0.6,n+1.6]=>n si dans les 120 premières frames)*/
	bool shouldSkipOnlineFrame(s32 frame);

	void handleLogInRequest();
	void handleLogOutRequest();
	void handleUpdateAppRequest();
	void prepareOnlineStatus();
	void handleConnectionCleanup();

	// replay playback stuff
	void prepareGameInfo(u8 *payload);
	void prepareGeckoList();
	void prepareCharacterFrameData(Slippi::FrameData *frame, u8 port, u8 isFollower);
	void prepareFrameData(u8 *payload);
	void prepareIsStockSteal(u8 *payload);
	void prepareIsFileReady();

	// misc stuff
	void logMessageFromGame(u8 *payload);
	void prepareFileLength(u8 *payload);
	void prepareFileLoad(u8 *payload);

	void FileWriteThread(void);

	Common::FifoQueue<std::unique_ptr<WriteMessage>, false> fileWriteQueue;
	bool writeThreadRunning = false;
	std::thread m_fileWriteThread;

	std::unordered_map<u8, std::string> getNetplayNames();

	std::vector<u8> playbackSavestatePayload;
	std::vector<u8> geckoList;

	//* Décompte des frames à stall dû à un décalage de timing entre les 2 émulateurs
	u32 stallFrameCount = 0; 

	//* A-t'on attendu pendant 7 secondes des informations de gamepad de l'adversaire en vain ? Si oui, déco et
	//isConnectionStalled=true
	bool isConnectionStalled = false;

	//* La structure contenant ce qui est passé au système de rollback ASM
	//* ce qui se fait avec DMARead (du point de Dolphin, une write queue, donc)
	//* "frame result" 1 = continue ; 2 = stall ; 3 = disconnect
	std::vector<u8> m_read_queue;

	std::unique_ptr<Slippi::SlippiGame> m_current_game = nullptr;
	SlippiSpectateServer *m_slippiserver = nullptr;
	SlippiMatchmaking::MatchSearchSettings lastSearch;

	//* Utilisé pour itérer à travers tous les stages une fois
	std::vector<u16> stagePool; 

	u32 frameSeqIdx = 0;

	bool isEnetInitialized = false;

	std::default_random_engine generator;

	// Frame skipping variables
	int framesToSkip = 0;
	bool isCurrentlySkipping = false;

	std::string forcedError = "";

  protected:
	void TransferByte(u8 &byte) override;

  private:
	SlippiPlayerSelections localSelections;

	std::unique_ptr<SlippiUser> user;
	std::unique_ptr<SlippiGameFileLoader> gameFileLoader;
	std::unique_ptr<SlippiNetplayClient> slippi_netplay;
	std::unique_ptr<SlippiMatchmaking> matchmaking;

	std::map<s32, std::unique_ptr<SlippiSavestate>> activeSavestates;
	std::deque<std::unique_ptr<SlippiSavestate>> availableSavestates;
};
