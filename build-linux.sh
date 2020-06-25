#!/bin/sh -e
# build-linux.sh

# Arguments passed to CMake
CMAKE_FLAGS=''

# Create the build directory if it doesn't exist
if [ ! -e "./build/" ]; then  mkdir ./build; fi

# Move into the build directory, run CMake, and compile the project
pushd ./build
cmake ${CMAKE_FLAGS} ../
make -j7
popd


