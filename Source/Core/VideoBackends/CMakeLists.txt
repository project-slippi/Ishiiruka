add_subdirectory(OGL)
add_subdirectory(Software)
if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD|NetBSD|OpenBSD")
	add_subdirectory(Vulkan)
endif()

# TODO: Add other backends here!
