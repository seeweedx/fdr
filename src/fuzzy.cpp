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

struct FuzzyMatcher::TrigramIndexImpl {
  // Maps trigram (3-char string) -> vector of line indices containing it
  HashTable<std::string, std::vector<std::size_t>> trigrams;
};

void FuzzyMatcher::build_index(const std::vector<std::string>& lines) {
  lines_ = &lines;

  delete index_;
  index_ = new TrigramIndexImpl();

  for (std::size_t i = 0; i < lines.size(); ++i) {
    const std::string& line = lines[i];
    // Generate lowercase trigrams for case-insensitive matching
    if (line.size() < 3) {
      // For short lines, index the whole line as-is
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
      for (std::size_t j = 0; j + 3 <= line.size(); ++j) {
        std::string trigram;
        trigram.reserve(3);
        for (int k = 0; k < 3; ++k)
          trigram += static_cast<char>(std::tolower(static_cast<unsigned char>(line[j + k])));

        auto* existing = index_->trigrams.find(trigram);
        if (existing) {
          // Avoid duplicate line indices for same trigram
          if (existing->empty() || existing->back() != i)
            existing->push_back(i);
        } else {
          index_->trigrams.insert(trigram, {i});
        }
      }
    }
  }
}

// Split string by spaces into non-empty tokens
static std::vector<std::string> split_tokens(const std::string& s) {
  std::vector<std::string> tokens;
  std::size_t start = 0;
  while (start < s.size()) {
    while (start < s.size() && s[start] == ' ')
      ++start;
    if (start >= s.size())
      break;
    std::size_t end = start;
    while (end < s.size() && s[end] != ' ')
      ++end;
    tokens.push_back(s.substr(start, end - start));
    start = end;
  }
  return tokens;
}

// Score a contiguous substring match starting at position start
static int score_substring(const std::string& line, const std::string& token,
                           std::size_t start,
                           std::vector<std::size_t>& match_positions) {
  int score = 0;
  for (std::size_t i = 0; i < token.size(); ++i) {
    std::size_t li = start + i;
    match_positions.push_back(li);

    int points = 16; // Higher base for contiguous match
    points += static_cast<int>(i) * 5; // Consecutive bonus grows

    if (line[li] == token[i])
      points += 2;

    if (line.size() > 0)
      points += static_cast<int>((line.size() - li) * 2 / line.size());

    if (li == 0 || line[li - 1] == '/' || line[li - 1] == '\\' ||
        line[li - 1] == '_' || line[li - 1] == '-' ||
        line[li - 1] == '.' || line[li - 1] == ' ')
      points += 8;

    score += points;
  }
  return score;
}

// Score a single token against a line.
// Tries substring match first, falls back to subsequence.
static int score_token(const std::string& line, const std::string& token,
                       std::vector<std::size_t>& match_positions) {
  // First: try to find best contiguous substring match
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
      std::vector<std::size_t> tmp;
      int s = score_substring(line, token, i, tmp);
      if (s > best_sub_score) {
        best_sub_score = s;
        best_sub_start = i;
      }
    }
  }

  if (best_sub_score > 0) {
    score_substring(line, token, best_sub_start, match_positions);
    return best_sub_score;
  }

  // Fallback: subsequence matching
  int score = 0;
  std::size_t qi = 0;
  int consecutive_bonus = 0;

  for (std::size_t li = 0; li < line.size() && qi < token.size(); ++li) {
    char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(line[li])));
    char qc = token[qi];

    if (lc == qc) {
      match_positions.push_back(li);

      int points = 10;

      if (consecutive_bonus > 0)
        points += consecutive_bonus * 5;
      consecutive_bonus++;

      if (line[li] == token[qi])
        points += 2;

      if (line.size() > 0)
        points += static_cast<int>((line.size() - li) * 2 / line.size());

      if (li == 0 || line[li - 1] == '/' || line[li - 1] == '\\' ||
          line[li - 1] == '_' || line[li - 1] == '-' ||
          line[li - 1] == '.' || line[li - 1] == ' ')
        points += 8;

      score += points;
      ++qi;
    } else {
      consecutive_bonus = 0;
    }
  }

  if (qi < token.size())
    return 0;

  return score;
}

MatchResult FuzzyMatcher::score_line(std::size_t line_idx, const std::string& line, const std::string& query) const {
  MatchResult result;
  result.line_index = line_idx;
  result.score = 0;

  // Split query into space-separated tokens; ALL must match (any order)
  std::vector<std::string> tokens = split_tokens(query);
  if (tokens.empty())
    return result;

  for (const auto& token : tokens) {
    int token_score = score_token(line, token, result.match_positions);
    if (token_score == 0) {
      result.score = 0;
      result.match_positions.clear();
      return result;
    }
    result.score += token_score;
  }

  // Sort and deduplicate match positions so highlighting works correctly
  std::sort(result.match_positions.begin(), result.match_positions.end());
  result.match_positions.erase(
    std::unique(result.match_positions.begin(), result.match_positions.end()),
    result.match_positions.end());

  return result;
}

std::vector<MatchResult> FuzzyMatcher::search(const std::string& query,
                                              std::size_t top_k) const {
  if (!lines_ || query.empty()) {
    // Return all lines with score 0 when no query
    std::vector<MatchResult> results;
    if (lines_) {
      std::size_t count = std::min(top_k, lines_->size());
      for (std::size_t i = 0; i < count; ++i)
        results.push_back({i, 0, {}});
    }
    return results;
  }

  // Build lowercase query
  std::string lower_query;
  lower_query.reserve(query.size());
  for (char c : query)
    lower_query += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

  // Split into tokens for trigram candidate gathering
  std::vector<std::string> tokens = split_tokens(lower_query);

  // Gather candidate line indices using trigram index + red-black tree
  RBTree<std::size_t> candidates;

  bool has_long_token = false;
  if (index_) {
    for (const auto& token : tokens) {
      if (token.size() >= 3) {
        has_long_token = true;
        for (std::size_t i = 0; i + 3 <= token.size(); ++i) {
          std::string trigram = token.substr(i, 3);
          auto* line_indices = index_->trigrams.find(trigram);
          if (line_indices) {
            for (std::size_t idx : *line_indices)
              candidates.insert(idx);
          }
        }
      }
    }
  }

  if (!has_long_token) {
    // All tokens are short: check all lines
    for (std::size_t i = 0; i < lines_->size(); ++i)
      candidates.insert(i);
  }

  // Score candidates and keep top-K using a min-heap
  // Min-heap so we can evict the lowest-scoring result when full
  auto cmp = [](const MatchResult& a, const MatchResult& b) {
    return a.score < b.score;
  };
  MinHeap<MatchResult, decltype(cmp)> heap(cmp);

  candidates.for_each([&](const std::size_t& idx) {
    MatchResult result = score_line(idx, (*lines_)[idx], lower_query);
    if (result.score <= 0)
      return;

    if (heap.size() < top_k) {
      heap.push(std::move(result));
    } else if (result.score > heap.top().score) {
      heap.replace_top(std::move(result));
    }
  });

  // Extract results and sort by score descending using quicksort
  std::vector<MatchResult> results;
  results.reserve(heap.size());
  while (!heap.empty())
    results.push_back(heap.pop());

  quicksort(results, [](const MatchResult& a, const MatchResult& b) {
    return a.score > b.score; // Descending
  });

  return results;
}

} // namespace fdr
