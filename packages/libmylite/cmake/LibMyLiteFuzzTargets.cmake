function(mylite_add_fuzzer target source corpus max_length runs)
  set(corpus_source "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/${corpus}")
  set(corpus_build "${CMAKE_CURRENT_BINARY_DIR}/fuzz/corpus/${corpus}")
  set(artifact_directory "${CMAKE_CURRENT_BINARY_DIR}/fuzz/artifacts")

  file(MAKE_DIRECTORY "${corpus_build}" "${artifact_directory}")
  file(COPY "${corpus_source}/" DESTINATION "${corpus_build}")

  add_executable("${target}" "${source}")
  target_link_libraries("${target}" PRIVATE MyLite::mylite)
  target_include_directories("${target}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_compile_options("${target}" PRIVATE -fsanitize=fuzzer-no-link)
  target_link_options("${target}" PRIVATE -fsanitize=fuzzer)
  mylite_configure_c_target("${target}")

  add_test(
    NAME "libmylite.fuzz.${corpus}"
    COMMAND "${target}"
      "-runs=${runs}"
      -seed=1
      "-max_len=${max_length}"
      -print_final_stats=1
      "-artifact_prefix=${artifact_directory}/${corpus}-"
      "${corpus_build}"
  )
  set_tests_properties("libmylite.fuzz.${corpus}" PROPERTIES
    LABELS "fuzz;sanitizer"
    TIMEOUT 120
  )
endfunction()

mylite_add_fuzzer(
  mylite_parser_fuzzer
  fuzz/parser_fuzzer.c
  parser
  65536
  10000
)
mylite_add_fuzzer(
  mylite_sql_normalization_fuzzer
  fuzz/sql_normalization_fuzzer.c
  sql_normalization
  65536
  10000
)
mylite_add_fuzzer(
  mylite_json_fuzzer
  fuzz/json_fuzzer.c
  json
  65536
  10000
)
mylite_add_fuzzer(
  mylite_geometry_fuzzer
  fuzz/geometry_fuzzer.c
  geometry
  65536
  10000
)
mylite_add_fuzzer(
  mylite_file_preamble_fuzzer
  fuzz/file_preamble_fuzzer.c
  file_preamble
  4096
  10000
)

set(mylite_fuzzer_targets
  mylite_parser_fuzzer
  mylite_sql_normalization_fuzzer
  mylite_json_fuzzer
  mylite_geometry_fuzzer
  mylite_file_preamble_fuzzer
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  mylite_add_fuzzer(
    mylite_database_open_fuzzer
    fuzz/database_open_fuzzer.c
    database_open
    65536
    1000
  )
  list(APPEND mylite_fuzzer_targets mylite_database_open_fuzzer)
endif()

add_custom_target(mylite_fuzzers DEPENDS ${mylite_fuzzer_targets})
