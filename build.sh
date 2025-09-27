#!/bin/bash

# --- Default Configuration ---
TARGET_OS="linux"
TARGET_ARCH="x64"
COMMAND="build"
VCPKG_ROOT=.vcpkg

# --- Script Logic ---
set -e # Exit immediately if a command fails.

# Parse command-line arguments
while [[ "$#" -gt 0 ]]; do
	case $1 in
	--os)
		TARGET_OS="$2"
		shift
		;;
	--arch)
		TARGET_ARCH="$2"
		shift
		;;
	configure | build | debug | release) COMMAND="$1" ;;
	*)
		echo "Unknown parameter passed: $1"
		exit 1
		;;
	esac
	shift
done

# --- Determine build environment ---
if [ "$TARGET_OS" == "linux" ]; then
	VCPKG_TRIPLET="$TARGET_ARCH-linux"
	TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
	BUILD_DIR="cmake-build-$TARGET_OS-$TARGET_ARCH"
elif [ "$TARGET_OS" == "windows" ]; then
	VCPKG_TRIPLET="$TARGET_ARCH-mingw-static"
	TOOLCHAIN_FILE="cross-windows-x64.cmake" # Our custom toolchain
	BUILD_DIR="cmake-build-$TARGET_OS-$TARGET_ARCH"
else
	echo "Unsupported OS: $TARGET_OS"
	exit 1
fi

DEBUG_DIR="$BUILD_DIR-debug"
RELEASE_DIR="$BUILD_DIR-release"

# --- Functions ---
setup_vcpkg() {
	if [ ! -d "$VCPKG_ROOT" ]; then
		echo "--- vcpkg not found. Setting up... ---"
		git clone --depth=1 https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
		"$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
	fi
}

configure() {
	BUILD_TYPE=$1
	BUILD_DIR=$2
	echo "--- Configuring for $TARGET_OS-$TARGET_ARCH ($BUILD_TYPE)... ---"
	cmake -B "$BUILD_DIR" -S . \
		-DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
		-DVCPKG_TARGET_TRIPLET="$VCPKG_TRIPLET" \
		-DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=1

	# Symlink for LSP, points to the last configured build
	ln -sf "$BUILD_DIR/compile_commands.json" "compile_commands.json"
}

# --- Command Handling ---
setup_vcpkg

case $COMMAND in
configure)
	configure "Debug" "$DEBUG_DIR"
	echo "✅ Configuration complete."
	;;

build | debug)
	if [ ! -f "$DEBUG_DIR/CMakeCache.txt" ]; then
		configure "Debug" "$DEBUG_DIR"
	fi
	echo "--- Building (Debug)... ---"
	cmake --build "$DEBUG_DIR"
	echo "✅ Debug build complete."
	;;

release)
	if [ ! -f "$RELEASE_DIR/CMakeCache.txt" ]; then
		configure "Release" "$RELEASE_DIR"
	fi
	echo "--- Building (Release)... ---"
	cmake --build "$RELEASE_DIR"
	echo "✅ Release build complete."
	;;

*)
	echo "Usage: $0 [--os <os>] [--arch <arch>] {configure|build|release}"
	exit 1
	;;
esac
