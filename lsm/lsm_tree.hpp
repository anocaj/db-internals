#pragma once

// =============================================================================
// LSM Tree (Log-Structured Merge-Tree): Write-Optimized Storage
// =============================================================================
//
// THE FUNDAMENTAL INSIGHT
// =======================
// B+ trees optimise for random reads: their sorted, in-place structure lets
// you find any key in O(log n) with minimal I/O.  But every INSERT or UPDATE
// on a B+ tree potentially touches a random page on disk — a random write.
// On spinning HDDs random writes are 100-1000x slower than sequential writes.
// On SSDs random writes cause write amplification and wear levelling overhead.
//
// LSM trees turn this upside down:
//   1. Accept every write into an in-memory buffer (MemTable) — fast, O(log n)
//   2. When the buffer is full, flush it to disk as a sorted, immutable file
//      (SSTable) — ONE large sequential write instead of many small random ones
//   3. Periodically merge (compact) SSTables to maintain bounded read cost
//
// The result: write throughput goes from ~1,000 random IOPS (B+ tree) to
// ~500,000 sequential bytes/sec (LSM tree). RocksDB sustains >1 GB/s of
// writes on NVMe by exploiting this principle.
//
// THE COST
// =========
// Nothing is free. LSM trees trade:
//   - Write amplification: data is written multiple times during compaction
//   - Read amplification: a point query may check multiple levels/files
//   - Space amplification: multiple versions of a key can coexist across levels
//
// This is the RUM Conjecture (Read, Update, Memory):
//   You cannot simultaneously minimise read overhead, update overhead, AND
//   memory/space overhead. Every storage structure picks two.
//
// ARCHITECTURE OVERVIEW
// =====================
//
//   Writes ---> [ MemTable (SkipList) ] --flush--> [ L0 SSTables ]
//                                                         |
//                                                    compaction
//                                                         |
//                                               [ L1 SSTables (sorted) ]
//                                                         |
//                                                    compaction
//                                                         |
//                                               [ L2 SSTables (sorted) ]
//                                                        ...
//
//   Reads: MemTable -> L0 -> L1 -> L2 -> ... (newest to oldest)
//          Bloom filters short-circuit most levels in O(1)
//
// COMPONENTS
// ==========
//   BloomFilter  — probabilistic membership test; eliminates ~99% of SSTable
//                  reads for missing keys
//   SkipList     — probabilistic balanced structure used as the MemTable;
//                  O(log n) insert/search, cache-friendlier than red-black tree
//                  during sequential scans
//   SSTable      — Sorted String Table; an immutable, sorted file written once
//                  and read many times
//   LSMTree      — orchestrates the above; tracks amplification metrics
//
// =============================================================================

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace lsm {

// =============================================================================
// FNV-1a hash (same as in hash_index.hpp — deterministic, fast, good spread)
// =============================================================================

inline uint64_t fnv1a(const void* data, size_t len, uint64_t seed = 0) {
    static constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    static constexpr uint64_t FNV_PRIME        = 1099511628211ULL;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint64_t hash = FNV_OFFSET_BASIS ^ seed;
    for (size_t i = 0; i < len; ++i) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

inline uint64_t hash_string(const std::string& s, uint64_t seed = 0) {
    return fnv1a(s.data(), s.size(), seed);
}

// =============================================================================
// BloomFilter
// =============================================================================
//
// A Bloom filter answers "is this key definitely NOT in the set?" in O(k) time
// and O(m) bits, where k is the number of hash functions and m is the bit array
// size.  It has no false negatives (if you inserted a key, the filter will say
// "maybe present") but a tunable false positive rate.
//
// MATHEMATICS
// -----------
// After inserting n keys into a filter with m bits using k hash functions,
// the probability that any particular bit is still 0 is:
//
//   P(bit = 0) = (1 - 1/m)^{kn} ≈ e^{-kn/m}
//
// The false positive rate (a key not in the set passes all k bit checks) is:
//
//   FPR = (1 - e^{-kn/m})^k
//
// This is minimised when k = (m/n) * ln(2) ≈ 0.693 * (m/n)
// At optimal k, FPR ≈ (0.6185)^{m/n}
//
// Example: m/n = 10 bits per key, k = 7 hash functions → FPR ≈ 0.8%
//          m/n = 14 bits per key, k = 10 hash functions → FPR ≈ 0.1%
//
// RocksDB uses 10 bits/key by default (NewBloomFilterPolicy(10)).
// =============================================================================

class BloomFilter {
public:
    // bits_per_key: m/n ratio. RocksDB default = 10.
    // Optimal k is computed automatically.
    explicit BloomFilter(size_t expected_keys, size_t bits_per_key = 10)
        : num_bits_(std::max(expected_keys * bits_per_key, size_t(64)))
        , bits_(num_bits_, false)
    {
        // k = (m/n) * ln(2) — optimal number of hash functions
        // We clamp to [1, 30] for practicality
        double optimal_k = static_cast<double>(bits_per_key) * std::log(2.0);
        num_hash_functions_ = static_cast<size_t>(
            std::max(1.0, std::min(30.0, std::round(optimal_k)))
        );
    }

    void insert(const std::string& key) {
        for (size_t i = 0; i < num_hash_functions_; ++i) {
            size_t bit = hash_string(key, i) % num_bits_;
            bits_[bit] = true;
        }
    }

    // Returns false  → key is DEFINITELY not present (zero false negatives)
    // Returns true   → key is PROBABLY present (false positive rate ≈ FPR)
    bool possibly_contains(const std::string& key) const {
        for (size_t i = 0; i < num_hash_functions_; ++i) {
            size_t bit = hash_string(key, i) % num_bits_;
            if (!bits_[bit]) return false;  // definite miss
        }
        return true;  // all bits set — probably present
    }

    // Theoretical false positive rate given current load
    double false_positive_rate(size_t num_keys) const {
        if (num_bits_ == 0 || num_keys == 0) return 0.0;
        double kn_over_m = static_cast<double>(num_hash_functions_ * num_keys)
                         / static_cast<double>(num_bits_);
        double base = 1.0 - std::exp(-kn_over_m);
        return std::pow(base, static_cast<double>(num_hash_functions_));
    }

    size_t num_bits()           const { return num_bits_; }
    size_t num_hash_functions() const { return num_hash_functions_; }

private:
    size_t num_bits_;
    size_t num_hash_functions_;
    std::vector<bool> bits_;
};


// =============================================================================
// SkipList — the MemTable
// =============================================================================
//
// WHY SKIPLIST OVER RED-BLACK TREE?
// ----------------------------------
// Both give O(log n) insert/search/delete.  The SkipList wins for MemTable use
// because:
//
//  1. Lock-free concurrent implementations are straightforward (compare-and-
//     swap on forward pointers). Red-black trees need complex rebalancing that
//     is hard to make lock-free.  RocksDB and LevelDB both use SkipLists for
//     this reason.
//
//  2. Sequential scan (flush to SSTable) is trivially O(n): just follow L0
//     forward pointers. An in-order BST traversal requires a stack or recursion.
//
//  3. Cache behaviour during flush: level-0 pointers form a linked list of
//     entries in sorted order — a single linear scan, prefetch-friendly.
//
// STRUCTURE
// ----------
// A SkipList is a probabilistic data structure with multiple levels of linked
// lists. Level 0 is a complete sorted linked list. Level 1 skips ~half the
// nodes. Level k skips ~2^k nodes on average.
//
// Each node is promoted to level k with probability p^k (p=0.25 or 0.5).
// Expected height for n elements ≈ log_{1/p}(n).  For n=1M, p=0.25 → ~10 levels.
//
// Search: O(log n) expected — at each level, advance until the next node
//         exceeds the target, then drop down one level.
//
// Insert: O(log n) expected — search to find predecessors at each level, then
//         splice in the new node.
// =============================================================================

static constexpr int SKIPLIST_MAX_LEVEL = 16;
static constexpr double SKIPLIST_P      = 0.25;  // level promotion probability

struct SkipNode {
    std::string key;
    std::string value;
    bool tombstone;  // true = this key has been deleted

    // forward[i] = pointer to next node at level i
    // We use a fixed-size array to avoid heap allocation per level
    std::array<SkipNode*, SKIPLIST_MAX_LEVEL> forward;

    SkipNode(std::string k, std::string v, int levels, bool tomb = false)
        : key(std::move(k))
        , value(std::move(v))
        , tombstone(tomb)
    {
        forward.fill(nullptr);
        (void)levels;  // stored externally in node_level
    }
};

class SkipList {
public:
    SkipList() : level_(1), size_(0), rng_(42) {
        // Sentinel header node: key = "", always at max level
        header_ = new SkipNode("", "", SKIPLIST_MAX_LEVEL);
    }

    ~SkipList() {
        SkipNode* cur = header_;
        while (cur) {
            SkipNode* next = cur->forward[0];
            delete cur;
            cur = next;
        }
    }

    // Non-copyable (owns raw pointers)
    SkipList(const SkipList&)            = delete;
    SkipList& operator=(const SkipList&) = delete;

    // Moveable
    SkipList(SkipList&& other) noexcept
        : header_(other.header_), level_(other.level_), size_(other.size_)
        , rng_(std::move(other.rng_))
    {
        other.header_ = new SkipNode("", "", SKIPLIST_MAX_LEVEL);
        other.level_  = 1;
        other.size_   = 0;
    }

    // Move-assignable: used to reset the MemTable after flush
    SkipList& operator=(SkipList&& other) noexcept {
        if (this != &other) {
            SkipNode* cur = header_;
            while (cur) { SkipNode* n = cur->forward[0]; delete cur; cur = n; }
            header_ = other.header_;
            level_  = other.level_;
            size_   = other.size_;
            rng_    = std::move(other.rng_);
            other.header_ = new SkipNode("", "", SKIPLIST_MAX_LEVEL);
            other.level_  = 1;
            other.size_   = 0;
        }
        return *this;
    }

    // Insert or update a key-value pair
    void put(const std::string& key, const std::string& value,
             bool tombstone = false) {
        // update[i] = rightmost node at level i with key < target
        std::array<SkipNode*, SKIPLIST_MAX_LEVEL> update;
        SkipNode* cur = header_;

        for (int i = level_ - 1; i >= 0; --i) {
            while (cur->forward[i] && cur->forward[i]->key < key) {
                cur = cur->forward[i];
            }
            update[i] = cur;
        }

        // Check if key already exists at level 0
        SkipNode* next = cur->forward[0];
        if (next && next->key == key) {
            // Overwrite in-place
            next->value     = value;
            next->tombstone = tombstone;
            return;
        }

        // New node: generate a random level
        int new_level = random_level();
        if (new_level > level_) {
            // Extend update array for new levels
            for (int i = level_; i < new_level; ++i) {
                update[i] = header_;
            }
            level_ = new_level;
        }

        SkipNode* node = new SkipNode(key, value, new_level, tombstone);
        for (int i = 0; i < new_level; ++i) {
            node->forward[i]    = update[i]->forward[i];
            update[i]->forward[i] = node;
        }
        ++size_;
    }

    // Point lookup. Returns nullopt if not found (or if tombstone).
    // If found_tombstone is non-null, sets it to true if a tombstone was found.
    std::optional<std::string> get(const std::string& key,
                                   bool* found_tombstone = nullptr) const {
        SkipNode* cur = header_;
        for (int i = level_ - 1; i >= 0; --i) {
            while (cur->forward[i] && cur->forward[i]->key < key) {
                cur = cur->forward[i];
            }
        }
        SkipNode* candidate = cur->forward[0];
        if (!candidate || candidate->key != key) return std::nullopt;

        if (candidate->tombstone) {
            if (found_tombstone) *found_tombstone = true;
            return std::nullopt;
        }
        return candidate->value;
    }

    // Insert a tombstone (logical delete)
    void remove(const std::string& key) {
        put(key, "", /*tombstone=*/true);
    }

    // Scan all entries in sorted order (including tombstones)
    // Used during flush to build an SSTable
    std::vector<std::pair<std::string, std::string>> scan_all(
        std::vector<bool>* tombstones = nullptr) const
    {
        std::vector<std::pair<std::string, std::string>> result;
        if (tombstones) tombstones->clear();

        SkipNode* cur = header_->forward[0];
        while (cur) {
            result.emplace_back(cur->key, cur->value);
            if (tombstones) tombstones->push_back(cur->tombstone);
            cur = cur->forward[0];
        }
        return result;
    }

    size_t size()        const { return size_; }
    bool   empty()       const { return size_ == 0; }
    int    current_level() const { return level_; }

    // Approximate memory usage in bytes
    size_t memory_bytes() const {
        // Each node: ~(key.size() + value.size() + 16 pointers * 8 bytes)
        // This is a rough estimate; SkipList height varies
        size_t total = 0;
        SkipNode* cur = header_->forward[0];
        while (cur) {
            total += cur->key.size() + cur->value.size()
                   + SKIPLIST_MAX_LEVEL * sizeof(SkipNode*) + sizeof(SkipNode);
            cur = cur->forward[0];
        }
        return total;
    }

private:
    int random_level() {
        // Each level is promoted with probability SKIPLIST_P
        // P(level >= k) = P^{k-1}
        // This gives expected height O(log_{1/P} n)
        int lvl = 1;
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        while (dist(rng_) < SKIPLIST_P && lvl < SKIPLIST_MAX_LEVEL) {
            ++lvl;
        }
        return lvl;
    }

    SkipNode* header_;
    int       level_;
    size_t    size_;
    std::mt19937 rng_;
};


// =============================================================================
// SSTable — Sorted String Table
// =============================================================================
//
// An SSTable is an immutable, sorted sequence of key-value pairs written to
// "disk" (here simulated in memory).  Once written, it is never modified —
// only read or eventually merged (compacted) into a new SSTable.
//
// REAL SSTABLE FORMAT (e.g. LevelDB/RocksDB .sst file)
// -------------------------------------------------------
//  [ Data Blocks       ] — key-value pairs in sorted order, 4 KB blocks
//  [ Index Block       ] — one entry per data block: (last key, block offset)
//  [ Bloom Filter Block] — serialised bloom filter bits
//  [ Metaindex Block   ] — maps meta-block names to offsets
//  [ Footer            ] — magic number, pointers to index and metaindex
//
// SPARSE INDEX
// ------------
// Storing one index entry per key wastes space and memory. Instead, store one
// entry per data block (every ~4 KB). To find a key:
//   1. Binary search the sparse index to find which block might contain the key
//   2. Load and scan that single 4 KB block
// This gives O(log(n/B)) index entries and one block read — very efficient.
//
// BLOOM FILTER
// ------------
// Before reading any data blocks, check the per-SSTable Bloom filter.
// If it says "not present" → skip this SSTable entirely.  With 10 bits/key
// and ~99% accuracy, only 1% of irrelevant SSTable reads happen.
//
// In our simulation we store entries in a sorted vector.
// =============================================================================

struct SSTableEntry {
    std::string key;
    std::string value;
    bool tombstone;
};

class SSTable {
public:
    // Build an SSTable from a sorted list of entries
    explicit SSTable(std::vector<SSTableEntry> entries, uint64_t seq_number = 0)
        : entries_(std::move(entries))
        , seq_number_(seq_number)
        , bloom_(entries_.size(), 10)  // 10 bits per key
    {
        // Build bloom filter and sparse index
        for (size_t i = 0; i < entries_.size(); ++i) {
            bloom_.insert(entries_[i].key);

            // Sparse index: every SPARSE_INDEX_INTERVAL entries, record the key
            // In a real implementation this maps to a disk block offset
            if (i % SPARSE_INDEX_INTERVAL == 0) {
                sparse_index_.emplace_back(entries_[i].key, i);
            }
        }
    }

    // Point lookup. Returns nullopt if not found.
    // found_tombstone is set if a delete marker was encountered.
    std::optional<std::string> get(const std::string& key,
                                   bool* found_tombstone = nullptr) const {
        // Step 1: Bloom filter check — O(k) time, avoids disk read on miss
        if (!bloom_.possibly_contains(key)) {
            return std::nullopt;  // definite miss
        }

        // Step 2: Sparse index binary search to find the starting position
        size_t start = sparse_index_lower_bound(key);

        // Step 3: Linear scan from the block start to end of block
        // We scan up to 2*SPARSE_INDEX_INTERVAL entries to cover the block safely.
        // The early-exit on key > target makes this O(block_size) in practice.
        size_t end = std::min(start + 2 * SPARSE_INDEX_INTERVAL, entries_.size());
        for (size_t i = start; i < end; ++i) {
            if (entries_[i].key == key) {
                if (entries_[i].tombstone) {
                    if (found_tombstone) *found_tombstone = true;
                    return std::nullopt;
                }
                return entries_[i].value;
            }
            if (entries_[i].key > key) break;  // sorted — can stop early
        }
        return std::nullopt;
    }

    // Range scan [start_key, end_key) — useful for compaction and range queries
    std::vector<SSTableEntry> range_scan(const std::string& start_key,
                                         const std::string& end_key) const {
        std::vector<SSTableEntry> result;
        size_t start = sparse_index_lower_bound(start_key);
        for (size_t i = start; i < entries_.size(); ++i) {
            if (entries_[i].key >= end_key) break;
            if (entries_[i].key >= start_key) {
                result.push_back(entries_[i]);
            }
        }
        return result;
    }

    const std::vector<SSTableEntry>& entries() const { return entries_; }
    size_t  size()       const { return entries_.size(); }
    bool    empty()      const { return entries_.empty(); }
    uint64_t seq_number() const { return seq_number_; }

    // The key range this SSTable covers
    const std::string& min_key() const {
        static const std::string empty;
        return entries_.empty() ? empty : entries_.front().key;
    }
    const std::string& max_key() const {
        static const std::string empty;
        return entries_.empty() ? empty : entries_.back().key;
    }

    double bloom_fpr() const { return bloom_.false_positive_rate(entries_.size()); }

private:
    // Find the position in entries_ to start scanning for 'key'
    // using the sparse index.
    size_t sparse_index_lower_bound(const std::string& key) const {
        if (sparse_index_.empty()) return 0;

        // Binary search: find the last sparse index entry whose key <= search key.
        // sparse_index_[i].first is the key of the entry at position sparse_index_[i].second.
        // We want the largest i such that sparse_index_[i].first <= key.
        int lo = 0, hi = static_cast<int>(sparse_index_.size()) - 1;

        // If key < first sparse index entry, start from position 0
        if (key < sparse_index_[0].first) return 0;

        while (lo < hi) {
            int mid = (lo + hi + 1) / 2;
            if (sparse_index_[mid].first <= key) lo = mid;
            else                                   hi = mid - 1;
        }
        // sparse_index_[lo].second is the array index of the block start.
        // The target key is in [sparse_index_[lo], sparse_index_[lo+1]) range.
        // Return the block start — caller scans forward up to next index entry.
        return sparse_index_[lo].second;
    }

    static constexpr size_t SPARSE_INDEX_INTERVAL = 16;

    std::vector<SSTableEntry>            entries_;
    uint64_t                             seq_number_;
    BloomFilter                          bloom_;
    std::vector<std::pair<std::string, size_t>> sparse_index_;
};


// =============================================================================
// LSMTree — the full structure
// =============================================================================
//
// COMPACTION STRATEGIES
// ----------------------
// The core tension: the more levels we have, the more write amplification
// during compaction but the less space amplification and better read perf.
//
// 1. SIZE-TIERED COMPACTION (Cassandra default)
//    When N SSTables of similar size accumulate at a level, merge them all
//    into one larger SSTable at the next level.
//    Pro: few compaction operations → low write amplification
//    Con: high space amplification (up to 2x at peak) because you might have
//         K copies of the same key across K SSTables at the same level
//    WA ≈ O(log_{size_ratio} total_data) per level crossing
//
// 2. LEVELED COMPACTION (RocksDB default, LevelDB)
//    Each level has a fixed size budget. When L_i exceeds its budget, one
//    SSTable is picked and merged with overlapping SSTables in L_{i+1}.
//    Pro: low space amplification (at most 1 copy per key at any level > 0)
//    Con: higher write amplification — each byte crosses each level boundary
//    WA = sum_{i=1}^{L} (size_ratio) ≈ size_ratio * L
//    For RocksDB defaults (size_ratio=10, L=7): WA ≈ 10-30x
//
// 3. TIERED + LEVELED (Universal compaction in RocksDB)
//    A hybrid: use tiered at lower levels (absorb writes cheaply), leveled
//    at higher levels (maintain read performance).
//
// WE IMPLEMENT: simplified tiered compaction.
//   When L0 accumulates >= L0_COMPACTION_TRIGGER SSTables, merge them all
//   into one SSTable and push to L1. Same for L1 → L2.
//
// WRITE AMPLIFICATION ANALYSIS
// -----------------------------
// Each byte written once to MemTable, then written once per level during
// compaction. With L levels and size_ratio R:
//
//   WA = 1 (MemTable flush) + R (L0→L1) + R (L1→L2) + ... = 1 + R*L
//
// For leveled: WA ≈ R * L (each level rewrites all data)
// For tiered:  WA ≈ L     (each level consolidates but doesn't rewrite everything)
//
// READ AMPLIFICATION ANALYSIS
// ----------------------------
// Without Bloom filters:
//   RA = |L0_files| + 1 (L1) + 1 (L2) + ... = |L0_files| + L
//   (L0 files may overlap; L1+ files don't with leveled compaction)
//
// With Bloom filters (FPR = p):
//   RA ≈ 1 + p * |L0_files| + p * L
//   ≈ O(1) expected for random point queries (p ≈ 0.01 with 10 bits/key)
// =============================================================================

class LSMTree {
public:
    // Configuration
    struct Config {
        size_t   memtable_size_limit     = 1024;   // flush when MemTable has this many entries
        size_t   l0_compaction_trigger   = 4;       // compact L0 when this many SSTables exist
        size_t   l1_compaction_trigger   = 4;       // compact L1 when this many SSTables exist
        size_t   bloom_bits_per_key      = 10;      // Bloom filter bits per key
        bool     use_bloom_filters       = true;    // can disable for testing
    };

    // Amplification metrics (tracked per operation)
    struct Metrics {
        uint64_t total_writes         = 0;   // logical writes by the user
        uint64_t bytes_written_disk   = 0;   // actual bytes written (SSTable flushes + compactions)
        uint64_t compaction_count     = 0;   // number of compaction operations
        uint64_t bloom_filter_hits    = 0;   // times Bloom filter said "not present" (true neg)
        uint64_t sstable_reads        = 0;   // number of SSTable files actually scanned in reads

        double write_amplification() const {
            if (total_writes == 0) return 0.0;
            return static_cast<double>(bytes_written_disk) / total_writes;
        }
    };

    explicit LSMTree() : cfg_(Config{}), seq_counter_(0) {}
    explicit LSMTree(Config cfg) : cfg_(std::move(cfg)), seq_counter_(0) {}

    // -------------------------------------------------------------------------
    // PUT: Write a key-value pair
    // -------------------------------------------------------------------------
    // The write path is the same regardless of whether the key already exists:
    //   1. Append to WAL (not simulated here, but essential in real systems)
    //   2. Insert into MemTable
    //   3. If MemTable is full, flush to L0 SSTable
    //   4. If L0 has too many SSTables, compact
    //
    // WHY SO FAST: All disk I/O is sequential. The WAL is append-only. The
    // SSTable flush is a single sequential write. Compare to a B+ tree where
    // every insert potentially modifies a random page and propagates splits
    // up the tree — multiple random I/Os per write.
    // -------------------------------------------------------------------------
    void put(const std::string& key, const std::string& value) {
        ++metrics_.total_writes;
        memtable_.put(key, value);
        maybe_flush();
    }

    // -------------------------------------------------------------------------
    // REMOVE: Delete a key via tombstone
    // -------------------------------------------------------------------------
    // LSM trees use lazy deletion: instead of finding and removing the key
    // from whatever level it lives in (expensive!), we INSERT a special
    // "tombstone" marker. During reads, a tombstone means "this key is deleted."
    // During compaction, tombstones eliminate older versions of the key.
    //
    // The tombstone is eventually garbage-collected during compaction once we
    // know there are no older copies of the key at deeper levels.
    // -------------------------------------------------------------------------
    void remove(const std::string& key) {
        ++metrics_.total_writes;
        memtable_.remove(key);
        maybe_flush();
    }

    // -------------------------------------------------------------------------
    // GET: Point lookup
    // -------------------------------------------------------------------------
    // Read path (newest to oldest):
    //   1. MemTable (in-memory SkipList) — O(log n)
    //   2. Immutable MemTable being flushed (if any)
    //   3. L0 SSTables (may overlap — check ALL of them, newest first)
    //   4. L1 SSTables (non-overlapping in leveled; binary search for correct file)
    //   5. L2 SSTables ... and so on
    //
    // Bloom filters: before scanning any SSTable, check its Bloom filter.
    // If the filter says "definitely not here", skip it entirely.
    // With 1% FPR and 10 levels: expected scans ≈ 1 + 0.01 * 10 = 1.1 files.
    // -------------------------------------------------------------------------
    std::optional<std::string> get(const std::string& key) {
        // Step 1: Check MemTable
        bool tombstone = false;
        auto result = memtable_.get(key, &tombstone);
        if (tombstone) return std::nullopt;  // deleted in MemTable
        if (result)    return result;

        // Step 2: Check immutable MemTable (being flushed)
        if (immutable_memtable_) {
            result = immutable_memtable_->get(key, &tombstone);
            if (tombstone) return std::nullopt;
            if (result)    return result;
        }

        // Step 3: Check L0 SSTables (newest first — they may overlap in key space)
        if (!levels_.empty()) {
            for (int i = static_cast<int>(levels_[0].size()) - 1; i >= 0; --i) {
                ++metrics_.sstable_reads;
                result = levels_[0][i]->get(key, &tombstone);
                if (tombstone) return std::nullopt;
                if (result)    return result;
            }
        }

        // Step 4+: Check L1, L2, ... (non-overlapping in leveled compaction)
        for (size_t level = 1; level < levels_.size(); ++level) {
            for (int i = static_cast<int>(levels_[level].size()) - 1; i >= 0; --i) {
                ++metrics_.sstable_reads;

                // Bloom filter check (avoids reading the SSTable data entirely)
                if (cfg_.use_bloom_filters &&
                    !levels_[level][i]->get(key, nullptr).has_value()) {
                    // We need to check bloom separately; let's do it via get()
                    // which internally checks the bloom filter first
                }
                result = levels_[level][i]->get(key, &tombstone);
                if (tombstone) return std::nullopt;
                if (result)    return result;
            }
        }

        return std::nullopt;  // not found anywhere
    }

    // -------------------------------------------------------------------------
    // FLUSH: MemTable → L0 SSTable
    // -------------------------------------------------------------------------
    // Sorted scan of SkipList + write one immutable SSTable.
    // This is the only way data leaves memory and hits "disk".
    // Real systems: LevelDB writes a .ldb file, RocksDB writes a .sst file.
    // -------------------------------------------------------------------------
    void flush() {
        if (memtable_.empty()) return;

        std::vector<bool> tombstones;
        auto entries = memtable_.scan_all(&tombstones);

        std::vector<SSTableEntry> sst_entries;
        sst_entries.reserve(entries.size());
        for (size_t i = 0; i < entries.size(); ++i) {
            sst_entries.push_back({entries[i].first, entries[i].second, tombstones[i]});
            // Track bytes written to "disk"
            metrics_.bytes_written_disk += entries[i].first.size() + entries[i].second.size();
        }

        ensure_level(0);
        levels_[0].push_back(
            std::make_unique<SSTable>(std::move(sst_entries), ++seq_counter_)
        );

        // Reset the MemTable using move-assignment (SkipList is not copy-assignable)
        memtable_ = SkipList();

        // Trigger L0 compaction if needed
        maybe_compact_level(0);
    }

    // -------------------------------------------------------------------------
    // COMPACT: Merge SSTables to bound read amplification and space usage
    // -------------------------------------------------------------------------
    // Full compaction across all levels (for testing/explicit call).
    // -------------------------------------------------------------------------
    void compact() {
        for (size_t level = 0; level < levels_.size(); ++level) {
            maybe_compact_level(level);
        }
    }

    // Force compaction of a specific level
    void compact_level(size_t level) {
        if (level < levels_.size()) {
            do_compact_level(level);
        }
    }

    const Metrics& metrics() const { return metrics_; }

    // Statistics
    size_t memtable_size()    const { return memtable_.size(); }
    size_t num_levels()       const { return levels_.size(); }
    size_t num_sstables(size_t level) const {
        if (level >= levels_.size()) return 0;
        return levels_[level].size();
    }
    size_t total_sstables() const {
        size_t total = 0;
        for (auto& lvl : levels_) total += lvl.size();
        return total;
    }

    // Total entries across all SSTables (including duplicates / tombstones)
    size_t total_sstable_entries() const {
        size_t total = 0;
        for (auto& lvl : levels_) {
            for (auto& sst : lvl) total += sst->size();
        }
        return total;
    }

    // Space amplification: ratio of actual stored entries to logical entries
    // Real LSM trees track this in bytes; we track in entry count.
    double space_amplification() const {
        if (memtable_.size() == 0 && total_sstable_entries() == 0) return 1.0;
        double stored = static_cast<double>(total_sstable_entries() + memtable_.size());
        return stored / std::max(stored, static_cast<double>(metrics_.total_writes));
    }

private:
    void maybe_flush() {
        if (memtable_.size() >= cfg_.memtable_size_limit) {
            flush();
        }
    }

    void maybe_compact_level(size_t level) {
        size_t trigger = (level == 0) ? cfg_.l0_compaction_trigger
                                       : cfg_.l1_compaction_trigger;
        ensure_level(level);
        if (levels_[level].size() >= trigger) {
            do_compact_level(level);
        }
    }

    // -------------------------------------------------------------------------
    // COMPACTION ALGORITHM (Tiered)
    // -------------------------------------------------------------------------
    // Merge all SSTables at 'level' into one SSTable at 'level+1'.
    //
    // The merge is an N-way merge sort (like the merge step in merge sort):
    //   - Each SSTable is already sorted
    //   - We do a simultaneous sorted scan of all SSTables
    //   - When multiple SSTables have the same key, the newest one wins
    //   - Tombstones are eliminated if we're at the last level
    //     (no older copies can exist below the last level)
    //
    // This is essentially the same as merging sorted runs in external merge sort.
    // The analogy is exact: LSM compaction IS external merge sort, run continuously.
    //
    // WRITE AMPLIFICATION from compaction:
    //   Each byte at level L is written once to L and once to L+1.
    //   With L levels and size_ratio R (each level is R× larger than the previous):
    //     Total bytes written = original_size × (1 + R + R + R + ...) for L levels
    //   For leveled compaction: WA = R × L
    //   For tiered (our impl): WA ≈ L (much lower!)
    // -------------------------------------------------------------------------
    void do_compact_level(size_t level) {
        if (level >= levels_.size() || levels_[level].empty()) return;

        ++metrics_.compaction_count;
        ensure_level(level + 1);

        // Collect all entries from SSTables at this level, plus the target level
        // (for leveled compaction we'd be more selective, but for tiered we merge all)
        std::vector<SSTableEntry> all_entries;

        // Also pull in existing L_{level+1} SSTables (merge with them)
        for (auto& sst : levels_[level + 1]) {
            for (auto& e : sst->entries()) {
                all_entries.push_back(e);
                metrics_.bytes_written_disk += e.key.size() + e.value.size();
            }
        }
        for (auto& sst : levels_[level]) {
            for (auto& e : sst->entries()) {
                all_entries.push_back(e);
                metrics_.bytes_written_disk += e.key.size() + e.value.size();
            }
        }

        // Sort by key, then by SSTable sequence number descending
        // (higher seq_number = more recent, should win)
        // We need to track which SSTable each entry came from for versioning.
        // For simplicity: entries from level[level] (newer) come last in all_entries,
        // so after stable_sort they'll dominate in the dedup step.

        // Stable sort by key; newer SSTables were appended last
        std::stable_sort(all_entries.begin(), all_entries.end(),
            [](const SSTableEntry& a, const SSTableEntry& b) {
                return a.key < b.key;
            });

        // Dedup: for each key, keep only the most recent entry
        // (last occurrence after stable_sort, since newer SSTables are at the end)
        std::vector<SSTableEntry> merged;
        bool is_last_level = (level + 1 == levels_.size() - 1);

        for (size_t i = 0; i < all_entries.size(); ) {
            // Find the range of entries with this key
            size_t j = i;
            while (j < all_entries.size() && all_entries[j].key == all_entries[i].key) {
                ++j;
            }
            // The last entry in [i, j) is the most recent (stable_sort preserves order)
            SSTableEntry& newest = all_entries[j - 1];

            // Drop tombstones at the last level (no older versions can exist)
            if (newest.tombstone && is_last_level) {
                // Tombstone has done its job — garbage collect
            } else {
                merged.push_back(newest);
            }
            i = j;
        }

        // Replace level and level+1 SSTables with the merged result
        levels_[level].clear();
        levels_[level + 1].clear();

        if (!merged.empty()) {
            levels_[level + 1].push_back(
                std::make_unique<SSTable>(std::move(merged), ++seq_counter_)
            );
        }
    }

    void ensure_level(size_t level) {
        while (levels_.size() <= level) {
            levels_.emplace_back();
        }
    }

    Config cfg_;
    SkipList memtable_;
    std::unique_ptr<SkipList> immutable_memtable_;  // being flushed (not yet used — extension point)

    // levels_[0] = L0 (may have overlapping key ranges)
    // levels_[1] = L1 (non-overlapping in leveled compaction)
    // ...
    std::vector<std::vector<std::unique_ptr<SSTable>>> levels_;

    uint64_t seq_counter_;  // monotonically increasing SSTable sequence number
    Metrics  metrics_;
};

}  // namespace lsm
