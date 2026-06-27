  add_test(NAME libmylite.runtime.diagnostics COMMAND mylite_runtime_diagnostics_test)
  add_test(
    NAME libmylite.runtime.catalog_foundation
    COMMAND mylite_runtime_catalog_foundation_test
  )
  add_test(
    NAME libmylite.runtime.basic_table_lifecycle
    COMMAND mylite_runtime_basic_table_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.table_if_exists_lifecycle
    COMMAND mylite_runtime_table_if_exists_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.schema_lifecycle
    COMMAND mylite_runtime_schema_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.current_database_function
    COMMAND mylite_runtime_current_database_function_test
  )
  add_test(
    NAME libmylite.runtime.current_user_identity
    COMMAND mylite_runtime_current_user_identity_test
  )
  add_test(
    NAME libmylite.runtime.connection_id_function
    COMMAND mylite_runtime_connection_id_function_test
  )
  add_test(
    NAME libmylite.runtime.version_function
    COMMAND mylite_runtime_version_function_test
  )
  add_test(
    NAME libmylite.runtime.named_lock_and_info_functions
    COMMAND mylite_runtime_named_lock_and_info_functions_test
  )
  add_test(
    NAME libmylite.runtime.system_function_residuals
    COMMAND mylite_runtime_system_function_residuals_test
  )
  add_test(
    NAME libmylite.runtime.replication_functions
    COMMAND mylite_runtime_replication_functions_test
  )
  add_test(
    NAME libmylite.runtime.sys_helper_functions
    COMMAND mylite_runtime_sys_helper_functions_test
  )
  add_test(
    NAME libmylite.runtime.sys_performance_schema_helper_functions
    COMMAND mylite_runtime_sys_performance_schema_helper_functions_test
  )
  add_test(
    NAME libmylite.runtime.sys_procedure_placeholders
    COMMAND mylite_runtime_sys_procedure_placeholders_test
  )
  add_test(
    NAME libmylite.runtime.if_function
    COMMAND mylite_runtime_if_function_test
  )
  add_test(
    NAME libmylite.runtime.ifnull_function
    COMMAND mylite_runtime_ifnull_function_test
  )
  add_test(
    NAME libmylite.runtime.coalesce_function
    COMMAND mylite_runtime_coalesce_function_test
  )
  add_test(
    NAME libmylite.runtime.nullif_function
    COMMAND mylite_runtime_nullif_function_test
  )
  add_test(
    NAME libmylite.runtime.isnull_function
    COMMAND mylite_runtime_isnull_function_test
  )
  add_test(
    NAME libmylite.runtime.case_operator
    COMMAND mylite_runtime_case_operator_test
  )
  add_test(
    NAME libmylite.runtime.do_statement
    COMMAND mylite_runtime_do_statement_test
  )
  add_test(
    NAME libmylite.runtime.scalar_expression_projection
    COMMAND mylite_runtime_scalar_expression_projection_test
  )
  add_test(
    NAME libmylite.runtime.row_cast_convert_projection
    COMMAND mylite_runtime_row_cast_convert_projection_test
  )
  add_test(
    NAME libmylite.runtime.optimizer_hints_noop
    COMMAND mylite_runtime_optimizer_hints_noop_test
  )
  add_test(
    NAME libmylite.runtime.explain_statement
    COMMAND mylite_runtime_explain_statement_test
  )
  add_test(
    NAME libmylite.runtime.create_table_partition_options
    COMMAND mylite_runtime_create_table_partition_options_test
  )
  add_test(
    NAME libmylite.runtime.table_partition_selection
    COMMAND mylite_runtime_table_partition_selection_test
  )
  add_test(
    NAME libmylite.runtime.scalar_arithmetic_projection
    COMMAND mylite_runtime_scalar_arithmetic_projection_test
  )
  add_test(
    NAME libmylite.runtime.scalar_modulo_projection
    COMMAND mylite_runtime_scalar_modulo_projection_test
  )
  add_test(
    NAME libmylite.runtime.scalar_div_projection
    COMMAND mylite_runtime_scalar_div_projection_test
  )
  add_test(
    NAME libmylite.runtime.scalar_division_projection
    COMMAND mylite_runtime_scalar_division_projection_test
  )
  add_test(
    NAME libmylite.runtime.scalar_bitwise_projection
    COMMAND mylite_runtime_scalar_bitwise_projection_test
  )
  add_test(
    NAME libmylite.runtime.row_bitwise_expressions
    COMMAND mylite_runtime_row_bitwise_expressions_test
  )
  add_test(
    NAME libmylite.runtime.abs_function
    COMMAND mylite_runtime_abs_function_test
  )
  add_test(
    NAME libmylite.runtime.sign_function
    COMMAND mylite_runtime_sign_function_test
  )
  add_test(
    NAME libmylite.runtime.ceil_floor_functions
    COMMAND mylite_runtime_ceil_floor_functions_test
  )
  add_test(
    NAME libmylite.runtime.round_function
    COMMAND mylite_runtime_round_function_test
  )
  add_test(
    NAME libmylite.runtime.bin_oct_functions
    COMMAND mylite_runtime_bin_oct_functions_test
  )
  add_test(
    NAME libmylite.runtime.conv_function
    COMMAND mylite_runtime_conv_function_test
  )
  add_test(
    NAME libmylite.runtime.pi_function
    COMMAND mylite_runtime_pi_function_test
  )
  add_test(
    NAME libmylite.runtime.rand_function
    COMMAND mylite_runtime_rand_function_test
  )
  add_test(
    NAME libmylite.runtime.sqrt_function
    COMMAND mylite_runtime_sqrt_function_test
  )
  add_test(
    NAME libmylite.runtime.degrees_radians_functions
    COMMAND mylite_runtime_degrees_radians_functions_test
  )
  add_test(
    NAME libmylite.runtime.acos_asin_functions
    COMMAND mylite_runtime_acos_asin_functions_test
  )
  add_test(
    NAME libmylite.runtime.atan_functions
    COMMAND mylite_runtime_atan_functions_test
  )
  add_test(
    NAME libmylite.runtime.trigonometric_functions
    COMMAND mylite_runtime_trigonometric_functions_test
  )
  add_test(
    NAME libmylite.runtime.exp_log_power_functions
    COMMAND mylite_runtime_exp_log_power_functions_test
  )
  add_test(
    NAME libmylite.runtime.bit_count_function
    COMMAND mylite_runtime_bit_count_function_test
  )
  add_test(
    NAME libmylite.runtime.row_scalar_numeric_functions
    COMMAND mylite_runtime_row_scalar_numeric_functions_test
  )
  add_test(
    NAME libmylite.runtime.numeric_format_truncate_crc32
    COMMAND mylite_runtime_numeric_format_truncate_crc32_test
  )
  add_test(
    NAME libmylite.runtime.row_scalar_numeric_comparison_update_contexts
    COMMAND mylite_runtime_row_scalar_numeric_comparison_update_contexts_test
  )
  add_test(
    NAME libmylite.runtime.hex_function
    COMMAND mylite_runtime_hex_function_test
  )
  add_test(
    NAME libmylite.runtime.base64_functions
    COMMAND mylite_runtime_base64_functions_test
  )
  add_test(
    NAME libmylite.runtime.compression_random_functions
    COMMAND mylite_runtime_compression_random_functions_test
  )
  add_test(
    NAME libmylite.runtime.ip_address_functions
    COMMAND mylite_runtime_ip_address_functions_test
  )
  add_test(
    NAME libmylite.runtime.digest_functions
    COMMAND mylite_runtime_digest_functions_test
  )
  add_test(
    NAME libmylite.runtime.validate_password_strength_function
    COMMAND mylite_runtime_validate_password_strength_function_test
  )
  add_test(
    NAME libmylite.runtime.unhex_function
    COMMAND mylite_runtime_unhex_function_test
  )
  add_test(
    NAME libmylite.runtime.uuid_conversion_functions
    COMMAND mylite_runtime_uuid_conversion_functions_test
  )
  add_test(
    NAME libmylite.runtime.uuid_function
    COMMAND mylite_runtime_uuid_function_test
  )
  add_test(
    NAME libmylite.runtime.char_function
    COMMAND mylite_runtime_char_function_test
  )
  add_test(
    NAME libmylite.runtime.default_function
    COMMAND mylite_runtime_default_function_test
  )
  add_test(
    NAME libmylite.runtime.charset_collation_functions
    COMMAND mylite_runtime_charset_collation_functions_test
  )
  add_test(
    NAME libmylite.runtime.coercibility_function
    COMMAND mylite_runtime_coercibility_function_test
  )
  add_test(
    NAME libmylite.runtime.string_length_functions
    COMMAND mylite_runtime_string_length_functions_test
  )
  add_test(
    NAME libmylite.runtime.ascii_ord_functions
    COMMAND mylite_runtime_ascii_ord_functions_test
  )
  add_test(
    NAME libmylite.runtime.string_case_functions
    COMMAND mylite_runtime_string_case_functions_test
  )
  add_test(
    NAME libmylite.runtime.concat_ws_function
    COMMAND mylite_runtime_concat_ws_function_test
  )
  add_test(
    NAME libmylite.runtime.pipes_as_concat
    COMMAND mylite_runtime_pipes_as_concat_test
  )
  add_test(
    NAME libmylite.runtime.replace_string_function
    COMMAND mylite_runtime_replace_string_function_test
  )
  add_test(
    NAME libmylite.runtime.insert_string_function
    COMMAND mylite_runtime_insert_string_function_test
  )
  add_test(
    NAME libmylite.runtime.reverse_function
    COMMAND mylite_runtime_reverse_function_test
  )
  add_test(
    NAME libmylite.runtime.soundex_function
    COMMAND mylite_runtime_soundex_function_test
  )
  add_test(
    NAME libmylite.runtime.quote_function
    COMMAND mylite_runtime_quote_function_test
  )
  add_test(
    NAME libmylite.runtime.trim_string_functions
    COMMAND mylite_runtime_trim_string_functions_test
  )
  add_test(
    NAME libmylite.runtime.string_slice_functions
    COMMAND mylite_runtime_string_slice_functions_test
  )
  add_test(
    NAME libmylite.runtime.string_padding_functions
    COMMAND mylite_runtime_string_padding_functions_test
  )
  add_test(
    NAME libmylite.runtime.string_bitmask_functions
    COMMAND mylite_runtime_string_bitmask_functions_test
  )
  add_test(
    NAME libmylite.runtime.string_search_functions
    COMMAND mylite_runtime_string_search_functions_test
  )
  add_test(
    NAME libmylite.runtime.string_row_scalar_update_contexts
    COMMAND mylite_runtime_string_row_scalar_update_contexts_test
  )
  add_test(
    NAME libmylite.runtime.temporal_row_scalar_update_contexts
    COMMAND mylite_runtime_temporal_row_scalar_update_contexts_test
  )
  add_test(
    NAME libmylite.runtime.date_add_second
    COMMAND mylite_runtime_date_add_second_test
  )
  add_test(
    NAME libmylite.runtime.timestampadd_second_function
    COMMAND mylite_runtime_timestampadd_second_function_test
  )
  add_test(
    NAME libmylite.runtime.addtime_subtime_functions
    COMMAND mylite_runtime_addtime_subtime_functions_test
  )
  add_test(
    NAME libmylite.runtime.row_temporal_interval_second_projection
    COMMAND mylite_runtime_row_temporal_interval_second_projection_test
  )
  add_test(
    NAME libmylite.runtime.date_format_function
    COMMAND mylite_runtime_date_format_function_test
  )
  add_test(
    NAME libmylite.runtime.get_format_function
    COMMAND mylite_runtime_get_format_function_test
  )
  add_test(
    NAME libmylite.runtime.time_format_function
    COMMAND mylite_runtime_time_format_function_test
  )
  add_test(
    NAME libmylite.runtime.str_to_date_function
    COMMAND mylite_runtime_str_to_date_function_test
  )
  add_test(
    NAME libmylite.runtime.datediff_function
    COMMAND mylite_runtime_datediff_function_test
  )
  add_test(
    NAME libmylite.runtime.timestampdiff_function
    COMMAND mylite_runtime_timestampdiff_function_test
  )
  add_test(
    NAME libmylite.runtime.timestamp_function
    COMMAND mylite_runtime_timestamp_function_test
  )
  add_test(
    NAME libmylite.runtime.timediff_function
    COMMAND mylite_runtime_timediff_function_test
  )
  add_test(
    NAME libmylite.runtime.calendar_date_functions
    COMMAND mylite_runtime_calendar_date_functions_test
  )
  add_test(
    NAME libmylite.runtime.day_month_name_functions
    COMMAND mylite_runtime_day_month_name_functions_test
  )
  add_test(
    NAME libmylite.runtime.week_temporal_functions
    COMMAND mylite_runtime_week_temporal_functions_test
  )
  add_test(
    NAME libmylite.runtime.unix_timestamp_function
    COMMAND mylite_runtime_unix_timestamp_function_test
  )
  add_test(
    NAME libmylite.runtime.from_unixtime_function
    COMMAND mylite_runtime_from_unixtime_function_test
  )
  add_test(
    NAME libmylite.runtime.time_second_conversion_functions
    COMMAND mylite_runtime_time_second_conversion_functions_test
  )
  add_test(
    NAME libmylite.runtime.to_days_to_seconds_functions
    COMMAND mylite_runtime_to_days_to_seconds_functions_test
  )
  add_test(
    NAME libmylite.runtime.temporal_constructor_functions
    COMMAND mylite_runtime_temporal_constructor_functions_test
  )
  add_test(
    NAME libmylite.runtime.scalar_period_timezone_weight_functions
    COMMAND mylite_runtime_scalar_period_timezone_weight_functions_test
  )

  add_test(
    NAME libmylite.runtime.temporal_extract_functions
    COMMAND mylite_runtime_temporal_extract_functions_test
  )
  add_test(
    NAME libmylite.runtime.elt_function
    COMMAND mylite_runtime_elt_function_test
  )
  add_test(
    NAME libmylite.runtime.field_function
    COMMAND mylite_runtime_field_function_test
  )
  add_test(
    NAME libmylite.runtime.order_by_field_function
    COMMAND mylite_runtime_order_by_field_function_test
  )
  add_test(
    NAME libmylite.runtime.greatest_least_functions
    COMMAND mylite_runtime_greatest_least_functions_test
  )
  add_test(
    NAME libmylite.runtime.interval_function
    COMMAND mylite_runtime_interval_function_test
  )
  add_test(
    NAME libmylite.runtime.json_valid_function
    COMMAND mylite_runtime_json_valid_function_test
  )
  add_test(
    NAME libmylite.runtime.json_extract_functions
    COMMAND mylite_runtime_json_extract_functions_test
  )
  add_test(
    NAME libmylite.runtime.json_value_function
    COMMAND mylite_runtime_json_value_function_test
  )
  add_test(
    NAME libmylite.runtime.json_quote_function
    COMMAND mylite_runtime_json_quote_function_test
  )
  add_test(
    NAME libmylite.runtime.json_contains_functions
    COMMAND mylite_runtime_json_contains_functions_test
  )
  add_test(
    NAME libmylite.runtime.json_search_function
    COMMAND mylite_runtime_json_search_function_test
  )
  add_test(
    NAME libmylite.runtime.json_overlaps_member_functions
    COMMAND mylite_runtime_json_overlaps_member_functions_test
  )
  add_test(
    NAME libmylite.runtime.json_construction_functions
    COMMAND mylite_runtime_json_construction_functions_test
  )
  add_test(
    NAME libmylite.runtime.json_set_function
    COMMAND mylite_runtime_json_set_function_test
  )
  add_test(
    NAME libmylite.runtime.json_insert_function
    COMMAND mylite_runtime_json_insert_function_test
  )
  add_test(
    NAME libmylite.runtime.json_array_mutation_functions
    COMMAND mylite_runtime_json_array_mutation_functions_test
  )
  add_test(
    NAME libmylite.runtime.json_merge_functions
    COMMAND mylite_runtime_json_merge_functions_test
  )
  add_test(
    NAME libmylite.runtime.json_replace_function
    COMMAND mylite_runtime_json_replace_function_test
  )
  add_test(
    NAME libmylite.runtime.json_remove_function
    COMMAND mylite_runtime_json_remove_function_test
  )
  add_test(
    NAME libmylite.runtime.json_introspection_functions
    COMMAND mylite_runtime_json_introspection_functions_test
  )
  add_test(
    NAME libmylite.runtime.json_row_scalar_contexts
    COMMAND mylite_runtime_json_row_scalar_contexts_test
  )
  add_test(
    NAME libmylite.runtime.scalar_comparison_projection
    COMMAND mylite_runtime_scalar_comparison_projection_test
  )
  add_test(
    NAME libmylite.runtime.scalar_logical_projection
    COMMAND mylite_runtime_scalar_logical_projection_test
  )
  add_test(
    NAME libmylite.runtime.scalar_is_projection
    COMMAND mylite_runtime_scalar_is_projection_test
  )
  add_test(
    NAME libmylite.runtime.session_value_scalar_projection
    COMMAND mylite_runtime_session_value_scalar_projection_test
  )
  add_test(
    NAME libmylite.runtime.row_count_function
    COMMAND mylite_runtime_row_count_function_test
  )
  add_test(
    NAME libmylite.runtime.found_rows_function
    COMMAND mylite_runtime_found_rows_function_test
  )
  add_test(
    NAME libmylite.runtime.select_noop_modifiers
    COMMAND mylite_runtime_select_noop_modifiers_test
  )
  add_test(
    NAME libmylite.runtime.select_locking_clauses
    COMMAND mylite_runtime_select_locking_clauses_test
  )
  add_test(
    NAME libmylite.runtime.last_insert_id_function
    COMMAND mylite_runtime_last_insert_id_function_test
  )
  add_test(
    NAME libmylite.runtime.count_aggregate
    COMMAND mylite_runtime_count_aggregate_test
  )
  add_test(
    NAME libmylite.runtime.min_max_aggregate
    COMMAND mylite_runtime_min_max_aggregate_test
  )
  add_test(
    NAME libmylite.runtime.sum_aggregate
    COMMAND mylite_runtime_sum_aggregate_test
  )
  add_test(
    NAME libmylite.runtime.avg_aggregate
    COMMAND mylite_runtime_avg_aggregate_test
  )
  add_test(
    NAME libmylite.runtime.multi_aggregate_select
    COMMAND mylite_runtime_multi_aggregate_select_test
  )
  add_test(
    NAME libmylite.runtime.statistical_aggregates
    COMMAND mylite_runtime_statistical_aggregates_test
  )
  add_test(
    NAME libmylite.runtime.bitwise_aggregates
    COMMAND mylite_runtime_bitwise_aggregates_test
  )
  add_test(
    NAME libmylite.runtime.group_concat_aggregate
    COMMAND mylite_runtime_group_concat_aggregate_test
  )
  add_test(
    NAME libmylite.runtime.group_concat_max_len_system_variable
    COMMAND mylite_runtime_group_concat_max_len_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.big_tables_system_variable
    COMMAND mylite_runtime_big_tables_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_stats_expiry_system_variable
    COMMAND mylite_runtime_information_schema_stats_expiry_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.session_tracking_system_variables
    COMMAND mylite_runtime_session_tracking_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.server_security_system_variables
    COMMAND mylite_runtime_server_security_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.global_resource_system_variables
    COMMAND mylite_runtime_global_resource_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.internal_session_system_variables
    COMMAND mylite_runtime_internal_session_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.resource_tuning_system_variables
    COMMAND mylite_runtime_resource_tuning_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.server_tls_system_variables
    COMMAND mylite_runtime_server_tls_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.server_logging_system_variables
    COMMAND mylite_runtime_server_logging_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.session_tuning_system_variables
    COMMAND mylite_runtime_session_tuning_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.jl_system_variables
    COMMAND mylite_runtime_jl_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.m_server_limit_system_variables
    COMMAND mylite_runtime_m_server_limit_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.m_session_limit_system_variables
    COMMAND mylite_runtime_m_session_limit_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.o_optimizer_system_variables
    COMMAND mylite_runtime_o_optimizer_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.replication_global_system_variables
    COMMAND mylite_runtime_replication_global_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.replica_applier_system_variables
    COMMAND mylite_runtime_replica_applier_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.myisam_system_variables
    COMMAND mylite_runtime_myisam_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.innodb_core_system_variables
    COMMAND mylite_runtime_innodb_core_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.innodb_storage_system_variables
    COMMAND mylite_runtime_innodb_storage_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.innodb_file_flush_system_variables
    COMMAND mylite_runtime_innodb_file_flush_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.innodb_fulltext_system_variables
    COMMAND mylite_runtime_innodb_fulltext_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.innodb_io_log_system_variables
    COMMAND mylite_runtime_innodb_io_log_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.innodb_dirty_purge_system_variables
    COMMAND mylite_runtime_innodb_dirty_purge_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.innodb_monitor_system_variables
    COMMAND mylite_runtime_innodb_monitor_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.innodb_page_read_purge_system_variables
    COMMAND mylite_runtime_innodb_page_read_purge_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.innodb_redo_rollback_spin_system_variables
    COMMAND mylite_runtime_innodb_redo_rollback_spin_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.innodb_stats_status_thread_undo_system_variables
    COMMAND mylite_runtime_innodb_stats_status_thread_undo_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.performance_schema_system_variables
    COMMAND mylite_runtime_performance_schema_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.performance_schema_variable_status_tables
    COMMAND mylite_runtime_performance_schema_variable_status_tables_test
  )
  add_test(
    NAME libmylite.runtime.performance_schema_performance_timers
    COMMAND mylite_runtime_performance_schema_performance_timers_test
  )
  add_test(
    NAME libmylite.runtime.performance_schema_setup_actor_consumer_tables
    COMMAND mylite_runtime_performance_schema_setup_actor_consumer_tables_test
  )
  add_test(
    NAME libmylite.runtime.performance_schema_setup_logger_meter_tables
    COMMAND mylite_runtime_performance_schema_setup_logger_meter_tables_test
  )
  add_test(
    NAME libmylite.runtime.performance_schema_setup_metrics_table
    COMMAND mylite_runtime_performance_schema_setup_metrics_table_test
  )
  add_test(
    NAME libmylite.runtime.performance_schema_setup_objects
    COMMAND mylite_runtime_performance_schema_setup_objects_test
  )
  add_test(
    NAME libmylite.runtime.performance_schema_setup_threads
    COMMAND mylite_runtime_performance_schema_setup_threads_test
  )
  add_test(
    NAME libmylite.runtime.performance_schema_table_handles_table
    COMMAND mylite_runtime_performance_schema_table_handles_table_test
  )
  add_test(
    NAME libmylite.runtime.performance_schema_user_defined_functions
    COMMAND mylite_runtime_performance_schema_user_defined_functions_test
  )
  add_test(
    NAME libmylite.runtime.performance_schema_user_variables_by_thread
    COMMAND mylite_runtime_performance_schema_user_variables_by_thread_test
  )
  add_test(
    NAME libmylite.runtime.network_timeout_system_variables
    COMMAND mylite_runtime_network_timeout_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.connection_system_variables
    COMMAND mylite_runtime_connection_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.binary_log_system_variables
    COMMAND mylite_runtime_binary_log_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.bootstrap_system_variables
    COMMAND mylite_runtime_bootstrap_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.compatibility_system_variables
    COMMAND mylite_runtime_compatibility_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.authentication_password_system_variables
    COMMAND mylite_runtime_authentication_password_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.server_capability_system_variables
    COMMAND mylite_runtime_server_capability_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.admin_listener_system_variables
    COMMAND mylite_runtime_admin_listener_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.mysqlx_system_variables
    COMMAND mylite_runtime_mysqlx_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.group_by_single_column_aggregate
    COMMAND mylite_runtime_group_by_single_column_aggregate_test
  )
  add_test(
    NAME libmylite.runtime.any_value_function
    COMMAND mylite_runtime_any_value_function_test
  )
  add_test(
    NAME libmylite.runtime.group_by_primary_key_projection
    COMMAND mylite_runtime_group_by_primary_key_projection_test
  )
  add_test(
    NAME libmylite.runtime.joined_aggregate_select
    COMMAND mylite_runtime_joined_aggregate_select_test
  )
  add_test(
    NAME libmylite.runtime.show_columns_introspection
    COMMAND mylite_runtime_show_columns_introspection_test
  )
  add_test(
    NAME libmylite.runtime.show_full_columns_introspection
    COMMAND mylite_runtime_show_full_columns_introspection_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_core
    COMMAND mylite_runtime_information_schema_core_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_conditional_table_absence
    COMMAND mylite_runtime_information_schema_conditional_table_absence_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_schemata_extensions
    COMMAND mylite_runtime_information_schema_schemata_extensions_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_extension_attribute_tables
    COMMAND mylite_runtime_information_schema_extension_attribute_tables_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_tablespaces_extensions
    COMMAND mylite_runtime_information_schema_tablespaces_extensions_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_cmp
    COMMAND mylite_runtime_information_schema_innodb_cmp_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_cmpmem
    COMMAND mylite_runtime_information_schema_innodb_cmpmem_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_foreign
    COMMAND mylite_runtime_information_schema_innodb_foreign_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_indexes
    COMMAND mylite_runtime_information_schema_innodb_indexes_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_columns
    COMMAND mylite_runtime_information_schema_innodb_columns_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_tables
    COMMAND mylite_runtime_information_schema_innodb_tables_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_tablestats
    COMMAND mylite_runtime_information_schema_innodb_tablestats_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_mysql_system_statistics
    COMMAND mylite_runtime_information_schema_mysql_system_statistics_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_mysql_system_constraints
    COMMAND mylite_runtime_information_schema_mysql_system_constraints_test
  )
  add_test(
    NAME libmylite.runtime.mysql_component_table
    COMMAND mylite_runtime_mysql_component_table_test
  )
  add_test(
    NAME libmylite.runtime.mysql_cost_tables
    COMMAND mylite_runtime_mysql_cost_tables_test
  )
  add_test(
    NAME libmylite.runtime.mysql_func_table
    COMMAND mylite_runtime_mysql_func_table_test
  )
  add_test(
    NAME libmylite.runtime.mysql_gtid_executed_table
    COMMAND mylite_runtime_mysql_gtid_executed_table_test
  )
  add_test(
    NAME libmylite.runtime.mysql_ndb_binlog_index
    COMMAND mylite_runtime_mysql_ndb_binlog_index_test
  )
  add_test(
    NAME libmylite.runtime.mysql_replication_metadata_tables
    COMMAND mylite_runtime_mysql_replication_metadata_tables_test
  )
  add_test(
    NAME libmylite.runtime.mysql_log_tables
    COMMAND mylite_runtime_mysql_log_tables_test
  )
  add_test(
    NAME libmylite.runtime.mysql_user_table
    COMMAND mylite_runtime_mysql_user_table_test
  )
  add_test(
    NAME libmylite.runtime.mysql_global_grants_table
    COMMAND mylite_runtime_mysql_global_grants_table_test
  )
  add_test(
    NAME libmylite.runtime.mysql_db_table
    COMMAND mylite_runtime_mysql_db_table_test
  )
  add_test(
    NAME libmylite.runtime.mysql_tables_priv_table
    COMMAND mylite_runtime_mysql_tables_priv_table_test
  )
  add_test(
    NAME libmylite.runtime.mysql_columns_priv_table
    COMMAND mylite_runtime_mysql_columns_priv_table_test
  )
  add_test(
    NAME libmylite.runtime.mysql_procs_priv_table
    COMMAND mylite_runtime_mysql_procs_priv_table_test
  )
  add_test(
    NAME libmylite.runtime.mysql_proxies_priv_table
    COMMAND mylite_runtime_mysql_proxies_priv_table_test
  )
  add_test(
    NAME libmylite.runtime.mysql_default_roles_table
    COMMAND mylite_runtime_mysql_default_roles_table_test
  )
  add_test(
    NAME libmylite.runtime.mysql_role_edges_table
    COMMAND mylite_runtime_mysql_role_edges_table_test
  )
  add_test(
    NAME libmylite.runtime.mysql_password_history_table
    COMMAND mylite_runtime_mysql_password_history_table_test
  )
  add_test(
    NAME libmylite.runtime.mysql_help_tables
    COMMAND mylite_runtime_mysql_help_tables_test
  )
  add_test(
    NAME libmylite.runtime.mysql_plugin_table
    COMMAND mylite_runtime_mysql_plugin_table_test
  )
  add_test(
    NAME libmylite.runtime.mysql_servers_table
    COMMAND mylite_runtime_mysql_servers_table_test
  )
  add_test(
    NAME libmylite.runtime.mysql_time_zone_tables
    COMMAND mylite_runtime_mysql_time_zone_tables_test
  )
  add_test(
    NAME libmylite.runtime.sys_sys_config_table
    COMMAND mylite_runtime_sys_sys_config_table_test
  )
  add_test(
    NAME libmylite.runtime.sys_sys_config_triggers
    COMMAND mylite_runtime_sys_sys_config_triggers_test
  )
  add_test(
    NAME libmylite.runtime.sys_version_view
    COMMAND mylite_runtime_sys_version_view_test
  )
  add_test(
    NAME libmylite.runtime.sys_host_summary_views
    COMMAND mylite_runtime_sys_host_summary_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_host_summary_by_file_io_views
    COMMAND mylite_runtime_sys_host_summary_by_file_io_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_host_summary_by_file_io_type_views
    COMMAND mylite_runtime_sys_host_summary_by_file_io_type_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_host_summary_by_stages_views
    COMMAND mylite_runtime_sys_host_summary_by_stages_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_host_summary_by_statement_latency_views
    COMMAND mylite_runtime_sys_host_summary_by_statement_latency_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_host_summary_by_statement_type_views
    COMMAND mylite_runtime_sys_host_summary_by_statement_type_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_user_summary_views
    COMMAND mylite_runtime_sys_user_summary_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_wait_views
    COMMAND mylite_runtime_sys_wait_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_innodb_buffer_stats_by_schema_views
    COMMAND mylite_runtime_sys_innodb_buffer_stats_by_schema_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_innodb_buffer_stats_by_table_views
    COMMAND mylite_runtime_sys_innodb_buffer_stats_by_table_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_innodb_lock_waits_views
    COMMAND mylite_runtime_sys_innodb_lock_waits_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_latest_file_io_views
    COMMAND mylite_runtime_sys_latest_file_io_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_memory_by_host_by_current_bytes_views
    COMMAND mylite_runtime_sys_memory_by_host_by_current_bytes_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_memory_by_thread_by_current_bytes_views
    COMMAND mylite_runtime_sys_memory_by_thread_by_current_bytes_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_memory_by_user_by_current_bytes_views
    COMMAND mylite_runtime_sys_memory_by_user_by_current_bytes_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_memory_global_by_current_bytes_views
    COMMAND mylite_runtime_sys_memory_global_by_current_bytes_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_memory_global_total_views
    COMMAND mylite_runtime_sys_memory_global_total_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_metrics_view
    COMMAND mylite_runtime_sys_metrics_view_test
  )
  add_test(
    NAME libmylite.runtime.sys_processlist_views
    COMMAND mylite_runtime_sys_processlist_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_session_views
    COMMAND mylite_runtime_sys_session_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_ps_digest_helper_views
    COMMAND mylite_runtime_sys_ps_digest_helper_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_statement_digest_views
    COMMAND mylite_runtime_sys_statement_digest_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_statement_sort_temp_views
    COMMAND mylite_runtime_sys_statement_sort_temp_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_io_by_thread_by_latency_views
    COMMAND mylite_runtime_sys_io_by_thread_by_latency_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_io_global_by_file_by_bytes_views
    COMMAND mylite_runtime_sys_io_global_by_file_by_bytes_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_io_global_by_file_by_latency_views
    COMMAND mylite_runtime_sys_io_global_by_file_by_latency_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_io_global_by_wait_by_bytes_views
    COMMAND mylite_runtime_sys_io_global_by_wait_by_bytes_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_io_global_by_wait_by_latency_views
    COMMAND mylite_runtime_sys_io_global_by_wait_by_latency_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_ps_check_lost_instrumentation_view
    COMMAND mylite_runtime_sys_ps_check_lost_instrumentation_view_test
  )
  add_test(
    NAME libmylite.runtime.sys_schema_auto_increment_columns_view
    COMMAND mylite_runtime_sys_schema_auto_increment_columns_view_test
  )
  add_test(
    NAME libmylite.runtime.sys_schema_index_statistics_views
    COMMAND mylite_runtime_sys_schema_index_statistics_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_schema_redundant_indexes_views
    COMMAND mylite_runtime_sys_schema_redundant_indexes_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_schema_table_lock_waits_views
    COMMAND mylite_runtime_sys_schema_table_lock_waits_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_x_ps_schema_table_statistics_io_view
    COMMAND mylite_runtime_sys_x_ps_schema_table_statistics_io_view_test
  )
  add_test(
    NAME libmylite.runtime.sys_schema_table_statistics_views
    COMMAND mylite_runtime_sys_schema_table_statistics_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_schema_table_statistics_with_buffer_views
    COMMAND mylite_runtime_sys_schema_table_statistics_with_buffer_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_schema_tables_with_full_table_scans_views
    COMMAND mylite_runtime_sys_schema_tables_with_full_table_scans_views_test
  )
  add_test(
    NAME libmylite.runtime.sys_schema_unused_indexes_view
    COMMAND mylite_runtime_sys_schema_unused_indexes_view_test
  )
  add_test(
    NAME libmylite.runtime.sys_schema_object_overview_view
    COMMAND mylite_runtime_sys_schema_object_overview_view_test
  )
  add_test(
    NAME libmylite.runtime.mysql_enterprise_table_absence
    COMMAND mylite_runtime_mysql_enterprise_table_absence_test
  )
  add_test(
    NAME libmylite.runtime.mysql_data_dictionary_table_diagnostics
    COMMAND mylite_runtime_mysql_data_dictionary_table_diagnostics_test
  )
  add_test(
    NAME libmylite.runtime.mysql_system_stats_table_status
    COMMAND mylite_runtime_mysql_system_stats_table_status_test
  )
  add_test(
    NAME libmylite.runtime.mysql_innodb_index_stats
    COMMAND mylite_runtime_mysql_innodb_index_stats_test
  )
  add_test(
    NAME libmylite.runtime.mysql_innodb_table_stats
    COMMAND mylite_runtime_mysql_innodb_table_stats_test
  )
  add_test(
    NAME libmylite.runtime.mysql_system_show_columns
    COMMAND mylite_runtime_mysql_system_show_columns_test
  )
  add_test(
    NAME libmylite.runtime.mysql_system_show_index
    COMMAND mylite_runtime_mysql_system_show_index_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_session_temp_tablespaces
    COMMAND mylite_runtime_information_schema_innodb_session_temp_tablespaces_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_virtual
    COMMAND mylite_runtime_information_schema_innodb_virtual_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_trx
    COMMAND mylite_runtime_information_schema_innodb_trx_test
  )
