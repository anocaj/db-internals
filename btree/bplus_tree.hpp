#pragma once

#include <vector>
#include <memory>
#include <optional>
#include <algorithm>
#include <iostream>

namespace btree {

// Forward declarations
template<typename KeyType, typename ValueType>
class BPlusTree;

template<typename KeyType, typename ValueType>
class BPlusTreeNode;

template<typename KeyType, typename ValueType>
class InternalNode;

template<typename KeyType, typename ValueType>
class LeafNode;

/**
 * Abstract base class for B+ Tree nodes
 */
template<typename KeyType, typename ValueType>
class BPlusTreeNode {
public:
    explicit BPlusTreeNode(bool is_leaf, size_t max_keys)
        : is_leaf_(is_leaf), max_keys_(max_keys), key_count_(0) {
        keys_.reserve(max_keys);
    }
    
    virtual ~BPlusTreeNode() = default;
    
    bool is_leaf() const { return is_leaf_; }
    size_t key_count() const { return key_count_; }
    size_t max_keys() const { return max_keys_; }
    bool is_full() const { return key_count_ >= max_keys_; }
    bool is_underflow() const { return key_count_ < (max_keys_ + 1) / 2; }
    
    const std::vector<KeyType>& keys() const { return keys_; }
    
    // Pure virtual methods to be implemented by derived classes
    virtual std::shared_ptr<btree::BPlusTreeNode<KeyType, ValueType>> split() = 0;
    virtual void print(int depth = 0) const = 0;

protected:
    bool is_leaf_;
    size_t max_keys_;
    size_t key_count_;
    std::vector<KeyType> keys_;
    
    friend class BPlusTree<KeyType, ValueType>;
};

/**
 * Internal (non-leaf) node implementation
 */
template<typename KeyType, typename ValueType>
class InternalNode : public BPlusTreeNode<KeyType, ValueType> {
public:
    explicit InternalNode(size_t max_keys)
        : BPlusTreeNode<KeyType, ValueType>(false, max_keys) {
        children_.reserve(max_keys + 1);
    }
    
    std::shared_ptr<btree::BPlusTreeNode<KeyType, ValueType>> get_child(size_t index) const {
        return (index < children_.size()) ? children_[index] : nullptr;
    }
    
    void insert_child(size_t index, const KeyType& key, 
                     std::shared_ptr<btree::BPlusTreeNode<KeyType, ValueType>> child) {
        if (index <= this->key_count_) {
            this->keys_.insert(this->keys_.begin() + index, key);
            children_.insert(children_.begin() + index + 1, child);
            this->key_count_++;
        }
    }
    
    void remove_child(size_t index) {
        if (index < this->key_count_) {
            this->keys_.erase(this->keys_.begin() + index);
            children_.erase(children_.begin() + index + 1);
            this->key_count_--;
        }
    }
    
    std::shared_ptr<btree::BPlusTreeNode<KeyType, ValueType>> split() override {
        size_t mid = this->max_keys_ / 2;
        auto new_node = std::make_shared<btree::InternalNode<KeyType, ValueType>>(this->max_keys_);
        
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
    
    void print(int depth = 0) const override {
        std::string indent(depth * 2, ' ');
        std::cout << indent << "Internal Node: ";
        for (size_t i = 0; i < this->key_count_; ++i) {
            std::cout << this->keys_[i];
            if (i < this->key_count_ - 1) std::cout << ", ";
        }
        std::cout << std::endl;
        
        for (size_t i = 0; i <= this->key_count_; ++i) {
            if (children_[i]) {
                children_[i]->print(depth + 1);
            }
        }
    }

private:
    std::vector<std::shared_ptr<btree::BPlusTreeNode<KeyType, ValueType>>> children_;
    
    friend class BPlusTree<KeyType, ValueType>;
};

/**
 * Leaf node implementation
 */
template<typename KeyType, typename ValueType>
class LeafNode : public BPlusTreeNode<KeyType, ValueType> {
public:
    explicit LeafNode(size_t max_keys)
        : BPlusTreeNode<KeyType, ValueType>(true, max_keys), next_(nullptr) {
        values_.reserve(max_keys);
    }
    
    std::optional<ValueType> find_value(const KeyType& key) const {
        auto it = std::lower_bound(this->keys_.begin(), 
                                  this->keys_.begin() + this->key_count_, key);
        size_t index = it - this->keys_.begin();
        
        if (index < this->key_count_ && this->keys_[index] == key) {
            return values_[index];
        }
        return std::nullopt;
    }
    
    bool insert_value(const KeyType& key, const ValueType& value) {
        auto it = std::lower_bound(this->keys_.begin(), 
                                  this->keys_.begin() + this->key_count_, key);
        size_t index = it - this->keys_.begin();
        
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
    
    bool remove_value(const KeyType& key) {
        auto it = std::lower_bound(this->keys_.begin(), 
                                  this->keys_.begin() + this->key_count_, key);
        size_t index = it - this->keys_.begin();
        
        if (index < this->key_count_ && this->keys_[index] == key) {
            this->keys_.erase(this->keys_.begin() + index);
            values_.erase(values_.begin() + index);
            this->key_count_--;
            return true;
        }
        return false;
    }
    
    std::shared_ptr<btree::BPlusTreeNode<KeyType, ValueType>> split() override {
        size_t mid = (this->max_keys_ + 1) / 2;
        auto new_leaf = std::make_shared<btree::LeafNode<KeyType, ValueType>>(this->max_keys_);
        
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
    
    void print(int depth = 0) const override {
        std::string indent(depth * 2, ' ');
        std::cout << indent << "Leaf Node: ";
        for (size_t i = 0; i < this->key_count_; ++i) {
            std::cout << "(" << this->keys_[i] << ":" << values_[i] << ")";
            if (i < this->key_count_ - 1) std::cout << ", ";
        }
        std::cout << std::endl;
    }
    
    std::shared_ptr<LeafNode<KeyType, ValueType>> get_next() const {
        return next_;
    }
    
    void set_next(std::shared_ptr<LeafNode<KeyType, ValueType>> next) {
        next_ = next;
    }

private:
    std::vector<ValueType> values_;
    std::shared_ptr<LeafNode<KeyType, ValueType>> next_;
    
    friend class BPlusTree<KeyType, ValueType>;
};

/**
 * Iterator for B+ Tree range scanning
 */
template<typename KeyType, typename ValueType>
class BPlusTreeIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = std::pair<KeyType, ValueType>;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;
    
    BPlusTreeIterator() : current_leaf_(nullptr), current_index_(0) {}
    
    BPlusTreeIterator(std::shared_ptr<LeafNode<KeyType, ValueType>> leaf, 
                     size_t index, const KeyType& end_key)
        : current_leaf_(leaf), current_index_(index), end_key_(end_key), has_end_(true) {}
    
    BPlusTreeIterator(std::shared_ptr<LeafNode<KeyType, ValueType>> leaf, size_t index)
        : current_leaf_(leaf), current_index_(index), has_end_(false) {}
    
    value_type operator*() const {
        if (!current_leaf_ || current_index_ >= current_leaf_->key_count()) {
            throw std::out_of_range("Iterator out of range");
        }
        return std::make_pair(current_leaf_->keys()[current_index_], 
                             current_leaf_->values_[current_index_]);
    }
    
    BPlusTreeIterator& operator++() {
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
        if (has_end_ && current_leaf_ && current_index_ < current_leaf_->key_count()) {
            if (current_leaf_->keys()[current_index_] > end_key_) {
                current_leaf_ = nullptr; // Mark as end
            }
        }
        
        return *this;
    }
    
    BPlusTreeIterator operator++(int) {
        BPlusTreeIterator temp = *this;
        ++(*this);
        return temp;
    }
    
    bool operator==(const BPlusTreeIterator& other) const {
        return current_leaf_ == other.current_leaf_ && current_index_ == other.current_index_;
    }
    
    bool operator!=(const BPlusTreeIterator& other) const {
        return !(*this == other);
    }
    
    bool is_end() const {
        return current_leaf_ == nullptr;
    }

private:
    std::shared_ptr<LeafNode<KeyType, ValueType>> current_leaf_;
    size_t current_index_;
    KeyType end_key_;
    bool has_end_ = false;
};

/**
 * Main B+ Tree implementation
 */
template<typename KeyType, typename ValueType>
class BPlusTree {
public:
    explicit BPlusTree(size_t branching_factor = 64)
        : branching_factor_(branching_factor), root_(nullptr) {
        if (branching_factor < 3) {
            branching_factor_ = 3; // Minimum branching factor
        }
    }
    
    /**
     * Insert a key-value pair into the tree
     */
    bool insert(const KeyType& key, const ValueType& value) {
        if (!root_) {
            root_ = std::make_shared<btree::LeafNode<KeyType, ValueType>>(branching_factor_ - 1);
        }
        
        auto result = insert_helper(root_, key, value);
        if (result.second) {
            // Root was split, create new root
            auto new_root = std::make_shared<btree::InternalNode<KeyType, ValueType>>(branching_factor_ - 1);
            new_root->keys_.push_back(result.first);
            new_root->children_.push_back(root_);
            new_root->children_.push_back(result.second);
            new_root->key_count_ = 1;
            root_ = new_root;
        }
        return true;
    }
    
    /**
     * Remove a key from the tree
     */
    bool remove(const KeyType& key) {
        if (!root_) {
            return false;
        }
        
        bool removed = remove_helper(root_, key);
        
        // If root is internal node with only one child, make that child the new root
        if (!root_->is_leaf() && root_->key_count() == 0) {
            auto internal_root = std::static_pointer_cast<btree::InternalNode<KeyType, ValueType>>(root_);
            if (internal_root->children_.size() == 1) {
                root_ = internal_root->children_[0];
            }
        }
        
        return removed;
    }
    
    /**
     * Search for a key in the tree
     */
    std::optional<ValueType> search(const KeyType& key) const {
        if (!root_) {
            return std::nullopt;
        }
        
        auto current = root_;
        
        // Traverse down to leaf level
        while (!current->is_leaf()) {
            auto internal = std::static_pointer_cast<btree::InternalNode<KeyType, ValueType>>(current);
            size_t child_index = 0;
            
            // Find appropriate child
            for (size_t i = 0; i < current->key_count(); ++i) {
                if (key < current->keys()[i]) {
                    break;
                }
                child_index = i + 1;
            }
            
            current = internal->get_child(child_index);
            if (!current) {
                return std::nullopt;
            }
        }
        
        // Search in leaf node
        auto leaf = std::static_pointer_cast<btree::LeafNode<KeyType, ValueType>>(current);
        return leaf->find_value(key);
    }
    
    /**
     * Range query - returns all key-value pairs in the specified range [start, end]
     */
    std::vector<std::pair<KeyType, ValueType>> range_query(const KeyType& start, const KeyType& end) const {
        std::vector<std::pair<KeyType, ValueType>> result;
        
        if (!root_) {
            return result;
        }
        
        // Find the starting leaf node
        auto current = root_;
        while (!current->is_leaf()) {
            auto internal = std::static_pointer_cast<btree::InternalNode<KeyType, ValueType>>(current);
            size_t child_index = 0;
            
            for (size_t i = 0; i < current->key_count(); ++i) {
                if (start < current->keys()[i]) {
                    break;
                }
                child_index = i + 1;
            }
            
            current = internal->get_child(child_index);
            if (!current) {
                return result;
            }
        }
        
        // Traverse leaf nodes using linked list
        auto leaf = std::static_pointer_cast<btree::LeafNode<KeyType, ValueType>>(current);
        
        while (leaf) {
            for (size_t i = 0; i < leaf->key_count(); ++i) {
                const KeyType& key = leaf->keys()[i];
                if (key >= start && key <= end) {
                    result.emplace_back(key, leaf->values_[i]);
                } else if (key > end) {
                    return result; // We've passed the end of range
                }
            }
            leaf = leaf->get_next();
        }
        
        return result;
    }
    
    /**
     * Print the tree structure for debugging/educational purposes
     */
    void print_tree() const {
        if (root_) {
            std::cout << "B+ Tree Structure:" << std::endl;
            root_->print();
        } else {
            std::cout << "Empty tree" << std::endl;
        }
    }
    
    /**
     * Check if the tree is empty
     */
    bool empty() const {
        return !root_ || root_->key_count() == 0;
    }

private:
    size_t branching_factor_;
    std::shared_ptr<btree::BPlusTreeNode<KeyType, ValueType>> root_;
    
    /**
     * Helper function for insertion
     * Returns pair<promoted_key, new_node> if split occurred, otherwise <key, nullptr>
     */
    std::pair<KeyType, std::shared_ptr<btree::BPlusTreeNode<KeyType, ValueType>>> 
    insert_helper(std::shared_ptr<btree::BPlusTreeNode<KeyType, ValueType>> node, 
                  const KeyType& key, const ValueType& value) {
        
        if (node->is_leaf()) {
            auto leaf = std::static_pointer_cast<btree::LeafNode<KeyType, ValueType>>(node);
            
            if (leaf->insert_value(key, value)) {
                return {key, nullptr}; // No split needed
            }
            
            // Node is full, need to split
            auto new_leaf = std::static_pointer_cast<btree::LeafNode<KeyType, ValueType>>(leaf->split());
            
            // Insert into appropriate leaf
            if (key <= leaf->keys().back()) {
                leaf->insert_value(key, value);
            } else {
                new_leaf->insert_value(key, value);
            }
            
            return {new_leaf->keys()[0], new_leaf};
        } else {
            auto internal = std::static_pointer_cast<btree::InternalNode<KeyType, ValueType>>(node);
            
            // Find child to insert into
            size_t child_index = 0;
            for (size_t i = 0; i < node->key_count(); ++i) {
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
                    std::vector<std::shared_ptr<btree::BPlusTreeNode<KeyType, ValueType>>> all_children = internal->children_;
                    
                    // Insert the new key and child in the correct position
                    all_keys.insert(all_keys.begin() + child_index, result.first);
                    all_children.insert(all_children.begin() + child_index + 1, result.second);
                    
                    // Now split based on the expanded arrays
                    size_t mid = all_keys.size() / 2;
                    KeyType promoted_key = all_keys[mid];
                    
                    // Create new internal node
                    auto new_internal = std::make_shared<btree::InternalNode<KeyType, ValueType>>(internal->max_keys_);
                    
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
    
    /**
     * Helper function for deletion
     */
    bool remove_helper(std::shared_ptr<btree::BPlusTreeNode<KeyType, ValueType>> node, const KeyType& key) {
        if (node->is_leaf()) {
            auto leaf = std::static_pointer_cast<btree::LeafNode<KeyType, ValueType>>(node);
            return leaf->remove_value(key);
        } else {
            auto internal = std::static_pointer_cast<btree::InternalNode<KeyType, ValueType>>(node);
            
            // Find child that should contain the key
            size_t child_index = 0;
            for (size_t i = 0; i < node->key_count(); ++i) {
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

public:
    using iterator = BPlusTreeIterator<KeyType, ValueType>;
    
    /**
     * Get iterator for range scanning starting from a specific key
     */
    iterator range_begin(const KeyType& start_key) const {
        if (!root_) {
            return iterator();
        }
        
        // Find the starting leaf node
        auto current = root_;
        while (!current->is_leaf()) {
            auto internal = std::static_pointer_cast<btree::InternalNode<KeyType, ValueType>>(current);
            size_t child_index = 0;
            
            for (size_t i = 0; i < current->key_count(); ++i) {
                if (start_key < current->keys()[i]) {
                    break;
                }
                child_index = i + 1;
            }
            
            current = internal->get_child(child_index);
            if (!current) {
                return iterator();
            }
        }
        
        auto leaf = std::static_pointer_cast<btree::LeafNode<KeyType, ValueType>>(current);
        
        // Find the starting position within the leaf
        size_t start_index = 0;
        for (size_t i = 0; i < leaf->key_count(); ++i) {
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
    
    /**
     * Get iterator for range scanning with both start and end bounds
     */
    iterator range_begin(const KeyType& start_key, const KeyType& end_key) const {
        if (!root_) {
            return iterator();
        }
        
        // Find the starting leaf node (same as above)
        auto current = root_;
        while (!current->is_leaf()) {
            auto internal = std::static_pointer_cast<btree::InternalNode<KeyType, ValueType>>(current);
            size_t child_index = 0;
            
            for (size_t i = 0; i < current->key_count(); ++i) {
                if (start_key < current->keys()[i]) {
                    break;
                }
                child_index = i + 1;
            }
            
            current = internal->get_child(child_index);
            if (!current) {
                return iterator();
            }
        }
        
        auto leaf = std::static_pointer_cast<btree::LeafNode<KeyType, ValueType>>(current);
        
        // Find the starting position within the leaf
        size_t start_index = 0;
        for (size_t i = 0; i < leaf->key_count(); ++i) {
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
    
    /**
     * Get end iterator
     */
    iterator range_end() const {
        return iterator();
    }
};



} // namespace btree