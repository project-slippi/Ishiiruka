include_directories(${CMAKE_SOURCE_DIR}/Externals/SlippiRustExtensions/ffi/includes)

set(SRCS Analytics.cpp
		 BreakPoints.cpp
		 CDUtils.cpp
		 ColorUtil.cpp
		 ENetUtil.cpp
		 FileSearch.cpp
		 FileUtil.cpp
		 GekkoDisassembler.cpp
		 Hash.cpp
		 IniFile.cpp
		 JitRegister.cpp
		 MathUtil.cpp
		 MemArena.cpp
		 MemoryUtil.cpp
		 Misc.cpp
		 MsgHandler.cpp
		 NandPaths.cpp
		 Network.cpp
		 PcapFile.cpp
		 PerformanceCounter.cpp
		 Profiler.cpp
		 SettingsHandler.cpp
		 SDCardUtil.cpp
		 StringUtil.cpp
		 SymbolDB.cpp
		 SysConf.cpp
		 Thread.cpp
		 Timer.cpp
		 TraversalClient.cpp
		 Version.cpp
		 x64ABI.cpp
		 x64Analyzer.cpp
		 x64Emitter.cpp
		 MD5.cpp
		 Crypto/bn.cpp
		 Crypto/ec.cpp
		 Logging/LogManager.cpp)

if(ANDROID)
	set(SRCS ${SRCS}
			 Logging/ConsoleListenerDroid.cpp)
else()
	set(SRCS ${SRCS}
			 Logging/ConsoleListenerNix.cpp)
endif()

list(APPEND LIBS enet ${CURL_LIBRARIES})
if(_M_ARM_64)
	set(SRCS ${SRCS}
			 Arm64Emitter.cpp
			 ArmCPUDetect.cpp
			 GenericFPURoundMode.cpp)
else()
	if(_M_X86) #X86
		set(SRCS ${SRCS}
				 x64FPURoundMode.cpp
				 x64CPUDetect.cpp)
	else() # Generic
		set(SRCS ${SRCS}
				 GenericFPURoundMode.cpp
				 GenericCPUDetect.cpp)
	endif()
endif()

list(APPEND LIBS "${CMAKE_THREAD_LIBS_INIT}" ${VTUNE_LIBRARIES})

# OpenGL Interface
set(SRCS ${SRCS}
	GL/GLUtil.cpp
	GL/GLExtensions/GLExtensions.cpp
	GL/GLInterface/GLInterface.cpp)

if(USE_EGL)
	set(SRCS ${SRCS} GL/GLInterface/EGL.cpp)
	if(ANDROID)
		set(SRCS ${SRCS} GL/GLInterface/EGLAndroid.cpp)
	elseif(USE_X11)
		set(SRCS ${SRCS} GL/GLInterface/EGLX11.cpp)
	endif()
	set(LIBS ${LIBS} EGL)
endif()

if(WIN32)
	set(SRCS ${SRCS} GL/GLInterface/WGL.cpp)
elseif(APPLE)
	set(SRCS ${SRCS} GL/GLInterface/AGL.mm)
elseif(USE_X11)
	if (NOT USE_EGL)
		set(SRCS ${SRCS} GL/GLInterface/GLX.cpp)
		# GLX has a hard dependency on libGL.
		# Make sure to link to it if using GLX.
		set(LIBS ${LIBS} ${OPENGL_LIBRARIES})
	endif()
	set(SRCS ${SRCS} GL/GLInterface/X11_Util.cpp)
	set(LIBS ${LIBS} ${XRANDR_LIBRARIES})
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(LIBS ${LIBS} dl)
endif()

add_dolphin_library(common "${SRCS}" "${LIBS}")
add_executable(traversal_server TraversalServer.cpp)
