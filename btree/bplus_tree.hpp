#pragma once

#include "node.hpp"
#include "iterator.hpp"
#include <memory>
#include <optional>
#include <vector>
#include <utility>

namespace btree {

/**
 * @brief B+ Tree implementation for ordered key-value storage
 * 
 * A B+ Tree is a self-balancing tree data structure that maintains sorted data
 * and allows searches, sequential access, insertions, and deletions in logarithmic time.
 * 
 * @tparam KeyType The type of keys stored in the tree (must be comparable)
 * @tparam ValueType The type of values associated with keys
 */
template<typename KeyType, typename ValueType>
class BPlusTree {
public:
    using iterator = BPlusTreeIterator<KeyType, ValueType>;
    using key_type = KeyType;
    using value_type = ValueType;
    using size_type = std::size_t;

    /**
     * @brief Construct a new B+ Tree
     * @param branching_factor Maximum number of children per internal node (minimum 3)
     */
    explicit BPlusTree(size_type branching_factor = 64);

    // Disable copy constructor and assignment (can be implemented if needed)
    BPlusTree(const BPlusTree&) = delete;
    BPlusTree& operator=(const BPlusTree&) = delete;

    // Enable move constructor and assignment
    BPlusTree(BPlusTree&&) = default;
    BPlusTree& operator=(BPlusTree&&) = default;

    /**
     * @brief Insert a key-value pair into the tree
     * @param key The key to insert
     * @param value The value to associate with the key
     * @return true if insertion was successful, false otherwise
     */
    bool insert(const KeyType& key, const ValueType& value);

    /**
     * @brief Remove a key from the tree
     * @param key The key to remove
     * @return true if the key was found and removed, false otherwise
     */
    bool remove(const KeyType& key);

    /**
     * @brief Search for a key in the tree
     * @param key The key to search for
     * @return Optional containing the value if found, nullopt otherwise
     */
    std::optional<ValueType> search(const KeyType& key) const;

    /**
     * @brief Perform a range query
     * @param start_key The start of the range (inclusive)
     * @param end_key The end of the range (inclusive)
     * @return Vector of key-value pairs in the specified range
     */
    std::vector<std::pair<KeyType, ValueType>> range_query(
        const KeyType& start_key, const KeyType& end_key) const;

    /**
     * @brief Get iterator for range scanning starting from a specific key
     * @param start_key The starting key for iteration
     * @return Iterator positioned at the first key >= start_key
     */
    iterator range_begin(const KeyType& start_key) const;

    /**
     * @brief Get iterator for range scanning with both start and end bounds
     * @param start_key The starting key for iteration
     * @param end_key The ending key for iteration
     * @return Iterator positioned at the first key >= start_key, bounded by end_key
     */
    iterator range_begin(const KeyType& start_key, const KeyType& end_key) const;

    /**
     * @brief Get end iterator
     * @return End iterator for range scanning
     */
    iterator range_end() const;

    /**
     * @brief Check if the tree is empty
     * @return true if the tree contains no elements, false otherwise
     */
    bool empty() const;

    /**
     * @brief Get the branching factor of the tree
     * @return The maximum number of children per internal node
     */
    size_type branching_factor() const { return branching_factor_; }

    /**
     * @brief Print the tree structure for debugging
     * @param os Output stream to print to
     */
    void print_tree(std::ostream& os = std::cout) const;

private:
    using NodePtr = std::shared_ptr<BPlusTreeNode<KeyType, ValueType>>;
    using SplitResult = std::pair<KeyType, NodePtr>;

    /**
     * @brief Helper function for insertion
     * @param node The node to insert into
     * @param key The key to insert
     * @param value The value to insert
     * @return Pair of promoted key and new node if split occurred, otherwise {key, nullptr}
     */
    SplitResult insert_helper(NodePtr node, const KeyType& key, const ValueType& value);

    /**
     * @brief Helper function for deletion
     * @param node The node to delete from
     * @param key The key to delete
     * @return true if the key was found and removed, false otherwise
     */
    bool remove_helper(NodePtr node, const KeyType& key);

    /**
     * @brief Find the leaf node that should contain the given key
     * @param key The key to search for
     * @return Shared pointer to the leaf node, or nullptr if tree is empty
     */
    std::shared_ptr<LeafNode<KeyType, ValueType>> find_leaf(const KeyType& key) const;

    size_type branching_factor_;
    NodePtr root_;
};

} // namespace btree

#include "bplus_tree_impl.hpp"