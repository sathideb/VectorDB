cmake_minimum_required(VERSION 3.14)
project(VeloDB CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(GTest REQUIRED)

# the database code, compiled once and shared by the app and the tests
add_library(velodb_core
    src/BTreeManager.cpp
    src/StorageEngine.cpp)
target_include_directories(velodb_core PUBLIC src)
target_link_libraries(velodb_core PUBLIC Threads::Threads)

# the real program
add_executable(velodb src/main.cpp)
target_link_libraries(velodb PRIVATE velodb_core)

# the tests
add_executable(btree_tests tests/test_btree.cpp)
target_link_libraries(btree_tests PRIVATE velodb_core GTest::gtest_main)

enable_testing()
add_test(NAME btree_tests COMMAND btree_tests)