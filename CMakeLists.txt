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
# vcpkg Package Manager Setup
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

# Threading configuration - platform specific
if(WIN32)
    # Use Win32 threads on Windows
    set(CMAKE_USE_WIN32_THREADS_INIT 1)
    set(CMAKE_USE_PTHREADS_INIT 0)
else()
    # Use pthread on Unix-like systems
    set(CMAKE_THREAD_LIBS_INIT "-lpthread")
    set(CMAKE_USE_WIN32_THREADS_INIT 0)  
    set(CMAKE_USE_PTHREADS_INIT 1)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
endif()

find_package(Threads REQUIRED)

# =============================================================================
# CMake Module Path
# =============================================================================

# Add cmake modules directory to module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# =============================================================================
# Dependencies and Configuration
# =============================================================================

# Load dependency configuration
include(dependencies)

# Load platform-specific configuration  
include(platform)

# Load library configuration
include(library)

# Load testing configuration
include(testing)

# =============================================================================
# Main Executable
# =============================================================================

# Create main executable (only main.cpp, no core sources)
add_executable(${PROJECT_NAME} main.cpp)

# Link against our library - inherits all settings automatically
target_link_libraries(${PROJECT_NAME} PRIVATE stack::lib)

# Apply platform-specific configuration to main executable
configure_platform_target(${PROJECT_NAME})

# =============================================================================
# Development Tools
# =============================================================================

# Copy compile commands to source root for clangd
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    add_custom_target(copy_compile_commands ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
        COMMENT "Copying compile commands for clangd"
    )
endif()

# =============================================================================
# Build Summary
# =============================================================================

message(STATUS "")
message(STATUS "=== Stack Network Library Configuration Summary ===")
message(STATUS "Project Version: ${PROJECT_VERSION}")  
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "")
