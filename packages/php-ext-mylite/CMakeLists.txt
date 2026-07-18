add_library(mylite_php_extension MODULE
  src/php_mylite.c
)
target_link_libraries(mylite_php_extension PRIVATE MyLite::mylite)
target_compile_definitions(mylite_php_extension PRIVATE COMPILE_DL_MYLITE=1)
mylite_configure_php_extension(mylite_php_extension)
set_target_properties(mylite_php_extension PROPERTIES OUTPUT_NAME mylite)
mylite_configure_c_target(mylite_php_extension)
if(NOT MSVC)
  target_compile_options(mylite_php_extension PRIVATE -Wno-unused-parameter)
endif()
install(
  TARGETS mylite_php_extension
  LIBRARY DESTINATION "${MYLITE_PHP_INSTALL_DIR}"
  RUNTIME DESTINATION "${MYLITE_PHP_INSTALL_DIR}"
)

if(BUILD_TESTING)
  mylite_add_php_test(
    php-ext-mylite.core-api
    -n
    -d "extension=$<TARGET_FILE:mylite_php_extension>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/core_api_test.php"
  )
endif()
