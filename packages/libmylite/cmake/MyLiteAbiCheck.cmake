foreach(required IN ITEMS NM_TOOL LIBRARY MANIFEST HEADER HEADER_MANIFEST)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E compare_files "${HEADER}" "${HEADER_MANIFEST}"
  RESULT_VARIABLE header_compare_result
)
if(NOT header_compare_result EQUAL 0)
  message(FATAL_ERROR
    "libmylite public declarations differ from ${HEADER_MANIFEST}; update the ABI epoch or the "
    "reviewed header contract"
  )
endif()

execute_process(
  COMMAND "${NM_TOOL}" -D --defined-only --format=posix "${LIBRARY}"
  RESULT_VARIABLE nm_result
  OUTPUT_VARIABLE nm_output
  ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0)
  message(FATAL_ERROR "Unable to read shared-library symbols: ${nm_error}")
endif()

string(REPLACE "\n" ";" nm_lines "${nm_output}")
set(actual_symbols)
foreach(line IN LISTS nm_lines)
  if(line MATCHES "^([^ ]+) ([TDBR]) ")
    list(APPEND actual_symbols "${CMAKE_MATCH_1}")
  endif()
endforeach()
list(SORT actual_symbols)
list(JOIN actual_symbols "\n" actual_text)
string(APPEND actual_text "\n")

file(READ "${MANIFEST}" expected_text)
string(REPLACE "\r\n" "\n" expected_text "${expected_text}")
if(NOT actual_text STREQUAL expected_text)
  set(actual_path "${CMAKE_CURRENT_BINARY_DIR}/libmylite-actual.symbols")
  file(WRITE "${actual_path}" "${actual_text}")
  message(FATAL_ERROR
    "libmylite ABI differs from ${MANIFEST}; actual symbols: ${actual_path}"
  )
endif()

message(STATUS "libmylite ABI matches ${MANIFEST} and ${HEADER_MANIFEST}")
