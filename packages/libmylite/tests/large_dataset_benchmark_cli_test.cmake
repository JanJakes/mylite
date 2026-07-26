if(NOT DEFINED BENCHMARK OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "BENCHMARK and OUTPUT are required")
endif()

execute_process(
  COMMAND "${BENCHMARK}"
    --rows 1000
    --samples 2
    --warmup 1
    --iterations 3
    --output "${OUTPUT}"
  RESULT_VARIABLE benchmark_result
  ERROR_VARIABLE benchmark_error
)
if(NOT benchmark_result EQUAL 0)
  message(FATAL_ERROR "large-dataset benchmark smoke failed: ${benchmark_error}")
endif()

file(READ "${OUTPUT}" benchmark_output)
foreach(expected
    "record,scenario,rows,engine,mode"
    "summary,load.total,1000,mylite,load"
    "summary,load.total,1000,sqlite,load"
    "sample,point_lookup_prepared,1000,mylite,prepared,1,3"
    "sample,point_lookup_prepared,1000,sqlite,prepared,1,3")
  string(FIND "${benchmark_output}" "${expected}" expected_offset)
  if(expected_offset EQUAL -1)
    message(FATAL_ERROR "large-dataset output is missing: ${expected}")
  endif()
endforeach()

set(expected_scenarios
  point_lookup_prepare_each
  point_lookup_prepared
  secondary_lookup
  range_aggregate
  full_scan_expression
  text_expression
  group_aggregate
  indexed_order_limit
  parent_join
  bridge_join
  correlated_exists
  indexed_update_rollback
  foreign_key_insert_rollback
  foreign_key_cascade_rollback
)
foreach(scenario IN LISTS expected_scenarios)
  foreach(engine mylite sqlite)
    set(expected "summary,${scenario},1000,${engine},")
    string(FIND "${benchmark_output}" "${expected}" expected_offset)
    if(expected_offset EQUAL -1)
      message(FATAL_ERROR "large-dataset output is missing: ${expected}")
    endif()
  endforeach()
endforeach()

execute_process(
  COMMAND "${BENCHMARK}" --scenario missing --rows 100
  RESULT_VARIABLE invalid_result
  OUTPUT_QUIET
  ERROR_QUIET
)
if(invalid_result EQUAL 0)
  message(FATAL_ERROR "unknown large-dataset scenario unexpectedly succeeded")
endif()
