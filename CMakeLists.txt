cmake_minimum_required(VERSION 3.16)
project(fdr LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(fdr
  src/main.cpp
  src/fuzzy.cpp
  src/terminal.cpp
)

target_include_directories(fdr PRIVATE src)
