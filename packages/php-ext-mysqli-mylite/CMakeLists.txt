add_library(mylite_php_mysqli_extension MODULE
  src/mysqli_api.c
  src/mysqli_extension.c
  src/mysqli_registration.c
  src/mysqli_support.c
)
target_include_directories(mylite_php_mysqli_extension PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
  "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/packages/libmylite/include>"
  "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/packages/libmylite/include>"
)
add_dependencies(mylite_php_mysqli_extension mylite_php_extension)
target_compile_definitions(mylite_php_mysqli_extension PRIVATE
  COMPILE_DL_MYSQLI=1
)
mylite_configure_php_extension(mylite_php_mysqli_extension)
set_target_properties(mylite_php_mysqli_extension PROPERTIES
  OUTPUT_NAME mysqli
)
mylite_configure_c_target(mylite_php_mysqli_extension)
if(NOT MSVC)
  target_compile_options(mylite_php_mysqli_extension PRIVATE -Wno-unused-parameter)
endif()

if(BUILD_TESTING)
  set(MYLITE_PHP_CORE_EXTENSION "$<TARGET_FILE:mylite_php_extension>")
  set(MYLITE_PHP_MYSQLI_EXTENSION "$<TARGET_FILE:mylite_php_mysqli_extension>")

  mylite_add_php_test(
    php-ext-mysqli-mylite.basic
    -n
    -d "extension=${MYLITE_PHP_CORE_EXTENSION}"
    -d "extension=${MYLITE_PHP_MYSQLI_EXTENSION}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/mysqli_basic.php"
  )
  mylite_add_php_test(
    php-ext-mysqli-mylite.procedural
    -n
    -d "extension=${MYLITE_PHP_CORE_EXTENSION}"
    -d "extension=${MYLITE_PHP_MYSQLI_EXTENSION}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/mysqli_procedural.php"
  )
  mylite_add_php_test(
    php-ext-mysqli-mylite.fetch-object
    -n
    -d "extension=${MYLITE_PHP_CORE_EXTENSION}"
    -d "extension=${MYLITE_PHP_MYSQLI_EXTENSION}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/mysqli_fetch_object.php"
  )
  mylite_add_php_test(
    php-ext-mysqli-mylite.statement
    -n
    -d "extension=${MYLITE_PHP_CORE_EXTENSION}"
    -d "extension=${MYLITE_PHP_MYSQLI_EXTENSION}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/mysqli_statement.php"
  )
  mylite_add_php_test(
    php-ext-mysqli-mylite.errors
    -n
    -d "extension=${MYLITE_PHP_CORE_EXTENSION}"
    -d "extension=${MYLITE_PHP_MYSQLI_EXTENSION}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/mysqli_errors.php"
  )
  mylite_add_php_test(
    php-ext-mysqli-mylite.api-surface
    -n
    -d "extension=${MYLITE_PHP_CORE_EXTENSION}"
    -d "extension=${MYLITE_PHP_MYSQLI_EXTENSION}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/mysqli_api_surface.php"
  )
endif()
