/* $Id: $

   !!! CMake generated file, do NOT edit! Use CMake-GUI to change configuration instead. !!!

   Header file configured by CMake to convert CMake options/vars to macros. It is done this way because if set via
   preprocessor options, MSVC f.i. has no way of knowing when an option (or var) changes as there is no dependency chain.
   
   The generated "options_cmake.h" should be included like so:
   
   #ifdef PORTAUDIO_CMAKE_GENERATED
   #include "options_cmake.h"
   #endif
   
   so that non-CMake build environments are left intact.
   
   Source template: cmake_support/options_cmake.h.in
*/

#ifdef _WIN32
#if defined(PA_USE_ASIO) || defined(PA_USE_DS) || defined(PA_USE_WMME) || defined(PA_USE_WASAPI) || defined(PA_USE_WDMKS)
#error "This header needs to be included before pa_hostapi.h!!"
#endif

#define PA_USE_ASIO 0
#define PA_USE_DS 0
#define PA_USE_WMME 1
#define PA_USE_WASAPI 0
#define PA_USE_WDMKS 1
#else
#error "Platform currently not supported by CMake script"
#endif
