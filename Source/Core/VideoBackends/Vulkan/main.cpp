// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <vector>

#include "Common/Logging/LogManager.h"
#include "Core/Host.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/Constants.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/PerfQuery.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/SwapChain.h"
#include "VideoBackends/Vulkan/TextureCache.h"
#include "VideoBackends/Vulkan/VertexManager.h"
#include "VideoBackends/Vulkan/VideoBackend.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

#if defined(VK_USE_PLATFORM_METAL_EXT)
#include <CoreGraphics/CGBase.h>
#include <CoreGraphics/CGGeometry.h>
#include <objc/message.h>
#endif

namespace Vulkan
{

static void *s_metal_view_handle = nullptr;

void VideoBackend::InitBackendInfo()
{
	VulkanContext::PopulateBackendInfo(&g_Config);

	if (LoadVulkanLibrary())
	{
		VkInstance temp_instance = VulkanContext::CreateVulkanInstance(false, false, false);
		if (temp_instance)
		{
			if (LoadVulkanInstanceFunctions(temp_instance))
			{
				VulkanContext::GPUList gpu_list = VulkanContext::EnumerateGPUs(temp_instance);
				VulkanContext::PopulateBackendInfoAdapters(&g_Config, gpu_list);

				if (!gpu_list.empty())
				{
					// Use the selected adapter, or the first to fill features.
					size_t device_index = static_cast<size_t>(g_Config.iAdapter);
					if (device_index >= gpu_list.size())
						device_index = 0;

					VkPhysicalDevice gpu = gpu_list[device_index];
					VkPhysicalDeviceProperties properties;
					vkGetPhysicalDeviceProperties(gpu, &properties);
					VkPhysicalDeviceFeatures features;
					vkGetPhysicalDeviceFeatures(gpu, &features);
					VulkanContext::PopulateBackendInfoFeatures(&g_Config, gpu, features);
					VulkanContext::PopulateBackendInfoMultisampleModes(&g_Config, gpu, properties);
				}
			}

			vkDestroyInstance(temp_instance, nullptr);
		}
		else
		{
			PanicAlert("Failed to create Vulkan instance.");
		}

		UnloadVulkanLibrary();
	}
	else
	{
		PanicAlert("Failed to load Vulkan library.");
	}
}

// Helper method to check whether the Host GPU logging category is enabled.
static bool IsHostGPULoggingEnabled()
{
	return LogManager::GetInstance()->IsEnabled(LogTypes::HOST_GPU, LogTypes::LERROR);
}

// Helper method to determine whether to enable the debug report extension.
static bool ShouldEnableDebugReports(bool enable_validation_layers)
{
	// Enable debug reports if the Host GPU log option is checked, or validation layers are enabled.
	// The only issue here is that if Host GPU is not checked when the instance is created, the debug
	// report extension will not be enabled, requiring the game to be restarted before any reports
	// will be logged. Otherwise, we'd have to enable debug reports on every instance, when most
	// users will never check the Host GPU logging category.
	return enable_validation_layers || IsHostGPULoggingEnabled();
}

bool VideoBackend::Initialize(void *window_handle)
{
	if (!LoadVulkanLibrary())
	{
		PanicAlert("Failed to load Vulkan library.");
		return false;
	}

	// HACK: Use InitBackendInfo to initially populate backend features.
	// This is because things like stereo get disabled when the config is validated,
	// which happens before our device is created (settings control instance behavior),
	// and we don't want that to happen if the device actually supports it.
	InitBackendInfo();
	InitializeShared();

	// Check for presence of the validation layers before trying to enable it
	bool enable_validation_layer = g_Config.bEnableValidationLayer;
	if (enable_validation_layer && !VulkanContext::CheckValidationLayerAvailablility())
	{
		WARN_LOG(VIDEO, "Validation layer requested but not available, disabling.");
		enable_validation_layer = false;
	}

	// On macOS, we want to get the subview that hosts the rendering layer. Other platforms
	// render through to the underlying view with no issues.
#if defined(VK_USE_PLATFORM_METAL_EXT)
	void *win_handle = s_metal_view_handle;
#else
	void *win_handle = window_handle;
#endif

	// Create Vulkan instance, needed before we can create a surface.
	bool enable_surface = win_handle != nullptr;
	bool enable_debug_reports = ShouldEnableDebugReports(enable_validation_layer);
	VkInstance instance =
	    VulkanContext::CreateVulkanInstance(enable_surface, enable_debug_reports, enable_validation_layer);
	if (instance == VK_NULL_HANDLE)
	{
		PanicAlert("Failed to create Vulkan instance.");
		UnloadVulkanLibrary();
		ShutdownShared();
		return false;
	}

	// Load instance function pointers
	if (!LoadVulkanInstanceFunctions(instance))
	{
		PanicAlert("Failed to load Vulkan instance functions.");
		vkDestroyInstance(instance, nullptr);
		UnloadVulkanLibrary();
		ShutdownShared();
		return false;
	}

	// Create Vulkan surface
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	if (enable_surface)
	{
		surface = SwapChain::CreateVulkanSurface(instance, win_handle);
		if (surface == VK_NULL_HANDLE)
		{
			PanicAlert("Failed to create Vulkan surface.");
			vkDestroyInstance(instance, nullptr);
			UnloadVulkanLibrary();
			ShutdownShared();
			return false;
		}
	}

	// Fill the adapter list, and check if the user has selected an invalid device
	// For some reason nvidia's driver crashes randomly if you call vkEnumeratePhysicalDevices
	// after creating a device..
	VulkanContext::GPUList gpu_list = VulkanContext::EnumerateGPUs(instance);
	size_t selected_adapter_index = static_cast<size_t>(g_Config.iAdapter);
	if (gpu_list.empty())
	{
		PanicAlert("No Vulkan physical devices available.");
		if (surface != VK_NULL_HANDLE)
			vkDestroySurfaceKHR(instance, surface, nullptr);

		vkDestroyInstance(instance, nullptr);
		UnloadVulkanLibrary();
		ShutdownShared();
		return false;
	}
	else if (selected_adapter_index >= gpu_list.size())
	{
		WARN_LOG(VIDEO, "Vulkan adapter index out of range, selecting first adapter.");
		selected_adapter_index = 0;
	}

	// Pass ownership over to VulkanContext, and let it take care of everything.
	g_vulkan_context = VulkanContext::Create(instance, gpu_list[selected_adapter_index], surface, &g_Config,
	                                         enable_debug_reports, enable_validation_layer);
	if (!g_vulkan_context)
	{
		PanicAlert("Failed to create Vulkan device");
		UnloadVulkanLibrary();
		ShutdownShared();
		return false;
	}

	// Create swap chain. This has to be done early so that the target size is correct for auto-scale.
	std::unique_ptr<SwapChain> swap_chain;
	if (surface != VK_NULL_HANDLE)
	{
		swap_chain = SwapChain::Create(win_handle, surface, g_Config.IsVSync());
		if (!swap_chain)
		{
			PanicAlert("Failed to create Vulkan swap chain.");
			return false;
		}
	}

	// Create command buffers. We do this separately because the other classes depend on it.
	g_command_buffer_mgr = std::make_unique<CommandBufferManager>(g_Config.bBackendMultithreading);
	if (!g_command_buffer_mgr->Initialize())
	{
		PanicAlert("Failed to create Vulkan command buffers");
		g_command_buffer_mgr.reset();
		g_vulkan_context.reset();
		UnloadVulkanLibrary();
		ShutdownShared();
		return false;
	}

	// Create main wrapper instances.
	g_object_cache = std::make_unique<ObjectCache>();
	g_framebuffer_manager = std::make_unique<FramebufferManager>();
	g_renderer = std::make_unique<Renderer>(std::move(swap_chain));
	g_renderer->Init();

	// We cache this on the renderer if it's Metal, as fullscreen changes need to use the
	// correct rendering layer to handle swap chain recreation.
#if defined(VK_USE_PLATFORM_METAL_EXT)
	g_renderer->CacheSurfaceHandle(s_metal_view_handle);
#endif

	// Invoke init methods on main wrapper classes.
	// These have to be done before the others because the destructors
	// for the remaining classes may call methods on these.
	if (!g_object_cache->Initialize() || !FramebufferManager::GetInstance()->Initialize() ||
	    !StateTracker::CreateInstance() || !Renderer::GetInstance()->Initialize())
	{
		PanicAlert("Failed to initialize Vulkan classes.");
		g_renderer.reset();
		StateTracker::DestroyInstance();
		g_framebuffer_manager.reset();
		g_object_cache.reset();
		g_command_buffer_mgr.reset();
		g_vulkan_context.reset();
		UnloadVulkanLibrary();
		ShutdownShared();
		return false;
	}

	// Create remaining wrapper instances.
	g_vertex_manager = std::make_unique<VertexManager>();
	g_texture_cache = std::make_unique<TextureCache>();
	g_perf_query = std::make_unique<PerfQuery>();
	if (!VertexManager::GetInstance()->Initialize() || !TextureCache::GetInstance()->Initialize() ||
	    !PerfQuery::GetInstance()->Initialize())
	{
		PanicAlert("Failed to initialize Vulkan classes.");
		g_perf_query.reset();
		g_texture_cache.reset();
		g_vertex_manager.reset();
		g_renderer.reset();
		StateTracker::DestroyInstance();
		g_framebuffer_manager.reset();
		g_object_cache.reset();
		g_command_buffer_mgr.reset();
		g_vulkan_context.reset();
		UnloadVulkanLibrary();
		ShutdownShared();
		return false;
	}

	return true;
}

// This is called after Initialize() from the Core
// Run from the graphics thread
void VideoBackend::Video_Prepare()
{
	// Display the name so the user knows which device was actually created
	OSD::AddMessage(
	    StringFromFormat("Using physical adapter %s", g_vulkan_context->GetDeviceProperties().deviceName).c_str(),
	    5000);
}

void VideoBackend::Shutdown()
{
	g_command_buffer_mgr->WaitForGPUIdle();

	g_object_cache.reset();
	g_command_buffer_mgr.reset();
	g_vulkan_context.reset();

	UnloadVulkanLibrary();

	ShutdownShared();

#if defined(VK_USE_PLATFORM_METAL_EXT)
	s_metal_view_handle = nullptr;
#endif
}

void VideoBackend::Video_Cleanup()
{
	g_command_buffer_mgr->WaitForGPUIdle();

	// Save all cached pipelines out to disk for next time.
	g_object_cache->SavePipelineCache();

	g_perf_query.reset();
	g_texture_cache.reset();
	g_vertex_manager.reset();
	g_framebuffer_manager.reset();
	StateTracker::DestroyInstance();
	g_renderer.reset();

	CleanupShared();
}

#if defined(VK_USE_PLATFORM_METAL_EXT)
// This is injected as a subclass method on the custom layer view, and
// tells macOS to avoid `drawRect:` and opt for direct layer updating instead.
BOOL wantsUpdateLayer(id self, SEL _cmd, id sender)
{
	return YES;
}

// Used by some internals, but ideally never gets called to begin with.
Class getLayerClass(id self, SEL _cmd)
{
	Class clsCAMetalLayerClass = objc_getClass("CAMetalLayer");
	return clsCAMetalLayerClass;
}

// When `wantsLayer` is true, this method is invoked to create the actual backing layer.
id makeBackingLayer(id self, SEL _cmd)
{
	Class metalLayerClass = objc_getClass("CAMetalLayer");

	// This should only be possible prior to macOS 10.14, but worth logging regardless.
	if (!metalLayerClass)
	{
		ERROR_LOG(VIDEO, "Failed to get CAMetalLayer class.");
	}

	id layer = reinterpret_cast<id (*)(Class, SEL)>(objc_msgSend)(metalLayerClass, sel_getUid("layer"));

	id screen = reinterpret_cast<id (*)(Class, SEL)>(objc_msgSend)(objc_getClass("NSScreen"), sel_getUid("mainScreen"));

	// CGFloat factor = [screen backingScaleFactor]
	double factor = reinterpret_cast<double (*)(id, SEL)>(objc_msgSend)(screen, sel_getUid("backingScaleFactor"));

	// layer.contentsScale = factor
	reinterpret_cast<void (*)(id, SEL, double)>(objc_msgSend)(layer, sel_getUid("setContentsScale:"), factor);

	// This is an oddity, but alright. The SwapChain is already configured to be respective of Vsync, but the underlying
	// CAMetalLayer *also* needs to be instructed to respect it. This defaults to YES; if we're not supposed to have
	// vsync enabled, then we need to flip this.
	//
	// Notably, some M1 Macs have issues without this logic.
	//
	// I have absolutely no clue why this works, as MoltenVK also sets this property. Setting it before giving the layer
	// to MoltenVK seems to make it stick, though.
	if (!g_Config.IsVSync())
	{
		// Explicitly tells the underlying layer to NOT use vsync.
		// [view setDisplaySyncEnabled:NO]
		reinterpret_cast<void (*)(id, SEL, BOOL)>(objc_msgSend)(layer, sel_getUid("setDisplaySyncEnabled:"), NO);
	}

	// CAMetalLayer is triple-buffered by default; we can lower this to double buffering.
	//
	// (The only acceptable values are `2` or `3`). Typically it's only iMacs that can handle this, so we'll just
	// enable an ENV variable for it and document it on the wiki.
	if (getenv("SLP_METAL_DOUBLE_BUFFER") != NULL)
	{
		reinterpret_cast<void (*)(id, SEL, BOOL)>(objc_msgSend)(layer, sel_getUid("setMaximumDrawableCount:"), 2);
	}

	return layer;
}

constexpr char kSLPMetalLayerViewClassName[] = "SLPMetalLayerViewClass";

// This method injects a custom NSView subclass into the Objective-C runtime.
//
// The reason this is done is due to wanting to bypass NSView's `drawRect:` for Metal rendering
// purposes. To do this, it's not enough to just set `wantsLayer` to true - we need to also implement
// a few subclass methods, and tell the system we *want* the fast path.
//
// We have to inject a custom subclass as we can't modify the view (window_handle) in `PrepareWindow`,
// as that's a wxWidgets handle that relies on `drawRect:` being called for things to work. To work
// around this, we simply take the `window_handle` (i.e the view), create an instance of our `SLPMetalLayerView`,
// and attach that as a child view. `SLPMetalLayerView` should get the fast path, while everything else should
// stay golden.
Class getSLPMetalLayerViewClassType()
{
	Class SLPMetalLayerViewClass = objc_getClass(kSLPMetalLayerViewClassName);

	if (SLPMetalLayerViewClass == nullptr)
	{
#ifdef IS_PLAYBACK
		// These are disabled on Playback builds for now, as M1 devices running Playback under Rosetta 2
		// seem to hit a race condition with asynchronous queue submits. Rendering takes a slight hit but
		// this matters less in playback, and it's still better than OpenGL.
		// setenv("MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS", "0", 0);
		// setenv("MVK_CONFIG_PRESENT_WITH_COMMAND_BUFFER", "0", 0);
#else
		// This does a one-time opt-in to a MVK flag that seems to universally help in Ishiiruka.
		// (mainline should not need this)
		setenv("MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS", "0", 0);
		setenv("MVK_CONFIG_PRESENT_WITH_COMMAND_BUFFER", "0", 0);
#endif
		SLPMetalLayerViewClass = objc_allocateClassPair((Class)objc_getClass("NSView"), kSLPMetalLayerViewClassName, 0);

		class_addMethod(SLPMetalLayerViewClass, sel_getUid("layerClass"), (IMP)getLayerClass, "@:@");
		class_addMethod(SLPMetalLayerViewClass, sel_getUid("wantsUpdateLayer"), (IMP)wantsUpdateLayer, "v@:");
		class_addMethod(SLPMetalLayerViewClass, sel_getUid("makeBackingLayer"), (IMP)makeBackingLayer, "@:@");
		class_addMethod(SLPMetalLayerViewClass, sel_getUid("isOpaque"), (IMP)wantsUpdateLayer, "v@:");
		objc_registerClassPair(SLPMetalLayerViewClass);
	}

	return SLPMetalLayerViewClass;
}
#endif

void VideoBackend::PrepareWindow(void *window_handle)
{
#if defined(VK_USE_PLATFORM_METAL_EXT)
	id view = reinterpret_cast<id>(window_handle);

	CGRect (*sendRectFn)(id receiver, SEL operation);
	sendRectFn = (CGRect(*)(id, SEL))objc_msgSend_stret;
	CGRect frame = sendRectFn(view, sel_getUid("frame"));

	Class SLPMetalLayerViewClass = getSLPMetalLayerViewClassType();
	id alloc = reinterpret_cast<id (*)(Class, SEL)>(objc_msgSend)(SLPMetalLayerViewClass, sel_getUid("alloc"));

	auto rect = (CGRect){{0, 0}, {frame.size.width, frame.size.height}};
	id metal_view = reinterpret_cast<id (*)(id, SEL, CGRect)>(objc_msgSend)(alloc, sel_getUid("initWithFrame:"), rect);
	reinterpret_cast<id (*)(id, SEL, BOOL)>(objc_msgSend)(metal_view, sel_getUid("setWantsLayer:"), YES);

	// The below does: objc_msgSend(view, sel_getUid("setAutoresizingMask"), NSViewWidthSizable | NSViewHeightSizable);
	// All this is doing is telling the view/layer to resize when the parent does.
	reinterpret_cast<id (*)(id, SEL, unsigned long)>(objc_msgSend)(metal_view, sel_getUid("setAutoresizingMask:"), 18);

	reinterpret_cast<id (*)(id, SEL, id)>(objc_msgSend)(view, sel_getUid("addSubview:"), metal_view);
	s_metal_view_handle = metal_view;
#endif
}
} // namespace Vulkan
