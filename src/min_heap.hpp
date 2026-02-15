#pragma once

// Min-Heap (Binary Heap)
//
// A complete binary tree stored in a flat array where every parent
// is smaller than (or equal to) its children according to the
// comparator. The smallest element is always at index 0 (the root).
//
// Array layout for a node at index i:
//   parent:      (i - 1) / 2
//   left child:  2 * i + 1
//   right child: 2 * i + 2
//
// Operations:
//   push        - O(log n) - append to end, sift up to restore heap property
//   pop         - O(log n) - swap root with last, remove last, sift down
//   top         - O(1)     - peek at minimum element
//   replace_top - O(log n) - overwrite root and sift down (avoids pop+push)
//
// Used in fdr: keeps the top-K highest-scoring search results.
// We use a min-heap so the lowest score is at the root — when a
// better candidate arrives, we replace the root (evict the weakest).

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace fdr {

template <typename T, typename Compare = std::less<T>>
class MinHeap {
public:
  explicit MinHeap(Compare comp = Compare{}) : comp_(comp) {}

  // Add element: place at the end of the array, then sift up
  // (swap with parent) until the heap property is restored.
  void push(const T& value) {
    data_.push_back(value);
    sift_up(data_.size() - 1);
  }

  void push(T&& value) {
    data_.push_back(std::move(value));
    sift_up(data_.size() - 1);
  }

  // Peek at the minimum element (root of the heap)
  const T& top() const { return data_[0]; }

  // Remove and return the minimum element.
  // Moves the last element to root, then sift down to restore order.
  T pop() {
    T result = std::move(data_[0]);
    data_[0] = std::move(data_.back());
    data_.pop_back();
    if (!data_.empty())
      sift_down(0);
    return result;
  }

  // Replace root with a new value and sift down.
  // More efficient than pop() + push() because it's a single sift
  // instead of two. Used when evicting the lowest-scoring result.
  void replace_top(const T& value) {
    data_[0] = value;
    sift_down(0);
  }

  void replace_top(T&& value) {
    data_[0] = std::move(value);
    sift_down(0);
  }

  std::size_t size() const { return data_.size(); }
  bool empty() const { return data_.empty(); }

  // Repeatedly pop to get all elements in sorted order.
  // Destroys the heap in the process.
  std::vector<T> drain_sorted() {
    std::vector<T> result;
    result.reserve(data_.size());
    while (!data_.empty())
      result.push_back(pop());
    return result;
  }

  const std::vector<T>& data() const { return data_; }

private:
  std::vector<T> data_; // Flat array backing the binary tree
  Compare comp_;        // Comparator defining the heap ordering

  // Navigate the implicit tree structure in the array
  static std::size_t parent(std::size_t i) { return (i - 1) / 2; }
  static std::size_t left_child(std::size_t i) { return 2 * i + 1; }
  static std::size_t right_child(std::size_t i) { return 2 * i + 2; }

  // Sift up: after inserting at the end, repeatedly swap with parent
  // if the new element is smaller, moving it toward the root.
  void sift_up(std::size_t i) {
    while (i > 0 && comp_(data_[i], data_[parent(i)])) {
      std::swap(data_[i], data_[parent(i)]);
      i = parent(i);
    }
  }

  // Sift down: after replacing the root, repeatedly swap with the
  // smaller child, moving the element down until heap property holds.
  void sift_down(std::size_t i) {
    std::size_t n = data_.size();
    while (true) {
      std::size_t smallest = i;
      std::size_t l = left_child(i);
      std::size_t r = right_child(i);

      // Find which of {current, left, right} is smallest
      if (l < n && comp_(data_[l], data_[smallest]))
        smallest = l;
      if (r < n && comp_(data_[r], data_[smallest]))
        smallest = r;

      if (smallest == i)
        break; // Heap property satisfied

      std::swap(data_[i], data_[smallest]);
      i = smallest;
    }
  }
};

} // namespace fdr
