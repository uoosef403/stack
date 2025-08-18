cmake_minimum_required(VERSION 3.16)

# --- CMake Policies ---
cmake_policy(SET CMP0135 NEW)
cmake_policy(SET CMP0167 NEW)

# --- Project Configuration ---
project(stack CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies ---
# This section assumes Boost and {fmt} are provided by a package manager
# like vcpkg or are installed on the system.
include(FetchContent)

# Find Boost.Asio
find_package(Boost REQUIRED COMPONENTS asio)

# Find {fmt}
find_package(fmt CONFIG REQUIRED)


# Fetch libtins without pcap/Npcap dependency
set(LIBTINS_ENABLE_PCAP OFF CACHE BOOL "Disable libpcap/Npcap dependency")
set(LIBTINS_ENABLE_DOT11 OFF CACHE BOOL "Disable 802.11 features that need pcap")
set(LIBTINS_BUILD_EXAMPLES OFF CACHE BOOL "Don't build examples")
set(LIBTINS_BUILD_TESTS OFF CACHE BOOL "Don't build tests")
set(LIBTINS_BUILD_SHARED OFF CACHE BOOL "Build a static library")
set(LIBTINS_ENABLE_ACK_TRACKER OFF CACHE BOOL "Disable TCP ACK tracker feature")
set(LIBTINS_ENABLE_TCP_STREAM_CUSTOM_DATA OFF CACHE BOOL "Disable TCP stream custom data")

include(FetchContent)

FetchContent_Declare(
        libtins
        GIT_REPOSITORY https://github.com/uoosef403/libtins
)

FetchContent_MakeAvailable(libtins)

target_compile_definitions(tins INTERFACE TINS_STATIC)

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
file(GLOB_RECURSE CORE_HEADERS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

add_executable(${PROJECT_NAME} main.cpp ${CORE_SOURCES})

target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

target_include_directories(${PROJECT_NAME} PUBLIC src)
# work around for w4 related msbuild error shitty libtins !
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${libtins_SOURCE_DIR}/include)

target_sources(${PROJECT_NAME} PRIVATE
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES ${CORE_HEADERS}
)

# --- Target Properties ---
target_compile_definitions(${PROJECT_NAME} PRIVATE BOOST_ASIO_HAS_CO_AWAIT)

# Link dependencies using modern CMake targets. These targets automatically
# handle include directories and other required properties.
target_link_libraries(${PROJECT_NAME} PRIVATE
        Boost::asio
        fmt::fmt
        tins
)

# Windows-specific settings
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601)
    target_link_libraries(${PROJECT_NAME} PRIVATE iphlpapi winmm ws2_32 Rpcrt4)

    # --- Fetch WinTun automatically ---
    FetchContent_Declare(
            wintun
            URL "https://www.wintun.net/builds/wintun-0.14.1.zip"
            URL_HASH SHA256=07c256185d6ee3652e09fa55c0b673e2624b565e02c4b9091c79ca7d2f24ef51
    )
    FetchContent_MakeAvailable(wintun)

    # --- Put the appropriate wintun.dll beside the executable ---
    function(copy_wintun_dll TARGET_NAME)
        # Use the path from FetchContent instead of a local vendor folder
        set(WINTUN_DIR ${wintun_SOURCE_DIR})

        # Add the include directory to the target
        target_include_directories(${TARGET_NAME} SYSTEM PRIVATE ${WINTUN_DIR}/include)

        # This architecture detection logic is perfect, no changes needed
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

        # This custom command is also perfect, no changes needed
        add_custom_command(
                TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${WINTUN_DLL}"
                "$<TARGET_FILE_DIR:${TARGET_NAME}>/wintun.dll"
                COMMENT "Copying wintun.dll for target ${TARGET_NAME}"
        )
    endfunction()

    copy_wintun_dll(${PROJECT_NAME})
endif()