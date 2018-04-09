// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.


// IMPORTANT: UI etc should modify g_Config. Graphics code should read g_ActiveConfig.
// The reason for this is to get rid of race conditions etc when the configuration
// changes in the middle of a frame. This is done by copying g_Config to g_ActiveConfig
// at the start of every frame. Noone should ever change members of g_ActiveConfig
// directly.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/XFMemory.h"

// Log in two categories, and save three other options in the same byte
#define CONF_LOG          1
#define CONF_PRIMLOG      2
#define CONF_SAVETARGETS  8
#define CONF_SAVESHADERS  16

enum AspectMode
{
	ASPECT_AUTO = 0,
	ASPECT_ANALOG_WIDE = 1,
	ASPECT_ANALOG = 2,
	ASPECT_STRETCH = 3,
	ASPECT_4_3 = 4,
	ASPECT_73_60 = 5,
	ASPECT_16_9 = 6,
	ASPECT_16_10 = 7,
    ASPECT_INTEGER = 8
};

enum EFBScale
{
	SCALE_FORCE_INTEGRAL = -1,
	SCALE_AUTO,
	SCALE_AUTO_INTEGRAL,
	SCALE_1X,
	SCALE_1_5X,
	SCALE_2X,
	SCALE_2_5X,
	SCALE_3X,
	SCALE_4X,
	SCALE_5X,
	SCALE_6X,
	SCALE_7X,
	SCALE_8X
};

enum StereoMode
{
	STEREO_OFF = 0,
	STEREO_SBS,
	STEREO_TAB,
	STEREO_SHADER,
	STEREO_3DVISION,
};

enum BBoxMode : s32
{
	BBoxNone = 0,
	BBoxCPU = 1,
	BBoxGPU = 2
};



class IniFile;

// NEVER inherit from this class.
struct VideoConfig final
{
	VideoConfig();
	void Load(const std::string& ini_file);
	void GameIniLoad();
	void VerifyValidity();
	void Save(const std::string& ini_file);
	void UpdateProjectionHack();
	bool IsVSync() const;
	bool PixelLightingEnabled(const XFMemory& xfr, const u32 components) const;

	// General
	bool bVSync;
	bool bRunning;
	bool bWidescreenHack;
	int iAspectRatio;

    int GetCurrentAspect();

	bool bCrop;   // Aspect ratio controls.
	bool bUseXFB;
	bool bUseRealXFB;

	// OpenCL/OpenMP
	bool bEnableOpenCL;
	bool bOMPDecoder;

	// Enhancements
	int iMultisamples;
	bool bSSAA;
	int iEFBScale;
	bool bForceFiltering;
	bool bDisableTextureFiltering;
	int iMaxAnisotropy;
	bool bPostProcessingEnable;
	int iPostProcessingTrigger;
	std::string sPostProcessingShaders;
	std::string sScalingShader;
	std::string sStereoShader;
	bool bUseScalingFilter;
	bool bTexDeposterize;
	int iTexScalingType;
	int iTexScalingFactor;
	bool bTessellation;
	bool bTessellationEarlyCulling;
	int iTessellationDistance;
	int iTessellationMax;
	int iTessellationRoundingIntensity;
	int iTessellationDisplacementIntensity;
	bool bForceTrueColor;

	// Information
	bool bShowFPS;
	bool bShowNetPlayPing;
	bool bShowNetPlayMessages;
    bool bShowOSDClock;
    bool bShowFrameTimes;
	bool bShowInputDisplay;
	bool bOverlayStats;
	bool bOverlayProjStats;
	bool bTexFmtOverlayEnable;
	bool bTexFmtOverlayCenter;
	bool bLogRenderTimeToFile;

	// Render
	bool bWireFrame;
	bool bDisableFog;

	// Utility
	bool bDumpTextures;
	bool bDumpVertexLoaders;
	bool bHiresTextures;
	bool bHiresMaterialMaps;
	bool bHiresMaterialMapsBuild;
	bool bConvertHiresTextures;
	bool bCacheHiresTextures;
	bool bDumpEFBTarget;
	bool bDumpFramesAsImages;
	bool bUseFFV1;
	std::string sDumpCodec;
	std::string sDumpFormat;
	std::string sDumpPath;
	bool bInternalResolutionFrameDumps;
	bool bFreeLook;
	bool bBorderlessFullscreen;
	int iBitrateKbps;
	bool bCompileShaderOnStartup;


	// Hacks
	bool bEFBAccessEnable;
	bool bEFBFastAccess;
	bool bForceProgressive;
	bool bPerfQueriesEnable;
	bool bFullAsyncShaderCompilation;
	bool bPredictiveFifo;
	bool bWaitForShaderCompilation;
	bool bEnableGPUTextureDecoding;
	bool bEnableComputeTextureEncoding;
	bool bEFBEmulateFormatChanges;
	bool bSkipEFBCopyToRam;
	bool bCopyEFBScaled;
	int iSafeTextureCache_ColorSamples;
	int iPhackvalue[4];
	std::string sPhackvalue[2];
	float fAspectRatioHackW, fAspectRatioHackH;
	bool bEnablePixelLighting;
	bool bForcedLighting;
	bool bForcePhongShading;
	int iRimPower;
	int iRimIntesity;
	int iRimBase;
	int iSpecularMultiplier;
	bool bLastStoryEFBToRam;
	bool bForceLogicOpBlend;

	bool bSimBumpEnabled;
	int iSimBumpDetailBlend;
	int iSimBumpDetailFrequency;
	int iSimBumpThreshold;
	int iSimBumpStrength;

	bool bFastDepthCalc;
	bool bVertexRounding;
	int iBBoxMode;
	//for dx9-backend
	bool bForceDualSourceBlend;
	int iLog; // CONF_ bits
	int iSaveTargetId; // TODO: Should be dropped

	// Stereoscopy
	int iStereoMode;
	int iStereoDepth;
	int iStereoConvergence;
	int iStereoConvergencePercentage;
	bool bStereoSwapEyes;
	bool bStereoEFBMonoDepth;
	int iStereoDepthPercentage;

	// D3D only config, mostly to be merged into the above
	int iAdapter;

	// VideoSW Debugging
	int drawStart;
	int drawEnd;
	bool bZComploc;
	bool bZFreeze;
	bool bDumpObjects;
	bool bDumpTevStages;
	bool bDumpTevTextureFetches;

	bool bEnableValidationLayer;

	// Multithreaded submission, currently only supported with Vulkan.
	bool bBackendMultithreading;

	// Early command buffer execution interval in number of draws.
	// Currently only supported with Vulkan.
	int iCommandBufferExecuteInterval;

	// Static config per API
	// TODO: Move this out of VideoConfig
	struct
	{
		API_TYPE APIType;

		std::vector<std::string> Adapters; // for D3D9 and D3D11
		std::vector<int> AAModes;

		// TODO: merge AdapterName and Adapters array
		std::string AdapterName; // for OpenGL

		u32 MaxTextureSize;

		bool bSupportedFormats[16]; // used for D3D9 in TextureCache
		bool bSupportsDualSourceBlend; // only supported by D3D11 and OpenGL
		bool bSupportsPixelLighting;
		bool bSupportsNormalMaps;
		bool bSupportsSeparateAlphaFunction;
		bool bSupportsBindingLayout; // needed by PixelShaderGen, so must stay in VideoCommon
		bool bSupportsEarlyZ; // needed by PixelShaderGen, so must stay in VideoCommon
		bool bNeedBlendIndices; // needed by PixelShaderGen, so must stay in VideoCommon
		bool bSupportsOversizedViewports;
		bool bSupportsPostProcessing;
		bool bSupportsGeometryShaders;
		bool bSupportsComputeShaders;
		bool bSupports3DVision;
		bool bSupportsExclusiveFullscreen;
		bool bSupportsBBox;
		bool bSupportsGSInstancing; // Needed by GeometryShaderGen, so must stay in VideoCommon
		bool bSupportsPaletteConversion;
		bool bSupportsClipControl; // Needed by VertexShaderGen, so must stay in VideoCommon
		bool bSupportsSSAA;
		bool bSupportsTessellation;
		bool bSupportsScaling;
		bool bSupportsDepthClamp;  // Needed by VertexShaderGen, so must stay in VideoCommon
		bool bSupportsGPUTextureDecoding;
		bool bSupportsComputeTextureEncoding;
		bool bSupportsMultithreading;
		bool bSupportsValidationLayer;
		bool bSupportsReversedDepthRange;
		bool bSupportsInternalResolutionFrameDumps;
		bool bSupportsAsyncShaderCompilation;
	} backend_info;

	// Utility
	inline bool RealXFBEnabled() const
	{
		return bUseXFB && bUseRealXFB;
	}
	inline bool VirtualXFBEnabled() const
	{
		return bUseXFB && !bUseRealXFB;
	}
	inline bool ExclusiveFullscreenEnabled() const
	{
		return backend_info.bSupportsExclusiveFullscreen && !bBorderlessFullscreen;
	}
	inline bool HiresMaterialMapsEnabled() const
	{
		return backend_info.bSupportsNormalMaps && bHiresTextures && bHiresMaterialMaps;
	}
	inline bool TessellationEnabled() const
	{
		return backend_info.bSupportsTessellation && bTessellation && bEnablePixelLighting;
	}
	inline bool UseGPUTextureDecoding() const
	{
		return backend_info.bSupportsGPUTextureDecoding && bEnableGPUTextureDecoding;
	}
};

extern VideoConfig g_Config;
extern VideoConfig g_ActiveConfig;

// Called every frame.
void UpdateActiveConfig();
