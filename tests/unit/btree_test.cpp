#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../../btree/bplus_tree.hpp"
#include <vector>
#include <random>
#include <algorithm>
#include <set>

using namespace testing;
using namespace btree;

class BPlusTreeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create trees with different branching factors for testing
        small_tree = std::make_unique<BPlusTree<int, std::string>>(4);  // Small branching factor
        medium_tree = std::make_unique<BPlusTree<int, std::string>>(8); // Medium branching factor
        large_tree = std::make_unique<BPlusTree<int, std::string>>(64); // Default branching factor
    }

    void TearDown() override {
        small_tree.reset();
        medium_tree.reset();
        large_tree.reset();
    }

    std::unique_ptr<BPlusTree<int, std::string>> small_tree;
    std::unique_ptr<BPlusTree<int, std::string>> medium_tree;
    std::unique_ptr<BPlusTree<int, std::string>> large_tree;
};

// Test basic insertion functionality
TEST_F(BPlusTreeTest, BasicInsertion) {
    // Test insertion into empty tree
    EXPECT_TRUE(small_tree->empty());
    EXPECT_TRUE(small_tree->insert(10, "ten"));
    EXPECT_FALSE(small_tree->empty());
    
    // Test single key search
    auto result = small_tree->search(10);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "ten");
    
    // Test non-existent key
    auto missing = small_tree->search(20);
    EXPECT_FALSE(missing.has_value());
}

// Test multiple insertions and ordering
TEST_F(BPlusTreeTest, MultipleInsertions) {
    std::vector<std::pair<int, std::string>> test_data = {
        {5, "five"}, {15, "fifteen"}, {10, "ten"}, {20, "twenty"}, {1, "one"}
    };
    
    // Insert all data
    for (const auto& [key, value] : test_data) {
        EXPECT_TRUE(small_tree->insert(key, value));
    }
    
    // Verify all insertions
    for (const auto& [key, value] : test_data) {
        auto result = small_tree->search(key);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), value);
    }
}

// Test insertion with duplicate keys (should update value)
TEST_F(BPlusTreeTest, DuplicateKeyInsertion) {
    EXPECT_TRUE(small_tree->insert(10, "ten"));
    EXPECT_TRUE(small_tree->insert(10, "updated_ten"));
    
    auto result = small_tree->search(10);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "updated_ten");
}

// Test basic deletion functionality
TEST_F(BPlusTreeTest, BasicDeletion) {
    // Insert some data
    small_tree->insert(10, "ten");
    small_tree->insert(20, "twenty");
    small_tree->insert(30, "thirty");
    
    // Test successful deletion
    EXPECT_TRUE(small_tree->remove(20));
    auto result = small_tree->search(20);
    EXPECT_FALSE(result.has_value());
    
    // Verify other keys still exist
    EXPECT_TRUE(small_tree->search(10).has_value());
    EXPECT_TRUE(small_tree->search(30).has_value());
    
    // Test deletion of non-existent key
    EXPECT_FALSE(small_tree->remove(40));
}

// Test deletion from empty tree
TEST_F(BPlusTreeTest, DeletionFromEmptyTree) {
    EXPECT_FALSE(small_tree->remove(10));
}

// Test range queries with basic data
TEST_F(BPlusTreeTest, BasicRangeQuery) {
    // Insert test data
    std::vector<std::pair<int, std::string>> test_data = {
        {1, "one"}, {3, "three"}, {5, "five"}, {7, "seven"}, {9, "nine"}
    };
    
    for (const auto& [key, value] : test_data) {
        small_tree->insert(key, value);
    }
    
    // Test range query [3, 7]
    auto range_result = small_tree->range_query(3, 7);
    EXPECT_EQ(range_result.size(), 3);
    
    // Verify results are in order and correct
    std::vector<std::pair<int, std::string>> expected = {
        {3, "three"}, {5, "five"}, {7, "seven"}
    };
    EXPECT_EQ(range_result, expected);
}

// Test range queries with various distributions
TEST_F(BPlusTreeTest, RangeQueryVariousDistributions) {
    // Test with sequential data
    for (int i = 1; i <= 20; ++i) {
        medium_tree->insert(i, "value_" + std::to_string(i));
    }
    
    // Test full range
    auto full_range = medium_tree->range_query(1, 20);
    EXPECT_EQ(full_range.size(), 20);
    
    // Test partial range
    auto partial_range = medium_tree->range_query(5, 15);
    EXPECT_EQ(partial_range.size(), 11);
    
    // Test single element range
    auto single_range = medium_tree->range_query(10, 10);
    EXPECT_EQ(single_range.size(), 1);
    EXPECT_EQ(single_range[0].first, 10);
    
    // Test empty range
    auto empty_range = medium_tree->range_query(25, 30);
    EXPECT_EQ(empty_range.size(), 0);
}

// Test range queries with random data distribution
TEST_F(BPlusTreeTest, RangeQueryRandomDistribution) {
    std::vector<int> keys;
    std::random_device rd;
    std::mt19937 gen(42); // Fixed seed for reproducible tests
    std::uniform_int_distribution<> dis(1, 1000);
    
    // Generate random keys
    std::set<int> unique_keys;
    while (unique_keys.size() < 50) {
        unique_keys.insert(dis(gen));
    }
    
    keys.assign(unique_keys.begin(), unique_keys.end());
    
    // Insert random data
    for (int key : keys) {
        medium_tree->insert(key, "value_" + std::to_string(key));
    }
    
    // Test range query on subset
    std::sort(keys.begin(), keys.end());
    int start_key = keys[10];
    int end_key = keys[40];
    
    auto range_result = medium_tree->range_query(start_key, end_key);
    
    // Verify all results are within range and sorted
    for (size_t i = 0; i < range_result.size(); ++i) {
        EXPECT_GE(range_result[i].first, start_key);
        EXPECT_LE(range_result[i].first, end_key);
        
        if (i > 0) {
            EXPECT_LT(range_result[i-1].first, range_result[i].first);
        }
    }
}

// Test tree structure invariants after insertions
TEST_F(BPlusTreeTest, TreeStructureAfterInsertions) {
    // Insert enough data to force tree growth
    for (int i = 1; i <= 100; ++i) {
        EXPECT_TRUE(small_tree->insert(i, "value_" + std::to_string(i)));
    }
    
    // Verify all keys can be found
    for (int i = 1; i <= 100; ++i) {
        auto result = small_tree->search(i);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), "value_" + std::to_string(i));
    }
    
    // Test range query covers all data
    auto full_range = small_tree->range_query(1, 100);
    EXPECT_EQ(full_range.size(), 100);
    
    // Verify range query results are sorted
    for (size_t i = 1; i < full_range.size(); ++i) {
        EXPECT_LT(full_range[i-1].first, full_range[i].first);
    }
}

// Test tree structure invariants after deletions
TEST_F(BPlusTreeTest, TreeStructureAfterDeletions) {
    // Insert data
    for (int i = 1; i <= 50; ++i) {
        small_tree->insert(i, "value_" + std::to_string(i));
    }
    
    // Delete every other element
    for (int i = 2; i <= 50; i += 2) {
        EXPECT_TRUE(small_tree->remove(i));
    }
    
    // Verify remaining elements
    for (int i = 1; i <= 50; ++i) {
        auto result = small_tree->search(i);
        if (i % 2 == 1) {
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(result.value(), "value_" + std::to_string(i));
        } else {
            EXPECT_FALSE(result.has_value());
        }
    }
    
    // Test range query on remaining data
    auto range_result = small_tree->range_query(1, 50);
    EXPECT_EQ(range_result.size(), 25); // Only odd numbers remain
    
    // Verify all results are odd numbers
    for (const auto& [key, value] : range_result) {
        EXPECT_EQ(key % 2, 1);
    }
}

// Test mixed operations (insert, delete, search)
TEST_F(BPlusTreeTest, MixedOperations) {
    std::vector<int> keys = {10, 5, 15, 3, 7, 12, 18, 1, 4, 6, 8, 11, 13, 16, 20};
    
    // Insert all keys
    for (int key : keys) {
        EXPECT_TRUE(medium_tree->insert(key, "value_" + std::to_string(key)));
    }
    
    // Delete some keys
    std::vector<int> to_delete = {3, 7, 13, 18};
    for (int key : to_delete) {
        EXPECT_TRUE(medium_tree->remove(key));
    }
    
    // Verify remaining keys
    std::set<int> remaining_keys(keys.begin(), keys.end());
    for (int key : to_delete) {
        remaining_keys.erase(key);
    }
    
    for (int key : remaining_keys) {
        auto result = medium_tree->search(key);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), "value_" + std::to_string(key));
    }
    
    // Verify deleted keys are gone
    for (int key : to_delete) {
        EXPECT_FALSE(medium_tree->search(key).has_value());
    }
    
    // Test range query
    auto range_result = medium_tree->range_query(5, 15);
    
    // Count expected results in range [5, 15] excluding deleted keys
    int expected_count = 0;
    for (int key : remaining_keys) {
        if (key >= 5 && key <= 15) {
            expected_count++;
        }
    }
    
    EXPECT_EQ(range_result.size(), expected_count);
}

// Test large dataset operations
TEST_F(BPlusTreeTest, LargeDatasetOperations) {
    const int dataset_size = 1000;
    
    // Insert large dataset
    for (int i = 0; i < dataset_size; ++i) {
        EXPECT_TRUE(large_tree->insert(i, "value_" + std::to_string(i)));
    }
    
    // Random search operations
    std::random_device rd;
    std::mt19937 gen(42);
    std::uniform_int_distribution<> dis(0, dataset_size - 1);
    
    for (int i = 0; i < 100; ++i) {
        int key = dis(gen);
        auto result = large_tree->search(key);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), "value_" + std::to_string(key));
    }
    
    // Large range query
    auto large_range = large_tree->range_query(100, 899);
    EXPECT_EQ(large_range.size(), 800);
    
    // Verify range query results are sorted
    for (size_t i = 1; i < large_range.size(); ++i) {
        EXPECT_LT(large_range[i-1].first, large_range[i].first);
    }
}

// Test edge cases
TEST_F(BPlusTreeTest, EdgeCases) {
    // Test with minimum branching factor
    BPlusTree<int, std::string> min_tree(3);
    
    // Insert and test with minimum tree
    for (int i = 1; i <= 10; ++i) {
        EXPECT_TRUE(min_tree.insert(i, "value_" + std::to_string(i)));
    }
    
    // Test all operations work with minimum tree
    EXPECT_TRUE(min_tree.search(5).has_value());
    EXPECT_TRUE(min_tree.remove(5));
    EXPECT_FALSE(min_tree.search(5).has_value());
    
    auto range = min_tree.range_query(3, 7);
    EXPECT_EQ(range.size(), 4); // 3, 4, 6, 7 (5 was deleted)
    
    // Test range query with inverted bounds
    auto empty_range = min_tree.range_query(10, 5);
    EXPECT_EQ(empty_range.size(), 0);
    
    // Test range query with same start and end
    min_tree.insert(5, "five");
    auto single_range = min_tree.range_query(5, 5);
    EXPECT_EQ(single_range.size(), 1);
    EXPECT_EQ(single_range[0].first, 5);
}

// Test string keys
TEST_F(BPlusTreeTest, StringKeys) {
    BPlusTree<std::string, int> string_tree(8);
    
    std::vector<std::pair<std::string, int>> test_data = {
        {"apple", 1}, {"banana", 2}, {"cherry", 3}, {"date", 4}, {"elderberry", 5}
    };
    
    // Insert string keys
    for (const auto& [key, value] : test_data) {
        EXPECT_TRUE(string_tree.insert(key, value));
    }
    
    // Test search
    for (const auto& [key, value] : test_data) {
        auto result = string_tree.search(key);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), value);
    }
    
    // Test range query with strings
    auto range_result = string_tree.range_query("banana", "date");
    EXPECT_EQ(range_result.size(), 3); // banana, cherry, date
    
    // Verify alphabetical ordering
    EXPECT_EQ(range_result[0].first, "banana");
    EXPECT_EQ(range_result[1].first, "cherry");
    EXPECT_EQ(range_result[2].first, "date");
}

// Test tree consistency after complex operations
TEST_F(BPlusTreeTest, TreeConsistencyStressTest) {
    std::vector<int> keys;
    for (int i = 0; i < 200; ++i) {
        keys.push_back(i);
    }
    
    // Shuffle keys for random insertion order
    std::random_device rd;
    std::mt19937 gen(42);
    std::shuffle(keys.begin(), keys.end(), gen);
    
    // Insert in random order
    for (int key : keys) {
        EXPECT_TRUE(medium_tree->insert(key, "value_" + std::to_string(key)));
    }
    
    // Verify all keys exist and are searchable
    for (int i = 0; i < 200; ++i) {
        auto result = medium_tree->search(i);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), "value_" + std::to_string(i));
    }
    
    // Test comprehensive range query
    auto full_range = medium_tree->range_query(0, 199);
    EXPECT_EQ(full_range.size(), 200);
    
    // Verify range query maintains sorted order
    for (size_t i = 0; i < full_range.size(); ++i) {
        EXPECT_EQ(full_range[i].first, static_cast<int>(i));
        EXPECT_EQ(full_range[i].second, "value_" + std::to_string(i));
    }
    
    // Random deletions
    std::shuffle(keys.begin(), keys.end(), gen);
    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(medium_tree->remove(keys[i]));
    }
    
    // Verify remaining 100 keys
    std::set<int> deleted_keys(keys.begin(), keys.begin() + 100);
    for (int i = 0; i < 200; ++i) {
        auto result = medium_tree->search(i);
        if (deleted_keys.count(i)) {
            EXPECT_FALSE(result.has_value());
        } else {
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(result.value(), "value_" + std::to_string(i));
        }
    }
}