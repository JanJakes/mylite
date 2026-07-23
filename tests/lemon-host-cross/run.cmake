foreach(required_variable
    MYLITE_BUILD_DIR
    MYLITE_C_COMPILER
    MYLITE_GENERATOR
    MYLITE_SOURCE_DIR)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "Lemon cross test requires ${required_variable}")
  endif()
endforeach()

set(test_root "${MYLITE_BUILD_DIR}/lemon-host-cross")
set(cross_build "${test_root}/build")
set(target_compiler "${test_root}/target-compiler")
set(toolchain_file "${test_root}/toolchain.cmake")
file(REMOVE_RECURSE "${test_root}")
file(MAKE_DIRECTORY "${test_root}")

file(WRITE "${target_compiler}"
  "#!/bin/sh\n"
  "for argument in \"$@\"; do\n"
  "  if [ \"$argument\" = \"-c\" ]; then\n"
  "    exec \"${MYLITE_C_COMPILER}\" \"$@\"\n"
  "  fi\n"
  "done\n"
  "echo 'target compiler cannot link host executables' >&2\n"
  "exit 1\n"
)
file(CHMOD "${target_compiler}"
  PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
)
file(WRITE "${toolchain_file}"
  "set(CMAKE_SYSTEM_NAME Generic)\n"
  "set(CMAKE_C_COMPILER \"${target_compiler}\")\n"
  "set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)\n"
)

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${MYLITE_SOURCE_DIR}/tests/lemon-host-cross"
    -B "${cross_build}"
    -G "${MYLITE_GENERATOR}"
    "-DCMAKE_TOOLCHAIN_FILE=${toolchain_file}"
    "-DMYLITE_HOST_C_COMPILER=${MYLITE_C_COMPILER}"
    "-DMYLITE_SOURCE_DIR=${MYLITE_SOURCE_DIR}"
  RESULT_VARIABLE configure_result
  OUTPUT_VARIABLE configure_output
  ERROR_VARIABLE configure_error
)
if(NOT configure_result EQUAL 0)
  message(FATAL_ERROR
    "Lemon cross fixture configuration failed\n"
    "stdout:\n${configure_output}\n"
    "stderr:\n${configure_error}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    --build "${cross_build}"
    --target lemon_fixture_parser
  RESULT_VARIABLE first_build_result
  OUTPUT_VARIABLE first_build_output
  ERROR_VARIABLE first_build_error
)
if(NOT first_build_result EQUAL 0)
  message(FATAL_ERROR
    "Lemon cross fixture build failed\n"
    "stdout:\n${first_build_output}\n"
    "stderr:\n${first_build_error}")
endif()

set(generated_c "${cross_build}/generated/fixture.c")
set(generated_h "${cross_build}/generated/fixture.h")
if(NOT EXISTS "${generated_c}" OR NOT EXISTS "${generated_h}")
  message(FATAL_ERROR "Lemon cross fixture did not generate both parser files")
endif()
file(SHA256 "${generated_c}" generated_c_hash)
file(SHA256 "${generated_h}" generated_h_hash)

file(REMOVE "${generated_c}" "${generated_h}")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    --build "${cross_build}"
    --target lemon_fixture_parser
  RESULT_VARIABLE cached_build_result
  OUTPUT_VARIABLE cached_build_output
  ERROR_VARIABLE cached_build_error
)
if(NOT cached_build_result EQUAL 0)
  message(FATAL_ERROR
    "Cached Lemon cross fixture build failed\n"
    "stdout:\n${cached_build_output}\n"
    "stderr:\n${cached_build_error}")
endif()
if(NOT cached_build_output MATCHES "Restoring cached Lemon parser fixture")
  message(FATAL_ERROR
    "Second Lemon build did not restore the parser cache\n"
    "stdout:\n${cached_build_output}")
endif()
file(SHA256 "${generated_c}" restored_c_hash)
file(SHA256 "${generated_h}" restored_h_hash)
if(NOT generated_c_hash STREQUAL restored_c_hash OR
   NOT generated_h_hash STREQUAL restored_h_hash)
  message(FATAL_ERROR "Restored Lemon parser differs from generated output")
endif()

file(APPEND "${cross_build}/lempar.c" "\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    --build "${cross_build}"
    --target lemon_fixture_parser
  RESULT_VARIABLE template_build_result
  OUTPUT_VARIABLE template_build_output
  ERROR_VARIABLE template_build_error
)
if(NOT template_build_result EQUAL 0)
  message(FATAL_ERROR
    "Template-invalidated Lemon build failed\n"
    "stdout:\n${template_build_output}\n"
    "stderr:\n${template_build_error}")
endif()
if(NOT template_build_output MATCHES "Generating uncached Lemon parser fixture")
  message(FATAL_ERROR
    "Template-only change did not invalidate the Lemon parser cache\n"
    "stdout:\n${template_build_output}")
endif()
