if(NOT(USE_X11 OR ENABLE_HEADLESS))
  return()
endif()

set(DOLPHIN_NOGUI_EXE dolphin-emu-nogui)

set(NOGUI_SRCS MainNoGUI.cpp)

if(USE_X11)
  set(NOGUI_SRCS ${NOGUI_SRCS} ../DolphinWX/X11Utils.cpp)
endif()

add_executable(dolphin-nogui ${NOGUI_SRCS})
set_target_properties(dolphin-nogui PROPERTIES OUTPUT_NAME ${DOLPHIN_NOGUI_EXE})

target_link_libraries(dolphin-nogui PRIVATE
  core
  uicommon
  ${LIBS}
)

target_link_libraries(dolphin-nogui PUBLIC slippi-rust-extensions)

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} dolphin-nogui)
install(TARGETS dolphin-nogui RUNTIME DESTINATION ${bindir})
