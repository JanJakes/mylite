cmake_minimum_required(VERSION 3.25)

project(MyLiteLemonCrossFixture LANGUAGES C)

if(NOT CMAKE_CROSSCOMPILING)
  message(FATAL_ERROR "The Lemon host fixture must be cross-compiled")
endif()
if(NOT MYLITE_SOURCE_DIR)
  message(FATAL_ERROR "MYLITE_SOURCE_DIR is required")
endif()

list(APPEND CMAKE_MODULE_PATH "${MYLITE_SOURCE_DIR}/cmake")
include(MyLiteLemon)

configure_file(
  "${MYLITE_SOURCE_DIR}/third_party/sqlite/upstream/tool/lempar.c"
  "${CMAKE_CURRENT_BINARY_DIR}/lempar.c"
  COPYONLY
)

mylite_add_lemon_host_tool()
mylite_add_lemon_parser(
  BASENAME fixture
  GRAMMAR "${CMAKE_CURRENT_SOURCE_DIR}/fixture.y"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  TEMPLATE "${CMAKE_CURRENT_BINARY_DIR}/lempar.c"
)

add_custom_target(lemon_fixture_parser
  DEPENDS
    "${MYLITE_LEMON_PARSER_C}"
    "${MYLITE_LEMON_PARSER_H}"
)
