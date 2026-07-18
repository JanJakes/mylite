set(mylite_install_consumer_sanitizer_link_option "")
if(MYLITE_ENABLE_ASAN_UBSAN)
  set(mylite_install_consumer_sanitizer_link_option "-fsanitize=address,undefined")
elseif(MYLITE_ENABLE_TSAN)
  set(mylite_install_consumer_sanitizer_link_option "-fsanitize=thread")
endif()

add_test(
  NAME mylite.install.consumer
  COMMAND "${CMAKE_COMMAND}"
    "-DMYLITE_BUILD_DIR=${PROJECT_BINARY_DIR}"
    "-DMYLITE_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DMYLITE_INSTALL_CONFIG=$<CONFIG>"
    "-DMYLITE_GENERATOR=${CMAKE_GENERATOR}"
    "-DMYLITE_C_COMPILER=${CMAKE_C_COMPILER}"
    "-DMYLITE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
    "-DMYLITE_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}"
    "-DMYLITE_SANITIZER_LINK_OPTION=${mylite_install_consumer_sanitizer_link_option}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/install-consumer/run.cmake"
)
set_tests_properties(
  mylite.install.consumer
  PROPERTIES
    LABELS "install;integration"
    TIMEOUT 180
)

add_test(
  NAME mylite.install.pkgconfig_multiarch
  COMMAND "${CMAKE_COMMAND}"
    "-DMYLITE_BUILD_DIR=${PROJECT_BINARY_DIR}"
    "-DMYLITE_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DMYLITE_GENERATOR=${CMAKE_GENERATOR}"
    "-DMYLITE_C_COMPILER=${CMAKE_C_COMPILER}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/pkgconfig-multiarch/run.cmake"
)
set_tests_properties(
  mylite.install.pkgconfig_multiarch
  PROPERTIES
    LABELS "install;integration"
    TIMEOUT 30
)
