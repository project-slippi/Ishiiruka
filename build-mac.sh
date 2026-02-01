#!/bin/bash -e
# build-mac.sh

CMAKE_FLAGS=''

PLAYBACK_CODES_PATH="./Data/PlaybackGeckoCodes/"

DATA_SYS_PATH="./Data/Sys/"
BINARY_PATH="./build/Binaries/Slippi Dolphin.app/Contents/Resources/"

# Use CMake 3.x when system has CMake 4.x (project externals require < 3.5 compatibility)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CMAKE31_VERSION="3.31.6"
CMAKE31_DIR="${SCRIPT_DIR}/.cmake-3.31"
CMAKE31_TARBALL="cmake-${CMAKE31_VERSION}-macos-universal.tar.gz"
CMAKE31_URL="https://cmake.org/files/v3.31/${CMAKE31_TARBALL}"
CMAKE31_BIN="${CMAKE31_DIR}/cmake-${CMAKE31_VERSION}-macos-universal/CMake.app/Contents/bin"

if cmake --version 2>/dev/null | head -1 | grep -q "version 4\."; then
	if [ ! -x "${CMAKE31_BIN}/cmake" ]; then
		echo "System CMake is 4.x; using CMake ${CMAKE31_VERSION} for this project..."
		mkdir -p "${CMAKE31_DIR}"
		(cd "${CMAKE31_DIR}" && curl -sL -O "${CMAKE31_URL}" && tar xzf "${CMAKE31_TARBALL}")
		[ -x "${CMAKE31_BIN}/cmake" ] || { echo "Download failed."; exit 1; }
	fi
	export PATH="${CMAKE31_BIN}:${PATH}"
fi

# Apple Silicon: use Vulkan SDK for arm64 libvulkan.dylib (bundled Externals/MoltenVK is x86_64 only)
if [ "$(uname -m)" = "arm64" ] && [ -z "${VULKAN_SDK:-}" ]; then
	for sdk in "$HOME/VulkanSDK/"*/macOS "$HOME/VulkanSDK/"*/lib "$HOME/VulkanSDK/"*; do
		[ -e "$sdk" ] || continue
		if [ -f "$sdk/libvulkan.dylib" ]; then
			# sdk is a lib dir (e.g. .../macOS or .../lib); use parent as SDK root for .../lib
			case "$sdk" in */lib) export VULKAN_SDK="${sdk%/lib}" ;; *) export VULKAN_SDK="$sdk" ;; esac
			echo "Using Vulkan SDK: $VULKAN_SDK"
			break
		fi
		if [ -f "$sdk/lib/libvulkan.dylib" ]; then
			export VULKAN_SDK="$sdk"
			echo "Using Vulkan SDK: $VULKAN_SDK"
			break
		fi
	done
	if [ -z "${VULKAN_SDK:-}" ]; then
		echo "Apple Silicon: Vulkan SDK not found. For Vulkan backend, install from https://vulkan.lunarg.com/sdk/home#mac then set VULKAN_SDK (e.g. export VULKAN_SDK=\$HOME/VulkanSDK/1.4.xxx/macOS)."
	fi
fi

# Build type
if [ "$1" == "playback" ]
    then
        CMAKE_FLAGS+=" -DIS_PLAYBACK=true"
        echo "Using Playback build config"
else
        echo "Using Netplay build config"
fi

# Move into the build directory, run CMake, and compile the project
mkdir -p build
pushd build
cmake ${CMAKE_FLAGS} ..
make -j$(sysctl -n hw.ncpu)
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
