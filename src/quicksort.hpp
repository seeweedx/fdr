#pragma once

// Quicksort with Median-of-Three Pivot and Insertion Sort Fallback
//
// A hybrid sorting algorithm that combines quicksort's average O(n log n)
// performance with insertion sort's efficiency on small arrays.
//
// Quicksort works by picking a "pivot" element, partitioning the array
// so all elements less than the pivot are on the left and all greater
// are on the right, then recursing on each half.
//
// Optimizations:
//   Median-of-three: picks pivot as the median of {first, middle, last}
//     elements, which avoids worst-case O(n²) on already-sorted input
//   Insertion sort fallback: for partitions smaller than 10 elements,
//     insertion sort's low overhead beats quicksort's recursion cost
//
// Complexity:
//   Average: O(n log n)
//   Worst:   O(n²) — rare with median-of-three
//   Space:   O(log n) stack frames from recursion
//
// Used in fdr: sorts the final top-K search results by score
// (descending) for display to the user.

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace fdr {

// Partition the subarray arr[low..high] around a pivot chosen via
// median-of-three. Returns the final index of the pivot element.
//
// Steps:
//   1. Sort arr[low], arr[mid], arr[high] so the median ends up at mid
//   2. Move pivot (median) to arr[high-1] to keep it out of the way
//   3. Walk two pointers (i from left, j from right) toward each other,
//      swapping elements that are on the wrong side of the pivot
//   4. Place the pivot in its final position at index i
//
// After partitioning: all elements left of i are < pivot,
//                     all elements right of i are > pivot
template <typename T, typename Compare = std::less<T>>
std::size_t partition(std::vector<T>& arr, std::size_t low, std::size_t high, Compare comp) {
  // Median-of-three: sort low, mid, high so arr[mid] is the median.
  // This avoids picking extreme values as pivot (which causes O(n²)).
  std::size_t mid = low + (high - low) / 2;
  if (comp(arr[high], arr[low]))
    std::swap(arr[low], arr[high]);
  if (comp(arr[mid], arr[low]))
    std::swap(arr[low], arr[mid]);
  if (comp(arr[high], arr[mid]))
    std::swap(arr[mid], arr[high]);

  // Stash the pivot at high-1 (arr[low] and arr[high] are already
  // on the correct side since we just sorted three elements)
  T pivot = arr[mid];
  std::swap(arr[mid], arr[high - 1]);

  // Hoare-style two-pointer partitioning:
  // i scans rightward for elements >= pivot
  // j scans leftward for elements <= pivot
  std::size_t i = low;
  std::size_t j = high - 1;

  while (true) {
    while (comp(arr[++i], pivot)) {} // Find element >= pivot from left
    while (comp(pivot, arr[--j])) {} // Find element <= pivot from right
    if (i >= j)
      break;                         // Pointers crossed — done
    std::swap(arr[i], arr[j]);       // Swap misplaced elements
  }

  // Restore pivot to its final sorted position
  std::swap(arr[i], arr[high - 1]);
  return i;
}

// Recursive quicksort implementation with insertion sort fallback
// for small partitions (< 10 elements).
//
// Insertion sort is O(n²) in general but very fast for tiny arrays
// because it has minimal overhead (no recursion, no pivot selection,
// good cache behavior). The threshold of 10 is a common empirical choice.
template <typename T, typename Compare = std::less<T>>
void quicksort_impl(std::vector<T>& arr, std::size_t low, std::size_t high, Compare comp) {
  if (high <= low || high - low < 1)
    return;

  // Insertion sort for small partitions: shift elements rightward
  // until we find the correct position for each key.
  // Much faster than quicksort for n < ~10 due to lower overhead.
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

  // Partition and recurse on both halves
  std::size_t pivot_idx = partition(arr, low, high, comp);
  if (pivot_idx > 0)
    quicksort_impl(arr, low, pivot_idx - 1, comp);
  quicksort_impl(arr, pivot_idx + 1, high, comp);
}

// Public entry point: sorts the entire vector using quicksort.
// Accepts a custom comparator (defaults to std::less for ascending order).
template <typename T, typename Compare = std::less<T>>
void quicksort(std::vector<T>& arr, Compare comp = Compare{}) {
  if (arr.size() <= 1)
    return;
  quicksort_impl(arr, 0, arr.size() - 1, comp);
}

} // namespace fdr
