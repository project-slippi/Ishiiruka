# This file will be configured to contain variables for CPack. These variables
# should be set in the CMake list file of the project before CPack module is
# included. The list of available CPACK_xxx variables and their associated
# documentation may be obtained using
#  cpack --help-variable-list
#
# Some variables are common to all generators (e.g. CPACK_PACKAGE_NAME)
# and some are specific to a generator
# (e.g. CPACK_NSIS_EXTRA_INSTALL_COMMANDS). The generator specific variables
# usually begin with CPACK_<GENNAME>_xxxx.


set(CPACK_BINARY_7Z "OFF")
set(CPACK_BINARY_IFW "OFF")
set(CPACK_BINARY_NSIS "ON")
set(CPACK_BINARY_NUGET "OFF")
set(CPACK_BINARY_WIX "OFF")
set(CPACK_BINARY_ZIP "OFF")
set(CPACK_BUILD_SOURCE_DIRS "G:/Projects/Ishiiruka;G:/Projects/Ishiiruka")
set(CPACK_CMAKE_GENERATOR "Visual Studio 16 2019")
set(CPACK_COMPONENT_UNSPECIFIED_HIDDEN "TRUE")
set(CPACK_COMPONENT_UNSPECIFIED_REQUIRED "TRUE")
set(CPACK_DEFAULT_PACKAGE_DESCRIPTION_FILE "C:/Program Files/CMake/share/cmake-3.17/Templates/CPack.GenericDescription.txt")
set(CPACK_DEFAULT_PACKAGE_DESCRIPTION_SUMMARY "dolphin-emu built using CMake")
set(CPACK_GENERATOR "NSIS")
set(CPACK_INSTALL_CMAKE_PROJECTS "G:/Projects/Ishiiruka;dolphin-emu;ALL;/")
set(CPACK_INSTALL_PREFIX "C:/Program Files (x86)/dolphin-emu")
set(CPACK_MODULE_PATH "G:/Projects/Ishiiruka/CMakeTests")
set(CPACK_NSIS_DISPLAY_NAME "dolphin-emu 5.0.6211a99fb2c237f9e8b1e55db8c1068512d6a015")
set(CPACK_NSIS_INSTALLER_ICON_CODE "")
set(CPACK_NSIS_INSTALLER_MUI_ICON_CODE "")
set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
set(CPACK_NSIS_PACKAGE_NAME "dolphin-emu 5.0.6211a99fb2c237f9e8b1e55db8c1068512d6a015")
set(CPACK_NSIS_UNINSTALL_NAME "Uninstall")
set(CPACK_OUTPUT_CONFIG_FILE "G:/Projects/Ishiiruka/CPackConfig.cmake")
set(CPACK_PACKAGE_DEFAULT_LOCATION "/")
set(CPACK_PACKAGE_DESCRIPTION_FILE "G:/Projects/Ishiiruka/Data/cpack_package_description.txt")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A GameCube and Wii emulator")
set(CPACK_PACKAGE_FILE_NAME "dolphin-emu-5.0.6211a99fb2c237f9e8b1e55db8c1068512d6a015-win64")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "dolphin-emu 5.0.6211a99fb2c237f9e8b1e55db8c1068512d6a015")
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "dolphin-emu 5.0.6211a99fb2c237f9e8b1e55db8c1068512d6a015")
set(CPACK_PACKAGE_NAME "dolphin-emu")
set(CPACK_PACKAGE_RELOCATABLE "true")
set(CPACK_PACKAGE_VENDOR "Dolphin Team")
set(CPACK_PACKAGE_VERSION "5.0.6211a99fb2c237f9e8b1e55db8c1068512d6a015")
set(CPACK_PACKAGE_VERSION_MAJOR "5")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "6211a99fb2c237f9e8b1e55db8c1068512d6a015")
set(CPACK_RESOURCE_FILE_LICENSE "C:/Program Files/CMake/share/cmake-3.17/Templates/CPack.GenericLicense.txt")
set(CPACK_RESOURCE_FILE_README "C:/Program Files/CMake/share/cmake-3.17/Templates/CPack.GenericDescription.txt")
set(CPACK_RESOURCE_FILE_WELCOME "C:/Program Files/CMake/share/cmake-3.17/Templates/CPack.GenericWelcome.txt")
set(CPACK_RPM_PACKAGE_GROUP "System/Emulators/Other")
set(CPACK_RPM_PACKAGE_LICENSE "GPL-2.0")
set(CPACK_SET_DESTDIR "ON")
set(CPACK_SOURCE_GENERATOR "TGZ;TBZ2;ZIP")
set(CPACK_SOURCE_IGNORE_FILES "\\.#;/#;.*~;\\.swp;/\\.git;G:/Projects/Ishiiruka")
set(CPACK_SOURCE_OUTPUT_CONFIG_FILE "G:/Projects/Ishiiruka/CPackSourceConfig.cmake")
set(CPACK_SYSTEM_NAME "win64")
set(CPACK_TOPLEVEL_TAG "win64")
set(CPACK_WIX_SIZEOF_VOID_P "8")

if(NOT CPACK_PROPERTIES_FILE)
  set(CPACK_PROPERTIES_FILE "G:/Projects/Ishiiruka/CPackProperties.cmake")
endif()

if(EXISTS ${CPACK_PROPERTIES_FILE})
  include(${CPACK_PROPERTIES_FILE})
endif()
