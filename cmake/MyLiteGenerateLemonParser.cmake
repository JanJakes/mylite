foreach(required_variable
    BASENAME
    CACHE_DIR
    COMPILER_CONTEXT
    GRAMMAR
    LEMON_EXECUTABLE
    LEMON_SOURCE
    OUTPUT_DIR
    TEMPLATE)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR
      "MyLiteGenerateLemonParser requires ${required_variable}")
  endif()
endforeach()

foreach(required_file
    COMPILER_CONTEXT
    GRAMMAR
    LEMON_EXECUTABLE
    LEMON_SOURCE
    TEMPLATE)
  if(NOT EXISTS "${${required_file}}")
    message(FATAL_ERROR
      "${required_file} does not exist: ${${required_file}}")
  endif()
endforeach()

file(SHA256 "${COMPILER_CONTEXT}" compiler_context_hash)
file(SHA256 "${GRAMMAR}" grammar_hash)
file(SHA256 "${LEMON_EXECUTABLE}" lemon_executable_hash)
file(SHA256 "${LEMON_SOURCE}" lemon_source_hash)
file(SHA256 "${TEMPLATE}" template_hash)
set(cache_input
  "${BASENAME}|${compiler_context_hash}|${grammar_hash}|"
  "${lemon_executable_hash}|${lemon_source_hash}|${template_hash}")
string(SHA256 cache_key "${cache_input}")

set(cache_entry "${CACHE_DIR}/lemon/${cache_key}")
set(cached_c "${cache_entry}/${BASENAME}.c")
set(cached_h "${cache_entry}/${BASENAME}.h")
set(output_c "${OUTPUT_DIR}/${BASENAME}.c")
set(output_h "${OUTPUT_DIR}/${BASENAME}.h")

file(MAKE_DIRECTORY "${CACHE_DIR}/lemon")
file(LOCK
  "${CACHE_DIR}/lemon/${cache_key}.lock"
  GUARD PROCESS
  TIMEOUT 300
)

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
if(EXISTS "${cached_c}" AND EXISTS "${cached_h}")
  message(STATUS "Restoring cached Lemon parser ${BASENAME}")
  file(COPY_FILE "${cached_c}" "${output_c}" ONLY_IF_DIFFERENT)
  file(COPY_FILE "${cached_h}" "${output_h}" ONLY_IF_DIFFERENT)
  return()
endif()

message(STATUS "Generating uncached Lemon parser ${BASENAME}")
execute_process(
  COMMAND "${LEMON_EXECUTABLE}"
    -q
    "-T${TEMPLATE}"
    "-d${OUTPUT_DIR}"
    "${GRAMMAR}"
  RESULT_VARIABLE lemon_result
  OUTPUT_VARIABLE lemon_output
  ERROR_VARIABLE lemon_error
)
if(NOT lemon_result EQUAL 0)
  message(FATAL_ERROR
    "Lemon failed with status ${lemon_result}\n"
    "stdout:\n${lemon_output}\n"
    "stderr:\n${lemon_error}")
endif()
if(NOT EXISTS "${output_c}" OR NOT EXISTS "${output_h}")
  message(FATAL_ERROR
    "Lemon did not create ${output_c} and ${output_h}")
endif()

file(MAKE_DIRECTORY "${cache_entry}")
file(COPY_FILE "${output_c}" "${cached_c}" ONLY_IF_DIFFERENT)
file(COPY_FILE "${output_h}" "${cached_h}" ONLY_IF_DIFFERENT)
