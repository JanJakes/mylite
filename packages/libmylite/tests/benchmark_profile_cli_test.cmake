if(NOT DEFINED BENCHMARK OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "BENCHMARK and OUTPUT are required")
endif()

set(sentinel "existing profile output\n")
file(WRITE "${OUTPUT}" "${sentinel}")
execute_process(
  COMMAND "${BENCHMARK}"
    --only runtime
    --scenario runtime.unknown
    --iterations 1
    --warmup 0
    --profile-json "${OUTPUT}"
  RESULT_VARIABLE invalid_result
  OUTPUT_QUIET
  ERROR_QUIET
)
if(invalid_result EQUAL 0)
  message(FATAL_ERROR "unknown runtime scenario unexpectedly succeeded")
endif()
file(READ "${OUTPUT}" invalid_output)
if(NOT invalid_output STREQUAL sentinel)
  message(FATAL_ERROR "unknown runtime scenario changed the profile output")
endif()

execute_process(
  COMMAND "${BENCHMARK}"
    --only runtime
    --scenario runtime.wp_frontend_request.query2
    --iterations 1
    --samples 2
    --warmup 0
    --profile-json "${OUTPUT}"
  RESULT_VARIABLE valid_result
  OUTPUT_QUIET
  ERROR_VARIABLE valid_error
)
if(NOT valid_result EQUAL 0)
  message(FATAL_ERROR "profile benchmark failed: ${valid_error}")
endif()

file(STRINGS "${OUTPUT}" profile_lines)
list(LENGTH profile_lines profile_line_count)
if(NOT profile_line_count EQUAL 2)
  message(FATAL_ERROR "expected two profile samples, got ${profile_line_count}")
endif()
foreach(profile_line IN LISTS profile_lines)
  string(JSON scenario ERROR_VARIABLE json_error GET "${profile_line}" scenario)
  if(json_error OR NOT scenario STREQUAL "runtime.wp_frontend_request.query2")
    message(FATAL_ERROR "invalid profile scenario: ${json_error}")
  endif()
  string(JSON unattributed ERROR_VARIABLE json_error GET "${profile_line}" unattributed_ns)
  if(json_error OR unattributed LESS 0)
    message(FATAL_ERROR "invalid unattributed time: ${json_error}")
  endif()
  string(JSON sqlite_step_count ERROR_VARIABLE json_error GET "${profile_line}" sqlite_step_count)
  if(json_error OR sqlite_step_count LESS 1)
    message(FATAL_ERROR "invalid SQLite step count: ${json_error}")
  endif()
  string(JSON sqlite_step_ns ERROR_VARIABLE json_error GET "${profile_line}" sqlite_step_ns)
  if(json_error OR sqlite_step_ns LESS 1)
    message(FATAL_ERROR "invalid SQLite step time: ${json_error}")
  endif()
endforeach()
