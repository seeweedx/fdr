#pragma once

#include "linked_list.hpp"
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace fdr {

template <typename Key, typename Value>
class HashTable {
public:
  struct Entry {
    Key key;
    Value value;

    Entry(const Key& k, const Value& v) : key(k), value(v) {}
    Entry(Key&& k, Value&& v) : key(std::move(k)), value(std::move(v)) {}

    bool operator==(const Entry& other) const { return key == other.key; }
  };

  static constexpr std::size_t DEFAULT_CAPACITY = 64;
  static constexpr double MAX_LOAD_FACTOR = 0.75;

  explicit HashTable(std::size_t capacity = DEFAULT_CAPACITY)
    : buckets_(capacity), size_(0) {}

  void insert(const Key& key, const Value& value) {
    if (load_factor() >= MAX_LOAD_FACTOR)
      rehash(buckets_.size() * 2);

    std::size_t idx = bucket_index(key);
    auto* node = find_node(idx, key);
    if (node) {
      node->data.value = value;
    } else {
      buckets_[idx].push_front(Entry(key, value));
      ++size_;
    }
  }

  Value* find(const Key& key) {
    std::size_t idx = bucket_index(key);
    auto* node = find_node(idx, key);
    return node ? &node->data.value : nullptr;
  }

  const Value* find(const Key& key) const {
    std::size_t idx = bucket_index(key);
    auto* node = find_node(idx, key);
    return node ? &node->data.value : nullptr;
  }

  bool remove(const Key& key) {
    std::size_t idx = bucket_index(key);
    auto* node = find_node(idx, key);
    if (node) {
      buckets_[idx].remove(node->data);
      --size_;
      return true;
    }
    return false;
  }

  bool contains(const Key& key) const { return find(key) != nullptr; }

  Value& operator[](const Key& key) {
    if (load_factor() >= MAX_LOAD_FACTOR)
      rehash(buckets_.size() * 2);

    std::size_t idx = bucket_index(key);
    auto* node = find_node(idx, key);
    if (node)
      return node->data.value;

    buckets_[idx].push_front(Entry(key, Value{}));
    ++size_;
    return buckets_[idx].head()->data.value;
  }

  template <typename Func>
  void for_each(Func func) const {
    for (const auto& bucket : buckets_) {
      bucket.for_each([&func](const Entry& entry) {
        func(entry.key, entry.value);
      });
    }
  }

  std::size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  std::size_t bucket_count() const { return buckets_.size(); }
  double load_factor() const {
    return static_cast<double>(size_) / buckets_.size();
  }

private:
  // std::vector only used to hold the array of buckets;
  // each bucket is our custom LinkedList for chaining.
  std::vector<LinkedList<Entry>> buckets_;
  std::size_t size_;

  std::size_t bucket_index(const Key& key) const {
    return std::hash<Key>{}(key) % buckets_.size();
  }

  typename LinkedList<Entry>::Node* find_node(std::size_t idx, const Key& key) const {
    return buckets_[idx].find_if([&key](const Entry& e) { return e.key == key; });
  }

  void rehash(std::size_t new_capacity) {
    std::vector<LinkedList<Entry>> new_buckets(new_capacity);
    for (auto& bucket : buckets_) {
      bucket.for_each([&](const Entry& entry) {
        std::size_t new_idx = std::hash<Key>{}(entry.key) % new_capacity;
        new_buckets[new_idx].push_front(entry);
      });
    }
    buckets_ = std::move(new_buckets);
  }
};

} // namespace fdr
