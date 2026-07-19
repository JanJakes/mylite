foreach(required IN ITEMS
    MYLITE_BUILD_DIR
    MYLITE_SOURCE_DIR
    MYLITE_GENERATOR
    MYLITE_C_COMPILER
)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

set(test_build "${MYLITE_BUILD_DIR}/pkgconfig-multiarch-test")
file(REMOVE_RECURSE "${test_build}")
set(fixture_configure_command
  "${CMAKE_COMMAND}"
    -S "${MYLITE_SOURCE_DIR}"
    -B "${test_build}"
    -G "${MYLITE_GENERATOR}"
    "-DCMAKE_C_COMPILER=${MYLITE_C_COMPILER}"
    -DCMAKE_INSTALL_PREFIX=/usr
    -DCMAKE_INSTALL_LIBDIR=lib/x86_64-linux-gnu
    -DBUILD_TESTING=OFF
    -DMYLITE_BUILD_TOOLS=OFF
)
if(DEFINED MYLITE_TOOLCHAIN_FILE AND NOT MYLITE_TOOLCHAIN_FILE STREQUAL "")
  list(APPEND fixture_configure_command
    "-DCMAKE_TOOLCHAIN_FILE=${MYLITE_TOOLCHAIN_FILE}"
  )
endif()
if(DEFINED MYLITE_VCPKG_TARGET_TRIPLET
   AND NOT MYLITE_VCPKG_TARGET_TRIPLET STREQUAL "")
  list(APPEND fixture_configure_command
    "-DVCPKG_TARGET_TRIPLET=${MYLITE_VCPKG_TARGET_TRIPLET}"
  )
endif()
execute_process(
  COMMAND ${fixture_configure_command}
  RESULT_VARIABLE configure_result
)
if(NOT configure_result EQUAL 0)
  message(FATAL_ERROR "Multiarch pkg-config fixture configuration failed")
endif()

file(READ "${test_build}/packages/libmylite/mylite.pc" pkgconfig_contents)
string(FIND
  "${pkgconfig_contents}"
  "prefix=\${pcfiledir}/../../.."
  prefix_index
)
if(prefix_index EQUAL -1)
  message(FATAL_ERROR "Multiarch pkg-config prefix is not relocatable")
endif()
string(FIND
  "${pkgconfig_contents}"
  "libdir=\${prefix}/lib/x86_64-linux-gnu"
  libdir_index
)
if(libdir_index EQUAL -1)
  message(FATAL_ERROR "Multiarch pkg-config library path is incorrect")
endif()
