add_executable(mylite_lemon
  "${PROJECT_SOURCE_DIR}/third_party/sqlite/upstream/tool/lemon.c"
)

set_target_properties(mylite_lemon PROPERTIES
  OUTPUT_NAME lemon
)

add_custom_command(TARGET mylite_lemon POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${PROJECT_SOURCE_DIR}/third_party/sqlite/upstream/tool/lempar.c"
    "$<TARGET_FILE_DIR:mylite_lemon>/lempar.c"
  COMMENT "Copying Lemon parser template"
  VERBATIM
)
