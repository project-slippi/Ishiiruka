# OUR SOURCES
set(SRCS
  png.h
  pngconf.h
  png.c
  pngerror.c
  pngget.c
  pngmem.c
  pngpread.c
  pngread.c
  pngrio.c
  pngrtran.c
  pngrutil.c
  pngset.c
  pngtrans.c
  pngwio.c
  pngwrite.c
  pngwtran.c
  pngwutil.c
)
add_definitions(-Wno-self-assign)
# Disable ARM NEON filter (png_init_filter_functions_neon) when arm sources are not in tree
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|arm")
	add_definitions(-DPNG_ARM_NEON_OPT=0)
endif()
add_library(png STATIC ${SRCS})
