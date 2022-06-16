

set(SRCS
	xbrz.cpp
)


add_library(xbrz STATIC ${SRCS})
if(APPLE OR ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  add_definitions(-DNDEBUG=1)
endif()
