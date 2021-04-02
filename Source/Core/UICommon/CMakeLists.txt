set(SRCS Disassembler.cpp
		 DiscordPresence.cpp
         UICommon.cpp)

set(LIBS common)

add_dolphin_library(uicommon "${SRCS}" "${LIBS}")

if(USE_DISCORD_PRESENCE)
  target_compile_definitions(uicommon PRIVATE -DUSE_DISCORD_PRESENCE)
  target_link_libraries(uicommon PRIVATE discord-rpc)
endif() 