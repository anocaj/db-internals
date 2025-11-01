# LSM Tree Module CMakeLists.txt

# Create header-only library for now (will be populated in later tasks)
add_library(lsm_lib INTERFACE)

# Set include directories
target_include_directories(lsm_lib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/lsm>
)

# Link with threading library and btree module
target_link_libraries(lsm_lib INTERFACE 
    Threads::Threads
    btree_lib
    hash_index_lib
)

# Installation
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lsm
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)