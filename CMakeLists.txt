cmake_minimum_required(VERSION 3.7)
project("json_parser")
add_definitions("-g")
include_directories(${PROJECT_SOURCE_DIR}/include)

add_executable(test test/test.c src/leptjson.c)
add_executable(test1 test/test1.c src/leptjson.c)
add_executable(test2 test/test2.c src/leptjson.c)

add_library(json_parser SHARED src/leptjson.c)
add_library(json_parser_static STATIC src/leptjson.c)
set_target_properties(json_parser_static PROPERTIES OUTPUTNA "json_parser")


set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

install(FILES include/leptjson.h DESTINATION include/json_parser)
install(TARGETS json_parser json_parser_static
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)