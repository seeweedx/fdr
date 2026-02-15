#pragma once

// Fuzzy Matching Engine
//
// The core search algorithm for fdr. Provides fuzzy text matching
// using a trigram index for fast candidate retrieval and a scoring
// system that ranks matches by quality.
//
// Search pipeline:
//   1. build_index() — precompute trigram index over all input lines
//   2. search() — for each query:
//      a. Extract trigrams from query tokens
//      b. Look up candidate lines via hash table (O(1) per trigram)
//      c. Deduplicate candidates using red-black tree
//      d. Score each candidate (substring match preferred, subsequence fallback)
//      e. Keep top-K results using min-heap
//      f. Sort final results with quicksort
//
// Multi-token matching: space-separated query words are matched
// independently (AND logic, any order). "bar foo" matches "foo bar".

#include <cstddef>
#include <string>
#include <vector>

namespace fdr {

// Represents a single search result: which line matched, how well
// it scored, and which character positions were matched (for highlighting).
struct MatchResult {
  std::size_t line_index;                   // Index into the original lines vector
  int score;                                // Higher = better match quality
  std::vector<std::size_t> match_positions; // Character indices that matched (for highlighting)
};

// FuzzyMatcher owns the trigram index and performs searches against it.
// Usage: call build_index() once with all lines, then search() per keystroke.
class FuzzyMatcher {
public:
  // Build the trigram index over the input lines.
  // Extracts all 3-character substrings (trigrams) from each line
  // and maps them to line indices in a hash table for O(1) lookup.
  void build_index(const std::vector<std::string>& lines);

  // Find the top_k best matches for the given query.
  // Returns results sorted by score (highest first) using quicksort.
  std::vector<MatchResult> search(const std::string& query, std::size_t top_k) const;

private:
  const std::vector<std::string>* lines_ = nullptr;

  // Forward declaration for the PIMPL pattern — keeps hash_table.hpp
  // out of this header so consumers don't need to include it.
  struct TrigramIndex;
  struct TrigramIndexImpl;
  TrigramIndexImpl* index_ = nullptr;

  // Score a single line against the full query (all tokens).
  // Returns a MatchResult with score=0 if any token fails to match.
  MatchResult score_line(std::size_t line_idx, const std::string& line, const std::string& query) const;
};

} // namespace fdr
