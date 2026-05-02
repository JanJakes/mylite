find_program(MYLITE_PHP_CONFIG_EXECUTABLE NAMES php-config)

if(NOT MYLITE_PHP_CONFIG_EXECUTABLE)
  message(STATUS "php-config not found; skipping php-ext-mylite")
  return()
endif()

execute_process(
  COMMAND "${MYLITE_PHP_CONFIG_EXECUTABLE}" --includes
  OUTPUT_VARIABLE MYLITE_PHP_INCLUDE_FLAGS
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
  COMMAND "${MYLITE_PHP_CONFIG_EXECUTABLE}" --php-binary
  OUTPUT_VARIABLE MYLITE_PHP_EXECUTABLE
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

separate_arguments(MYLITE_PHP_INCLUDE_FLAG_LIST UNIX_COMMAND "${MYLITE_PHP_INCLUDE_FLAGS}")
set(MYLITE_PHP_INCLUDE_DIRS)
foreach(flag IN LISTS MYLITE_PHP_INCLUDE_FLAG_LIST)
  if(flag MATCHES "^-I(.+)$")
    list(APPEND MYLITE_PHP_INCLUDE_DIRS "${CMAKE_MATCH_1}")
  endif()
endforeach()

if(NOT MYLITE_PHP_INCLUDE_DIRS OR NOT MYLITE_PHP_EXECUTABLE)
  message(STATUS "Could not query PHP build settings; skipping php-ext-mylite")
  return()
endif()

add_library(mylite_php_mysqli MODULE
  src/mysqli_extension.c
)
target_include_directories(mylite_php_mysqli PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)
target_include_directories(mylite_php_mysqli SYSTEM PRIVATE
  ${MYLITE_PHP_INCLUDE_DIRS}
)
target_link_libraries(mylite_php_mysqli PRIVATE MyLite::mylite)
target_compile_definitions(mylite_php_mysqli PRIVATE
  COMPILE_DL_MYSQLI=1
  ZEND_ENABLE_STATIC_TSRMLS_CACHE=1
)
set_target_properties(mylite_php_mysqli PROPERTIES
  OUTPUT_NAME mysqli
  PREFIX ""
)
if(APPLE)
  target_link_options(mylite_php_mysqli PRIVATE "-undefined" "dynamic_lookup")
endif()
mylite_configure_c_target(mylite_php_mysqli)
if(NOT MSVC)
  target_compile_options(mylite_php_mysqli PRIVATE -Wno-unused-parameter)
endif()

add_executable(mylite_php_binary
  src/mylite_cli.c
)
target_link_libraries(mylite_php_binary PRIVATE MyLite::mylite)
set_target_properties(mylite_php_binary PROPERTIES
  OUTPUT_NAME mylite
)
mylite_configure_c_target(mylite_php_binary)

if(BUILD_TESTING)
  set(MYLITE_PHP_EXTENSION "$<TARGET_FILE:mylite_php_mysqli>")

  add_test(
    NAME php-ext-mylite.mysqli-basic
    COMMAND "${MYLITE_PHP_EXECUTABLE}" -n -d "extension=${MYLITE_PHP_EXTENSION}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/mysqli_basic.php"
  )
  add_test(
    NAME php-ext-mylite.mysqli-procedural
    COMMAND "${MYLITE_PHP_EXECUTABLE}" -n -d "extension=${MYLITE_PHP_EXTENSION}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/mysqli_procedural.php"
  )
  add_test(
    NAME php-ext-mylite.mysqli-statement
    COMMAND "${MYLITE_PHP_EXECUTABLE}" -n -d "extension=${MYLITE_PHP_EXTENSION}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/mysqli_statement.php"
  )
  add_test(
    NAME php-ext-mylite.mysqli-errors
    COMMAND "${MYLITE_PHP_EXECUTABLE}" -n -d "extension=${MYLITE_PHP_EXTENSION}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/mysqli_errors.php"
  )
  add_test(
    NAME php-ext-mylite.mysqli-api-surface
    COMMAND "${MYLITE_PHP_EXECUTABLE}" -n -d "extension=${MYLITE_PHP_EXTENSION}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/mysqli_api_surface.php"
  )
  add_test(
    NAME php-ext-mylite.binary
    COMMAND "$<TARGET_FILE:mylite_php_binary>" --memory "SELECT 123 AS value"
  )
  set_tests_properties(
    php-ext-mylite.mysqli-basic
    php-ext-mylite.mysqli-procedural
    php-ext-mylite.mysqli-statement
    php-ext-mylite.mysqli-errors
    php-ext-mylite.mysqli-api-surface
    PROPERTIES SKIP_RETURN_CODE 77
  )
endif()
