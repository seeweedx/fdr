#pragma once

// Hash Table with Separate Chaining
//
// Maps keys to values using a hash function (std::hash) to determine
// which bucket a key-value pair belongs to. Collisions (multiple keys
// hashing to the same bucket) are resolved via separate chaining,
// where each bucket is a LinkedList of entries.
//
// Operations:
//   insert   - O(1) average, O(n) worst - add or update key-value pair
//   find     - O(1) average, O(n) worst - return pointer to value
//   remove   - O(1) average, O(n) worst - delete entry by key
//   operator[] - O(1) average - access or create entry
//
// Automatically rehashes (doubles bucket count) when load factor
// exceeds 0.75 to keep chains short and maintain O(1) average.
//
// Used in fdr: maps trigram strings -> vectors of line indices
// for fast candidate lookup during fuzzy search.

#include "linked_list.hpp"
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace fdr {

template <typename Key, typename Value>
class HashTable {
public:
  // Each bucket entry stores a key-value pair.
  // operator== compares only keys (not values) so LinkedList::remove
  // can find entries by key alone.
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

  // Insert or update: if key exists, overwrite value; otherwise
  // push a new entry to the front of the bucket's chain.
  void insert(const Key& key, const Value& value) {
    if (load_factor() >= MAX_LOAD_FACTOR)
      rehash(buckets_.size() * 2);

    std::size_t idx = bucket_index(key);
    auto* node = find_node(idx, key);
    if (node) {
      node->data.value = value; // Key exists: update value
    } else {
      buckets_[idx].push_front(Entry(key, value)); // New key: add to chain
      ++size_;
    }
  }

  // Returns pointer to value if found, nullptr otherwise.
  // Caller can modify the value through the returned pointer.
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

  // Remove entry by key. Returns true if found and removed.
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

  // Subscript operator: returns reference to value, inserting a
  // default-constructed value if the key doesn't exist yet.
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

  // Iterate all entries across all buckets
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

  // Load factor = entries / buckets. When this exceeds MAX_LOAD_FACTOR,
  // we rehash to keep average chain length near 1.
  double load_factor() const {
    return static_cast<double>(size_) / buckets_.size();
  }

private:
  // std::vector only used to hold the array of buckets;
  // each bucket is our custom LinkedList for chaining.
  std::vector<LinkedList<Entry>> buckets_;
  std::size_t size_;

  // Hash the key and map to a bucket index using modulo
  std::size_t bucket_index(const Key& key) const {
    return std::hash<Key>{}(key) % buckets_.size();
  }

  // Search a specific bucket's chain for a matching key
  typename LinkedList<Entry>::Node* find_node(std::size_t idx, const Key& key) const {
    return buckets_[idx].find_if([&key](const Entry& e) { return e.key == key; });
  }

  // Rehash: create a new, larger bucket array and redistribute all entries.
  // This is triggered when load factor exceeds the threshold.
  // Every entry is re-hashed because bucket_index depends on bucket count.
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
