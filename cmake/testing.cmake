# =============================================================================
# Stack Network Library - Testing Configuration
# =============================================================================

# =============================================================================
# Test Target Configuration
# =============================================================================

# Collect test files
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
)

# Only create test targets if test files exist
if(TEST_SOURCES)
    # Create test executable (only with test sources, no core sources)
    add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})
    
    # Set test executable alias
    add_executable(stack::tests ALIAS ${PROJECT_NAME}_tests)
    
    # Configure test target with reduced warning level (no -Werror for tests)
    target_compile_options(${PROJECT_NAME}_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wno-nested-anon-types>
    )
    
    # Link against our library and test dependencies
    target_link_libraries(${PROJECT_NAME}_tests PRIVATE
        stack::lib        # Our main library - inherits all dependencies
        gtest_main
        gmock_main
    )
    
    # Apply platform-specific configuration to test target
    configure_platform_target(${PROJECT_NAME}_tests)
    
    # Discover and register tests
    gtest_discover_tests(${PROJECT_NAME}_tests)
    
    message(STATUS "Test executable '${PROJECT_NAME}_tests' will be created")
else()
    message(STATUS "No test files found in tests/ directory - skipping test target creation")
endif()
