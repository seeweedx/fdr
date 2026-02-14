# fdr

A fuzzy text search tool for the command line. Pipe any input into `fdr` and interactively filter results in real time.

## Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.16+

## Building

```bash
cmake -B build
cmake --build build
```

The binary will be at `./build/fdr` (or `./build/Release/fdr.exe` on Windows with MSVC).

### Build types

```bash
# Release (optimized, -O2/-O3)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Debug (no optimization, full debug symbols)
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Clean and rebuild

```bash
rm -rf build
cmake -B build
cmake --build build
```

## Adding to PATH

To use `fdr` from anywhere in your **current** terminal session:

**Linux / macOS:**
```bash
export PATH="$PATH:~/Projects/fdr/build"
```

**Windows (CMD):**
```cmd
set PATH=%PATH%;C:\path\to\fdr\build
```

**Windows (PowerShell):**
```powershell
$env:PATH += ";C:\path\to\fdr\build"
```

To make it permanent, add the line to your shell config (`~/.bashrc`, `~/.zshrc`) or on Windows go to Settings > System > About > Advanced system settings > Environment Variables.

## Usage

Pipe any command's output into `fdr`:

```bash
ls -la | fdr
cat /etc/passwd | fdr
find . -type f | fdr
```

### Controls

| Key | Action |
|---|---|
| Type text | Filter results (space-separated words match independently, in any order) |
| Arrow Up/Down | Navigate results |
| Enter | Select and print the chosen line to stdout |
| Escape / Ctrl-C | Cancel and exit |
| Backspace | Delete last character |

### Example

```bash
# Pick a file and open it in vim
vim "$(find . -type f | fdr)"

# Select a git branch
git checkout "$(git branch | fdr)"
```

The selected line is printed to stdout, so it can be captured with `$()` or backticks.

## Cross-Platform Support

- **Linux** and **macOS**: Uses POSIX termios for terminal raw mode
- **Windows**: Uses Windows Console API (requires Windows 10+ for ANSI escape support)

## Project Structure

```
src/
  main.cpp            Entry point, stdin reading, interactive loop
  fuzzy.hpp / .cpp    Fuzzy matching, trigram indexing, scoring
  terminal.hpp / .cpp Cross-platform terminal handling
  hash_table.hpp      Hash table with linked-list chaining
  linked_list.hpp     Singly linked list
  min_heap.hpp        Min-heap for top-K selection
  rbtree.hpp          Red-black tree
  quicksort.hpp       Quicksort
CMakeLists.txt        Build configuration
```
