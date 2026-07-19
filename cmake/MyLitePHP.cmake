set(MYLITE_PHP_CONFIG_EXECUTABLE
  "php-config"
  CACHE FILEPATH
  "php-config executable used to build MyLite PHP extensions"
)
set(MYLITE_PHP_EXECUTABLE
  "php"
  CACHE FILEPATH
  "PHP CLI executable used to test MyLite PHP extensions"
)
set(MYLITE_PHP_INSTALL_DIR
  "${CMAKE_INSTALL_LIBDIR}/mylite/php"
  CACHE PATH
  "Install directory for MyLite PHP extension modules"
)

function(mylite_configure_php)
  mylite_configure_php_sanitizer_dlopen_wrapper()

  if(DEFINED MYLITE_PHP_INCLUDE_DIRS)
    return()
  endif()

  find_program(MYLITE_RESOLVED_PHP_CONFIG_EXECUTABLE
    NAMES "${MYLITE_PHP_CONFIG_EXECUTABLE}"
    REQUIRED
  )
  find_program(MYLITE_RESOLVED_PHP_EXECUTABLE
    NAMES "${MYLITE_PHP_EXECUTABLE}"
    REQUIRED
  )

  execute_process(
    COMMAND "${MYLITE_RESOLVED_PHP_CONFIG_EXECUTABLE}" --includes
    OUTPUT_VARIABLE mylite_php_include_flags
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
  )
  execute_process(
    COMMAND "${MYLITE_RESOLVED_PHP_CONFIG_EXECUTABLE}" --version
    OUTPUT_VARIABLE mylite_php_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
  )

  separate_arguments(mylite_php_include_flags NATIVE_COMMAND "${mylite_php_include_flags}")
  set(mylite_php_include_dirs)
  foreach(flag IN LISTS mylite_php_include_flags)
    if(flag MATCHES "^-I(.+)$")
      list(APPEND mylite_php_include_dirs "${CMAKE_MATCH_1}")
    endif()
  endforeach()

  if(NOT mylite_php_include_dirs)
    message(FATAL_ERROR "php-config --includes did not return PHP include directories")
  endif()

  set(MYLITE_PHP_INCLUDE_DIRS
    "${mylite_php_include_dirs}"
    CACHE INTERNAL
    "PHP include directories"
  )
  set(MYLITE_PHP_CLI
    "${MYLITE_RESOLVED_PHP_EXECUTABLE}"
    CACHE INTERNAL
    "Resolved PHP CLI executable"
  )
  set(MYLITE_PHP_VERSION
    "${mylite_php_version}"
    CACHE INTERNAL
    "Resolved PHP version"
  )

  message(STATUS "Building MyLite PHP extensions for PHP ${MYLITE_PHP_VERSION}")
endfunction()

function(mylite_configure_php_extension target)
  mylite_configure_php()

  target_include_directories("${target}" SYSTEM PRIVATE ${MYLITE_PHP_INCLUDE_DIRS})
  target_compile_definitions("${target}" PRIVATE ZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
  set_target_properties("${target}" PROPERTIES
    C_VISIBILITY_PRESET hidden
    PREFIX ""
    SUFFIX ".so"
  )

  if(TARGET mylite_php_sanitizer_dlopen_wrapper)
    add_dependencies("${target}" mylite_php_sanitizer_dlopen_wrapper)
  endif()

  if(APPLE)
    target_link_options("${target}" PRIVATE "LINKER:-undefined,dynamic_lookup")
  endif()
endfunction()

function(mylite_add_php_test test_name)
  mylite_configure_php()
  add_test(NAME "${test_name}" COMMAND "${MYLITE_PHP_CLI}" ${ARGN})
  set_tests_properties("${test_name}" PROPERTIES LABELS "php;compat.integration")
  if(TARGET mylite_php_sanitizer_dlopen_wrapper)
    set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT_MODIFICATION
      "LD_PRELOAD=path_list_prepend:$<TARGET_FILE:mylite_php_sanitizer_dlopen_wrapper>"
    )
  endif()
endfunction()

function(mylite_configure_php_sanitizer_dlopen_wrapper)
  if(NOT MYLITE_ENABLE_ASAN_UBSAN
     OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux"
     OR TARGET mylite_php_sanitizer_dlopen_wrapper)
    return()
  endif()

  add_library(mylite_php_sanitizer_dlopen_wrapper MODULE
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/MyLitePHPSanitizerDlopen.c"
  )
  target_compile_features(mylite_php_sanitizer_dlopen_wrapper PRIVATE c_std_17)
  target_compile_definitions(mylite_php_sanitizer_dlopen_wrapper PRIVATE _GNU_SOURCE)
  target_compile_options(mylite_php_sanitizer_dlopen_wrapper PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
  )
  if(MYLITE_WARNINGS_AS_ERRORS)
    target_compile_options(mylite_php_sanitizer_dlopen_wrapper PRIVATE -Werror)
  endif()
  target_link_libraries(mylite_php_sanitizer_dlopen_wrapper PRIVATE "${CMAKE_DL_LIBS}")
  set_target_properties(mylite_php_sanitizer_dlopen_wrapper PROPERTIES
    C_EXTENSIONS OFF
    PREFIX ""
  )
endfunction()
