if(NOT DEFINED SIZE_TOOL OR NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "SIZE_TOOL, INPUT, and OUTPUT are required")
endif()

execute_process(
  COMMAND "${SIZE_TOOL}" -A -d "${INPUT}"
  RESULT_VARIABLE size_result
  OUTPUT_VARIABLE size_output
  ERROR_VARIABLE size_error
)
if(NOT size_result EQUAL 0)
  message(FATAL_ERROR "size report failed: ${size_error}")
endif()

get_filename_component(input_name "${INPUT}" NAME)
string(REPLACE "${INPUT}" "${input_name}" size_output "${size_output}")
set(report "artifact: ${input_name}\n\n[sections-and-objects]\n${size_output}")
if(DEFINED NM_TOOL AND NOT NM_TOOL STREQUAL "")
  execute_process(
    COMMAND "${NM_TOOL}" -S --size-sort --radix=d "${INPUT}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE nm_output
    ERROR_VARIABLE nm_error
  )
  if(nm_result EQUAL 0)
    string(APPEND report "\n[sorted-symbols]\n${nm_output}")
  else()
    string(APPEND report "\n[sorted-symbols]\nunavailable: ${nm_error}")
  endif()
endif()

file(WRITE "${OUTPUT}" "${report}")
message(STATUS "Wrote ${OUTPUT}")
