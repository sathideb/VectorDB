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
    src/StorageEngine.cpp
    src/flat_search.cpp
    src/distance.cpp
    src/vector_file.cpp
    src/recall.cpp)
target_include_directories(velodb_core PUBLIC src)
target_link_libraries(velodb_core PUBLIC Threads::Threads)

# the real program
add_executable(velodb src/main.cpp)
target_link_libraries(velodb PRIVATE velodb_core)

# the tests
add_executable(test_distance
    tests/test_distance.cpp
    src/distance.cpp)
target_include_directories(test_distance PRIVATE src)
target_link_libraries(test_distance PRIVATE GTest::gtest_main)

add_executable(btree_tests tests/test_btree.cpp)
target_link_libraries(btree_tests PRIVATE velodb_core GTest::gtest_main)

add_executable(flat_search_tests tests/test_flat_search.cpp)
target_link_libraries(flat_search_tests PRIVATE velodb_core GTest::gtest_main)

add_executable(recall_tests tests/test_recall.cpp)
target_link_libraries(recall_tests PRIVATE velodb_core GTest::gtest_main)

enable_testing()
add_test(NAME btree_tests COMMAND btree_tests)
add_test(NAME test_distance COMMAND test_distance)
add_test(NAME flat_search_tests COMMAND flat_search_tests)
add_test(NAME recall_tests COMMAND recall_tests)
add_test(NAME flat_search_tests COMMAND flat_search_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})