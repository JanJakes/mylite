if(NOT DEFINED BENCHMARK
    OR NOT DEFINED SYSTEM_BENCHMARK
    OR NOT DEFINED OUTPUT
    OR NOT DEFINED SYSTEM_OUTPUT
    OR NOT DEFINED DATABASE_BASE)
  message(FATAL_ERROR
    "BENCHMARK, SYSTEM_BENCHMARK, OUTPUT, SYSTEM_OUTPUT, and DATABASE_BASE are required"
  )
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
  selectivity_zero
  selectivity_one
  selectivity_001pct
  selectivity_1pct
  selectivity_10pct
  selectivity_full
  covering_composite_range
  noncovering_composite_range
  indexed_null_lookup
  primary_key_or_lookup
  deep_offset_90pct
  unindexed_sort_top_1000
  high_cardinality_group
  high_cardinality_distinct
  window_partition_rank
  three_table_join
  left_join_range
  anti_join
  large_large_bounded_join
  skew_hot_tenant
  skew_cold_tenant
  update_001pct_rollback
  update_1pct_rollback
  update_10pct_rollback
  delete_001pct_rollback
  delete_1pct_rollback
  upsert_hit_rollback
  upsert_miss_rollback
  insert_zero_indexes_rollback
  insert_one_index_rollback
  insert_five_indexes_rollback
  insert_ten_indexes_rollback
  insert_batch_10_rollback
  composite_foreign_key_insert_rollback
  composite_foreign_key_invalid_rollback
  foreign_key_cascade_fanout_rollback
  foreign_key_restrict_miss_rollback
  foreign_key_set_null_fanout_rollback
  result_narrow_100
  result_narrow_10000
  result_narrow_full
  result_wide_100
  result_wide_10000
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

file(REMOVE
  "${DATABASE_BASE}.mylite"
  "${DATABASE_BASE}.mylite-journal"
  "${DATABASE_BASE}.mylite-wal"
  "${DATABASE_BASE}.mylite-shm"
  "${DATABASE_BASE}.sqlite"
  "${DATABASE_BASE}.sqlite-journal"
  "${DATABASE_BASE}.sqlite-wal"
  "${DATABASE_BASE}.sqlite-shm"
)
execute_process(
  COMMAND "${BENCHMARK}"
    --rows 1000
    --database-base "${DATABASE_BASE}"
    --seed-only
  RESULT_VARIABLE seed_result
  OUTPUT_QUIET
  ERROR_VARIABLE seed_error
)
if(NOT seed_result EQUAL 0)
  message(FATAL_ERROR "large-dataset system seed failed: ${seed_error}")
endif()

execute_process(
  COMMAND "${SYSTEM_BENCHMARK}"
    --database-base "${DATABASE_BASE}"
    --rows 1000
    --iterations 2
    --output "${SYSTEM_OUTPUT}"
  RESULT_VARIABLE system_result
  ERROR_VARIABLE system_error
)
if(NOT system_result EQUAL 0)
  message(FATAL_ERROR "large-dataset system benchmark smoke failed: ${system_error}")
endif()
file(READ "${SYSTEM_OUTPUT}" system_output)
foreach(expected
    "warm_reopen_count,1000,mylite"
    "warm_reopen_count,1000,sqlite"
    "parallel_readers_4,1000,mylite"
    "readers_4_writer_1,1000,sqlite"
    "long_reader_writer,1000,mylite"
    "delete_commit_reclaim,1000,sqlite")
  string(FIND "${system_output}" "${expected}" expected_offset)
  if(expected_offset EQUAL -1)
    message(FATAL_ERROR "large-dataset system output is missing: ${expected}")
  endif()
endforeach()
file(REMOVE
  "${DATABASE_BASE}.mylite"
  "${DATABASE_BASE}.mylite-journal"
  "${DATABASE_BASE}.mylite-wal"
  "${DATABASE_BASE}.mylite-shm"
  "${DATABASE_BASE}.sqlite"
  "${DATABASE_BASE}.sqlite-journal"
  "${DATABASE_BASE}.sqlite-wal"
  "${DATABASE_BASE}.sqlite-shm"
)

execute_process(
  COMMAND "${BENCHMARK}" --scenario missing --rows 100
  RESULT_VARIABLE invalid_result
  OUTPUT_QUIET
  ERROR_QUIET
)
if(invalid_result EQUAL 0)
  message(FATAL_ERROR "unknown large-dataset scenario unexpectedly succeeded")
endif()
