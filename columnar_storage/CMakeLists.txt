# Columnar Storage Module CMakeLists.txt

# Create header-only library for now (will be populated in later tasks)
add_library(columnar_storage_lib INTERFACE)

# Set include directories
target_include_directories(columnar_storage_lib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/columnar_storage>
)

# Link with threading library
target_link_libraries(columnar_storage_lib INTERFACE Threads::Threads)

# Installation
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/columnar_storage
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)