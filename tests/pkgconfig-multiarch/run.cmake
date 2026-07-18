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
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${MYLITE_SOURCE_DIR}"
    -B "${test_build}"
    -G "${MYLITE_GENERATOR}"
    "-DCMAKE_C_COMPILER=${MYLITE_C_COMPILER}"
    -DCMAKE_INSTALL_PREFIX=/usr
    -DCMAKE_INSTALL_LIBDIR=lib/x86_64-linux-gnu
    -DBUILD_TESTING=OFF
    -DMYLITE_BUILD_TOOLS=OFF
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
