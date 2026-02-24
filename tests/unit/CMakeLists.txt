# Unit Tests CMakeLists.txt

# B+ Tree unit tests
create_test(btree_test btree_test.cpp)

# Hash Index unit tests
create_test(hash_index_test hash_index_test.cpp)

# LSM Tree unit tests
create_test(lsm_test lsm_test.cpp)

# Additional unit test files will be added in later tasks
# create_test(columnar_storage_test columnar_storage_test.cpp)
# create_test(query_engine_test query_engine_test.cpp)