// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/WASAPIStream.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "VideoCommon/OnScreenDisplay.h"

#include <mmdeviceapi.h>

#include <locale>

static std::string wasapi_hresult_to_string(HRESULT res)
{
	switch(res)
	{
#define DEFINE_FOR(hres) case hres: return #hres; 
		DEFINE_FOR(AUDCLNT_E_NOT_INITIALIZED)
		DEFINE_FOR(AUDCLNT_E_ALREADY_INITIALIZED)
		DEFINE_FOR(AUDCLNT_E_WRONG_ENDPOINT_TYPE)
		DEFINE_FOR(AUDCLNT_E_DEVICE_INVALIDATED)
		DEFINE_FOR(AUDCLNT_E_NOT_STOPPED)
		DEFINE_FOR(AUDCLNT_E_BUFFER_TOO_LARGE)
		DEFINE_FOR(AUDCLNT_E_OUT_OF_ORDER)
		DEFINE_FOR(AUDCLNT_E_UNSUPPORTED_FORMAT)
		DEFINE_FOR(AUDCLNT_E_INVALID_SIZE)
		DEFINE_FOR(AUDCLNT_E_DEVICE_IN_USE)
		DEFINE_FOR(AUDCLNT_E_BUFFER_OPERATION_PENDING)
		DEFINE_FOR(AUDCLNT_E_THREAD_NOT_REGISTERED)
		DEFINE_FOR(AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED)
		DEFINE_FOR(AUDCLNT_E_ENDPOINT_CREATE_FAILED)
		DEFINE_FOR(AUDCLNT_E_SERVICE_NOT_RUNNING)
		DEFINE_FOR(AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED)
		DEFINE_FOR(AUDCLNT_E_EXCLUSIVE_MODE_ONLY)
		DEFINE_FOR(AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL)
		DEFINE_FOR(AUDCLNT_E_EVENTHANDLE_NOT_SET)
		DEFINE_FOR(AUDCLNT_E_INCORRECT_BUFFER_SIZE)
		DEFINE_FOR(AUDCLNT_E_BUFFER_SIZE_ERROR)
		DEFINE_FOR(AUDCLNT_E_CPUUSAGE_EXCEEDED)
		DEFINE_FOR(AUDCLNT_E_RESOURCES_INVALIDATED)
		DEFINE_FOR(AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
		DEFINE_FOR(AUDCLNT_E_INVALID_DEVICE_PERIOD)
		DEFINE_FOR(E_POINTER)
		DEFINE_FOR(E_INVALIDARG)
		DEFINE_FOR(E_OUTOFMEMORY)
	}

	return "UNKNOWN, " + std::to_string(res);
}

#ifndef PKEY_Device_FriendlyName
DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName, 0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);
#endif

// https://github.com/mvaneerde/blog/blob/master/play-exclusive/play-exclusive/play.cpp
bool WASAPIStream::Start()
{
	HRESULT hr = S_OK;
	IMMDeviceEnumerator* mm_device_enumerator;

	hr = CoCreateInstance(
		__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator),
		(void**)&mm_device_enumerator
	);

	if(FAILED(hr))
	{
		ERROR_LOG(AUDIO, "WASAPIStream: HRESULT %s", wasapi_hresult_to_string(hr).c_str());
		ERROR_LOG(AUDIO, "WASAPIStream: Error @ CoCreateInstance of MMDeviceEnumerator");
		return false;
	}

	IMMDevice* mm_device = nullptr;

	std::string lower_device = "";
	for(char c : m_selected_device)
		lower_device += std::tolower(c, std::locale::classic());

	if(lower_device.find("default") != std::string::npos)
		hr = mm_device_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &mm_device);
	else
	{
		IMMDeviceCollection* devices = nullptr;
		hr = mm_device_enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices);

		UINT device_count = 0;
		devices->GetCount(&device_count);

		for(UINT i = 0; i < device_count; i++)
		{
			IMMDevice* device;
			devices->Item(i, &device);

			IPropertyStore* pstore;
			device->OpenPropertyStore(STGM_READ, &pstore);

			PROPVARIANT name_prop;
			PropVariantInit(&name_prop);

			pstore->GetValue(PKEY_Device_FriendlyName, &name_prop);

			char name_cstr[2048];
			size_t ret;

			ret = wcstombs(name_cstr, name_prop.pwszVal, sizeof(name_cstr));
			if(ret == 2048) name_cstr[2047] = '\0';

			std::string name_stdstr = name_cstr;

			for(int i = 0; i <= 9; i++)
			{
				if(name_stdstr.substr(0, std::string("0 - ").size()) == std::to_string(i) + " - ")
					name_stdstr = name_stdstr.substr(std::string("0 - ").size()) + " [" + std::to_string(i) + "]";
			}

			// if(name_stdstr.size() > 40)
			//     name_stdstr = name_stdstr.substr(0, 40) + "...";
			// needs to be preserved for uniqueness

			if(name_stdstr == m_selected_device)
				mm_device = device;

			PropVariantClear(&name_prop);

			pstore->Release();

			if(mm_device != device)
				device->Release();
		}

		devices->Release();
	}

	if(mm_device == nullptr)
	{
		OSD::AddMessage("Invalid audio device \"" + m_selected_device + "\" selected for WASAPI. Check your backend settings.", 6000U);
		return false;
	}

	if(FAILED(hr))
	{
		ERROR_LOG(AUDIO, "WASAPIStream: HRESULT %s", wasapi_hresult_to_string(hr).c_str());
		ERROR_LOG(AUDIO, "WASAPIStream: Error @ MMDeviceEnumerator::GetDefaultAudioEndpoint");
		return false;
	}

	hr = mm_device->Activate(
		__uuidof(IAudioClient),
		CLSCTX_ALL, NULL,
		(void**)&m_audio_client
	);

	if(FAILED(hr))
	{
		ERROR_LOG(AUDIO, "WASAPIStream: HRESULT %s", wasapi_hresult_to_string(hr).c_str());
		ERROR_LOG(AUDIO, "WASAPIStream: Error @ MMDeviceEnumerator -> IAudioClient");
		return false;
	}

	LPWSTR id;
	mm_device->GetId(&id);

	char buffer[2048];
	size_t ret;

	ret = wcstombs(buffer, id, sizeof(buffer));
	if(ret == 2048) buffer[2047] = '\0';

	INFO_LOG(AUDIO, "WASAPIStream: Using device %s", buffer);

	fmt = { 0 };
	fmt.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	fmt.Format.nChannels = 2;
	fmt.Format.nSamplesPerSec = 48000;
	fmt.Format.nAvgBytesPerSec = fmt.Format.nSamplesPerSec * 4;
	fmt.Format.nBlockAlign = 4;
	fmt.Format.wBitsPerSample = 16;

	fmt.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

	fmt.Samples.wValidBitsPerSample = fmt.Format.wBitsPerSample;
	fmt.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
	fmt.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

	/* WAVEFORMATEX* fmtex;
	m_audio_client->GetMixFormat(&fmtex);
	fmt = *reinterpret_cast<PWAVEFORMATEXTENSIBLE>(fmtex); */

	REFERENCE_TIME device_period;
	hr = m_audio_client->GetDevicePeriod(
		m_exclusive_mode ? nullptr : &device_period,
		m_exclusive_mode ? &device_period : nullptr
	);

	device_period += SConfig::GetInstance().iLatency * (10000 / fmt.Format.nChannels);

	if(FAILED(hr))
	{
		ERROR_LOG(AUDIO, "WASAPIStream: HRESULT %s", wasapi_hresult_to_string(hr).c_str());
		ERROR_LOG(AUDIO, "WASAPIStream: Couldn't get minimum device period.");

		m_audio_client->Release();
		m_audio_client = nullptr;

		mm_device_enumerator->Release();
		mm_device->Release();

		return false;
	}

#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000

	hr = m_audio_client->Initialize(
		m_exclusive_mode ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
		AUDCLNT_STREAMFLAGS_NOPERSIST,
		device_period, 
		m_exclusive_mode ? device_period : 0,
		reinterpret_cast<WAVEFORMATEX*>(&fmt), 
		nullptr
	);

	if(hr == AUDCLNT_E_UNSUPPORTED_FORMAT)
		OSD::AddMessage("Your current audio device doesn't support 16-bit 48000 hz PCM audio. WASAPI exclusive mode won't work.", 6000U);

	if(hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
	{
		ERROR_LOG(AUDIO, "WASAPIStream: HRESULT %s", wasapi_hresult_to_string(hr).c_str());
		INFO_LOG(AUDIO, "WASAPIStream: Device period not aligned, attempting to fix...");

		hr = m_audio_client->GetBufferSize(&frames_in_buffer);
		m_audio_client->Release();

		if(FAILED(hr)) 
		{
			ERROR_LOG(AUDIO, "WASAPIStream: HRESULT %s", wasapi_hresult_to_string(hr).c_str());
			ERROR_LOG(AUDIO, "WASAPIStream: Couldn't get buffer size for alignment.");

			m_audio_client = nullptr;
			mm_device_enumerator->Release();
			mm_device->Release();

			return false;
		}

		device_period = static_cast<REFERENCE_TIME>(10000.0 * 1000 * frames_in_buffer / fmt.Format.nSamplesPerSec + 0.5) + SConfig::GetInstance().iLatency * 10000;

		hr = mm_device->Activate(
			__uuidof(IAudioClient),
			CLSCTX_ALL, NULL,
			(void**)&m_audio_client
		);

		if(FAILED(hr))
		{
			ERROR_LOG(AUDIO, "WASAPIStream: HRESULT %s", wasapi_hresult_to_string(hr).c_str());
			ERROR_LOG(AUDIO, "WASAPIStream: Error @ MMDeviceEnumerator -> IAudioClient");

			mm_device_enumerator->Release();
			mm_device->Release();

			return false;
		}

		hr = m_audio_client->Initialize(
			m_exclusive_mode ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED,
			AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
			AUDCLNT_STREAMFLAGS_NOPERSIST,
			device_period, 
			m_exclusive_mode ? device_period : 0,
			reinterpret_cast<WAVEFORMATEX*>(&fmt), 
			nullptr
		);
	}

	mm_device_enumerator->Release();
	mm_device->Release();

	if(FAILED(hr))
	{
		ERROR_LOG(AUDIO, "WASAPIStream: HRESULT %s", wasapi_hresult_to_string(hr).c_str());
		ERROR_LOG(AUDIO, "WASAPIStream: Couldn't initialize audio client (device period: %i).", device_period);

		m_audio_client->Release();
		m_audio_client = nullptr;

		return false;
	}

	hr = m_audio_client->GetBufferSize(&frames_in_buffer);

	if(FAILED(hr))
	{
		ERROR_LOG(AUDIO, "WASAPIStream: HRESULT %s", wasapi_hresult_to_string(hr).c_str());
		ERROR_LOG(AUDIO, "WASAPIStream: Couldn't get buffer size.");

		m_audio_client = nullptr;
		m_audio_client->Release();

		return false;
	}

	device_period = static_cast<REFERENCE_TIME>(10000.0 * 1000 * frames_in_buffer / fmt.Format.nSamplesPerSec + 0.5) + SConfig::GetInstance().iLatency * 10000;

	if(m_exclusive_mode)
		OSD::AddMessage("WASAPI exclusive mode latency configured to " + std::to_string(device_period / 10000.0f) + " ms", 6000U);

	m_need_data_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_audio_client->SetEventHandle(m_need_data_event);

	hr = m_audio_client->GetService(
		__uuidof(IAudioRenderClient),
		(void**)&m_renderer
	);

	if(FAILED(hr))
	{
		ERROR_LOG(AUDIO, "WASAPIStream: HRESULT %s", wasapi_hresult_to_string(hr).c_str());
		ERROR_LOG(AUDIO, "WASAPIStream: Couldn't get IAudioClient renderer.");

		CloseHandle(m_need_data_event);
		m_audio_client->Release();

		m_need_data_event = nullptr;
		m_audio_client = nullptr;
		return false;
	}

	hr = m_audio_client->Start();

	if(FAILED(hr))
	{
		ERROR_LOG(AUDIO, "WASAPIStream: HRESULT %s", wasapi_hresult_to_string(hr).c_str());
		ERROR_LOG(AUDIO, "WASAPIStream: Couldn't start audio client.");

		CloseHandle(m_need_data_event);
		m_renderer->Release();
		m_audio_client->Release();

		m_need_data_event = nullptr;
		m_renderer = nullptr;
		m_audio_client = nullptr;
		return false;
	}

	SoundStream::Start();
	return true;
}

void WASAPIStream::SoundLoop()
{
	if(m_audio_client && m_renderer && m_need_data_event)
	{
		Common::SetCurrentThreadName("WASAPI Exclusive Event Thread");

		u8* data = nullptr;

		m_renderer->GetBuffer(frames_in_buffer, &data);
		m_renderer->ReleaseBuffer(frames_in_buffer, AUDCLNT_BUFFERFLAGS_SILENT);

		while(threadData.load())
		{
			WaitForSingleObject(m_need_data_event, 1000);
			if(!threadData.load())
				return;

			m_renderer->GetBuffer(frames_in_buffer, &data);
			m_mixer->Mix(reinterpret_cast<s16*>(data), frames_in_buffer);

			float volume = SConfig::GetInstance().m_IsMuted ? 0 : SConfig::GetInstance().m_Volume / 100.0f;

			for(u32 i = 0; i < frames_in_buffer * 2; i++)
				reinterpret_cast<s16*>(data)[i] = static_cast<s16>(reinterpret_cast<s16*>(data)[i] * volume);

			m_renderer->ReleaseBuffer(frames_in_buffer, Core::GetState() != Core::CORE_RUN ? AUDCLNT_BUFFERFLAGS_SILENT : 0);
		}
	}
}

void WASAPIStream::Stop()
{
	SoundStream::Stop();

	if(m_need_data_event)
		CloseHandle(m_need_data_event);
	if(m_audio_client)
		m_audio_client->Stop();
	if(m_renderer)
		m_renderer->Release();
	if(m_audio_client)
		m_audio_client->Release();

	m_need_data_event = nullptr;
	m_renderer = nullptr;
	m_audio_client = nullptr;
}

std::vector<std::string> WASAPIStream::GetAudioDevices()
{
	HRESULT hr = S_OK;
	IMMDeviceEnumerator* mm_device_enumerator;

	hr = CoCreateInstance(
		__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator),
		(void**)&mm_device_enumerator
	);

	if(FAILED(hr))
	{
		ERROR_LOG(AUDIO, "WASAPIStream: HRESULT %s", wasapi_hresult_to_string(hr).c_str());
		ERROR_LOG(AUDIO, "WASAPIStream: Error in GetAudioDevices @ CoCreateInstance of MMDeviceEnumerator");
		return {};
	}

	IMMDeviceCollection* devices = nullptr;
	hr = mm_device_enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices);

	if(FAILED(hr))
	{
		ERROR_LOG(AUDIO, "WASAPIStream: HRESULT %s", wasapi_hresult_to_string(hr).c_str());
		ERROR_LOG(AUDIO, "WASAPIStream: Error in GetAudioDevices @ EnumAudioEndpoints");

		mm_device_enumerator->Release();
		return {};
	}

	UINT device_count = 0;
	devices->GetCount(&device_count);

	std::vector<std::string> results;
	for(UINT i = 0; i < device_count; i++)
	{
		IMMDevice* device;
		devices->Item(i, &device);

		IPropertyStore* pstore;
		device->OpenPropertyStore(STGM_READ, &pstore);

		PROPVARIANT name_prop;
		PropVariantInit(&name_prop);

		pstore->GetValue(PKEY_Device_FriendlyName, &name_prop);

		char name_cstr[2048];
		size_t ret;

		ret = wcstombs(name_cstr, name_prop.pwszVal, sizeof(name_cstr));
		if(ret == 2048) name_cstr[2047] = '\0';

		std::string name_stdstr = name_cstr;

		for(int i = 0; i <= 9; i++)
		{
			if(name_stdstr.substr(0, std::string("0 - ").size()) == std::to_string(i) + " - ")
				name_stdstr = name_stdstr.substr(std::string("0 - ").size()) + " [" + std::to_string(i) + "]";
		}

		// if(name_stdstr.size() > 40)
		//     name_stdstr = name_stdstr.substr(0, 40) + "...";
		// needs to be preserved for uniqueness

		results.push_back(name_stdstr);
		PropVariantClear(&name_prop);

		pstore->Release();
		device->Release();
	}

	devices->Release();
	mm_device_enumerator->Release();

	return results;
}