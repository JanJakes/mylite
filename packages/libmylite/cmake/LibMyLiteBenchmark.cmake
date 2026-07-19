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
