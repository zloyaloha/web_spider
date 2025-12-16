cmake_minimum_required(VERSION 3.16)
project(wiki_searcher VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

find_package(mongocxx REQUIRED)
find_package(bsoncxx REQUIRED)
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

add_library(search_lib src/tokenizer.cpp src/searcher.cpp)
target_include_directories(search_lib PUBLIC include/)

add_executable(main lab3/main.cpp)
target_link_libraries(main PRIVATE mongo::mongocxx_shared mongo::bsoncxx_shared)
target_link_libraries(main PUBLIC search_lib)

add_executable(doc_searcher lab3/searcher.cpp)
target_link_libraries(doc_searcher PRIVATE mongo::mongocxx_shared mongo::bsoncxx_shared)
target_link_libraries(doc_searcher PUBLIC search_lib)

add_executable(unit_tests
    tests/test_tokenizer.cpp
    tests/test_set_logic.cpp
    tests/test_indexator.cpp
    tests/test_searcher.cpp
)

target_link_libraries(unit_tests
    search_lib
    gtest_main
)

enable_testing()