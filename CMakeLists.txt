cmake_minimum_required(VERSION 3.16)
project(DatabaseInternals VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for different build types
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic -fsanitize=address")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")

# Default to Release build if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Enable testing
enable_testing()

# Find required packages
find_package(Threads REQUIRED)

# Google Test setup
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Include directories
include_directories(${CMAKE_SOURCE_DIR})

# Add subdirectories for each module
add_subdirectory(btree)
add_subdirectory(hash_index)
add_subdirectory(columnar_storage)
add_subdirectory(lsm)
add_subdirectory(query_engine)
add_subdirectory(tests)
add_subdirectory(benchmarks)
add_subdirectory(examples)

# Create a main library that combines all modules
add_library(database_internals INTERFACE)
target_link_libraries(database_internals INTERFACE
    btree_lib
    hash_index_lib
    columnar_storage_lib
    lsm_lib
    query_engine_lib
)

# Installation rules - Install all libraries together
install(TARGETS 
    database_internals
    btree_lib
    hash_index_lib
    columnar_storage_lib
    lsm_lib
    query_engine_lib
    EXPORT DatabaseInternalsTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Export configuration
install(EXPORT DatabaseInternalsTargets
    FILE DatabaseInternalsTargets.cmake
    NAMESPACE DatabaseInternals::
    DESTINATION lib/cmake/DatabaseInternals
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    DatabaseInternalsConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/DatabaseInternalsConfigVersion.cmake
    DESTINATION lib/cmake/DatabaseInternals
)