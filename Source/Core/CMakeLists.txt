add_subdirectory(AudioCommon)
add_subdirectory(Common)
add_subdirectory(Core)
add_subdirectory(DiscIO)
add_subdirectory(DolphinWX)
add_subdirectory(DolphinNoGUI)
add_subdirectory(InputCommon)
add_subdirectory(UICommon)
add_subdirectory(VideoCommon)
add_subdirectory(VideoBackends)

if(ENABLE_QT)
	add_subdirectory(DolphinQt)
endif()

# Build SlippiRustExtensions for DolphinWX and DolphinNoGui.
#
# (This doesn't feel perfect here, but truth be told I can't think offhand of
# a better location for it at the moment.)
set(RUST_FEATURES "")
if(DIS_PLAYBACK)
  set(RUST_FEATURES "playback")
endif()

corrosion_import_crate(MANIFEST_PATH ${CMAKE_SOURCE_DIR}/Externals/SlippiRustExtensions/Cargo.toml ${RUST_FEATURES})
