#pragma once

#include <vector>
#include <list>
#include <optional>
#include <functional>
#include <stdexcept>
#include <cstdint>
#include <string>

namespace hash_index {

// ============================================================================
// FNV-1a Hash Function
// ============================================================================
// FNV-1a (Fowler–Noll–Vo) is a fast, non-cryptographic hash function.
// The key insight: XOR then multiply (vs FNV-1 which multiplies then XORs).
// XOR-first gives better avalanche effect for small differences in input.
//
// Why FNV-1a over std::hash?
//   - Deterministic across platforms and compilers
//   - Simple to understand and verify
//   - Good distribution for typical key types
// ============================================================================

inline uint64_t fnv1a(const void* data, size_t len) {
    static constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    static constexpr uint64_t FNV_PRIME        = 1099511628211ULL;

    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint64_t hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < len; ++i) {
        hash ^= bytes[i];   // XOR with byte
        hash *= FNV_PRIME;  // Multiply by prime to spread bits
    }
    return hash;
}

// Generic hasher — falls back to std::hash for unknown types
template<typename K>
struct Hasher {
    uint64_t operator()(const K& key) const {
        return std::hash<K>{}(key);
    }
};

// Specialization for strings using FNV-1a
template<>
struct Hasher<std::string> {
    uint64_t operator()(const std::string& key) const {
        return fnv1a(key.data(), key.size());
    }
};

// Specialization for integers using FNV-1a
template<>
struct Hasher<int> {
    uint64_t operator()(int key) const {
        return fnv1a(&key, sizeof(key));
    }
};

template<>
struct Hasher<int64_t> {
    uint64_t operator()(int64_t key) const {
        return fnv1a(&key, sizeof(key));
    }
};


// ============================================================================
// ChainingHashIndex — Separate Chaining
// ============================================================================
// The simplest collision resolution strategy: each bucket holds a linked list.
// When two keys hash to the same bucket, they share the list.
//
// Pros:
//   - Simple to implement
//   - Handles high load factors gracefully
//   - Deletion is easy (just remove from list)
//
// Cons:
//   - Pointer chasing hurts cache performance
//   - Memory overhead of linked list nodes
//
// Used by: Java HashMap, PostgreSQL hash joins
// ============================================================================

template<typename KeyType, typename ValueType>
class ChainingHashIndex {
public:
    using key_type   = KeyType;
    using value_type = ValueType;
    using size_type  = std::size_t;

    // Load factor threshold: ratio of elements to buckets
    // When exceeded, we double the bucket count and rehash everything.
    // 0.75 is the classic tradeoff: good memory use vs collision rate.
    static constexpr double MAX_LOAD_FACTOR = 0.75;
    static constexpr size_type INITIAL_CAPACITY = 16;

    explicit ChainingHashIndex(size_type initial_capacity = INITIAL_CAPACITY)
        : buckets_(initial_capacity), size_(0) {}

    // -------------------------------------------------------------------
    // insert: O(1) amortized
    // If key exists, update its value. Otherwise add a new entry.
    // May trigger a rehash if load factor is exceeded.
    // -------------------------------------------------------------------
    void insert(const KeyType& key, const ValueType& value) {
        if (load_factor() >= MAX_LOAD_FACTOR) {
            rehash(buckets_.size() * 2);
        }

        size_type idx = bucket_index(key);
        auto& chain = buckets_[idx];

        // Check if key already exists in this bucket's chain
        for (auto& [k, v] : chain) {
            if (k == key) {
                v = value;  // Update existing
                return;
            }
        }

        chain.emplace_back(key, value);
        ++size_;
    }

    // -------------------------------------------------------------------
    // search: O(1) average, O(n) worst case (all keys in one bucket)
    // Returns nullopt if key not found.
    // -------------------------------------------------------------------
    std::optional<ValueType> search(const KeyType& key) const {
        size_type idx = bucket_index(key);
        for (const auto& [k, v] : buckets_[idx]) {
            if (k == key) return v;
        }
        return std::nullopt;
    }

    // -------------------------------------------------------------------
    // remove: O(1) average
    // Returns true if the key was found and removed.
    // -------------------------------------------------------------------
    bool remove(const KeyType& key) {
        size_type idx = bucket_index(key);
        auto& chain = buckets_[idx];

        for (auto it = chain.begin(); it != chain.end(); ++it) {
            if (it->first == key) {
                chain.erase(it);
                --size_;
                return true;
            }
        }
        return false;
    }

    bool   empty()       const { return size_ == 0; }
    size_type size()     const { return size_; }
    double load_factor() const { return static_cast<double>(size_) / buckets_.size(); }
    size_type bucket_count() const { return buckets_.size(); }

private:
    // Map key → bucket index
    size_type bucket_index(const KeyType& key) const {
        return hasher_(key) % buckets_.size();
    }

    // -------------------------------------------------------------------
    // rehash: O(n) — rebuild the table with more buckets
    // Called when load factor exceeds MAX_LOAD_FACTOR.
    // All existing entries are re-inserted into the new bucket layout.
    // -------------------------------------------------------------------
    void rehash(size_type new_capacity) {
        std::vector<std::list<std::pair<KeyType, ValueType>>> new_buckets(new_capacity);

        for (auto& chain : buckets_) {
            for (auto& [k, v] : chain) {
                size_type new_idx = hasher_(k) % new_capacity;
                new_buckets[new_idx].emplace_back(k, v);
            }
        }

        buckets_ = std::move(new_buckets);
    }

    std::vector<std::list<std::pair<KeyType, ValueType>>> buckets_;
    size_type size_;
    Hasher<KeyType> hasher_;
};


// ============================================================================
// OpenAddressingHashIndex — Linear Probing
// ============================================================================
// Instead of linked lists, all entries live directly in the array.
// On collision: probe forward (index+1, index+2, ...) until empty slot found.
//
// The key insight: keeping data in a flat array is cache-friendly.
// Modern CPUs fetch cache lines (64 bytes). Probing sequential slots
// is much faster than following pointers in a linked list.
//
// Pros:
//   - Better cache performance than chaining
//   - No pointer overhead
//
// Cons:
//   - "Clustering": long runs of occupied slots slow down probes
//   - Deletion is tricky (can't just clear — breaks probe chains)
//   - Must keep load factor low (< 0.7) to avoid performance cliff
//
// Deletion trick: mark slots as "DELETED" (tombstone) rather than empty.
// Probing skips tombstones but insertion can reuse them.
//
// Used by: Python dict, Go map, many open-source hash maps
// ============================================================================

template<typename KeyType, typename ValueType>
class OpenAddressingHashIndex {
public:
    using key_type   = KeyType;
    using value_type = ValueType;
    using size_type  = std::size_t;

    static constexpr double   MAX_LOAD_FACTOR  = 0.70;
    static constexpr size_type INITIAL_CAPACITY = 16;

    explicit OpenAddressingHashIndex(size_type initial_capacity = INITIAL_CAPACITY)
        : slots_(initial_capacity), size_(0), tombstones_(0) {}

    // -------------------------------------------------------------------
    // insert: O(1) amortized
    // Probes linearly until an EMPTY or DELETED slot is found.
    // -------------------------------------------------------------------
    void insert(const KeyType& key, const ValueType& value) {
        if (load_factor() >= MAX_LOAD_FACTOR) {
            rehash(slots_.size() * 2);
        }

        size_type idx = start_index(key);
        size_type tombstone_idx = slots_.size(); // Track first tombstone seen

        for (size_type i = 0; i < slots_.size(); ++i) {
            size_type probe = (idx + i) % slots_.size();
            Slot& slot = slots_[probe];

            if (slot.state == SlotState::OCCUPIED && slot.key == key) {
                slot.value = value;  // Update existing key
                return;
            }
            if (slot.state == SlotState::DELETED && tombstone_idx == slots_.size()) {
                tombstone_idx = probe;  // Remember first tombstone for reuse
            }
            if (slot.state == SlotState::EMPTY) {
                // Use tombstone if we passed one, otherwise use this empty slot
                size_type insert_idx = (tombstone_idx < slots_.size()) ? tombstone_idx : probe;
                if (slots_[insert_idx].state == SlotState::DELETED) --tombstones_;
                slots_[insert_idx] = {SlotState::OCCUPIED, key, value};
                ++size_;
                return;
            }
        }
        // Table is full (shouldn't happen with load factor check)
        throw std::runtime_error("Hash table is full");
    }

    // -------------------------------------------------------------------
    // search: O(1) average
    // Probes until key found, EMPTY slot (key can't exist), or full loop.
    // -------------------------------------------------------------------
    std::optional<ValueType> search(const KeyType& key) const {
        size_type idx = start_index(key);

        for (size_type i = 0; i < slots_.size(); ++i) {
            size_type probe = (idx + i) % slots_.size();
            const Slot& slot = slots_[probe];

            if (slot.state == SlotState::EMPTY)     return std::nullopt; // Key can't be further
            if (slot.state == SlotState::DELETED)   continue;            // Skip tombstones
            if (slot.key == key)                    return slot.value;
        }
        return std::nullopt;
    }

    // -------------------------------------------------------------------
    // remove: O(1) average
    // We can't clear the slot (would break probe chains for other keys).
    // Instead, mark it DELETED — a "tombstone".
    // -------------------------------------------------------------------
    bool remove(const KeyType& key) {
        size_type idx = start_index(key);

        for (size_type i = 0; i < slots_.size(); ++i) {
            size_type probe = (idx + i) % slots_.size();
            Slot& slot = slots_[probe];

            if (slot.state == SlotState::EMPTY)   return false;
            if (slot.state == SlotState::DELETED) continue;
            if (slot.key == key) {
                slot.state = SlotState::DELETED;
                --size_;
                ++tombstones_;
                return true;
            }
        }
        return false;
    }

    bool      empty()        const { return size_ == 0; }
    size_type size()         const { return size_; }
    double    load_factor()  const { return static_cast<double>(size_ + tombstones_) / slots_.size(); }
    size_type slot_count()   const { return slots_.size(); }

private:
    enum class SlotState { EMPTY, OCCUPIED, DELETED };

    struct Slot {
        SlotState state = SlotState::EMPTY;
        KeyType   key{};
        ValueType value{};
    };

    size_type start_index(const KeyType& key) const {
        return hasher_(key) % slots_.size();
    }

    // Rehash: rebuild with new capacity, drop all tombstones
    void rehash(size_type new_capacity) {
        std::vector<Slot> new_slots(new_capacity);
        size_type new_size = 0;

        for (const auto& slot : slots_) {
            if (slot.state != SlotState::OCCUPIED) continue;

            size_type idx = hasher_(slot.key) % new_capacity;
            for (size_type i = 0; i < new_capacity; ++i) {
                size_type probe = (idx + i) % new_capacity;
                if (new_slots[probe].state == SlotState::EMPTY) {
                    new_slots[probe] = {SlotState::OCCUPIED, slot.key, slot.value};
                    ++new_size;
                    break;
                }
            }
        }

        slots_      = std::move(new_slots);
        size_       = new_size;
        tombstones_ = 0;  // Tombstones are gone after rehash
    }

    std::vector<Slot> slots_;
    size_type size_;
    size_type tombstones_;
    Hasher<KeyType> hasher_;
};

} // namespace hash_index
