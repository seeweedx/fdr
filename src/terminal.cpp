#include "terminal.hpp"
#include "fuzzy.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used Windows headers
#define NOMINMAX            // Prevent min/max macros conflicting with std::min/max
#include <windows.h>
#else
#include <sys/ioctl.h> // ioctl() + TIOCGWINSZ for terminal size
#include <termios.h>   // termios struct for raw mode configuration
#include <unistd.h>    // read(), STDIN_FILENO, STDERR_FILENO
#endif

namespace fdr {

#ifndef _WIN32
// PIMPL storage for the original terminal settings.
// We save these before entering raw mode so we can restore them on exit.
struct Terminal::TermiosState {
  struct termios original;
};
#endif

Terminal::Terminal() {}

// Destructor ensures raw mode is disabled even if the caller forgets,
// preventing the terminal from being left in an unusable state.
Terminal::~Terminal() {
  if (raw_mode_enabled_)
    disable_raw_mode();
#ifndef _WIN32
  delete original_termios_;
#endif
}

// Enter raw mode: configures the terminal for character-by-character
// input without echo, and switches to an alternate screen buffer.
//
// Unix (termios):
//   - ECHO off: don't display typed characters (we render them ourselves)
//   - ICANON off: deliver input immediately (not line-by-line)
//   - ISIG off: don't generate signals for Ctrl-C (we handle it as a key)
//   - IEXTEN off: disable implementation-defined input processing
//   - Various input flags off: disable flow control, break handling, etc.
//   - OPOST off: disable output post-processing (we control all output)
//   - VMIN=1, VTIME=0: read() blocks until at least 1 byte available
//
// Windows (Console API):
//   - ENABLE_WINDOW_INPUT only: raw key events via ReadConsoleInput
//   - ENABLE_VIRTUAL_TERMINAL_PROCESSING: enable ANSI escape codes
void Terminal::enable_raw_mode() {
  if (raw_mode_enabled_)
    return;

#ifdef _WIN32
  HANDLE h_stdin = GetStdHandle(STD_INPUT_HANDLE);
  HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);

  // Save original modes so we can restore them later
  DWORD stdin_mode, stdout_mode;
  GetConsoleMode(h_stdin, &stdin_mode);
  GetConsoleMode(h_stdout, &stdout_mode);

  original_stdin_mode_ = reinterpret_cast<void*>(static_cast<uintptr_t>(stdin_mode));
  original_stdout_mode_ = reinterpret_cast<void*>(static_cast<uintptr_t>(stdout_mode));

  // Disable line input and echo; enable raw key event reading
  SetConsoleMode(h_stdin, ENABLE_WINDOW_INPUT);
  // Enable ANSI escape code processing for colored output
  SetConsoleMode(h_stdout, stdout_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN);
#else
  if (!original_termios_)
    original_termios_ = new TermiosState();

  // Save current terminal settings for later restoration
  tcgetattr(STDIN_FILENO, &original_termios_->original);

  struct termios raw = original_termios_->original;
  // Local flags: disable echo, canonical mode, signals, extended input
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  // Input flags: disable break, CR-to-NL, parity, strip, flow control
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  // Output flags: disable post-processing (we write raw ANSI output)
  raw.c_oflag &= ~(OPOST);
  // Control flags: set 8-bit characters
  raw.c_cflag |= (CS8);
  // Blocking read: wait for at least 1 byte, no timeout
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
#endif

  raw_mode_enabled_ = true;

  // Switch to alternate screen buffer — this preserves the user's
  // previous terminal content and restores it when we exit
  write_stderr("\x1b[?1049h");
}

// Exit raw mode: leave the alternate screen buffer (restoring previous
// terminal content) and restore the original terminal settings.
void Terminal::disable_raw_mode() {
  if (!raw_mode_enabled_)
    return;

  // Leave alternate screen buffer — restores the user's previous
  // terminal content (scrollback, command history, etc.)
  write_stderr("\x1b[?1049l");

#ifdef _WIN32
  // Restore original Windows console modes
  HANDLE h_stdin = GetStdHandle(STD_INPUT_HANDLE);
  HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleMode(h_stdin, static_cast<DWORD>(reinterpret_cast<uintptr_t>(original_stdin_mode_)));
  SetConsoleMode(h_stdout, static_cast<DWORD>(reinterpret_cast<uintptr_t>(original_stdout_mode_)));
#else
  // Restore original Unix termios settings
  if (original_termios_)
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios_->original);
#endif

  raw_mode_enabled_ = false;
}

// Read a single key event from the terminal.
//
// Windows: uses ReadConsoleInput() which returns structured INPUT_RECORD
// events with virtual key codes — no need to parse escape sequences.
// We loop to skip non-key events (window resize, mouse, etc.).
//
// Unix: reads raw bytes from stdin. Most keys are a single byte, but
// arrow keys are 3-byte escape sequences: ESC [ A (up), ESC [ B (down).
// The challenge is distinguishing a standalone Escape press from the
// start of an escape sequence. We solve this with a 100ms timeout:
// after reading ESC, we briefly switch to non-blocking mode and try
// to read 2 more bytes. If nothing arrives within 100ms, it was
// a standalone Escape. If bytes arrive, it's an escape sequence.
KeyEvent Terminal::read_key() {
#ifdef _WIN32
  HANDLE h_stdin = GetStdHandle(STD_INPUT_HANDLE);
  while (true) {
    INPUT_RECORD ir;
    DWORD count;
    if (!ReadConsoleInput(h_stdin, &ir, 1, &count) || count == 0)
      return {Key::ESCAPE, 0};

    // Skip non-key events and key-up events
    if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown)
      continue;

    // Map Windows virtual key codes to our Key enum
    WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
    char ch = ir.Event.KeyEvent.uChar.AsciiChar;

    if (vk == VK_UP)
      return {Key::ARROW_UP, 0};
    if (vk == VK_DOWN)
      return {Key::ARROW_DOWN, 0};
    if (vk == VK_RETURN)
      return {Key::ENTER, 0};
    if (vk == VK_ESCAPE)
      return {Key::ESCAPE, 0};
    if (vk == VK_BACK)
      return {Key::BACKSPACE, 0};
    if (vk == VK_TAB)
      return {Key::TAB, 0};

    if (ch == 3) // ASCII 3 = Ctrl-C
      return {Key::CTRL_C, 0};

    // Printable ASCII characters (space through tilde)
    if (ch >= 32 && ch < 127)
      return {Key::CHAR, ch};
  }
#else
  // Unix: read one byte at a time
  char c;
  if (::read(STDIN_FILENO, &c, 1) <= 0)
    return {Key::ESCAPE, 0};

  if (c == '\x1b') {
    // Got ESC byte — could be standalone Escape or start of escape sequence.
    // Temporarily switch to non-blocking mode with 100ms timeout to check.
    char seq[2];
    struct termios current;
    tcgetattr(STDIN_FILENO, &current);
    struct termios timeout_mode = current;
    timeout_mode.c_cc[VMIN] = 0;  // Don't require any bytes
    timeout_mode.c_cc[VTIME] = 1; // 100ms timeout (units of 0.1s)
    tcsetattr(STDIN_FILENO, TCSANOW, &timeout_mode);

    // Try to read the next two bytes of the escape sequence
    ssize_t n = ::read(STDIN_FILENO, &seq[0], 1);
    if (n <= 0) {
      // No byte within 100ms — it was a standalone Escape keypress
      tcsetattr(STDIN_FILENO, TCSANOW, &current);
      return {Key::ESCAPE, 0};
    }
    n = ::read(STDIN_FILENO, &seq[1], 1);
    tcsetattr(STDIN_FILENO, TCSANOW, &current);
    if (n <= 0)
      return {Key::ESCAPE, 0};

    // Parse the escape sequence: ESC [ <letter>
    if (seq[0] == '[') {
      switch (seq[1]) {
      case 'A': return {Key::ARROW_UP, 0};   // ESC [ A = Up arrow
      case 'B': return {Key::ARROW_DOWN, 0}; // ESC [ B = Down arrow
      default:  return {Key::UNKNOWN, 0};
      }
    }
    return {Key::ESCAPE, 0};
  }

  // Single-byte key mappings
  if (c == '\r' || c == '\n')
    return {Key::ENTER, 0};
  if (c == 127 || c == 8) // 127 = DEL (most terminals), 8 = BS (some terminals)
    return {Key::BACKSPACE, 0};
  if (c == '\t')
    return {Key::TAB, 0};
  if (c == 3) // ASCII 3 = Ctrl-C
    return {Key::CTRL_C, 0};
  if (c >= 32 && c < 127) // Printable ASCII
    return {Key::CHAR, c};

  return {Key::UNKNOWN, 0};
#endif
}

// Get terminal width in columns.
// Uses ioctl(TIOCGWINSZ) on Unix, GetConsoleScreenBufferInfo on Windows.
// Falls back to 80 columns if the query fails.
int Terminal::get_width() const {
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  return csbi.srWindow.Right - csbi.srWindow.Left + 1;
#else
  struct winsize ws;
  if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == 0)
    return ws.ws_col;
  return 80; // Fallback default
#endif
}

// Get terminal height in rows.
// Falls back to 24 rows if the query fails.
int Terminal::get_height() const {
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#else
  struct winsize ws;
  if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == 0)
    return ws.ws_row;
  return 24; // Fallback default
#endif
}

// Clear entire screen and move cursor to top-left (row 1, col 1).
// Uses ANSI escape: ESC[2J = clear screen, ESC[H = cursor home.
void Terminal::clear_screen() {
  write_stderr("\x1b[2J\x1b[H");
}

// Move cursor to absolute position using ANSI CSI sequence.
// Row and column are 1-based (top-left is row=1, col=1).
void Terminal::move_cursor(int row, int col) {
  write_stderr("\x1b[" + std::to_string(row) + ";" + std::to_string(col) + "H");
}

// Write raw text to stderr. All UI output goes through stderr so that
// stdout remains clean — the only thing written to stdout is the final
// selected line when the user presses Enter.
void Terminal::write_stderr(const std::string& text) {
  ::fwrite(text.data(), 1, text.size(), stderr);
  ::fflush(stderr);
}

// Render a complete frame of the search UI.
//
// Screen layout (bottom-aligned):
//   ┌─────────────────────────────┐
//   │  (empty space or overflow)  │  ← rows 1 to first_result_row-1
//   │  result N (lowest score)    │  ← results grow upward
//   │  ...                        │
//   │  result 1 (best match)      │  ← closest to the prompt
//   │  2/150                      │  ← status: matches / total lines
//   │  > query text               │  ← input prompt (last row)
//   └─────────────────────────────┘
//
// Scrolling: when there are more results than screen space, a scroll
// window follows the selected index. The scroll_offset shifts the
// visible window so the selected result is always on screen.
//
// All output is built into a single string buffer and flushed once
// to minimize flickering (avoids partial screen updates).
void Terminal::render_frame(const std::string& query,
                            const std::vector<MatchResult>& results,
                            const std::vector<std::string>& lines,
                            std::size_t selected_idx,
                            std::size_t total_lines) {
  int height = get_height();
  int width = get_width();
  int max_results = height - 2; // Reserve 2 rows for status + prompt

  std::string buf;
  buf.reserve(4096); // Pre-allocate to reduce reallocations

  buf += "\x1b[?25l"; // Hide cursor during rendering to prevent flicker

  int total_results = static_cast<int>(results.size());
  int display_count = std::min(total_results, max_results);

  // Scrolling logic: selected=0 is the best match (nearest to prompt),
  // higher indices are further up the list. When the selection moves
  // beyond the visible window, shift scroll_offset to follow it.
  int sel = static_cast<int>(selected_idx);
  int scroll_offset = 0;
  if (sel >= display_count)
    scroll_offset = sel - display_count + 1;

  // Clear any empty rows above the results area
  int first_result_row = height - 2 - display_count + 1;
  for (int row = 1; row <= first_result_row - 1 && row < height - 1; ++row) {
    buf += "\x1b[" + std::to_string(row) + ";1H";
    buf += "\x1b[K"; // Clear to end of line
  }

  // Render the visible window of results (bottom-aligned, best match lowest)
  for (int i = 0; i < display_count; ++i) {
    // Map screen row to result index: screen top = highest index (worst),
    // screen bottom = lowest index (best match, near the prompt)
    int result_idx = scroll_offset + display_count - 1 - i;
    if (result_idx >= total_results)
      result_idx = total_results - 1;

    int screen_row = first_result_row + i;
    if (screen_row < 1)
      continue;

    buf += "\x1b[" + std::to_string(screen_row) + ";1H";

    const auto& match = results[result_idx];
    const std::string& line = lines[match.line_index];

    // Selected line gets reverse video (white-on-black → black-on-white)
    if (result_idx == sel)
      buf += "\x1b[7m";

    buf += "  "; // 2-space indent for all result lines

    // Render each character, highlighting matched positions in bold green.
    // match_positions is sorted, so we walk through it with match_pos_idx.
    std::size_t match_pos_idx = 0;
    int chars_written = 2;
    for (std::size_t ci = 0; ci < line.size() && chars_written < width - 1; ++ci) {
      bool is_match = match_pos_idx < match.match_positions.size() &&
                      match.match_positions[match_pos_idx] == ci;
      if (is_match) {
        buf += "\x1b[32;1m"; // Bold green for matched character
        buf += line[ci];
        buf += "\x1b[0m";   // Reset colors
        if (result_idx == sel)
          buf += "\x1b[7m"; // Re-enable reverse video for selected line
        ++match_pos_idx;
      } else {
        buf += line[ci];
      }
      ++chars_written;
    }

    buf += "\x1b[0m\x1b[K"; // Reset formatting + clear rest of line
  }

  // Status line: "  N/M" showing matched count / total lines
  buf += "\x1b[" + std::to_string(height - 1) + ";1H";
  buf += "\x1b[36m"; // Cyan color for status
  std::string status = "  " + std::to_string(results.size()) + "/" +
                       std::to_string(total_lines);
  buf += status;
  buf += "\x1b[0m\x1b[K";

  // Prompt line: "> query_text" with green ">" indicator
  buf += "\x1b[" + std::to_string(height) + ";1H";
  buf += "\x1b[32m> \x1b[0m"; // Green "> " prompt
  buf += query;
  buf += "\x1b[K"; // Clear any leftover text after query

  buf += "\x1b[?25h"; // Show cursor (positioned after query text)

  // Flush entire frame at once to minimize screen flicker
  write_stderr(buf);
}

} // namespace fdr
