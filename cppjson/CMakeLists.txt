cmake_minimum_required(VERSION 3.28)
project(leptjson_test CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
endif()

add_library(leptjson leptjson.cpp)
target_include_directories(leptjson PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(leptjson_test test.cpp)
target_link_libraries(leptjson_test leptjson)
