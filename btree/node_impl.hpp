#pragma once

#include "node.hpp"
#include <algorithm>
#include <iomanip>

namespace btree {

// BPlusTreeNode implementation
template<typename KeyType, typename ValueType>
BPlusTreeNode<KeyType, ValueType>::BPlusTreeNode(bool is_leaf, size_type max_keys)
    : is_leaf_(is_leaf), max_keys_(max_keys), key_count_(0) {
    keys_.reserve(max_keys);
}

// InternalNode implementation
template<typename KeyType, typename ValueType>
InternalNode<KeyType, ValueType>::InternalNode(size_type max_keys)
    : BPlusTreeNode<KeyType, ValueType>(false, max_keys) {
    children_.reserve(max_keys + 1);
}

template<typename KeyType, typename ValueType>
auto InternalNode<KeyType, ValueType>::get_child(size_type index) const -> NodePtr {
    return (index < children_.size()) ? children_[index] : nullptr;
}

template<typename KeyType, typename ValueType>
void InternalNode<KeyType, ValueType>::insert_child(size_type index, const KeyType& key, NodePtr child) {
    if (index <= this->key_count_) {
        this->keys_.insert(this->keys_.begin() + index, key);
        children_.insert(children_.begin() + index + 1, child);
        this->key_count_++;
    }
}

template<typename KeyType, typename ValueType>
void InternalNode<KeyType, ValueType>::remove_child(size_type index) {
    if (index < this->key_count_) {
        this->keys_.erase(this->keys_.begin() + index);
        children_.erase(children_.begin() + index + 1);
        this->key_count_--;
    }
}

template<typename KeyType, typename ValueType>
std::shared_ptr<BPlusTreeNode<KeyType, ValueType>> InternalNode<KeyType, ValueType>::split() {
    size_type mid = this->max_keys_ / 2;
    auto new_node = std::make_shared<InternalNode<KeyType, ValueType>>(this->max_keys_);
    
    // The middle key will be promoted to parent, so we don't include it in either node
    // Move keys after mid to new node (excluding the middle key)
    new_node->keys_.assign(this->keys_.begin() + mid + 1, this->keys_.end());
    new_node->children_.assign(children_.begin() + mid + 1, children_.end());
    new_node->key_count_ = this->keys_.size() - mid - 1;
    
    // Truncate current node (excluding the middle key)
    this->keys_.resize(mid);
    children_.resize(mid + 1);
    this->key_count_ = mid;
    
    return new_node;
}

template<typename KeyType, typename ValueType>
void InternalNode<KeyType, ValueType>::print(std::ostream& os, int depth) const {
    std::string indent(depth * 2, ' ');
    os << indent << "Internal Node: ";
    for (size_type i = 0; i < this->key_count_; ++i) {
        os << this->keys_[i];
        if (i < this->key_count_ - 1) os << ", ";
    }
    os << std::endl;
    
    for (size_type i = 0; i <= this->key_count_; ++i) {
        if (i < children_.size() && children_[i]) {
            children_[i]->print(os, depth + 1);
        }
    }
}

// LeafNode implementation
template<typename KeyType, typename ValueType>
LeafNode<KeyType, ValueType>::LeafNode(size_type max_keys)
    : BPlusTreeNode<KeyType, ValueType>(true, max_keys), next_(nullptr) {
    values_.reserve(max_keys);
}

template<typename KeyType, typename ValueType>
std::optional<ValueType> LeafNode<KeyType, ValueType>::find_value(const KeyType& key) const {
    auto it = std::lower_bound(this->keys_.begin(), 
                              this->keys_.begin() + this->key_count_, key);
    size_type index = it - this->keys_.begin();
    
    if (index < this->key_count_ && this->keys_[index] == key) {
        return values_[index];
    }
    return std::nullopt;
}

template<typename KeyType, typename ValueType>
bool LeafNode<KeyType, ValueType>::insert_value(const KeyType& key, const ValueType& value) {
    auto it = std::lower_bound(this->keys_.begin(), 
                              this->keys_.begin() + this->key_count_, key);
    size_type index = it - this->keys_.begin();
    
    // Check if key already exists
    if (index < this->key_count_ && this->keys_[index] == key) {
        values_[index] = value; // Update existing value
        return true;
    }
    
    // Check if node is full
    if (this->is_full()) {
        return false;
    }
    
    // Insert key and value
    this->keys_.insert(this->keys_.begin() + index, key);
    values_.insert(values_.begin() + index, value);
    this->key_count_++;
    return true;
}

template<typename KeyType, typename ValueType>
bool LeafNode<KeyType, ValueType>::remove_value(const KeyType& key) {
    auto it = std::lower_bound(this->keys_.begin(), 
                              this->keys_.begin() + this->key_count_, key);
    size_type index = it - this->keys_.begin();
    
    if (index < this->key_count_ && this->keys_[index] == key) {
        this->keys_.erase(this->keys_.begin() + index);
        values_.erase(values_.begin() + index);
        this->key_count_--;
        return true;
    }
    return false;
}

template<typename KeyType, typename ValueType>
std::shared_ptr<BPlusTreeNode<KeyType, ValueType>> LeafNode<KeyType, ValueType>::split() {
    size_type mid = (this->max_keys_ + 1) / 2;
    auto new_leaf = std::make_shared<LeafNode<KeyType, ValueType>>(this->max_keys_);
    
    // Move keys and values to new leaf
    new_leaf->keys_.assign(this->keys_.begin() + mid, this->keys_.end());
    new_leaf->values_.assign(values_.begin() + mid, values_.end());
    new_leaf->key_count_ = this->keys_.size() - mid;
    
    // Update linked list pointers
    new_leaf->next_ = this->next_;
    this->next_ = new_leaf;
    
    // Truncate current leaf
    this->keys_.resize(mid);
    values_.resize(mid);
    this->key_count_ = mid;
    
    return new_leaf;
}

template<typename KeyType, typename ValueType>
void LeafNode<KeyType, ValueType>::print(std::ostream& os, int depth) const {
    std::string indent(depth * 2, ' ');
    os << indent << "Leaf Node: ";
    for (size_type i = 0; i < this->key_count_; ++i) {
        os << "(" << this->keys_[i] << ":" << values_[i] << ")";
        if (i < this->key_count_ - 1) os << ", ";
    }
    os << std::endl;
}

} // namespace btree