# B+ Tree Module CMakeLists.txt

# Create header-only library for B+ Tree implementation
add_library(btree_lib INTERFACE)

# Set include directories
target_include_directories(btree_lib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/btree>
)

# Link with threading library
target_link_libraries(btree_lib INTERFACE Threads::Threads)

# Installation
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/btree
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)