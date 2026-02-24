# LSM Tree Module CMakeLists.txt

# Header-only INTERFACE library (all implementation is in lsm_tree.hpp)
add_library(lsm_lib INTERFACE)

# Set include directories
target_include_directories(lsm_lib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/lsm>
)

# Link with threading library
target_link_libraries(lsm_lib INTERFACE
    Threads::Threads
)

# Installation
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lsm
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
