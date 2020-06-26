

set(SRCS
	SlippiGame.cpp
)

# glslang requires C++11 at a minimum to compile.
add_definitions(-std=c++14)

add_library(SlippiLib STATIC ${SRCS})
