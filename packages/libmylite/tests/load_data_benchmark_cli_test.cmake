cmake_policy(SET CMP0007 NEW)

if(NOT DEFINED BENCHMARK OR NOT DEFINED OUTPUT_BASE OR NOT DEFINED PROFILE)
  message(FATAL_ERROR "BENCHMARK, OUTPUT_BASE, and PROFILE are required")
endif()

set(cases
  "narrow|0|2"
  "narrow|5|1"
  "wide|0|1"
  "many|0|1"
  "escaped|0|1"
)
foreach(case IN LISTS cases)
  set(case_key "${case}")
  string(REPLACE "|" "_" output_key "${case}")
  string(REPLACE "|" ";" case_fields "${case}")
  list(GET case_fields 0 shape)
  list(GET case_fields 1 indexes)
  list(GET case_fields 2 samples)
  set(output "${OUTPUT_BASE}-${output_key}.csv")

  execute_process(
    COMMAND "${BENCHMARK}"
      --shape "${shape}"
      --indexes "${indexes}"
      --rows 25
      --samples "${samples}"
      --warmup 0
      --output "${output}"
    RESULT_VARIABLE benchmark_result
    ERROR_VARIABLE benchmark_error
  )
  if(NOT benchmark_result EQUAL 0)
    message(FATAL_ERROR
      "LOAD DATA benchmark ${case_key} failed: ${benchmark_error}"
    )
  endif()

  file(STRINGS "${output}" output_lines)
  list(LENGTH output_lines line_count)
  math(EXPR expected_line_count "${samples} + 1")
  if(NOT line_count EQUAL expected_line_count)
    message(FATAL_ERROR
      "LOAD DATA benchmark ${case_key} emitted ${line_count} lines"
    )
  endif()
  list(GET output_lines 0 header)
  if(NOT header STREQUAL
     "record,shape,index_count,rows,sample,input_bytes,total_ms,rows_per_second,bytes_per_second,process_peak_rss_kib,affected_rows,result_checksum,allocations,allocation_bytes,sqlite_steps,metadata_steps")
    message(FATAL_ERROR "LOAD DATA benchmark header changed: ${header}")
  endif()

  foreach(sample RANGE 1 ${samples})
    list(GET output_lines ${sample} line)
    string(REPLACE "," ";" fields "${line}")
    list(LENGTH fields field_count)
    if(NOT field_count EQUAL 16)
      message(FATAL_ERROR
        "LOAD DATA benchmark ${case_key} row has ${field_count} fields: ${line}"
      )
    endif()
    list(GET fields 0 record)
    list(GET fields 1 actual_shape)
    list(GET fields 2 actual_indexes)
    list(GET fields 3 rows)
    list(GET fields 4 actual_sample)
    list(GET fields 5 input_bytes)
    list(GET fields 6 total_ms)
    list(GET fields 7 rows_per_second)
    list(GET fields 8 bytes_per_second)
    list(GET fields 9 peak_rss_kib)
    list(GET fields 10 affected_rows)
    list(GET fields 11 checksum)
    list(GET fields 12 allocations)
    list(GET fields 13 allocation_bytes)
    list(GET fields 14 sqlite_steps)
    list(GET fields 15 metadata_steps)
    if(NOT record STREQUAL "measurement"
        OR NOT actual_shape STREQUAL shape
        OR NOT actual_indexes EQUAL indexes
        OR NOT rows EQUAL 25
        OR NOT actual_sample EQUAL sample
        OR input_bytes LESS 1
        OR total_ms LESS_EQUAL 0
        OR rows_per_second LESS_EQUAL 0
        OR bytes_per_second LESS_EQUAL 0
        OR (NOT WIN32 AND peak_rss_kib LESS 1)
        OR NOT affected_rows EQUAL 25
        OR checksum EQUAL 0)
      message(FATAL_ERROR
        "LOAD DATA benchmark ${case_key} emitted invalid values: ${line}"
      )
    endif()
    if(PROFILE)
      if(allocations LESS 1
          OR NOT allocations LESS 64
          OR allocation_bytes LESS 1
          OR NOT allocation_bytes LESS 1048576
          OR sqlite_steps LESS 25
          OR metadata_steps LESS 1)
        message(FATAL_ERROR
          "LOAD DATA benchmark ${case_key} profile is invalid: ${line}"
        )
      endif()
    elseif(NOT allocations EQUAL 0
        OR NOT allocation_bytes EQUAL 0
        OR NOT sqlite_steps EQUAL 0
        OR NOT metadata_steps EQUAL 0)
      message(FATAL_ERROR
        "uninstrumented LOAD DATA benchmark contains counters: ${line}"
      )
    endif()
  endforeach()
endforeach()

execute_process(
  COMMAND "${BENCHMARK}" --list-shapes
  RESULT_VARIABLE list_result
  OUTPUT_VARIABLE shape_list
)
if(NOT list_result EQUAL 0
    OR NOT shape_list MATCHES "narrow"
    OR NOT shape_list MATCHES "wide"
    OR NOT shape_list MATCHES "many"
    OR NOT shape_list MATCHES "escaped")
  message(FATAL_ERROR "LOAD DATA benchmark shape listing is invalid")
endif()

execute_process(
  COMMAND "${BENCHMARK}"
    --shape wide
    --indexes 5
    --rows 1
    --samples 1
    --warmup 0
  RESULT_VARIABLE invalid_result
  OUTPUT_QUIET
  ERROR_QUIET
)
if(invalid_result EQUAL 0)
  message(FATAL_ERROR "LOAD DATA benchmark accepted indexes for a non-narrow shape")
endif()
