cmake_minimum_required(VERSION 3.16)

# --- CMake Policies ---
cmake_policy(SET CMP0135 NEW)
cmake_policy(SET CMP0167 NEW)

# --- Project Configuration ---
project(stack CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

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

add_subdirectory(libtins)

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
file(GLOB_RECURSE CORE_HEADERS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

add_executable(${PROJECT_NAME} main.cpp ${CORE_SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC src)
target_include_directories(${PROJECT_NAME} PRIVATE libtins/include)

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
    target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601) # Updated for modern Windows
    target_link_libraries(${PROJECT_NAME} PRIVATE iphlpapi ws2_32) # ws2_32 is often needed with Asio
endif()