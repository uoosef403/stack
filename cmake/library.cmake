# =============================================================================
# Stack Network Library - Library Target Configuration
# =============================================================================

# =============================================================================
# Source Files Collection
# =============================================================================

# Collect all source and header files recursively
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
file(GLOB_RECURSE CORE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

# =============================================================================
# Stack Core Library
# =============================================================================

# Create the core library with all sources
add_library(stack_lib STATIC ${CORE_SOURCES})

# Set library alias for consistent naming
add_library(stack::lib ALIAS stack_lib)

# =============================================================================
# Library Configuration
# =============================================================================

# C++ standard requirements
target_compile_features(stack_lib PUBLIC cxx_std_20)

# Compiler warning configuration
target_compile_options(stack_lib PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

# Include directories
target_include_directories(stack_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

# Add include directories
target_include_directories(stack_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${libtins_SOURCE_DIR}/include"
)

# System include for libtins to avoid compiler warnings
target_include_directories(stack_lib SYSTEM PRIVATE ${libtins_SOURCE_DIR}/include)

# Configure target file sets for headers
target_sources(stack_lib PRIVATE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
    FILES ${CORE_HEADERS}
)

# Compile definitions
target_compile_definitions(stack_lib PUBLIC 
    BOOST_ASIO_HAS_CO_AWAIT
)

# Link dependencies
target_link_libraries(stack_lib 
    PUBLIC
        Boost::asio
        spdlog::spdlog
        Threads::Threads
        tins  # Make tins PUBLIC so executables inherit its SYSTEM includes
)

# =============================================================================
# Platform-Specific Configuration
# =============================================================================

# Apply platform-specific configuration
configure_platform_target(stack_lib)
