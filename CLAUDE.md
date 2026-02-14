# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**fdr** is a fuzzy text search CLI tool (similar to fzf) built in C++17 for a CSD2183 Data Structures assignment. It reads lines from stdin and provides interactive fuzzy filtering with ranked results.

## Build Commands

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Testing

```bash
ls -la | ./build/fdr
cat /etc/passwd | ./build/fdr
find /usr -name "*.h" | ./build/fdr
```

## Architecture

The project demonstrates 5 course data structures, all implemented from scratch (no STL containers for the core structures):

- **Hash table** (`src/hash_table.hpp`) — trigram index mapping 3-char substrings to line indices, O(1) lookup
- **Linked list** (`src/linked_list.hpp`) — hash table collision chaining
- **Min-heap** (`src/min_heap.hpp`) — top-K result selection in O(n log K)
- **Red-black tree** (`src/rbtree.hpp`) — deduplicated candidate set from trigram lookups
- **Quicksort** (`src/quicksort.hpp`) — final result sorting by score

Non-data-structure modules:
- **Fuzzy matching** (`src/fuzzy.hpp` + `src/fuzzy.cpp`) — trigram indexing, scoring algorithm
- **Terminal** (`src/terminal.hpp` + `src/terminal.cpp`) — cross-platform raw mode input/rendering
- **Main** (`src/main.cpp`) — entry point, stdin reading, interactive loop

## Key Conventions

- C++17 standard, CMake build system
- Templated data structures are header-only `.hpp` (required by C++ templates)
- Non-templated modules use `.hpp` + `.cpp` separation
- Cross-platform: Unix (POSIX termios) and Windows (Console API) via `#ifdef _WIN32`
- All data structures live in the `fdr` namespace
- No external dependencies — everything implemented from scratch

## Naming Conventions

- **snake_case** throughout (STL-consistent)
- Functions/methods: `push_front()`, `find_if()`, `build_index()`
- Variables/members: `head_node`, `bucket_count`, `match_score`
- Private members: trailing underscore — `head_`, `size_`, `capacity_`
- Classes/structs: **PascalCase** — `LinkedList`, `HashTable`, `MinHeap`, `RBTree`
- Template parameters: **PascalCase** — `<typename Key, typename Value>`
- Constants/enums: **ALL_CAPS** — `MAX_RESULTS`, `DEFAULT_CAPACITY`
- Namespaces: **lowercase** — `fdr`
- Files: **snake_case** — `linked_list.hpp`, `hash_table.hpp`
- Header guards: `#pragma once` (no `#ifndef` guards)

## Formatting

- 2-space indentation
- Opening brace on same line
- Single-line `if` returns without braces
