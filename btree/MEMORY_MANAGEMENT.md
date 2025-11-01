# B+ Tree Memory Management

## Current Implementation: In-Memory Only

Our B+ Tree currently stores all data in **RAM** using smart pointers for automatic memory management. Here's how it works:

## **Memory Architecture**

### **1. Smart Pointer Hierarchy**

```cpp
// All nodes are managed by shared_ptr for automatic cleanup
using NodePtr = std::shared_ptr<BPlusTreeNode<KeyType, ValueType>>;

class BPlusTree {
private:
    NodePtr root_;  // Root node pointer
    size_t branching_factor_;
};
```

**Memory Layout:**
```
BPlusTree Object (Stack/Heap)
├── root_ (shared_ptr) ──────────┐
├── branching_factor_ (size_t)   │
└── ...                          │
                                 ▼
                        Root Node (Heap)
                        ├── keys_ (vector)
                        ├── children_ (vector<shared_ptr>)
                        └── ...
                                 │
                                 ▼
                        Child Nodes (Heap)
                        ├── Leaf Nodes
                        └── Internal Nodes
```

### **2. Automatic Memory Management**

```cpp
// When a BPlusTree is destroyed:
BPlusTree<int, std::string> tree;
tree.insert(1, "one");
tree.insert(2, "two");
// ... tree goes out of scope ...
// ✅ All nodes automatically cleaned up via shared_ptr destructors
```

**Reference Counting:**
- Each node is managed by `std::shared_ptr`
- When last reference is removed, node is automatically deleted
- No manual `delete` calls needed
- Exception-safe cleanup

### **3. Memory Efficiency Features**

```cpp
// Pre-allocation to avoid frequent reallocations
template<typename KeyType, typename ValueType>
BPlusTreeNode<KeyType, ValueType>::BPlusTreeNode(bool is_leaf, size_t max_keys)
    : is_leaf_(is_leaf), max_keys_(max_keys), key_count_(0) {
    keys_.reserve(max_keys);  // ✅ Pre-allocate memory
}

// Leaf nodes also pre-allocate
LeafNode<KeyType, ValueType>::LeafNode(size_t max_keys)
    : BPlusTreeNode<KeyType, ValueType>(true, max_keys), next_(nullptr) {
    values_.reserve(max_keys);  // ✅ Pre-allocate memory
}
```

## **Memory Usage Analysis**

### **Per-Node Memory Overhead**

**Internal Node:**
```cpp
class InternalNode {
    // Inherited from BPlusTreeNode:
    bool is_leaf_;                    // 1 byte
    size_t max_keys_;                // 8 bytes
    size_t key_count_;               // 8 bytes
    std::vector<KeyType> keys_;      // 24 bytes + key storage
    
    // InternalNode specific:
    std::vector<NodePtr> children_;  // 24 bytes + pointer storage
    
    // Total overhead: ~65 bytes + actual data
};
```

**Leaf Node:**
```cpp
class LeafNode {
    // Inherited from BPlusTreeNode:
    bool is_leaf_;                    // 1 byte
    size_t max_keys_;                // 8 bytes
    size_t key_count_;               // 8 bytes
    std::vector<KeyType> keys_;      // 24 bytes + key storage
    
    // LeafNode specific:
    std::vector<ValueType> values_;  // 24 bytes + value storage
    std::shared_ptr<LeafNode> next_; // 16 bytes
    
    // Total overhead: ~81 bytes + actual data
};
```

### **Memory Usage Example**

For a tree with 1000 integer keys and string values:

```cpp
BPlusTree<int, std::string> tree(16);  // Branching factor 16

// Approximate memory usage:
// - 1000 keys × 4 bytes = 4,000 bytes
// - 1000 values × ~20 bytes avg = 20,000 bytes  
// - ~63 leaf nodes × 81 bytes overhead = 5,103 bytes
// - ~4 internal nodes × 65 bytes overhead = 260 bytes
// - Total: ~29,363 bytes (~29 KB)
```

## **Current Limitations**

### **❌ What We DON'T Have (Yet)**

1. **Persistence**: Data is lost when program ends
2. **Memory Mapping**: No virtual memory support
3. **Compression**: No key/value compression
4. **Paging**: No disk-based storage
5. **Memory Pools**: Uses standard allocator

## **Persistence Options (Future Enhancements)**

### **Option 1: Serialization**

```cpp
class BPlusTree {
public:
    // Save tree to file
    bool save_to_file(const std::string& filename) const {
        std::ofstream file(filename, std::ios::binary);
        return serialize_node(file, root_);
    }
    
    // Load tree from file
    bool load_from_file(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        root_ = deserialize_node(file);
        return root_ != nullptr;
    }

private:
    bool serialize_node(std::ofstream& file, NodePtr node) const;
    NodePtr deserialize_node(std::ifstream& file);
};
```

### **Option 2: Memory-Mapped Files**

```cpp
#include <sys/mman.h>  // Unix/Linux
#include <windows.h>   // Windows

class PersistentBPlusTree {
private:
    void* mapped_memory_;
    size_t file_size_;
    int fd_;  // File descriptor
    
public:
    bool open(const std::string& filename, size_t initial_size) {
        // Open file and map to memory
        fd_ = open(filename.c_str(), O_RDWR | O_CREAT, 0644);
        mapped_memory_ = mmap(nullptr, initial_size, 
                             PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        // Use mapped_memory_ as heap for tree nodes
    }
};
```

### **Option 3: Page-Based Storage**

```cpp
class PageManager {
    static constexpr size_t PAGE_SIZE = 4096;  // 4KB pages
    
    struct Page {
        char data[PAGE_SIZE];
        bool dirty;
        size_t page_id;
    };
    
    std::unordered_map<size_t, std::unique_ptr<Page>> page_cache_;
    std::string filename_;
    
public:
    Page* get_page(size_t page_id);
    void flush_page(size_t page_id);
    size_t allocate_page();
};

class PersistentBPlusTree {
    PageManager page_manager_;
    size_t root_page_id_;
    
    // Nodes reference pages instead of using shared_ptr
};
```

## **Memory Optimization Strategies**

### **1. Custom Allocators**

```cpp
// Pool allocator for nodes
template<typename T>
class NodeAllocator {
    static constexpr size_t POOL_SIZE = 1024 * 1024;  // 1MB pool
    char memory_pool_[POOL_SIZE];
    size_t offset_ = 0;
    
public:
    T* allocate(size_t n) {
        if (offset_ + sizeof(T) * n > POOL_SIZE) {
            throw std::bad_alloc();
        }
        T* ptr = reinterpret_cast<T*>(memory_pool_ + offset_);
        offset_ += sizeof(T) * n;
        return ptr;
    }
};

// Usage:
using NodePtr = std::shared_ptr<BPlusTreeNode<KeyType, ValueType>>;
// becomes:
using NodePtr = std::unique_ptr<BPlusTreeNode<KeyType, ValueType>, 
                               NodeDeleter<NodeAllocator>>;
```

### **2. Compression**

```cpp
class CompressedLeafNode : public LeafNode<KeyType, ValueType> {
    std::vector<uint8_t> compressed_data_;
    
    void compress_keys_values() {
        // Use compression algorithm (LZ4, Snappy, etc.)
    }
    
    void decompress_keys_values() {
        // Decompress when accessing data
    }
};
```

### **3. Memory Monitoring**

```cpp
class BPlusTree {
    mutable std::atomic<size_t> memory_usage_{0};
    
public:
    size_t get_memory_usage() const { return memory_usage_.load(); }
    
private:
    void track_allocation(size_t bytes) {
        memory_usage_.fetch_add(bytes);
    }
    
    void track_deallocation(size_t bytes) {
        memory_usage_.fetch_sub(bytes);
    }
};
```

## **Performance Characteristics**

### **Memory Access Patterns**

```cpp
// ✅ Good: Sequential access in leaf nodes
for (auto it = tree.range_begin(start); it != tree.range_end(); ++it) {
    // Accesses linked leaf nodes sequentially
    // Cache-friendly access pattern
}

// ❌ Less optimal: Random access
for (int i = 0; i < 1000; ++i) {
    int random_key = rand() % 10000;
    tree.search(random_key);  // Random tree traversal
}
```

### **Cache Efficiency**

- **Leaf nodes**: Store data contiguously in vectors (cache-friendly)
- **Tree traversal**: Follows pointer chains (less cache-friendly)
- **Range queries**: Excellent cache locality through linked leaves

## **Summary**

**Current State:**
- ✅ **Automatic memory management** with smart pointers
- ✅ **Exception-safe** cleanup
- ✅ **Memory-efficient** node structure
- ✅ **Cache-friendly** leaf node layout

**Future Enhancements:**
- 🔄 **Persistence** through serialization or memory mapping
- 🔄 **Custom allocators** for better performance
- 🔄 **Compression** for reduced memory usage
- 🔄 **Memory monitoring** and limits

The current implementation prioritizes **correctness and simplicity** while providing a solid foundation for future persistence and optimization features!