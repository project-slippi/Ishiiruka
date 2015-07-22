// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Atomic.h"
#include "Common/ChunkFile.h"
#include "Common/CPUDetect.h"
#include "Common/FPURoundMode.h"
#include "Common/MemoryUtil.h"
#include "Common/Thread.h"

#include "Core/Core.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"

#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/Fifo.h"
#ifdef _WIN32
#include "VideoCommon/HLSLCompiler.h"
#endif
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/OpcodeDecodingSC.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/VideoConfig.h"

bool g_bSkipCurrentFrame = false;
DataReader g_VideoData;

static volatile bool GpuRunningState = false;
static volatile bool EmuRunningState = false;
static std::mutex m_csHWVidOccupied;
// STATE_TO_SAVE
static u8 *s_video_buffer;
static size_t s_video_buffer_size = 0;
static u8 *s_video_buffer_SC;
static size_t s_video_buffer_size_SC = 0;

volatile u32 CPReadFifoSC;
volatile u32 CPBaseFifoSC;
volatile u32 CPEndFifoSC;

void Fifo_DoState(PointerWrap &p)
{
	p.DoArray(s_video_buffer, FIFO_SIZE);
	p.Do(s_video_buffer_size);
	u8* videodata = const_cast<u8*>(g_VideoData.GetReadPosition());
	p.DoPointer(videodata, s_video_buffer);
	g_VideoData.SetReadPosition(videodata);
	p.Do(g_bSkipCurrentFrame);
	p.DoArray(s_video_buffer_SC, FIFO_SIZE);
	p.Do(s_video_buffer_size_SC);
	videodata = const_cast<u8*>(g_VideoDataSC.GetReadPosition());
	p.DoPointer(videodata, s_video_buffer_SC);
	g_VideoDataSC.SetReadPosition(videodata);
	p.Do(CPReadFifoSC);
	p.Do(CPBaseFifoSC);
	p.Do(CPEndFifoSC);
}

void Fifo_PauseAndLock(bool doLock, bool unpauseOnUnlock)
{
	if (doLock)
	{
		EmulatorState(false);
		if (!Core::IsGPUThread())
			m_csHWVidOccupied.lock();
		_dbg_assert_(COMMON, !CommandProcessor::fifo.isGpuReadingData);
	}
	else
	{
		if (unpauseOnUnlock)
			EmulatorState(true);
		if (!Core::IsGPUThread())
			m_csHWVidOccupied.unlock();
	}
}


void Fifo_Init()
{
	s_video_buffer = (u8*)AllocateMemoryPages(FIFO_SIZE + 16);
	s_video_buffer_size = 0;
	GpuRunningState = false;
	Common::AtomicStore(CommandProcessor::VITicks, CommandProcessor::m_cpClockOrigin);
	s_video_buffer_SC = (u8*)AllocateMemoryPages(FIFO_SIZE + 16);
	s_video_buffer_size_SC = 0;
	CPReadFifoSC = -1;
	CPBaseFifoSC = -1;
	CPEndFifoSC = -1;
}

void Fifo_Shutdown()
{
	if (GpuRunningState) PanicAlert("Fifo shutting down while active");
	FreeMemoryPages(s_video_buffer, FIFO_SIZE + 16);
	FreeMemoryPages(s_video_buffer_SC, FIFO_SIZE + 16);
	s_video_buffer = nullptr;
	s_video_buffer_SC = nullptr;
}

u8* GetVideoBufferStartPtr()
{
	return s_video_buffer;
}

u8* GetVideoBufferEndPtr()
{
	return &s_video_buffer[s_video_buffer_size];
}

u8* GetVideoBufferStartPtrSC()
{
	return s_video_buffer_SC;
}

u8* GetVideoBufferEndPtrSC()
{
	return &s_video_buffer_SC[s_video_buffer_size_SC];
}


void Fifo_SetRendering(bool enabled)
{
	g_bSkipCurrentFrame = !enabled;
}

// May be executed from any thread, even the graphics thread.
// Created to allow for self shutdown.
void ExitGpuLoop()
{
	// This should break the wait loop in CPU thread
	CommandProcessor::fifo.bFF_GPReadEnable = false;
	SCPFifoStruct &fifo = CommandProcessor::fifo;
	while (fifo.isGpuReadingData) Common::YieldCPU();
	// Terminate GPU thread loop
	GpuRunningState = false;
	EmuRunningState = true;
}

void EmulatorState(bool running)
{
	EmuRunningState = running;
}


// Description: RunGpuLoop() sends data through this function.
void ReadDataFromFifo(u8* _uData)
{
	size_t len = 32;
	if (s_video_buffer_size + len >= FIFO_SIZE)
	{
		size_t pos = g_VideoData.GetReadPosition() - s_video_buffer;
		s_video_buffer_size -= pos;
		if (s_video_buffer_size + len > FIFO_SIZE)
		{
			PanicAlert("FIFO out of bounds (s_video_buffer_size = %i, len = %i at %08x)", s_video_buffer_size, len, pos);
		}
		memmove(s_video_buffer, s_video_buffer + pos, s_video_buffer_size);
		g_VideoData.SetReadPosition(s_video_buffer);
	}
	// Copy new video instructions to s_video_buffer for future use in rendering the new picture
	memcpy(s_video_buffer + s_video_buffer_size, _uData, len);
	s_video_buffer_size += len;
}

void ResetVideoBuffer()
{
	g_VideoData.SetReadPosition(s_video_buffer);
	s_video_buffer_size = 0;
	g_VideoDataSC.SetReadPosition(s_video_buffer_SC);
	s_video_buffer_size_SC = 0;

	CPReadFifoSC = -1;
	CPBaseFifoSC = -1;
	CPEndFifoSC = -1;
}


// Description: Main FIFO update loop
// Purpose: Keep the Core HW updated about the CPU-GPU distance
void RunGpuLoop()
{
	std::lock_guard<std::mutex> lk(m_csHWVidOccupied);
	GpuRunningState = true;
	SCPFifoStruct &fifo = CommandProcessor::fifo;
	u32 cyclesExecuted = 0;

	// If the host CPU has only two cores, idle loop instead of busy loop
	// This allows a system that we are maxing out in dual core mode to do other things
	bool yield_cpu = cpu_info.num_cores <= 2;

	AsyncRequests::GetInstance()->SetEnable(true);
	AsyncRequests::GetInstance()->SetPassthrough(false);

	while (GpuRunningState)
	{
		g_video_backend->PeekMessages();

		AsyncRequests::GetInstance()->PullEvents();

		CommandProcessor::SetCpStatus();

		Common::AtomicStore(CommandProcessor::VITicks, CommandProcessor::m_cpClockOrigin);

		// check if we are able to run this buffer	
		while (GpuRunningState && EmuRunningState && !CommandProcessor::interruptWaiting && fifo.bFF_GPReadEnable && fifo.CPReadWriteDistance && !AtBreakpoint())
		{
			fifo.isGpuReadingData = true;
			CommandProcessor::isPossibleWaitingSetDrawDone = fifo.bFF_GPLinkEnable ? true : false;

			if (!SConfig::GetInstance().bSyncGPU || Common::AtomicLoad(CommandProcessor::VITicks) > CommandProcessor::m_cpClockOrigin)
			{
				u32 readPtr = fifo.CPReadPointer;
				u8 *uData = Memory::GetPointer(readPtr);

				if (readPtr == fifo.CPEnd)
					readPtr = fifo.CPBase;
				else
					readPtr += 32;

				_assert_msg_(COMMANDPROCESSOR, (s32)fifo.CPReadWriteDistance - 32 >= 0,
					"Negative fifo.CPReadWriteDistance = %i in FIFO Loop !\nThat can produce instability in the game. Please report it.", fifo.CPReadWriteDistance - 32);

				ReadDataFromFifo(uData);

				cyclesExecuted = OpcodeDecoder_Run(GetVideoBufferEndPtr());

				if (SConfig::GetInstance().bSyncGPU && Common::AtomicLoad(CommandProcessor::VITicks) > cyclesExecuted)
					Common::AtomicAdd(CommandProcessor::VITicks, -(s32)cyclesExecuted);

				Common::AtomicStore(fifo.CPReadPointer, readPtr);
				Common::AtomicAdd(fifo.CPReadWriteDistance, -32);
				if ((GetVideoBufferEndPtr() - g_VideoData.GetReadPosition()) == 0)
					Common::AtomicStore(fifo.SafeCPReadPointer, fifo.CPReadPointer);
			}

			CommandProcessor::SetCpStatus();

			// This call is pretty important in DualCore mode and must be called in the FIFO Loop.
			// If we don't, s_swapRequested or s_efbAccessRequested won't be set to false
			// leading the CPU thread to wait in Video_BeginField or Video_AccessEFB thus slowing things down.
			AsyncRequests::GetInstance()->PullEvents();
			CommandProcessor::isPossibleWaitingSetDrawDone = false;
		}

		fifo.isGpuReadingData = false;

		if (EmuRunningState)
		{
			// NOTE(jsd): Calling SwitchToThread() on Windows 7 x64 is a hot spot, according to profiler.
			// See https://docs.google.com/spreadsheet/ccc?key=0Ah4nh0yGtjrgdFpDeF9pS3V6RUotRVE3S3J4TGM1NlE#gid=0
			// for benchmark details.
			if (yield_cpu)
				Common::YieldCPU();
		}
		else
		{
			// While the emu is paused, we still handle async requests then sleep.
			while (!EmuRunningState)
			{
				g_video_backend->PeekMessages();
				m_csHWVidOccupied.unlock();
				Common::SleepCurrentThread(1);
				m_csHWVidOccupied.lock();
			}
		}
	}
	AsyncRequests::GetInstance()->SetEnable(false);
	AsyncRequests::GetInstance()->SetPassthrough(true);
}


bool AtBreakpoint()
{
	SCPFifoStruct &fifo = CommandProcessor::fifo;
	return fifo.bFF_BPEnable && (fifo.CPReadPointer == fifo.CPBreakpoint);
}

void RunGpu()
{
	SCPFifoStruct &fifo = CommandProcessor::fifo;
	while (fifo.bFF_GPReadEnable && fifo.CPReadWriteDistance && !AtBreakpoint())
	{
		u8 *uData = Memory::GetPointer(fifo.CPReadPointer);

		FPURoundMode::SaveSIMDState();
		FPURoundMode::LoadDefaultSIMDState();
		ReadDataFromFifo(uData);
		OpcodeDecoder_Run(GetVideoBufferEndPtr());
		FPURoundMode::LoadSIMDState();

		//DEBUG_LOG(COMMANDPROCESSOR, "Fifo wraps to base");

		if (fifo.CPReadPointer == fifo.CPEnd)
			fifo.CPReadPointer = fifo.CPBase;
		else
			fifo.CPReadPointer += 32;

		fifo.CPReadWriteDistance -= 32;
	}
	CommandProcessor::SetCpStatus();
}

void PreProcessingFifo(bool GPReadEnabled)
{

	SCPFifoStruct &fifo = CommandProcessor::fifo;

	if (CPBaseFifoSC
		!= fifo.CPBase || CPEndFifoSC != fifo.CPEnd)
	{
		CPBaseFifoSC = fifo.CPBase;
		CPEndFifoSC = fifo.CPEnd;
		CPReadFifoSC = fifo.CPReadPointer;
	}

	if (GPReadEnabled && CPReadFifoSC != fifo.CPWritePointer)
	{

		u8 *uData = Memory::GetPointer(CPReadFifoSC);

		if (CPReadFifoSC < fifo.CPWritePointer)
		{
			ReadDataFromPreProcFifo(uData, (fifo.CPWritePointer - CPReadFifoSC));
		}
		else
		{
			ReadDataFromPreProcFifo(uData, (fifo.CPEnd - CPReadFifoSC) + 32);
			CPReadFifoSC = fifo.CPBase;
			uData = Memory::GetPointer(CPReadFifoSC);
			ReadDataFromPreProcFifo(uData, (fifo.CPWritePointer - CPReadFifoSC));
		}
		CPReadFifoSC = fifo.CPWritePointer;

		OpcodeDecoderSC_Run(GetVideoBufferEndPtrSC());
#ifdef _WIN32
		if (g_ActiveConfig.bWaitForShaderCompilation)
			HLSLAsyncCompiler::getInstance().WaitForCompilationFinished();
#endif
	}

}


void ReadDataFromPreProcFifo(u8* _uData, u32 len)
{
	if (len == 0) return;
	if (s_video_buffer_size_SC + len >= FIFO_SIZE)
	{
		int pos = (int)(g_VideoDataSC.GetReadPosition() - s_video_buffer_SC);
		s_video_buffer_size_SC -= pos;
		if (s_video_buffer_size_SC + len > FIFO_SIZE)
		{
			PanicAlert("FIFO SC out of bounds (s_video_buffer_size = %i, len = %i at %08x)", s_video_buffer_size_SC, len, pos);
		}
		memmove(&s_video_buffer_SC[0], &s_video_buffer_SC[pos], s_video_buffer_size_SC);
		g_VideoDataSC.SetReadPosition(s_video_buffer_SC);
	}
	// Copy new video instructions to s_video_buffer for future use in rendering the new picture
	memcpy(s_video_buffer_SC + s_video_buffer_size_SC, _uData, len);
	s_video_buffer_size_SC += len;
}
