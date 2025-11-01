#pragma once

#include "iterator.hpp"
#include "node.hpp"

namespace btree {

template<typename KeyType, typename ValueType>
BPlusTreeIterator<KeyType, ValueType>::BPlusTreeIterator(
    std::shared_ptr<LeafNode<KeyType, ValueType>> leaf, 
    std::size_t index, 
    const KeyType& end_key)
    : current_leaf_(leaf), current_index_(index), end_key_(end_key), has_end_(true) {}

template<typename KeyType, typename ValueType>
BPlusTreeIterator<KeyType, ValueType>::BPlusTreeIterator(
    std::shared_ptr<LeafNode<KeyType, ValueType>> leaf, 
    std::size_t index)
    : current_leaf_(leaf), current_index_(index), has_end_(false) {}

template<typename KeyType, typename ValueType>
auto BPlusTreeIterator<KeyType, ValueType>::operator*() const -> value_type {
    if (!current_leaf_ || current_index_ >= current_leaf_->key_count()) {
        throw std::out_of_range("Iterator out of range");
    }
    return std::make_pair(current_leaf_->keys()[current_index_], 
                         current_leaf_->values_[current_index_]);
}

template<typename KeyType, typename ValueType>
BPlusTreeIterator<KeyType, ValueType>& BPlusTreeIterator<KeyType, ValueType>::operator++() {
    if (!current_leaf_) {
        return *this;
    }
    
    current_index_++;
    
    // Check if we need to move to next leaf
    if (current_index_ >= current_leaf_->key_count()) {
        current_leaf_ = current_leaf_->get_next();
        current_index_ = 0;
    }
    
    // Check if we've exceeded the end key
    if (exceeds_end_bound()) {
        current_leaf_ = nullptr; // Mark as end
    }
    
    return *this;
}

template<typename KeyType, typename ValueType>
BPlusTreeIterator<KeyType, ValueType> BPlusTreeIterator<KeyType, ValueType>::operator++(int) {
    BPlusTreeIterator temp = *this;
    ++(*this);
    return temp;
}

template<typename KeyType, typename ValueType>
bool BPlusTreeIterator<KeyType, ValueType>::operator==(const BPlusTreeIterator& other) const {
    return current_leaf_ == other.current_leaf_ && current_index_ == other.current_index_;
}

template<typename KeyType, typename ValueType>
bool BPlusTreeIterator<KeyType, ValueType>::operator!=(const BPlusTreeIterator& other) const {
    return !(*this == other);
}

template<typename KeyType, typename ValueType>
bool BPlusTreeIterator<KeyType, ValueType>::exceeds_end_bound() const {
    return has_end_ && current_leaf_ && 
           current_index_ < current_leaf_->key_count() && 
           current_leaf_->keys()[current_index_] > end_key_;
}

} // namespace btree