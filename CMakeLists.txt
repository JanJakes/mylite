cmake_minimum_required(VERSION 3.20)

project(MyLite LANGUAGES C)

include(CTest)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(MYLITE_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(MYLITE_TIDB_GRAMMAR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tidb/parser.y")
set(MYLITE_LEMON_GRAMMAR "${MYLITE_GENERATED_DIR}/mylite_tidb_parser.lemon")
set(MYLITE_TOKEN_MAP "${MYLITE_GENERATED_DIR}/mylite_tidb_tokens.inc")
set(MYLITE_LEMON_C "${MYLITE_GENERATED_DIR}/mylite_tidb_parser.c")
set(MYLITE_LEMON_H "${MYLITE_GENERATED_DIR}/mylite_tidb_parser.h")
set(MYLITE_LEMON_OUT "${MYLITE_GENERATED_DIR}/mylite_tidb_parser.out")

add_executable(mylite_lemon third_party/sqlite/lemon.c)

add_custom_command(
  OUTPUT
    "${MYLITE_LEMON_GRAMMAR}"
    "${MYLITE_TOKEN_MAP}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${MYLITE_GENERATED_DIR}"
  COMMAND
    "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/port_tidb_lemon.py"
    --source "${MYLITE_TIDB_GRAMMAR}"
    --grammar-output "${MYLITE_LEMON_GRAMMAR}"
    --tokens-output "${MYLITE_TOKEN_MAP}"
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/port_tidb_lemon.py"
    "${MYLITE_TIDB_GRAMMAR}"
  VERBATIM
)

add_custom_command(
  OUTPUT
    "${MYLITE_LEMON_C}"
    "${MYLITE_LEMON_H}"
    "${MYLITE_LEMON_OUT}"
  COMMAND
    $<TARGET_FILE:mylite_lemon>
    -q
    "-T${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite/lempar.c"
    "-d${MYLITE_GENERATED_DIR}"
    "${MYLITE_LEMON_GRAMMAR}"
  DEPENDS
    mylite_lemon
    "${MYLITE_LEMON_GRAMMAR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite/lempar.c"
  VERBATIM
)

add_library(mylite_parser STATIC
  src/parser/lexer.c
  src/parser/parser.c
  "${MYLITE_LEMON_C}"
)

target_include_directories(mylite_parser
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE
    "${MYLITE_GENERATED_DIR}"
)

target_compile_features(mylite_parser PUBLIC c_std_99)

if(MSVC)
  target_compile_options(mylite_parser PRIVATE /W4)
else()
  target_compile_options(mylite_parser PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(mylite-parse src/tools/mylite_parse.c)
target_link_libraries(mylite-parse PRIVATE mylite_parser)

if(BUILD_TESTING)
  add_executable(mylite_parser_smoke_test tests/parser_smoke_test.c)
  target_link_libraries(mylite_parser_smoke_test PRIVATE mylite_parser)
  add_test(NAME mylite_parser_smoke_test COMMAND mylite_parser_smoke_test)
endif()
