# Hash Index Module CMakeLists.txt

# Create header-only library for now (will be populated in later tasks)
add_library(hash_index_lib INTERFACE)

# Set include directories
target_include_directories(hash_index_lib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/hash_index>
)

# Link with threading library
target_link_libraries(hash_index_lib INTERFACE Threads::Threads)

# Installation
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/hash_index
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)