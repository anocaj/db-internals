#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../../hash_index/hash_index.hpp"
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <unordered_map>

using namespace hash_index;
using namespace testing;

// ============================================================================
// ChainingHashIndex Tests
// ============================================================================

class ChainingHashTest : public ::testing::Test {
protected:
    ChainingHashIndex<int, std::string>    int_map;
    ChainingHashIndex<std::string, int>    str_map;
};

TEST_F(ChainingHashTest, EmptyOnConstruct) {
    EXPECT_TRUE(int_map.empty());
    EXPECT_EQ(int_map.size(), 0u);
}

TEST_F(ChainingHashTest, BasicInsertAndSearch) {
    int_map.insert(1, "one");
    EXPECT_FALSE(int_map.empty());
    EXPECT_EQ(int_map.size(), 1u);

    auto result = int_map.search(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "one");

    EXPECT_FALSE(int_map.search(99).has_value());
}

TEST_F(ChainingHashTest, UpdateExistingKey) {
    int_map.insert(1, "one");
    int_map.insert(1, "ONE");
    EXPECT_EQ(int_map.size(), 1u);
    EXPECT_EQ(int_map.search(1).value(), "ONE");
}

TEST_F(ChainingHashTest, BasicRemove) {
    int_map.insert(1, "one");
    int_map.insert(2, "two");

    EXPECT_TRUE(int_map.remove(1));
    EXPECT_FALSE(int_map.search(1).has_value());
    EXPECT_TRUE(int_map.search(2).has_value());
    EXPECT_EQ(int_map.size(), 1u);

    // Remove non-existent key
    EXPECT_FALSE(int_map.remove(99));
}

TEST_F(ChainingHashTest, StringKeys) {
    str_map.insert("apple", 1);
    str_map.insert("banana", 2);
    str_map.insert("cherry", 3);

    EXPECT_EQ(str_map.search("apple").value(), 1);
    EXPECT_EQ(str_map.search("banana").value(), 2);
    EXPECT_EQ(str_map.search("cherry").value(), 3);
    EXPECT_FALSE(str_map.search("grape").has_value());
}

TEST_F(ChainingHashTest, Rehashing) {
    // Insert enough elements to trigger rehash (load factor > 0.75)
    size_t initial_buckets = int_map.bucket_count();

    for (int i = 0; i < 100; ++i) {
        int_map.insert(i, "value_" + std::to_string(i));
    }

    // Buckets should have grown
    EXPECT_GT(int_map.bucket_count(), initial_buckets);
    EXPECT_EQ(int_map.size(), 100u);

    // All values should still be accessible
    for (int i = 0; i < 100; ++i) {
        auto r = int_map.search(i);
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r.value(), "value_" + std::to_string(i));
    }
}

TEST_F(ChainingHashTest, LoadFactorStaysUnderThreshold) {
    const double threshold = ChainingHashIndex<int,std::string>::MAX_LOAD_FACTOR + 0.01;
    for (int i = 0; i < 200; ++i) {
        int_map.insert(i, "v");
        EXPECT_LE(int_map.load_factor(), threshold);
    }
}

TEST_F(ChainingHashTest, CorrectnessVsStdUnorderedMap) {
    std::unordered_map<int, std::string> ref;
    std::mt19937 gen(42);
    std::uniform_int_distribution<> key_dist(0, 200);

    for (int i = 0; i < 500; ++i) {
        int key = key_dist(gen);
        std::string val = "v" + std::to_string(i);

        int_map.insert(key, val);
        ref[key] = val;
    }

    for (const auto& [k, v] : ref) {
        auto r = int_map.search(k);
        ASSERT_TRUE(r.has_value()) << "Missing key: " << k;
        EXPECT_EQ(r.value(), v);
    }
}

// ============================================================================
// OpenAddressingHashIndex Tests
// ============================================================================

class OpenAddressingHashTest : public ::testing::Test {
protected:
    OpenAddressingHashIndex<int, std::string>    int_map;
    OpenAddressingHashIndex<std::string, int>    str_map;
};

TEST_F(OpenAddressingHashTest, EmptyOnConstruct) {
    EXPECT_TRUE(int_map.empty());
    EXPECT_EQ(int_map.size(), 0u);
}

TEST_F(OpenAddressingHashTest, BasicInsertAndSearch) {
    int_map.insert(1, "one");
    EXPECT_FALSE(int_map.empty());
    EXPECT_EQ(int_map.size(), 1u);

    auto result = int_map.search(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "one");

    EXPECT_FALSE(int_map.search(99).has_value());
}

TEST_F(OpenAddressingHashTest, UpdateExistingKey) {
    int_map.insert(1, "one");
    int_map.insert(1, "ONE");
    EXPECT_EQ(int_map.size(), 1u);
    EXPECT_EQ(int_map.search(1).value(), "ONE");
}

TEST_F(OpenAddressingHashTest, BasicRemove) {
    int_map.insert(1, "one");
    int_map.insert(2, "two");
    int_map.insert(3, "three");

    EXPECT_TRUE(int_map.remove(2));
    EXPECT_FALSE(int_map.search(2).has_value());
    // Keys around the deleted one still accessible (tombstone works)
    EXPECT_TRUE(int_map.search(1).has_value());
    EXPECT_TRUE(int_map.search(3).has_value());
    EXPECT_EQ(int_map.size(), 2u);

    EXPECT_FALSE(int_map.remove(99));
}

TEST_F(OpenAddressingHashTest, TombstonesAllowProbeChainIntegrity) {
    // Insert items that might hash to same bucket (small table, many items)
    OpenAddressingHashIndex<int, int> small(8);
    for (int i = 0; i < 4; ++i) small.insert(i, i * 10);

    // Remove middle of a potential chain
    small.remove(1);
    small.remove(2);

    // Items after tombstones in chain should still be reachable
    EXPECT_TRUE(small.search(3).has_value());
    EXPECT_EQ(small.search(3).value(), 30);
}

TEST_F(OpenAddressingHashTest, Rehashing) {
    size_t initial_slots = int_map.slot_count();

    for (int i = 0; i < 100; ++i) {
        int_map.insert(i, "value_" + std::to_string(i));
    }

    EXPECT_GT(int_map.slot_count(), initial_slots);
    EXPECT_EQ(int_map.size(), 100u);

    for (int i = 0; i < 100; ++i) {
        auto r = int_map.search(i);
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r.value(), "value_" + std::to_string(i));
    }
}

TEST_F(OpenAddressingHashTest, StringKeys) {
    str_map.insert("apple", 1);
    str_map.insert("banana", 2);
    str_map.insert("cherry", 3);

    EXPECT_EQ(str_map.search("apple").value(), 1);
    EXPECT_EQ(str_map.search("banana").value(), 2);
    EXPECT_EQ(str_map.search("cherry").value(), 3);
    EXPECT_FALSE(str_map.search("grape").has_value());
}

TEST_F(OpenAddressingHashTest, CorrectnessVsStdUnorderedMap) {
    std::unordered_map<int, std::string> ref;
    std::mt19937 gen(99);
    std::uniform_int_distribution<> key_dist(0, 200);

    for (int i = 0; i < 500; ++i) {
        int key = key_dist(gen);
        std::string val = "v" + std::to_string(i);
        int_map.insert(key, val);
        ref[key] = val;
    }

    for (const auto& [k, v] : ref) {
        auto r = int_map.search(k);
        ASSERT_TRUE(r.has_value()) << "Missing key: " << k;
        EXPECT_EQ(r.value(), v);
    }
}

TEST_F(OpenAddressingHashTest, InsertAfterRemove) {
    // Ensure tombstone slots are reused
    for (int i = 0; i < 10; ++i) int_map.insert(i, "v");
    for (int i = 0; i < 10; ++i) int_map.remove(i);

    EXPECT_EQ(int_map.size(), 0u);

    // Re-insert same keys
    for (int i = 0; i < 10; ++i) int_map.insert(i, "new_v");
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(int_map.search(i).has_value());
        EXPECT_EQ(int_map.search(i).value(), "new_v");
    }
}
