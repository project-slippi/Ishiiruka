

set(SRCS
	xbrz.cpp
)


add_library(xbrz STATIC ${SRCS})
if(APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()
