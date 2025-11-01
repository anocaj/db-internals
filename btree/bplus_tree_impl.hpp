#pragma once

#include "bplus_tree.hpp"
#include <iostream>

namespace btree {

template<typename KeyType, typename ValueType>
BPlusTree<KeyType, ValueType>::BPlusTree(size_type branching_factor)
    : branching_factor_(branching_factor), root_(nullptr) {
    if (branching_factor < 3) {
        branching_factor_ = 3; // Minimum branching factor
    }
}

template<typename KeyType, typename ValueType>
bool BPlusTree<KeyType, ValueType>::insert(const KeyType& key, const ValueType& value) {
    if (!root_) {
        root_ = std::make_shared<LeafNode<KeyType, ValueType>>(branching_factor_ - 1);
    }
    
    auto result = insert_helper(root_, key, value);
    if (result.second) {
        // Root was split, create new root
        auto new_root = std::make_shared<InternalNode<KeyType, ValueType>>(branching_factor_ - 1);
        new_root->keys_.push_back(result.first);
        new_root->children_.push_back(root_);
        new_root->children_.push_back(result.second);
        new_root->key_count_ = 1;
        root_ = new_root;
    }
    return true;
}

template<typename KeyType, typename ValueType>
bool BPlusTree<KeyType, ValueType>::remove(const KeyType& key) {
    if (!root_) {
        return false;
    }
    
    bool removed = remove_helper(root_, key);
    
    // If root is internal node with only one child, make that child the new root
    if (!root_->is_leaf() && root_->key_count() == 0) {
        auto internal_root = std::static_pointer_cast<InternalNode<KeyType, ValueType>>(root_);
        if (internal_root->children_.size() == 1) {
            root_ = internal_root->children_[0];
        }
    }
    
    return removed;
}

template<typename KeyType, typename ValueType>
std::optional<ValueType> BPlusTree<KeyType, ValueType>::search(const KeyType& key) const {
    auto leaf = find_leaf(key);
    if (!leaf) {
        return std::nullopt;
    }
    return leaf->find_value(key);
}

template<typename KeyType, typename ValueType>
std::vector<std::pair<KeyType, ValueType>> BPlusTree<KeyType, ValueType>::range_query(
    const KeyType& start_key, const KeyType& end_key) const {
    
    std::vector<std::pair<KeyType, ValueType>> result;
    
    if (!root_) {
        return result;
    }
    
    // Find the starting leaf node
    auto leaf = find_leaf(start_key);
    if (!leaf) {
        return result;
    }
    
    // Traverse leaf nodes using linked list
    while (leaf) {
        for (size_type i = 0; i < leaf->key_count(); ++i) {
            const KeyType& key = leaf->keys()[i];
            if (key >= start_key && key <= end_key) {
                result.emplace_back(key, leaf->values_[i]);
            } else if (key > end_key) {
                return result; // We've passed the end of range
            }
        }
        leaf = leaf->get_next();
    }
    
    return result;
}

template<typename KeyType, typename ValueType>
auto BPlusTree<KeyType, ValueType>::range_begin(const KeyType& start_key) const -> iterator {
    if (!root_) {
        return iterator();
    }
    
    auto leaf = find_leaf(start_key);
    if (!leaf) {
        return iterator();
    }
    
    // Find the starting position within the leaf
    size_type start_index = 0;
    for (size_type i = 0; i < leaf->key_count(); ++i) {
        if (leaf->keys()[i] >= start_key) {
            start_index = i;
            break;
        }
        start_index = i + 1;
    }
    
    // If start_index is beyond this leaf, move to next leaf
    while (leaf && start_index >= leaf->key_count()) {
        leaf = leaf->get_next();
        start_index = 0;
    }
    
    return iterator(leaf, start_index);
}

template<typename KeyType, typename ValueType>
auto BPlusTree<KeyType, ValueType>::range_begin(const KeyType& start_key, const KeyType& end_key) const -> iterator {
    if (!root_) {
        return iterator();
    }
    
    auto leaf = find_leaf(start_key);
    if (!leaf) {
        return iterator();
    }
    
    // Find the starting position within the leaf
    size_type start_index = 0;
    for (size_type i = 0; i < leaf->key_count(); ++i) {
        if (leaf->keys()[i] >= start_key) {
            start_index = i;
            break;
        }
        start_index = i + 1;
    }
    
    // If start_index is beyond this leaf, move to next leaf
    while (leaf && start_index >= leaf->key_count()) {
        leaf = leaf->get_next();
        start_index = 0;
    }
    
    return iterator(leaf, start_index, end_key);
}

template<typename KeyType, typename ValueType>
auto BPlusTree<KeyType, ValueType>::range_end() const -> iterator {
    return iterator();
}

template<typename KeyType, typename ValueType>
bool BPlusTree<KeyType, ValueType>::empty() const {
    return !root_ || root_->key_count() == 0;
}

template<typename KeyType, typename ValueType>
void BPlusTree<KeyType, ValueType>::print_tree(std::ostream& os) const {
    if (root_) {
        os << "B+ Tree Structure:" << std::endl;
        root_->print(os);
    } else {
        os << "Empty tree" << std::endl;
    }
}

template<typename KeyType, typename ValueType>
auto BPlusTree<KeyType, ValueType>::insert_helper(NodePtr node, const KeyType& key, const ValueType& value) -> SplitResult {
    if (node->is_leaf()) {
        auto leaf = std::static_pointer_cast<LeafNode<KeyType, ValueType>>(node);
        
        if (leaf->insert_value(key, value)) {
            return {key, nullptr}; // No split needed
        }
        
        // Node is full, need to split
        auto new_leaf = std::static_pointer_cast<LeafNode<KeyType, ValueType>>(leaf->split());
        
        // Insert into appropriate leaf
        if (key <= leaf->keys().back()) {
            leaf->insert_value(key, value);
        } else {
            new_leaf->insert_value(key, value);
        }
        
        return {new_leaf->keys()[0], new_leaf};
    } else {
        auto internal = std::static_pointer_cast<InternalNode<KeyType, ValueType>>(node);
        
        // Find child to insert into
        size_type child_index = 0;
        for (size_type i = 0; i < node->key_count(); ++i) {
            if (key < node->keys()[i]) {
                break;
            }
            child_index = i + 1;
        }
        
        auto child = internal->get_child(child_index);
        auto result = insert_helper(child, key, value);
        
        if (result.second) {
            // Child was split
            if (!internal->is_full()) {
                internal->insert_child(child_index, result.first, result.second);
                return {key, nullptr};
            } else {
                // Internal node is full, need to split
                // Create temporary vectors to hold all keys and children including the new one
                std::vector<KeyType> all_keys = internal->keys_;
                std::vector<NodePtr> all_children = internal->children_;
                
                // Insert the new key and child in the correct position
                all_keys.insert(all_keys.begin() + child_index, result.first);
                all_children.insert(all_children.begin() + child_index + 1, result.second);
                
                // Now split based on the expanded arrays
                size_type mid = all_keys.size() / 2;
                KeyType promoted_key = all_keys[mid];
                
                // Create new internal node
                auto new_internal = std::make_shared<InternalNode<KeyType, ValueType>>(internal->max_keys_);
                
                // Distribute keys and children
                internal->keys_.assign(all_keys.begin(), all_keys.begin() + mid);
                internal->children_.assign(all_children.begin(), all_children.begin() + mid + 1);
                internal->key_count_ = mid;
                
                new_internal->keys_.assign(all_keys.begin() + mid + 1, all_keys.end());
                new_internal->children_.assign(all_children.begin() + mid + 1, all_children.end());
                new_internal->key_count_ = all_keys.size() - mid - 1;
                
                return {promoted_key, new_internal};
            }
        }
        
        return {key, nullptr};
    }
}

template<typename KeyType, typename ValueType>
bool BPlusTree<KeyType, ValueType>::remove_helper(NodePtr node, const KeyType& key) {
    if (node->is_leaf()) {
        auto leaf = std::static_pointer_cast<LeafNode<KeyType, ValueType>>(node);
        return leaf->remove_value(key);
    } else {
        auto internal = std::static_pointer_cast<InternalNode<KeyType, ValueType>>(node);
        
        // Find child that should contain the key
        size_type child_index = 0;
        for (size_type i = 0; i < node->key_count(); ++i) {
            if (key < node->keys()[i]) {
                break;
            }
            child_index = i + 1;
        }
        
        auto child = internal->get_child(child_index);
        if (!child) {
            return false;
        }
        
        bool removed = remove_helper(child, key);
        
        // Handle underflow (simplified - in a full implementation, 
        // we would implement borrowing and merging)
        if (removed && child->is_underflow() && child != root_) {
            // For educational purposes, we'll keep this simple
            // A full implementation would handle borrowing from siblings
            // and merging nodes when necessary
        }
        
        return removed;
    }
}

template<typename KeyType, typename ValueType>
std::shared_ptr<LeafNode<KeyType, ValueType>> BPlusTree<KeyType, ValueType>::find_leaf(const KeyType& key) const {
    if (!root_) {
        return nullptr;
    }
    
    auto current = root_;
    
    // Traverse down to leaf level
    while (!current->is_leaf()) {
        auto internal = std::static_pointer_cast<InternalNode<KeyType, ValueType>>(current);
        size_type child_index = 0;
        
        // Find appropriate child
        for (size_type i = 0; i < current->key_count(); ++i) {
            if (key < current->keys()[i]) {
                break;
            }
            child_index = i + 1;
        }
        
        current = internal->get_child(child_index);
        if (!current) {
            return nullptr;
        }
    }
    
    return std::static_pointer_cast<LeafNode<KeyType, ValueType>>(current);
}

} // namespace btree