if(NOT DEFINED TCLSH)
  message(FATAL_ERROR "TCLSH is required")
endif()
if(NOT DEFINED SCRIPT)
  message(FATAL_ERROR "SCRIPT is required")
endif()
if(NOT DEFINED PARSE_HEADER)
  message(FATAL_ERROR "PARSE_HEADER is required")
endif()
if(NOT DEFINED VDBE_SOURCE)
  message(FATAL_ERROR "VDBE_SOURCE is required")
endif()
if(NOT DEFINED OUTPUT)
  message(FATAL_ERROR "OUTPUT is required")
endif()

set(input "${OUTPUT}.input")
file(READ "${PARSE_HEADER}" parse_header)
file(READ "${VDBE_SOURCE}" vdbe_source)
file(WRITE "${input}" "${parse_header}${vdbe_source}")

execute_process(
  COMMAND "${TCLSH}" "${SCRIPT}"
  INPUT_FILE "${input}"
  OUTPUT_FILE "${OUTPUT}"
  ERROR_VARIABLE error_output
  RESULT_VARIABLE result
)
file(REMOVE "${input}")

if(NOT result EQUAL 0)
  message(FATAL_ERROR "failed to generate ${OUTPUT}: ${error_output}")
endif()
