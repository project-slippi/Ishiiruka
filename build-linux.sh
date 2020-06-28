#!/bin/bash -e
# build-linux.sh

case "${1}" in
	# Enables portable configuration files via `portable.txt`
	"portable")
		CMAKE_FLAGS='-DLINUX_LOCAL_DEV=true'
		;;
	"appimage")
		CMAKE_FLAGS=''
		;;
	*)
		echo "usage: ${0} <portable|appimage>"
		exit 0
		;;
esac

# Create the build directory if it doesn't exist
if [ ! -e "./build/" ]; then  mkdir ./build; fi

# Move into the build directory, run CMake, and compile the project
pushd ./build
cmake ${CMAKE_FLAGS} ../
make -j$(nproc)
popd


