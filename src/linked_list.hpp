#pragma once

// Singly Linked List
//
// A generic singly linked list used for hash table collision chaining.
// Each node stores a data element and a pointer to the next node.
//
// Operations:
//   push_front - O(1) - insert at head
//   remove     - O(n) - linear scan to find and unlink node
//   find       - O(n) - linear scan
//   for_each   - O(n) - iterate all nodes
//
// Memory: Each node is heap-allocated. The list owns all its nodes
// and frees them on destruction or clear().

#include <cstddef>
#include <utility>

namespace fdr {

template <typename T>
class LinkedList {
public:
  struct Node {
    T data;
    Node* next;

    Node(const T& d, Node* n = nullptr) : data(d), next(n) {}
    Node(T&& d, Node* n = nullptr) : data(std::move(d)), next(n) {}
  };

  LinkedList() : head_(nullptr), size_(0) {}

  // Destructor frees all nodes to prevent memory leaks
  ~LinkedList() { clear(); }

  // Deep copy: walks the source list and duplicates each node.
  // Uses a pointer-to-pointer (dst) to build the new list in order
  // without needing a separate "tail" pointer.
  LinkedList(const LinkedList& other) : head_(nullptr), size_(0) {
    Node* src = other.head_;
    Node** dst = &head_;
    while (src) {
      *dst = new Node(src->data);
      dst = &((*dst)->next);
      src = src->next;
      ++size_;
    }
  }

  // Copy assignment: clears existing nodes, then deep copies.
  // Self-assignment check prevents destroying our own data.
  LinkedList& operator=(const LinkedList& other) {
    if (this != &other) {
      clear();
      Node* src = other.head_;
      Node** dst = &head_;
      while (src) {
        *dst = new Node(src->data);
        dst = &((*dst)->next);
        src = src->next;
        ++size_;
      }
    }
    return *this;
  }

  // Move constructor: takes ownership of other's nodes in O(1)
  // by stealing the head pointer, then nulls out the source.
  LinkedList(LinkedList&& other) noexcept
    : head_(other.head_), size_(other.size_) {
    other.head_ = nullptr;
    other.size_ = 0;
  }

  // Move assignment: same idea as move constructor but clears
  // our existing nodes first.
  LinkedList& operator=(LinkedList&& other) noexcept {
    if (this != &other) {
      clear();
      head_ = other.head_;
      size_ = other.size_;
      other.head_ = nullptr;
      other.size_ = 0;
    }
    return *this;
  }

  // Insert at front in O(1): new node points to current head,
  // then becomes the new head.
  void push_front(const T& value) {
    head_ = new Node(value, head_);
    ++size_;
  }

  void push_front(T&& value) {
    head_ = new Node(std::move(value), head_);
    ++size_;
  }

  // Remove first occurrence of value. Uses pointer-to-pointer
  // technique: 'current' points to the pointer that holds the
  // node address (either head_ or some node's 'next'), so we
  // can relink without tracking a separate 'previous' pointer.
  bool remove(const T& value) {
    Node** current = &head_;
    while (*current) {
      if ((*current)->data == value) {
        Node* to_delete = *current;
        *current = to_delete->next; // Relink: skip over deleted node
        delete to_delete;
        --size_;
        return true;
      }
      current = &((*current)->next);
    }
    return false;
  }

  // Linear search for exact value match
  Node* find(const T& value) const {
    Node* current = head_;
    while (current) {
      if (current->data == value)
        return current;
      current = current->next;
    }
    return nullptr;
  }

  // Search with a custom predicate (used by HashTable to match
  // entries by key without needing full Entry equality)
  template <typename Pred>
  Node* find_if(Pred pred) const {
    Node* current = head_;
    while (current) {
      if (pred(current->data))
        return current;
      current = current->next;
    }
    return nullptr;
  }

  // Apply a function to every element (used for iteration
  // without exposing internal node structure)
  template <typename Func>
  void for_each(Func func) const {
    Node* current = head_;
    while (current) {
      func(current->data);
      current = current->next;
    }
  }

  // Delete all nodes and reset to empty state
  void clear() {
    while (head_) {
      Node* next = head_->next;
      delete head_;
      head_ = next;
    }
    size_ = 0;
  }

  bool empty() const { return head_ == nullptr; }
  std::size_t size() const { return size_; }
  Node* head() const { return head_; }

private:
  Node* head_;
  std::size_t size_;
};

} // namespace fdr
