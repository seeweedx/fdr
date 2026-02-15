#include "fuzzy.hpp"
#include "hash_table.hpp"
#include "min_heap.hpp"
#include "quicksort.hpp"
#include "rbtree.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace fdr {

// PIMPL implementation: the actual trigram index is a HashTable
// mapping 3-character strings to vectors of line indices.
// This keeps the hash_table.hpp dependency out of the public header.
struct FuzzyMatcher::TrigramIndexImpl {
  HashTable<std::string, std::vector<std::size_t>> trigrams;
};

// Build the trigram index for all input lines.
//
// For each line, we extract every 3-character window (trigram) in
// lowercase and store the line's index in the hash table. This allows
// O(1) lookup during search to quickly find which lines contain a
// given trigram, avoiding a full O(n) scan of all lines.
//
// Example: line "Hello" at index 5 produces trigrams:
//   "hel" -> [5], "ell" -> [5], "llo" -> [5]
//
// Lines shorter than 3 characters are indexed using their full text
// as the key, since no trigrams can be extracted from them.
void FuzzyMatcher::build_index(const std::vector<std::string>& lines) {
  lines_ = &lines;

  delete index_;
  index_ = new TrigramIndexImpl();

  for (std::size_t i = 0; i < lines.size(); ++i) {
    const std::string& line = lines[i];
    // All trigrams are stored in lowercase for case-insensitive matching
    if (line.size() < 3) {
      // Short lines (1-2 chars): index the whole string as a key
      std::string lower;
      lower.reserve(line.size());
      for (char c : line)
        lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if (!lower.empty()) {
        auto* existing = index_->trigrams.find(lower);
        if (existing) {
          existing->push_back(i);
        } else {
          index_->trigrams.insert(lower, {i});
        }
      }
    } else {
      // Slide a 3-character window across the line to extract trigrams
      for (std::size_t j = 0; j + 3 <= line.size(); ++j) {
        std::string trigram;
        trigram.reserve(3);
        for (int k = 0; k < 3; ++k)
          trigram += static_cast<char>(std::tolower(static_cast<unsigned char>(line[j + k])));

        auto* existing = index_->trigrams.find(trigram);
        if (existing) {
          // Only add line index if it's not already the last entry
          // (same line produces multiple overlapping trigrams)
          if (existing->empty() || existing->back() != i)
            existing->push_back(i);
        } else {
          index_->trigrams.insert(trigram, {i});
        }
      }
    }
  }
}

// Split a query string by spaces into individual non-empty tokens.
// This enables multi-token matching where "foo bar" finds lines
// containing both "foo" and "bar" in any order.
static std::vector<std::string> split_tokens(const std::string& s) {
  std::vector<std::string> tokens;
  std::size_t start = 0;
  while (start < s.size()) {
    // Skip leading spaces
    while (start < s.size() && s[start] == ' ')
      ++start;
    if (start >= s.size())
      break;
    // Find end of current token
    std::size_t end = start;
    while (end < s.size() && s[end] != ' ')
      ++end;
    tokens.push_back(s.substr(start, end - start));
    start = end;
  }
  return tokens;
}

// Score a contiguous (substring) match of token starting at position `start`
// in the line. Contiguous matches get higher scores than scattered subsequences
// because they indicate a more relevant result.
//
// Scoring factors for each matched character:
//   - Base:          16 points (higher than subsequence's 10)
//   - Consecutive:   +5 per position in the run (rewards longer contiguous matches)
//   - Case exact:    +2 if original case matches exactly
//   - Position:      bonus for matches earlier in the line
//   - Word boundary: +8 if character follows a separator (/, \, _, -, ., space)
//                    or is at the start of the line
static int score_substring(const std::string& line, const std::string& token,
                           std::size_t start,
                           std::vector<std::size_t>& match_positions) {
  int score = 0;
  for (std::size_t i = 0; i < token.size(); ++i) {
    std::size_t li = start + i;
    match_positions.push_back(li); // Record position for highlighting

    int points = 16;                   // Higher base for contiguous match
    points += static_cast<int>(i) * 5; // Consecutive bonus grows with run length

    if (line[li] == token[i])
      points += 2; // Case-exact bonus

    if (line.size() > 0)
      points += static_cast<int>((line.size() - li) * 2 / line.size()); // Earlier = better

    // Word boundary bonus: matches at start of path segments or words
    // score higher (e.g., matching "main" at start of "main.cpp")
    if (li == 0 || line[li - 1] == '/' || line[li - 1] == '\\' ||
        line[li - 1] == '_' || line[li - 1] == '-' ||
        line[li - 1] == '.' || line[li - 1] == ' ')
      points += 8;

    score += points;
  }
  return score;
}

// Score a single token against a line using a two-phase strategy:
//
// Phase 1 — Substring match (preferred):
//   Slide the token across the line looking for a case-insensitive
//   contiguous match. If found, score it with score_substring() which
//   gives higher base points. We try all positions and keep the
//   highest-scoring one (e.g., a word-boundary match scores higher
//   than one in the middle of a word).
//
// Phase 2 — Subsequence match (fallback):
//   If no contiguous match exists, try matching token characters in
//   order but not necessarily adjacent. For example, "edtr" matches
//   "editor" via e-d-t-r scattered across the string.
//   Returns 0 if not all token characters can be matched.
//
// The substring-first approach prevents misleading highlights where
// a greedy subsequence picks up early scattered characters instead
// of an obvious contiguous match.
static int score_token(const std::string& line, const std::string& token,
                       std::vector<std::size_t>& match_positions) {
  // Phase 1: Try every position for a contiguous substring match
  int best_sub_score = 0;
  std::size_t best_sub_start = 0;
  for (std::size_t i = 0; i + token.size() <= line.size(); ++i) {
    bool match = true;
    for (std::size_t j = 0; j < token.size(); ++j) {
      char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(line[i + j])));
      if (lc != token[j]) {
        match = false;
        break;
      }
    }
    if (match) {
      // Score this candidate position and keep the best one
      std::vector<std::size_t> tmp;
      int s = score_substring(line, token, i, tmp);
      if (s > best_sub_score) {
        best_sub_score = s;
        best_sub_start = i;
      }
    }
  }

  if (best_sub_score > 0) {
    // Use the best substring match position
    score_substring(line, token, best_sub_start, match_positions);
    return best_sub_score;
  }

  // Phase 2: Subsequence matching — characters must appear in order
  // but can have gaps between them. Less precise than substring but
  // catches fuzzy/typo matches like "edtr" → "editor".
  int score = 0;
  std::size_t qi = 0;        // Index into the token
  int consecutive_bonus = 0; // Tracks consecutive character matches

  for (std::size_t li = 0; li < line.size() && qi < token.size(); ++li) {
    char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(line[li])));
    char qc = token[qi];

    if (lc == qc) {
      match_positions.push_back(li);

      int points = 10; // Lower base than substring match

      if (consecutive_bonus > 0)
        points += consecutive_bonus * 5; // Reward consecutive runs
      consecutive_bonus++;

      if (line[li] == token[qi])
        points += 2; // Case-exact bonus

      if (line.size() > 0)
        points += static_cast<int>((line.size() - li) * 2 / line.size());

      // Word boundary bonus (same as substring scoring)
      if (li == 0 || line[li - 1] == '/' || line[li - 1] == '\\' ||
          line[li - 1] == '_' || line[li - 1] == '-' ||
          line[li - 1] == '.' || line[li - 1] == ' ')
        points += 8;

      score += points;
      ++qi;
    } else {
      consecutive_bonus = 0; // Reset streak on gap
    }
  }

  // If we didn't match all token characters, the line doesn't match
  if (qi < token.size())
    return 0;

  return score;
}

// Score a complete line against the full query (which may contain
// multiple space-separated tokens). ALL tokens must match for the
// line to be considered a result (AND logic). The total score is the
// sum of individual token scores.
//
// After scoring all tokens, match_positions are sorted and deduplicated
// so the renderer can correctly highlight matched characters in order.
MatchResult FuzzyMatcher::score_line(std::size_t line_idx, const std::string& line, const std::string& query) const {
  MatchResult result;
  result.line_index = line_idx;
  result.score = 0;

  std::vector<std::string> tokens = split_tokens(query);
  if (tokens.empty())
    return result;

  // Each token must match — if any token scores 0, the whole line fails
  for (const auto& token : tokens) {
    int token_score = score_token(line, token, result.match_positions);
    if (token_score == 0) {
      result.score = 0;
      result.match_positions.clear();
      return result; // AND logic: all tokens must match
    }
    result.score += token_score;
  }

  // Multiple tokens may highlight overlapping or out-of-order positions.
  // Sort and deduplicate so the renderer iterates positions sequentially.
  std::sort(result.match_positions.begin(), result.match_positions.end());
  result.match_positions.erase(
      std::unique(result.match_positions.begin(), result.match_positions.end()),
      result.match_positions.end());

  return result;
}

// Main search method — orchestrates all 5 data structures:
//
// 1. HASH TABLE: look up trigrams from the query to find candidate lines
// 2. RED-BLACK TREE: collect candidate line indices with automatic dedup
// 3. Score each candidate line against the query
// 4. MIN-HEAP: maintain only the top-K highest-scoring results efficiently
// 5. QUICKSORT: sort the final K results by score for display
//
// When the query is empty, returns all lines (up to top_k) unscored
// so the user sees the full input list before typing.
std::vector<MatchResult> FuzzyMatcher::search(const std::string& query,
                                              std::size_t top_k) const {
  if (!lines_ || query.empty()) {
    // No query: return all lines in original order with score 0
    std::vector<MatchResult> results;
    if (lines_) {
      std::size_t count = std::min(top_k, lines_->size());
      for (std::size_t i = 0; i < count; ++i)
        results.push_back({i, 0, {}});
    }
    return results;
  }

  // Convert query to lowercase for case-insensitive matching
  std::string lower_query;
  lower_query.reserve(query.size());
  for (char c : query)
    lower_query += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

  std::vector<std::string> tokens = split_tokens(lower_query);

  // --- Step 1 & 2: Candidate gathering ---
  // Extract trigrams from each query token and look them up in the
  // hash table to find which lines contain those substrings.
  // The red-black tree automatically deduplicates line indices
  // (a line matching multiple trigrams is only scored once).
  RBTree<std::size_t> candidates;

  bool has_long_token = false;
  if (index_) {
    for (const auto& token : tokens) {
      if (token.size() >= 3) {
        has_long_token = true;
        // Extract trigrams from this token and look up each one
        for (std::size_t i = 0; i + 3 <= token.size(); ++i) {
          std::string trigram = token.substr(i, 3);
          auto* line_indices = index_->trigrams.find(trigram);
          if (line_indices) {
            for (std::size_t idx : *line_indices)
              candidates.insert(idx); // RBTree deduplicates automatically
          }
        }
      }
    }
  }

  // If all tokens are shorter than 3 characters, we can't use the
  // trigram index — fall back to checking every line
  if (!has_long_token) {
    for (std::size_t i = 0; i < lines_->size(); ++i)
      candidates.insert(i);
  }

  // --- Step 3 & 4: Score candidates, keep top-K with min-heap ---
  // The min-heap keeps the K best results. The minimum (weakest) score
  // is at the root, so when a better candidate arrives, we evict the
  // root using replace_top() — a single O(log K) sift instead of
  // separate pop + push operations.
  auto cmp = [](const MatchResult& a, const MatchResult& b) {
    return a.score < b.score;
  };
  MinHeap<MatchResult, decltype(cmp)> heap(cmp);

  candidates.for_each([&](const std::size_t& idx) {
    MatchResult result = score_line(idx, (*lines_)[idx], lower_query);
    if (result.score <= 0)
      return; // No match — skip

    if (heap.size() < top_k) {
      heap.push(std::move(result)); // Heap not full yet — add
    } else if (result.score > heap.top().score) {
      heap.replace_top(std::move(result)); // Better than worst — evict
    }
  });

  // --- Step 5: Sort final results with quicksort ---
  // Extract all results from the heap and sort descending by score
  // so the best match appears at the top of the display.
  std::vector<MatchResult> results;
  results.reserve(heap.size());
  while (!heap.empty())
    results.push_back(heap.pop());

  quicksort(results, [](const MatchResult& a, const MatchResult& b) {
    return a.score > b.score; // Descending order (best match first)
  });

  return results;
}

} // namespace fdr
