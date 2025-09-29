# =============================================================================
# Stack Network Library CMake Configuration  
# =============================================================================

cmake_minimum_required(VERSION 3.21)

# =============================================================================
# CMake Policies
# =============================================================================

cmake_policy(SET CMP0135 NEW)
cmake_policy(SET CMP0167 NEW)
cmake_policy(SET CMP0169 NEW)  # Allow FetchContent_MakeAvailable for vcpkg

# =============================================================================
# vcpkg Package Manager Setup (Before project() call)
# =============================================================================

include(FetchContent)

# Setup vcpkg if toolchain is not already configured
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE OR CMAKE_TOOLCHAIN_FILE STREQUAL "")
    # Fetch vcpkg
    FetchContent_Declare(
        vcpkg
        GIT_REPOSITORY https://github.com/microsoft/vcpkg.git
        GIT_SHALLOW TRUE
    )
    
    # Make vcpkg available
    FetchContent_MakeAvailable(vcpkg)

    # Bootstrap vcpkg
    message(STATUS "Bootstrapping vcpkg...")
    if(WIN32)
        set(BOOTSTRAP_COMMAND "${vcpkg_SOURCE_DIR}/bootstrap-vcpkg.bat")
    else()
        set(BOOTSTRAP_COMMAND "${vcpkg_SOURCE_DIR}/bootstrap-vcpkg.sh")
    endif()

    execute_process(
        COMMAND "${BOOTSTRAP_COMMAND}" -disableMetrics
        WORKING_DIRECTORY "${vcpkg_SOURCE_DIR}"
        RESULT_VARIABLE result
    )
    
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Bootstrapping vcpkg failed!")
    endif()

    # Set the toolchain file
    set(CMAKE_TOOLCHAIN_FILE "${vcpkg_SOURCE_DIR}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
    
    message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()
# =============================================================================
# Project Configuration
# =============================================================================

project(stack CXX)

# Export compile commands for clangd and other tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile commands for clangd" FORCE)

# C++ standard requirements
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Threading configuration
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)

# =============================================================================
# Dependencies
# =============================================================================
# Boost libraries
find_package(Boost REQUIRED)
find_package(Boost REQUIRED COMPONENTS asio)

# fmt formatting library
find_package(fmt CONFIG REQUIRED)

# libtins network packet library (with custom configuration)
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

# Configure libtins as static library
target_compile_definitions(tins INTERFACE TINS_STATIC)

# =============================================================================
# Source Files
# =============================================================================

# Collect all source and header files recursively
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
file(GLOB_RECURSE CORE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

# =============================================================================
# Target Configuration
# =============================================================================

# Create main executable
add_executable(${PROJECT_NAME} main.cpp ${CORE_SOURCES})

# Compiler warning configuration
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC src)

# System include for libtins to avoid compiler warnings
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${libtins_SOURCE_DIR}/include)

# Configure target file sets for headers
target_sources(${PROJECT_NAME} PRIVATE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
    FILES ${CORE_HEADERS}
)

# Compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE 
    BOOST_ASIO_HAS_CO_AWAIT
)

# Link dependencies using modern CMake targets
target_link_libraries(${PROJECT_NAME} PRIVATE
    Boost::asio
    fmt::fmt
    tins
)

# =============================================================================
# Development Tools
# =============================================================================

# Copy compile commands to source root for clangd
execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_SOURCE_DIR}/compile_commands.json
)

# =============================================================================
# Platform-Specific Configuration
# =============================================================================

# Windows-specific settings and dependencies
if(WIN32)
    # Windows API definitions
    target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601)
    
    # Windows system libraries
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        iphlpapi 
        winmm 
        ws2_32 
        Rpcrt4
    )

    # WinTun VPN adapter library
    FetchContent_Declare(
        wintun
        URL "https://www.wintun.net/builds/wintun-0.14.1.zip"
        URL_HASH SHA256=07c256185d6ee3652e09fa55c0b673e2624b565e02c4b9091c79ca7d2f24ef51
    )
    FetchContent_MakeAvailable(wintun)

    # Function to copy WinTun DLL to executable directory
    function(copy_wintun_dll TARGET_NAME)
        set(WINTUN_DIR ${wintun_SOURCE_DIR})

        # Add WinTun include directory
        target_include_directories(${TARGET_NAME} SYSTEM PRIVATE ${WINTUN_DIR}/include)

        # Determine architecture-specific DLL path
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64")
                set(WINTUN_ARCH_DIR "bin/arm64")
            else()
                set(WINTUN_ARCH_DIR "bin/amd64")
            endif()
        else()
            if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM")
                set(WINTUN_ARCH_DIR "bin/arm")
            else()
                set(WINTUN_ARCH_DIR "bin/x86")
            endif()
        endif()

        set(WINTUN_DLL "${WINTUN_DIR}/${WINTUN_ARCH_DIR}/wintun.dll")

        # Copy DLL to output directory after build
        add_custom_command(
            TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WINTUN_DLL}"
            "$<TARGET_FILE_DIR:${TARGET_NAME}>/wintun.dll"
            COMMENT "Copying wintun.dll for target ${TARGET_NAME}"
        )
    endfunction()

    # Apply WinTun configuration to main target
    copy_wintun_dll(${PROJECT_NAME})
endif()
