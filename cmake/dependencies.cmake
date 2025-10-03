# =============================================================================
# Core Dependencies
# =============================================================================

# Boost libraries
find_package(Boost REQUIRED)
find_package(Boost REQUIRED COMPONENTS asio)

# fmt formatting library
find_package(spdlog CONFIG REQUIRED)

# libtins network packet library
set(LIBTINS_ENABLE_PCAP OFF CACHE BOOL "Disable libpcap/Npcap dependency")
set(LIBTINS_ENABLE_DOT11 OFF CACHE BOOL "Disable 802.11 features that need pcap")
set(LIBTINS_BUILD_EXAMPLES OFF CACHE BOOL "Don't build examples")
set(LIBTINS_BUILD_TESTS OFF CACHE BOOL "Don't build tests")
set(LIBTINS_BUILD_SHARED OFF CACHE BOOL "Build a static library")
set(LIBTINS_ENABLE_ACK_TRACKER OFF CACHE BOOL "Disable TCP ACK tracker feature")
set(LIBTINS_ENABLE_TCP_STREAM_CUSTOM_DATA OFF CACHE BOOL "Disable TCP stream custom data")

FetchContent_Declare(
    libtins
    GIT_REPOSITORY https://github.com/uoosef403/libtins
)
FetchContent_MakeAvailable(libtins)

# Configure libtins as a static library
target_compile_definitions(tins INTERFACE TINS_STATIC)

# =============================================================================
# Testing Dependencies
# =============================================================================

# Enable testing support
enable_testing()

# Google Test testing framework
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

# Configure Google Test
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Include GoogleTest module for test discovery
include(GoogleTest)
