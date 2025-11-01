# Examples CMakeLists.txt

# Function to create example executables
function(create_example example_name source_file)
    add_executable(${example_name} ${source_file})
    target_link_libraries(${example_name}
        database_internals
    )
    
    # Add compiler flags for examples
    target_compile_options(${example_name} PRIVATE
        -Wall -Wextra
    )
    
    # Set example properties
    set_target_properties(${example_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
    )
endfunction()

# Example files will be added in later tasks
# Examples:
# create_example(btree_demo btree_demo.cpp)
# create_example(hash_index_demo hash_index_demo.cpp)
# create_example(columnar_storage_demo columnar_storage_demo.cpp)
# create_example(lsm_demo lsm_demo.cpp)
# create_example(query_engine_demo query_engine_demo.cpp)

# Create a target to build all examples
add_custom_target(build_examples
    COMMENT "Building all examples"
)