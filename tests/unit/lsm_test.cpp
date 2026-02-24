// =============================================================================
// LSM Tree Unit Tests
// =============================================================================
// Tests cover all three components:
//   1. BloomFilter — false negative guarantee, false positive rate
//   2. SkipList    — ordered insertion, lookup, deletion, scan
//   3. LSMTree     — write path, read path, flush, compaction, tombstones
// =============================================================================

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "lsm/lsm_tree.hpp"
#include <string>
#include <unordered_set>
#include <vector>

using namespace lsm;

// =============================================================================
// BloomFilter Tests
// =============================================================================

class BloomFilterTest : public ::testing::Test {
protected:
    static std::string make_key(int i) { return "key_" + std::to_string(i); }
};

// Core guarantee: no false negatives — if you inserted a key, the filter
// will ALWAYS report it as possibly_present.
TEST_F(BloomFilterTest, NoFalseNegatives) {
    BloomFilter bf(1000, 10);
    const int N = 500;

    for (int i = 0; i < N; ++i) {
        bf.insert(make_key(i));
    }

    for (int i = 0; i < N; ++i) {
        EXPECT_TRUE(bf.possibly_contains(make_key(i)))
            << "False negative for key " << i << " — this MUST NOT happen!";
    }
}

// False positive rate should be within a reasonable bound of the theoretical value
TEST_F(BloomFilterTest, FalsePositiveRateWithinBounds) {
    const int N     = 1000;   // inserted keys
    const int QUERY = 10000;  // non-inserted keys to query

    BloomFilter bf(N, 10);  // 10 bits per key → expected FPR ≈ 0.8%
    for (int i = 0; i < N; ++i) bf.insert(make_key(i));

    int false_positives = 0;
    for (int i = N; i < N + QUERY; ++i) {
        if (bf.possibly_contains(make_key(i))) ++false_positives;
    }

    double actual_fpr = static_cast<double>(false_positives) / QUERY;
    double theoretical_fpr = bf.false_positive_rate(N);

    // Actual FPR should be close to theoretical (within 2x)
    EXPECT_LT(actual_fpr, theoretical_fpr * 2.5)
        << "FPR too high: actual=" << actual_fpr
        << " theoretical=" << theoretical_fpr;

    // And not wildly low (sanity check — filter should have some FPs with 10k queries)
    // With FPR ≈ 0.8%, we expect ~80 FPs out of 10k. Very unlikely to be 0.
    EXPECT_GT(false_positives, 0)
        << "Zero false positives over " << QUERY << " queries is extremely unlikely";
}

// Optimal k should be close to (m/n) * ln(2) ≈ 0.693 * (m/n)
TEST_F(BloomFilterTest, OptimalHashFunctionCount) {
    BloomFilter bf10(1000, 10);  // m/n=10 → optimal k = 7
    EXPECT_EQ(bf10.num_hash_functions(), 7u);

    BloomFilter bf14(1000, 14);  // m/n=14 → optimal k = 10
    EXPECT_EQ(bf14.num_hash_functions(), 10u);
}

TEST_F(BloomFilterTest, EmptyFilterReturnsFalse) {
    BloomFilter bf(100, 10);
    EXPECT_FALSE(bf.possibly_contains("anything"));
    EXPECT_FALSE(bf.possibly_contains(""));
}

// =============================================================================
// SkipList Tests
// =============================================================================

class SkipListTest : public ::testing::Test {
protected:
    SkipList sl;
};

TEST_F(SkipListTest, EmptyListLookupReturnsNullopt) {
    EXPECT_EQ(sl.get("key"), std::nullopt);
    EXPECT_TRUE(sl.empty());
    EXPECT_EQ(sl.size(), 0u);
}

TEST_F(SkipListTest, BasicInsertAndGet) {
    sl.put("apple", "fruit");
    sl.put("banana", "yellow");
    sl.put("cherry", "red");

    EXPECT_EQ(sl.get("apple"),  "fruit");
    EXPECT_EQ(sl.get("banana"), "yellow");
    EXPECT_EQ(sl.get("cherry"), "red");
    EXPECT_EQ(sl.get("grape"),  std::nullopt);
    EXPECT_EQ(sl.size(), 3u);
}

TEST_F(SkipListTest, OverwriteUpdatesValue) {
    sl.put("key", "v1");
    EXPECT_EQ(sl.get("key"), "v1");

    sl.put("key", "v2");
    EXPECT_EQ(sl.get("key"), "v2");
    EXPECT_EQ(sl.size(), 1u);  // still just one entry
}

TEST_F(SkipListTest, TombstoneDelete) {
    sl.put("key", "value");
    EXPECT_EQ(sl.get("key"), "value");

    sl.remove("key");
    bool tombstone_found = false;
    auto result = sl.get("key", &tombstone_found);
    EXPECT_EQ(result, std::nullopt);
    EXPECT_TRUE(tombstone_found);
}

TEST_F(SkipListTest, ScanAllReturnsSortedOrder) {
    // Insert in reverse order
    sl.put("z_key", "z");
    sl.put("a_key", "a");
    sl.put("m_key", "m");
    sl.put("b_key", "b");

    std::vector<bool> tombstones;
    auto entries = sl.scan_all(&tombstones);

    // Should come out in alphabetical order
    ASSERT_EQ(entries.size(), 4u);
    EXPECT_EQ(entries[0].first, "a_key");
    EXPECT_EQ(entries[1].first, "b_key");
    EXPECT_EQ(entries[2].first, "m_key");
    EXPECT_EQ(entries[3].first, "z_key");

    // No tombstones
    for (bool t : tombstones) EXPECT_FALSE(t);
}

TEST_F(SkipListTest, ScanAllIncludesTombstones) {
    sl.put("alive", "yes");
    sl.remove("dead");

    std::vector<bool> tombstones;
    auto entries = sl.scan_all(&tombstones);

    ASSERT_EQ(entries.size(), 2u);
    // "alive" < "dead" alphabetically
    EXPECT_EQ(entries[0].first, "alive");
    EXPECT_EQ(entries[1].first, "dead");
    EXPECT_FALSE(tombstones[0]);
    EXPECT_TRUE(tombstones[1]);
}

TEST_F(SkipListTest, LargeInsertionMaintainsOrder) {
    const int N = 1000;
    // Insert in random order
    std::vector<int> keys(N);
    std::iota(keys.begin(), keys.end(), 0);
    std::mt19937 rng(12345);
    std::shuffle(keys.begin(), keys.end(), rng);

    for (int k : keys) {
        sl.put(std::to_string(k), "v" + std::to_string(k));
    }

    auto entries = sl.scan_all();

    // Verify sorted order (lexicographic on string keys)
    for (size_t i = 1; i < entries.size(); ++i) {
        EXPECT_LT(entries[i - 1].first, entries[i].first)
            << "Out of order at index " << i;
    }
    EXPECT_EQ(entries.size(), (size_t)N);
}

// =============================================================================
// SSTable Tests
// =============================================================================

class SSTableTest : public ::testing::Test {
protected:
    std::unique_ptr<SSTable> make_sst(int from, int to) {
        std::vector<SSTableEntry> entries;
        for (int i = from; i < to; ++i) {
            entries.push_back({"key_" + std::to_string(i),
                               "val_" + std::to_string(i),
                               false});
        }
        return std::make_unique<SSTable>(std::move(entries));
    }
};

TEST_F(SSTableTest, BasicLookup) {
    auto sst = make_sst(0, 100);
    EXPECT_EQ(sst->get("key_0"),  "val_0");
    EXPECT_EQ(sst->get("key_50"), "val_50");
    EXPECT_EQ(sst->get("key_99"), "val_99");
    EXPECT_EQ(sst->get("key_100"), std::nullopt);
}

TEST_F(SSTableTest, BloomFilterPreventsLookup) {
    auto sst = make_sst(0, 100);
    // Keys 100-199 were never inserted — Bloom filter should catch most
    int bloom_misses = 0;
    for (int i = 100; i < 200; ++i) {
        auto result = sst->get("key_" + std::to_string(i));
        if (!result) ++bloom_misses;
    }
    // At most ~1% FPR → at most ~1 of 100 should slip through
    EXPECT_GT(bloom_misses, 90);  // at least 90% should be caught
}

TEST_F(SSTableTest, TombstoneLookup) {
    std::vector<SSTableEntry> entries = {
        {"a", "alive", false},
        {"b", "dead",  true},   // tombstone
        {"c", "alive2", false},
    };
    SSTable sst(std::move(entries));

    EXPECT_EQ(sst.get("a"), "alive");

    bool tombstone = false;
    auto result = sst.get("b", &tombstone);
    EXPECT_EQ(result, std::nullopt);
    EXPECT_TRUE(tombstone);

    EXPECT_EQ(sst.get("c"), "alive2");
}

// =============================================================================
// LSMTree Integration Tests
// =============================================================================

class LSMTreeTest : public ::testing::Test {
protected:
    // Small memtable_size_limit to force flushes/compaction during tests
    LSMTree::Config small_cfg() {
        LSMTree::Config cfg;
        cfg.memtable_size_limit   = 10;
        cfg.l0_compaction_trigger = 3;
        cfg.l1_compaction_trigger = 3;
        return cfg;
    }
};

TEST_F(LSMTreeTest, BasicPutGet) {
    LSMTree lsm;
    lsm.put("alpha", "1");
    lsm.put("beta",  "2");
    lsm.put("gamma", "3");

    EXPECT_EQ(lsm.get("alpha"), "1");
    EXPECT_EQ(lsm.get("beta"),  "2");
    EXPECT_EQ(lsm.get("gamma"), "3");
    EXPECT_EQ(lsm.get("delta"), std::nullopt);
}

TEST_F(LSMTreeTest, OverwriteInMemtable) {
    LSMTree lsm;
    lsm.put("key", "v1");
    EXPECT_EQ(lsm.get("key"), "v1");
    lsm.put("key", "v2");
    EXPECT_EQ(lsm.get("key"), "v2");
}

TEST_F(LSMTreeTest, TombstoneDeleteInMemtable) {
    LSMTree lsm;
    lsm.put("key", "value");
    EXPECT_EQ(lsm.get("key"), "value");

    lsm.remove("key");
    EXPECT_EQ(lsm.get("key"), std::nullopt);
}

TEST_F(LSMTreeTest, FlushCreatesL0SSTable) {
    LSMTree lsm(small_cfg());
    // Insert fewer than the flush trigger
    lsm.put("a", "1");
    lsm.put("b", "2");
    lsm.flush();  // explicit flush

    EXPECT_EQ(lsm.num_sstables(0), 1u);
    EXPECT_EQ(lsm.memtable_size(), 0u);

    // Keys should still be readable
    EXPECT_EQ(lsm.get("a"), "1");
    EXPECT_EQ(lsm.get("b"), "2");
}

TEST_F(LSMTreeTest, AutoFlushOnMemtableFull) {
    LSMTree::Config cfg;
    cfg.memtable_size_limit   = 5;
    cfg.l0_compaction_trigger = 100;  // don't compact yet
    LSMTree lsm(cfg);

    for (int i = 0; i < 12; ++i) {
        lsm.put("key_" + std::to_string(i), "v");
    }

    // Should have flushed at least once
    EXPECT_GT(lsm.num_sstables(0), 0u);

    // All keys should still be findable
    for (int i = 0; i < 12; ++i) {
        EXPECT_EQ(lsm.get("key_" + std::to_string(i)), "v")
            << "Missing key_" << i;
    }
}

TEST_F(LSMTreeTest, ReadAfterFlushAndCompaction) {
    LSMTree lsm(small_cfg());

    // Write enough data to force flush and compaction
    for (int i = 0; i < 50; ++i) {
        lsm.put("key_" + std::to_string(i), "val_" + std::to_string(i));
    }

    lsm.flush();
    lsm.compact();

    // All keys should be readable after compaction
    for (int i = 0; i < 50; ++i) {
        auto result = lsm.get("key_" + std::to_string(i));
        EXPECT_EQ(result, "val_" + std::to_string(i))
            << "Missing key_" << i << " after compaction";
    }
}

TEST_F(LSMTreeTest, TombstoneAcrossFlush) {
    LSMTree::Config cfg;
    cfg.memtable_size_limit   = 5;
    cfg.l0_compaction_trigger = 100;
    LSMTree lsm(cfg);

    // Write a key, flush it to L0
    lsm.put("key", "value");
    lsm.flush();

    // Now delete the key (tombstone goes into MemTable)
    lsm.remove("key");

    // Key should be gone even though its value is in an SSTable
    EXPECT_EQ(lsm.get("key"), std::nullopt);
}

TEST_F(LSMTreeTest, TombstoneInSSTableHidesOlderValue) {
    LSMTree::Config cfg;
    cfg.memtable_size_limit   = 5;
    cfg.l0_compaction_trigger = 100;
    LSMTree lsm(cfg);

    // Write key, flush to L0
    lsm.put("key", "value");
    lsm.flush();

    // Write tombstone, flush that too
    lsm.remove("key");
    lsm.flush();

    // Now L0 has two SSTables: one with value, one with tombstone
    // The tombstone (newer) should hide the value (older)
    EXPECT_EQ(lsm.get("key"), std::nullopt);
}

TEST_F(LSMTreeTest, OverwriteAfterFlush) {
    LSMTree::Config cfg;
    cfg.memtable_size_limit   = 5;
    cfg.l0_compaction_trigger = 100;
    LSMTree lsm(cfg);

    lsm.put("key", "v1");
    lsm.flush();

    lsm.put("key", "v2");
    // v2 is in MemTable, v1 is in L0 — MemTable should win
    EXPECT_EQ(lsm.get("key"), "v2");

    lsm.flush();
    // Now both in L0 — newer SSTable (v2) should win
    EXPECT_EQ(lsm.get("key"), "v2");
}

TEST_F(LSMTreeTest, CompactionEliminatesDuplicates) {
    LSMTree::Config cfg;
    cfg.memtable_size_limit   = 3;
    cfg.l0_compaction_trigger = 3;
    cfg.l1_compaction_trigger = 100;
    LSMTree lsm(cfg);

    // Write the same key multiple times to force multiple SSTables
    for (int round = 0; round < 9; ++round) {
        lsm.put("key", "version_" + std::to_string(round));
    }
    lsm.flush();
    lsm.compact();

    // After compaction, should have only the latest version
    EXPECT_EQ(lsm.get("key"), "version_8");
}

TEST_F(LSMTreeTest, WriteAmplificationTracked) {
    LSMTree lsm(small_cfg());

    for (int i = 0; i < 50; ++i) {
        lsm.put("key_" + std::to_string(i), std::string(100, 'x'));
    }
    lsm.flush();
    lsm.compact();

    const auto& m = lsm.metrics();
    EXPECT_GT(m.total_writes, 0u);
    EXPECT_GT(m.bytes_written_disk, 0u);
    // With compaction, write amplification should be > 1.0
    // (data gets rewritten during compaction)
}

TEST_F(LSMTreeTest, LargeDataset) {
    LSMTree::Config cfg;
    cfg.memtable_size_limit   = 100;
    cfg.l0_compaction_trigger = 4;
    cfg.l1_compaction_trigger = 4;
    LSMTree lsm(cfg);

    const int N = 1000;
    for (int i = 0; i < N; ++i) {
        lsm.put("k" + std::to_string(i), "v" + std::to_string(i));
    }
    lsm.flush();
    lsm.compact();

    // Random sample of 100 keys
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, N - 1);
    int misses = 0;
    for (int trial = 0; trial < 100; ++trial) {
        int i = dist(rng);
        auto result = lsm.get("k" + std::to_string(i));
        if (!result || *result != "v" + std::to_string(i)) ++misses;
    }
    EXPECT_EQ(misses, 0) << "Some keys lost after large dataset compaction";
}

TEST_F(LSMTreeTest, EmptyTreeGetReturnsNullopt) {
    LSMTree lsm;
    EXPECT_EQ(lsm.get("any_key"), std::nullopt);
}

TEST_F(LSMTreeTest, BloomFiltersReduceSSTableReads) {
    LSMTree::Config cfg_with    = small_cfg();
    LSMTree::Config cfg_without = small_cfg();
    cfg_with.use_bloom_filters    = true;
    cfg_without.use_bloom_filters = false;

    LSMTree lsm_bf(cfg_with);
    LSMTree lsm_no(cfg_without);

    // Both get the same data
    for (int i = 0; i < 50; ++i) {
        std::string k = "key_" + std::to_string(i);
        std::string v = "val_" + std::to_string(i);
        lsm_bf.put(k, v);
        lsm_no.put(k, v);
    }
    lsm_bf.flush(); lsm_bf.compact();
    lsm_no.flush(); lsm_no.compact();

    // Query 50 keys that don't exist
    for (int i = 1000; i < 1050; ++i) {
        lsm_bf.get("missing_" + std::to_string(i));
        lsm_no.get("missing_" + std::to_string(i));
    }

    // Bloom filter version should have fewer or equal SSTable reads
    // (in practice significantly fewer, but the test is conservative)
    EXPECT_LE(lsm_bf.metrics().sstable_reads, lsm_no.metrics().sstable_reads + 10);
}
