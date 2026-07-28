add_library(mylite_php_pdo_extension MODULE
  src/php_pdo_mylite.c
)
target_include_directories(mylite_php_pdo_extension PRIVATE
  "${PROJECT_SOURCE_DIR}/packages/php-ext-common"
)
target_link_libraries(mylite_php_pdo_extension PRIVATE MyLite::headers)
add_dependencies(mylite_php_pdo_extension mylite_php_extension)
target_compile_definitions(mylite_php_pdo_extension PRIVATE COMPILE_DL_PDO_MYLITE=1)
mylite_configure_php_extension(mylite_php_pdo_extension)
set_target_properties(mylite_php_pdo_extension PROPERTIES OUTPUT_NAME pdo_mylite)
mylite_configure_c_target(mylite_php_pdo_extension)
if(NOT MSVC)
  target_compile_options(mylite_php_pdo_extension PRIVATE -Wno-unused-parameter)
endif()
mylite_add_artifact_size_report(
  mylite_php_pdo_size_report
  mylite_php_pdo_extension
  mylite-php-pdo-size-report.txt
  "${MYLITE_MAX_PHP_PDO_SIZE_BYTES}"
)
install(
  TARGETS mylite_php_pdo_extension
  LIBRARY DESTINATION "${MYLITE_PHP_INSTALL_DIR}"
  RUNTIME DESTINATION "${MYLITE_PHP_INSTALL_DIR}"
)

if(BUILD_TESTING)
  mylite_add_php_test(
    php-ext-pdo-mylite.api
    -d "extension=$<TARGET_FILE:mylite_php_extension>"
    -d "extension=$<TARGET_FILE:mylite_php_pdo_extension>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/pdo_api_test.php"
  )
  mylite_add_php_test(
    php-ext-pdo-mylite.statement-diagnostics
    -d "extension=$<TARGET_FILE:mylite_php_extension>"
    -d "extension=$<TARGET_FILE:mylite_php_pdo_extension>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/pdo_statement_diagnostics.php"
  )
  mylite_add_php_test(
    php-ext-pdo-mylite.scalar-types-server-identity
    -d "extension=$<TARGET_FILE:mylite_php_extension>"
    -d "extension=$<TARGET_FILE:mylite_php_pdo_extension>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/pdo_scalar_types_server_identity.php"
  )
  mylite_add_php_test(
    php-ext-pdo-mylite.metadata-observables
    -d "extension=$<TARGET_FILE:mylite_php_extension>"
    -d "extension=$<TARGET_FILE:mylite_php_pdo_extension>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/pdo_metadata_observables.php"
  )
endif()
