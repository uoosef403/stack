#!/bin/bash

# Network Stack Build Script
# Enhanced build system with configuration management support

# --- Default Configuration ---
TARGET_OS="linux"
TARGET_ARCH="x64"
COMMAND="build"
VCPKG_ROOT=.vcpkg

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

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
	configure | build | debug | release | test | clean | help) COMMAND="$1" ;;
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

# Print colored output
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Show usage information
show_help() {
    echo "Network Stack Build Script"
    echo ""
    echo "Usage: $0 [--os <os>] [--arch <arch>] {command}"
    echo ""
    echo "Commands:"
    echo "  configure                 Configure CMake build"
    echo "  build, debug              Build debug version"
    echo "  release                   Build release version"
    echo "  test                      Run unit tests"
    echo "  clean                     Clean build directories"
    echo "  help                      Show this help message"
    echo ""
    echo "Options:"
    echo "  --os <os>                 Target OS (linux, windows)"
    echo "  --arch <arch>             Target architecture (x64)"
    echo ""
    echo "Examples:"
    echo "  $0 release                # Build release version for Linux"
    echo "  $0 --os windows build     # Cross-compile for Windows"
    echo "  $0 test                   # Run unit tests"
    echo ""
    echo "Configuration Examples:"
    echo "  # Run with default config"
    echo "  ./build/stack"
    echo ""
    echo "  # Run with custom config file"
    echo "  ./build/stack --config examples/production-proxy.toml"
    echo ""
    echo "  # Run with command-line options"
    echo "  ./build/stack --socks5-host proxy.example.com --log-level debug"
}

# Clean build directories
clean_build() {
    print_info "Cleaning build directories..."
    rm -rf cmake-build-*
    rm -f compile_commands.json
    print_success "Build directories cleaned"
}

# Run tests
run_tests() {
    print_info "Running unit tests..."
    
    if [ -f "$DEBUG_DIR/CMakeCache.txt" ]; then
        cd "$DEBUG_DIR"
        ctest --output-on-failure
    elif [ -f "$RELEASE_DIR/CMakeCache.txt" ]; then
        cd "$RELEASE_DIR"
        ctest --output-on-failure
    else
        print_warning "No build found. Building debug version first..."
        configure "Debug" "$DEBUG_DIR"
        cmake --build "$DEBUG_DIR"
        cd "$DEBUG_DIR"
        ctest --output-on-failure
    fi
    
    print_success "Unit tests completed"
}

# --- Command Handling ---
setup_vcpkg

case $COMMAND in
configure)
	configure "Debug" "$DEBUG_DIR"
	print_success "Configuration complete."
	;;

build | debug)
	if [ ! -f "$DEBUG_DIR/CMakeCache.txt" ]; then
		configure "Debug" "$DEBUG_DIR"
	fi
	print_info "Building (Debug)..."
	cmake --build "$DEBUG_DIR"
	print_success "Debug build complete."
	print_info "Run with: ./$DEBUG_DIR/stack --help"
	;;

release)
	if [ ! -f "$RELEASE_DIR/CMakeCache.txt" ]; then
		configure "Release" "$RELEASE_DIR"
	fi
	print_info "Building (Release)..."
	cmake --build "$RELEASE_DIR"
	print_success "Release build complete."
	print_info "Run with: ./$RELEASE_DIR/stack --help"
	;;

test)
	run_tests
	;;

clean)
	clean_build
	;;

help)
	show_help
	;;

*)
	print_error "Unknown command: $COMMAND"
	echo ""
	show_help
	exit 1
	;;
esac
