set(SRCS
	NullBackend.cpp
	Render.cpp
	VertexManager.cpp
)

set(LIBS
	videocommon
	common
)

add_dolphin_library(videonull "${SRCS}" "${LIBS}")

