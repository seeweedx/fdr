// fdr — Fuzzy text search tool for the command line
//
// Entry point for the interactive fuzzy finder. The program works in
// two phases:
//
// Phase 1 — Data ingestion:
//   Reads all lines from stdin (piped input), builds a trigram index
//   over those lines using FuzzyMatcher::build_index().
//
// Phase 2 — Interactive search:
//   Reopens stdin from the terminal (since the original stdin was a pipe),
//   enters raw mode, and runs an event loop that re-searches on every
//   keystroke, displaying ranked results in real time.
//
// The selected line is printed to stdout on Enter, making fdr composable
// with other shell commands: selected=$(ls | fdr)
//
// Exit codes:
//   0   — line selected successfully
//   1   — no input or no selection
//   130 — cancelled by user (Escape / Ctrl-C)

#include "fuzzy.hpp"
#include "terminal.hpp"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

// Cross-platform isatty/fileno: detect whether stdin is a pipe or terminal.
// On Windows, these functions have underscore prefixes.
#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

int main() {
  // --- Phase 1: Read piped input ---

  std::vector<std::string> lines;
  std::string line;

  // Guard: fdr requires piped input. If stdin is a terminal (no pipe),
  // the user ran "fdr" directly without piping data into it.
  if (isatty(fileno(stdin))) {
    std::fprintf(stderr, "Usage: <command> | fdr\n");
    std::fprintf(stderr, "Example: ls -la | fdr\n");
    return 1;
  }

  // Read all lines from the pipe into memory (skip empty lines)
  while (std::getline(std::cin, line)) {
    if (!line.empty())
      lines.push_back(std::move(line));
  }

  if (lines.empty()) {
    std::fprintf(stderr, "No input received.\n");
    return 1;
  }

  // Build the trigram index for fast fuzzy matching.
  // This preprocesses all lines once so that each search query
  // can quickly narrow down candidates via hash table lookups.
  fdr::FuzzyMatcher matcher;
  matcher.build_index(lines);

  // --- Phase 2: Interactive search ---

  // Reopen stdin from the actual terminal device so we can read keystrokes.
  // The original stdin was a pipe (exhausted after reading all lines).
  // Unix: /dev/tty always refers to the controlling terminal.
  // Windows: "CON" is the console device.
#ifdef _WIN32
  std::freopen("CON", "r", stdin);
#else
  std::freopen("/dev/tty", "r", stdin);
#endif

  // Set up terminal for interactive use (raw mode + alternate screen)
  fdr::Terminal term;
  term.enable_raw_mode();

  std::string query;
  std::size_t selected = 0; // Index into results (0 = best match)
  bool done = false;
  bool cancelled = false;

  // Show all lines initially (no query = no filtering)
  auto results = matcher.search(query, lines.size());
  term.render_frame(query, results, lines, selected, lines.size());

  // Main event loop: read key → update state → re-search → re-render
  while (!done) {
    auto key = term.read_key();

    switch (key.type) {
    case fdr::Key::CHAR:
      query += key.ch;   // Append typed character to query
      selected = 0;      // Reset selection to best match
      break;

    case fdr::Key::BACKSPACE:
      if (!query.empty()) {
        query.pop_back(); // Delete last character from query
        selected = 0;     // Reset selection to best match
      }
      break;

    case fdr::Key::ARROW_UP:
      // Move selection upward (toward lower-scoring results)
      if (selected + 1 < results.size())
        ++selected;
      break;

    case fdr::Key::ARROW_DOWN:
      // Move selection downward (toward higher-scoring results)
      if (selected > 0)
        --selected;
      break;

    case fdr::Key::ENTER:
      done = true; // Confirm selection
      break;

    case fdr::Key::ESCAPE:
    case fdr::Key::CTRL_C:
      done = true;
      cancelled = true; // Exit without selecting
      break;

    default:
      break;
    }

    if (!done) {
      // Re-run search with updated query on every keystroke
      results = matcher.search(query, lines.size());
      // Clamp selection if results shrunk (e.g., stricter query)
      if (selected >= results.size() && !results.empty())
        selected = results.size() - 1;
      term.render_frame(query, results, lines, selected, lines.size());
    }
  }

  // Restore terminal to normal mode (leaves alternate screen buffer,
  // restoring the user's previous terminal content)
  term.disable_raw_mode();

  // Print selected line to stdout (the only stdout output from fdr).
  // This makes fdr composable: selected=$(ls | fdr)
  if (!cancelled && !results.empty() && selected < results.size()) {
    std::cout << lines[results[selected].line_index] << std::endl;
    return 0;
  }

  // Return 130 for user cancellation (convention matching Ctrl-C behavior),
  // or 1 if no results were available to select
  return cancelled ? 130 : 1;
}
