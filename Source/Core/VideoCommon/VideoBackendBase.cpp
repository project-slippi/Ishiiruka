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
#ifndef __APPLE__
#include "VideoBackends/Vulkan/VideoBackend.h"
#endif
std::vector<std::unique_ptr<VideoBackendBase>> g_available_video_backends;
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
	// D3D11 > D3D12 > D3D9 > OGL > VULKAN > SW
#ifdef _WIN32
	if (IsWindowsVistaOrGreater())
	{
		g_available_video_backends.push_back(std::make_unique<DX11::VideoBackend>());
		// More robust way to check for D3D12 support than (unreliable) OS version checks.
		HMODULE d3d12_module = LoadLibraryA("d3d12.dll");
		if (d3d12_module != NULL)
		{
			FreeLibrary(d3d12_module);
			g_available_video_backends.push_back(std::make_unique<DX12::VideoBackend>());
		}
	}
	g_available_video_backends.push_back(std::make_unique<DX9::VideoBackend>());
#endif
	// disable OGL video Backend while is merged from master
	g_available_video_backends.push_back(std::make_unique<OGL::VideoBackend>());
#ifndef __APPLE__
	g_available_video_backends.push_back(std::make_unique<Vulkan::VideoBackend>());
#endif
	// Disable software video backend as is currently not working
	//g_available_video_backends.push_back(std::make_unique<SW::VideoSoftware>());

	for (auto& backend : g_available_video_backends)
	{
		if (backend)
		{
			s_default_backend = g_video_backend = backend.get();
			break;
		}
	}
}

void VideoBackendBase::ClearList()
{
	g_available_video_backends.clear();
}

void VideoBackendBase::ActivateBackend(const std::string& name)
{
	if (name.empty()) // If nullptr, set it to the default backend (expected behavior)
		g_video_backend = s_default_backend;

	for (auto& backend : g_available_video_backends)
		if (name == backend->GetName())
			g_video_backend = backend.get();
}
