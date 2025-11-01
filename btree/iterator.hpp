#pragma once

#include <iterator>
#include <memory>
#include <utility>
#include <stdexcept>

namespace btree {

// Forward declaration
template<typename KeyType, typename ValueType>
class LeafNode;

/**
 * @brief Iterator for B+ Tree range scanning
 * 
 * This iterator provides forward iteration over key-value pairs in the B+ Tree.
 * It supports both bounded and unbounded range scanning by traversing the
 * linked list of leaf nodes.
 */
template<typename KeyType, typename ValueType>
class BPlusTreeIterator {
public:
    // Iterator traits
    using iterator_category = std::forward_iterator_tag;
    using value_type = std::pair<KeyType, ValueType>;
    using difference_type = std::ptrdiff_t;
    using pointer = const value_type*;
    using reference = const value_type&;

    /**
     * @brief Construct an end iterator
     */
    BPlusTreeIterator() : current_leaf_(nullptr), current_index_(0), has_end_(false) {}

    /**
     * @brief Construct an iterator with end bound
     * @param leaf Starting leaf node
     * @param index Starting index within the leaf
     * @param end_key Upper bound for iteration (inclusive)
     */
    BPlusTreeIterator(std::shared_ptr<LeafNode<KeyType, ValueType>> leaf, 
                     std::size_t index, const KeyType& end_key);

    /**
     * @brief Construct an unbounded iterator
     * @param leaf Starting leaf node
     * @param index Starting index within the leaf
     */
    BPlusTreeIterator(std::shared_ptr<LeafNode<KeyType, ValueType>> leaf, std::size_t index);

    /**
     * @brief Dereference the iterator
     * @return Reference to the current key-value pair
     * @throws std::out_of_range if iterator is at end
     */
    value_type operator*() const;

    /**
     * @brief Pre-increment operator
     * @return Reference to this iterator after advancing
     */
    BPlusTreeIterator& operator++();

    /**
     * @brief Post-increment operator
     * @return Copy of this iterator before advancing
     */
    BPlusTreeIterator operator++(int);

    /**
     * @brief Equality comparison
     * @param other Iterator to compare with
     * @return true if iterators point to the same position
     */
    bool operator==(const BPlusTreeIterator& other) const;

    /**
     * @brief Inequality comparison
     * @param other Iterator to compare with
     * @return true if iterators point to different positions
     */
    bool operator!=(const BPlusTreeIterator& other) const;

    /**
     * @brief Check if iterator is at end
     * @return true if iterator has reached the end
     */
    bool is_end() const { return current_leaf_ == nullptr; }

private:
    std::shared_ptr<LeafNode<KeyType, ValueType>> current_leaf_;
    std::size_t current_index_;
    KeyType end_key_;
    bool has_end_;

    /**
     * @brief Check if current position exceeds the end bound
     * @return true if current key is beyond the end bound
     */
    bool exceeds_end_bound() const;
};

} // namespace btree

#include "iterator_impl.hpp"