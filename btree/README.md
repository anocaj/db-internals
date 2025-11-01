# B+ Tree Implementation

A high-performance, template-based B+ Tree implementation in modern C++17.

## Overview

This B+ Tree implementation provides:
- **Logarithmic time complexity** for insert, delete, and search operations
- **Efficient range queries** through linked leaf nodes
- **Template-based design** supporting any comparable key type and any value type
- **Memory-efficient** structure with configurable branching factor
- **Iterator support** for range scanning
- **Exception safety** and modern C++ best practices

## File Structure

```
btree/
├── bplus_tree.hpp          # Main B+ Tree class interface
├── bplus_tree_impl.hpp     # B+ Tree implementation
├── node.hpp                # Node base class and derived classes
├── node_impl.hpp           # Node implementations
├── iterator.hpp            # Iterator class interface
├── iterator_impl.hpp       # Iterator implementation
├── CMakeLists.txt          # Build configuration
└── README.md               # This file
```

## Usage

### Basic Operations

```cpp
#include "btree/bplus_tree.hpp"

// Create a B+ Tree with default branching factor (64)
btree::BPlusTree<int, std::string> tree;

// Insert key-value pairs
tree.insert(10, "ten");
tree.insert(5, "five");
tree.insert(15, "fifteen");

// Search for values
auto result = tree.search(10);
if (result) {
    std::cout << "Found: " << *result << std::endl;
}

// Remove keys
bool removed = tree.remove(5);

// Check if tree is empty
if (tree.empty()) {
    std::cout << "Tree is empty" << std::endl;
}
```

### Range Queries

```cpp
// Range query returning vector of pairs
auto range_result = tree.range_query(5, 15);
for (const auto& [key, value] : range_result) {
    std::cout << key << ": " << value << std::endl;
}

// Iterator-based range scanning
for (auto it = tree.range_begin(5, 15); it != tree.range_end(); ++it) {
    auto [key, value] = *it;
    std::cout << key << ": " << value << std::endl;
}
```

### Custom Types

```cpp
// Using custom key and value types
btree::BPlusTree<std::string, Person> people_tree;

// Custom branching factor for memory optimization
btree::BPlusTree<int, Data> small_tree(16);  // Branching factor of 16
```

## Design Principles

### 1. **Separation of Concerns**
- **Node classes** (`node.hpp`) handle tree structure and node operations
- **Iterator class** (`iterator.hpp`) provides range scanning functionality
- **Main tree class** (`bplus_tree.hpp`) orchestrates high-level operations

### 2. **Template-Based Design**
- Supports any comparable key type (`KeyType`)
- Supports any value type (`ValueType`)
- Compile-time optimization through templates

### 3. **Memory Management**
- Uses `std::shared_ptr` for automatic memory management
- RAII principles ensure exception safety
- No manual memory allocation/deallocation

### 4. **Modern C++ Features**
- `std::optional` for safe value returns
- Range-based for loops support through iterators
- Move semantics for efficient operations
- `constexpr` where applicable

## Performance Characteristics

| Operation | Time Complexity | Space Complexity |
|-----------|----------------|------------------|
| Insert    | O(log n)       | O(1)            |
| Delete    | O(log n)       | O(1)            |
| Search    | O(log n)       | O(1)            |
| Range Query | O(log n + k) | O(k)            |

Where:
- `n` = number of elements in the tree
- `k` = number of elements in the range query result

## Configuration

### Branching Factor
The branching factor determines the maximum number of children per internal node:
- **Higher values** (64-256): Better for large datasets, fewer levels
- **Lower values** (4-16): Better for small datasets, more cache-friendly
- **Minimum value**: 3 (enforced automatically)

```cpp
// High branching factor for large datasets
btree::BPlusTree<int, std::string> large_tree(128);

// Low branching factor for small datasets
btree::BPlusTree<int, std::string> small_tree(8);
```

## Thread Safety

This implementation is **not thread-safe**. For concurrent access:
- Use external synchronization (mutexes, read-write locks)
- Consider lock-free alternatives for high-performance scenarios
- Multiple readers are safe if no writers are present

## Limitations

1. **Deletion**: Simplified deletion without node merging/borrowing
2. **Persistence**: In-memory only (no disk persistence)
3. **Bulk Operations**: No bulk insert/delete optimizations
4. **Compression**: No key/value compression

## Testing

Comprehensive unit tests are available in `tests/unit/btree_test.cpp`:

```bash
# Build and run tests
mkdir build && cd build
cmake ..
make btree_test
./tests/unit/btree_test
```

## Future Enhancements

- [ ] Complete deletion with node merging and borrowing
- [ ] Disk-based persistence with page management
- [ ] Bulk loading optimizations
- [ ] Key compression for string keys
- [ ] Lock-free concurrent operations
- [ ] Memory pool allocation for better performance