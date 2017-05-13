set(SRCS	AsyncRequests.cpp
			BoundingBox.cpp
			BPFunctions.cpp
			BPMemory.cpp
			BPStructs.cpp
			CPMemory.cpp
			CommandProcessor.cpp
			Debugger.cpp
			DDSLoader.cpp
			DriverDetails.cpp
			Fifo.cpp
			FPSCounter.cpp
			FramebufferManagerBase.cpp
			GeometryShaderGen.cpp
			GeometryShaderManager.cpp
			G_G4BP08_pvt.cpp
			G_GB4P51_pvt.cpp
			G_GFZE01_pvt.cpp
			G_GLMP01_pvt.cpp
			G_GM8E01_pvt.cpp
			G_GNUEDA_pvt.cpp
			G_GSAE01_pvt.cpp
			G_GZ2P01_pvt.cpp
			G_R5WEA4_pvt.cpp
			G_RBUP08_pvt.cpp
			G_RMCP01_pvt.cpp
			G_RMGP01_pvt.cpp
			G_RSBP01_pvt.cpp
			G_SDWP18_pvt.cpp
			G_SMNP01_pvt.cpp
			G_SPDE52_pvt.cpp
			G_SPXP41_pvt.cpp
			G_SX4E01_pvt.cpp
			HiresTextures.cpp
			ImageWrite.cpp
			IndexGenerator.cpp
			MainBase.cpp
			OnScreenDisplay.cpp
			OpcodeDecoding.cpp
			PerfQueryBase.cpp
			PixelEngine.cpp
			PixelShaderGen.cpp
			PixelShaderManager.cpp
			PNGLoader.cpp
			PostProcessing.cpp
			RenderBase.cpp
			Statistics.cpp
			TessellationShaderGen.cpp
			TessellationShaderManager.cpp
			TextureCacheBase.cpp
			TextureConversionShaderGL.cpp
			TextureUtil.cpp
			TextureScalerCommon.cpp
			VertexLoader.cpp
			VertexLoaderBase.cpp
			VertexLoaderCompiled.cpp
			VertexLoaderManager.cpp
			VertexLoader_Mtx.cpp
			VertexLoader_Color.cpp
			VertexLoader_Normal.cpp
			VertexLoader_Position.cpp
			VertexLoader_TextCoord.cpp
			VertexManagerBase.cpp
			VertexShaderGen.cpp
			VertexShaderManager.cpp
			VideoBackendBase.cpp
			VideoConfig.cpp
			VideoState.cpp
			XFMemory.cpp
			XFStructs.cpp)
set(LIBS core png xbrz)

if(_M_X86)
	set(SRCS ${SRCS} x64TextureDecoder.cpp VertexLoaderX64.cpp)
elseif(_M_ARM_64)
	set(SRCS ${SRCS} VertexLoaderARM64.cpp TextureDecoder_Generic.cpp)
else()
	set(SRCS ${SRCS} TextureDecoder_Generic.cpp)
endif()

if(LIBAV_FOUND OR WIN32)
	set(SRCS ${SRCS} AVIDump.cpp)
endif()

add_dolphin_library(videocommon "${SRCS}" "${LIBS}")

if(LIBAV_FOUND)
	target_link_libraries(videocommon PRIVATE ${LIBS} ${LIBAV_LIBRARIES})
endif()
