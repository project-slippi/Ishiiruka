#!/bin/bash -e
# build-linux.sh

case "${1}" in
	# Enables portable configuration files via portable.txt
	portable)
		CMAKE_FLAGS='-DLINUX_LOCAL_DEV=true'
		;;
	appimage)
		CMAKE_FLAGS=''
		;;
	*)
		echo "usage: ${0} <portable|appimage>"
		exit 1
		;;
esac

# Move into the build directory, run CMake, and compile the project
mkdir -p build
pushd build
cmake ${CMAKE_FLAGS} ..
make -j$(nproc)
popd
