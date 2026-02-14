#pragma once

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace fdr {

template <typename T, typename Compare = std::less<T>>
std::size_t partition(std::vector<T>& arr, std::size_t low, std::size_t high, Compare comp) {
  // Median-of-three pivot selection
  std::size_t mid = low + (high - low) / 2;
  if (comp(arr[high], arr[low]))
    std::swap(arr[low], arr[high]);
  if (comp(arr[mid], arr[low]))
    std::swap(arr[low], arr[mid]);
  if (comp(arr[high], arr[mid]))
    std::swap(arr[mid], arr[high]);

  T pivot = arr[mid];
  std::swap(arr[mid], arr[high - 1]);

  std::size_t i = low;
  std::size_t j = high - 1;

  while (true) {
    while (comp(arr[++i], pivot)) {}
    while (comp(pivot, arr[--j])) {}
    if (i >= j)
      break;
    std::swap(arr[i], arr[j]);
  }

  std::swap(arr[i], arr[high - 1]);
  return i;
}

template <typename T, typename Compare = std::less<T>>
void quicksort_impl(std::vector<T>& arr, std::size_t low, std::size_t high, Compare comp) {
  if (high <= low || high - low < 1)
    return;

  // Insertion sort for small partitions
  if (high - low < 10) {
    for (std::size_t i = low + 1; i <= high; ++i) {
      T key = std::move(arr[i]);
      std::size_t j = i;
      while (j > low && comp(key, arr[j - 1])) {
        arr[j] = std::move(arr[j - 1]);
        --j;
      }
      arr[j] = std::move(key);
    }
    return;
  }

  std::size_t pivot_idx = partition(arr, low, high, comp);
  if (pivot_idx > 0)
    quicksort_impl(arr, low, pivot_idx - 1, comp);
  quicksort_impl(arr, pivot_idx + 1, high, comp);
}

template <typename T, typename Compare = std::less<T>>
void quicksort(std::vector<T>& arr, Compare comp = Compare{}) {
  if (arr.size() <= 1)
    return;
  quicksort_impl(arr, 0, arr.size() - 1, comp);
}

} // namespace fdr
