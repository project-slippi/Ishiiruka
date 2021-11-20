// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Movie.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

VideoConfig g_Config;
VideoConfig g_ActiveConfig;

void UpdateActiveConfig()
{
	if (Movie::IsPlayingInput() && Movie::IsConfigSaved())
		Movie::SetGraphicsConfig();
	g_ActiveConfig = g_Config;
}

VideoConfig::VideoConfig()
{
	bRunning = false;

	// Needed for the first frame, I think
	fAspectRatioHackW = 1;
	fAspectRatioHackH = 1;

	// disable all features by default
	backend_info.APIType = API_NONE;
	for (s32 i = 0; i < 16; i++)
	{
		backend_info.bSupportedFormats[i] = false;
	}
	backend_info.bSupportsExclusiveFullscreen = false;

	// Game-specific stereoscopy settings
	bStereoEFBMonoDepth = false;
	iStereoDepthPercentage = 100;
	iStereoConvergence = 20;
	bUseScalingFilter = false;
	bTexDeposterize = false;
	iTexScalingType = 0;
	iTexScalingFactor = 2;
	backend_info.bSupportsMultithreading = false;
	backend_info.bSupportsInternalResolutionFrameDumps = false;
	bEnableValidationLayer = false;
	bBackendMultithreading = true;
	backend_info.MaxTextureSize = 4096;
}

int VideoConfig::GetCurrentAspect()
{
    if(SConfig::GetInstance().bMeleeForceWidescreen)
        return ASPECT_16_9;
    return iAspectRatio;
}

void VideoConfig::Load(const std::string& ini_file)
{
	IniFile iniFile;
	iniFile.Load(ini_file);

	IniFile::Section* hardware = iniFile.GetOrCreateSection("Hardware");
#ifdef IS_PLAYBACK
	hardware->Get("VSync", &bVSync, true);
#else
	hardware->Get("VSync", &bVSync, false);
#endif
	hardware->Get("Adapter", &iAdapter, 0);

	IniFile::Section* settings = iniFile.GetOrCreateSection("Settings");
	settings->Get("wideScreenHack", &bWidescreenHack, false);
#ifdef IS_PLAYBACK
	settings->Get("AspectRatio", &iAspectRatio, (int)ASPECT_AUTO);
#else
	settings->Get("AspectRatio", &iAspectRatio, (int)ASPECT_73_60);
#endif
	settings->Get("Crop", &bCrop, false);
	settings->Get("UseXFB", &bUseXFB, 0);
	settings->Get("UseRealXFB", &bUseRealXFB, 0);
	settings->Get("SafeTextureCacheColorSamples", &iSafeTextureCache_ColorSamples, 128);
#ifdef IS_PLAYBACK
	settings->Get("ShowFPS", &bShowFPS, false);
	settings->Get("ShowNetPlayPing", &bShowNetPlayPing, false);
#else
	settings->Get("ShowFPS", &bShowFPS, true);
	settings->Get("ShowNetPlayPing", &bShowNetPlayPing, true);
#endif
	settings->Get("ShowNetPlayMessages", &bShowNetPlayMessages, false);
    settings->Get("ShowOSDClock", &bShowOSDClock, false);
    settings->Get("ShowFrameTimes", &bShowFrameTimes, false);
	settings->Get("LogRenderTimeToFile", &bLogRenderTimeToFile, false);
	settings->Get("ShowInputDisplay", &bShowInputDisplay, false);
	settings->Get("OverlayStats", &bOverlayStats, false);
	settings->Get("OverlayProjStats", &bOverlayProjStats, false);
	settings->Get("DumpTextures", &bDumpTextures, 0);
	settings->Get("DumpVertexLoader", &bDumpVertexLoaders, 0);
	settings->Get("HiresTextures", &bHiresTextures, 0);
	settings->Get("HiresMaterialMaps", &bHiresMaterialMaps, 0);
	settings->Get("HiresMaterialMapsBuild", &bHiresMaterialMapsBuild, false);
	settings->Get("ConvertHiresTextures", &bConvertHiresTextures, 0);
	settings->Get("CacheHiresTextures", &bCacheHiresTextures, 0);
	settings->Get("DumpEFBTarget", &bDumpEFBTarget, 0);
	settings->Get("DumpFramesAsImages", &bDumpFramesAsImages, 0);
	settings->Get("FreeLook", &bFreeLook, 0);
	settings->Get("CompileShaderOnStartup", &bCompileShaderOnStartup, 0);
	settings->Get("UseFFV1", &bUseFFV1, 0);
	settings->Get("DumpFormat", &sDumpFormat, "avi");
	settings->Get("DumpCodec", &sDumpCodec, "");
	settings->Get("DumpPath", &sDumpPath, "");
	settings->Get("BitrateKbps", &iBitrateKbps, 2500);
	settings->Get("InternalResolutionFrameDumps", &bInternalResolutionFrameDumps, 0);
	settings->Get("EnablePixelLighting", &bEnablePixelLighting, 0);
	settings->Get("ForcedLighting", &bForcedLighting, 0);
	settings->Get("ForcePhongShading", &bForcePhongShading, 0);
	settings->Get("RimPower", &iRimPower, 80);
	settings->Get("RimIntesity", &iRimIntesity, 0);
	settings->Get("RimBase", &iRimBase, 10);
	settings->Get("SpecularMultiplier", &iSpecularMultiplier, 255);

	settings->Get("SimBumpEnabled", &bSimBumpEnabled, 0);
	settings->Get("SimBumpStrength", &iSimBumpStrength, 0);
	settings->Get("SimBumpDetailFrequency", &iSimBumpDetailFrequency, 128);
	settings->Get("SimBumpThreshold", &iSimBumpThreshold, 16);
	settings->Get("SimBumpDetailBlend", &iSimBumpDetailBlend, 16);

	settings->Get("FastDepthCalc", &bFastDepthCalc, true);
	settings->Get("MSAA", &iMultisamples, 1);

// Retina displays on macOS seemingly have worse performance when defaulted to @2x.
#ifdef __APPLE__
	settings->Get("EFBScale", &iEFBScale, (int)SCALE_1X);
#else
	settings->Get("EFBScale", &iEFBScale, (int)SCALE_2X);
#endif

	settings->Get("TexFmtOverlayEnable", &bTexFmtOverlayEnable, 0);
	settings->Get("TexFmtOverlayCenter", &bTexFmtOverlayCenter, 0);
	settings->Get("WireFrame", &bWireFrame, 0);
	settings->Get("DisableFog", &bDisableFog, 0);
	settings->Get("SSAA", &bSSAA, false);
	settings->Get("EnableOpenCL", &bEnableOpenCL, false);
	settings->Get("BorderlessFullscreen", &bBorderlessFullscreen, false);

	settings->Get("SWZComploc", &bZComploc, true);
	settings->Get("SWZFreeze", &bZFreeze, true);
	settings->Get("SWDumpObjects", &bDumpObjects, false);
	settings->Get("SWDumpTevStages", &bDumpTevStages, false);
	settings->Get("SWDumpTevTexFetches", &bDumpTevTextureFetches, false);
	settings->Get("SWDrawStart", &drawStart, 0);
	settings->Get("SWDrawEnd", &drawEnd, 100000);

	settings->Get("EnableValidationLayer", &bEnableValidationLayer, false);
	settings->Get("BackendMultithreading", &bBackendMultithreading, true);
	settings->Get("CommandBufferExecuteInterval", &iCommandBufferExecuteInterval, 100);

	IniFile::Section* enhancements = iniFile.GetOrCreateSection("Enhancements");
	enhancements->Get("ForceFiltering", &bForceFiltering, 0);
	enhancements->Get("DisableFiltering", &bDisableTextureFiltering, 0);
	enhancements->Get("MaxAnisotropy", &iMaxAnisotropy, 0);  // NOTE - this is x in (1 << x)
	enhancements->Get("PostProcessingEnable", &bPostProcessingEnable, false);
	enhancements->Get("PostProcessingTrigger", &iPostProcessingTrigger, 0);
	enhancements->Get("PostProcessingShaders", &sPostProcessingShaders, "");
	enhancements->Get("ScalingShader", &sScalingShader, "");
	enhancements->Get("UseScalingFilter", &bUseScalingFilter, true);
	enhancements->Get("TextureScalingType", &iTexScalingType, 0);
	enhancements->Get("TextureScalingFactor", &iTexScalingFactor, 2);
	enhancements->Get("UseDePosterize", &bTexDeposterize, true);
	enhancements->Get("Tessellation", &bTessellation, 0);
	enhancements->Get("TessellationEarlyCulling", &bTessellationEarlyCulling, 0);
	enhancements->Get("TessellationDistance", &iTessellationDistance, 0);
	enhancements->Get("TessellationMax", &iTessellationMax, 6);
	enhancements->Get("TessellationRoundingIntensity", &iTessellationRoundingIntensity, 0);
	enhancements->Get("TessellationDisplacementIntensity", &iTessellationDisplacementIntensity, 0);
	enhancements->Get("ForceTrueColor", &bForceTrueColor, false);

	IniFile::Section* stereoscopy = iniFile.GetOrCreateSection("Stereoscopy");
	stereoscopy->Get("StereoMode", &iStereoMode, 0);
	stereoscopy->Get("StereoDepth", &iStereoDepth, 20);
	stereoscopy->Get("StereoConvergencePercentage", &iStereoConvergencePercentage, 100);
	stereoscopy->Get("StereoSwapEyes", &bStereoSwapEyes, false);
	stereoscopy->Get("StereoShader", &sStereoShader, "Anaglyph/dubois");

	IniFile::Section* hacks = iniFile.GetOrCreateSection("Hacks");
	hacks->Get("EFBAccessEnable", &bEFBAccessEnable, false);
	hacks->Get("EFBFastAccess", &bEFBFastAccess, false);
	hacks->Get("ForceProgressive", &bForceProgressive, true);
	hacks->Get("EFBToTextureEnable", &bSkipEFBCopyToRam, true);
#ifdef IS_PLAYBACK
	hacks->Get("EFBScaledCopy", &bCopyEFBScaled, true);
#else
	hacks->Get("EFBScaledCopy", &bCopyEFBScaled, false);
#endif
	hacks->Get("EFBEmulateFormatChanges", &bEFBEmulateFormatChanges, false);
	hacks->Get("ForceDualSourceBlend", &bForceDualSourceBlend, false);
	hacks->Get("FullAsyncShaderCompilation", &bFullAsyncShaderCompilation, true);
	hacks->Get("WaitForShaderCompilation", &bWaitForShaderCompilation, false);
	hacks->Get("EnableGPUTextureDecoding", &bEnableGPUTextureDecoding, false);
	hacks->Get("EnableComputeTextureEncoding", &bEnableComputeTextureEncoding, false);
	hacks->Get("PredictiveFifo", &bPredictiveFifo, false);
	hacks->Get("BoundingBoxMode", &iBBoxMode, (int)BBoxMode::BBoxNone);
	hacks->Get("LastStoryEFBToRam", &bLastStoryEFBToRam, false);
	hacks->Get("ForceLogicOpBlend", &bForceLogicOpBlend, false);
	hacks->Get("VertexRounding", &bVertexRounding, false);
	

	// hacks which are disabled by default
	iPhackvalue[0] = 0;
	bPerfQueriesEnable = false;

	// Load common settings
	iniFile.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	IniFile::Section* interface = iniFile.GetOrCreateSection("Interface");
	bool bTmp;
	interface->Get("UsePanicHandlers", &bTmp, true);
	SetEnableAlert(bTmp);

	// Shader Debugging causes a huge slowdown and it's easy to forget about it
	// since it's not exposed in the settings dialog. It's only used by
	// developers, so displaying an obnoxious message avoids some confusion and
	// is not too annoying/confusing for users.
	//
	// XXX(delroth): This is kind of a bad place to put this, but the current
	// VideoCommon is a mess and we don't have a central initialization
	// function to do these kind of checks. Instead, the init code is
	// triplicated for each video backend.
	VerifyValidity();
}

void VideoConfig::GameIniLoad()
{
	bool gfx_override_exists = false;

	// XXX: Again, bad place to put OSD messages at (see delroth's comment above)
	// XXX: This will add an OSD message for each projection hack value... meh
#define CHECK_SETTING(section, key, var) do { \
		decltype(var) temp = var; \
		if (iniFile.GetIfExists(section, key, &var) && var != temp) { \
			std::string msg = StringFromFormat("Note: Option \"%s\" is overridden by game ini.", key); \
			OSD::AddMessage(msg, 7500); \
			gfx_override_exists = true; \
		} \
	} while (0)

	IniFile iniFile = SConfig::GetInstance().LoadGameIni();

	CHECK_SETTING("Video_Hardware", "VSync", bVSync);

	CHECK_SETTING("Video_Settings", "wideScreenHack", bWidescreenHack);
	CHECK_SETTING("Video_Settings", "AspectRatio", iAspectRatio);
	CHECK_SETTING("Video_Settings", "Crop", bCrop);
	CHECK_SETTING("Video_Settings", "UseXFB", bUseXFB);
	CHECK_SETTING("Video_Settings", "UseRealXFB", bUseRealXFB);
	CHECK_SETTING("Video_Settings", "SafeTextureCacheColorSamples", iSafeTextureCache_ColorSamples);
	CHECK_SETTING("Video_Settings", "HiresTextures", bHiresTextures);
	CHECK_SETTING("Video_Settings", "HiresMaterialMaps", bHiresMaterialMaps);

	CHECK_SETTING("Video_Settings", "CacheHiresTextures", bCacheHiresTextures);
	CHECK_SETTING("Video_Settings", "EnablePixelLighting", bEnablePixelLighting);
	CHECK_SETTING("Video_Settings", "ForcedLighting", bForcedLighting);

	CHECK_SETTING("Video_Settings", "ForcePhongShading", bForcePhongShading);
	CHECK_SETTING("Video_Settings", "RimPower", iRimPower);
	CHECK_SETTING("Video_Settings", "RimIntesity", iRimIntesity);
	CHECK_SETTING("Video_Settings", "RimBase", iRimBase);
	CHECK_SETTING("Video_Settings", "SpecularMultiplier", iSpecularMultiplier);
	CHECK_SETTING("Video_Settings", "SimBumpEnabled", bSimBumpEnabled);
	CHECK_SETTING("Video_Settings", "SimBumpStrength", iSimBumpStrength);
	CHECK_SETTING("Video_Settings", "SimBumpDetailFrequency", iSimBumpDetailFrequency);
	CHECK_SETTING("Video_Settings", "SimBumpThreshold", iSimBumpThreshold);
	CHECK_SETTING("Video_Settings", "SimBumpDetailBlend", iSimBumpDetailBlend);

	CHECK_SETTING("Video_Settings", "FastDepthCalc", bFastDepthCalc);
	CHECK_SETTING("Video_Settings", "MSAA", iMultisamples);
	CHECK_SETTING("Video_Settings", "SSAA", bSSAA);
	int tmp = -9000;
	CHECK_SETTING("Video_Settings", "EFBScale", tmp); // integral
	if (tmp != -9000)
	{
		if (tmp != SCALE_FORCE_INTEGRAL)
		{
			iEFBScale = tmp;
		}
		else // Round down to multiple of native IR
		{
			switch (iEFBScale)
			{
			case SCALE_AUTO:
				iEFBScale = SCALE_AUTO_INTEGRAL;
				break;
			case SCALE_1_5X:
				iEFBScale = SCALE_1X;
				break;
			case SCALE_2_5X:
				iEFBScale = SCALE_2X;
				break;
			default:
				break;
			}
		}
	}

	CHECK_SETTING("Video_Settings", "DisableFog", bDisableFog);
	CHECK_SETTING("Video_Settings", "EnableOpenCL", bEnableOpenCL);
	CHECK_SETTING("Video_Settings", "BackendMultithreading", bBackendMultithreading);
	CHECK_SETTING("Video_Settings", "CommandBufferExecuteInterval", iCommandBufferExecuteInterval);

	// These are not overrides, they are per-game stereoscopy parameters, hence no warning
	iniFile.GetIfExists("Video_Stereoscopy", "StereoConvergence", &iStereoConvergence, 20);
	iniFile.GetIfExists("Video_Stereoscopy", "StereoEFBMonoDepth", &bStereoEFBMonoDepth, false);
	iniFile.GetIfExists("Video_Stereoscopy", "StereoDepthPercentage", &iStereoDepthPercentage, 100);

	CHECK_SETTING("Video_Enhancements", "ForceFiltering", bForceFiltering);
	CHECK_SETTING("Video_Enhancements", "DisableFiltering", bDisableTextureFiltering);
	CHECK_SETTING("Video_Enhancements", "MaxAnisotropy", iMaxAnisotropy);  // NOTE - this is x in (1 << x)
	CHECK_SETTING("Video_Enhancements", "UseScalingFilter", bUseScalingFilter);
	CHECK_SETTING("Video_Enhancements", "TextureScalingType", iTexScalingType);
	CHECK_SETTING("Video_Enhancements", "TextureScalingFactor", iTexScalingFactor);
	CHECK_SETTING("Video_Enhancements", "UseDePosterize", bTexDeposterize);
	CHECK_SETTING("Video_Enhancements", "Tessellation", bTessellation);
	CHECK_SETTING("Video_Enhancements", "TessellationEarlyCulling", bTessellationEarlyCulling);
	CHECK_SETTING("Video_Enhancements", "TessellationDistance", iTessellationDistance);
	CHECK_SETTING("Video_Enhancements", "TessellationMax", iTessellationMax);
	CHECK_SETTING("Video_Enhancements", "TessellationRoundingIntensity", iTessellationRoundingIntensity);
	CHECK_SETTING("Video_Enhancements", "TessellationDisplacementIntensity", iTessellationDisplacementIntensity);
	CHECK_SETTING("Video_Enhancements", "PostProcessingEnable", bPostProcessingEnable);
	CHECK_SETTING("Video_Enhancements", "PostProcessingTrigger", iPostProcessingTrigger);
	CHECK_SETTING("Video_Enhancements", "PostProcessingShaders", sPostProcessingShaders);
	CHECK_SETTING("Video_Enhancements", "ScalingShader", sScalingShader);
	CHECK_SETTING("Video_Enhancements", "ForceTrueColor", bForceTrueColor);

	CHECK_SETTING("Video_Stereoscopy", "StereoMode", iStereoMode);
	CHECK_SETTING("Video_Stereoscopy", "StereoDepth", iStereoDepth);
	CHECK_SETTING("Video_Stereoscopy", "StereoSwapEyes", bStereoSwapEyes);
	CHECK_SETTING("Video_Stereoscopy", "StereoShader", sStereoShader);

	CHECK_SETTING("Video_Hacks", "EFBAccessEnable", bEFBAccessEnable);
	CHECK_SETTING("Video_Hacks", "EFBFastAccess", bEFBFastAccess);
	CHECK_SETTING("Video_Hacks", "ForceProgressive", bForceProgressive);
	CHECK_SETTING("Video_Hacks", "EFBToTextureEnable", bSkipEFBCopyToRam);
	CHECK_SETTING("Video_Hacks", "EFBScaledCopy", bCopyEFBScaled);
	CHECK_SETTING("Video_Hacks", "EFBEmulateFormatChanges", bEFBEmulateFormatChanges);
	CHECK_SETTING("Video_Hacks", "BoundingBoxMode", iBBoxMode);
	CHECK_SETTING("Video_Hacks", "LastStoryEFBToRam", bLastStoryEFBToRam);
	CHECK_SETTING("Video_Hacks", "VertexRounding", bVertexRounding);

	CHECK_SETTING("Video", "ProjectionHack", iPhackvalue[0]);
	CHECK_SETTING("Video", "PH_SZNear", iPhackvalue[1]);
	CHECK_SETTING("Video", "PH_SZFar", iPhackvalue[2]);
	CHECK_SETTING("Video", "PH_ExtraParam", iPhackvalue[3]);
	CHECK_SETTING("Video", "PH_ZNear", sPhackvalue[0]);
	CHECK_SETTING("Video", "PH_ZFar", sPhackvalue[1]);
	CHECK_SETTING("Video", "PerfQueriesEnable", bPerfQueriesEnable);
	CHECK_SETTING("Video", "FullAsyncShaderCompilation", bFullAsyncShaderCompilation);
	CHECK_SETTING("Video", "WaitForShaderCompilation", bWaitForShaderCompilation);
	CHECK_SETTING("Video", "EnableGPUTextureDecoding", bEnableGPUTextureDecoding);
	CHECK_SETTING("Video", "EnableComputeTextureEncoding", bEnableComputeTextureEncoding);
	CHECK_SETTING("Video", "PredictiveFifo", bPredictiveFifo);
	if (gfx_override_exists)
		OSD::AddMessage("Warning: Opening the graphics configuration will reset settings and might cause issues!", 10000);
}

void VideoConfig::VerifyValidity()
{
	// Disable while is unstable
	bEnableOpenCL = false;
	// TODO: Check iMaxAnisotropy value
	if (iAdapter < 0 || iAdapter >((int)backend_info.Adapters.size() - 1))
		iAdapter = 0;
	if (std::find(backend_info.AAModes.begin(), backend_info.AAModes.end(), iMultisamples) == backend_info.AAModes.end())
		iMultisamples = 1;
	if (!backend_info.bSupportsPixelLighting) bEnablePixelLighting = false;
	bForcePhongShading = bForcePhongShading && bEnablePixelLighting;
	bForcedLighting = bForcedLighting && bEnablePixelLighting;
	iRimPower = std::min(std::max(iRimPower, 0), 255);
	iRimIntesity = std::min(std::max(iRimIntesity, 0), 255);
	iRimBase = std::min(std::max(iRimBase, 0), 127);
	iSpecularMultiplier = std::min(std::max(iSpecularMultiplier, 0), 510);
	iSimBumpStrength = std::min(std::max(iSimBumpStrength, 0), 1024);
	iSimBumpDetailBlend = std::min(std::max(iSimBumpDetailBlend, 0), 255);
	iSimBumpDetailFrequency = std::min(std::max(iSimBumpDetailFrequency, 4), 255);
	iSimBumpThreshold = std::min(std::max(iSimBumpThreshold, 0), 255);
	iTessellationMax = iTessellationMax < 2 ? 2 : (iTessellationMax > 63 ? 63 : iTessellationMax);
	iTessellationRoundingIntensity = iTessellationRoundingIntensity > 100 ? 100 : (iTessellationRoundingIntensity < 0 ? 0 : iTessellationRoundingIntensity);
	iTessellationDisplacementIntensity = iTessellationDisplacementIntensity > 300 ? 300 : (iTessellationDisplacementIntensity < 0 ? 0 : iTessellationDisplacementIntensity);
	if (iStereoMode > 0)
	{
		if (!backend_info.bSupportsGeometryShaders)
		{
			OSD::AddMessage("Stereoscopic 3D isn't supported by your GPU, support for OpenGL 3.2 is required.", 10000);
			iStereoMode = 0;
		}
		if (bUseXFB && bUseRealXFB)
		{
			OSD::AddMessage("Stereoscopic 3D isn't supported with Real XFB, turning off stereoscopy.", 10000);
			iStereoMode = 0;
		}
	}
	// Disable while reimplementing predictive fifo
	bPredictiveFifo = false;
	bWaitForShaderCompilation = false;
	if (iBBoxMode > BBoxGPU || iBBoxMode < BBoxNone)
	{
		iBBoxMode = BBoxNone;
	}
	if (backend_info.APIType & API_D3D9 && iBBoxMode == BBoxGPU)
	{
		iBBoxMode = BBoxCPU;
	}
	if (iTexScalingFactor < 2)
	{
		iTexScalingFactor = 2;
	}
	else if (iTexScalingFactor > 5)
	{
		iTexScalingFactor = 5;
	}
	if (iTexScalingType < 0)
	{
		iTexScalingType = 0;
	}
	else if (iTexScalingType > 10)
	{
		iTexScalingType = 10;
	}
	bHiresMaterialMaps = bHiresMaterialMaps && bHiresTextures && bEnablePixelLighting;
	bLastStoryEFBToRam = bLastStoryEFBToRam && StringStartsWith(SConfig::GetInstance().GetGameID(), "SLS");
}

void VideoConfig::Save(const std::string& ini_file)
{
	IniFile iniFile;
	iniFile.Load(ini_file);

	IniFile::Section* hardware = iniFile.GetOrCreateSection("Hardware");
	hardware->Set("VSync", bVSync);
	hardware->Set("Adapter", iAdapter);

	IniFile::Section* settings = iniFile.GetOrCreateSection("Settings");
	settings->Set("AspectRatio", iAspectRatio);
	settings->Set("Crop", bCrop);
	settings->Set("wideScreenHack", bWidescreenHack);
	settings->Set("UseXFB", bUseXFB);
	settings->Set("UseRealXFB", bUseRealXFB);
	settings->Set("SafeTextureCacheColorSamples", iSafeTextureCache_ColorSamples);
	settings->Set("ShowFPS", bShowFPS);
	settings->Set("ShowNetPlayPing", bShowNetPlayPing);
	settings->Set("ShowNetPlayMessages", bShowNetPlayMessages);
    settings->Set("ShowOSDClock", bShowOSDClock);
    settings->Set("ShowFrameTimes", bShowFrameTimes);
	settings->Set("LogRenderTimeToFile", bLogRenderTimeToFile);
	settings->Set("ShowInputDisplay", bShowInputDisplay);
	settings->Set("OverlayStats", bOverlayStats);
	settings->Set("OverlayProjStats", bOverlayProjStats);
	settings->Set("DumpTextures", bDumpTextures);
	settings->Set("DumpVertexLoader", bDumpVertexLoaders);
	settings->Set("HiresTextures", bHiresTextures);
	settings->Set("HiresMaterialMaps", bHiresMaterialMaps);
	settings->Set("HiresMaterialMapsBuild", bHiresMaterialMapsBuild);
	settings->Set("ConvertHiresTextures", bConvertHiresTextures);
	settings->Set("CacheHiresTextures", bCacheHiresTextures);
	settings->Set("DumpEFBTarget", bDumpEFBTarget);
	settings->Set("DumpFramesAsImages", bDumpFramesAsImages);
	settings->Set("FreeLook", bFreeLook);
	settings->Set("InternalResolutionFrameDumps", bInternalResolutionFrameDumps);
	settings->Set("CompileShaderOnStartup", bCompileShaderOnStartup);
	settings->Set("UseFFV1", bUseFFV1);
	settings->Set("DumpFormat", sDumpFormat);
	settings->Set("DumpCodec", sDumpCodec);
	settings->Set("DumpPath", sDumpPath);
	settings->Set("BitrateKbps", iBitrateKbps);
	settings->Set("EnablePixelLighting", bEnablePixelLighting);
	settings->Set("ForcedLighting", bForcedLighting);
	settings->Set("ForcePhongShading", bForcePhongShading);
	settings->Set("RimPower", iRimPower);
	settings->Set("RimIntesity", iRimIntesity);
	settings->Set("RimBase", iRimBase);
	settings->Set("SpecularMultiplier", iSpecularMultiplier);

	settings->Set("SimBumpEnabled", bSimBumpEnabled);
	settings->Set("SimBumpStrength", iSimBumpStrength);
	settings->Set("SimBumpDetailFrequency", iSimBumpDetailFrequency);
	settings->Set("SimBumpThreshold", iSimBumpThreshold);
	settings->Set("SimBumpDetailBlend", iSimBumpDetailBlend);

	settings->Set("FastDepthCalc", bFastDepthCalc);
	settings->Set("MSAA", iMultisamples);
	settings->Set("SSAA", bSSAA);
	settings->Set("EFBScale", iEFBScale);
	settings->Set("TexFmtOverlayEnable", bTexFmtOverlayEnable);
	settings->Set("TexFmtOverlayCenter", bTexFmtOverlayCenter);
	settings->Set("Wireframe", bWireFrame);
	settings->Set("DisableFog", bDisableFog);

	settings->Set("EnableOpenCL", bEnableOpenCL);
	settings->Set("BorderlessFullscreen", bBorderlessFullscreen);

	settings->Set("SWZComploc", bZComploc);
	settings->Set("SWZFreeze", bZFreeze);
	settings->Set("SWDumpObjects", bDumpObjects);
	settings->Set("SWDumpTevStages", bDumpTevStages);
	settings->Set("SWDumpTevTexFetches", bDumpTevTextureFetches);
	settings->Set("SWDrawStart", drawStart);
	settings->Set("SWDrawEnd", drawEnd);

	settings->Set("EnableValidationLayer", bEnableValidationLayer);
	settings->Set("BackendMultithreading", bBackendMultithreading);
	settings->Set("CommandBufferExecuteInterval", iCommandBufferExecuteInterval);

	IniFile::Section* enhancements = iniFile.GetOrCreateSection("Enhancements");
	enhancements->Set("ForceFiltering", bForceFiltering);
	enhancements->Set("DisableFiltering", bDisableTextureFiltering);
	enhancements->Set("MaxAnisotropy", iMaxAnisotropy);
	enhancements->Set("PostProcessingEnable", bPostProcessingEnable);
	enhancements->Set("PostProcessingTrigger", iPostProcessingTrigger);
	enhancements->Set("PostProcessingShaders", sPostProcessingShaders);
	enhancements->Set("ScalingShader", sScalingShader);
	enhancements->Set("UseScalingFilter", bUseScalingFilter);
	enhancements->Set("TextureScalingType", iTexScalingType);
	enhancements->Set("TextureScalingFactor", iTexScalingFactor);
	enhancements->Set("UseDePosterize", bTexDeposterize);
	enhancements->Set("Tessellation", bTessellation);
	enhancements->Set("TessellationEarlyCulling", bTessellationEarlyCulling);
	enhancements->Set("TessellationDistance", iTessellationDistance);
	enhancements->Set("TessellationMax", iTessellationMax);
	enhancements->Set("TessellationRoundingIntensity", iTessellationRoundingIntensity);
	enhancements->Set("TessellationDisplacementIntensity", iTessellationDisplacementIntensity);
	enhancements->Set("ForceTrueColor", bForceTrueColor);

	IniFile::Section* stereoscopy = iniFile.GetOrCreateSection("Stereoscopy");
	stereoscopy->Set("StereoMode", iStereoMode);
	stereoscopy->Set("StereoDepth", iStereoDepth);
	stereoscopy->Set("StereoConvergencePercentage", iStereoConvergencePercentage);
	stereoscopy->Set("StereoSwapEyes", bStereoSwapEyes);
	stereoscopy->Set("StereoShader", sStereoShader);

	IniFile::Section* hacks = iniFile.GetOrCreateSection("Hacks");
	hacks->Set("EFBAccessEnable", bEFBAccessEnable);
	hacks->Set("EFBFastAccess", bEFBFastAccess);
	hacks->Set("ForceProgressive", bForceProgressive);
	hacks->Set("EFBToTextureEnable", bSkipEFBCopyToRam);
	hacks->Set("EFBScaledCopy", bCopyEFBScaled);
	hacks->Set("EFBEmulateFormatChanges", bEFBEmulateFormatChanges);
	hacks->Set("ForceDualSourceBlend", bForceDualSourceBlend);
	hacks->Set("FullAsyncShaderCompilation", bFullAsyncShaderCompilation);
	hacks->Set("WaitForShaderCompilation", bWaitForShaderCompilation);
	hacks->Set("EnableGPUTextureDecoding", bEnableGPUTextureDecoding);
	hacks->Set("EnableComputeTextureEncoding", bEnableComputeTextureEncoding);
	hacks->Set("PredictiveFifo", bPredictiveFifo);
	hacks->Set("BoundingBoxMode", iBBoxMode);
	hacks->Set("LastStoryEFBToRam", bLastStoryEFBToRam);
	hacks->Set("ForceLogicOpBlend", bForceLogicOpBlend);
	hacks->Set("VertexRounding", bVertexRounding);


	iniFile.Save(ini_file);
}

bool VideoConfig::IsVSync() const
{
	return bVSync && !Core::GetIsThrottlerTempDisabled();
}

bool VideoConfig::PixelLightingEnabled(const XFMemory& xfr, const u32 components) const
{
	return (xfr.numChan.numColorChans > 0) && bEnablePixelLighting && backend_info.bSupportsPixelLighting && ((components & VB_HAS_NRM0) == VB_HAS_NRM0);
}
