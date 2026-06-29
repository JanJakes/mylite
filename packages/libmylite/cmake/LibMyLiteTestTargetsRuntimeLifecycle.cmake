  add_executable(mylite_runtime_builtin_schema_write_access_test
    tests/runtime_builtin_schema_write_access_test.c
  )
  target_link_libraries(mylite_runtime_builtin_schema_write_access_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_builtin_schema_write_access_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_builtin_schema_write_access_test)

  add_executable(mylite_runtime_information_schema_constraints_test
    tests/runtime_information_schema_constraints_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_constraints_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_constraints_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_constraints_test)

  add_executable(mylite_runtime_information_schema_check_constraints_test
    tests/runtime_information_schema_check_constraints_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_check_constraints_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_check_constraints_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_check_constraints_test)

  add_executable(mylite_runtime_information_schema_events_test
    tests/runtime_information_schema_events_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_events_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_events_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_events_test)

  add_executable(mylite_runtime_information_schema_parameters_test
    tests/runtime_information_schema_parameters_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_parameters_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_parameters_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_parameters_test)

  add_executable(mylite_runtime_information_schema_partitions_test
    tests/runtime_information_schema_partitions_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_partitions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_partitions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_partitions_test)

  add_executable(mylite_runtime_information_schema_processlist_test
    tests/runtime_information_schema_processlist_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_processlist_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_processlist_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_processlist_test)

  add_executable(mylite_runtime_information_schema_privileges_test
    tests/runtime_information_schema_privileges_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_privileges_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_privileges_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_privileges_test)

  add_executable(mylite_runtime_information_schema_user_attributes_test
    tests/runtime_information_schema_user_attributes_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_user_attributes_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_user_attributes_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_user_attributes_test)

  add_executable(mylite_runtime_information_schema_role_grant_tables_test
    tests/runtime_information_schema_role_grant_tables_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_role_grant_tables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_role_grant_tables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_role_grant_tables_test)

  add_executable(mylite_runtime_information_schema_role_session_tables_test
    tests/runtime_information_schema_role_session_tables_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_role_session_tables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_role_session_tables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_role_session_tables_test)

  add_executable(mylite_runtime_information_schema_optimizer_trace_test
    tests/runtime_information_schema_optimizer_trace_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_optimizer_trace_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_optimizer_trace_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_optimizer_trace_test)

  add_executable(mylite_runtime_information_schema_profiling_test
    tests/runtime_information_schema_profiling_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_profiling_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_profiling_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_profiling_test)

  add_executable(mylite_runtime_information_schema_resource_groups_test
    tests/runtime_information_schema_resource_groups_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_resource_groups_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_resource_groups_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_resource_groups_test)

  add_executable(mylite_runtime_information_schema_st_geometry_columns_test
    tests/runtime_information_schema_st_geometry_columns_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_st_geometry_columns_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_st_geometry_columns_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_st_geometry_columns_test)

  add_executable(mylite_runtime_information_schema_st_spatial_reference_systems_test
    tests/runtime_information_schema_st_spatial_reference_systems_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_st_spatial_reference_systems_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_st_spatial_reference_systems_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_st_spatial_reference_systems_test)

  add_executable(mylite_runtime_information_schema_st_units_of_measure_test
    tests/runtime_information_schema_st_units_of_measure_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_st_units_of_measure_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_st_units_of_measure_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_st_units_of_measure_test)

  add_executable(mylite_runtime_information_schema_routines_test
    tests/runtime_information_schema_routines_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_routines_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_routines_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_routines_test)

  add_executable(mylite_runtime_information_schema_triggers_test
    tests/runtime_information_schema_triggers_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_triggers_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_triggers_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_triggers_test)

  add_executable(mylite_runtime_information_schema_views_test
    tests/runtime_information_schema_views_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_views_test)

  add_executable(mylite_runtime_information_schema_view_table_usage_test
    tests/runtime_information_schema_view_table_usage_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_view_table_usage_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_view_table_usage_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_view_table_usage_test)

  add_executable(mylite_runtime_information_schema_view_routine_usage_test
    tests/runtime_information_schema_view_routine_usage_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_view_routine_usage_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_view_routine_usage_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_view_routine_usage_test)

  add_executable(mylite_runtime_information_schema_static_catalogs_test
    tests/runtime_information_schema_static_catalogs_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_static_catalogs_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_static_catalogs_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_static_catalogs_test)

  add_executable(mylite_runtime_builtin_schema_table_directory_test
    tests/runtime_builtin_schema_table_directory_test.c
  )
  target_link_libraries(mylite_runtime_builtin_schema_table_directory_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_builtin_schema_table_directory_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_builtin_schema_table_directory_test)

  add_executable(mylite_runtime_show_plugins_metadata_test
    tests/runtime_show_plugins_metadata_test.c
  )
  target_link_libraries(mylite_runtime_show_plugins_metadata_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_plugins_metadata_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_plugins_metadata_test)

  add_executable(mylite_runtime_information_schema_keywords_test
    tests/runtime_information_schema_keywords_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_keywords_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_keywords_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_keywords_test)

  add_executable(mylite_runtime_show_index_empty_introspection_test
    tests/runtime_show_index_empty_introspection_test.c
  )
  target_link_libraries(mylite_runtime_show_index_empty_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_index_empty_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_index_empty_introspection_test)

  add_executable(mylite_runtime_show_triggers_empty_introspection_test
    tests/runtime_show_triggers_empty_introspection_test.c
  )
  target_link_libraries(mylite_runtime_show_triggers_empty_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_triggers_empty_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_triggers_empty_introspection_test)

  add_executable(mylite_runtime_show_events_empty_introspection_test
    tests/runtime_show_events_empty_introspection_test.c
  )
  target_link_libraries(mylite_runtime_show_events_empty_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_events_empty_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_events_empty_introspection_test)

  add_executable(mylite_runtime_show_open_tables_empty_introspection_test
    tests/runtime_show_open_tables_empty_introspection_test.c
  )
  target_link_libraries(mylite_runtime_show_open_tables_empty_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_open_tables_empty_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_open_tables_empty_introspection_test)

  add_executable(mylite_runtime_show_routine_status_empty_introspection_test
    tests/runtime_show_routine_status_empty_introspection_test.c
  )
  target_link_libraries(mylite_runtime_show_routine_status_empty_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_routine_status_empty_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_routine_status_empty_introspection_test)

  add_executable(mylite_runtime_stored_procedure_select_call_test
    tests/runtime_stored_procedure_select_call_test.c
  )
  target_link_libraries(mylite_runtime_stored_procedure_select_call_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_stored_procedure_select_call_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_stored_procedure_select_call_test)

  add_executable(mylite_runtime_stored_procedure_local_variables_test
    tests/runtime_stored_procedure_local_variables_test.c
  )
  target_link_libraries(mylite_runtime_stored_procedure_local_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_stored_procedure_local_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_stored_procedure_local_variables_test)

  add_executable(mylite_runtime_admin_stored_program_placeholders_test
    tests/runtime_admin_stored_program_placeholders_test.c
  )
  target_link_libraries(mylite_runtime_admin_stored_program_placeholders_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_admin_stored_program_placeholders_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_admin_stored_program_placeholders_test)

  add_executable(mylite_runtime_utility_diagnostics_placeholders_test
    tests/runtime_utility_diagnostics_placeholders_test.c
  )
  target_link_libraries(mylite_runtime_utility_diagnostics_placeholders_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_utility_diagnostics_placeholders_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_utility_diagnostics_placeholders_test)

  add_executable(mylite_runtime_show_processlist_introspection_test
    tests/runtime_show_processlist_introspection_test.c
  )
  target_link_libraries(mylite_runtime_show_processlist_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_processlist_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_processlist_introspection_test)

  add_executable(mylite_runtime_show_grants_test
    tests/runtime_show_grants_test.c
  )
  target_link_libraries(mylite_runtime_show_grants_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_grants_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_grants_test)

  add_executable(mylite_runtime_show_create_residuals_test
    tests/runtime_show_create_residuals_test.c
  )
  target_link_libraries(mylite_runtime_show_create_residuals_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_create_residuals_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_create_residuals_test)

  add_executable(mylite_runtime_show_privileges_test
    tests/runtime_show_privileges_test.c
  )
  target_link_libraries(mylite_runtime_show_privileges_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_privileges_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_privileges_test)

  add_executable(mylite_runtime_show_binary_log_metadata_test
    tests/runtime_show_binary_log_metadata_test.c
  )
  target_link_libraries(mylite_runtime_show_binary_log_metadata_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_binary_log_metadata_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_binary_log_metadata_test)

  add_executable(mylite_runtime_show_replica_metadata_test
    tests/runtime_show_replica_metadata_test.c
  )
  target_link_libraries(mylite_runtime_show_replica_metadata_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_replica_metadata_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_replica_metadata_test)

  add_executable(mylite_runtime_show_warnings_diagnostics_test
    tests/runtime_show_warnings_diagnostics_test.c
  )
  target_link_libraries(mylite_runtime_show_warnings_diagnostics_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_warnings_diagnostics_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_warnings_diagnostics_test)

  add_executable(mylite_runtime_show_errors_diagnostics_test
    tests/runtime_show_errors_diagnostics_test.c
  )
  target_link_libraries(mylite_runtime_show_errors_diagnostics_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_errors_diagnostics_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_errors_diagnostics_test)

  add_executable(mylite_runtime_diagnostics_count_variables_test
    tests/runtime_diagnostics_count_variables_test.c
  )
  target_link_libraries(mylite_runtime_diagnostics_count_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_diagnostics_count_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_diagnostics_count_variables_test)

  add_executable(mylite_runtime_diagnostics_code_order_test
    tests/runtime_diagnostics_code_order_test.c
  )
  target_link_libraries(mylite_runtime_diagnostics_code_order_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_diagnostics_code_order_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_diagnostics_code_order_test)

  add_executable(mylite_runtime_autocommit_system_variable_test
    tests/runtime_autocommit_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_autocommit_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_autocommit_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_autocommit_system_variable_test)

  add_executable(mylite_runtime_sql_quote_show_create_system_variable_test
    tests/runtime_sql_quote_show_create_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_quote_show_create_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_quote_show_create_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_quote_show_create_system_variable_test)

  add_executable(mylite_runtime_foreign_key_checks_system_variable_test
    tests/runtime_foreign_key_checks_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_foreign_key_checks_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_foreign_key_checks_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_foreign_key_checks_system_variable_test)

  add_executable(mylite_runtime_unique_checks_system_variable_test
    tests/runtime_unique_checks_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_unique_checks_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_unique_checks_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_unique_checks_system_variable_test)

  add_executable(mylite_runtime_updatable_views_with_limit_system_variable_test
    tests/runtime_updatable_views_with_limit_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_updatable_views_with_limit_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_updatable_views_with_limit_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_updatable_views_with_limit_system_variable_test)

  add_executable(mylite_runtime_sql_auto_is_null_system_variable_test
    tests/runtime_sql_auto_is_null_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_auto_is_null_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_auto_is_null_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_auto_is_null_system_variable_test)

  add_executable(mylite_runtime_sql_big_selects_system_variable_test
    tests/runtime_sql_big_selects_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_big_selects_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_big_selects_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_big_selects_system_variable_test)

  add_executable(mylite_runtime_sql_generate_invisible_primary_key_system_variable_test
    tests/runtime_sql_generate_invisible_primary_key_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_generate_invisible_primary_key_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_generate_invisible_primary_key_system_variable_test
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(
    mylite_runtime_sql_generate_invisible_primary_key_system_variable_test
  )

  add_executable(mylite_runtime_explicit_defaults_for_timestamp_system_variable_test
    tests/runtime_explicit_defaults_for_timestamp_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_explicit_defaults_for_timestamp_system_variable_test
    PRIVATE
    MyLite::mylite
  )
  target_include_directories(
    mylite_runtime_explicit_defaults_for_timestamp_system_variable_test
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(
    mylite_runtime_explicit_defaults_for_timestamp_system_variable_test
  )

  add_executable(mylite_runtime_sql_buffer_result_system_variable_test
    tests/runtime_sql_buffer_result_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_buffer_result_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_buffer_result_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_buffer_result_system_variable_test)

  add_executable(mylite_runtime_sql_log_bin_system_variable_test
    tests/runtime_sql_log_bin_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_log_bin_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_log_bin_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_log_bin_system_variable_test)

  add_executable(mylite_runtime_server_identity_binary_log_system_variables_test
    tests/runtime_server_identity_binary_log_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_server_identity_binary_log_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_server_identity_binary_log_system_variables_test
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_server_identity_binary_log_system_variables_test)

  add_executable(mylite_runtime_server_environment_system_variables_test
    tests/runtime_server_environment_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_server_environment_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_server_environment_system_variables_test
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_server_environment_system_variables_test)

  add_executable(mylite_runtime_server_build_system_variables_test
    tests/runtime_server_build_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_server_build_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_server_build_system_variables_test
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_server_build_system_variables_test)

  add_executable(mylite_runtime_sql_log_off_system_variable_test
    tests/runtime_sql_log_off_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_log_off_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_log_off_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_log_off_system_variable_test)

  add_executable(mylite_runtime_sql_mode_system_variable_test
    tests/runtime_sql_mode_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_mode_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_mode_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_mode_system_variable_test)

  add_executable(mylite_runtime_time_zone_system_variable_test
    tests/runtime_time_zone_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_time_zone_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_time_zone_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_time_zone_system_variable_test)

  add_executable(mylite_runtime_sql_require_primary_key_system_variable_test
    tests/runtime_sql_require_primary_key_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_require_primary_key_system_variable_test
    PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_require_primary_key_system_variable_test
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(
    mylite_runtime_sql_require_primary_key_system_variable_test
  )

  add_executable(mylite_runtime_sql_replica_skip_counter_system_variable_test
    tests/runtime_sql_replica_skip_counter_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_replica_skip_counter_system_variable_test
    PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_replica_skip_counter_system_variable_test
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(
    mylite_runtime_sql_replica_skip_counter_system_variable_test
  )

  add_executable(mylite_runtime_sql_slave_skip_counter_system_variable_test
    tests/runtime_sql_slave_skip_counter_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_slave_skip_counter_system_variable_test
    PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_slave_skip_counter_system_variable_test
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(
    mylite_runtime_sql_slave_skip_counter_system_variable_test
  )

  add_executable(mylite_runtime_sql_safe_updates_system_variable_test
    tests/runtime_sql_safe_updates_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_safe_updates_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_safe_updates_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_safe_updates_system_variable_test)

  add_executable(mylite_runtime_sql_select_limit_system_variable_test
    tests/runtime_sql_select_limit_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_select_limit_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_select_limit_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_select_limit_system_variable_test)

  add_executable(mylite_runtime_max_allowed_packet_system_variable_test
    tests/runtime_max_allowed_packet_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_max_allowed_packet_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_max_allowed_packet_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_max_allowed_packet_system_variable_test)

  add_executable(mylite_runtime_timeout_system_variables_test
    tests/runtime_timeout_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_timeout_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_timeout_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_timeout_system_variables_test)

  add_executable(mylite_runtime_auto_increment_step_system_variables_test
    tests/runtime_auto_increment_step_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_auto_increment_step_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_auto_increment_step_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_auto_increment_step_system_variables_test)

  add_executable(mylite_runtime_sql_notes_system_variable_test
    tests/runtime_sql_notes_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_notes_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_notes_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_notes_system_variable_test)

  add_executable(mylite_runtime_max_error_count_system_variable_test
    tests/runtime_max_error_count_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_max_error_count_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_max_error_count_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_max_error_count_system_variable_test)

  add_executable(mylite_runtime_sql_warnings_system_variable_test
    tests/runtime_sql_warnings_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_warnings_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_warnings_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_warnings_system_variable_test)

  add_executable(mylite_runtime_character_set_system_variables_test
    tests/runtime_character_set_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_character_set_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_character_set_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_character_set_system_variables_test)

  add_executable(mylite_runtime_set_connection_character_set_test
    tests/runtime_set_connection_character_set_test.c
  )
  target_link_libraries(mylite_runtime_set_connection_character_set_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_set_connection_character_set_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_set_connection_character_set_test)

  add_executable(mylite_runtime_set_fixed_system_variables_test
    tests/runtime_set_fixed_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_set_fixed_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_set_fixed_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_set_fixed_system_variables_test)

  add_executable(mylite_runtime_read_only_system_variables_test
    tests/runtime_read_only_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_read_only_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_read_only_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_read_only_system_variables_test)

  add_executable(mylite_runtime_user_variables_test
    tests/runtime_user_variables_test.c
  )
  target_link_libraries(mylite_runtime_user_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_user_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_user_variables_test)

  add_executable(mylite_runtime_select_into_user_variables_test
    tests/runtime_select_into_user_variables_test.c
  )
  target_link_libraries(mylite_runtime_select_into_user_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_select_into_user_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_select_into_user_variables_test)

  add_executable(mylite_runtime_sql_prepared_statements_test
    tests/runtime_sql_prepared_statements_test.c
  )
  target_link_libraries(mylite_runtime_sql_prepared_statements_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_prepared_statements_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_prepared_statements_test)

  add_executable(mylite_runtime_show_like_filters_test
    tests/runtime_show_like_filters_test.c
  )
  target_link_libraries(mylite_runtime_show_like_filters_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_like_filters_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_like_filters_test)

  add_executable(mylite_runtime_show_databases_where_test
    tests/runtime_show_databases_where_test.c
  )
  target_link_libraries(mylite_runtime_show_databases_where_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_databases_where_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_databases_where_test)

  add_executable(mylite_runtime_show_create_table_test
    tests/runtime_show_create_table_test.c
  )
  target_link_libraries(mylite_runtime_show_create_table_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_show_create_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_create_table_test)

  add_executable(mylite_runtime_create_table_comment_option_test
    tests/runtime_create_table_comment_option_test.c
  )
  target_link_libraries(mylite_runtime_create_table_comment_option_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_create_table_comment_option_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_create_table_comment_option_test)

  add_executable(mylite_runtime_table_storage_statistics_options_test
    tests/runtime_table_storage_statistics_options_test.c
  )
  target_link_libraries(mylite_runtime_table_storage_statistics_options_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_table_storage_statistics_options_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_table_storage_statistics_options_test)

  add_executable(mylite_runtime_column_comments_test
    tests/runtime_column_comments_test.c
  )
  target_link_libraries(mylite_runtime_column_comments_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_column_comments_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_column_comments_test)

  add_executable(mylite_runtime_show_create_database_test
    tests/runtime_show_create_database_test.c
  )
  target_link_libraries(mylite_runtime_show_create_database_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_create_database_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_create_database_test)

  add_executable(mylite_runtime_schema_default_charset_collation_test
    tests/runtime_schema_default_charset_collation_test.c
  )
  target_link_libraries(mylite_runtime_schema_default_charset_collation_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_schema_default_charset_collation_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_schema_default_charset_collation_test)

  add_executable(mylite_runtime_show_table_status_introspection_test
    tests/runtime_show_table_status_introspection_test.c
  )
  target_link_libraries(mylite_runtime_show_table_status_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_table_status_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_table_status_introspection_test)

  add_executable(mylite_runtime_show_character_set_collation_test
    tests/runtime_show_character_set_collation_test.c
  )
  target_link_libraries(mylite_runtime_show_character_set_collation_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_character_set_collation_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_character_set_collation_test)

  add_executable(mylite_runtime_show_variables_test
    tests/runtime_show_variables_test.c
  )
  target_link_libraries(mylite_runtime_show_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_variables_test)

  add_executable(mylite_runtime_show_variables_optional_absence_test
    tests/runtime_show_variables_optional_absence_test.c
  )
  target_link_libraries(mylite_runtime_show_variables_optional_absence_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_variables_optional_absence_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_variables_optional_absence_test)

  add_executable(mylite_runtime_show_status_test
    tests/runtime_show_status_test.c
  )
  target_link_libraries(mylite_runtime_show_status_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_status_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_status_test)

  add_executable(mylite_runtime_show_status_optional_absence_test
    tests/runtime_show_status_optional_absence_test.c
  )
  target_link_libraries(mylite_runtime_show_status_optional_absence_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_status_optional_absence_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_status_optional_absence_test)

  add_executable(mylite_runtime_innodb_engine_surface_test
    tests/runtime_innodb_engine_surface_test.c
  )
  target_link_libraries(mylite_runtime_innodb_engine_surface_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_innodb_engine_surface_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_innodb_engine_surface_test)

  add_executable(mylite_runtime_table_charset_collation_surface_test
    tests/runtime_table_charset_collation_surface_test.c
  )
  target_link_libraries(mylite_runtime_table_charset_collation_surface_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_table_charset_collation_surface_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_table_charset_collation_surface_test)

  add_executable(mylite_runtime_wordpress_core_ddl_fixtures_test
    tests/runtime_wordpress_core_ddl_fixtures_test.c
  )
  target_link_libraries(mylite_runtime_wordpress_core_ddl_fixtures_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_wordpress_core_ddl_fixtures_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_wordpress_core_ddl_fixtures_test)

  add_executable(mylite_runtime_wordpress_dbdelta_introspection_test
    tests/runtime_wordpress_dbdelta_introspection_test.c
  )
  target_link_libraries(mylite_runtime_wordpress_dbdelta_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_wordpress_dbdelta_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_wordpress_dbdelta_introspection_test)

  add_executable(mylite_runtime_column_charset_collation_attributes_test
    tests/runtime_column_charset_collation_attributes_test.c
  )
  target_link_libraries(mylite_runtime_column_charset_collation_attributes_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_column_charset_collation_attributes_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_column_charset_collation_attributes_test)

  add_executable(mylite_runtime_alter_table_default_charset_collation_test
    tests/runtime_alter_table_default_charset_collation_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_default_charset_collation_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_alter_table_default_charset_collation_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_default_charset_collation_test)

  add_executable(mylite_runtime_alter_table_convert_character_set_test
    tests/runtime_alter_table_convert_character_set_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_convert_character_set_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_alter_table_convert_character_set_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_convert_character_set_test)

  add_executable(mylite_runtime_alter_table_order_by_test
    tests/runtime_alter_table_order_by_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_order_by_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_order_by_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_order_by_test)

  add_executable(mylite_runtime_alter_table_force_test
    tests/runtime_alter_table_force_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_force_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_force_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_force_test)

  add_executable(mylite_runtime_alter_table_algorithm_lock_clauses_test
    tests/runtime_alter_table_algorithm_lock_clauses_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_algorithm_lock_clauses_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_alter_table_algorithm_lock_clauses_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_algorithm_lock_clauses_test)

  add_executable(mylite_runtime_alter_table_disable_enable_keys_test
    tests/runtime_alter_table_disable_enable_keys_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_disable_enable_keys_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_alter_table_disable_enable_keys_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_disable_enable_keys_test)

  add_executable(mylite_runtime_table_maintenance_test
    tests/runtime_table_maintenance_test.c
  )
  target_link_libraries(mylite_runtime_table_maintenance_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_table_maintenance_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_table_maintenance_test)

  add_executable(mylite_runtime_create_table_like_lifecycle_test
    tests/runtime_create_table_like_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_create_table_like_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_create_table_like_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_create_table_like_lifecycle_test)

  add_executable(mylite_runtime_temporary_table_like_test
    tests/runtime_temporary_table_like_test.c
  )
  target_link_libraries(mylite_runtime_temporary_table_like_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_temporary_table_like_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_temporary_table_like_test)

  add_executable(mylite_runtime_temporary_create_table_select_test
    tests/runtime_temporary_create_table_select_test.c
  )
  target_link_libraries(mylite_runtime_temporary_create_table_select_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_temporary_create_table_select_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_temporary_create_table_select_test)

  add_executable(mylite_runtime_temporary_auto_increment_test
    tests/runtime_temporary_auto_increment_test.c
  )
  target_link_libraries(mylite_runtime_temporary_auto_increment_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_temporary_auto_increment_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_temporary_auto_increment_test)

  add_executable(mylite_runtime_secondary_index_lifecycle_test
    tests/runtime_secondary_index_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_secondary_index_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_secondary_index_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_secondary_index_lifecycle_test)

  add_executable(mylite_runtime_fulltext_index_metadata_test
    tests/runtime_fulltext_index_metadata_test.c
  )
  target_link_libraries(mylite_runtime_fulltext_index_metadata_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_fulltext_index_metadata_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_fulltext_index_metadata_test)

  add_executable(mylite_runtime_spatial_index_metadata_test
    tests/runtime_spatial_index_metadata_test.c
  )
  target_link_libraries(mylite_runtime_spatial_index_metadata_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_spatial_index_metadata_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_spatial_index_metadata_test)

  add_executable(mylite_runtime_index_options_metadata_test
    tests/runtime_index_options_metadata_test.c
  )
  target_link_libraries(mylite_runtime_index_options_metadata_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_index_options_metadata_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_index_options_metadata_test)

  add_executable(mylite_runtime_alter_table_add_index_test
    tests/runtime_alter_table_add_index_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_add_index_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_add_index_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_add_index_test)

  add_executable(mylite_runtime_alter_table_add_unique_test
    tests/runtime_alter_table_add_unique_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_add_unique_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_add_unique_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_add_unique_test)

  add_executable(mylite_runtime_create_index_lifecycle_test
    tests/runtime_create_index_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_create_index_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_create_index_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_create_index_lifecycle_test)

  add_executable(mylite_runtime_functional_multivalued_index_diagnostics_test
    tests/runtime_functional_multivalued_index_diagnostics_test.c
  )
  target_link_libraries(mylite_runtime_functional_multivalued_index_diagnostics_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_functional_multivalued_index_diagnostics_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_functional_multivalued_index_diagnostics_test)

  add_executable(mylite_runtime_alter_table_drop_index_test
    tests/runtime_alter_table_drop_index_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_drop_index_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_drop_index_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_drop_index_test)

  add_executable(mylite_runtime_alter_table_multi_action_test
    tests/runtime_alter_table_multi_action_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_multi_action_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_alter_table_multi_action_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_multi_action_test)

  add_executable(mylite_runtime_drop_index_lifecycle_test
    tests/runtime_drop_index_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_drop_index_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_drop_index_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_drop_index_lifecycle_test)

  add_executable(mylite_runtime_drop_constraint_lifecycle_test
    tests/runtime_drop_constraint_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_drop_constraint_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_drop_constraint_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_drop_constraint_lifecycle_test)

  add_executable(mylite_runtime_rename_index_lifecycle_test
    tests/runtime_rename_index_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_rename_index_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_rename_index_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_rename_index_lifecycle_test)

  add_executable(mylite_runtime_alter_index_visibility_test
    tests/runtime_alter_index_visibility_test.c
  )
  target_link_libraries(mylite_runtime_alter_index_visibility_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_index_visibility_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_index_visibility_test)

  add_executable(mylite_runtime_unique_index_lifecycle_test
    tests/runtime_unique_index_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_unique_index_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_unique_index_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_unique_index_lifecycle_test)

  add_executable(mylite_runtime_named_unique_constraint_test
    tests/runtime_named_unique_constraint_test.c
  )
  target_link_libraries(mylite_runtime_named_unique_constraint_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_named_unique_constraint_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_named_unique_constraint_test)

  add_executable(mylite_runtime_unique_prefix_index_lifecycle_test
    tests/runtime_unique_prefix_index_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_unique_prefix_index_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_unique_prefix_index_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_unique_prefix_index_lifecycle_test)

  add_executable(mylite_runtime_binary_full_column_index_test
    tests/runtime_binary_full_column_index_test.c
  )
  target_link_libraries(mylite_runtime_binary_full_column_index_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_binary_full_column_index_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_binary_full_column_index_test)

  add_executable(mylite_runtime_descending_index_key_parts_test
    tests/runtime_descending_index_key_parts_test.c
  )
  target_link_libraries(mylite_runtime_descending_index_key_parts_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_descending_index_key_parts_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_descending_index_key_parts_test)

  add_executable(mylite_runtime_foreign_key_constraints_test
    tests/runtime_foreign_key_constraints_test.c
  )
  target_link_libraries(mylite_runtime_foreign_key_constraints_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_foreign_key_constraints_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_foreign_key_constraints_test)

  add_executable(mylite_runtime_check_constraint_lifecycle_test
    tests/runtime_check_constraint_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_check_constraint_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_check_constraint_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_check_constraint_lifecycle_test)

  add_executable(mylite_runtime_alter_check_constraint_lifecycle_test
    tests/runtime_alter_check_constraint_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_alter_check_constraint_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_alter_check_constraint_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_check_constraint_lifecycle_test)

  add_executable(mylite_runtime_char_varchar_key_lifecycle_test
    tests/runtime_char_varchar_key_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_char_varchar_key_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_char_varchar_key_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_char_varchar_key_lifecycle_test)

  add_executable(mylite_runtime_composite_string_primary_key_test
    tests/runtime_composite_string_primary_key_test.c
  )
  target_link_libraries(mylite_runtime_composite_string_primary_key_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_composite_string_primary_key_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_composite_string_primary_key_test)

  add_executable(mylite_runtime_create_table_select_lifecycle_test
    tests/runtime_create_table_select_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_create_table_select_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_create_table_select_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_create_table_select_lifecycle_test)

  add_executable(mylite_runtime_explain_table_introspection_test
    tests/runtime_explain_table_introspection_test.c
  )
  target_link_libraries(mylite_runtime_explain_table_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_explain_table_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_explain_table_introspection_test)

  add_executable(mylite_runtime_table_rename_lifecycle_test
    tests/runtime_table_rename_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_table_rename_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_table_rename_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_table_rename_lifecycle_test)

  add_executable(mylite_runtime_temporary_table_lifecycle_test
    tests/runtime_temporary_table_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_temporary_table_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_temporary_table_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_temporary_table_lifecycle_test)

  add_executable(mylite_runtime_temporary_index_lifecycle_test
    tests/runtime_temporary_index_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_temporary_index_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_temporary_index_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_temporary_index_lifecycle_test)

  add_executable(mylite_runtime_lock_tables_lifecycle_test
    tests/runtime_lock_tables_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_lock_tables_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_lock_tables_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_lock_tables_lifecycle_test)

  add_executable(mylite_runtime_row_values_lifecycle_test
    tests/runtime_row_values_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_row_values_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_row_values_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_row_values_lifecycle_test)

  add_executable(mylite_runtime_replace_values_lifecycle_test
    tests/runtime_replace_values_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_replace_values_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_replace_values_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_replace_values_lifecycle_test)

  add_executable(mylite_runtime_replace_set_lifecycle_test
    tests/runtime_replace_set_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_replace_set_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_replace_set_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_replace_set_lifecycle_test)

  add_executable(mylite_runtime_replace_key_lifecycle_test
    tests/runtime_replace_key_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_replace_key_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_replace_key_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_replace_key_lifecycle_test)

  add_executable(mylite_runtime_replace_select_lifecycle_test
    tests/runtime_replace_select_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_replace_select_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_replace_select_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_replace_select_lifecycle_test)

  add_executable(mylite_runtime_replace_modifier_lifecycle_test
    tests/runtime_replace_modifier_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_replace_modifier_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_replace_modifier_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_replace_modifier_lifecycle_test)

  add_executable(mylite_runtime_insert_modifier_lifecycle_test
    tests/runtime_insert_modifier_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_insert_modifier_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_insert_modifier_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_insert_modifier_lifecycle_test)

  add_executable(mylite_runtime_insert_ignore_lifecycle_test
    tests/runtime_insert_ignore_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_insert_ignore_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_insert_ignore_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_insert_ignore_lifecycle_test)

  add_executable(mylite_runtime_load_data_infile_test
    tests/runtime_load_data_infile_test.c
  )
  target_link_libraries(mylite_runtime_load_data_infile_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_load_data_infile_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_load_data_infile_test)

  add_executable(mylite_runtime_nonstrict_dml_coercion_test
    tests/runtime_nonstrict_dml_coercion_test.c
  )
  target_link_libraries(mylite_runtime_nonstrict_dml_coercion_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_nonstrict_dml_coercion_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_nonstrict_dml_coercion_test)

  add_executable(mylite_runtime_dml_string_numeric_coercion_test
    tests/runtime_dml_string_numeric_coercion_test.c
  )
  target_link_libraries(mylite_runtime_dml_string_numeric_coercion_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_dml_string_numeric_coercion_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_dml_string_numeric_coercion_test)

  add_executable(mylite_runtime_dml_constant_scalar_values_test
    tests/runtime_dml_constant_scalar_values_test.c
  )
  target_link_libraries(mylite_runtime_dml_constant_scalar_values_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_dml_constant_scalar_values_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_dml_constant_scalar_values_test)

  add_executable(mylite_runtime_dml_scalar_expression_values_test
    tests/runtime_dml_scalar_expression_values_test.c
  )
  target_link_libraries(mylite_runtime_dml_scalar_expression_values_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_dml_scalar_expression_values_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_dml_scalar_expression_values_test)

  add_executable(mylite_runtime_insert_on_duplicate_key_update_test
    tests/runtime_insert_on_duplicate_key_update_test.c
  )
  target_link_libraries(mylite_runtime_insert_on_duplicate_key_update_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_insert_on_duplicate_key_update_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_insert_on_duplicate_key_update_test)

  add_executable(mylite_runtime_select_where_lifecycle_test
    tests/runtime_select_where_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_select_where_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_select_where_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_select_where_lifecycle_test)

  add_executable(mylite_runtime_select_row_scalar_predicates_test
    tests/runtime_select_row_scalar_predicates_test.c
  )
  target_link_libraries(mylite_runtime_select_row_scalar_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_select_row_scalar_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_select_row_scalar_predicates_test)

  add_executable(mylite_runtime_exists_subquery_predicates_test
    tests/runtime_exists_subquery_predicates_test.c
  )
  target_link_libraries(mylite_runtime_exists_subquery_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_exists_subquery_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_exists_subquery_predicates_test)

  add_executable(mylite_runtime_in_subquery_predicates_test
    tests/runtime_in_subquery_predicates_test.c
  )
  target_link_libraries(mylite_runtime_in_subquery_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_in_subquery_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_in_subquery_predicates_test)

  add_executable(mylite_runtime_quantified_subquery_predicates_test
    tests/runtime_quantified_subquery_predicates_test.c
  )
  target_link_libraries(mylite_runtime_quantified_subquery_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_quantified_subquery_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_quantified_subquery_predicates_test)

  add_executable(mylite_runtime_where_and_predicates_test
    tests/runtime_where_and_predicates_test.c
  )
  target_link_libraries(mylite_runtime_where_and_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_where_and_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_where_and_predicates_test)

  add_executable(mylite_runtime_select_order_limit_lifecycle_test
    tests/runtime_select_order_limit_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_select_order_limit_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_select_order_limit_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_select_order_limit_lifecycle_test)

  add_executable(mylite_runtime_select_distinct_rowsets_test
    tests/runtime_select_distinct_rowsets_test.c
  )
  target_link_libraries(mylite_runtime_select_distinct_rowsets_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_select_distinct_rowsets_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_select_distinct_rowsets_test)

  add_executable(mylite_runtime_table_statement_test
    tests/runtime_table_statement_test.c
  )
  target_link_libraries(mylite_runtime_table_statement_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_table_statement_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_table_statement_test)

  add_executable(mylite_runtime_values_statement_test
    tests/runtime_values_statement_test.c
  )
  target_link_libraries(mylite_runtime_values_statement_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_values_statement_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_values_statement_test)

  add_executable(mylite_runtime_select_qualified_columns_test
    tests/runtime_select_qualified_columns_test.c
  )
  target_link_libraries(mylite_runtime_select_qualified_columns_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_select_qualified_columns_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_select_qualified_columns_test)

  add_executable(mylite_runtime_union_select_lifecycle_test
    tests/runtime_union_select_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_union_select_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_union_select_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_union_select_lifecycle_test)

  add_executable(mylite_runtime_intersect_except_select_lifecycle_test
    tests/runtime_intersect_except_select_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_intersect_except_select_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_intersect_except_select_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_intersect_except_select_lifecycle_test)

  add_executable(mylite_runtime_inner_join_select_test
    tests/runtime_inner_join_select_test.c
  )
  target_link_libraries(mylite_runtime_inner_join_select_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_inner_join_select_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_inner_join_select_test)

  add_executable(mylite_runtime_left_join_select_test
    tests/runtime_left_join_select_test.c
  )
  target_link_libraries(mylite_runtime_left_join_select_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_left_join_select_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_left_join_select_test)

  add_executable(mylite_runtime_right_join_select_test
    tests/runtime_right_join_select_test.c
  )
  target_link_libraries(mylite_runtime_right_join_select_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_right_join_select_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_right_join_select_test)

  add_executable(mylite_runtime_select_item_alias_test
    tests/runtime_select_item_alias_test.c
  )
  target_link_libraries(mylite_runtime_select_item_alias_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_select_item_alias_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_select_item_alias_test)

  add_executable(mylite_runtime_row_number_window_function_test
    tests/runtime_row_number_window_function_test.c
  )
  target_link_libraries(mylite_runtime_row_number_window_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_row_number_window_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_row_number_window_function_test)

  add_executable(mylite_runtime_named_window_definitions_test
    tests/runtime_named_window_definitions_test.c
  )
  target_link_libraries(mylite_runtime_named_window_definitions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_named_window_definitions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_named_window_definitions_test)

  add_executable(mylite_runtime_core_aggregate_window_functions_test
    tests/runtime_core_aggregate_window_functions_test.c
  )
  target_link_libraries(mylite_runtime_core_aggregate_window_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_core_aggregate_window_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_core_aggregate_window_functions_test)

  add_executable(mylite_runtime_bitwise_aggregate_window_functions_test
    tests/runtime_bitwise_aggregate_window_functions_test.c
  )
  target_link_libraries(mylite_runtime_bitwise_aggregate_window_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_bitwise_aggregate_window_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_bitwise_aggregate_window_functions_test)

  add_executable(mylite_runtime_statistical_aggregate_window_functions_test
    tests/runtime_statistical_aggregate_window_functions_test.c
  )
  target_link_libraries(mylite_runtime_statistical_aggregate_window_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_statistical_aggregate_window_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_statistical_aggregate_window_functions_test)

  add_executable(mylite_runtime_window_rank_navigation_functions_test
    tests/runtime_window_rank_navigation_functions_test.c
  )
  target_link_libraries(mylite_runtime_window_rank_navigation_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_window_rank_navigation_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_window_rank_navigation_functions_test)

  add_executable(mylite_runtime_select_literal_projection_test
    tests/runtime_select_literal_projection_test.c
  )
  target_link_libraries(mylite_runtime_select_literal_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_select_literal_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_select_literal_projection_test)

  add_executable(mylite_runtime_row_scalar_expressions_test
    tests/runtime_row_scalar_expressions_test.c
  )
  target_link_libraries(mylite_runtime_row_scalar_expressions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_row_scalar_expressions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_row_scalar_expressions_test)

  add_executable(mylite_runtime_scalar_subquery_projection_test
    tests/runtime_scalar_subquery_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_subquery_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_subquery_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_subquery_projection_test)

  add_executable(mylite_runtime_string_equality_predicates_test
    tests/runtime_string_equality_predicates_test.c
  )
  target_link_libraries(mylite_runtime_string_equality_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_string_equality_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_equality_predicates_test)

  add_executable(mylite_runtime_expression_collate_predicates_test
    tests/runtime_expression_collate_predicates_test.c
  )
  target_link_libraries(mylite_runtime_expression_collate_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_expression_collate_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_expression_collate_predicates_test)

  add_executable(mylite_runtime_string_order_lifecycle_test
    tests/runtime_string_order_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_string_order_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_string_order_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_order_lifecycle_test)

  add_executable(mylite_runtime_string_range_predicates_test
    tests/runtime_string_range_predicates_test.c
  )
  target_link_libraries(mylite_runtime_string_range_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_string_range_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_range_predicates_test)

  add_executable(mylite_runtime_like_predicates_test
    tests/runtime_like_predicates_test.c
  )
  target_link_libraries(mylite_runtime_like_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_like_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_like_predicates_test)

  add_executable(mylite_runtime_regexp_rlike_predicates_test
    tests/runtime_regexp_rlike_predicates_test.c
  )
  target_link_libraries(mylite_runtime_regexp_rlike_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_regexp_rlike_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_regexp_rlike_predicates_test)

  add_executable(mylite_runtime_regexp_like_function_test
    tests/runtime_regexp_like_function_test.c
  )
  target_link_libraries(mylite_runtime_regexp_like_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_regexp_like_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_regexp_like_function_test)

  add_executable(mylite_runtime_view_lifecycle_test
    tests/runtime_view_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_view_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_view_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_view_lifecycle_test)

  add_executable(mylite_runtime_delete_lifecycle_test
    tests/runtime_delete_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_delete_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_delete_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_delete_lifecycle_test)

  add_executable(mylite_runtime_joined_delete_lifecycle_test
    tests/runtime_joined_delete_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_joined_delete_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_joined_delete_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_joined_delete_lifecycle_test)

  add_executable(mylite_runtime_joined_update_lifecycle_test
    tests/runtime_joined_update_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_joined_update_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_joined_update_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_joined_update_lifecycle_test)

  add_executable(mylite_runtime_update_lifecycle_test
    tests/runtime_update_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_update_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_update_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_update_lifecycle_test)

  add_executable(mylite_runtime_update_index_hints_noop_test
    tests/runtime_update_index_hints_noop_test.c
  )
  target_link_libraries(mylite_runtime_update_index_hints_noop_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_update_index_hints_noop_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_update_index_hints_noop_test)

  add_executable(mylite_runtime_update_ignore_modifier_test
    tests/runtime_update_ignore_modifier_test.c
  )
  target_link_libraries(mylite_runtime_update_ignore_modifier_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_update_ignore_modifier_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_update_ignore_modifier_test)

  add_executable(mylite_runtime_update_scalar_subquery_assignment_test
    tests/runtime_update_scalar_subquery_assignment_test.c
  )
  target_link_libraries(mylite_runtime_update_scalar_subquery_assignment_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_update_scalar_subquery_assignment_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_update_scalar_subquery_assignment_test)

  add_executable(mylite_runtime_update_subquery_predicates_test
    tests/runtime_update_subquery_predicates_test.c
  )
  target_link_libraries(mylite_runtime_update_subquery_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_update_subquery_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_update_subquery_predicates_test)

  add_executable(mylite_runtime_update_arithmetic_assignment_test
    tests/runtime_update_arithmetic_assignment_test.c
  )
  target_link_libraries(mylite_runtime_update_arithmetic_assignment_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_update_arithmetic_assignment_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_update_arithmetic_assignment_test)

  add_executable(mylite_runtime_update_unix_timestamp_arithmetic_test
    tests/runtime_update_unix_timestamp_arithmetic_test.c
  )
  target_link_libraries(mylite_runtime_update_unix_timestamp_arithmetic_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_update_unix_timestamp_arithmetic_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_update_unix_timestamp_arithmetic_test)

  add_executable(mylite_runtime_update_date_interval_assignment_test
    tests/runtime_update_date_interval_assignment_test.c
  )
  target_link_libraries(mylite_runtime_update_date_interval_assignment_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_update_date_interval_assignment_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_update_date_interval_assignment_test)

  add_executable(mylite_runtime_update_multiple_assignments_test
    tests/runtime_update_multiple_assignments_test.c
  )
  target_link_libraries(mylite_runtime_update_multiple_assignments_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_update_multiple_assignments_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_update_multiple_assignments_test)

  add_executable(mylite_runtime_transaction_lifecycle_test
    tests/runtime_transaction_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_transaction_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_transaction_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_transaction_lifecycle_test)

  add_executable(mylite_runtime_dml_default_keyword_values_test
    tests/runtime_dml_default_keyword_values_test.c
  )
  target_link_libraries(mylite_runtime_dml_default_keyword_values_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_dml_default_keyword_values_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_dml_default_keyword_values_test)

  add_executable(mylite_runtime_alter_column_visibility_test
    tests/runtime_alter_column_visibility_test.c
  )
  target_link_libraries(mylite_runtime_alter_column_visibility_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_column_visibility_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_column_visibility_test)

  add_executable(mylite_runtime_small_integer_types_test
    tests/runtime_small_integer_types_test.c
  )
  target_link_libraries(mylite_runtime_small_integer_types_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_small_integer_types_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_small_integer_types_test)

  add_executable(mylite_runtime_varchar_type_test
    tests/runtime_varchar_type_test.c
  )
  target_link_libraries(mylite_runtime_varchar_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_varchar_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_varchar_type_test)

  add_executable(mylite_runtime_enum_type_test
    tests/runtime_enum_type_test.c
  )
  target_link_libraries(mylite_runtime_enum_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_enum_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_enum_type_test)

  add_executable(mylite_runtime_set_type_test
    tests/runtime_set_type_test.c
  )
  target_link_libraries(mylite_runtime_set_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_set_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_set_type_test)

  add_executable(mylite_runtime_char_type_test
    tests/runtime_char_type_test.c
  )
  target_link_libraries(mylite_runtime_char_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_char_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_char_type_test)

  add_executable(mylite_runtime_character_alias_lifecycle_test
    tests/runtime_character_alias_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_character_alias_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_character_alias_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_character_alias_lifecycle_test)

  add_executable(mylite_runtime_text_type_test
    tests/runtime_text_type_test.c
  )
  target_link_libraries(mylite_runtime_text_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_text_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_text_type_test)

  add_executable(mylite_runtime_json_type_test
    tests/runtime_json_type_test.c
  )
  target_link_libraries(mylite_runtime_json_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_type_test)

  add_executable(mylite_runtime_binary_string_types_test
    tests/runtime_binary_string_types_test.c
  )
  target_link_libraries(mylite_runtime_binary_string_types_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_binary_string_types_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_binary_string_types_test)

  add_executable(mylite_runtime_long_character_binary_aliases_test
    tests/runtime_long_character_binary_aliases_test.c
  )
  target_link_libraries(mylite_runtime_long_character_binary_aliases_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_long_character_binary_aliases_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_long_character_binary_aliases_test)

  add_executable(mylite_runtime_bit_type_test
    tests/runtime_bit_type_test.c
  )
  target_link_libraries(mylite_runtime_bit_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_bit_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_bit_type_test)

  add_executable(mylite_runtime_string_defaults_test
    tests/runtime_string_defaults_test.c
  )
  target_link_libraries(mylite_runtime_string_defaults_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_string_defaults_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_defaults_test)

  add_executable(mylite_runtime_decimal_type_test
    tests/runtime_decimal_type_test.c
  )
  target_link_libraries(mylite_runtime_decimal_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_decimal_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_decimal_type_test)

  add_executable(mylite_runtime_float_double_type_test
    tests/runtime_float_double_type_test.c
  )
  target_link_libraries(mylite_runtime_float_double_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_float_double_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_float_double_type_test)

  add_executable(mylite_runtime_date_type_test
    tests/runtime_date_type_test.c
  )
  target_link_libraries(mylite_runtime_date_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_date_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_date_type_test)

  add_executable(mylite_runtime_year_type_test
    tests/runtime_year_type_test.c
  )
  target_link_libraries(mylite_runtime_year_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_year_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_year_type_test)

  add_executable(mylite_runtime_time_type_test
    tests/runtime_time_type_test.c
  )
  target_link_libraries(mylite_runtime_time_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_time_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_time_type_test)

  add_executable(mylite_runtime_datetime_type_test
    tests/runtime_datetime_type_test.c
  )
  target_link_libraries(mylite_runtime_datetime_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_datetime_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_datetime_type_test)

  add_executable(mylite_runtime_timestamp_type_test
    tests/runtime_timestamp_type_test.c
  )
  target_link_libraries(mylite_runtime_timestamp_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_timestamp_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_timestamp_type_test)

  add_executable(mylite_runtime_zero_temporal_sql_modes_test
    tests/runtime_zero_temporal_sql_modes_test.c
  )
  target_link_libraries(mylite_runtime_zero_temporal_sql_modes_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_zero_temporal_sql_modes_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_zero_temporal_sql_modes_test)

  add_executable(mylite_runtime_relaxed_temporal_dml_literals_test
    tests/runtime_relaxed_temporal_dml_literals_test.c
  )
  target_link_libraries(mylite_runtime_relaxed_temporal_dml_literals_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_relaxed_temporal_dml_literals_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_relaxed_temporal_dml_literals_test)

  add_executable(mylite_runtime_current_timestamp_defaults_test
    tests/runtime_current_timestamp_defaults_test.c
  )
  target_link_libraries(mylite_runtime_current_timestamp_defaults_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_current_timestamp_defaults_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_current_timestamp_defaults_test)

  add_executable(mylite_runtime_current_date_time_defaults_test
    tests/runtime_current_date_time_defaults_test.c
  )
  target_link_libraries(mylite_runtime_current_date_time_defaults_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_current_date_time_defaults_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_current_date_time_defaults_test)

  add_executable(mylite_runtime_current_date_time_functions_test
    tests/runtime_current_date_time_functions_test.c
  )
  target_link_libraries(mylite_runtime_current_date_time_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_current_date_time_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_current_date_time_functions_test)

  add_executable(mylite_runtime_utc_date_time_functions_test
    tests/runtime_utc_date_time_functions_test.c
  )
  target_link_libraries(mylite_runtime_utc_date_time_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_utc_date_time_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_utc_date_time_functions_test)

  add_executable(mylite_runtime_sysdate_function_test
    tests/runtime_sysdate_function_test.c
  )
  target_link_libraries(mylite_runtime_sysdate_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sysdate_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sysdate_function_test)

  add_executable(mylite_runtime_primary_key_lifecycle_test
    tests/runtime_primary_key_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_primary_key_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_primary_key_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_primary_key_lifecycle_test)

  add_executable(mylite_runtime_alter_table_add_primary_key_test
    tests/runtime_alter_table_add_primary_key_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_add_primary_key_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_add_primary_key_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_add_primary_key_test)

  add_executable(mylite_runtime_alter_table_drop_primary_key_test
    tests/runtime_alter_table_drop_primary_key_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_drop_primary_key_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_drop_primary_key_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_drop_primary_key_test)

  add_executable(mylite_runtime_alter_table_auto_increment_option_test
    tests/runtime_alter_table_auto_increment_option_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_auto_increment_option_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_alter_table_auto_increment_option_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_auto_increment_option_test)

  add_executable(mylite_runtime_auto_increment_lifecycle_test
    tests/runtime_auto_increment_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_auto_increment_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_auto_increment_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_auto_increment_lifecycle_test)

  add_executable(mylite_runtime_serial_alias_lifecycle_test
    tests/runtime_serial_alias_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_serial_alias_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_serial_alias_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_serial_alias_lifecycle_test)

  add_executable(mylite_runtime_integer_default_literals_test
    tests/runtime_integer_default_literals_test.c
  )
  target_link_libraries(mylite_runtime_integer_default_literals_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_integer_default_literals_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_integer_default_literals_test)

  add_executable(mylite_runtime_generated_column_lifecycle_test
    tests/runtime_generated_column_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_generated_column_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_generated_column_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_generated_column_lifecycle_test)

  add_executable(mylite_runtime_insert_set_lifecycle_test
    tests/runtime_insert_set_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_insert_set_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_insert_set_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_insert_set_lifecycle_test)

  add_executable(mylite_runtime_insert_unix_timestamp_arithmetic_test
    tests/runtime_insert_unix_timestamp_arithmetic_test.c
  )
  target_link_libraries(mylite_runtime_insert_unix_timestamp_arithmetic_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_insert_unix_timestamp_arithmetic_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_insert_unix_timestamp_arithmetic_test)

  add_executable(mylite_runtime_insert_select_lifecycle_test
    tests/runtime_insert_select_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_insert_select_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_insert_select_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_insert_select_lifecycle_test)

  add_executable(mylite_runtime_insert_select_union_source_test
    tests/runtime_insert_select_union_source_test.c
  )
  target_link_libraries(mylite_runtime_insert_select_union_source_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_insert_select_union_source_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_insert_select_union_source_test)

  add_executable(mylite_runtime_insert_select_on_duplicate_key_update_test
    tests/runtime_insert_select_on_duplicate_key_update_test.c
  )
  target_link_libraries(mylite_runtime_insert_select_on_duplicate_key_update_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_insert_select_on_duplicate_key_update_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_insert_select_on_duplicate_key_update_test)

  add_executable(mylite_runtime_values_function_non_odku_test
    tests/runtime_values_function_non_odku_test.c
  )
  target_link_libraries(mylite_runtime_values_function_non_odku_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_values_function_non_odku_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_values_function_non_odku_test)

  add_executable(mylite_runtime_truncate_table_lifecycle_test
    tests/runtime_truncate_table_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_truncate_table_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_truncate_table_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_truncate_table_lifecycle_test)

  add_executable(mylite_runtime_alter_table_add_column_lifecycle_test
    tests/runtime_alter_table_add_column_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_add_column_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_add_column_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_add_column_lifecycle_test)

  add_executable(mylite_runtime_alter_table_drop_column_lifecycle_test
    tests/runtime_alter_table_drop_column_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_drop_column_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_drop_column_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_drop_column_lifecycle_test)

  add_executable(mylite_runtime_alter_table_rename_column_lifecycle_test
    tests/runtime_alter_table_rename_column_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_rename_column_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_rename_column_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_rename_column_lifecycle_test)

  add_executable(mylite_runtime_alter_table_modify_column_lifecycle_test
    tests/runtime_alter_table_modify_column_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_modify_column_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_modify_column_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_modify_column_lifecycle_test)

  add_executable(mylite_runtime_alter_table_change_column_lifecycle_test
    tests/runtime_alter_table_change_column_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_change_column_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_change_column_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_change_column_lifecycle_test)

  add_executable(mylite_runtime_sqlite_owner_test
    tests/runtime_sqlite_owner_test.c
  )
  target_link_libraries(mylite_runtime_sqlite_owner_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_sqlite_owner_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sqlite_owner_test)

  add_executable(mylite_runtime_diagnostics_test
    tests/runtime_diagnostics_test.c
  )
  target_link_libraries(mylite_runtime_diagnostics_test PRIVATE MyLite::mylite)
  target_include_directories(mylite_runtime_diagnostics_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_diagnostics_test)

  add_executable(mylite_runtime_statement_context_test
    tests/runtime_statement_context_test.c
  )
  target_link_libraries(mylite_runtime_statement_context_test PRIVATE MyLite::mylite)
  target_include_directories(mylite_runtime_statement_context_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_statement_context_test)

  add_executable(mylite_runtime_result_metadata_test
    tests/runtime_result_metadata_test.c
  )
  target_link_libraries(mylite_runtime_result_metadata_test PRIVATE MyLite::mylite)
  target_include_directories(mylite_runtime_result_metadata_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_result_metadata_test)

  add_executable(mylite_runtime_result_column_metadata_test
    tests/runtime_result_column_metadata_test.c
  )
  target_link_libraries(mylite_runtime_result_column_metadata_test PRIVATE MyLite::mylite)
  target_include_directories(mylite_runtime_result_column_metadata_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_result_column_metadata_test)

  add_executable(mylite_runtime_sqlite_bootstrap_test
    tests/runtime_sqlite_bootstrap_test.c
  )
  target_link_libraries(mylite_runtime_sqlite_bootstrap_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_sqlite_bootstrap_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sqlite_bootstrap_test)
