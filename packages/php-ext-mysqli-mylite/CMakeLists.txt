add_library(mylite_php_mysqli_extension MODULE
  src/mysqli_api.c
  src/mysqli_extension.c
  src/mysqli_registration.c
  src/mysqli_support.c
)
target_include_directories(mylite_php_mysqli_extension PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
  "${PROJECT_SOURCE_DIR}/packages/php-ext-common"
)
target_link_libraries(mylite_php_mysqli_extension PRIVATE MyLite::headers)
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
mylite_add_artifact_size_report(
  mylite_php_mysqli_size_report
  mylite_php_mysqli_extension
  mylite-php-mysqli-size-report.txt
  "${MYLITE_MAX_PHP_MYSQLI_SIZE_BYTES}"
)
install(
  TARGETS mylite_php_mysqli_extension
  LIBRARY DESTINATION "${MYLITE_PHP_INSTALL_DIR}"
  RUNTIME DESTINATION "${MYLITE_PHP_INSTALL_DIR}"
)

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
    php-ext-mysqli-mylite.pending-result-state
    -n
    -d "extension=${MYLITE_PHP_CORE_EXTENSION}"
    -d "extension=${MYLITE_PHP_MYSQLI_EXTENSION}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/mysqli_pending_result_state.php"
  )
  mylite_add_php_test(
    php-ext-mysqli-mylite.warnings
    -n
    -d "extension=${MYLITE_PHP_CORE_EXTENSION}"
    -d "extension=${MYLITE_PHP_MYSQLI_EXTENSION}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/mysqli_warnings.php"
  )
  mylite_add_php_test(
    php-ext-mysqli-mylite.scalar-types
    -n
    -d "extension=${MYLITE_PHP_CORE_EXTENSION}"
    -d "extension=${MYLITE_PHP_MYSQLI_EXTENSION}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/mysqli_scalar_types.php"
  )
  mylite_add_php_test(
    php-ext-mysqli-mylite.connection-identity
    -n
    -d "extension=${MYLITE_PHP_CORE_EXTENSION}"
    -d "extension=${MYLITE_PHP_MYSQLI_EXTENSION}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/mysqli_connection_identity.php"
  )
  mylite_add_php_test(
    php-ext-mysqli-mylite.api-surface
    -n
    -d "extension=${MYLITE_PHP_CORE_EXTENSION}"
    -d "extension=${MYLITE_PHP_MYSQLI_EXTENSION}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/mysqli_api_surface.php"
  )
endif()
