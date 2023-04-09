cmake_minimum_required(VERSION 3.7)
project("json_parser")
add_definitions("-Wall -g")
include_directories(${PROJECT_SOURCE_DIR}/include)

add_executable(test test/test.c src/leptjson.c)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)