if(NOT DEFINED MYLITE_RETRY_SOURCE_ROOT)
  message(FATAL_ERROR "MYLITE_RETRY_SOURCE_ROOT is required")
endif()
if(NOT DEFINED MYLITE_RETRY_SOURCE_BYTE_CEILING)
  message(FATAL_ERROR "MYLITE_RETRY_SOURCE_BYTE_CEILING is required")
endif()

set(retry_source_files
  mylite_parser_placeholders.c
  mylite_parser_placeholders_retry.inc
  mylite_parser_placeholders.h
)
set(retry_source_bytes 0)

foreach(retry_source_file IN LISTS retry_source_files)
  set(retry_source_path "${MYLITE_RETRY_SOURCE_ROOT}/${retry_source_file}")
  if(NOT EXISTS "${retry_source_path}")
    message(FATAL_ERROR "Retry source does not exist: ${retry_source_path}")
  endif()
  file(READ "${retry_source_path}" retry_source_contents)
  string(REPLACE "\r\n" "\n" retry_source_contents "${retry_source_contents}")
  string(LENGTH "${retry_source_contents}" retry_source_file_bytes)
  math(EXPR retry_source_bytes "${retry_source_bytes} + ${retry_source_file_bytes}")
endforeach()

if(retry_source_bytes GREATER MYLITE_RETRY_SOURCE_BYTE_CEILING)
  message(FATAL_ERROR
    "Parser retry source grew to ${retry_source_bytes} bytes; "
    "the ceiling is ${MYLITE_RETRY_SOURCE_BYTE_CEILING} bytes"
  )
endif()

message(STATUS
  "Parser retry source is ${retry_source_bytes}/${MYLITE_RETRY_SOURCE_BYTE_CEILING} bytes"
)
