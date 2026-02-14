#pragma once

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace fdr {

template <typename T, typename Compare = std::less<T>>
class MinHeap {
public:
  explicit MinHeap(Compare comp = Compare{}) : comp_(comp) {}

  void push(const T& value) {
    data_.push_back(value);
    sift_up(data_.size() - 1);
  }

  void push(T&& value) {
    data_.push_back(std::move(value));
    sift_up(data_.size() - 1);
  }

  const T& top() const { return data_[0]; }

  T pop() {
    T result = std::move(data_[0]);
    data_[0] = std::move(data_.back());
    data_.pop_back();
    if (!data_.empty())
      sift_down(0);
    return result;
  }

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

  // Extract all elements in sorted order (drains the heap)
  std::vector<T> drain_sorted() {
    std::vector<T> result;
    result.reserve(data_.size());
    while (!data_.empty())
      result.push_back(pop());
    return result;
  }

  const std::vector<T>& data() const { return data_; }

private:
  std::vector<T> data_;
  Compare comp_;

  static std::size_t parent(std::size_t i) { return (i - 1) / 2; }
  static std::size_t left_child(std::size_t i) { return 2 * i + 1; }
  static std::size_t right_child(std::size_t i) { return 2 * i + 2; }

  void sift_up(std::size_t i) {
    while (i > 0 && comp_(data_[i], data_[parent(i)])) {
      std::swap(data_[i], data_[parent(i)]);
      i = parent(i);
    }
  }

  void sift_down(std::size_t i) {
    std::size_t n = data_.size();
    while (true) {
      std::size_t smallest = i;
      std::size_t l = left_child(i);
      std::size_t r = right_child(i);

      if (l < n && comp_(data_[l], data_[smallest]))
        smallest = l;
      if (r < n && comp_(data_[r], data_[smallest]))
        smallest = r;

      if (smallest == i)
        break;

      std::swap(data_[i], data_[smallest]);
      i = smallest;
    }
  }
};

} // namespace fdr
