#pragma once

#include <string>
#include <vector>

namespace fdr {

struct MatchResult;

enum class Key {
  CHAR, // Regular character
  BACKSPACE,
  ENTER,
  ESCAPE,
  ARROW_UP,
  ARROW_DOWN,
  TAB,
  CTRL_C,
  UNKNOWN,
};

struct KeyEvent {
  Key type;
  char ch; // Only valid when type == CHAR
};

class Terminal {
public:
  Terminal();
  ~Terminal();

  // Enter/exit raw mode
  void enable_raw_mode();
  void disable_raw_mode();

  // Read a single key event (blocking)
  KeyEvent read_key();

  // Get terminal dimensions
  int get_width() const;
  int get_height() const;

  // Rendering
  void clear_screen();
  void move_cursor(int row, int col);
  void render_frame(const std::string& query, const std::vector<MatchResult>& results,
                    const std::vector<std::string>& lines, std::size_t selected_idx,
                    std::size_t total_lines);

  // Write output to stderr (so stdout stays clean for selected result)
  void write_stderr(const std::string& text);

private:
  bool raw_mode_enabled_ = false;

#ifdef _WIN32
  void* original_stdin_mode_ = nullptr;
  void* original_stdout_mode_ = nullptr;
#else
  struct TermiosState;
  TermiosState* original_termios_ = nullptr;
#endif
};

} // namespace fdr
