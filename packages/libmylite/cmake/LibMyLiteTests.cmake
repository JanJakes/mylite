add_library(mylite_test_support STATIC
  tests/mylite_test_assertions.c
  tests/mylite_test_paths.c
  tests/mylite_test_runtime.c
)
target_link_libraries(mylite_test_support PUBLIC MyLite::headers)
target_include_directories(mylite_test_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/tests")
mylite_configure_c_target(mylite_test_support)

add_library(mylite_parser_test_support OBJECT
  tests/parser_test_support.c
)
target_link_libraries(mylite_parser_test_support PRIVATE MyLite::mylite)
target_include_directories(mylite_parser_test_support PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
mylite_configure_c_target(mylite_parser_test_support)

function(mylite_add_manifest_test source profile condition)
  if(condition STREQUAL "profiling" AND NOT MYLITE_ENABLE_PROFILING)
    return()
  endif()
  if(condition STREQUAL "allocator-failpoints" AND
     NOT MYLITE_ENABLE_TEST_ALLOCATOR_FAILPOINTS)
    return()
  endif()
  if(NOT condition MATCHES "^(always|profiling|allocator-failpoints)$")
    message(FATAL_ERROR "Unknown native-test condition: ${condition}")
  endif()

  get_filename_component(source_stem "${source}" NAME_WE)
  set(target "mylite_${source_stem}")
  set(target_sources "${source}")

  if(profile STREQUAL "benchmark-csv")
    list(PREPEND target_sources benchmarks/mylite_benchmark_csv.c)
  elseif(profile STREQUAL "benchmark-parse")
    list(PREPEND target_sources benchmarks/mylite_benchmark_parse_expectations.c)
  elseif(profile STREQUAL "benchmark-sql-mode")
    list(PREPEND target_sources
      benchmarks/mylite_benchmark_csv.c
      benchmarks/mylite_benchmark_sql_mode.c
    )
  elseif(profile STREQUAL "parser")
    list(APPEND target_sources $<TARGET_OBJECTS:mylite_parser_test_support>)
  endif()

  add_executable("${target}" ${target_sources})
  target_link_libraries("${target}" PRIVATE mylite_test_support)

  if(profile STREQUAL "sqlite")
    target_link_libraries("${target}" PRIVATE MyLite::sqlite)
  elseif(profile STREQUAL "benchmark-csv")
    target_include_directories("${target}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks")
  else()
    target_link_libraries("${target}" PRIVATE MyLite::mylite)
  endif()

  if(profile MATCHES "^(internal|internal-sql|internal-sqlite|lexer|parser|profile)$")
    target_include_directories("${target}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  endif()
  if(profile MATCHES "^benchmark-(parse|sql-mode)$")
    target_include_directories("${target}" PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks"
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
  endif()
  if(profile STREQUAL "internal-sql")
    target_include_directories("${target}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/sql")
  elseif(profile STREQUAL "internal-sqlite")
    target_link_libraries("${target}" PRIVATE MyLite::sqlite)
  elseif(profile STREQUAL "lexer")
    target_compile_definitions("${target}" PRIVATE
      MYLITE_LEXER_CORPUS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/mysql_lexer_success.sql"
    )
  elseif(profile STREQUAL "profile")
    target_compile_definitions("${target}" PRIVATE MYLITE_ENABLE_PROFILING=1)
    add_dependencies("${target}" mylite_benchmark)
  elseif(NOT profile MATCHES
         "^(benchmark-csv|benchmark-parse|benchmark-sql-mode|internal|parser|public|sqlite)$")
    message(FATAL_ERROR "Unknown native-test profile: ${profile}")
  endif()

  mylite_configure_c_target("${target}")

  if(source_stem STREQUAL "offset_vfs_test")
    set(test_name "libmylite.storage.offset_vfs")
  elseif(source_stem STREQUAL "lexer_test")
    set(test_name "libmylite.lexer")
  else()
    string(REGEX REPLACE "_test$" "" test_stem "${source_stem}")
    if(test_stem MATCHES "^runtime_(.*)$")
      set(test_name "libmylite.runtime.${CMAKE_MATCH_1}")
    elseif(test_stem MATCHES "^parser_(.*)$")
      set(test_name "libmylite.parser.${CMAKE_MATCH_1}")
    elseif(test_stem MATCHES "^benchmark_(.*)$")
      set(test_name "libmylite.benchmark.${CMAKE_MATCH_1}")
    else()
      set(test_name "libmylite.${test_stem}")
    endif()
  endif()

  add_test(NAME "${test_name}" COMMAND "${target}")
endfunction()

set(mylite_native_test_manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/native-tests.txt")
file(STRINGS "${mylite_native_test_manifest}" mylite_native_test_entries)
set(mylite_manifest_sources)

foreach(manifest_entry IN LISTS mylite_native_test_entries)
  if(manifest_entry MATCHES "^#|^[ \t]*$")
    continue()
  endif()

  string(REPLACE "|" ";" manifest_fields "${manifest_entry}")
  list(LENGTH manifest_fields manifest_field_count)
  if(NOT manifest_field_count EQUAL 3)
    message(FATAL_ERROR "Invalid native-test manifest entry: ${manifest_entry}")
  endif()

  list(GET manifest_fields 0 test_source)
  list(GET manifest_fields 1 test_profile)
  list(GET manifest_fields 2 test_condition)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${test_source}")
    message(FATAL_ERROR "Native-test manifest source does not exist: ${test_source}")
  endif()
  if(test_source IN_LIST mylite_manifest_sources)
    message(FATAL_ERROR "Duplicate native-test manifest source: ${test_source}")
  endif()

  list(APPEND mylite_manifest_sources "${test_source}")
  mylite_add_manifest_test("${test_source}" "${test_profile}" "${test_condition}")
endforeach()

file(GLOB mylite_native_test_sources RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/*_test.c"
)
list(SORT mylite_native_test_sources)
list(SORT mylite_manifest_sources)
if(NOT mylite_native_test_sources STREQUAL mylite_manifest_sources)
  message(FATAL_ERROR
    "Native-test manifest is stale. Sources=${mylite_native_test_sources}; "
    "manifest=${mylite_manifest_sources}"
  )
endif()

add_test(
  NAME libmylite.runtime.alter_table_rename_to
  COMMAND mylite_runtime_table_rename_lifecycle_test
)

if(MYLITE_ENABLE_PROFILING)
  add_test(
    NAME libmylite.benchmark.profile_cli
    COMMAND "${CMAKE_COMMAND}"
      "-DBENCHMARK=$<TARGET_FILE:mylite_benchmark>"
      "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/benchmark-profile-cli.jsonl"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_profile_cli_test.cmake"
  )
endif()

set_tests_properties(
  libmylite.runtime.recovery_model
  PROPERTIES
    LABELS "crash;fault-injection;model;runtime"
    RESOURCE_LOCK mylite_recovery
    TIMEOUT 180
)
set_tests_properties(
  libmylite.runtime.show_processlist_introspection
  libmylite.runtime.transaction_lifecycle
  PROPERTIES
    LABELS "runtime;concurrency"
    RESOURCE_LOCK mylite_concurrency
    TIMEOUT 120
)
set_tests_properties(
  libmylite.runtime.auto_increment_lifecycle
  PROPERTIES
    LABELS "crash;runtime"
    RESOURCE_LOCK mylite_recovery
    TIMEOUT 180
)
set_tests_properties(
  libmylite.runtime.file_backed_open
  PROPERTIES
    LABELS "crash;runtime;storage"
    RESOURCE_LOCK mylite_recovery
    TIMEOUT 180
)
if(MYLITE_ENABLE_TEST_ALLOCATOR_FAILPOINTS)
  set_tests_properties(
    libmylite.runtime.allocator_failpoint
    PROPERTIES LABELS "fault-injection;runtime"
  )
endif()

include("${CMAKE_CURRENT_LIST_DIR}/LibMyLiteTestProperties.cmake")
