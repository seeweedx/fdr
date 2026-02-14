#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace fdr {

struct MatchResult {
  std::size_t line_index;
  int score;
  std::vector<std::size_t> match_positions; // positions of matched chars in the line
};

class FuzzyMatcher {
public:
  // Build the trigram index over the input lines
  void build_index(const std::vector<std::string>& lines);

  // Find top_k matches for the given query
  std::vector<MatchResult> search(const std::string& query, std::size_t top_k) const;

private:
  const std::vector<std::string>* lines_ = nullptr;

  // Trigram index: maps trigram string -> list of line indices
  // Stored as parallel vectors for the hash table
  struct TrigramIndex;
  struct TrigramIndexImpl;
  TrigramIndexImpl* index_ = nullptr;

  // Score a single line against the query
  MatchResult score_line(std::size_t line_idx, const std::string& line, const std::string& query) const;
};

} // namespace fdr
