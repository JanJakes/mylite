  add_test(
    NAME libmylite.runtime.information_schema_innodb_cached_indexes
    COMMAND mylite_runtime_information_schema_innodb_cached_indexes_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_buffer_page_tables
    COMMAND mylite_runtime_information_schema_innodb_buffer_page_tables_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_buffer_pool_stats
    COMMAND mylite_runtime_information_schema_innodb_buffer_pool_stats_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_metrics
    COMMAND mylite_runtime_information_schema_innodb_metrics_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_cmp_per_index
    COMMAND mylite_runtime_information_schema_innodb_cmp_per_index_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_tablespace_metadata
    COMMAND mylite_runtime_information_schema_innodb_tablespace_metadata_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_files
    COMMAND mylite_runtime_information_schema_files_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_ft_config
    COMMAND mylite_runtime_information_schema_innodb_ft_config_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_ft_deleted
    COMMAND mylite_runtime_information_schema_innodb_ft_deleted_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_ft_index
    COMMAND mylite_runtime_information_schema_innodb_ft_index_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_ft_default_stopword
    COMMAND mylite_runtime_information_schema_innodb_ft_default_stopword_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_innodb_temp_table_info
    COMMAND mylite_runtime_information_schema_innodb_temp_table_info_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_column_statistics
    COMMAND mylite_runtime_information_schema_column_statistics_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_connection_control_failed_login_attempts
    COMMAND mylite_runtime_information_schema_connection_control_failed_login_attempts_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_predicates
    COMMAND mylite_runtime_information_schema_predicates_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_write_access
    COMMAND mylite_runtime_information_schema_write_access_test
  )
  add_test(
    NAME libmylite.runtime.builtin_schema_write_access
    COMMAND mylite_runtime_builtin_schema_write_access_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_constraints
    COMMAND mylite_runtime_information_schema_constraints_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_check_constraints
    COMMAND mylite_runtime_information_schema_check_constraints_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_events
    COMMAND mylite_runtime_information_schema_events_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_parameters
    COMMAND mylite_runtime_information_schema_parameters_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_partitions
    COMMAND mylite_runtime_information_schema_partitions_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_processlist
    COMMAND mylite_runtime_information_schema_processlist_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_privileges
    COMMAND mylite_runtime_information_schema_privileges_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_user_attributes
    COMMAND mylite_runtime_information_schema_user_attributes_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_role_grant_tables
    COMMAND mylite_runtime_information_schema_role_grant_tables_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_role_session_tables
    COMMAND mylite_runtime_information_schema_role_session_tables_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_optimizer_trace
    COMMAND mylite_runtime_information_schema_optimizer_trace_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_profiling
    COMMAND mylite_runtime_information_schema_profiling_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_resource_groups
    COMMAND mylite_runtime_information_schema_resource_groups_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_st_geometry_columns
    COMMAND mylite_runtime_information_schema_st_geometry_columns_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_st_spatial_reference_systems
    COMMAND mylite_runtime_information_schema_st_spatial_reference_systems_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_st_units_of_measure
    COMMAND mylite_runtime_information_schema_st_units_of_measure_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_routines
    COMMAND mylite_runtime_information_schema_routines_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_triggers
    COMMAND mylite_runtime_information_schema_triggers_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_views
    COMMAND mylite_runtime_information_schema_views_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_view_table_usage
    COMMAND mylite_runtime_information_schema_view_table_usage_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_view_routine_usage
    COMMAND mylite_runtime_information_schema_view_routine_usage_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_static_catalogs
    COMMAND mylite_runtime_information_schema_static_catalogs_test
  )
  add_test(
    NAME libmylite.runtime.builtin_schema_table_directory
    COMMAND mylite_runtime_builtin_schema_table_directory_test
  )
  add_test(
    NAME libmylite.runtime.show_plugins_metadata
    COMMAND mylite_runtime_show_plugins_metadata_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_keywords
    COMMAND mylite_runtime_information_schema_keywords_test
  )
  add_test(
    NAME libmylite.runtime.show_index_empty_introspection
    COMMAND mylite_runtime_show_index_empty_introspection_test
  )
  add_test(
    NAME libmylite.runtime.show_triggers_empty_introspection
    COMMAND mylite_runtime_show_triggers_empty_introspection_test
  )
  add_test(
    NAME libmylite.runtime.show_events_empty_introspection
    COMMAND mylite_runtime_show_events_empty_introspection_test
  )
  add_test(
    NAME libmylite.runtime.show_open_tables_empty_introspection
    COMMAND mylite_runtime_show_open_tables_empty_introspection_test
  )
  add_test(
    NAME libmylite.runtime.show_routine_status_empty_introspection
    COMMAND mylite_runtime_show_routine_status_empty_introspection_test
  )
  add_test(
    NAME libmylite.runtime.stored_procedure_select_call
    COMMAND mylite_runtime_stored_procedure_select_call_test
  )
  add_test(
    NAME libmylite.runtime.admin_stored_program_placeholders
    COMMAND mylite_runtime_admin_stored_program_placeholders_test
  )
  add_test(
    NAME libmylite.runtime.utility_diagnostics_placeholders
    COMMAND mylite_runtime_utility_diagnostics_placeholders_test
  )
  add_test(
    NAME libmylite.runtime.show_processlist_introspection
    COMMAND mylite_runtime_show_processlist_introspection_test
  )
  add_test(
    NAME libmylite.runtime.show_grants
    COMMAND mylite_runtime_show_grants_test
  )
  add_test(
    NAME libmylite.runtime.show_privileges
    COMMAND mylite_runtime_show_privileges_test
  )
  add_test(
    NAME libmylite.runtime.show_binary_log_metadata
    COMMAND mylite_runtime_show_binary_log_metadata_test
  )
  add_test(
    NAME libmylite.runtime.show_replica_metadata
    COMMAND mylite_runtime_show_replica_metadata_test
  )
  add_test(
    NAME libmylite.runtime.show_warnings_diagnostics
    COMMAND mylite_runtime_show_warnings_diagnostics_test
  )
  add_test(
    NAME libmylite.runtime.show_errors_diagnostics
    COMMAND mylite_runtime_show_errors_diagnostics_test
  )
  add_test(
    NAME libmylite.runtime.diagnostics_count_variables
    COMMAND mylite_runtime_diagnostics_count_variables_test
  )
  add_test(
    NAME libmylite.runtime.diagnostics_code_order
    COMMAND mylite_runtime_diagnostics_code_order_test
  )
  add_test(
    NAME libmylite.runtime.autocommit_system_variable
    COMMAND mylite_runtime_autocommit_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_quote_show_create_system_variable
    COMMAND mylite_runtime_sql_quote_show_create_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.foreign_key_checks_system_variable
    COMMAND mylite_runtime_foreign_key_checks_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.unique_checks_system_variable
    COMMAND mylite_runtime_unique_checks_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.updatable_views_with_limit_system_variable
    COMMAND mylite_runtime_updatable_views_with_limit_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_auto_is_null_system_variable
    COMMAND mylite_runtime_sql_auto_is_null_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_big_selects_system_variable
    COMMAND mylite_runtime_sql_big_selects_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_generate_invisible_primary_key_system_variable
    COMMAND mylite_runtime_sql_generate_invisible_primary_key_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.explicit_defaults_for_timestamp_system_variable
    COMMAND mylite_runtime_explicit_defaults_for_timestamp_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_buffer_result_system_variable
    COMMAND mylite_runtime_sql_buffer_result_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_log_bin_system_variable
    COMMAND mylite_runtime_sql_log_bin_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.server_identity_binary_log_system_variables
    COMMAND mylite_runtime_server_identity_binary_log_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.server_environment_system_variables
    COMMAND mylite_runtime_server_environment_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.server_build_system_variables
    COMMAND mylite_runtime_server_build_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.sql_log_off_system_variable
    COMMAND mylite_runtime_sql_log_off_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_mode_system_variable
    COMMAND mylite_runtime_sql_mode_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.time_zone_system_variable
    COMMAND mylite_runtime_time_zone_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_require_primary_key_system_variable
    COMMAND mylite_runtime_sql_require_primary_key_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_replica_skip_counter_system_variable
    COMMAND mylite_runtime_sql_replica_skip_counter_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_slave_skip_counter_system_variable
    COMMAND mylite_runtime_sql_slave_skip_counter_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_safe_updates_system_variable
    COMMAND mylite_runtime_sql_safe_updates_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_select_limit_system_variable
    COMMAND mylite_runtime_sql_select_limit_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.max_allowed_packet_system_variable
    COMMAND mylite_runtime_max_allowed_packet_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.timeout_system_variables
    COMMAND mylite_runtime_timeout_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.auto_increment_step_system_variables
    COMMAND mylite_runtime_auto_increment_step_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.sql_notes_system_variable
    COMMAND mylite_runtime_sql_notes_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_warnings_system_variable
    COMMAND mylite_runtime_sql_warnings_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.character_set_system_variables
    COMMAND mylite_runtime_character_set_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.set_connection_character_set
    COMMAND mylite_runtime_set_connection_character_set_test
  )
  add_test(
    NAME libmylite.runtime.set_fixed_system_variables
    COMMAND mylite_runtime_set_fixed_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.read_only_system_variables
    COMMAND mylite_runtime_read_only_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.user_variables
    COMMAND mylite_runtime_user_variables_test
  )
  add_test(
    NAME libmylite.runtime.select_into_user_variables
    COMMAND mylite_runtime_select_into_user_variables_test
  )
  add_test(
    NAME libmylite.runtime.sql_prepared_statements
    COMMAND mylite_runtime_sql_prepared_statements_test
  )
  add_test(
    NAME libmylite.runtime.show_like_filters
    COMMAND mylite_runtime_show_like_filters_test
  )
  add_test(
    NAME libmylite.runtime.show_databases_where
    COMMAND mylite_runtime_show_databases_where_test
  )
  add_test(
    NAME libmylite.runtime.show_create_table
    COMMAND mylite_runtime_show_create_table_test
  )
  add_test(
    NAME libmylite.runtime.create_table_comment_option
    COMMAND mylite_runtime_create_table_comment_option_test
  )
  add_test(
    NAME libmylite.runtime.table_storage_statistics_options
    COMMAND mylite_runtime_table_storage_statistics_options_test
  )
  add_test(
    NAME libmylite.runtime.column_comments
    COMMAND mylite_runtime_column_comments_test
  )
  add_test(
    NAME libmylite.runtime.show_create_database
    COMMAND mylite_runtime_show_create_database_test
  )
  add_test(
    NAME libmylite.runtime.schema_default_charset_collation
    COMMAND mylite_runtime_schema_default_charset_collation_test
  )
  add_test(
    NAME libmylite.runtime.show_table_status_introspection
    COMMAND mylite_runtime_show_table_status_introspection_test
  )
  add_test(
    NAME libmylite.runtime.show_character_set_collation
    COMMAND mylite_runtime_show_character_set_collation_test
  )
  add_test(
    NAME libmylite.runtime.show_variables
    COMMAND mylite_runtime_show_variables_test
  )
  add_test(
    NAME libmylite.runtime.show_status
    COMMAND mylite_runtime_show_status_test
  )
  add_test(
    NAME libmylite.runtime.innodb_engine_surface
    COMMAND mylite_runtime_innodb_engine_surface_test
  )
  add_test(
    NAME libmylite.runtime.table_charset_collation_surface
    COMMAND mylite_runtime_table_charset_collation_surface_test
  )
  add_test(
    NAME libmylite.runtime.wordpress_core_ddl_fixtures
    COMMAND mylite_runtime_wordpress_core_ddl_fixtures_test
  )
  add_test(
    NAME libmylite.runtime.wordpress_dbdelta_introspection
    COMMAND mylite_runtime_wordpress_dbdelta_introspection_test
  )
  add_test(
    NAME libmylite.runtime.column_charset_collation_attributes
    COMMAND mylite_runtime_column_charset_collation_attributes_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_default_charset_collation
    COMMAND mylite_runtime_alter_table_default_charset_collation_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_convert_character_set
    COMMAND mylite_runtime_alter_table_convert_character_set_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_order_by
    COMMAND mylite_runtime_alter_table_order_by_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_force
    COMMAND mylite_runtime_alter_table_force_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_algorithm_lock_clauses
    COMMAND mylite_runtime_alter_table_algorithm_lock_clauses_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_disable_enable_keys
    COMMAND mylite_runtime_alter_table_disable_enable_keys_test
  )
  add_test(
    NAME libmylite.runtime.table_maintenance
    COMMAND mylite_runtime_table_maintenance_test
  )
  add_test(
    NAME libmylite.runtime.create_table_like_lifecycle
    COMMAND mylite_runtime_create_table_like_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.temporary_table_like
    COMMAND mylite_runtime_temporary_table_like_test
  )
  add_test(
    NAME libmylite.runtime.temporary_create_table_select
    COMMAND mylite_runtime_temporary_create_table_select_test
  )
  add_test(
    NAME libmylite.runtime.temporary_auto_increment
    COMMAND mylite_runtime_temporary_auto_increment_test
  )
  add_test(
    NAME libmylite.runtime.secondary_index_lifecycle
    COMMAND mylite_runtime_secondary_index_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.fulltext_index_metadata
    COMMAND mylite_runtime_fulltext_index_metadata_test
  )
  add_test(
    NAME libmylite.runtime.spatial_index_metadata
    COMMAND mylite_runtime_spatial_index_metadata_test
  )
  add_test(
    NAME libmylite.runtime.index_options_metadata
    COMMAND mylite_runtime_index_options_metadata_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_add_index
    COMMAND mylite_runtime_alter_table_add_index_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_add_unique
    COMMAND mylite_runtime_alter_table_add_unique_test
  )
  add_test(
    NAME libmylite.runtime.create_index_lifecycle
    COMMAND mylite_runtime_create_index_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.functional_multivalued_index_diagnostics
    COMMAND mylite_runtime_functional_multivalued_index_diagnostics_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_drop_index
    COMMAND mylite_runtime_alter_table_drop_index_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_multi_action
    COMMAND mylite_runtime_alter_table_multi_action_test
  )
  add_test(
    NAME libmylite.runtime.drop_index_lifecycle
    COMMAND mylite_runtime_drop_index_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.drop_constraint_lifecycle
    COMMAND mylite_runtime_drop_constraint_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.rename_index_lifecycle
    COMMAND mylite_runtime_rename_index_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.alter_index_visibility
    COMMAND mylite_runtime_alter_index_visibility_test
  )
  add_test(
    NAME libmylite.runtime.unique_index_lifecycle
    COMMAND mylite_runtime_unique_index_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.named_unique_constraint
    COMMAND mylite_runtime_named_unique_constraint_test
  )
  add_test(
    NAME libmylite.runtime.unique_prefix_index_lifecycle
    COMMAND mylite_runtime_unique_prefix_index_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.binary_full_column_index
    COMMAND mylite_runtime_binary_full_column_index_test
  )
  add_test(
    NAME libmylite.runtime.descending_index_key_parts
    COMMAND mylite_runtime_descending_index_key_parts_test
  )
  add_test(
    NAME libmylite.runtime.foreign_key_constraints
    COMMAND mylite_runtime_foreign_key_constraints_test
  )
  add_test(
    NAME libmylite.runtime.check_constraint_lifecycle
    COMMAND mylite_runtime_check_constraint_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.alter_check_constraint_lifecycle
    COMMAND mylite_runtime_alter_check_constraint_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.char_varchar_key_lifecycle
    COMMAND mylite_runtime_char_varchar_key_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.composite_string_primary_key
    COMMAND mylite_runtime_composite_string_primary_key_test
  )
  add_test(
    NAME libmylite.runtime.create_table_select_lifecycle
    COMMAND mylite_runtime_create_table_select_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.explain_table_introspection
    COMMAND mylite_runtime_explain_table_introspection_test
  )
  add_test(
    NAME libmylite.runtime.table_rename_lifecycle
    COMMAND mylite_runtime_table_rename_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.temporary_table_lifecycle
    COMMAND mylite_runtime_temporary_table_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.temporary_index_lifecycle
    COMMAND mylite_runtime_temporary_index_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.lock_tables_lifecycle
    COMMAND mylite_runtime_lock_tables_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_rename_to
    COMMAND mylite_runtime_table_rename_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.row_values_lifecycle
    COMMAND mylite_runtime_row_values_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.replace_values_lifecycle
    COMMAND mylite_runtime_replace_values_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.replace_set_lifecycle
    COMMAND mylite_runtime_replace_set_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.replace_key_lifecycle
    COMMAND mylite_runtime_replace_key_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.replace_select_lifecycle
    COMMAND mylite_runtime_replace_select_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.replace_modifier_lifecycle
    COMMAND mylite_runtime_replace_modifier_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.insert_modifier_lifecycle
    COMMAND mylite_runtime_insert_modifier_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.insert_ignore_lifecycle
    COMMAND mylite_runtime_insert_ignore_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.load_data_infile
    COMMAND mylite_runtime_load_data_infile_test
  )
  add_test(
    NAME libmylite.runtime.nonstrict_dml_coercion
    COMMAND mylite_runtime_nonstrict_dml_coercion_test
  )
  add_test(
    NAME libmylite.runtime.dml_string_numeric_coercion
    COMMAND mylite_runtime_dml_string_numeric_coercion_test
  )
  add_test(
    NAME libmylite.runtime.dml_constant_scalar_values
    COMMAND mylite_runtime_dml_constant_scalar_values_test
  )
  add_test(
    NAME libmylite.runtime.dml_scalar_expression_values
    COMMAND mylite_runtime_dml_scalar_expression_values_test
  )
  add_test(
    NAME libmylite.runtime.insert_on_duplicate_key_update
    COMMAND mylite_runtime_insert_on_duplicate_key_update_test
  )
  add_test(
    NAME libmylite.runtime.select_where_lifecycle
    COMMAND mylite_runtime_select_where_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.select_row_scalar_predicates
    COMMAND mylite_runtime_select_row_scalar_predicates_test
  )
  add_test(
    NAME libmylite.runtime.exists_subquery_predicates
    COMMAND mylite_runtime_exists_subquery_predicates_test
  )
  add_test(
    NAME libmylite.runtime.in_subquery_predicates
    COMMAND mylite_runtime_in_subquery_predicates_test
  )
  add_test(
    NAME libmylite.runtime.where_and_predicates
    COMMAND mylite_runtime_where_and_predicates_test
  )
  add_test(
    NAME libmylite.runtime.select_order_limit_lifecycle
    COMMAND mylite_runtime_select_order_limit_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.select_distinct_rowsets
    COMMAND mylite_runtime_select_distinct_rowsets_test
  )
  add_test(
    NAME libmylite.runtime.table_statement
    COMMAND mylite_runtime_table_statement_test
  )
  add_test(
    NAME libmylite.runtime.values_statement
    COMMAND mylite_runtime_values_statement_test
  )
  add_test(
    NAME libmylite.runtime.select_qualified_columns
    COMMAND mylite_runtime_select_qualified_columns_test
  )
  add_test(
    NAME libmylite.runtime.union_select_lifecycle
    COMMAND mylite_runtime_union_select_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.intersect_except_select_lifecycle
    COMMAND mylite_runtime_intersect_except_select_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.inner_join_select
    COMMAND mylite_runtime_inner_join_select_test
  )
  add_test(
    NAME libmylite.runtime.left_join_select
    COMMAND mylite_runtime_left_join_select_test
  )
  add_test(
    NAME libmylite.runtime.right_join_select
    COMMAND mylite_runtime_right_join_select_test
  )
  add_test(
    NAME libmylite.runtime.select_item_alias
    COMMAND mylite_runtime_select_item_alias_test
  )
  add_test(
    NAME libmylite.runtime.row_number_window_function
    COMMAND mylite_runtime_row_number_window_function_test
  )
  add_test(
    NAME libmylite.runtime.window_rank_navigation_functions
    COMMAND mylite_runtime_window_rank_navigation_functions_test
  )
  add_test(
    NAME libmylite.runtime.select_literal_projection
    COMMAND mylite_runtime_select_literal_projection_test
  )
  add_test(
    NAME libmylite.runtime.row_scalar_expressions
    COMMAND mylite_runtime_row_scalar_expressions_test
  )
  add_test(
    NAME libmylite.runtime.scalar_subquery_projection
    COMMAND mylite_runtime_scalar_subquery_projection_test
  )
  add_test(
    NAME libmylite.runtime.string_equality_predicates
    COMMAND mylite_runtime_string_equality_predicates_test
  )
  add_test(
    NAME libmylite.runtime.expression_collate_predicates
    COMMAND mylite_runtime_expression_collate_predicates_test
  )
  add_test(
    NAME libmylite.runtime.string_order_lifecycle
    COMMAND mylite_runtime_string_order_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.string_range_predicates
    COMMAND mylite_runtime_string_range_predicates_test
  )
  add_test(
    NAME libmylite.runtime.like_predicates
    COMMAND mylite_runtime_like_predicates_test
  )
  add_test(
    NAME libmylite.runtime.regexp_rlike_predicates
    COMMAND mylite_runtime_regexp_rlike_predicates_test
  )
  add_test(
    NAME libmylite.runtime.regexp_like_function
    COMMAND mylite_runtime_regexp_like_function_test
  )
  add_test(
    NAME libmylite.runtime.view_lifecycle
    COMMAND mylite_runtime_view_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.delete_lifecycle
    COMMAND mylite_runtime_delete_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.joined_delete_lifecycle
    COMMAND mylite_runtime_joined_delete_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.joined_update_lifecycle
    COMMAND mylite_runtime_joined_update_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.update_lifecycle
    COMMAND mylite_runtime_update_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.update_index_hints_noop
    COMMAND mylite_runtime_update_index_hints_noop_test
  )
  add_test(
    NAME libmylite.runtime.update_ignore_modifier
    COMMAND mylite_runtime_update_ignore_modifier_test
  )
  add_test(
    NAME libmylite.runtime.update_scalar_subquery_assignment
    COMMAND mylite_runtime_update_scalar_subquery_assignment_test
  )
  add_test(
    NAME libmylite.runtime.update_subquery_predicates
    COMMAND mylite_runtime_update_subquery_predicates_test
  )
  add_test(
    NAME libmylite.runtime.update_arithmetic_assignment
    COMMAND mylite_runtime_update_arithmetic_assignment_test
  )
  add_test(
    NAME libmylite.runtime.update_unix_timestamp_arithmetic
    COMMAND mylite_runtime_update_unix_timestamp_arithmetic_test
  )
  add_test(
    NAME libmylite.runtime.update_date_interval_assignment
    COMMAND mylite_runtime_update_date_interval_assignment_test
  )
  add_test(
    NAME libmylite.runtime.update_multiple_assignments
    COMMAND mylite_runtime_update_multiple_assignments_test
  )
  add_test(
    NAME libmylite.runtime.transaction_lifecycle
    COMMAND mylite_runtime_transaction_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.dml_default_keyword_values
    COMMAND mylite_runtime_dml_default_keyword_values_test
  )
  add_test(
    NAME libmylite.runtime.alter_column_visibility
    COMMAND mylite_runtime_alter_column_visibility_test
  )
  add_test(
    NAME libmylite.runtime.small_integer_types
    COMMAND mylite_runtime_small_integer_types_test
  )
  add_test(
    NAME libmylite.runtime.varchar_type
    COMMAND mylite_runtime_varchar_type_test
  )
  add_test(
    NAME libmylite.runtime.enum_type
    COMMAND mylite_runtime_enum_type_test
  )
  add_test(
    NAME libmylite.runtime.set_type
    COMMAND mylite_runtime_set_type_test
  )
  add_test(
    NAME libmylite.runtime.char_type
    COMMAND mylite_runtime_char_type_test
  )
  add_test(
    NAME libmylite.runtime.character_alias_lifecycle
    COMMAND mylite_runtime_character_alias_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.text_type
    COMMAND mylite_runtime_text_type_test
  )
  add_test(
    NAME libmylite.runtime.json_type
    COMMAND mylite_runtime_json_type_test
  )
  add_test(
    NAME libmylite.runtime.binary_string_types
    COMMAND mylite_runtime_binary_string_types_test
  )
  add_test(
    NAME libmylite.runtime.long_character_binary_aliases
    COMMAND mylite_runtime_long_character_binary_aliases_test
  )
  add_test(
    NAME libmylite.runtime.bit_type
    COMMAND mylite_runtime_bit_type_test
  )
  add_test(
    NAME libmylite.runtime.string_defaults
    COMMAND mylite_runtime_string_defaults_test
  )
  add_test(
    NAME libmylite.runtime.decimal_type
    COMMAND mylite_runtime_decimal_type_test
  )
  add_test(
    NAME libmylite.runtime.float_double_type
    COMMAND mylite_runtime_float_double_type_test
  )
  add_test(
    NAME libmylite.runtime.date_type
    COMMAND mylite_runtime_date_type_test
  )
  add_test(
    NAME libmylite.runtime.year_type
    COMMAND mylite_runtime_year_type_test
  )
  add_test(
    NAME libmylite.runtime.time_type
    COMMAND mylite_runtime_time_type_test
  )
  add_test(
    NAME libmylite.runtime.datetime_type
    COMMAND mylite_runtime_datetime_type_test
  )
  add_test(
    NAME libmylite.runtime.timestamp_type
    COMMAND mylite_runtime_timestamp_type_test
  )
  add_test(
    NAME libmylite.runtime.zero_temporal_sql_modes
    COMMAND mylite_runtime_zero_temporal_sql_modes_test
  )
  add_test(
    NAME libmylite.runtime.relaxed_temporal_dml_literals
    COMMAND mylite_runtime_relaxed_temporal_dml_literals_test
  )
  add_test(
    NAME libmylite.runtime.current_timestamp_defaults
    COMMAND mylite_runtime_current_timestamp_defaults_test
  )
  add_test(
    NAME libmylite.runtime.current_date_time_defaults
    COMMAND mylite_runtime_current_date_time_defaults_test
  )
  add_test(
    NAME libmylite.runtime.current_date_time_functions
    COMMAND mylite_runtime_current_date_time_functions_test
  )
  add_test(
    NAME libmylite.runtime.utc_date_time_functions
    COMMAND mylite_runtime_utc_date_time_functions_test
  )
  add_test(
    NAME libmylite.runtime.sysdate_function
    COMMAND mylite_runtime_sysdate_function_test
  )
  add_test(
    NAME libmylite.runtime.primary_key_lifecycle
    COMMAND mylite_runtime_primary_key_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_add_primary_key
    COMMAND mylite_runtime_alter_table_add_primary_key_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_drop_primary_key
    COMMAND mylite_runtime_alter_table_drop_primary_key_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_auto_increment_option
    COMMAND mylite_runtime_alter_table_auto_increment_option_test
  )
  add_test(
    NAME libmylite.runtime.auto_increment_lifecycle
    COMMAND mylite_runtime_auto_increment_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.serial_alias_lifecycle
    COMMAND mylite_runtime_serial_alias_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.integer_default_literals
    COMMAND mylite_runtime_integer_default_literals_test
  )
  add_test(
    NAME libmylite.runtime.generated_column_lifecycle
    COMMAND mylite_runtime_generated_column_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.insert_set_lifecycle
    COMMAND mylite_runtime_insert_set_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.insert_unix_timestamp_arithmetic
    COMMAND mylite_runtime_insert_unix_timestamp_arithmetic_test
  )
  add_test(
    NAME libmylite.runtime.insert_select_lifecycle
    COMMAND mylite_runtime_insert_select_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.insert_select_union_source
    COMMAND mylite_runtime_insert_select_union_source_test
  )
  add_test(
    NAME libmylite.runtime.insert_select_on_duplicate_key_update
    COMMAND mylite_runtime_insert_select_on_duplicate_key_update_test
  )
  add_test(
    NAME libmylite.runtime.truncate_table_lifecycle
    COMMAND mylite_runtime_truncate_table_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_add_column_lifecycle
    COMMAND mylite_runtime_alter_table_add_column_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_drop_column_lifecycle
    COMMAND mylite_runtime_alter_table_drop_column_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_rename_column_lifecycle
    COMMAND mylite_runtime_alter_table_rename_column_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_modify_column_lifecycle
    COMMAND mylite_runtime_alter_table_modify_column_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_change_column_lifecycle
    COMMAND mylite_runtime_alter_table_change_column_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.file_backed_open
    COMMAND mylite_runtime_file_backed_open_test
  )
  add_test(NAME libmylite.runtime.open_memory COMMAND mylite_runtime_open_memory_test)
  add_test(NAME libmylite.runtime.result_metadata COMMAND mylite_runtime_result_metadata_test)
  add_test(
    NAME libmylite.runtime.result_column_metadata
    COMMAND mylite_runtime_result_column_metadata_test
  )
  add_test(
    NAME libmylite.runtime.sqlite_bootstrap
    COMMAND mylite_runtime_sqlite_bootstrap_test
  )
  add_test(
    NAME libmylite.runtime.sqlite_owner
    COMMAND mylite_runtime_sqlite_owner_test
  )
  add_test(
    NAME libmylite.runtime.statement_context
    COMMAND mylite_runtime_statement_context_test
  )
  add_test(NAME libmylite.sqlite COMMAND mylite_sqlite_test)
  add_test(NAME libmylite.version COMMAND mylite_version_test)
