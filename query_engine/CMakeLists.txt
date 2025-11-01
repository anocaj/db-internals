# Query Engine Module CMakeLists.txt

# Create header-only library for now (will be populated in later tasks)
add_library(query_engine_lib INTERFACE)

# Set include directories
target_include_directories(query_engine_lib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/query_engine>
)

# Link with threading library and all other modules
target_link_libraries(query_engine_lib INTERFACE 
    Threads::Threads
    btree_lib
    hash_index_lib
    columnar_storage_lib
    lsm_lib
)

# Installation
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/query_engine
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)