// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/VideoBackendBase.h"

// TODO: ugly
#ifdef _WIN32
#include "VideoBackends/DX9/VideoBackend.h"
#include "VideoBackends/DX11/VideoBackend.h"
#include "VideoBackends/D3D12/VideoBackend.h"
#endif
#include "VideoBackends/OGL/VideoBackend.h"
#include "VideoBackends/Software/VideoBackend.h"

std::vector<VideoBackendBase*> g_available_video_backends;
VideoBackendBase* g_video_backend = nullptr;
static VideoBackendBase* s_default_backend = nullptr;

#ifdef _WIN32
#include <windows.h>
#include <VersionHelpers.h>
#define _WIN32_WINNT_WINTHRESHOLD           0x0A00 // Windows 10
#define _WIN32_WINNT_WIN10                  0x0A00 // Windows 10
#endif

void VideoBackendBase::PopulateList()
{
	VideoBackendBase* backends[4] = { NULL };

	// D3D9 > D3D11 > OGL > SW
#ifdef _WIN32
	g_available_video_backends.push_back(backends[0] = new DX9::VideoBackend);
	if (IsWindowsVistaOrGreater())
	{
		g_available_video_backends.push_back(backends[1] = new DX11::VideoBackend);
		// More robust way to check for D3D12 support than (unreliable) OS version checks.
		HMODULE d3d12_module = LoadLibraryA("d3d12.dll");
		if (d3d12_module != NULL)
		{
			FreeLibrary(d3d12_module);
			g_available_video_backends.push_back(backends[2] = new DX12::VideoBackend);
		}
	}
#endif
// disable OGL video Backend while is merged from master
#if !defined(USE_GLES) || USE_GLES3
	g_available_video_backends.push_back(backends[2] = new OGL::VideoBackend);
#endif
	g_available_video_backends.push_back(backends[3] = new SW::VideoSoftware);

	for (int i = 0; i < 4; ++i)
	{
		if (backends[i])
		{
			s_default_backend = g_video_backend = backends[i];
			break;
		}
	}
}

void VideoBackendBase::ClearList()
{
	while (!g_available_video_backends.empty())
	{
		delete g_available_video_backends.back();
		g_available_video_backends.pop_back();
	}
}

void VideoBackendBase::ActivateBackend(const std::string& name)
{
	if (name.length() == 0) // If nullptr, set it to the default backend (expected behavior)
		g_video_backend = s_default_backend;

	for (VideoBackendBase* backend : g_available_video_backends)
		if (name == backend->GetName())
			g_video_backend = backend;;
}
