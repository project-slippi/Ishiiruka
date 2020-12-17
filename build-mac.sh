#!/bin/bash -e
# build-mac.sh

CMAKE_FLAGS=''

DESKTOP_APP_URL="https://github.com/project-slippi/slippi-desktop-app"
DESKTOP_APP_SYS_PATH="./slippi-desktop-app/app/dolphin-dev/overwrite/Sys"

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
        echo "Copying playback codes into the bundle"
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
        cp -r "${DESKTOP_APP_SYS_PATH}" "${BINARY_PATH}"
fi
