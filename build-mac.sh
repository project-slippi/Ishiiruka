#!/bin/bash -e
# build-mac.sh

CMAKE_FLAGS=''

DESKTOP_APP_URL="https://github.com/project-slippi/slippi-desktop-app"
DESKTOP_APP_SYS_PATH="./slippi-desktop-app/app/dolphin-dev/overwrite/Sys"

DATA_SYS_PATH="./Data/Sys/"
BINARY_PATH="./build/Binaries/Slippi\ Dolphin.app/Contents/Resources/"

# Build type
if [ "$1" == "playback" ]
    then
        CMAKE_FLAGS+=" -DIS_PLAYBACK=true"
        echo "Using Playback build config"
else
        echo "Using Netplay build config"
fi

if [[ -z "$MACOS_CERT_PASSWD" ]]
    then
        echo "Building with code signing"
        CMAKE_FLAGS+='-DMACOS_CODE_SIGNING="ON"'
else
        echo "Building without code signing"
fi

# Move into the build directory, run CMake, and compile the project
mkdir -p build
pushd build
cmake ${CMAKE_FLAGS} ../
make -j$(nproc)
popd

# Copy the Sys folder in
cp -Rfn ${DATA_SYS_PATH} ${BINARY_PATH}

# Copy playback specific codes if needed
if [ "$1" == "playback" ]
    then
        if [ -d "slippi-desktop-app" ]
			then
				pushd slippi-desktop-app
				git checkout master
				git pull --ff-only
				popd
		else
			git clone ${DESKTOP_APP_URL}
		fi
        
        rm -rf "${BINARY_PATH}/Sys/GameSettings"
        cp -r ${DESKTOP_APP_SYS_PATH} ${BINARY_PATH}
fi