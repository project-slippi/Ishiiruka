#!/bin/bash -e
# build-mac.sh

CMAKE_FLAGS=''

PLAYBACK_CODES_PATH="./Data/PlaybackGeckoCodes/"

DATA_SYS_PATH="./Data/Sys/"
BINARY_PATH="./build/Binaries/Slippi Dolphin.app/Contents/Resources/"

# Build type
if [ "$1" == "playback" ]
    then
        CMAKE_FLAGS+=" -DIS_PLAYBACK=true"
        echo "Using Playback build config"
else
        echo "Using Netplay build config"
fi

if [[ -z "${CERTIFICATE_MACOS_APPLICATION}" ]]
    then
        echo "Building without code signing"
else
        echo "Building with code signing"
        CMAKE_FLAGS+=' -DMACOS_CODE_SIGNING="ON"'
fi

# Move into the build directory, run CMake, and compile the project
mkdir -p build
pushd build
cmake ${CMAKE_FLAGS} ..
make -j7
popd

# Copy the Sys folder in
echo "Copying Sys files into the bundle"
cp -Rfn "${DATA_SYS_PATH}" "${BINARY_PATH}"

# Copy playback specific codes if needed
if [ "$1" == "playback" ]
    then
        # Update Sys dir with playback codes
        echo "Copying playback gecko codes into the bundle"
		rm -rf "${BINARY_PATH}/Sys/GameSettings" # Delete netplay codes
		cp -r "${PLAYBACK_CODES_PATH}/." "${BINARY_PATH}/Sys/GameSettings/"
fi
