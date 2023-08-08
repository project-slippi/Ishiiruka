// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <locale>
#include <mutex>
#include <ostream>
#include <set>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/ConsoleListener.h"
#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"

#include "SlippiRustExtensions.h"

void GenericLog(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char *file, int line, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	if (LogManager::GetInstance())
		LogManager::GetInstance()->Log(level, type, file, line, fmt, args);
	va_end(args);
}

// See the notes in the header definition for why this exists.
void SlippiRustExtensionsLogger(int level, int slp_log_type, const char *msg)
{
	LogTypes::LOG_LEVELS log_level = static_cast<LogTypes::LOG_LEVELS>(level);
	LogTypes::LOG_TYPE log_type = static_cast<LogTypes::LOG_TYPE>(slp_log_type);

	if (LogManager::GetInstance())
		LogManager::GetInstance()->LogPreformatted(log_level, log_type, msg);
}

LogManager *LogManager::m_logManager = nullptr;

static size_t DeterminePathCutOffPoint()
{
	constexpr const char *pattern = "/source/core/";
#ifdef _WIN32
	constexpr const char *pattern2 = "\\source\\core\\";
#endif
	std::string path = __FILE__;
	std::transform(path.begin(), path.end(), path.begin(),
	               [](char c) { return std::tolower(c, std::locale::classic()); });
	size_t pos = path.find(pattern);
#ifdef _WIN32
	if (pos == std::string::npos)
		pos = path.find(pattern2);
#endif
	if (pos != std::string::npos)
		return pos + strlen(pattern);
	return 0;
}

LogManager::LogManager()
{
	// We want this called before we create any `LogContainer`s below that may register with the Rust
	// side of things.
	slprs_logging_init(SlippiRustExtensionsLogger);

	// create log containers
	m_Log[LogTypes::ACTIONREPLAY] = new LogContainer("ActionReplay", "ActionReplay", LogTypes::ACTIONREPLAY);
	m_Log[LogTypes::AUDIO] = new LogContainer("Audio", "Audio Emulator", LogTypes::AUDIO);
	m_Log[LogTypes::AUDIO_INTERFACE] = new LogContainer("AI", "Audio Interface (AI)", LogTypes::AUDIO_INTERFACE);
	m_Log[LogTypes::BOOT] = new LogContainer("BOOT", "Boot", LogTypes::BOOT);
	m_Log[LogTypes::COMMANDPROCESSOR] = new LogContainer("CP", "CommandProc", LogTypes::COMMANDPROCESSOR);
	m_Log[LogTypes::COMMON] = new LogContainer("COMMON", "Common", LogTypes::COMMON);
	m_Log[LogTypes::CONSOLE] = new LogContainer("CONSOLE", "Dolphin Console", LogTypes::CONSOLE);
	m_Log[LogTypes::DISCIO] = new LogContainer("DIO", "Disc IO", LogTypes::DISCIO);
	m_Log[LogTypes::DSPHLE] = new LogContainer("DSPHLE", "DSP HLE", LogTypes::DSPHLE);
	m_Log[LogTypes::DSPLLE] = new LogContainer("DSPLLE", "DSP LLE", LogTypes::DSPLLE);
	m_Log[LogTypes::DSP_MAIL] = new LogContainer("DSPMails", "DSP Mails", LogTypes::DSP_MAIL);
	m_Log[LogTypes::DSPINTERFACE] = new LogContainer("DSP", "DSPInterface", LogTypes::DSPINTERFACE);
	m_Log[LogTypes::DVDINTERFACE] = new LogContainer("DVD", "DVD Interface", LogTypes::DVDINTERFACE);
	m_Log[LogTypes::DYNA_REC] = new LogContainer("JIT", "Dynamic Recompiler", LogTypes::DYNA_REC);
	m_Log[LogTypes::EXPANSIONINTERFACE] = new LogContainer("EXI", "Expansion Interface", LogTypes::EXPANSIONINTERFACE);
	m_Log[LogTypes::SLIPPI] = new LogContainer("SLIPPI", "Slippi", LogTypes::SLIPPI);
	m_Log[LogTypes::SLIPPI_ONLINE] = new LogContainer("SLIPPI_ONLINE", "Slippi Online", LogTypes::SLIPPI_ONLINE);

	// This LogContainer will register with the Rust side under the "SLIPPI_RUST_DEPENDENCIES" target.
	// This is intended to be a catch-all for situations where we want to inspect logs from dependencies
	// we pull in.
	m_Log[LogTypes::SLIPPI_RUST_DEPENDENCIES] = new LogContainer(
	    "SLIPPI_RUST_DEPENDENCIES", "[Rust] Slippi Dependencies", LogTypes::SLIPPI_RUST_DEPENDENCIES, true);

	// This LogContainer will register with the Rust side under the "SLIPPI_RUST_EXI" target.
	m_Log[LogTypes::SLIPPI_RUST_EXI] =
	    new LogContainer("SLIPPI_RUST_EXI", "[Rust] Slippi EXI", LogTypes::SLIPPI_RUST_EXI, true);

	// This LogContainer will register with the Rust side under the "SLIPPI_RUST_GAME_REPORTER" target.
	m_Log[LogTypes::SLIPPI_RUST_GAME_REPORTER] = new LogContainer(
	    "SLIPPI_RUST_GAME_REPORTER", "[Rust] Slippi Game Reporter", LogTypes::SLIPPI_RUST_GAME_REPORTER, true);

	// This LogContainer will register with the Rust side under the "SLIPPI_RUST_JUKEBOX" target.
	m_Log[LogTypes::SLIPPI_RUST_JUKEBOX] =
	    new LogContainer("SLIPPI_RUST_JUKEBOX", "[Rust] Slippi Jukebox", LogTypes::SLIPPI_RUST_JUKEBOX, true);

	m_Log[LogTypes::FILEMON] = new LogContainer("FileMon", "File Monitor", LogTypes::FILEMON);
	m_Log[LogTypes::GDB_STUB] = new LogContainer("GDB_STUB", "GDB Stub", LogTypes::GDB_STUB);
	m_Log[LogTypes::GPFIFO] = new LogContainer("GP", "GPFifo", LogTypes::GPFIFO);
	m_Log[LogTypes::HOST_GPU] = new LogContainer("Host GPU", "Host GPU", LogTypes::HOST_GPU);
	m_Log[LogTypes::MASTER_LOG] = new LogContainer("*", "Master Log", LogTypes::MASTER_LOG);
	m_Log[LogTypes::MEMCARD_MANAGER] =
	    new LogContainer("MemCard Manager", "MemCard Manager", LogTypes::MEMCARD_MANAGER);
	m_Log[LogTypes::MEMMAP] = new LogContainer("MI", "MI & memmap", LogTypes::MEMMAP);
	m_Log[LogTypes::NETPLAY] = new LogContainer("NETPLAY", "Netplay", LogTypes::NETPLAY);
	m_Log[LogTypes::OSHLE] = new LogContainer("HLE", "HLE", LogTypes::OSHLE);
	m_Log[LogTypes::OSREPORT] = new LogContainer("OSREPORT", "OSReport", LogTypes::OSREPORT);
	m_Log[LogTypes::PAD] = new LogContainer("PAD", "Pad", LogTypes::PAD);
	m_Log[LogTypes::PIXELENGINE] = new LogContainer("PE", "PixelEngine", LogTypes::PIXELENGINE);
	m_Log[LogTypes::PROCESSORINTERFACE] = new LogContainer("PI", "ProcessorInt", LogTypes::PROCESSORINTERFACE);
	m_Log[LogTypes::POWERPC] = new LogContainer("PowerPC", "IBM CPU", LogTypes::POWERPC);
	m_Log[LogTypes::SERIALINTERFACE] = new LogContainer("SI", "Serial Interface (SI)", LogTypes::SERIALINTERFACE);
	m_Log[LogTypes::SP1] = new LogContainer("SP1", "Serial Port 1", LogTypes::SP1);
	m_Log[LogTypes::VIDEO] = new LogContainer("Video", "Video Backend", LogTypes::VIDEO);
	m_Log[LogTypes::VIDEOINTERFACE] = new LogContainer("VI", "Video Interface (VI)", LogTypes::VIDEOINTERFACE);
	m_Log[LogTypes::WIIMOTE] = new LogContainer("Wiimote", "Wiimote", LogTypes::WIIMOTE);
	m_Log[LogTypes::WII_IPC] = new LogContainer("WII_IPC", "WII IPC", LogTypes::WII_IPC);
	m_Log[LogTypes::WII_IPC_DVD] = new LogContainer("WII_IPC_DVD", "WII IPC DVD", LogTypes::WII_IPC_DVD);
	m_Log[LogTypes::WII_IPC_ES] = new LogContainer("WII_IPC_ES", "WII IPC ES", LogTypes::WII_IPC_ES);
	m_Log[LogTypes::WII_IPC_FILEIO] = new LogContainer("WII_IPC_FILEIO", "WII IPC FILEIO", LogTypes::WII_IPC_FILEIO);
	m_Log[LogTypes::WII_IPC_HID] = new LogContainer("WII_IPC_HID", "WII IPC HID", LogTypes::WII_IPC_HID);
	m_Log[LogTypes::WII_IPC_HLE] = new LogContainer("WII_IPC_HLE", "WII IPC HLE", LogTypes::WII_IPC_HLE);
	m_Log[LogTypes::WII_IPC_SD] = new LogContainer("WII_IPC_SD", "WII IPC SD", LogTypes::WII_IPC_SD);
	m_Log[LogTypes::WII_IPC_SSL] = new LogContainer("WII_IPC_SSL", "WII IPC SSL", LogTypes::WII_IPC_SSL);
	m_Log[LogTypes::WII_IPC_STM] = new LogContainer("WII_IPC_STM", "WII IPC STM", LogTypes::WII_IPC_STM);
	m_Log[LogTypes::WII_IPC_NET] = new LogContainer("WII_IPC_NET", "WII IPC NET", LogTypes::WII_IPC_NET);
	m_Log[LogTypes::WII_IPC_WC24] = new LogContainer("WII_IPC_WC24", "WII IPC WC24", LogTypes::WII_IPC_WC24);
	m_Log[LogTypes::WII_IPC_WIIMOTE] =
	    new LogContainer("WII_IPC_WIIMOTE", "WII IPC WIIMOTE", LogTypes::WII_IPC_WIIMOTE);

	RegisterListener(LogListener::FILE_LISTENER, new FileLogListener(File::GetUserPath(F_MAINLOG_IDX)));
	RegisterListener(LogListener::CONSOLE_LISTENER, new ConsoleListener());

	IniFile ini;
	ini.Load(File::GetUserPath(F_LOGGERCONFIG_IDX));
	IniFile::Section *logs = ini.GetOrCreateSection("Logs");
	IniFile::Section *options = ini.GetOrCreateSection("Options");
	bool write_file;
	bool write_console;
	options->Get("WriteToFile", &write_file, false);
	options->Get("WriteToConsole", &write_console, true);

	for (LogContainer *container : m_Log)
	{
		bool enable;
		logs->Get(container->GetShortName(), &enable, false);
		container->SetEnable(enable);
		if (enable && write_file)
			container->AddListener(LogListener::FILE_LISTENER);
		if (enable && write_console)
			container->AddListener(LogListener::CONSOLE_LISTENER);
	}

	m_path_cutoff_point = DeterminePathCutOffPoint();
}

LogManager::~LogManager()
{
	for (LogContainer *container : m_Log)
		delete container;

	// The log window listener pointer is owned by the GUI code.
	delete m_listeners[LogListener::CONSOLE_LISTENER];
	delete m_listeners[LogListener::FILE_LISTENER];
}

// Extensions that need to log across the boundary often have to allocate
// an owned String on their side; if they can vend us a c_str then we can avoid
// duplicating the allocation over here for the logger.
//
// The alternative here would be opening up `m_log` and `m_listeners` to be public,
// but this feels like it'll transplant easier onto mainline.
void LogManager::LogPreformatted(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char *msg)
{
	LogContainer *log = m_Log[type];

	if (!log->IsEnabled() || level > log->GetLevel() || !log->HasListeners())
		return;

	for (auto listener_id : *log)
		m_listeners[listener_id]->Log(level, msg);
}

void LogManager::Log(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char *file, int line,
                     const char *format, va_list args)
{
	char temp[MAX_MSGLEN];
	LogContainer *log = m_Log[type];

	if (!log->IsEnabled() || level > log->GetLevel() || !log->HasListeners())
		return;

	CharArrayFromFormatV(temp, MAX_MSGLEN, format, args);

	const char *path_to_print = file + m_path_cutoff_point;

	std::string msg =
	    StringFromFormat("%s %s:%u %c[%s]: %s\n", Common::Timer::GetTimeFormatted().c_str(), path_to_print, line,
	                     LogTypes::LOG_LEVEL_TO_CHAR[(int)level], log->GetShortName().c_str(), temp);

	for (auto listener_id : *log)
		m_listeners[listener_id]->Log(level, msg.c_str());
}

void LogManager::Init()
{
	m_logManager = new LogManager();
}

void LogManager::Shutdown()
{
	delete m_logManager;
	m_logManager = nullptr;
}

LogContainer::LogContainer(const std::string &shortName, const std::string &fullName, LogTypes::LOG_TYPE logtype,
                           bool isRustLog, bool enable)
    : m_fullName(fullName)
    , m_shortName(shortName)
    , m_logtype(logtype)
    , m_isRustLog(isRustLog)
    , m_enable(enable)
    , m_level(LogTypes::LWARNING)
{
	if (m_isRustLog)
	{
		slprs_logging_register_container(m_shortName.c_str(), m_logtype, m_enable, m_level);
	}
}

void LogContainer::SetEnable(bool enable)
{
	m_enable = enable;

	if (m_isRustLog)
	{
		slprs_logging_update_container(m_shortName.c_str(), m_enable, m_level);
	}
}

void LogContainer::SetLevel(LogTypes::LOG_LEVELS level)
{
	m_level = level;

	if (m_isRustLog)
	{
		slprs_logging_update_container(m_shortName.c_str(), m_enable, m_level);
	}
}

FileLogListener::FileLogListener(const std::string &filename)
{
	OpenFStream(m_logfile, filename, std::ios::app);
	SetEnable(true);
}

void FileLogListener::Log(LogTypes::LOG_LEVELS, const char *msg)
{
	if (!IsEnabled() || !IsValid())
		return;

	std::lock_guard<std::mutex> lk(m_log_lock);
	m_logfile << msg << std::flush;
}
