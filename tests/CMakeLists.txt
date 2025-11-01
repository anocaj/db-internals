# Tests CMakeLists.txt

# Function to create test executables
function(create_test test_name source_file)
    add_executable(${test_name} ${source_file})
    target_link_libraries(${test_name}
        database_internals
        gtest_main
        gmock_main
    )
    
    # Add compiler flags for tests
    target_compile_options(${test_name} PRIVATE
        -Wall -Wextra -Wpedantic
    )
    
    # Register test with CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
    
    # Set test properties
    set_tests_properties(${test_name} PROPERTIES
        TIMEOUT 30
        LABELS "unit"
    )
endfunction()

# Unit test subdirectories (will be populated in later tasks)
add_subdirectory(unit)
add_subdirectory(integration)

# Create a target to run all tests
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS 
    COMMENT "Running all tests"
)