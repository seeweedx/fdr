#include "terminal.hpp"
#include "fuzzy.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace fdr {

#ifndef _WIN32
struct Terminal::TermiosState {
  struct termios original;
};
#endif

Terminal::Terminal() {}

Terminal::~Terminal() {
  if (raw_mode_enabled_)
    disable_raw_mode();
#ifndef _WIN32
  delete original_termios_;
#endif
}

void Terminal::enable_raw_mode() {
  if (raw_mode_enabled_)
    return;

#ifdef _WIN32
  HANDLE h_stdin = GetStdHandle(STD_INPUT_HANDLE);
  HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);

  DWORD stdin_mode, stdout_mode;
  GetConsoleMode(h_stdin, &stdin_mode);
  GetConsoleMode(h_stdout, &stdout_mode);

  original_stdin_mode_ = reinterpret_cast<void*>(static_cast<uintptr_t>(stdin_mode));
  original_stdout_mode_ = reinterpret_cast<void*>(static_cast<uintptr_t>(stdout_mode));

  // Disable line input and echo for raw key reading
  SetConsoleMode(h_stdin, ENABLE_WINDOW_INPUT);
  // Enable ANSI escape codes for output
  SetConsoleMode(h_stdout, stdout_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN);
#else
  if (!original_termios_)
    original_termios_ = new TermiosState();

  tcgetattr(STDIN_FILENO, &original_termios_->original);

  struct termios raw = original_termios_->original;
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
#endif

  raw_mode_enabled_ = true;

  // Enter alternate screen buffer
  write_stderr("\x1b[?1049h");
}

void Terminal::disable_raw_mode() {
  if (!raw_mode_enabled_)
    return;

  // Leave alternate screen buffer
  write_stderr("\x1b[?1049l");

#ifdef _WIN32
  HANDLE h_stdin = GetStdHandle(STD_INPUT_HANDLE);
  HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleMode(h_stdin, static_cast<DWORD>(reinterpret_cast<uintptr_t>(original_stdin_mode_)));
  SetConsoleMode(h_stdout, static_cast<DWORD>(reinterpret_cast<uintptr_t>(original_stdout_mode_)));
#else
  if (original_termios_)
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios_->original);
#endif

  raw_mode_enabled_ = false;
}

KeyEvent Terminal::read_key() {
#ifdef _WIN32
  HANDLE h_stdin = GetStdHandle(STD_INPUT_HANDLE);
  while (true) {
    INPUT_RECORD ir;
    DWORD count;
    if (!ReadConsoleInput(h_stdin, &ir, 1, &count) || count == 0)
      return {Key::ESCAPE, 0};

    // Only process key-down events
    if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown)
      continue;

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

    // Ctrl-C
    if (ch == 3)
      return {Key::CTRL_C, 0};

    // Printable characters
    if (ch >= 32 && ch < 127)
      return {Key::CHAR, ch};
  }
#else
  char c;
  if (::read(STDIN_FILENO, &c, 1) <= 0)
    return {Key::ESCAPE, 0};

  if (c == '\x1b') {
    char seq[2];
    // Non-blocking read with 100ms timeout to distinguish
    // standalone Escape from escape sequences (e.g. arrow keys)
    struct termios current;
    tcgetattr(STDIN_FILENO, &current);
    struct termios timeout_mode = current;
    timeout_mode.c_cc[VMIN] = 0;
    timeout_mode.c_cc[VTIME] = 1; // 100ms
    tcsetattr(STDIN_FILENO, TCSANOW, &timeout_mode);

    ssize_t n = ::read(STDIN_FILENO, &seq[0], 1);
    if (n <= 0) {
      tcsetattr(STDIN_FILENO, TCSANOW, &current);
      return {Key::ESCAPE, 0};
    }
    n = ::read(STDIN_FILENO, &seq[1], 1);
    tcsetattr(STDIN_FILENO, TCSANOW, &current);
    if (n <= 0)
      return {Key::ESCAPE, 0};

    if (seq[0] == '[') {
      switch (seq[1]) {
      case 'A': return {Key::ARROW_UP, 0};
      case 'B': return {Key::ARROW_DOWN, 0};
      default:  return {Key::UNKNOWN, 0};
      }
    }
    return {Key::ESCAPE, 0};
  }

  if (c == '\r' || c == '\n')
    return {Key::ENTER, 0};
  if (c == 127 || c == 8)
    return {Key::BACKSPACE, 0};
  if (c == '\t')
    return {Key::TAB, 0};
  if (c == 3)
    return {Key::CTRL_C, 0};
  if (c >= 32 && c < 127)
    return {Key::CHAR, c};

  return {Key::UNKNOWN, 0};
#endif
}

int Terminal::get_width() const {
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  return csbi.srWindow.Right - csbi.srWindow.Left + 1;
#else
  struct winsize ws;
  if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == 0)
    return ws.ws_col;
  return 80;
#endif
}

int Terminal::get_height() const {
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#else
  struct winsize ws;
  if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == 0)
    return ws.ws_row;
  return 24;
#endif
}

void Terminal::clear_screen() {
  write_stderr("\x1b[2J\x1b[H");
}

void Terminal::move_cursor(int row, int col) {
  write_stderr("\x1b[" + std::to_string(row) + ";" + std::to_string(col) + "H");
}

void Terminal::write_stderr(const std::string& text) {
  ::fwrite(text.data(), 1, text.size(), stderr);
  ::fflush(stderr);
}

void Terminal::render_frame(const std::string& query,
                            const std::vector<MatchResult>& results,
                            const std::vector<std::string>& lines,
                            std::size_t selected_idx,
                            std::size_t total_lines) {
  int height = get_height();
  int width = get_width();
  int max_results = height - 2; // Reserve lines for prompt and status

  std::string buf;
  buf.reserve(4096);

  // Hide cursor
  buf += "\x1b[?25l";

  // Layout (bottom-aligned):
  //   rows above: results (closest match nearest to prompt)
  //   row (height-2): status "N/M"
  //   row (height-1): prompt "> query"

  int display_count = std::min(static_cast<int>(results.size()), max_results);

  // First row of content = height - 2 - display_count + 1
  // Start by clearing empty rows above results
  int first_result_row = height - 2 - display_count + 1;
  for (int row = 1; row <= first_result_row - 1 && row < height - 1; ++row) {
    buf += "\x1b[" + std::to_string(row) + ";1H";
    buf += "\x1b[K";
  }

  // Render results (furthest match at top, closest match near prompt)
  for (int i = 0; i < display_count; ++i) {
    int result_idx = display_count - 1 - i; // Reverse: best match at bottom
    int screen_row = first_result_row + i;
    if (screen_row < 1)
      continue;

    buf += "\x1b[" + std::to_string(screen_row) + ";1H";

    const auto& match = results[result_idx];
    const std::string& line = lines[match.line_index];

    // Highlight selected line
    if (static_cast<std::size_t>(result_idx) == selected_idx)
      buf += "\x1b[7m"; // Reverse video

    buf += "  ";

    // Render line with matched characters highlighted
    std::size_t match_pos_idx = 0;
    int chars_written = 2;
    for (std::size_t ci = 0; ci < line.size() && chars_written < width - 1; ++ci) {
      bool is_match = match_pos_idx < match.match_positions.size() &&
                      match.match_positions[match_pos_idx] == ci;
      if (is_match) {
        buf += "\x1b[32;1m"; // Bold green
        buf += line[ci];
        buf += "\x1b[0m";
        if (static_cast<std::size_t>(result_idx) == selected_idx)
          buf += "\x1b[7m";
        ++match_pos_idx;
      } else {
        buf += line[ci];
      }
      ++chars_written;
    }

    buf += "\x1b[0m\x1b[K";
  }

  // Status line (second to last row)
  buf += "\x1b[" + std::to_string(height - 1) + ";1H";
  buf += "\x1b[36m"; // Cyan
  std::string status = "  " + std::to_string(results.size()) + "/" +
                       std::to_string(total_lines);
  buf += status;
  buf += "\x1b[0m\x1b[K";

  // Prompt line (last row)
  buf += "\x1b[" + std::to_string(height) + ";1H";
  buf += "\x1b[32m> \x1b[0m";
  buf += query;
  buf += "\x1b[K";

  // Show cursor at end of query
  buf += "\x1b[?25h";

  write_stderr(buf);
}

} // namespace fdr
