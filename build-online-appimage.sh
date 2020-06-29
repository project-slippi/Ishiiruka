#!/bin/bash -e
# build-online-appimage.sh

ZSYNC_STRING="gh-releases-zsync|project-slippi|Ishiiruka|latest|Slippi_Online-x86_64.AppImage.zsync"

LINUXDEPLOY_PATH="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous"
LINUXDEPLOY_FILE="linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_URL="${LINUXDEPLOY_PATH}/${LINUXDEPLOY_FILE}"

UPDATEPLUG_PATH="https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/continuous"
UPDATEPLUG_FILE="linuxdeploy-plugin-appimage-x86_64.AppImage"
UPDATEPLUG_URL="${UPDATEPLUG_PATH}/${UPDATEPLUG_FILE}"

UPDATETOOL_PATH="https://github.com/AppImage/AppImageUpdate/releases/download/continuous"
UPDATETOOL_FILE="appimageupdatetool-x86_64.AppImage"
UPDATETOOL_URL="${UPDATETOOL_PATH}/${UPDATETOOL_FILE}"

# Grab various appimage binaries from GitHub if we don't have them
if [ ! -e ./linuxdeploy ]; then
	wget ${LINUXDEPLOY_URL} -O linuxdeploy
	chmod +x linuxdeploy
fi
if [ ! -e ./linuxdeploy-update-plugin ]; then
	wget ${UPDATEPLUG_URL} -O linuxdeploy-update-plugin
	chmod +x linuxdeploy-update-plugin
fi
if [ ! -e ./appimageupdatetool ]; then
	wget ${UPDATEPLUG_URL} -O appimageupdatetool
	chmod +x appimageupdatetool
fi

# Build the AppDir directory for this image
mkdir -p AppDir
./linuxdeploy \
	--appdir=./AppDir \
	-e ./build/Binaries/dolphin-emu \
	-d ./Data/slippi-online.desktop \
	-i ./Data/dolphin-emu.png

# Package up the update tool within the AppImage
cp ./appimageupdatetool ./AppDir/usr/bin/

# Bake an AppImage with the update metadata
UPDATE_INFORMATION="${ZSYNC_STRING}" \
	./linuxdeploy-update-plugin --appdir=./AppDir/ \
