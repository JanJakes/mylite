  add_executable(mylite_runtime_open_memory_test
    tests/runtime_open_memory_test.c
  )
  target_link_libraries(mylite_runtime_open_memory_test PRIVATE MyLite::mylite)
  target_include_directories(mylite_runtime_open_memory_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_open_memory_test)

  add_executable(mylite_runtime_file_backed_open_test
    tests/runtime_file_backed_open_test.c
  )
  target_link_libraries(mylite_runtime_file_backed_open_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_file_backed_open_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_file_backed_open_test)

  add_executable(mylite_runtime_catalog_foundation_test
    tests/runtime_catalog_foundation_test.c
  )
  target_link_libraries(mylite_runtime_catalog_foundation_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_catalog_foundation_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_catalog_foundation_test)

  add_executable(mylite_runtime_basic_table_lifecycle_test
    tests/runtime_basic_table_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_basic_table_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_basic_table_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_basic_table_lifecycle_test)

  add_executable(mylite_runtime_table_if_exists_lifecycle_test
    tests/runtime_table_if_exists_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_table_if_exists_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_table_if_exists_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_table_if_exists_lifecycle_test)

  add_executable(mylite_runtime_schema_lifecycle_test
    tests/runtime_schema_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_schema_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_schema_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_schema_lifecycle_test)

  add_executable(mylite_runtime_current_database_function_test
    tests/runtime_current_database_function_test.c
  )
  target_link_libraries(mylite_runtime_current_database_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_current_database_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_current_database_function_test)

  add_executable(mylite_runtime_current_user_identity_test
    tests/runtime_current_user_identity_test.c
  )
  target_link_libraries(mylite_runtime_current_user_identity_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_current_user_identity_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_current_user_identity_test)

  add_executable(mylite_runtime_connection_id_function_test
    tests/runtime_connection_id_function_test.c
  )
  target_link_libraries(mylite_runtime_connection_id_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_connection_id_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_connection_id_function_test)

  add_executable(mylite_runtime_version_function_test
    tests/runtime_version_function_test.c
  )
  target_link_libraries(mylite_runtime_version_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_version_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_version_function_test)

  add_executable(mylite_runtime_named_lock_and_info_functions_test
    tests/runtime_named_lock_and_info_functions_test.c
  )
  target_link_libraries(mylite_runtime_named_lock_and_info_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_named_lock_and_info_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_named_lock_and_info_functions_test)

  add_executable(mylite_runtime_system_function_residuals_test
    tests/runtime_system_function_residuals_test.c
  )
  target_link_libraries(mylite_runtime_system_function_residuals_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_system_function_residuals_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_system_function_residuals_test)

  add_executable(mylite_runtime_internal_native_helper_rejections_test
    tests/runtime_internal_native_helper_rejections_test.c
  )
  target_link_libraries(mylite_runtime_internal_native_helper_rejections_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_internal_native_helper_rejections_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_internal_native_helper_rejections_test)

  add_executable(mylite_runtime_roles_graphml_function_test
    tests/runtime_roles_graphml_function_test.c
  )
  target_link_libraries(mylite_runtime_roles_graphml_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_roles_graphml_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_roles_graphml_function_test)

  add_executable(mylite_runtime_statement_digest_text_function_test
    tests/runtime_statement_digest_text_function_test.c
  )
  target_link_libraries(mylite_runtime_statement_digest_text_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_statement_digest_text_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_statement_digest_text_function_test)

  add_executable(mylite_runtime_replication_functions_test
    tests/runtime_replication_functions_test.c
  )
  target_link_libraries(mylite_runtime_replication_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_replication_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_replication_functions_test)

  add_executable(mylite_runtime_sys_helper_functions_test
    tests/runtime_sys_helper_functions_test.c
  )
  target_link_libraries(mylite_runtime_sys_helper_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_helper_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_helper_functions_test)

  add_executable(mylite_runtime_sys_performance_schema_helper_functions_test
    tests/runtime_sys_performance_schema_helper_functions_test.c
  )
  target_link_libraries(mylite_runtime_sys_performance_schema_helper_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_performance_schema_helper_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_performance_schema_helper_functions_test)

  add_executable(mylite_runtime_sys_procedure_placeholders_test
    tests/runtime_sys_procedure_placeholders_test.c
  )
  target_link_libraries(mylite_runtime_sys_procedure_placeholders_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_procedure_placeholders_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_procedure_placeholders_test)

  add_executable(mylite_runtime_if_function_test
    tests/runtime_if_function_test.c
  )
  target_link_libraries(mylite_runtime_if_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_if_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_if_function_test)

  add_executable(mylite_runtime_ifnull_function_test
    tests/runtime_ifnull_function_test.c
  )
  target_link_libraries(mylite_runtime_ifnull_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_ifnull_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_ifnull_function_test)

  add_executable(mylite_runtime_coalesce_function_test
    tests/runtime_coalesce_function_test.c
  )
  target_link_libraries(mylite_runtime_coalesce_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_coalesce_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_coalesce_function_test)

  add_executable(mylite_runtime_nullif_function_test
    tests/runtime_nullif_function_test.c
  )
  target_link_libraries(mylite_runtime_nullif_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_nullif_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_nullif_function_test)

  add_executable(mylite_runtime_isnull_function_test
    tests/runtime_isnull_function_test.c
  )
  target_link_libraries(mylite_runtime_isnull_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_isnull_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_isnull_function_test)

  add_executable(mylite_runtime_case_operator_test
    tests/runtime_case_operator_test.c
  )
  target_link_libraries(mylite_runtime_case_operator_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_case_operator_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_case_operator_test)

  add_executable(mylite_runtime_do_statement_test
    tests/runtime_do_statement_test.c
  )
  target_link_libraries(mylite_runtime_do_statement_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_do_statement_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_do_statement_test)

  add_executable(mylite_runtime_scalar_expression_projection_test
    tests/runtime_scalar_expression_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_expression_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_expression_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_expression_projection_test)

  add_executable(mylite_runtime_row_cast_convert_projection_test
    tests/runtime_row_cast_convert_projection_test.c
  )
  target_link_libraries(mylite_runtime_row_cast_convert_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_row_cast_convert_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_row_cast_convert_projection_test)

  add_executable(mylite_runtime_optimizer_hints_noop_test
    tests/runtime_optimizer_hints_noop_test.c
  )
  target_link_libraries(mylite_runtime_optimizer_hints_noop_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_optimizer_hints_noop_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_optimizer_hints_noop_test)

  add_executable(mylite_runtime_explain_statement_test
    tests/runtime_explain_statement_test.c
  )
  target_link_libraries(mylite_runtime_explain_statement_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_explain_statement_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_explain_statement_test)

  add_executable(mylite_runtime_create_table_partition_options_test
    tests/runtime_create_table_partition_options_test.c
  )
  target_link_libraries(mylite_runtime_create_table_partition_options_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_create_table_partition_options_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_create_table_partition_options_test)

  add_executable(mylite_runtime_table_partition_selection_test
    tests/runtime_table_partition_selection_test.c
  )
  target_link_libraries(mylite_runtime_table_partition_selection_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_table_partition_selection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_table_partition_selection_test)

  add_executable(mylite_runtime_scalar_arithmetic_projection_test
    tests/runtime_scalar_arithmetic_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_arithmetic_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_arithmetic_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_arithmetic_projection_test)

  add_executable(mylite_runtime_scalar_modulo_projection_test
    tests/runtime_scalar_modulo_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_modulo_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_modulo_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_modulo_projection_test)

  add_executable(mylite_runtime_scalar_div_projection_test
    tests/runtime_scalar_div_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_div_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_div_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_div_projection_test)

  add_executable(mylite_runtime_scalar_division_projection_test
    tests/runtime_scalar_division_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_division_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_division_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_division_projection_test)

  add_executable(mylite_runtime_scalar_bitwise_projection_test
    tests/runtime_scalar_bitwise_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_bitwise_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_bitwise_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_bitwise_projection_test)

  add_executable(mylite_runtime_row_bitwise_expressions_test
    tests/runtime_row_bitwise_expressions_test.c
  )
  target_link_libraries(mylite_runtime_row_bitwise_expressions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_row_bitwise_expressions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_row_bitwise_expressions_test)

  add_executable(mylite_runtime_abs_function_test
    tests/runtime_abs_function_test.c
  )
  target_link_libraries(mylite_runtime_abs_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_abs_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_abs_function_test)

  add_executable(mylite_runtime_sign_function_test
    tests/runtime_sign_function_test.c
  )
  target_link_libraries(mylite_runtime_sign_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sign_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sign_function_test)

  add_executable(mylite_runtime_ceil_floor_functions_test
    tests/runtime_ceil_floor_functions_test.c
  )
  target_link_libraries(mylite_runtime_ceil_floor_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_ceil_floor_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_ceil_floor_functions_test)

  add_executable(mylite_runtime_round_function_test
    tests/runtime_round_function_test.c
  )
  target_link_libraries(mylite_runtime_round_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_round_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_round_function_test)

  add_executable(mylite_runtime_bin_oct_functions_test
    tests/runtime_bin_oct_functions_test.c
  )
  target_link_libraries(mylite_runtime_bin_oct_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_bin_oct_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_bin_oct_functions_test)

  add_executable(mylite_runtime_conv_function_test
    tests/runtime_conv_function_test.c
  )
  target_link_libraries(mylite_runtime_conv_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_conv_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_conv_function_test)

  add_executable(mylite_runtime_pi_function_test
    tests/runtime_pi_function_test.c
  )
  target_link_libraries(mylite_runtime_pi_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_pi_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_pi_function_test)

  add_executable(mylite_runtime_rand_function_test
    tests/runtime_rand_function_test.c
  )
  target_link_libraries(mylite_runtime_rand_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_rand_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_rand_function_test)

  add_executable(mylite_runtime_sqrt_function_test
    tests/runtime_sqrt_function_test.c
  )
  target_link_libraries(mylite_runtime_sqrt_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sqrt_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sqrt_function_test)

  add_executable(mylite_runtime_degrees_radians_functions_test
    tests/runtime_degrees_radians_functions_test.c
  )
  target_link_libraries(mylite_runtime_degrees_radians_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_degrees_radians_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_degrees_radians_functions_test)

  add_executable(mylite_runtime_acos_asin_functions_test
    tests/runtime_acos_asin_functions_test.c
  )
  target_link_libraries(mylite_runtime_acos_asin_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_acos_asin_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_acos_asin_functions_test)

  add_executable(mylite_runtime_atan_functions_test
    tests/runtime_atan_functions_test.c
  )
  target_link_libraries(mylite_runtime_atan_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_atan_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_atan_functions_test)

  add_executable(mylite_runtime_trigonometric_functions_test
    tests/runtime_trigonometric_functions_test.c
  )
  target_link_libraries(mylite_runtime_trigonometric_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_trigonometric_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_trigonometric_functions_test)

  add_executable(mylite_runtime_exp_log_power_functions_test
    tests/runtime_exp_log_power_functions_test.c
  )
  target_link_libraries(mylite_runtime_exp_log_power_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_exp_log_power_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_exp_log_power_functions_test)

  add_executable(mylite_runtime_bit_count_function_test
    tests/runtime_bit_count_function_test.c
  )
  target_link_libraries(mylite_runtime_bit_count_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_bit_count_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_bit_count_function_test)

  add_executable(mylite_runtime_row_scalar_numeric_functions_test
    tests/runtime_row_scalar_numeric_functions_test.c
  )
  target_link_libraries(mylite_runtime_row_scalar_numeric_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_row_scalar_numeric_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_row_scalar_numeric_functions_test)

  add_executable(mylite_runtime_numeric_format_truncate_crc32_test
    tests/runtime_numeric_format_truncate_crc32_test.c
  )
  target_link_libraries(mylite_runtime_numeric_format_truncate_crc32_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_numeric_format_truncate_crc32_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_numeric_format_truncate_crc32_test)

  add_executable(mylite_runtime_row_scalar_numeric_comparison_update_contexts_test
    tests/runtime_row_scalar_numeric_comparison_update_contexts_test.c
  )
  target_link_libraries(
    mylite_runtime_row_scalar_numeric_comparison_update_contexts_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(
    mylite_runtime_row_scalar_numeric_comparison_update_contexts_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_row_scalar_numeric_comparison_update_contexts_test)

  add_executable(mylite_runtime_hex_function_test
    tests/runtime_hex_function_test.c
  )
  target_link_libraries(mylite_runtime_hex_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_hex_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_hex_function_test)

  add_executable(mylite_runtime_base64_functions_test
    tests/runtime_base64_functions_test.c
  )
  target_link_libraries(mylite_runtime_base64_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_base64_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_base64_functions_test)

  add_executable(mylite_runtime_compression_random_functions_test
    tests/runtime_compression_random_functions_test.c
  )
  target_link_libraries(mylite_runtime_compression_random_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_compression_random_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_compression_random_functions_test)

  add_executable(mylite_runtime_aes_encryption_functions_test
    tests/runtime_aes_encryption_functions_test.c
  )
  target_link_libraries(mylite_runtime_aes_encryption_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_aes_encryption_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_aes_encryption_functions_test)

  add_executable(mylite_runtime_ip_address_functions_test
    tests/runtime_ip_address_functions_test.c
  )
  target_link_libraries(mylite_runtime_ip_address_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_ip_address_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_ip_address_functions_test)

  add_executable(mylite_runtime_digest_functions_test
    tests/runtime_digest_functions_test.c
  )
  target_link_libraries(mylite_runtime_digest_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_digest_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_digest_functions_test)

  add_executable(mylite_runtime_validate_password_strength_function_test
    tests/runtime_validate_password_strength_function_test.c
  )
  target_link_libraries(mylite_runtime_validate_password_strength_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_validate_password_strength_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_validate_password_strength_function_test)

  add_executable(mylite_runtime_unhex_function_test
    tests/runtime_unhex_function_test.c
  )
  target_link_libraries(mylite_runtime_unhex_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_unhex_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_unhex_function_test)

  add_executable(mylite_runtime_uuid_conversion_functions_test
    tests/runtime_uuid_conversion_functions_test.c
  )
  target_link_libraries(mylite_runtime_uuid_conversion_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_uuid_conversion_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_uuid_conversion_functions_test)

  add_executable(mylite_runtime_uuid_function_test
    tests/runtime_uuid_function_test.c
  )
  target_link_libraries(mylite_runtime_uuid_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_uuid_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_uuid_function_test)

  add_executable(mylite_runtime_char_function_test
    tests/runtime_char_function_test.c
  )
  target_link_libraries(mylite_runtime_char_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_char_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_char_function_test)

  add_executable(mylite_runtime_default_function_test
    tests/runtime_default_function_test.c
  )
  target_link_libraries(mylite_runtime_default_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_default_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_default_function_test)

  add_executable(mylite_runtime_charset_collation_functions_test
    tests/runtime_charset_collation_functions_test.c
  )
  target_link_libraries(mylite_runtime_charset_collation_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_charset_collation_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_charset_collation_functions_test)

  add_executable(mylite_runtime_coercibility_function_test
    tests/runtime_coercibility_function_test.c
  )
  target_link_libraries(mylite_runtime_coercibility_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_coercibility_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_coercibility_function_test)

  add_executable(mylite_runtime_string_length_functions_test
    tests/runtime_string_length_functions_test.c
  )
  target_link_libraries(mylite_runtime_string_length_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_string_length_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_length_functions_test)

  add_executable(mylite_runtime_ascii_ord_functions_test
    tests/runtime_ascii_ord_functions_test.c
  )
  target_link_libraries(mylite_runtime_ascii_ord_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_ascii_ord_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_ascii_ord_functions_test)

  add_executable(mylite_runtime_string_case_functions_test
    tests/runtime_string_case_functions_test.c
  )
  target_link_libraries(mylite_runtime_string_case_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_string_case_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_case_functions_test)

  add_executable(mylite_runtime_concat_ws_function_test
    tests/runtime_concat_ws_function_test.c
  )
  target_link_libraries(mylite_runtime_concat_ws_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_concat_ws_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_concat_ws_function_test)

  add_executable(mylite_runtime_pipes_as_concat_test
    tests/runtime_pipes_as_concat_test.c
  )
  target_link_libraries(mylite_runtime_pipes_as_concat_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_pipes_as_concat_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_pipes_as_concat_test)

  add_executable(mylite_runtime_replace_string_function_test
    tests/runtime_replace_string_function_test.c
  )
  target_link_libraries(mylite_runtime_replace_string_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_replace_string_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_replace_string_function_test)

  add_executable(mylite_runtime_insert_string_function_test
    tests/runtime_insert_string_function_test.c
  )
  target_link_libraries(mylite_runtime_insert_string_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_insert_string_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_insert_string_function_test)

  add_executable(mylite_runtime_reverse_function_test
    tests/runtime_reverse_function_test.c
  )
  target_link_libraries(mylite_runtime_reverse_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_reverse_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_reverse_function_test)

  add_executable(mylite_runtime_soundex_function_test
    tests/runtime_soundex_function_test.c
  )
  target_link_libraries(mylite_runtime_soundex_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_soundex_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_soundex_function_test)

  add_executable(mylite_runtime_quote_function_test
    tests/runtime_quote_function_test.c
  )
  target_link_libraries(mylite_runtime_quote_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_quote_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_quote_function_test)

  add_executable(mylite_runtime_trim_string_functions_test
    tests/runtime_trim_string_functions_test.c
  )
  target_link_libraries(mylite_runtime_trim_string_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_trim_string_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_trim_string_functions_test)

  add_executable(mylite_runtime_string_slice_functions_test
    tests/runtime_string_slice_functions_test.c
  )
  target_link_libraries(mylite_runtime_string_slice_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_string_slice_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_slice_functions_test)

  add_executable(mylite_runtime_string_padding_functions_test
    tests/runtime_string_padding_functions_test.c
  )
  target_link_libraries(mylite_runtime_string_padding_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_string_padding_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_padding_functions_test)

  add_executable(mylite_runtime_string_bitmask_functions_test
    tests/runtime_string_bitmask_functions_test.c
  )
  target_link_libraries(mylite_runtime_string_bitmask_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_string_bitmask_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_bitmask_functions_test)

  add_executable(mylite_runtime_string_search_functions_test
    tests/runtime_string_search_functions_test.c
  )
  target_link_libraries(mylite_runtime_string_search_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_string_search_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_search_functions_test)

  add_executable(mylite_runtime_string_row_scalar_update_contexts_test
    tests/runtime_string_row_scalar_update_contexts_test.c
  )
  target_link_libraries(mylite_runtime_string_row_scalar_update_contexts_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_string_row_scalar_update_contexts_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_row_scalar_update_contexts_test)

  add_executable(mylite_runtime_temporal_row_scalar_update_contexts_test
    tests/runtime_temporal_row_scalar_update_contexts_test.c
  )
  target_link_libraries(mylite_runtime_temporal_row_scalar_update_contexts_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_temporal_row_scalar_update_contexts_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_temporal_row_scalar_update_contexts_test)

  add_executable(mylite_runtime_date_add_second_test
    tests/runtime_date_add_second_test.c
  )
  target_link_libraries(mylite_runtime_date_add_second_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_date_add_second_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_date_add_second_test)

  add_executable(mylite_runtime_timestampadd_second_function_test
    tests/runtime_timestampadd_second_function_test.c
  )
  target_link_libraries(mylite_runtime_timestampadd_second_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_timestampadd_second_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_timestampadd_second_function_test)

  add_executable(mylite_runtime_addtime_subtime_functions_test
    tests/runtime_addtime_subtime_functions_test.c
  )
  target_link_libraries(mylite_runtime_addtime_subtime_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_addtime_subtime_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_addtime_subtime_functions_test)

  add_executable(mylite_runtime_row_temporal_interval_second_projection_test
    tests/runtime_row_temporal_interval_second_projection_test.c
  )
  target_link_libraries(mylite_runtime_row_temporal_interval_second_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_row_temporal_interval_second_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_row_temporal_interval_second_projection_test)

  add_executable(mylite_runtime_date_format_function_test
    tests/runtime_date_format_function_test.c
  )
  target_link_libraries(mylite_runtime_date_format_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_date_format_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_date_format_function_test)

  add_executable(mylite_runtime_get_format_function_test
    tests/runtime_get_format_function_test.c
  )
  target_link_libraries(mylite_runtime_get_format_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_get_format_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_get_format_function_test)

  add_executable(mylite_runtime_time_format_function_test
    tests/runtime_time_format_function_test.c
  )
  target_link_libraries(mylite_runtime_time_format_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_time_format_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_time_format_function_test)

  add_executable(mylite_runtime_str_to_date_function_test
    tests/runtime_str_to_date_function_test.c
  )
  target_link_libraries(mylite_runtime_str_to_date_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_str_to_date_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_str_to_date_function_test)

  add_executable(mylite_runtime_datediff_function_test
    tests/runtime_datediff_function_test.c
  )
  target_link_libraries(mylite_runtime_datediff_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_datediff_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_datediff_function_test)

  add_executable(mylite_runtime_timestampdiff_function_test
    tests/runtime_timestampdiff_function_test.c
  )
  target_link_libraries(mylite_runtime_timestampdiff_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_timestampdiff_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_timestampdiff_function_test)

  add_executable(mylite_runtime_timestamp_function_test
    tests/runtime_timestamp_function_test.c
  )
  target_link_libraries(mylite_runtime_timestamp_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_timestamp_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_timestamp_function_test)

  add_executable(mylite_runtime_timediff_function_test
    tests/runtime_timediff_function_test.c
  )
  target_link_libraries(mylite_runtime_timediff_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_timediff_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_timediff_function_test)

  add_executable(mylite_runtime_calendar_date_functions_test
    tests/runtime_calendar_date_functions_test.c
  )
  target_link_libraries(mylite_runtime_calendar_date_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_calendar_date_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_calendar_date_functions_test)

  add_executable(mylite_runtime_day_month_name_functions_test
    tests/runtime_day_month_name_functions_test.c
  )
  target_link_libraries(mylite_runtime_day_month_name_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_day_month_name_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_day_month_name_functions_test)

  add_executable(mylite_runtime_week_temporal_functions_test
    tests/runtime_week_temporal_functions_test.c
  )
  target_link_libraries(mylite_runtime_week_temporal_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_week_temporal_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_week_temporal_functions_test)

  add_executable(mylite_runtime_unix_timestamp_function_test
    tests/runtime_unix_timestamp_function_test.c
  )
  target_link_libraries(mylite_runtime_unix_timestamp_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_unix_timestamp_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_unix_timestamp_function_test)

  add_executable(mylite_runtime_from_unixtime_function_test
    tests/runtime_from_unixtime_function_test.c
  )
  target_link_libraries(mylite_runtime_from_unixtime_function_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_from_unixtime_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_from_unixtime_function_test)

  add_executable(mylite_runtime_time_second_conversion_functions_test
    tests/runtime_time_second_conversion_functions_test.c
  )
  target_link_libraries(mylite_runtime_time_second_conversion_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_time_second_conversion_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_time_second_conversion_functions_test)

  add_executable(mylite_runtime_to_days_to_seconds_functions_test
    tests/runtime_to_days_to_seconds_functions_test.c
  )
  target_link_libraries(mylite_runtime_to_days_to_seconds_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_to_days_to_seconds_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_to_days_to_seconds_functions_test)

  add_executable(mylite_runtime_temporal_constructor_functions_test
    tests/runtime_temporal_constructor_functions_test.c
  )
  target_link_libraries(mylite_runtime_temporal_constructor_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_temporal_constructor_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_temporal_constructor_functions_test)

  add_executable(mylite_runtime_scalar_period_timezone_weight_functions_test
    tests/runtime_scalar_period_timezone_weight_functions_test.c
  )
  target_link_libraries(mylite_runtime_scalar_period_timezone_weight_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_period_timezone_weight_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_period_timezone_weight_functions_test)

  add_executable(mylite_runtime_temporal_extract_functions_test
    tests/runtime_temporal_extract_functions_test.c
  )
  target_link_libraries(mylite_runtime_temporal_extract_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_temporal_extract_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_temporal_extract_functions_test)

  add_executable(mylite_runtime_elt_function_test
    tests/runtime_elt_function_test.c
  )
  target_link_libraries(mylite_runtime_elt_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_elt_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_elt_function_test)

  add_executable(mylite_runtime_field_function_test
    tests/runtime_field_function_test.c
  )
  target_link_libraries(mylite_runtime_field_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_field_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_field_function_test)

  add_executable(mylite_runtime_order_by_field_function_test
    tests/runtime_order_by_field_function_test.c
  )
  target_link_libraries(mylite_runtime_order_by_field_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_order_by_field_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_order_by_field_function_test)

  add_executable(mylite_runtime_greatest_least_functions_test
    tests/runtime_greatest_least_functions_test.c
  )
  target_link_libraries(mylite_runtime_greatest_least_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_greatest_least_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_greatest_least_functions_test)

  add_executable(mylite_runtime_interval_function_test
    tests/runtime_interval_function_test.c
  )
  target_link_libraries(mylite_runtime_interval_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_interval_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_interval_function_test)

  add_executable(mylite_runtime_json_valid_function_test
    tests/runtime_json_valid_function_test.c
  )
  target_link_libraries(mylite_runtime_json_valid_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_valid_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_valid_function_test)

  add_executable(mylite_runtime_json_extract_functions_test
    tests/runtime_json_extract_functions_test.c
  )
  target_link_libraries(mylite_runtime_json_extract_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_extract_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_extract_functions_test)

  add_executable(mylite_runtime_json_value_function_test
    tests/runtime_json_value_function_test.c
  )
  target_link_libraries(mylite_runtime_json_value_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_value_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_value_function_test)

  add_executable(mylite_runtime_json_quote_function_test
    tests/runtime_json_quote_function_test.c
  )
  target_link_libraries(mylite_runtime_json_quote_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_quote_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_quote_function_test)

  add_executable(mylite_runtime_json_contains_functions_test
    tests/runtime_json_contains_functions_test.c
  )
  target_link_libraries(mylite_runtime_json_contains_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_contains_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_contains_functions_test)

  add_executable(mylite_runtime_json_search_function_test
    tests/runtime_json_search_function_test.c
  )
  target_link_libraries(mylite_runtime_json_search_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_search_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_search_function_test)

  add_executable(mylite_runtime_json_overlaps_member_functions_test
    tests/runtime_json_overlaps_member_functions_test.c
  )
  target_link_libraries(mylite_runtime_json_overlaps_member_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_overlaps_member_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_overlaps_member_functions_test)

  add_executable(mylite_runtime_json_construction_functions_test
    tests/runtime_json_construction_functions_test.c
  )
  target_link_libraries(mylite_runtime_json_construction_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_construction_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_construction_functions_test)

  add_executable(mylite_runtime_json_set_function_test
    tests/runtime_json_set_function_test.c
  )
  target_link_libraries(mylite_runtime_json_set_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_set_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_set_function_test)

  add_executable(mylite_runtime_json_insert_function_test
    tests/runtime_json_insert_function_test.c
  )
  target_link_libraries(mylite_runtime_json_insert_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_insert_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_insert_function_test)

  add_executable(mylite_runtime_json_array_mutation_functions_test
    tests/runtime_json_array_mutation_functions_test.c
  )
  target_link_libraries(mylite_runtime_json_array_mutation_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_array_mutation_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_array_mutation_functions_test)

  add_executable(mylite_runtime_json_merge_functions_test
    tests/runtime_json_merge_functions_test.c
  )
  target_link_libraries(mylite_runtime_json_merge_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_merge_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_merge_functions_test)

  add_executable(mylite_runtime_json_replace_function_test
    tests/runtime_json_replace_function_test.c
  )
  target_link_libraries(mylite_runtime_json_replace_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_replace_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_replace_function_test)

  add_executable(mylite_runtime_json_remove_function_test
    tests/runtime_json_remove_function_test.c
  )
  target_link_libraries(mylite_runtime_json_remove_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_remove_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_remove_function_test)

  add_executable(mylite_runtime_json_introspection_functions_test
    tests/runtime_json_introspection_functions_test.c
  )
  target_link_libraries(mylite_runtime_json_introspection_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_introspection_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_introspection_functions_test)

  add_executable(mylite_runtime_json_row_scalar_contexts_test
    tests/runtime_json_row_scalar_contexts_test.c
  )
  target_link_libraries(mylite_runtime_json_row_scalar_contexts_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_row_scalar_contexts_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_row_scalar_contexts_test)

  add_executable(mylite_runtime_spatial_basic_functions_test
    tests/runtime_spatial_basic_functions_test.c
  )
  target_link_libraries(mylite_runtime_spatial_basic_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_spatial_basic_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_spatial_basic_functions_test)

  add_executable(mylite_runtime_spatial_measure_accessor_functions_test
    tests/runtime_spatial_measure_accessor_functions_test.c
  )
  target_link_libraries(mylite_runtime_spatial_measure_accessor_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_spatial_measure_accessor_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_spatial_measure_accessor_functions_test)

  add_executable(mylite_runtime_spatial_validity_functions_test
    tests/runtime_spatial_validity_functions_test.c
  )
  target_link_libraries(mylite_runtime_spatial_validity_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_spatial_validity_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_spatial_validity_functions_test)

  add_executable(mylite_runtime_spatial_simplicity_function_test
    tests/runtime_spatial_simplicity_function_test.c
  )
  target_link_libraries(mylite_runtime_spatial_simplicity_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_spatial_simplicity_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_spatial_simplicity_function_test)

  add_executable(mylite_runtime_spatial_simplify_function_test
    tests/runtime_spatial_simplify_function_test.c
  )
  target_link_libraries(mylite_runtime_spatial_simplify_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_spatial_simplify_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_spatial_simplify_function_test)

  add_executable(mylite_runtime_spatial_disjoint_intersects_functions_test
    tests/runtime_spatial_disjoint_intersects_functions_test.c
  )
  target_link_libraries(mylite_runtime_spatial_disjoint_intersects_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_spatial_disjoint_intersects_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_spatial_disjoint_intersects_functions_test)

  add_executable(mylite_runtime_spatial_contains_within_functions_test
    tests/runtime_spatial_contains_within_functions_test.c
  )
  target_link_libraries(mylite_runtime_spatial_contains_within_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_spatial_contains_within_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_spatial_contains_within_functions_test)

  add_executable(mylite_runtime_spatial_equals_function_test
    tests/runtime_spatial_equals_function_test.c
  )
  target_link_libraries(mylite_runtime_spatial_equals_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_spatial_equals_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_spatial_equals_function_test)

  add_executable(mylite_runtime_spatial_touches_function_test
    tests/runtime_spatial_touches_function_test.c
  )
  target_link_libraries(mylite_runtime_spatial_touches_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_spatial_touches_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_spatial_touches_function_test)

  add_executable(mylite_runtime_spatial_crosses_function_test
    tests/runtime_spatial_crosses_function_test.c
  )
  target_link_libraries(mylite_runtime_spatial_crosses_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_spatial_crosses_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_spatial_crosses_function_test)

  add_executable(mylite_runtime_spatial_overlaps_function_test
    tests/runtime_spatial_overlaps_function_test.c
  )
  target_link_libraries(mylite_runtime_spatial_overlaps_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_spatial_overlaps_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_spatial_overlaps_function_test)

  add_executable(mylite_runtime_spatial_collect_aggregate_test
    tests/runtime_spatial_collect_aggregate_test.c
  )
  target_link_libraries(mylite_runtime_spatial_collect_aggregate_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_spatial_collect_aggregate_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_spatial_collect_aggregate_test)

  add_executable(mylite_runtime_spatial_geohash_functions_test
    tests/runtime_spatial_geohash_functions_test.c
  )
  target_link_libraries(mylite_runtime_spatial_geohash_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_spatial_geohash_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_spatial_geohash_functions_test)

  add_executable(mylite_runtime_spatial_latitude_longitude_functions_test
    tests/runtime_spatial_latitude_longitude_functions_test.c
  )
  target_link_libraries(mylite_runtime_spatial_latitude_longitude_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_spatial_latitude_longitude_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_spatial_latitude_longitude_functions_test)

  add_executable(mylite_runtime_spatial_geojson_functions_test
    tests/runtime_spatial_geojson_functions_test.c
  )
  target_link_libraries(mylite_runtime_spatial_geojson_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_spatial_geojson_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_spatial_geojson_functions_test)

  add_executable(mylite_runtime_scalar_comparison_projection_test
    tests/runtime_scalar_comparison_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_comparison_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_comparison_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_comparison_projection_test)

  add_executable(mylite_runtime_scalar_logical_projection_test
    tests/runtime_scalar_logical_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_logical_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_logical_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_logical_projection_test)

  add_executable(mylite_runtime_scalar_is_projection_test
    tests/runtime_scalar_is_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_is_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_is_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_is_projection_test)

  add_executable(mylite_runtime_session_value_scalar_projection_test
    tests/runtime_session_value_scalar_projection_test.c
  )
  target_link_libraries(mylite_runtime_session_value_scalar_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_session_value_scalar_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_session_value_scalar_projection_test)

  add_executable(mylite_runtime_row_count_function_test
    tests/runtime_row_count_function_test.c
  )
  target_link_libraries(mylite_runtime_row_count_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_row_count_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_row_count_function_test)

  add_executable(mylite_runtime_found_rows_function_test
    tests/runtime_found_rows_function_test.c
  )
  target_link_libraries(mylite_runtime_found_rows_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_found_rows_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_found_rows_function_test)

  add_executable(mylite_runtime_select_noop_modifiers_test
    tests/runtime_select_noop_modifiers_test.c
  )
  target_link_libraries(mylite_runtime_select_noop_modifiers_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_select_noop_modifiers_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_select_noop_modifiers_test)

  add_executable(mylite_runtime_select_locking_clauses_test
    tests/runtime_select_locking_clauses_test.c
  )
  target_link_libraries(mylite_runtime_select_locking_clauses_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_select_locking_clauses_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_select_locking_clauses_test)

  add_executable(mylite_runtime_last_insert_id_function_test
    tests/runtime_last_insert_id_function_test.c
  )
  target_link_libraries(mylite_runtime_last_insert_id_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_last_insert_id_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_last_insert_id_function_test)

  add_executable(mylite_runtime_count_aggregate_test
    tests/runtime_count_aggregate_test.c
  )
  target_link_libraries(mylite_runtime_count_aggregate_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_count_aggregate_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_count_aggregate_test)

  add_executable(mylite_runtime_min_max_aggregate_test
    tests/runtime_min_max_aggregate_test.c
  )
  target_link_libraries(mylite_runtime_min_max_aggregate_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_min_max_aggregate_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_min_max_aggregate_test)

  add_executable(mylite_runtime_sum_aggregate_test
    tests/runtime_sum_aggregate_test.c
  )
  target_link_libraries(mylite_runtime_sum_aggregate_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_sum_aggregate_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sum_aggregate_test)

  add_executable(mylite_runtime_avg_aggregate_test
    tests/runtime_avg_aggregate_test.c
  )
  target_link_libraries(mylite_runtime_avg_aggregate_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_avg_aggregate_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_avg_aggregate_test)

  add_executable(mylite_runtime_multi_aggregate_select_test
    tests/runtime_multi_aggregate_select_test.c
  )
  target_link_libraries(mylite_runtime_multi_aggregate_select_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_multi_aggregate_select_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_multi_aggregate_select_test)

  add_executable(mylite_runtime_statistical_aggregates_test
    tests/runtime_statistical_aggregates_test.c
  )
  target_link_libraries(mylite_runtime_statistical_aggregates_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_statistical_aggregates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_statistical_aggregates_test)

  add_executable(mylite_runtime_bitwise_aggregates_test
    tests/runtime_bitwise_aggregates_test.c
  )
  target_link_libraries(mylite_runtime_bitwise_aggregates_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_bitwise_aggregates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_bitwise_aggregates_test)

  add_executable(mylite_runtime_group_concat_aggregate_test
    tests/runtime_group_concat_aggregate_test.c
  )
  target_link_libraries(mylite_runtime_group_concat_aggregate_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_group_concat_aggregate_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_group_concat_aggregate_test)

  add_executable(mylite_runtime_group_concat_max_len_system_variable_test
    tests/runtime_group_concat_max_len_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_group_concat_max_len_system_variable_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_group_concat_max_len_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_group_concat_max_len_system_variable_test)

  add_executable(mylite_runtime_remaining_system_variables_test
    tests/runtime_remaining_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_remaining_system_variables_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_remaining_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_remaining_system_variables_test)

  add_executable(mylite_runtime_big_tables_system_variable_test
    tests/runtime_big_tables_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_big_tables_system_variable_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_big_tables_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_big_tables_system_variable_test)

  add_executable(mylite_runtime_information_schema_stats_expiry_system_variable_test
    tests/runtime_information_schema_stats_expiry_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_stats_expiry_system_variable_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_information_schema_stats_expiry_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_stats_expiry_system_variable_test)

  add_executable(mylite_runtime_session_tracking_system_variables_test
    tests/runtime_session_tracking_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_session_tracking_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_session_tracking_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_session_tracking_system_variables_test)

  add_executable(mylite_runtime_server_security_system_variables_test
    tests/runtime_server_security_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_server_security_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_server_security_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_server_security_system_variables_test)

  add_executable(mylite_runtime_global_resource_system_variables_test
    tests/runtime_global_resource_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_global_resource_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_global_resource_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_global_resource_system_variables_test)

  add_executable(mylite_runtime_internal_session_system_variables_test
    tests/runtime_internal_session_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_internal_session_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_internal_session_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_internal_session_system_variables_test)

  add_executable(mylite_runtime_resource_tuning_system_variables_test
    tests/runtime_resource_tuning_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_resource_tuning_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_resource_tuning_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_resource_tuning_system_variables_test)

  add_executable(mylite_runtime_server_tls_system_variables_test
    tests/runtime_server_tls_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_server_tls_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_server_tls_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_server_tls_system_variables_test)

  add_executable(mylite_runtime_server_logging_system_variables_test
    tests/runtime_server_logging_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_server_logging_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_server_logging_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_server_logging_system_variables_test)

  add_executable(mylite_runtime_session_tuning_system_variables_test
    tests/runtime_session_tuning_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_session_tuning_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_session_tuning_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_session_tuning_system_variables_test)

  add_executable(mylite_runtime_jl_system_variables_test
    tests/runtime_jl_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_jl_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_jl_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_jl_system_variables_test)

  add_executable(mylite_runtime_m_server_limit_system_variables_test
    tests/runtime_m_server_limit_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_m_server_limit_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_m_server_limit_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_m_server_limit_system_variables_test)

  add_executable(mylite_runtime_m_session_limit_system_variables_test
    tests/runtime_m_session_limit_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_m_session_limit_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_m_session_limit_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_m_session_limit_system_variables_test)

  add_executable(mylite_runtime_o_optimizer_system_variables_test
    tests/runtime_o_optimizer_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_o_optimizer_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_o_optimizer_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_o_optimizer_system_variables_test)

  add_executable(mylite_runtime_replication_global_system_variables_test
    tests/runtime_replication_global_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_replication_global_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_replication_global_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_replication_global_system_variables_test)

  add_executable(mylite_runtime_replica_applier_system_variables_test
    tests/runtime_replica_applier_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_replica_applier_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_replica_applier_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_replica_applier_system_variables_test)

  add_executable(mylite_runtime_myisam_system_variables_test
    tests/runtime_myisam_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_myisam_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_myisam_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_myisam_system_variables_test)

  add_executable(mylite_runtime_innodb_core_system_variables_test
    tests/runtime_innodb_core_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_innodb_core_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_innodb_core_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_innodb_core_system_variables_test)

  add_executable(mylite_runtime_innodb_storage_system_variables_test
    tests/runtime_innodb_storage_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_innodb_storage_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_innodb_storage_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_innodb_storage_system_variables_test)

  add_executable(mylite_runtime_innodb_file_flush_system_variables_test
    tests/runtime_innodb_file_flush_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_innodb_file_flush_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_innodb_file_flush_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_innodb_file_flush_system_variables_test)

  add_executable(mylite_runtime_innodb_fulltext_system_variables_test
    tests/runtime_innodb_fulltext_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_innodb_fulltext_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_innodb_fulltext_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_innodb_fulltext_system_variables_test)

  add_executable(mylite_runtime_innodb_io_log_system_variables_test
    tests/runtime_innodb_io_log_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_innodb_io_log_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_innodb_io_log_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_innodb_io_log_system_variables_test)

  add_executable(mylite_runtime_innodb_dirty_purge_system_variables_test
    tests/runtime_innodb_dirty_purge_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_innodb_dirty_purge_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_innodb_dirty_purge_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_innodb_dirty_purge_system_variables_test)

  add_executable(mylite_runtime_innodb_monitor_system_variables_test
    tests/runtime_innodb_monitor_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_innodb_monitor_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_innodb_monitor_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_innodb_monitor_system_variables_test)

  add_executable(mylite_runtime_innodb_page_read_purge_system_variables_test
    tests/runtime_innodb_page_read_purge_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_innodb_page_read_purge_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_innodb_page_read_purge_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_innodb_page_read_purge_system_variables_test)

  add_executable(mylite_runtime_innodb_redo_rollback_spin_system_variables_test
    tests/runtime_innodb_redo_rollback_spin_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_innodb_redo_rollback_spin_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_innodb_redo_rollback_spin_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_innodb_redo_rollback_spin_system_variables_test)

  add_executable(mylite_runtime_innodb_stats_status_thread_undo_system_variables_test
    tests/runtime_innodb_stats_status_thread_undo_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_innodb_stats_status_thread_undo_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_innodb_stats_status_thread_undo_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_innodb_stats_status_thread_undo_system_variables_test)

  add_executable(mylite_runtime_performance_schema_system_variables_test
    tests/runtime_performance_schema_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_system_variables_test)

  add_executable(mylite_runtime_performance_schema_variable_status_tables_test
    tests/runtime_performance_schema_variable_status_tables_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_variable_status_tables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_variable_status_tables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_variable_status_tables_test)

  add_executable(mylite_runtime_performance_schema_variable_info_tables_test
    tests/runtime_performance_schema_variable_info_tables_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_variable_info_tables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_variable_info_tables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_variable_info_tables_test)

  add_executable(mylite_runtime_performance_schema_instance_tables_test
    tests/runtime_performance_schema_instance_tables_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_instance_tables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_instance_tables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_instance_tables_test)

  add_executable(mylite_runtime_performance_schema_instrumentation_placeholders_test
    tests/runtime_performance_schema_instrumentation_placeholders_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_instrumentation_placeholders_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_instrumentation_placeholders_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_instrumentation_placeholders_test)

  add_executable(mylite_runtime_performance_schema_event_history_placeholders_test
    tests/runtime_performance_schema_event_history_placeholders_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_event_history_placeholders_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_event_history_placeholders_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_event_history_placeholders_test)

  add_executable(mylite_runtime_performance_schema_statement_event_placeholders_test
    tests/runtime_performance_schema_statement_event_placeholders_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_statement_event_placeholders_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_statement_event_placeholders_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_statement_event_placeholders_test)

  add_executable(mylite_runtime_performance_schema_transaction_event_placeholders_test
    tests/runtime_performance_schema_transaction_event_placeholders_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_transaction_event_placeholders_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_transaction_event_placeholders_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_transaction_event_placeholders_test)

  add_executable(mylite_runtime_performance_schema_remaining_placeholders_test
    tests/runtime_performance_schema_remaining_placeholders_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_remaining_placeholders_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_remaining_placeholders_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_remaining_placeholders_test)

  add_executable(mylite_runtime_performance_schema_error_summary_placeholders_test
    tests/runtime_performance_schema_error_summary_placeholders_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_error_summary_placeholders_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_error_summary_placeholders_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_error_summary_placeholders_test)

  add_executable(mylite_runtime_performance_schema_file_summary_placeholders_test
    tests/runtime_performance_schema_file_summary_placeholders_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_file_summary_placeholders_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_file_summary_placeholders_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_file_summary_placeholders_test)

  add_executable(mylite_runtime_performance_schema_socket_summary_placeholders_test
    tests/runtime_performance_schema_socket_summary_placeholders_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_socket_summary_placeholders_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_socket_summary_placeholders_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_socket_summary_placeholders_test)

  add_executable(mylite_runtime_performance_schema_memory_summary_placeholders_test
    tests/runtime_performance_schema_memory_summary_placeholders_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_memory_summary_placeholders_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_memory_summary_placeholders_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_memory_summary_placeholders_test)

  add_executable(mylite_runtime_performance_schema_table_wait_summary_placeholders_test
    tests/runtime_performance_schema_table_wait_summary_placeholders_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_table_wait_summary_placeholders_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_table_wait_summary_placeholders_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_table_wait_summary_placeholders_test)

  add_executable(mylite_runtime_performance_schema_stage_wait_summary_placeholders_test
    tests/runtime_performance_schema_stage_wait_summary_placeholders_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_stage_wait_summary_placeholders_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_stage_wait_summary_placeholders_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_stage_wait_summary_placeholders_test)

  add_executable(mylite_runtime_performance_schema_host_keyring_placeholders_test
    tests/runtime_performance_schema_host_keyring_placeholders_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_host_keyring_placeholders_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_host_keyring_placeholders_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_host_keyring_placeholders_test)

  add_executable(mylite_runtime_performance_schema_replication_placeholders_test
    tests/runtime_performance_schema_replication_placeholders_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_replication_placeholders_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_replication_placeholders_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_replication_placeholders_test)

  add_executable(mylite_runtime_performance_schema_connection_tables_test
    tests/runtime_performance_schema_connection_tables_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_connection_tables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_connection_tables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_connection_tables_test)

  add_executable(mylite_runtime_performance_schema_connection_attribute_tables_test
    tests/runtime_performance_schema_connection_attribute_tables_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_connection_attribute_tables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_connection_attribute_tables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_connection_attribute_tables_test)

  add_executable(mylite_runtime_performance_schema_thread_status_variable_tables_test
    tests/runtime_performance_schema_thread_status_variable_tables_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_thread_status_variable_tables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_thread_status_variable_tables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_thread_status_variable_tables_test)

  add_executable(mylite_runtime_performance_schema_performance_timers_test
    tests/runtime_performance_schema_performance_timers_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_performance_timers_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_performance_timers_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_performance_timers_test)

  add_executable(mylite_runtime_performance_schema_setup_actor_consumer_tables_test
    tests/runtime_performance_schema_setup_actor_consumer_tables_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_setup_actor_consumer_tables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_setup_actor_consumer_tables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_setup_actor_consumer_tables_test)

  add_executable(mylite_runtime_performance_schema_setup_logger_meter_tables_test
    tests/runtime_performance_schema_setup_logger_meter_tables_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_setup_logger_meter_tables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_setup_logger_meter_tables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_setup_logger_meter_tables_test)

  add_executable(mylite_runtime_performance_schema_setup_metrics_table_test
    tests/runtime_performance_schema_setup_metrics_table_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_setup_metrics_table_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_setup_metrics_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_setup_metrics_table_test)

  add_executable(mylite_runtime_performance_schema_setup_objects_test
    tests/runtime_performance_schema_setup_objects_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_setup_objects_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_setup_objects_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_setup_objects_test)

  add_executable(mylite_runtime_performance_schema_setup_threads_test
    tests/runtime_performance_schema_setup_threads_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_setup_threads_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_setup_threads_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_setup_threads_test)

  add_executable(mylite_runtime_performance_schema_metadata_locks_test
    tests/runtime_performance_schema_metadata_locks_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_metadata_locks_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_metadata_locks_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_metadata_locks_test)

  add_executable(mylite_runtime_performance_schema_object_tls_placeholders_test
    tests/runtime_performance_schema_object_tls_placeholders_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_object_tls_placeholders_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_object_tls_placeholders_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_object_tls_placeholders_test)

  add_executable(mylite_runtime_performance_schema_table_handles_table_test
    tests/runtime_performance_schema_table_handles_table_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_table_handles_table_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_table_handles_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_table_handles_table_test)

  add_executable(mylite_runtime_performance_schema_user_defined_functions_test
    tests/runtime_performance_schema_user_defined_functions_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_user_defined_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_user_defined_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_user_defined_functions_test)

  add_executable(mylite_runtime_performance_schema_user_variables_by_thread_test
    tests/runtime_performance_schema_user_variables_by_thread_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_user_variables_by_thread_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_user_variables_by_thread_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_user_variables_by_thread_test)

  add_executable(mylite_runtime_network_timeout_system_variables_test
    tests/runtime_network_timeout_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_network_timeout_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_network_timeout_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_network_timeout_system_variables_test)

  add_executable(mylite_runtime_connection_system_variables_test
    tests/runtime_connection_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_connection_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_connection_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_connection_system_variables_test)

  add_executable(mylite_runtime_binary_log_system_variables_test
    tests/runtime_binary_log_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_binary_log_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_binary_log_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_binary_log_system_variables_test)

  add_executable(mylite_runtime_bootstrap_system_variables_test
    tests/runtime_bootstrap_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_bootstrap_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_bootstrap_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_bootstrap_system_variables_test)

  add_executable(mylite_runtime_compatibility_system_variables_test
    tests/runtime_compatibility_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_compatibility_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_compatibility_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_compatibility_system_variables_test)

  add_executable(mylite_runtime_authentication_password_system_variables_test
    tests/runtime_authentication_password_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_authentication_password_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_authentication_password_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_authentication_password_system_variables_test)

  add_executable(mylite_runtime_server_capability_system_variables_test
    tests/runtime_server_capability_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_server_capability_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_server_capability_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_server_capability_system_variables_test)

  add_executable(mylite_runtime_admin_listener_system_variables_test
    tests/runtime_admin_listener_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_admin_listener_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_admin_listener_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_admin_listener_system_variables_test)

  add_executable(mylite_runtime_mysqlx_system_variables_test
    tests/runtime_mysqlx_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_mysqlx_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysqlx_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysqlx_system_variables_test)

  add_executable(mylite_runtime_group_by_single_column_aggregate_test
    tests/runtime_group_by_single_column_aggregate_test.c
  )
  target_link_libraries(mylite_runtime_group_by_single_column_aggregate_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_group_by_single_column_aggregate_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_group_by_single_column_aggregate_test)

  add_executable(mylite_runtime_any_value_function_test
    tests/runtime_any_value_function_test.c
  )
  target_link_libraries(mylite_runtime_any_value_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_any_value_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_any_value_function_test)

  add_executable(mylite_runtime_group_by_primary_key_projection_test
    tests/runtime_group_by_primary_key_projection_test.c
  )
  target_link_libraries(mylite_runtime_group_by_primary_key_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_group_by_primary_key_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_group_by_primary_key_projection_test)

  add_executable(mylite_runtime_joined_aggregate_select_test
    tests/runtime_joined_aggregate_select_test.c
  )
  target_link_libraries(mylite_runtime_joined_aggregate_select_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_joined_aggregate_select_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_joined_aggregate_select_test)

  add_executable(mylite_runtime_show_columns_introspection_test
    tests/runtime_show_columns_introspection_test.c
  )
  target_link_libraries(mylite_runtime_show_columns_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_columns_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_columns_introspection_test)

  add_executable(mylite_runtime_show_full_columns_introspection_test
    tests/runtime_show_full_columns_introspection_test.c
  )
  target_link_libraries(mylite_runtime_show_full_columns_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_full_columns_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_full_columns_introspection_test)

  add_executable(mylite_runtime_information_schema_core_test
    tests/runtime_information_schema_core_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_core_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_core_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_core_test)

  add_executable(mylite_runtime_information_schema_conditional_table_absence_test
    tests/runtime_information_schema_conditional_table_absence_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_conditional_table_absence_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_conditional_table_absence_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_conditional_table_absence_test)

  add_executable(mylite_runtime_information_schema_schemata_extensions_test
    tests/runtime_information_schema_schemata_extensions_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_schemata_extensions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_schemata_extensions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_schemata_extensions_test)

  add_executable(mylite_runtime_information_schema_extension_attribute_tables_test
    tests/runtime_information_schema_extension_attribute_tables_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_extension_attribute_tables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_extension_attribute_tables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_extension_attribute_tables_test)

  add_executable(mylite_runtime_information_schema_tablespaces_extensions_test
    tests/runtime_information_schema_tablespaces_extensions_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_tablespaces_extensions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_tablespaces_extensions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_tablespaces_extensions_test)

  add_executable(mylite_runtime_information_schema_innodb_cmp_test
    tests/runtime_information_schema_innodb_cmp_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_cmp_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_cmp_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_cmp_test)

  add_executable(mylite_runtime_information_schema_innodb_cmpmem_test
    tests/runtime_information_schema_innodb_cmpmem_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_cmpmem_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_cmpmem_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_cmpmem_test)

  add_executable(mylite_runtime_information_schema_innodb_foreign_test
    tests/runtime_information_schema_innodb_foreign_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_foreign_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_foreign_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_foreign_test)

  add_executable(mylite_runtime_information_schema_innodb_indexes_test
    tests/runtime_information_schema_innodb_indexes_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_indexes_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_indexes_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_indexes_test)

  add_executable(mylite_runtime_information_schema_innodb_columns_test
    tests/runtime_information_schema_innodb_columns_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_columns_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_columns_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_columns_test)

  add_executable(mylite_runtime_information_schema_innodb_tables_test
    tests/runtime_information_schema_innodb_tables_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_tables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_tables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_tables_test)

  add_executable(mylite_runtime_information_schema_innodb_tablestats_test
    tests/runtime_information_schema_innodb_tablestats_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_tablestats_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_tablestats_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_tablestats_test)

  add_executable(mylite_runtime_information_schema_mysql_system_statistics_test
    tests/runtime_information_schema_mysql_system_statistics_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_mysql_system_statistics_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_mysql_system_statistics_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_mysql_system_statistics_test)

  add_executable(mylite_runtime_information_schema_mysql_system_constraints_test
    tests/runtime_information_schema_mysql_system_constraints_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_mysql_system_constraints_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_mysql_system_constraints_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_mysql_system_constraints_test)

  add_executable(mylite_runtime_mysql_component_table_test
    tests/runtime_mysql_component_table_test.c
  )
  target_link_libraries(mylite_runtime_mysql_component_table_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_component_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_component_table_test)

  add_executable(mylite_runtime_mysql_cost_tables_test
    tests/runtime_mysql_cost_tables_test.c
  )
  target_link_libraries(mylite_runtime_mysql_cost_tables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_cost_tables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_cost_tables_test)

  add_executable(mylite_runtime_mysql_func_table_test
    tests/runtime_mysql_func_table_test.c
  )
  target_link_libraries(mylite_runtime_mysql_func_table_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_func_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_func_table_test)

  add_executable(mylite_runtime_mysql_gtid_executed_table_test
    tests/runtime_mysql_gtid_executed_table_test.c
  )
  target_link_libraries(mylite_runtime_mysql_gtid_executed_table_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_gtid_executed_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_gtid_executed_table_test)

  add_executable(mylite_runtime_mysql_ndb_binlog_index_test
    tests/runtime_mysql_ndb_binlog_index_test.c
  )
  target_link_libraries(mylite_runtime_mysql_ndb_binlog_index_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_ndb_binlog_index_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_ndb_binlog_index_test)

  add_executable(mylite_runtime_mysql_replication_metadata_tables_test
    tests/runtime_mysql_replication_metadata_tables_test.c
  )
  target_link_libraries(mylite_runtime_mysql_replication_metadata_tables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_replication_metadata_tables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_replication_metadata_tables_test)

  add_executable(mylite_runtime_mysql_log_tables_test
    tests/runtime_mysql_log_tables_test.c
  )
  target_link_libraries(mylite_runtime_mysql_log_tables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_log_tables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_log_tables_test)

  add_executable(mylite_runtime_mysql_user_table_test
    tests/runtime_mysql_user_table_test.c
  )
  target_link_libraries(mylite_runtime_mysql_user_table_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_user_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_user_table_test)

  add_executable(mylite_runtime_mysql_global_grants_table_test
    tests/runtime_mysql_global_grants_table_test.c
  )
  target_link_libraries(mylite_runtime_mysql_global_grants_table_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_global_grants_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_global_grants_table_test)

  add_executable(mylite_runtime_mysql_db_table_test
    tests/runtime_mysql_db_table_test.c
  )
  target_link_libraries(mylite_runtime_mysql_db_table_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_db_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_db_table_test)

  add_executable(mylite_runtime_mysql_tables_priv_table_test
    tests/runtime_mysql_tables_priv_table_test.c
  )
  target_link_libraries(mylite_runtime_mysql_tables_priv_table_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_tables_priv_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_tables_priv_table_test)

  add_executable(mylite_runtime_mysql_columns_priv_table_test
    tests/runtime_mysql_columns_priv_table_test.c
  )
  target_link_libraries(mylite_runtime_mysql_columns_priv_table_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_columns_priv_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_columns_priv_table_test)

  add_executable(mylite_runtime_mysql_procs_priv_table_test
    tests/runtime_mysql_procs_priv_table_test.c
  )
  target_link_libraries(mylite_runtime_mysql_procs_priv_table_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_procs_priv_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_procs_priv_table_test)

  add_executable(mylite_runtime_mysql_proxies_priv_table_test
    tests/runtime_mysql_proxies_priv_table_test.c
  )
  target_link_libraries(mylite_runtime_mysql_proxies_priv_table_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_proxies_priv_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_proxies_priv_table_test)

  add_executable(mylite_runtime_mysql_default_roles_table_test
    tests/runtime_mysql_default_roles_table_test.c
  )
  target_link_libraries(mylite_runtime_mysql_default_roles_table_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_default_roles_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_default_roles_table_test)

  add_executable(mylite_runtime_mysql_role_edges_table_test
    tests/runtime_mysql_role_edges_table_test.c
  )
  target_link_libraries(mylite_runtime_mysql_role_edges_table_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_role_edges_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_role_edges_table_test)

  add_executable(mylite_runtime_mysql_password_history_table_test
    tests/runtime_mysql_password_history_table_test.c
  )
  target_link_libraries(mylite_runtime_mysql_password_history_table_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_password_history_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_password_history_table_test)

  add_executable(mylite_runtime_mysql_help_tables_test
    tests/runtime_mysql_help_tables_test.c
  )
  target_link_libraries(mylite_runtime_mysql_help_tables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_help_tables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_help_tables_test)

  add_executable(mylite_runtime_mysql_plugin_table_test
    tests/runtime_mysql_plugin_table_test.c
  )
  target_link_libraries(mylite_runtime_mysql_plugin_table_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_plugin_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_plugin_table_test)

  add_executable(mylite_runtime_mysql_servers_table_test
    tests/runtime_mysql_servers_table_test.c
  )
  target_link_libraries(mylite_runtime_mysql_servers_table_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_servers_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_servers_table_test)

  add_executable(mylite_runtime_mysql_time_zone_tables_test
    tests/runtime_mysql_time_zone_tables_test.c
  )
  target_link_libraries(mylite_runtime_mysql_time_zone_tables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_time_zone_tables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_time_zone_tables_test)

  add_executable(mylite_runtime_sys_sys_config_table_test
    tests/runtime_sys_sys_config_table_test.c
  )
  target_link_libraries(mylite_runtime_sys_sys_config_table_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_sys_config_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_sys_config_table_test)

  add_executable(mylite_runtime_sys_sys_config_triggers_test
    tests/runtime_sys_sys_config_triggers_test.c
  )
  target_link_libraries(mylite_runtime_sys_sys_config_triggers_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_sys_config_triggers_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_sys_config_triggers_test)

  add_executable(mylite_runtime_sys_version_view_test
    tests/runtime_sys_version_view_test.c
  )
  target_link_libraries(mylite_runtime_sys_version_view_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_version_view_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_version_view_test)

  add_executable(mylite_runtime_sys_host_summary_views_test
    tests/runtime_sys_host_summary_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_host_summary_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_host_summary_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_host_summary_views_test)

  add_executable(mylite_runtime_sys_host_summary_by_file_io_views_test
    tests/runtime_sys_host_summary_by_file_io_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_host_summary_by_file_io_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_host_summary_by_file_io_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_host_summary_by_file_io_views_test)

  add_executable(mylite_runtime_sys_host_summary_by_file_io_type_views_test
    tests/runtime_sys_host_summary_by_file_io_type_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_host_summary_by_file_io_type_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_host_summary_by_file_io_type_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_host_summary_by_file_io_type_views_test)

  add_executable(mylite_runtime_sys_host_summary_by_stages_views_test
    tests/runtime_sys_host_summary_by_stages_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_host_summary_by_stages_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_host_summary_by_stages_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_host_summary_by_stages_views_test)

  add_executable(mylite_runtime_sys_host_summary_by_statement_latency_views_test
    tests/runtime_sys_host_summary_by_statement_latency_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_host_summary_by_statement_latency_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_host_summary_by_statement_latency_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_host_summary_by_statement_latency_views_test)

  add_executable(mylite_runtime_sys_host_summary_by_statement_type_views_test
    tests/runtime_sys_host_summary_by_statement_type_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_host_summary_by_statement_type_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_host_summary_by_statement_type_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_host_summary_by_statement_type_views_test)

  add_executable(mylite_runtime_sys_user_summary_views_test
    tests/runtime_sys_user_summary_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_user_summary_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_user_summary_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_user_summary_views_test)

  add_executable(mylite_runtime_sys_wait_views_test
    tests/runtime_sys_wait_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_wait_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_wait_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_wait_views_test)

  add_executable(mylite_runtime_sys_innodb_buffer_stats_by_schema_views_test
    tests/runtime_sys_innodb_buffer_stats_by_schema_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_innodb_buffer_stats_by_schema_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_innodb_buffer_stats_by_schema_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_innodb_buffer_stats_by_schema_views_test)

  add_executable(mylite_runtime_sys_innodb_buffer_stats_by_table_views_test
    tests/runtime_sys_innodb_buffer_stats_by_table_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_innodb_buffer_stats_by_table_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_innodb_buffer_stats_by_table_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_innodb_buffer_stats_by_table_views_test)

  add_executable(mylite_runtime_sys_innodb_lock_waits_views_test
    tests/runtime_sys_innodb_lock_waits_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_innodb_lock_waits_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_innodb_lock_waits_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_innodb_lock_waits_views_test)

  add_executable(mylite_runtime_sys_latest_file_io_views_test
    tests/runtime_sys_latest_file_io_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_latest_file_io_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_latest_file_io_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_latest_file_io_views_test)

  add_executable(mylite_runtime_sys_memory_by_host_by_current_bytes_views_test
    tests/runtime_sys_memory_by_host_by_current_bytes_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_memory_by_host_by_current_bytes_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_memory_by_host_by_current_bytes_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_memory_by_host_by_current_bytes_views_test)

  add_executable(mylite_runtime_sys_memory_by_thread_by_current_bytes_views_test
    tests/runtime_sys_memory_by_thread_by_current_bytes_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_memory_by_thread_by_current_bytes_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_memory_by_thread_by_current_bytes_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_memory_by_thread_by_current_bytes_views_test)

  add_executable(mylite_runtime_sys_memory_by_user_by_current_bytes_views_test
    tests/runtime_sys_memory_by_user_by_current_bytes_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_memory_by_user_by_current_bytes_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_memory_by_user_by_current_bytes_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_memory_by_user_by_current_bytes_views_test)

  add_executable(mylite_runtime_sys_memory_global_by_current_bytes_views_test
    tests/runtime_sys_memory_global_by_current_bytes_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_memory_global_by_current_bytes_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_memory_global_by_current_bytes_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_memory_global_by_current_bytes_views_test)

  add_executable(mylite_runtime_sys_memory_global_total_views_test
    tests/runtime_sys_memory_global_total_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_memory_global_total_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_memory_global_total_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_memory_global_total_views_test)

  add_executable(mylite_runtime_sys_metrics_view_test
    tests/runtime_sys_metrics_view_test.c
  )
  target_link_libraries(mylite_runtime_sys_metrics_view_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_metrics_view_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_metrics_view_test)

  add_executable(mylite_runtime_sys_processlist_views_test
    tests/runtime_sys_processlist_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_processlist_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_processlist_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_processlist_views_test)

  add_executable(mylite_runtime_sys_session_views_test
    tests/runtime_sys_session_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_session_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_session_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_session_views_test)

  add_executable(mylite_runtime_sys_ps_digest_helper_views_test
    tests/runtime_sys_ps_digest_helper_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_ps_digest_helper_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_ps_digest_helper_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_ps_digest_helper_views_test)

  add_executable(mylite_runtime_sys_statement_digest_views_test
    tests/runtime_sys_statement_digest_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_statement_digest_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_statement_digest_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_statement_digest_views_test)

  add_executable(mylite_runtime_sys_statement_sort_temp_views_test
    tests/runtime_sys_statement_sort_temp_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_statement_sort_temp_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_statement_sort_temp_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_statement_sort_temp_views_test)

  add_executable(mylite_runtime_sys_io_by_thread_by_latency_views_test
    tests/runtime_sys_io_by_thread_by_latency_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_io_by_thread_by_latency_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_io_by_thread_by_latency_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_io_by_thread_by_latency_views_test)

  add_executable(mylite_runtime_sys_io_global_by_file_by_bytes_views_test
    tests/runtime_sys_io_global_by_file_by_bytes_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_io_global_by_file_by_bytes_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_io_global_by_file_by_bytes_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_io_global_by_file_by_bytes_views_test)

  add_executable(mylite_runtime_sys_io_global_by_file_by_latency_views_test
    tests/runtime_sys_io_global_by_file_by_latency_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_io_global_by_file_by_latency_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_io_global_by_file_by_latency_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_io_global_by_file_by_latency_views_test)

  add_executable(mylite_runtime_sys_io_global_by_wait_by_bytes_views_test
    tests/runtime_sys_io_global_by_wait_by_bytes_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_io_global_by_wait_by_bytes_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_io_global_by_wait_by_bytes_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_io_global_by_wait_by_bytes_views_test)

  add_executable(mylite_runtime_sys_io_global_by_wait_by_latency_views_test
    tests/runtime_sys_io_global_by_wait_by_latency_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_io_global_by_wait_by_latency_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_io_global_by_wait_by_latency_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_io_global_by_wait_by_latency_views_test)

  add_executable(mylite_runtime_sys_ps_check_lost_instrumentation_view_test
    tests/runtime_sys_ps_check_lost_instrumentation_view_test.c
  )
  target_link_libraries(mylite_runtime_sys_ps_check_lost_instrumentation_view_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_ps_check_lost_instrumentation_view_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_ps_check_lost_instrumentation_view_test)

  add_executable(mylite_runtime_sys_schema_auto_increment_columns_view_test
    tests/runtime_sys_schema_auto_increment_columns_view_test.c
  )
  target_link_libraries(mylite_runtime_sys_schema_auto_increment_columns_view_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_schema_auto_increment_columns_view_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_schema_auto_increment_columns_view_test)

  add_executable(mylite_runtime_sys_schema_index_statistics_views_test
    tests/runtime_sys_schema_index_statistics_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_schema_index_statistics_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_schema_index_statistics_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_schema_index_statistics_views_test)

  add_executable(mylite_runtime_sys_schema_redundant_indexes_views_test
    tests/runtime_sys_schema_redundant_indexes_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_schema_redundant_indexes_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_schema_redundant_indexes_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_schema_redundant_indexes_views_test)

  add_executable(mylite_runtime_sys_schema_table_lock_waits_views_test
    tests/runtime_sys_schema_table_lock_waits_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_schema_table_lock_waits_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_schema_table_lock_waits_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_schema_table_lock_waits_views_test)

  add_executable(mylite_runtime_sys_x_ps_schema_table_statistics_io_view_test
    tests/runtime_sys_x_ps_schema_table_statistics_io_view_test.c
  )
  target_link_libraries(mylite_runtime_sys_x_ps_schema_table_statistics_io_view_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_x_ps_schema_table_statistics_io_view_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_x_ps_schema_table_statistics_io_view_test)

  add_executable(mylite_runtime_sys_schema_table_statistics_views_test
    tests/runtime_sys_schema_table_statistics_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_schema_table_statistics_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_schema_table_statistics_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_schema_table_statistics_views_test)

  add_executable(mylite_runtime_sys_schema_table_statistics_with_buffer_views_test
    tests/runtime_sys_schema_table_statistics_with_buffer_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_schema_table_statistics_with_buffer_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_schema_table_statistics_with_buffer_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_schema_table_statistics_with_buffer_views_test)

  add_executable(mylite_runtime_sys_schema_tables_with_full_table_scans_views_test
    tests/runtime_sys_schema_tables_with_full_table_scans_views_test.c
  )
  target_link_libraries(mylite_runtime_sys_schema_tables_with_full_table_scans_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_schema_tables_with_full_table_scans_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_schema_tables_with_full_table_scans_views_test)

  add_executable(mylite_runtime_sys_schema_unused_indexes_view_test
    tests/runtime_sys_schema_unused_indexes_view_test.c
  )
  target_link_libraries(mylite_runtime_sys_schema_unused_indexes_view_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_schema_unused_indexes_view_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_schema_unused_indexes_view_test)

  add_executable(mylite_runtime_sys_schema_object_overview_view_test
    tests/runtime_sys_schema_object_overview_view_test.c
  )
  target_link_libraries(mylite_runtime_sys_schema_object_overview_view_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sys_schema_object_overview_view_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sys_schema_object_overview_view_test)

  add_executable(mylite_runtime_mysql_enterprise_table_absence_test
    tests/runtime_mysql_enterprise_table_absence_test.c
  )
  target_link_libraries(mylite_runtime_mysql_enterprise_table_absence_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_enterprise_table_absence_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_enterprise_table_absence_test)

  add_executable(mylite_runtime_performance_schema_optional_absence_test
    tests/runtime_performance_schema_optional_absence_test.c
  )
  target_link_libraries(mylite_runtime_performance_schema_optional_absence_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_performance_schema_optional_absence_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_performance_schema_optional_absence_test)

  add_executable(mylite_runtime_mysql_data_dictionary_table_diagnostics_test
    tests/runtime_mysql_data_dictionary_table_diagnostics_test.c
  )
  target_link_libraries(mylite_runtime_mysql_data_dictionary_table_diagnostics_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_data_dictionary_table_diagnostics_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_data_dictionary_table_diagnostics_test)

  add_executable(mylite_runtime_mysql_system_stats_table_status_test
    tests/runtime_mysql_system_stats_table_status_test.c
  )
  target_link_libraries(mylite_runtime_mysql_system_stats_table_status_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_system_stats_table_status_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_system_stats_table_status_test)

  add_executable(mylite_runtime_mysql_innodb_index_stats_test
    tests/runtime_mysql_innodb_index_stats_test.c
  )
  target_link_libraries(mylite_runtime_mysql_innodb_index_stats_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_innodb_index_stats_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_innodb_index_stats_test)

  add_executable(mylite_runtime_mysql_innodb_table_stats_test
    tests/runtime_mysql_innodb_table_stats_test.c
  )
  target_link_libraries(mylite_runtime_mysql_innodb_table_stats_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_innodb_table_stats_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_innodb_table_stats_test)

  add_executable(mylite_runtime_mysql_system_show_columns_test
    tests/runtime_mysql_system_show_columns_test.c
  )
  target_link_libraries(mylite_runtime_mysql_system_show_columns_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_system_show_columns_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_system_show_columns_test)

  add_executable(mylite_runtime_mysql_system_show_index_test
    tests/runtime_mysql_system_show_index_test.c
  )
  target_link_libraries(mylite_runtime_mysql_system_show_index_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_mysql_system_show_index_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_mysql_system_show_index_test)

  add_executable(mylite_runtime_information_schema_innodb_session_temp_tablespaces_test
    tests/runtime_information_schema_innodb_session_temp_tablespaces_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_session_temp_tablespaces_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(
    mylite_runtime_information_schema_innodb_session_temp_tablespaces_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(
    mylite_runtime_information_schema_innodb_session_temp_tablespaces_test
  )

  add_executable(mylite_runtime_information_schema_innodb_virtual_test
    tests/runtime_information_schema_innodb_virtual_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_virtual_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_virtual_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_virtual_test)

  add_executable(mylite_runtime_information_schema_innodb_trx_test
    tests/runtime_information_schema_innodb_trx_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_trx_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_trx_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_trx_test)

  add_executable(mylite_runtime_information_schema_innodb_cached_indexes_test
    tests/runtime_information_schema_innodb_cached_indexes_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_cached_indexes_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_cached_indexes_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_cached_indexes_test)

  add_executable(mylite_runtime_information_schema_innodb_buffer_page_tables_test
    tests/runtime_information_schema_innodb_buffer_page_tables_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_buffer_page_tables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_buffer_page_tables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_buffer_page_tables_test)

  add_executable(mylite_runtime_information_schema_innodb_buffer_pool_stats_test
    tests/runtime_information_schema_innodb_buffer_pool_stats_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_buffer_pool_stats_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_buffer_pool_stats_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_buffer_pool_stats_test)

  add_executable(mylite_runtime_information_schema_innodb_metrics_test
    tests/runtime_information_schema_innodb_metrics_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_metrics_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_metrics_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_metrics_test)

  add_executable(mylite_runtime_information_schema_innodb_cmp_per_index_test
    tests/runtime_information_schema_innodb_cmp_per_index_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_cmp_per_index_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_cmp_per_index_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_cmp_per_index_test)

  add_executable(mylite_runtime_information_schema_innodb_tablespace_metadata_test
    tests/runtime_information_schema_innodb_tablespace_metadata_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_tablespace_metadata_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_tablespace_metadata_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_tablespace_metadata_test)

  add_executable(mylite_runtime_information_schema_files_test
    tests/runtime_information_schema_files_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_files_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_files_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_files_test)

  add_executable(mylite_runtime_information_schema_innodb_ft_config_test
    tests/runtime_information_schema_innodb_ft_config_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_ft_config_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_ft_config_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_ft_config_test)

  add_executable(mylite_runtime_information_schema_innodb_ft_deleted_test
    tests/runtime_information_schema_innodb_ft_deleted_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_ft_deleted_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_ft_deleted_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_ft_deleted_test)

  add_executable(mylite_runtime_information_schema_innodb_ft_index_test
    tests/runtime_information_schema_innodb_ft_index_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_ft_index_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_ft_index_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_ft_index_test)

  add_executable(mylite_runtime_information_schema_innodb_ft_default_stopword_test
    tests/runtime_information_schema_innodb_ft_default_stopword_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_ft_default_stopword_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_ft_default_stopword_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_ft_default_stopword_test)

  add_executable(mylite_runtime_information_schema_innodb_temp_table_info_test
    tests/runtime_information_schema_innodb_temp_table_info_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_innodb_temp_table_info_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_innodb_temp_table_info_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_innodb_temp_table_info_test)

  add_executable(mylite_runtime_information_schema_column_statistics_test
    tests/runtime_information_schema_column_statistics_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_column_statistics_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_column_statistics_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_column_statistics_test)

  add_executable(mylite_runtime_information_schema_connection_control_failed_login_attempts_test
    tests/runtime_information_schema_connection_control_failed_login_attempts_test.c
  )
  target_link_libraries(
    mylite_runtime_information_schema_connection_control_failed_login_attempts_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(
    mylite_runtime_information_schema_connection_control_failed_login_attempts_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(
    mylite_runtime_information_schema_connection_control_failed_login_attempts_test
  )

  add_executable(mylite_runtime_information_schema_predicates_test
    tests/runtime_information_schema_predicates_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_predicates_test)

  add_executable(mylite_runtime_information_schema_write_access_test
    tests/runtime_information_schema_write_access_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_write_access_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_write_access_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_write_access_test)
