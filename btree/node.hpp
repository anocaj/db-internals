#pragma once

#include <vector>
#include <memory>
#include <optional>
#include <iostream>

namespace btree {

// Forward declarations
template<typename KeyType, typename ValueType>
class BPlusTree;

template<typename KeyType, typename ValueType>
class BPlusTreeIterator;

/**
 * @brief Abstract base class for B+ Tree nodes
 * 
 * This class provides the common interface and functionality for both
 * internal nodes and leaf nodes in the B+ Tree.
 */
template<typename KeyType, typename ValueType>
class BPlusTreeNode {
public:
    using size_type = std::size_t;

    /**
     * @brief Construct a new B+ Tree Node
     * @param is_leaf Whether this node is a leaf node
     * @param max_keys Maximum number of keys this node can hold
     */
    explicit BPlusTreeNode(bool is_leaf, size_type max_keys);

    virtual ~BPlusTreeNode() = default;

    // Disable copy and move operations (nodes are managed by shared_ptr)
    BPlusTreeNode(const BPlusTreeNode&) = delete;
    BPlusTreeNode& operator=(const BPlusTreeNode&) = delete;
    BPlusTreeNode(BPlusTreeNode&&) = delete;
    BPlusTreeNode& operator=(BPlusTreeNode&&) = delete;

    // Accessors
    bool is_leaf() const { return is_leaf_; }
    size_type key_count() const { return key_count_; }
    size_type max_keys() const { return max_keys_; }
    bool is_full() const { return key_count_ >= max_keys_; }
    bool is_underflow() const { return key_count_ < min_keys(); }
    const std::vector<KeyType>& keys() const { return keys_; }

    /**
     * @brief Get the minimum number of keys for this node type
     * @return Minimum number of keys to avoid underflow
     */
    size_type min_keys() const { return (max_keys_ + 1) / 2; }

    /**
     * @brief Split this node when it becomes overfull
     * @return Shared pointer to the new node created by splitting
     */
    virtual std::shared_ptr<BPlusTreeNode<KeyType, ValueType>> split() = 0;

    /**
     * @brief Print the node structure for debugging
     * @param os Output stream to print to
     * @param depth Indentation depth for pretty printing
     */
    virtual void print(std::ostream& os, int depth = 0) const = 0;

protected:
    bool is_leaf_;
    size_type max_keys_;
    size_type key_count_;
    std::vector<KeyType> keys_;

    friend class BPlusTree<KeyType, ValueType>;
    friend class BPlusTreeIterator<KeyType, ValueType>;
};

/**
 * @brief Internal (non-leaf) node implementation
 * 
 * Internal nodes contain keys and pointers to child nodes.
 * They guide the search process but don't contain actual values.
 */
template<typename KeyType, typename ValueType>
class InternalNode : public BPlusTreeNode<KeyType, ValueType> {
public:
    using NodePtr = std::shared_ptr<BPlusTreeNode<KeyType, ValueType>>;
    using size_type = typename BPlusTreeNode<KeyType, ValueType>::size_type;

    /**
     * @brief Construct a new Internal Node
     * @param max_keys Maximum number of keys this node can hold
     */
    explicit InternalNode(size_type max_keys);

    /**
     * @brief Get a child node at the specified index
     * @param index Index of the child (0 to key_count)
     * @return Shared pointer to the child node, or nullptr if index is invalid
     */
    NodePtr get_child(size_type index) const;

    /**
     * @brief Insert a new key and child at the specified position
     * @param index Position to insert at
     * @param key Key to insert
     * @param child Child node to insert
     */
    void insert_child(size_type index, const KeyType& key, NodePtr child);

    /**
     * @brief Remove a key and child at the specified position
     * @param index Position to remove from
     */
    void remove_child(size_type index);

    std::shared_ptr<BPlusTreeNode<KeyType, ValueType>> split() override;
    void print(std::ostream& os, int depth = 0) const override;

private:
    std::vector<NodePtr> children_;

    friend class BPlusTree<KeyType, ValueType>;
};

/**
 * @brief Leaf node implementation
 * 
 * Leaf nodes contain the actual key-value pairs and are linked together
 * to enable efficient range queries.
 */
template<typename KeyType, typename ValueType>
class LeafNode : public BPlusTreeNode<KeyType, ValueType> {
public:
    using size_type = typename BPlusTreeNode<KeyType, ValueType>::size_type;

    /**
     * @brief Construct a new Leaf Node
     * @param max_keys Maximum number of keys this node can hold
     */
    explicit LeafNode(size_type max_keys);

    /**
     * @brief Find the value associated with a key
     * @param key Key to search for
     * @return Optional containing the value if found, nullopt otherwise
     */
    std::optional<ValueType> find_value(const KeyType& key) const;

    /**
     * @brief Insert a key-value pair into this leaf
     * @param key Key to insert
     * @param value Value to insert
     * @return true if insertion was successful, false if node is full
     */
    bool insert_value(const KeyType& key, const ValueType& value);

    /**
     * @brief Remove a key-value pair from this leaf
     * @param key Key to remove
     * @return true if the key was found and removed, false otherwise
     */
    bool remove_value(const KeyType& key);

    /**
     * @brief Get the next leaf node in the linked list
     * @return Shared pointer to the next leaf, or nullptr if this is the last leaf
     */
    std::shared_ptr<LeafNode<KeyType, ValueType>> get_next() const { return next_; }

    /**
     * @brief Set the next leaf node in the linked list
     * @param next Shared pointer to the next leaf node
     */
    void set_next(std::shared_ptr<LeafNode<KeyType, ValueType>> next) { next_ = next; }

    std::shared_ptr<BPlusTreeNode<KeyType, ValueType>> split() override;
    void print(std::ostream& os, int depth = 0) const override;

private:
    std::vector<ValueType> values_;
    std::shared_ptr<LeafNode<KeyType, ValueType>> next_;

    friend class BPlusTree<KeyType, ValueType>;
    friend class BPlusTreeIterator<KeyType, ValueType>;
};

} // namespace btree

#include "node_impl.hpp"