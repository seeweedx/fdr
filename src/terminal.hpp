#pragma once

// Cross-Platform Terminal Handling
//
// Provides raw-mode terminal input and ANSI-based rendering for the
// interactive fuzzy finder UI. Abstracts platform differences between
// Unix (POSIX termios) and Windows (Console API) behind a single interface.
//
// Key responsibilities:
//   - Raw mode: disable line buffering and echo so we receive each
//     keypress immediately instead of waiting for Enter
//   - Key reading: parse raw bytes / console events into structured
//     KeyEvent objects (handles escape sequences for arrow keys)
//   - Rendering: draw the search UI (results, status, prompt) using
//     ANSI escape codes, with bottom-aligned layout and scrolling
//   - Alternate screen buffer: preserves the user's previous terminal
//     content while fdr is running, restoring it on exit
//
// All output goes to stderr so that stdout remains clean for the
// selected result (which gets piped or captured by the shell).

#include <string>
#include <vector>

namespace fdr {

struct MatchResult; // Forward declaration (defined in fuzzy.hpp)

// Enumeration of key types the application cares about.
// CHAR holds a printable ASCII character; others are special keys.
enum class Key {
  CHAR,       // Regular printable character (a-z, 0-9, symbols)
  BACKSPACE,  // Delete previous character
  ENTER,      // Confirm selection
  ESCAPE,     // Cancel and exit
  ARROW_UP,   // Navigate to previous result
  ARROW_DOWN, // Navigate to next result
  TAB,        // (Reserved for future use)
  CTRL_C,     // Cancel and exit (same as ESCAPE)
  UNKNOWN,    // Unrecognized input (ignored)
};

// A parsed key event: the type of key pressed and, for CHAR events,
// which character it was.
struct KeyEvent {
  Key type;
  char ch; // Only meaningful when type == Key::CHAR
};

// Terminal manages raw mode, input reading, and screen rendering.
// Create one instance, call enable_raw_mode() to start interactive mode,
// and disable_raw_mode() when done.
class Terminal {
public:
  Terminal();
  ~Terminal(); // Automatically disables raw mode if still enabled

  // Enter raw mode: disables line buffering and echo, enables
  // alternate screen buffer to preserve terminal history.
  void enable_raw_mode();

  // Exit raw mode: restores original terminal settings and
  // leaves the alternate screen buffer.
  void disable_raw_mode();

  // Read a single key event (blocks until a key is pressed).
  // On Unix, uses a 100ms timeout after ESC to distinguish
  // standalone Escape from multi-byte escape sequences (arrow keys).
  KeyEvent read_key();

  // Query terminal dimensions (columns and rows)
  int get_width() const;
  int get_height() const;

  // Clear entire screen and move cursor to top-left
  void clear_screen();

  // Move cursor to a specific row and column (1-based)
  void move_cursor(int row, int col);

  // Render a complete frame of the search UI:
  //   - Results list with highlighted matched characters
  //   - Status line showing match count / total lines
  //   - Prompt line with current query text
  // Layout is bottom-aligned (prompt at bottom, results above).
  void render_frame(const std::string& query, const std::vector<MatchResult>& results,
                    const std::vector<std::string>& lines, std::size_t selected_idx,
                    std::size_t total_lines);

  // Write text to stderr (bypasses stdout to keep it clean for output)
  void write_stderr(const std::string& text);

private:
  bool raw_mode_enabled_ = false;

  // Platform-specific state for restoring original terminal settings.
  // On Windows: saved console mode flags.
  // On Unix: saved termios struct via PIMPL to avoid including termios.h.
#ifdef _WIN32
  void* original_stdin_mode_ = nullptr;
  void* original_stdout_mode_ = nullptr;
#else
  struct TermiosState;
  TermiosState* original_termios_ = nullptr;
#endif
};

} // namespace fdr
