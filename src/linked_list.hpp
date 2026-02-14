#pragma once

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

  ~LinkedList() { clear(); }

  // Copy
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

  // Move
  LinkedList(LinkedList&& other) noexcept
    : head_(other.head_), size_(other.size_) {
    other.head_ = nullptr;
    other.size_ = 0;
  }

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

  void push_front(const T& value) {
    head_ = new Node(value, head_);
    ++size_;
  }

  void push_front(T&& value) {
    head_ = new Node(std::move(value), head_);
    ++size_;
  }

  bool remove(const T& value) {
    Node** current = &head_;
    while (*current) {
      if ((*current)->data == value) {
        Node* to_delete = *current;
        *current = to_delete->next;
        delete to_delete;
        --size_;
        return true;
      }
      current = &((*current)->next);
    }
    return false;
  }

  Node* find(const T& value) const {
    Node* current = head_;
    while (current) {
      if (current->data == value)
        return current;
      current = current->next;
    }
    return nullptr;
  }

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

  template <typename Func>
  void for_each(Func func) const {
    Node* current = head_;
    while (current) {
      func(current->data);
      current = current->next;
    }
  }

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
