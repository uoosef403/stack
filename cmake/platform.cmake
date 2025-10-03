# =============================================================================
# Stack Network Library - Platform-Specific Configuration
# =============================================================================

# =============================================================================
# Windows-Specific Configuration
# =============================================================================

if(WIN32)
    # WinTun VPN adapter library
    include(FetchContent)
    FetchContent_Declare(
        wintun
        URL "https://www.wintun.net/builds/wintun-0.14.1.zip"
        URL_HASH SHA256=07c256185d6ee3652e09fa55c0b673e2624b565e02c4b9091c79ca7d2f24ef51
    )
    FetchContent_MakeAvailable(wintun)

    # Set WinTun directories for global use
    set(WINTUN_DIR ${wintun_SOURCE_DIR})
    
    # Debug: Print WinTun directory for troubleshooting
    message(STATUS "WinTun directory: ${WINTUN_DIR}")
    message(STATUS "WinTun include directory: ${WINTUN_DIR}/include")
    
    # Verify WinTun header exists
    if(NOT EXISTS "${WINTUN_DIR}/include/wintun.h")
        message(FATAL_ERROR "WinTun header not found at ${WINTUN_DIR}/include/wintun.h")
    else()
        message(STATUS "WinTun header found at ${WINTUN_DIR}/include/wintun.h")
    endif()
    
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

    # Function to copy WinTun DLL to executable directory
    function(copy_wintun_dll TARGET_NAME)
        # Copy DLL to output directory after build
        add_custom_command(
            TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WINTUN_DLL}"
            "$<TARGET_FILE_DIR:${TARGET_NAME}>/wintun.dll"
            COMMENT "Copying wintun.dll for target ${TARGET_NAME}"
        )
    endfunction()
    
    # Set global variables for use in main CMakeLists.txt
    set(WINTUN_DIR ${WINTUN_DIR} CACHE INTERNAL "WinTun directory")
    set(WINTUN_DLL ${WINTUN_DLL} CACHE INTERNAL "WinTun DLL path")
endif()

# =============================================================================
# Platform-Specific Target Configuration Function
# =============================================================================

# Function to apply platform-specific configuration to a target
function(configure_platform_target TARGET_NAME)
    if(WIN32)
        # Windows API definitions
        target_compile_definitions(${TARGET_NAME} PRIVATE _WIN32_WINNT=0x0601)
        
        # Add WinTun include directory
        target_include_directories(${TARGET_NAME} SYSTEM PRIVATE ${WINTUN_DIR}/include)
        
        # Windows system libraries
        target_link_libraries(${TARGET_NAME} PRIVATE 
            iphlpapi 
            winmm 
            ws2_32 
            Rpcrt4
        )
        
        # Copy WinTun DLL for the target
        copy_wintun_dll(${TARGET_NAME})
    endif()
endfunction()
