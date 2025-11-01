#include "../btree/bplus_tree.hpp"
#include <iostream>
#include <memory>
#include <chrono>

// Simple memory tracker to demonstrate memory usage
class MemoryTracker {
public:
    static size_t allocated_bytes;
    static size_t allocation_count;
    
    static void* allocate(size_t size) {
        allocated_bytes += size;
        allocation_count++;
        return std::malloc(size);
    }
    
    static void deallocate(void* ptr, size_t size) {
        allocated_bytes -= size;
        allocation_count--;
        std::free(ptr);
    }
    
    static void print_stats() {
        std::cout << "Memory allocated: " << allocated_bytes << " bytes\n";
        std::cout << "Active allocations: " << allocation_count << "\n";
    }
};

size_t MemoryTracker::allocated_bytes = 0;
size_t MemoryTracker::allocation_count = 0;

void demonstrate_memory_management() {
    std::cout << "=== B+ Tree Memory Management Demo ===\n\n";
    
    // Create a B+ Tree
    std::cout << "1. Creating B+ Tree...\n";
    {
        btree::BPlusTree<int, std::string> tree(8);  // Small branching factor
        
        std::cout << "2. Inserting data...\n";
        // Insert some data
        for (int i = 1; i <= 20; ++i) {
            tree.insert(i, "value_" + std::to_string(i));
        }
        
        std::cout << "3. Tree structure:\n";
        tree.print_tree();
        
        std::cout << "\n4. Memory characteristics:\n";
        std::cout << "   - All nodes managed by shared_ptr\n";
        std::cout << "   - Automatic cleanup when tree goes out of scope\n";
        std::cout << "   - No manual memory management needed\n";
        
        std::cout << "\n5. Demonstrating range query (cache-friendly):\n";
        auto range_result = tree.range_query(5, 15);
        std::cout << "   Range [5, 15]: ";
        for (const auto& [key, value] : range_result) {
            std::cout << key << " ";
        }
        std::cout << "\n";
        
        std::cout << "\n6. Tree is still alive here...\n";
    } // ← Tree destructor called here
    
    std::cout << "7. Tree destroyed - all memory automatically cleaned up!\n";
}

void demonstrate_memory_efficiency() {
    std::cout << "\n=== Memory Efficiency Demo ===\n\n";
    
    // Compare different branching factors
    auto test_branching_factor = [](size_t bf, const std::string& name) {
        std::cout << name << " (branching factor " << bf << "):\n";
        
        auto start = std::chrono::high_resolution_clock::now();
        
        btree::BPlusTree<int, int> tree(bf);
        
        // Insert 1000 elements
        for (int i = 0; i < 1000; ++i) {
            tree.insert(i, i * 2);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "  - Insertion time: " << duration.count() << " microseconds\n";
        
        // Test search performance
        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 100; ++i) {
            tree.search(i * 10);
        }
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "  - Search time (100 searches): " << duration.count() << " microseconds\n";
        std::cout << "  - Tree empty: " << (tree.empty() ? "yes" : "no") << "\n\n";
    };
    
    test_branching_factor(4, "Small tree");
    test_branching_factor(16, "Medium tree");
    test_branching_factor(64, "Large tree");
}

void demonstrate_smart_pointer_behavior() {
    std::cout << "=== Smart Pointer Behavior Demo ===\n\n";
    
    std::cout << "1. Creating tree and getting reference count info...\n";
    
    btree::BPlusTree<int, std::string> tree(4);
    
    // Insert data to create nodes
    for (int i = 1; i <= 10; ++i) {
        tree.insert(i, "value_" + std::to_string(i));
    }
    
    std::cout << "2. Tree structure with shared_ptr managed nodes:\n";
    tree.print_tree();
    
    std::cout << "\n3. Key benefits of shared_ptr management:\n";
    std::cout << "   ✅ Automatic cleanup - no memory leaks\n";
    std::cout << "   ✅ Exception safety - cleanup even if exceptions occur\n";
    std::cout << "   ✅ Shared ownership - nodes can be safely referenced\n";
    std::cout << "   ✅ RAII - Resource Acquisition Is Initialization\n";
    
    std::cout << "\n4. Memory is automatically freed when tree is destroyed\n";
}

int main() {
    try {
        demonstrate_memory_management();
        demonstrate_memory_efficiency();
        demonstrate_smart_pointer_behavior();
        
        std::cout << "\n=== Summary ===\n";
        std::cout << "Current B+ Tree implementation:\n";
        std::cout << "• In-memory only (RAM storage)\n";
        std::cout << "• Smart pointer managed (automatic cleanup)\n";
        std::cout << "• Cache-friendly leaf node layout\n";
        std::cout << "• No persistence (data lost when program ends)\n";
        std::cout << "• Ready for future persistence enhancements\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}