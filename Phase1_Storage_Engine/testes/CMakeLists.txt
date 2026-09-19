cmake_minimum_required(VERSION 3.14)
project(VeloDB CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Compile library sources
add_library(velodb_core
    src/StorageEngine.cpp
    src/BTreeManager.cpp
)

# Compile test runner binary
add_executable(run_tests tests/main.cpp)
target_link_libraries(run_tests PRIVATE velodb_core)