# Benchmarks CMakeLists.txt

# Function to create benchmark executables
function(create_benchmark benchmark_name source_file)
    add_executable(${benchmark_name} ${source_file})
    target_link_libraries(${benchmark_name}
        database_internals
    )
    
    # Add compiler flags for benchmarks (optimized)
    target_compile_options(${benchmark_name} PRIVATE
        -O3 -DNDEBUG -march=native
    )
    
    # Set benchmark properties
    set_target_properties(${benchmark_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmarks
    )
endfunction()

# Benchmark files will be added in later tasks
# Examples:
# create_benchmark(scan_vs_index_bench scan_vs_index_bench.cpp)
# create_benchmark(compression_bench compression_bench.cpp)
# create_benchmark(query_performance_bench query_performance_bench.cpp)

# Create a target to run all benchmarks
add_custom_target(run_benchmarks
    COMMAND echo "Benchmarks will be available after implementation"
    COMMENT "Running all benchmarks"
)