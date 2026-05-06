if(NOT DEFINED TCLSH)
  message(FATAL_ERROR "TCLSH is required")
endif()
if(NOT DEFINED SCRIPT)
  message(FATAL_ERROR "SCRIPT is required")
endif()
if(NOT DEFINED OUTPUT)
  message(FATAL_ERROR "OUTPUT is required")
endif()

execute_process(
  COMMAND "${TCLSH}" "${SCRIPT}" ${SCRIPT_ARGUMENTS}
  OUTPUT_FILE "${OUTPUT}"
  ERROR_VARIABLE error_output
  RESULT_VARIABLE result
)

if(NOT result EQUAL 0)
  message(FATAL_ERROR "failed to generate ${OUTPUT}: ${error_output}")
endif()
