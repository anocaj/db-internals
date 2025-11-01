# Examples CMakeLists.txt

# Memory management demonstration
add_executable(memory_demo memory_demo.cpp)
target_link_libraries(memory_demo btree_lib)
target_compile_features(memory_demo PRIVATE cxx_std_17)

# Set compiler flags
target_compile_options(memory_demo PRIVATE
    -Wall -Wextra -Wpedantic
)