#include "fuzzy.hpp"
#include "terminal.hpp"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

int main() {
  // Read all lines from stdin
  std::vector<std::string> lines;
  std::string line;

  // Check if stdin is a pipe
  if (isatty(fileno(stdin))) {
    std::fprintf(stderr, "Usage: <command> | fdr\n");
    std::fprintf(stderr, "Example: ls -la | fdr\n");
    return 1;
  }

  while (std::getline(std::cin, line)) {
    if (!line.empty())
      lines.push_back(std::move(line));
  }

  if (lines.empty()) {
    std::fprintf(stderr, "No input received.\n");
    return 1;
  }

  // Build fuzzy index
  fdr::FuzzyMatcher matcher;
  matcher.build_index(lines);

  // Reopen stdin from terminal for interactive input
#ifdef _WIN32
  std::freopen("CON", "r", stdin);
#else
  std::freopen("/dev/tty", "r", stdin);
#endif

  // Enter interactive mode
  fdr::Terminal term;
  term.enable_raw_mode();

  std::string query;
  std::size_t selected = 0;
  bool done = false;
  bool cancelled = false;

  // Initial render - fetch all matches, not just screen-sized
  auto results = matcher.search(query, lines.size());
  term.render_frame(query, results, lines, selected, lines.size());

  while (!done) {
    auto key = term.read_key();

    switch (key.type) {
    case fdr::Key::CHAR:
      query += key.ch;
      selected = 0;
      break;

    case fdr::Key::BACKSPACE:
      if (!query.empty()) {
        query.pop_back();
        selected = 0;
      }
      break;

    case fdr::Key::ARROW_UP:
      if (selected + 1 < results.size())
        ++selected;
      break;

    case fdr::Key::ARROW_DOWN:
      if (selected > 0)
        --selected;
      break;

    case fdr::Key::ENTER:
      done = true;
      break;

    case fdr::Key::ESCAPE:
    case fdr::Key::CTRL_C:
      done = true;
      cancelled = true;
      break;

    default:
      break;
    }

    if (!done) {
      results = matcher.search(query, lines.size());
      if (selected >= results.size() && !results.empty())
        selected = results.size() - 1;
      term.render_frame(query, results, lines, selected, lines.size());
    }
  }

  // Cleanup terminal (alternate screen buffer restores previous content)
  term.disable_raw_mode();

  // Output selected line to stdout
  if (!cancelled && !results.empty() && selected < results.size()) {
    std::cout << lines[results[selected].line_index] << std::endl;
    return 0;
  }

  return cancelled ? 130 : 1;
}
