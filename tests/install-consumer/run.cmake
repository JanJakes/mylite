foreach(required IN ITEMS
    MYLITE_BUILD_DIR
    MYLITE_SOURCE_DIR
    MYLITE_GENERATOR
    MYLITE_C_COMPILER
    MYLITE_INSTALL_LIBDIR
    MYLITE_INSTALL_BINDIR
)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

set(test_root "${MYLITE_BUILD_DIR}/install-consumer-test")
set(install_prefix "${test_root}/prefix")
set(cmake_consumer_build "${test_root}/cmake-build")
set(pkgconfig_consumer_executable
  "${test_root}/mylite_pkgconfig_consumer${CMAKE_EXECUTABLE_SUFFIX}"
)
if(WIN32)
  set(runtime_environment
    "PATH=${install_prefix}/${MYLITE_INSTALL_BINDIR}\;$ENV{PATH}"
  )
elseif(APPLE)
  set(runtime_environment
    "DYLD_LIBRARY_PATH=${install_prefix}/${MYLITE_INSTALL_LIBDIR}"
  )
else()
  set(runtime_environment
    "LD_LIBRARY_PATH=${install_prefix}/${MYLITE_INSTALL_LIBDIR}"
  )
endif()
file(REMOVE_RECURSE "${test_root}")

set(install_command
  "${CMAKE_COMMAND}" --install "${MYLITE_BUILD_DIR}" --prefix "${install_prefix}"
)
if(DEFINED MYLITE_INSTALL_CONFIG AND NOT MYLITE_INSTALL_CONFIG STREQUAL "")
  list(APPEND install_command --config "${MYLITE_INSTALL_CONFIG}")
endif()
execute_process(COMMAND ${install_command} RESULT_VARIABLE install_result)
if(NOT install_result EQUAL 0)
  message(FATAL_ERROR "MyLite installation failed")
endif()

set(consumer_configure_command
  "${CMAKE_COMMAND}"
    -S "${MYLITE_SOURCE_DIR}/tests/install-consumer"
    -B "${cmake_consumer_build}"
    -G "${MYLITE_GENERATOR}"
    "-DCMAKE_C_COMPILER=${MYLITE_C_COMPILER}"
    "-DCMAKE_PREFIX_PATH=${install_prefix}"
)
if(DEFINED MYLITE_TOOLCHAIN_FILE AND NOT MYLITE_TOOLCHAIN_FILE STREQUAL "")
  list(APPEND consumer_configure_command
    "-DCMAKE_TOOLCHAIN_FILE=${MYLITE_TOOLCHAIN_FILE}"
  )
endif()
if(DEFINED MYLITE_VCPKG_TARGET_TRIPLET
   AND NOT MYLITE_VCPKG_TARGET_TRIPLET STREQUAL "")
  list(APPEND consumer_configure_command
    "-DVCPKG_TARGET_TRIPLET=${MYLITE_VCPKG_TARGET_TRIPLET}"
  )
endif()
execute_process(
  COMMAND ${consumer_configure_command}
  RESULT_VARIABLE configure_result
)
if(NOT configure_result EQUAL 0)
  message(FATAL_ERROR "Installed-package consumer configuration failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${cmake_consumer_build}"
  RESULT_VARIABLE build_result
)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "Installed-package consumer build failed")
endif()

set(consumer_executable
  "${cmake_consumer_build}/mylite_install_consumer${CMAKE_EXECUTABLE_SUFFIX}"
)
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env "${runtime_environment}" "${consumer_executable}"
  RESULT_VARIABLE consumer_result
)
if(NOT consumer_result EQUAL 0)
  message(FATAL_ERROR "Installed-package consumer execution failed")
endif()

find_program(
  pkg_config_executable
  NAMES pkg-config
  PATHS /usr/bin /usr/local/bin /opt/homebrew/bin
  NO_DEFAULT_PATH
)
if(NOT pkg_config_executable)
  find_program(pkg_config_executable NAMES pkg-config)
endif()
if(pkg_config_executable)
  set(ENV{PKG_CONFIG_PATH} "${install_prefix}/${MYLITE_INSTALL_LIBDIR}/pkgconfig")
  execute_process(
    COMMAND "${pkg_config_executable}" --cflags --libs --static mylite
    RESULT_VARIABLE pkgconfig_query_result
    OUTPUT_VARIABLE pkgconfig_arguments
    ERROR_VARIABLE pkgconfig_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT pkgconfig_query_result EQUAL 0)
    message(FATAL_ERROR "Installed pkg-config metadata failed: ${pkgconfig_error}")
  endif()
  separate_arguments(pkgconfig_arguments NATIVE_COMMAND "${pkgconfig_arguments}")
  set(sanitizer_link_arguments)
  if(DEFINED MYLITE_SANITIZER_LINK_OPTION
     AND NOT MYLITE_SANITIZER_LINK_OPTION STREQUAL "")
    list(APPEND sanitizer_link_arguments "${MYLITE_SANITIZER_LINK_OPTION}")
  endif()

  execute_process(
    COMMAND "${MYLITE_C_COMPILER}"
      "${MYLITE_SOURCE_DIR}/tests/install-consumer/main.c"
      -o "${pkgconfig_consumer_executable}"
      ${pkgconfig_arguments}
      ${sanitizer_link_arguments}
    RESULT_VARIABLE pkgconfig_build_result
    ERROR_VARIABLE pkgconfig_build_error
  )
  if(NOT pkgconfig_build_result EQUAL 0)
    message(FATAL_ERROR "Installed pkg-config consumer build failed: ${pkgconfig_build_error}")
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      -E env "${runtime_environment}" "${pkgconfig_consumer_executable}"
    RESULT_VARIABLE pkgconfig_consumer_result
  )
  if(NOT pkgconfig_consumer_result EQUAL 0)
    message(FATAL_ERROR "Installed pkg-config consumer execution failed")
  endif()
endif()
