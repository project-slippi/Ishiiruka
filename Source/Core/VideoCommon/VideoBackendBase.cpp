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
#include "VideoBackends/Null/VideoBackend.h"
#include "VideoBackends/OGL/VideoBackend.h"
#include "VideoBackends/Software/VideoBackend.h"
#include "VideoBackends/Vulkan/VideoBackend.h"

#if defined(VK_USE_PLATFORM_METAL_EXT)
#include <objc/message.h>
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

// A runtime method for determining whether to allow
// Vulkan support. In particular, this is useful for
// blocking macOS High Sierra - that platform does have
// MoltenVK/Metal support, but it's incomplete and results
// in a buggy experience and is easier to just block it
// completely.
static bool PlatformSupportsVulkan()
{
#if defined(VK_USE_PLATFORM_METAL_EXT)
    // We want to only allow Vulkan to be loaded on macOS 14 (Mojave) or higher.
    // Bail out if we're on macOS and can't detect it, or the version is lower.
    //
    // This code is borrowed liberally from mainline Dolphin.
  id processInfo = reinterpret_cast<id (*)(Class, SEL)>(objc_msgSend)(
      objc_getClass("NSProcessInfo"), sel_getUid("processInfo"));
  if (!processInfo)
    return false;

  struct OSVersion  // NSOperatingSystemVersion
  {
    size_t major_version;  // NSInteger majorVersion
    size_t minor_version;  // NSInteger minorVersion
    size_t patch_version;  // NSInteger patchVersion
  };

  // const bool meets_requirement = [processInfo isOperatingSystemAtLeastVersion:required_version];
  constexpr OSVersion required_version = {10, 14, 0};
  const bool meets_requirement = reinterpret_cast<bool (*)(id, SEL, OSVersion)>(objc_msgSend)(
      processInfo, sel_getUid("isOperatingSystemAtLeastVersion:"), required_version);
  return meets_requirement;
#endif

  // Vulkan support defaults to true (supported).
  return true;
}

void VideoBackendBase::PopulateList()
{
	// D3D11 > D3D12 > D3D9 > OGL > VULKAN > SW > Null
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

	// on macOS, we want to push users to use Vulkan on 10.14+ (Mojave onwards). OpenGL has been 
	// long deprecated by Apple there and is a known stumbling block for performance for new players.
	//
	// That said, we still support High Sierra, which can't use Metal (it will load, but lacks certain critical pieces).
	//
	// This mirrors a recent (2021) change in mainline Dolphin, so should be relatively safe to do here as well. All
	// we're doing is shoving Vulkan to the front if it's macOS 10.14 or later, so it loads first.
	if(PlatformSupportsVulkan()) {
#ifdef __APPLE__
		if (__builtin_available(macOS 10.14, *)) {
			g_available_video_backends.emplace(
				g_available_video_backends.begin(),
				std::make_unique<Vulkan::VideoBackend>()
			);
		} 
		else
#endif
        	{
	        	g_available_video_backends.push_back(std::make_unique<Vulkan::VideoBackend>());
        	}
    }

	// Disable software video backend as is currently not working
	//g_available_video_backends.push_back(std::make_unique<SW::VideoSoftware>());
	g_available_video_backends.push_back(std::make_unique<Null::VideoBackend>());

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
