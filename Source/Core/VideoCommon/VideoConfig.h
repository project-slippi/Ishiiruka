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

// Log in two categories, and save three other options in the same byte
#define CONF_LOG          1
#define CONF_PRIMLOG      2
#define CONF_SAVETARGETS  8
#define CONF_SAVESHADERS  16

enum AspectMode
{
	ASPECT_AUTO        = 0,
	ASPECT_ANALOG_WIDE = 1,
	ASPECT_ANALOG      = 2,
	ASPECT_STRETCH     = 3,
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
	STEREO_ANAGLYPH,
	STEREO_INTERLACED,
	STEREO_3DVISION,	
};

constexpr int STEREOSCOPY_PRESETS_NUM = 3;

struct StereoscopyPreset final
{
   int depth;
   int convergence;
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
	bool IsVSync();

	// General
	bool bVSync;
	bool bExclusiveMode;
	bool bFullscreen;
	bool bRunning;
	bool bWidescreenHack;
	int iAspectRatio;
	bool bCrop;   // Aspect ratio controls.
	bool bUseXFB;
	bool bUseRealXFB;

	// OpenCL/OpenMP
	bool bEnableOpenCL;
	bool bOMPDecoder;

	// Enhancements
	int iMultisampleMode;
	bool bSSAA;
	int iEFBScale;
	bool bForceFiltering;
	int iMaxAnisotropy;
	std::string sPostProcessingShader;
	int iStereoMode;
	int iStereoDepth;
	int iStereoConvergence;
	bool bStereoSwapEyes;
	bool bUseScalingFilter;	
	bool bTexDeposterize;
	int iTexScalingType;
	int iTexScalingFactor;
	std::array<StereoscopyPreset, STEREOSCOPY_PRESETS_NUM> oStereoPresets;
	int iStereoActivePreset;
	// Information
	bool bShowFPS;
	bool bShowInputDisplay;
	bool bOverlayStats;
	bool bOverlayProjStats;
	bool bTexFmtOverlayEnable;
	bool bTexFmtOverlayCenter;
	bool bShowEFBCopyRegions;
	bool bLogRenderTimeToFile;

	// Render
	bool bWireFrame;
	bool bDisableFog;

	// Utility
	bool bDumpTextures;
	bool bDumpVertexLoaders;
	bool bHiresTextures;
	bool bHiresMaterialMaps;
	bool bConvertHiresTextures;
	bool bCacheHiresTextures;
	bool bCacheHiresTexturesGPU;
	bool bDumpEFBTarget;
	bool bUseFFV1;
	bool bFreeLook;
	bool bBorderlessFullscreen;

	// Hacks
	bool bEFBAccessEnable;
	bool bEFBFastAccess;
	bool bForceProgressive;
	bool bPerfQueriesEnable;
	bool bFullAsyncShaderCompilation;
	bool bPredictiveFifo;
	bool bWaitForShaderCompilation;
	bool bEFBEmulateFormatChanges;
	bool bSkipEFBCopyToRam;
	bool bCopyEFBScaled;
	int iSafeTextureCache_ColorSamples;
	int iPhackvalue[4];
	std::string sPhackvalue[2];
	float fAspectRatioHackW, fAspectRatioHackH;
	bool bZTPSpeedHack; // The Legend of Zelda: Twilight Princess
	bool bEnablePixelLighting;
	bool bForcePhongShading;
	bool bFastDepthCalc;
	int iBBoxMode;
	//for dx9-backend
	bool bForceDualSourceBlend;
	int iLog; // CONF_ bits
	int iSaveTargetId; // TODO: Should be dropped

	// Stereoscopy
	bool bStereoEFBMonoDepth;
	int iStereoDepthPercentage;
	int iStereoConvergenceMinimum;

	// D3D only config, mostly to be merged into the above
	int iAdapter;

	// Debugging
	bool bEnableShaderDebugging;

	// Static config per API
	// TODO: Move this out of VideoConfig
	struct
	{
		API_TYPE APIType;

		std::vector<std::string> Adapters; // for D3D9 and D3D11
		std::vector<std::string> AAModes;
		std::vector<std::string> PPShaders; // post-processing shaders
		std::vector<std::string> AnaglyphShaders; // anaglyph shaders

		bool bSupportedFormats[16]; // used for D3D9 in TextureCache		
		bool bSupportsDualSourceBlend; // only supported by D3D11 and OpenGL
		bool bSupportsPixelLighting;
		bool bSupportsNormalMaps;
		bool bSupportsPrimitiveRestart;
		bool bSupportsSeparateAlphaFunction;
		bool bSupportsBindingLayout; // needed by PixelShaderGen, so must stay in VideoCommon
		bool bSupportsEarlyZ; // needed by PixelShaderGen, so must stay in VideoCommon
		bool bNeedBlendIndices; // needed by PixelShaderGen, so must stay in VideoCommon
		bool bSupportsOversizedViewports;
		bool bSupportsPostProcessing;
		bool bSupportsGeometryShaders;
		bool bSupports3DVision;
		bool bSupportsExclusiveFullscreen;
		bool bSupportsBBox;
		bool bSupportsGSInstancing; // Needed by GeometryShaderGen, so must stay in VideoCommon
		bool bSupportsPaletteConversion;
		bool bSupportsClipControl; // Needed by VertexShaderGen, so must stay in VideoCommon		
		bool bSupportsSSAA;
	} backend_info;

	// Utility
	bool RealXFBEnabled() const { return bUseXFB && bUseRealXFB; }
	bool VirtualXFBEnabled() const { return bUseXFB && !bUseRealXFB; }
	bool ExclusiveFullscreenEnabled() const { return backend_info.bSupportsExclusiveFullscreen && !bBorderlessFullscreen; }
	bool HiresMaterialMapsEnabled() const { return backend_info.bSupportsNormalMaps && bHiresTextures && bHiresMaterialMaps; }
};

extern VideoConfig g_Config;
extern VideoConfig g_ActiveConfig;

// Called every frame.
void UpdateActiveConfig();
