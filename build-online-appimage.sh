#!/bin/bash -e
# build-online-appimage.sh

LINUXDEPLOY_REPO="https://github.com/linuxdeploy/linuxdeploy"
LINUXDEPLOY_PATH="releases/download/continuous"
LINUXDEPLOY_FILE="linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_URL="${LINUXDEPLOY_REPO}/${LINUXDEPLOY_PATH}/${LINUXDEPLOY_FILE}"

# Grab the linuxdeploy binary from GitHub if we don't have it
if [ ! -e ./linuxdeploy ]; then
	wget ${LINUXDEPLOY_URL} -O linuxdeploy && chmod +x linuxdeploy
fi


# Build the AppDir directory for this image
mkdir -p AppDir
./linuxdeploy \
	--appdir=./AppDir \
	-e ./build/Binaries/dolphin-emu \
	-d ./Data/slippi-online.desktop \
	-i ./Data/dolphin-emu.png

# Bake an AppImage
./linuxdeploy --appdir=./AppDir --output appimage

