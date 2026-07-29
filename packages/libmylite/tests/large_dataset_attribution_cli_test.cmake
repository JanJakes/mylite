cmake_policy(SET CMP0007 NEW)
cmake_policy(SET CMP0057 NEW)

if(NOT DEFINED BENCHMARK OR NOT DEFINED OUTPUT OR NOT DEFINED DATABASE_BASE)
  message(FATAL_ERROR "BENCHMARK, OUTPUT, and DATABASE_BASE are required")
endif()

execute_process(
  COMMAND "${BENCHMARK}"
    --attribution-seed
    --rows 100
    --samples 2
    --warmup 0
    --database-base "${DATABASE_BASE}"
    --output "${OUTPUT}"
  RESULT_VARIABLE benchmark_result
  ERROR_VARIABLE benchmark_error
)
if(NOT benchmark_result EQUAL 0)
  message(FATAL_ERROR "retained-write attribution smoke failed: ${benchmark_error}")
endif()

file(STRINGS "${OUTPUT}" output_lines)
set(program_count 0)
set(measurement_count 0)
set(expected_phases
  total
  accounts
  items
  tags
  support
  support.upsert_composite
  support.fanout
  support.restrict
  support.set_null
)
foreach(output_line IN LISTS output_lines)
  string(REPLACE "," ";" fields "${output_line}")
  list(LENGTH fields field_count)
  if(NOT field_count EQUAL 32)
    message(FATAL_ERROR "attribution row has ${field_count} fields: ${output_line}")
  endif()
  list(GET fields 0 record)
  if(record STREQUAL "program")
    math(EXPR program_count "${program_count} + 1")
    list(GET fields 6 physical_table)
    list(GET fields 7 parameter_count)
    list(GET fields 9 sql_hash)
    if(NOT physical_table MATCHES "^_mylite_user_table_[0-9]+$"
        OR parameter_count LESS 1
        OR sql_hash STREQUAL "0")
      message(FATAL_ERROR "invalid discovered attribution program: ${output_line}")
    endif()
  elseif(record STREQUAL "measurement")
    math(EXPR measurement_count "${measurement_count} + 1")
    list(GET fields 2 sample)
    list(GET fields 3 layer)
    list(GET fields 4 phase)
    list(GET fields 9 dataset_hash)
    list(GET fields 10 elapsed_ms)
    list(GET fields 11 cpu_ms)
    list(GET fields 12 sqlite_steps)
    list(GET fields 13 vm_steps)
    list(GET fields 21 metadata_vm_steps)
    list(GET fields 22 scalar_callbacks)
    list(GET fields 24 collation_callbacks)
    list(GET fields 28 dml_plans)
    list(GET fields 29 dml_plan_hits)
    if(NOT sample MATCHES "^[12]$"
        OR NOT phase IN_LIST expected_phases
        OR dataset_hash STREQUAL "0"
        OR elapsed_ms LESS 0
        OR cpu_ms LESS 0
        OR sqlite_steps LESS 1
        OR vm_steps LESS 1)
      message(FATAL_ERROR "invalid attribution measurement: ${output_line}")
    endif()
    if(phase STREQUAL "total")
      set("${sample}_${layer}_hash" "${dataset_hash}")
      set("${sample}_${layer}_vm_steps" "${vm_steps}")
      if(layer STREQUAL "sqlite")
        if(NOT metadata_vm_steps EQUAL 0
            OR NOT scalar_callbacks EQUAL 0
            OR NOT collation_callbacks EQUAL 0)
          message(FATAL_ERROR "native SQLite attribution counters are contaminated")
        endif()
      elseif(layer MATCHES "^mylite_(physical|guarded)$")
        if(NOT metadata_vm_steps EQUAL 0
            OR NOT scalar_callbacks EQUAL 0
            OR collation_callbacks LESS 1)
          message(FATAL_ERROR "direct MyLite attribution counters are invalid")
        endif()
      elseif(layer STREQUAL "mylite")
        if(metadata_vm_steps LESS 1
            OR collation_callbacks LESS 1
            OR NOT dml_plans EQUAL 11
            OR NOT dml_plan_hits EQUAL 1088)
          message(FATAL_ERROR "full MyLite attribution counters are invalid")
        endif()
      else()
        message(FATAL_ERROR "unknown attribution layer: ${layer}")
      endif()
    endif()
  endif()
endforeach()

if(NOT program_count EQUAL 11)
  message(FATAL_ERROR "expected 11 generated programs, got ${program_count}")
endif()
if(NOT measurement_count EQUAL 72)
  message(FATAL_ERROR "expected 72 attribution measurements, got ${measurement_count}")
endif()
foreach(sample 1 2)
  foreach(layer mylite_physical mylite_guarded mylite)
    if(NOT "${${sample}_${layer}_hash}" STREQUAL "${${sample}_sqlite_hash}")
      message(FATAL_ERROR "sample ${sample} ${layer} checksum differs from SQLite")
    endif()
  endforeach()
  if(NOT ${sample}_mylite_physical_vm_steps LESS ${sample}_mylite_guarded_vm_steps
      OR NOT ${sample}_mylite_guarded_vm_steps LESS ${sample}_mylite_vm_steps)
    message(FATAL_ERROR "sample ${sample} VM attribution layers are not ordered")
  endif()
endforeach()
