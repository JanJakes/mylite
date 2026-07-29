add_executable(mylite_benchmark EXCLUDE_FROM_ALL
  benchmarks/mylite_benchmark_csv.c
  benchmarks/mylite_benchmark_parse_expectations.c
  benchmarks/mylite_benchmark_runtime_stress.c
  benchmarks/mylite_benchmark_sql_mode.c
  benchmarks/mylite_benchmark.c
)
find_package(Threads REQUIRED)
target_link_libraries(mylite_benchmark PRIVATE MyLite::mylite Threads::Threads)
target_include_directories(mylite_benchmark PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
if(MYLITE_ENABLE_PROFILING)
  target_compile_definitions(mylite_benchmark PRIVATE MYLITE_ENABLE_PROFILING=1)
endif()
mylite_configure_c_target(mylite_benchmark)

add_executable(mylite_parser_recovery_benchmark EXCLUDE_FROM_ALL
  benchmarks/mylite_parser_recovery_benchmark.c
)
target_link_libraries(mylite_parser_recovery_benchmark PRIVATE MyLite::mylite)
target_include_directories(mylite_parser_recovery_benchmark PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
mylite_configure_c_target(mylite_parser_recovery_benchmark)

add_executable(mylite_spatial_validity_benchmark EXCLUDE_FROM_ALL
  benchmarks/mylite_spatial_validity_benchmark.c
)
target_link_libraries(mylite_spatial_validity_benchmark PRIVATE MyLite::mylite)
if(UNIX AND NOT APPLE)
  target_link_libraries(mylite_spatial_validity_benchmark PRIVATE m)
endif()
target_include_directories(mylite_spatial_validity_benchmark PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
mylite_configure_c_target(mylite_spatial_validity_benchmark)

add_executable(mylite_large_dataset_benchmark EXCLUDE_FROM_ALL
  benchmarks/mylite_large_dataset_benchmark.c
)
target_link_libraries(mylite_large_dataset_benchmark PRIVATE
  MyLite::mylite
  MyLite::sqlite
)
target_include_directories(mylite_large_dataset_benchmark PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
if(MYLITE_ENABLE_PROFILING)
  target_compile_definitions(mylite_large_dataset_benchmark PRIVATE MYLITE_ENABLE_PROFILING=1)
endif()
mylite_configure_c_target(mylite_large_dataset_benchmark)

add_executable(mylite_load_data_benchmark EXCLUDE_FROM_ALL
  benchmarks/mylite_load_data_benchmark.c
)
target_link_libraries(mylite_load_data_benchmark PRIVATE MyLite::mylite)
target_include_directories(mylite_load_data_benchmark PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
if(MYLITE_ENABLE_PROFILING)
  target_compile_definitions(mylite_load_data_benchmark PRIVATE MYLITE_ENABLE_PROFILING=1)
  set(mylite_load_data_benchmark_profile 1)
else()
  set(mylite_load_data_benchmark_profile 0)
endif()
mylite_configure_c_target(mylite_load_data_benchmark)

add_executable(mylite_large_dataset_system_benchmark EXCLUDE_FROM_ALL
  benchmarks/mylite_large_dataset_system_benchmark.c
)
target_link_libraries(mylite_large_dataset_system_benchmark PRIVATE
  MyLite::mylite
  MyLite::sqlite
  Threads::Threads
)
target_include_directories(mylite_large_dataset_system_benchmark PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
mylite_configure_c_target(mylite_large_dataset_system_benchmark)

add_executable(mylite_durable_autocommit_benchmark EXCLUDE_FROM_ALL
  benchmarks/mylite_durable_autocommit_benchmark.c
)
target_link_libraries(mylite_durable_autocommit_benchmark PRIVATE
  MyLite::mylite
  MyLite::sqlite
)
target_include_directories(mylite_durable_autocommit_benchmark PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
mylite_configure_c_target(mylite_durable_autocommit_benchmark)

add_custom_target(mylite_large_dataset_benchmark_check
  COMMAND "${CMAKE_COMMAND}"
    "-DBENCHMARK=$<TARGET_FILE:mylite_large_dataset_benchmark>"
    "-DSYSTEM_BENCHMARK=$<TARGET_FILE:mylite_large_dataset_system_benchmark>"
    "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/large-dataset-smoke.csv"
    "-DSYSTEM_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/large-dataset-system-smoke.csv"
    "-DDATABASE_BASE=${CMAKE_CURRENT_BINARY_DIR}/large-dataset-system-smoke"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/large_dataset_benchmark_cli_test.cmake"
  DEPENDS
    mylite_large_dataset_benchmark
    mylite_large_dataset_system_benchmark
  VERBATIM
)

add_custom_target(mylite_load_data_benchmark_check
  COMMAND "${CMAKE_COMMAND}"
    "-DBENCHMARK=$<TARGET_FILE:mylite_load_data_benchmark>"
    "-DOUTPUT_BASE=${CMAKE_CURRENT_BINARY_DIR}/load-data-smoke"
    "-DPROFILE=${mylite_load_data_benchmark_profile}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/load_data_benchmark_cli_test.cmake"
  DEPENDS mylite_load_data_benchmark
  VERBATIM
)
