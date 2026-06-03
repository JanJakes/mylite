#if defined(__linux__) && !defined(_XOPEN_SOURCE)
#  define _XOPEN_SOURCE 700
#endif

#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_connection.h"
#include "mylite_convert_tz.h"
#include "mylite_date_format.h"
#include "mylite_date_interval_second.h"
#include "mylite_datediff.h"
#include "mylite_diagnostics.h"
#include "mylite_dynamic_string.h"
#include "mylite_execution_catalog.h"
#include "mylite_execution_diagnostics.h"
#include "mylite_execution_dml_numeric.h"
#include "mylite_execution_loaded_catalog.h"
#include "mylite_execution_plan_types.h"
#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_charset_collation.h"
#include "mylite_execution_scalar_numeric.h"
#include "mylite_execution_scalar_regexp.h"
#include "mylite_execution_scalar_string_position.h"
#include "mylite_execution_scalar_string_transform.h"
#include "mylite_execution_scalar_temporal_format.h"
#include "mylite_execution_system_variables.h"
#include "mylite_integer_arithmetic.h"
#include "mylite_json.h"
#include "mylite_mysql_error_codes.h"
#include "mylite_mysql_server_identity.h"
#include "mylite_parser.h"
#include "mylite_period_functions.h"
#include "mylite_rand.h"
#include "mylite_regexp.h"
#include "mylite_result.h"
#include "mylite_sqlite_registration.h"
#include "mylite_statement_context.h"
#include "mylite_string_base64.h"
#include "mylite_string_bitmask.h"
#include "mylite_string_case.h"
#include "mylite_string_char.h"
#include "mylite_string_codepoint.h"
#include "mylite_string_concat.h"
#include "mylite_string_insert.h"
#include "mylite_string_padding.h"
#include "mylite_string_quote.h"
#include "mylite_string_replace.h"
#include "mylite_string_reverse.h"
#include "mylite_string_search.h"
#include "mylite_string_soundex.h"
#include "mylite_string_substring_index.h"
#include "mylite_string_trim.h"
#include "mylite_string_unhex.h"
#include "mylite_temporal_arithmetic.h"
#include "mylite_temporal_constructor.h"
#include "mylite_temporal_extract.h"
#include "mylite_timediff.h"
#include "mylite_timestamp_function.h"
#include "mylite_timestampdiff.h"
#include "mylite_unix_timestamp.h"
#include "mylite_uuid.h"
#include "mylite_weight_string.h"
#include "sqlite3.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#endif

enum {
    sqlite_use_nul_terminated_string = -1,
    year_conversion_incorrect_integer = 2,
    integer_even_divisor = 2,
    decimal_base = 10,
    decimal_round_half_digit = 5,
    rounding_negative_places_zero_threshold = 20,
    uint64_decimal_digit_capacity = 20,
    dml_integer_decimal_shift_limit = 1000000,
    time_zone_max_minute = 59,
    time_zone_seconds_per_minute = 60,
    time_zone_minutes_per_hour = 60,
    time_zone_max_positive_offset_minutes = 14 * time_zone_minutes_per_hour,
    time_zone_max_negative_offset_minutes =
        (13 * time_zone_minutes_per_hour) + time_zone_max_minute,
    temporal_predicate_offset_max_minutes = 14 * time_zone_minutes_per_hour,
    temporal_predicate_offset_sign_index = 0,
    temporal_predicate_offset_hour_tens_index = 1,
    temporal_predicate_offset_hour_ones_index = 2,
    temporal_predicate_offset_separator_index = 3,
    temporal_predicate_offset_minute_tens_index = 4,
    temporal_predicate_offset_minute_ones_index = 5,
    temporal_predicate_offset_text_length = 6,
    ascii_text_max_byte = 0x7fU,
    table_name_part_capacity = 3,
    integer_text_capacity = mylite_execution_scalar_integer_text_capacity,
    index_display_group_primary = 0,
    index_display_group_spatial = 1,
    index_display_group_not_null_unique = 2,
    index_display_group_nullable_unique = 3,
    index_display_group_secondary = 4,
    index_display_group_fulltext = 5,
    index_display_group_unknown = 6,
    select_source_alias_capacity = sizeof("_mylite_s") + integer_text_capacity,
    update_unique_internal_key_alias_capacity = sizeof("_mylite_key_") + integer_text_capacity,
    duplicate_key_value_display_length = 64,
    literal_projection_max_significant_digits = 81,
    literal_projection_text_capacity = mylite_execution_scalar_literal_projection_text_capacity,
    show_create_integer_default_text_capacity = integer_text_capacity + sizeof(" DEFAULT ''"),
    generated_default_display_text_capacity = (MYLITE_CATALOG_DEFAULT_TEXT_CAPACITY * 2) + 16,
    base64_invalid_message_capacity = 64,
    base64_signed_integer_message_capacity = 96,
    base64_column_message_capacity = 128,
    base64_unsupported_message_capacity = 256,
    window_function_name_lower_capacity = 32,
    window_function_unsupported_message_capacity = 256,
    integer_expression_default_text_initial_capacity = 16,
    integer_expression_default_text_growth_factor = 2,
    decimal_default_precision = 10,
    decimal_default_scale = 0,
    decimal_max_precision = 65,
    decimal_max_scale = 30,
    date_text_length = 10,
    date_year_text_offset = 0,
    date_year_text_length = 4,
    date_first_separator_offset = 4,
    date_month_text_offset = 5,
    date_month_text_length = 2,
    date_second_separator_offset = 7,
    date_day_text_offset = 8,
    date_day_text_length = 2,
    datetime_text_length = mylite_execution_scalar_datetime_text_length,
    datetime_date_text_length = 10,
    timestamp_2_text_length = 22,
    datetime_date_time_separator_offset = 10,
    datetime_hour_text_offset = 11,
    datetime_hour_text_length = 2,
    datetime_first_time_separator_offset = 13,
    datetime_minute_text_offset = 14,
    datetime_minute_text_length = 2,
    datetime_second_time_separator_offset = 16,
    datetime_second_text_offset = 17,
    datetime_second_text_length = 2,
    datetime_minimum_hour = 0,
    datetime_maximum_hour = 23,
    datetime_minimum_minute_or_second = 0,
    datetime_maximum_minute_or_second = 59,
    datetime_hours_per_day = 24,
    date_decimal_base = 10,
    date_minimum_year = 1000,
    date_maximum_year = 9999,
    date_first_month = 1,
    date_first_day = 1,
    date_february = 2,
    date_months_per_year = 12,
    date_maximum_day_field = 31,
    date_leap_day = 29,
    date_leap_year_quadricentennial = 400,
    date_leap_year_century = 100,
    date_leap_year_quadrennial = 4,
    date_add_march_year_shift_month = date_february,
    date_add_march_based_month_switch = 10,
    date_add_month_shift = 3,
    date_add_march_based_months_after_february = 9,
    date_add_month_scale = 153,
    date_add_month_bias = 2,
    date_add_month_divisor = 5,
    date_add_days_per_non_leap_year = 365,
    date_add_era_year_offset = date_leap_year_quadricentennial - 1,
    date_add_leap_cycle_four_year_days = 1460,
    date_add_leap_cycle_century_days = 36524,
    date_add_days_per_era = 146097,
    date_add_days_per_era_offset = date_add_days_per_era - 1,
    date_add_unix_epoch_day_offset = 719468,
    date_interval_diagnostic_capacity = 160,
    date_interval_temporal_diagnostic_capacity = 128,
    date_interval_nul_diagnostic_capacity = 96,
    date_interval_format_diagnostic_capacity = 96,
    date_interval_days_per_week = 7,
    date_interval_months_per_quarter = 3,
    date_interval_planned_argument_count = 5,
    system_variable_body_offset = 2,
    show_databases_initial_name_capacity = 8,
    show_tables_result_column_count = 2,
    show_tables_name_column = 0,
    show_tables_type_column = 1,
    show_columns_result_column_count = 6,
    show_full_columns_result_column_count = 9,
    show_columns_extra_column = 5,
    show_full_columns_default_column = 5,
    show_full_columns_extra_column = 6,
    show_full_columns_privileges_column = 7,
    show_full_columns_comment_column = 8,
    show_index_non_unique_column = 1,
    show_index_seq_in_index_column = 3,
    show_index_cardinality_column = 6,
    show_index_sub_part_column = 7,
    show_index_result_column_count = 15,
    show_index_null_column = 9,
    show_create_table_result_column_count = 2,
    show_create_view_result_column_count = 4,
    show_create_database_result_column_count = 2,
    show_table_status_name_column = 0,
    show_table_status_version_column = 2,
    show_table_status_rows_column = 4,
    show_table_status_average_row_length_column = 5,
    show_table_status_data_length_column = 6,
    show_table_status_max_data_length_column = 7,
    show_table_status_data_free_column = 9,
    show_table_status_checksum_column = 15,
    show_table_status_result_column_count = 18,
    show_table_status_index_length_column = 8,
    show_table_status_auto_increment_column = 10,
    show_table_status_create_time_column = 11,
    show_table_status_update_time_column = 12,
    show_table_status_data_length = 16384,
    table_status_create_options_capacity = 192,
    show_character_set_result_column_count = 4,
    show_collation_result_column_count = 7,
    show_triggers_result_column_count = 11,
    show_events_result_column_count = 15,
    show_open_tables_result_column_count = 4,
    show_routine_status_result_column_count = 12,
    show_processlist_result_column_count = 8,
    show_grants_result_column_count = 1,
    show_privileges_result_column_count = 3,
    show_binary_log_status_result_column_count = 5,
    show_binary_logs_result_column_count = 3,
    show_replica_status_result_column_count = 60,
    show_replicas_result_column_count = 5,
    show_warnings_result_column_count = 3,
    show_count_warnings_result_column_count = 1,
    show_errors_result_column_count = 3,
    show_count_errors_result_column_count = 1,
    table_maintenance_result_column_count = 4,
    checksum_table_result_column_count = 2,
    checksum_table_name_display_length = 384,
    checksum_table_checksum_display_length = 22,
    information_schema_schemata_column_count = 6,
    information_schema_schemata_extensions_column_count = 3,
    information_schema_columns_extensions_column_count = 6,
    information_schema_tables_extensions_column_count = 5,
    information_schema_table_constraints_extensions_column_count = 6,
    information_schema_tablespaces_extensions_column_count = 2,
    information_schema_files_column_count = 38,
    information_schema_innodb_cmp_column_count = 6,
    information_schema_innodb_cmpmem_column_count = 6,
    information_schema_innodb_cmp_per_index_column_count = 8,
    information_schema_innodb_datafiles_column_count = 2,
    information_schema_innodb_tablespaces_column_count = 15,
    information_schema_innodb_fields_column_count = 3,
    information_schema_innodb_foreign_column_count = 5,
    information_schema_innodb_foreign_cols_column_count = 4,
    information_schema_innodb_columns_column_count = 8,
    information_schema_innodb_indexes_column_count = 8,
    information_schema_innodb_tables_column_count = 10,
    information_schema_innodb_tablestats_column_count = 9,
    information_schema_innodb_metrics_column_count = 17,
    information_schema_innodb_session_temp_tablespaces_column_count = 6,
    information_schema_innodb_virtual_column_count = 3,
    information_schema_innodb_trx_column_count = 25,
    information_schema_innodb_buffer_page_column_count = 21,
    information_schema_innodb_buffer_page_lru_column_count = 20,
    information_schema_innodb_buffer_pool_stats_column_count = 32,
    information_schema_innodb_cached_indexes_column_count = 3,
    information_schema_innodb_ft_config_column_count = 2,
    information_schema_innodb_ft_deleted_column_count = 1,
    information_schema_innodb_ft_default_stopword_column_count = 1,
    information_schema_innodb_ft_index_column_count = 6,
    information_schema_innodb_tablespaces_brief_column_count = 5,
    information_schema_innodb_temp_table_info_column_count = 4,
    information_schema_innodb_index_nonunique_type = 0,
    information_schema_innodb_index_generated_cluster_type = 1,
    information_schema_innodb_index_unique_type = 2,
    information_schema_innodb_index_primary_type = 3,
    information_schema_innodb_index_fulltext_type = 32,
    information_schema_innodb_index_spatial_type = 64,
    information_schema_innodb_index_default_page_no = 0,
    information_schema_innodb_index_fulltext_page_no = -1,
    information_schema_innodb_index_space = 0,
    information_schema_innodb_index_merge_threshold = 50,
    information_schema_innodb_cluster_system_field_count = 2,
    information_schema_innodb_generated_cluster_system_field_count = 3,
    information_schema_innodb_generated_cluster_key_part_count = 1,
    information_schema_innodb_field_position_offset = 1,
    information_schema_innodb_table_redundant_flag = 0,
    information_schema_innodb_table_compact_flag = 1,
    information_schema_innodb_table_dynamic_flag = 33,
    information_schema_innodb_table_compressed_flag = 41,
    information_schema_innodb_table_hidden_column_count = 3,
    information_schema_innodb_table_space = 0,
    information_schema_innodb_table_default_zip_page_size = 0,
    information_schema_innodb_table_default_compressed_zip_page_size = 8192,
    information_schema_innodb_table_zip_page_unit = 1024,
    information_schema_innodb_table_instant_cols = 0,
    information_schema_innodb_table_total_row_versions = 0,
    information_schema_innodb_column_mtype_fixbinary = 3,
    information_schema_innodb_column_mtype_binary = 4,
    information_schema_innodb_column_mtype_blob = 5,
    information_schema_innodb_column_mtype_int = 6,
    information_schema_innodb_column_mtype_float = 9,
    information_schema_innodb_column_mtype_double = 10,
    information_schema_innodb_column_mtype_varchar = 12,
    information_schema_innodb_column_mtype_char = 13,
    information_schema_innodb_column_mtype_geometry = 14,
    information_schema_innodb_column_mysql_type_tiny = 1,
    information_schema_innodb_column_mysql_type_short = 2,
    information_schema_innodb_column_mysql_type_long = 3,
    information_schema_innodb_column_mysql_type_float = 4,
    information_schema_innodb_column_mysql_type_double = 5,
    information_schema_innodb_column_mysql_type_timestamp = 7,
    information_schema_innodb_column_mysql_type_longlong = 8,
    information_schema_innodb_column_mysql_type_int24 = 9,
    information_schema_innodb_column_mysql_type_date = 10,
    information_schema_innodb_column_mysql_type_time = 11,
    information_schema_innodb_column_mysql_type_datetime = 12,
    information_schema_innodb_column_mysql_type_year = 13,
    information_schema_innodb_column_mysql_type_varchar = 15,
    information_schema_innodb_column_mysql_type_bit = 16,
    information_schema_innodb_column_mysql_type_json = 245,
    information_schema_innodb_column_mysql_type_newdecimal = 246,
    information_schema_innodb_column_mysql_type_blob = 252,
    information_schema_innodb_column_mysql_type_string = 254,
    information_schema_innodb_column_mysql_type_geometry = 255,
    information_schema_innodb_column_not_null_flag = 256,
    information_schema_innodb_column_unsigned_flag = 512,
    information_schema_innodb_column_numeric_flag = 1024,
    information_schema_innodb_column_temporal_decimal_flag = 525312,
    information_schema_innodb_column_collation_shift = 16,
    information_schema_innodb_column_blob_length_tiny = 9,
    information_schema_innodb_column_blob_length_regular = 10,
    information_schema_innodb_column_blob_length_medium = 11,
    information_schema_innodb_column_blob_length_long = 12,
    information_schema_innodb_column_has_default = 0,
    information_schema_innodb_foreign_delete_cascade_type = 1,
    information_schema_innodb_foreign_delete_set_null_type = 2,
    information_schema_innodb_foreign_update_cascade_type = 4,
    information_schema_innodb_foreign_update_set_null_type = 8,
    information_schema_innodb_foreign_delete_no_action_type = 16,
    information_schema_innodb_foreign_update_no_action_type = 32,
    tablespace_name_separator_size = 1,
    tablespace_name_terminator_size = 1,
    information_schema_tables_column_count = 21,
    information_schema_columns_column_count = 22,
    information_schema_administrable_role_authorizations_column_count = 9,
    information_schema_applicable_roles_column_count = 9,
    information_schema_character_sets_column_count = 4,
    information_schema_check_constraints_column_count = 4,
    information_schema_column_privileges_column_count = 7,
    information_schema_column_statistics_column_count = 4,
    information_schema_connection_control_failed_login_attempts_column_count = 2,
    information_schema_collation_applicability_column_count = 2,
    information_schema_collations_column_count = 7,
    information_schema_enabled_roles_column_count = 4,
    information_schema_engines_column_count = 6,
    information_schema_events_column_count = 24,
    information_schema_keywords_column_count = 2,
    information_schema_optimizer_trace_column_count = 4,
    information_schema_parameters_column_count = 16,
    information_schema_partitions_column_count = 25,
    information_schema_plugins_column_count = 11,
    information_schema_processlist_column_count = 8,
    information_schema_profiling_column_count = 18,
    information_schema_resource_groups_column_count = 5,
    information_schema_st_geometry_columns_column_count = 7,
    information_schema_st_spatial_reference_systems_column_count = 6,
    information_schema_st_units_of_measure_column_count = 4,
    information_schema_st_geometry_columns_schema_column = 1,
    information_schema_st_geometry_columns_table_column = 2,
    information_schema_st_geometry_columns_column_name_column = 3,
    information_schema_st_geometry_columns_type_name_column = 6,
    information_schema_processlist_db_column = 3,
    information_schema_processlist_info_column = 7,
    information_schema_routines_column_count = 31,
    information_schema_schema_privileges_column_count = 5,
    information_schema_table_constraints_column_count = 7,
    information_schema_key_column_usage_column_count = 12,
    information_schema_statistics_column_count = 18,
    information_schema_referential_constraints_column_count = 11,
    information_schema_role_column_grants_column_count = 10,
    information_schema_role_routine_grants_column_count = 12,
    information_schema_role_table_grants_column_count = 9,
    information_schema_table_privileges_column_count = 6,
    information_schema_triggers_column_count = 22,
    information_schema_user_attributes_column_count = 3,
    information_schema_user_privileges_column_count = 4,
    information_schema_views_column_count = 10,
    information_schema_view_routine_usage_column_count = 6,
    information_schema_view_table_usage_column_count = 6,
    mysql_component_column_count = 3,
    mysql_engine_cost_column_count = 7,
    mysql_func_column_count = 4,
    mysql_gtid_executed_column_count = 4,
    mysql_general_log_column_count = 6,
    mysql_user_column_count = 51,
    mysql_global_grants_column_count = 4,
    mysql_db_column_count = 22,
    mysql_tables_priv_column_count = 8,
    mysql_columns_priv_column_count = 7,
    mysql_procs_priv_column_count = 8,
    mysql_proxies_priv_column_count = 7,
    mysql_default_roles_column_count = 4,
    mysql_role_edges_column_count = 5,
    mysql_password_history_column_count = 4,
    sys_sys_config_column_count = 4,
    sys_version_column_count = 2,
    sys_host_summary_column_count = 12,
    sys_host_summary_by_file_io_column_count = 3,
    sys_host_summary_by_file_io_type_column_count = 5,
    sys_host_summary_by_stages_column_count = 5,
    sys_host_summary_by_statement_latency_column_count = 10,
    sys_host_summary_by_statement_type_column_count = 11,
    sys_innodb_buffer_stats_by_schema_column_count = 7,
    sys_innodb_buffer_stats_by_table_column_count = 8,
    sys_innodb_lock_waits_column_count = 30,
    sys_io_by_thread_by_latency_column_count = 8,
    sys_io_global_by_file_by_bytes_column_count = 9,
    sys_io_global_by_file_by_latency_column_count = 9,
    sys_io_global_by_wait_by_bytes_column_count = 13,
    sys_io_global_by_wait_by_latency_column_count = 14,
    sys_latest_file_io_column_count = 5,
    sys_memory_by_host_by_current_bytes_column_count = 6,
    sys_memory_by_thread_by_current_bytes_column_count = 7,
    sys_memory_by_user_by_current_bytes_column_count = 6,
    sys_ps_check_lost_instrumentation_column_count = 2,
    sys_schema_auto_increment_columns_column_count = 10,
    sys_schema_index_statistics_column_count = 11,
    sys_x_ps_schema_table_statistics_io_column_count = 10,
    sys_schema_table_statistics_column_count = 19,
    sys_schema_table_statistics_with_buffer_column_count = 25,
    sys_schema_tables_with_full_table_scans_column_count = 4,
    sys_schema_unused_indexes_column_count = 3,
    sys_schema_redundant_indexes_column_count = 10,
    sys_x_schema_flattened_keys_column_count = 6,
    sys_schema_table_lock_waits_column_count = 18,
    sys_flattened_key_row_initial_capacity = 8,
    sys_schema_object_overview_column_count = 3,
    sys_schema_object_overview_initial_group_capacity = 8,
    mysql_slow_log_column_count = 12,
    mysql_help_category_column_count = 4,
    mysql_help_keyword_column_count = 2,
    mysql_help_relation_column_count = 2,
    mysql_help_topic_column_count = 6,
    mysql_ndb_binlog_index_column_count = 12,
    mysql_plugin_column_count = 2,
    mysql_slave_master_info_column_count = 33,
    mysql_slave_relay_log_info_column_count = 15,
    mysql_slave_worker_info_column_count = 13,
    mysql_server_cost_column_count = 5,
    mysql_servers_column_count = 9,
    mysql_innodb_index_stats_column_count = 8,
    mysql_innodb_table_stats_column_count = 6,
    information_schema_tables_table_schema_column = 1,
    information_schema_tables_table_name_column = 2,
    information_schema_tables_index_length_column = 11,
    information_schema_tables_auto_increment_column = 13,
    information_schema_tables_create_time_column = 14,
    information_schema_tables_update_time_column = 15,
    information_schema_columns_table_schema_column = 1,
    information_schema_columns_table_name_column = 2,
    information_schema_columns_column_name_column = 3,
    information_schema_columns_ordinal_position_column = 4,
    information_schema_columns_default_column = 5,
    information_schema_columns_is_nullable_column = 6,
    information_schema_columns_character_maximum_length_column = 8,
    information_schema_columns_character_octet_length_column = 9,
    information_schema_columns_numeric_precision_column = 10,
    information_schema_columns_numeric_scale_column = 11,
    information_schema_columns_datetime_precision_column = 12,
    information_schema_columns_character_set_name_column = 13,
    information_schema_columns_collation_name_column = 14,
    information_schema_columns_column_type_column = 15,
    information_schema_columns_column_key_column = 16,
    information_schema_columns_extra_column = 17,
    information_schema_columns_column_comment_column = 19,
    information_schema_columns_generation_expression_column = 20,
    show_processlist_info_truncation_length = 100,
    show_processlist_db_column = 3,
    show_processlist_info_column = 7,
    show_engines_result_column_count = 6,
    show_engine_status_result_column_count = 3,
    show_plugins_result_column_count = 5,
    select_item_alias_max_length = 256,
    select_item_alias_capacity = select_item_alias_max_length + 1,
    avg_fraction_digits = 4,
    avg_fraction_scale = 10000,
    avg_round_half_digit = 5,
    if_stack_initial_capacity = 8,
    scalar_bitwise_integer_bits = 64,
    base_conversion_binary_base = 2,
    base_conversion_octal_base = 8,
    base_conversion_hexadecimal_base = 16,
    base_conversion_max_base = 36,
    base_conversion_text_capacity = mylite_execution_scalar_base_conversion_text_capacity,
    mysql_collation_binary_id = 63,
    mysql_collation_utf8mb3_general_ci_id = 33,
    mysql_collation_utf8mb3_bin_id = 83,
    mysql_collation_latin1_swedish_ci_id = 8,
    mysql_collation_latin1_bin_id = 47,
    mysql_collation_utf8mb4_0900_ai_ci_id = 255,
    mysql_float_display_length = 12,
    mysql_double_display_length = 22,
    mysql_approximate_decimals = 31,
    mysql_tinyint_bool_display_length = 1,
    mysql_signed_tinyint_display_length = 4,
    mysql_unsigned_tinyint_display_length = 3,
    mysql_signed_smallint_display_length = 6,
    mysql_unsigned_smallint_display_length = 5,
    mysql_signed_mediumint_display_length = 9,
    mysql_unsigned_mediumint_display_length = 8,
    mysql_signed_int_display_length = 11,
    mysql_unsigned_int_display_length = 10,
    mysql_bigint_display_length = 20,
    mysql_scalar_bigint_display_length = 21,
    mysql_scalar_double_display_length = 23,
    mysql_database_function_display_length = 256,
    mysql_user_function_display_length = 1152,
    mysql_version_function_display_length = 20,
    mysql_json_value_display_character_length = 512,
    mysql_json_type_display_length = 68,
    mysql_temporal_string_function_display_length = 29,
    mysql_calendar_name_function_display_length = 9,
    mysql_regexp_string_function_display_length = 4096,
    double_text_max_significant_digits = 17,
    double_text_capacity = mylite_execution_scalar_double_text_capacity,
    scalar_exact_decimal_part_capacity = literal_projection_max_significant_digits + 1,
    scalar_format_max_decimals = 30,
    crc32_bits_per_byte = 8,
    double_format_error_capacity = 80,
    float_text_max_significant_digits = 6,
    approximate_numeric_text_capacity = 48,
    float_max_precision = 53,
    float_double_precision_threshold = 24,
    char_default_length = 1,
    char_max_length = 255,
    char_logical_prefix_length = 5,
    char_logical_syntax_overhead = 6,
    nchar_logical_prefix_length = 6,
    nchar_logical_syntax_overhead = 7,
    varchar_max_length = 16383,
    varchar_string_key_max_length = 255,
    varchar_logical_prefix_length = 8,
    varchar_logical_syntax_overhead = 9,
    nvarchar_logical_prefix_length = 9,
    nvarchar_logical_syntax_overhead = 10,
    binary_default_length = 1,
    binary_max_length = 255,
    binary_logical_prefix_length = 7,
    binary_logical_syntax_overhead = 8,
    varbinary_max_length = 65535,
    varbinary_row_size_max_length = 65532,
    varbinary_logical_prefix_length = 10,
    varbinary_logical_syntax_overhead = 11,
    bit_default_length = 1,
    bit_max_length = 64,
    bit_logical_prefix_length = 4,
    bit_logical_syntax_overhead = 5,
    bit_literal_default_capacity = sizeof("b'") + bit_max_length + sizeof("'"),
    enum_logical_prefix_length = 5,
    enum_logical_syntax_overhead = 6,
    enum_label_count_capacity = 255,
    enum_one_byte_label_count_max = 255,
    set_logical_prefix_length = 4,
    set_logical_syntax_overhead = 5,
    set_member_count_capacity = 64,
    set_one_byte_storage_member_count_max = 8,
    set_two_byte_storage_member_count_max = 16,
    set_three_byte_storage_member_count_max = 24,
    set_four_byte_storage_member_count_max = 32,
    set_one_byte_storage_size = 1,
    set_two_byte_storage_size = 2,
    set_three_byte_storage_size = 3,
    set_four_byte_storage_size = 4,
    set_eight_byte_storage_size = 8,
    bit_byte_rounding_offset = CHAR_BIT - 1,
    byte_bit_count = CHAR_BIT,
    byte_all_bits_mask = 0xff,
    byte_high_nibble_shift = 4,
    byte_low_nibble_mask = 0x0f,
    ascii_printable_min_byte = 0x20U,
    ascii_printable_max_byte = 0x7eU,
    blob_family_row_size_contribution = 12,
    mysql_max_row_size = 65535,
    innodb_max_key_length_bytes = 3072,
    utf8mb3_max_bytes_per_character = 3,
    utf8mb4_max_bytes_per_character = 4,
    varchar_one_byte_length_prefix_max = 255,
    text_family_row_size_contribution = 12,
    tinyint_row_size_bytes = 1,
    smallint_row_size_bytes = 2,
    mediumint_row_size_bytes = 3,
    int_row_size_bytes = 4,
    bigint_row_size_bytes = 8,
    float_row_size_bytes = 4,
    double_row_size_bytes = 8,
    date_row_size_bytes = 3,
    time_row_size_bytes = 3,
    datetime_row_size_bytes = 5,
    timestamp_row_size_bytes = 4,
    year_row_size_bytes = 1,
    year_text_length = 4,
    year_minimum_normal = 1901,
    year_maximum = 2155,
    year_two_digit_high_max = 69,
    year_two_digit_low_min = 70,
    year_two_digit_max = 99,
    year_two_digit_high_base = 2000,
    year_two_digit_low_base = 1900,
    year_direct_maximum = 9999,
    time_text_minimum_length = 8,
    time_text_maximum_length = 10,
    time_minute_second_suffix_length = 6,
    time_minimum_three_digit_hour = 100,
    time_second_per_minute = 60,
    time_second_per_hour = 3600,
    time_maximum_hour = 838,
    time_maximum_minute_or_second = 59,
    decimal_digits_per_storage_group = 9,
    tinytext_max_length = 255,
    text_max_length = 65535,
    mediumtext_max_length = 16777215,
    utf8_ascii_max = 0x7f,
    utf8_continuation_mask = 0xc0,
    utf8_continuation_tag = 0x80,
    utf8_two_byte_min = 0xc2,
    utf8_two_byte_max = 0xdf,
    utf8_three_byte_e0 = 0xe0,
    utf8_three_byte_e0_second_min = 0xa0,
    utf8_three_byte_second_max = 0xbf,
    utf8_three_byte_min = 0xe1,
    utf8_three_byte_first_gap_start = 0xed,
    utf8_three_byte_first_gap_end = 0xed,
    utf8_three_byte_max = 0xef,
    utf8_surrogate_second_max = 0x9f,
    utf8_four_byte_f0 = 0xf0,
    utf8_four_byte_f0_second_min = 0x90,
    utf8_four_byte_min = 0xf1,
    utf8_four_byte_max_before_last = 0xf3,
    utf8_four_byte_last = 0xf4,
    utf8_four_byte_last_second_max = 0x8f,
};

static const int64_t information_schema_innodb_generated_cluster_index_id_base =
    1000000000000000000LL;
static const char information_schema_innodb_generated_cluster_index_name[] = "GEN_CLUST_INDEX";

static const char *const show_index_result_columns[show_index_result_column_count] = {
    "Table",
    "Non_unique",
    "Key_name",
    "Seq_in_index",
    "Column_name",
    "Collation",
    "Cardinality",
    "Sub_part",
    "Packed",
    "Null",
    "Index_type",
    "Comment",
    "Index_comment",
    "Visible",
    "Expression",
};

static const char *const show_columns_result_columns[show_columns_result_column_count] = {
    "Field",
    "Type",
    "Null",
    "Key",
    "Default",
    "Extra",
};

static const char *const show_full_columns_result_columns[show_full_columns_result_column_count] = {
    "Field",
    "Type",
    "Collation",
    "Null",
    "Key",
    "Default",
    "Extra",
    "Privileges",
    "Comment",
};

static const char *const show_table_status_result_columns[show_table_status_result_column_count] = {
    "Name",
    "Engine",
    "Version",
    "Row_format",
    "Rows",
    "Avg_row_length",
    "Data_length",
    "Max_data_length",
    "Index_length",
    "Data_free",
    "Auto_increment",
    "Create_time",
    "Update_time",
    "Check_time",
    "Collation",
    "Checksum",
    "Create_options",
    "Comment",
};

static const char scalar_pi_text[] = "3.141593";
static const long double decimal_round_integer_step = 1.0L;
static const int tm_year_calendar_offset = 1900;
static const uint64_t longtext_max_length = 4294967295ULL;
static const uint64_t mysql_json_document_display_length = 4294967292ULL;
static const uint64_t max_allowed_packet_default_value = 67108864ULL;
static const uint64_t group_concat_max_len_default_value =
    MYLITE_SESSION_GROUP_CONCAT_MAX_LEN_DEFAULT_VALUE;
static const uint64_t group_concat_max_len_minimum_value = 4ULL;
static const uint64_t information_schema_stats_expiry_default_value =
    MYLITE_SESSION_INFORMATION_SCHEMA_STATS_EXPIRY_DEFAULT_VALUE;
static const uint64_t information_schema_stats_expiry_max_value = 31536000ULL;
static const uint64_t protocol_version_system_variable_value = 10ULL;
static const uint64_t port_system_variable_value = 3306ULL;
static const char basedir_system_variable_value[] = "/usr/";
static const char datadir_system_variable_value[] = "/var/lib/mysql/";
static const char hostname_system_variable_value[] = "mylite";
static const char license_system_variable_value[] = "GPL";
static const char pid_file_system_variable_value[] = "/var/run/mysqld/mysqld.pid";
static const char plugin_dir_system_variable_value[] = "/usr/lib64/mysql/plugin/";
static const char socket_system_variable_value[] = "/var/run/mysqld/mysqld.sock";
static const char version_compile_machine_system_variable_value[] = "aarch64";
static const char version_compile_os_system_variable_value[] = "Linux";
static const char version_compile_zlib_system_variable_value[] = "1.3.2";
static const uint64_t server_id_system_variable_value = 1ULL;
static const uint64_t server_id_bits_system_variable_value = 32ULL;
static const char server_uuid_system_variable_value[] = "4d796c69-7465-4000-8000-000000000001";
static const char log_bin_basename_system_variable_value[] = "binlog";
static const char log_bin_index_system_variable_value[] = "binlog.index";
static const uint64_t timeout_system_variable_default_value = MYLITE_SESSION_TIMEOUT_DEFAULT_VALUE;
static const uint64_t timeout_system_variable_max_value = 31536000ULL;
static const uint64_t scalar_integer_cast_int64_min_magnitude = 9223372036854775808ULL;
static const uint64_t table_key_block_size_eight = 8U;
static const uint64_t table_key_block_size_sixteen = 16U;
static const uint64_t table_stats_sample_pages_max = 65535U;
static const unsigned char ascii_max_byte = 0x7fU;
static const char national_character_set_name[] = "utf8mb3";
static const char national_collation_name[] = "utf8mb3_general_ci";
static const char national_character_set_warning_message[] =
    "NATIONAL/NCHAR/NVARCHAR implies the character set UTF8MB3, which will be replaced by "
    "UTF8MB4 in a future release. Please consider using CHAR(x) CHARACTER SET UTF8MB4 in order "
    "to be unambiguous.";
static const char string_key_collation_name[] = "utf8mb4_0900_ai_ci";
static const char insert_select_union_branch_column_name[] = "_mylite_union_branch";
static const char insert_select_union_current_alias[] = "_mylite_union_current";
static const char insert_select_union_prior_alias[] = "_mylite_union_prior";

static const char *const embedded_root_global_privileges[] = {
    "ALLOW_NONEXISTENT_DEFINER",
    "ALTER",
    "ALTER ROUTINE",
    "APPLICATION_PASSWORD_ADMIN",
    "AUDIT_ABORT_EXEMPT",
    "AUDIT_ADMIN",
    "AUTHENTICATION_POLICY_ADMIN",
    "BACKUP_ADMIN",
    "BINLOG_ADMIN",
    "BINLOG_ENCRYPTION_ADMIN",
    "CLONE_ADMIN",
    "CONNECTION_ADMIN",
    "CREATE",
    "CREATE ROLE",
    "CREATE ROUTINE",
    "CREATE TABLESPACE",
    "CREATE TEMPORARY TABLES",
    "CREATE USER",
    "CREATE VIEW",
    "DELETE",
    "DROP",
    "DROP ROLE",
    "ENCRYPTION_KEY_ADMIN",
    "EVENT",
    "EXECUTE",
    "FILE",
    "FIREWALL_EXEMPT",
    "FLUSH_OPTIMIZER_COSTS",
    "FLUSH_PRIVILEGES",
    "FLUSH_STATUS",
    "FLUSH_TABLES",
    "FLUSH_USER_RESOURCES",
    "GROUP_REPLICATION_ADMIN",
    "GROUP_REPLICATION_STREAM",
    "INDEX",
    "INNODB_REDO_LOG_ARCHIVE",
    "INNODB_REDO_LOG_ENABLE",
    "INSERT",
    "LOCK TABLES",
    "OPTIMIZE_LOCAL_TABLE",
    "PASSWORDLESS_USER_ADMIN",
    "PERSIST_RO_VARIABLES_ADMIN",
    "PROCESS",
    "REFERENCES",
    "RELOAD",
    "REPLICATION CLIENT",
    "REPLICATION SLAVE",
    "REPLICATION_APPLIER",
    "REPLICATION_SLAVE_ADMIN",
    "RESOURCE_GROUP_ADMIN",
    "RESOURCE_GROUP_USER",
    "ROLE_ADMIN",
    "SELECT",
    "SENSITIVE_VARIABLES_OBSERVER",
    "SERVICE_CONNECTION_ADMIN",
    "SESSION_VARIABLES_ADMIN",
    "SET_ANY_DEFINER",
    "SHOW DATABASES",
    "SHOW VIEW",
    "SHOW_ROUTINE",
    "SHUTDOWN",
    "SUPER",
    "SYSTEM_USER",
    "SYSTEM_VARIABLES_ADMIN",
    "TABLE_ENCRYPTION_ADMIN",
    "TELEMETRY_LOG_ADMIN",
    "TRANSACTION_GTID_TAG",
    "TRIGGER",
    "UPDATE",
    "XA_RECOVER_ADMIN",
};

static const char *const show_grants_embedded_root_rows[] = {
    "GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, RELOAD, SHUTDOWN, PROCESS, FILE, "
    "REFERENCES, INDEX, ALTER, SHOW DATABASES, SUPER, CREATE TEMPORARY TABLES, LOCK TABLES, "
    "EXECUTE, REPLICATION SLAVE, REPLICATION CLIENT, CREATE VIEW, SHOW VIEW, CREATE ROUTINE, "
    "ALTER ROUTINE, CREATE USER, EVENT, TRIGGER, CREATE TABLESPACE, CREATE ROLE, DROP ROLE ON "
    "*.* TO `root`@`%` WITH GRANT OPTION",
    "GRANT "
    "ALLOW_NONEXISTENT_DEFINER,APPLICATION_PASSWORD_ADMIN,AUDIT_ABORT_EXEMPT,AUDIT_ADMIN,"
    "AUTHENTICATION_POLICY_ADMIN,BACKUP_ADMIN,BINLOG_ADMIN,BINLOG_ENCRYPTION_ADMIN,"
    "CLONE_ADMIN,CONNECTION_ADMIN,ENCRYPTION_KEY_ADMIN,FIREWALL_EXEMPT,"
    "FLUSH_OPTIMIZER_COSTS,FLUSH_PRIVILEGES,FLUSH_STATUS,FLUSH_TABLES,"
    "FLUSH_USER_RESOURCES,GROUP_REPLICATION_ADMIN,GROUP_REPLICATION_STREAM,"
    "INNODB_REDO_LOG_ARCHIVE,INNODB_REDO_LOG_ENABLE,OPTIMIZE_LOCAL_TABLE,"
    "PASSWORDLESS_USER_ADMIN,PERSIST_RO_VARIABLES_ADMIN,REPLICATION_APPLIER,"
    "REPLICATION_SLAVE_ADMIN,RESOURCE_GROUP_ADMIN,RESOURCE_GROUP_USER,ROLE_ADMIN,"
    "SENSITIVE_VARIABLES_OBSERVER,SERVICE_CONNECTION_ADMIN,SESSION_VARIABLES_ADMIN,"
    "SET_ANY_DEFINER,SHOW_ROUTINE,SYSTEM_USER,SYSTEM_VARIABLES_ADMIN,TABLE_ENCRYPTION_ADMIN,"
    "TELEMETRY_LOG_ADMIN,TRANSACTION_GTID_TAG,XA_RECOVER_ADMIN ON *.* TO `root`@`%` WITH "
    "GRANT OPTION",
};

static const char *const show_binary_log_file_name = "binlog.000001";
static const char *const show_binary_log_placeholder_position = "4";
static const char *const show_binary_log_placeholder_file_size = "4";

struct show_privileges_row {
    const char *privilege;
    const char *context;
    const char *comment;
};

static const struct show_privileges_row show_privileges_rows[] = {
    {"Alter", "Tables", "To alter the table"},
    {"Alter routine", "Functions,Procedures", "To alter or drop stored functions/procedures"},
    {"Create", "Databases,Tables,Indexes", "To create new databases and tables"},
    {"Create routine", "Databases", "To use CREATE FUNCTION/PROCEDURE"},
    {"Create role", "Server Admin", "To create new roles"},
    {"Create temporary tables", "Databases", "To use CREATE TEMPORARY TABLE"},
    {"Create view", "Tables", "To create new views"},
    {"Create user", "Server Admin", "To create new users"},
    {"Delete", "Tables", "To delete existing rows"},
    {"Drop", "Databases,Tables", "To drop databases, tables, and views"},
    {"Drop role", "Server Admin", "To drop roles"},
    {"Event", "Server Admin", "To create, alter, drop and execute events"},
    {"Execute", "Functions,Procedures", "To execute stored routines"},
    {"File", "File access on server", "To read and write files on the server"},
    {"Grant option",
     "Databases,Tables,Functions,Procedures",
     "To give to other users those privileges you possess"},
    {"Index", "Tables", "To create or drop indexes"},
    {"Insert", "Tables", "To insert data into tables"},
    {"Lock tables", "Databases", "To use LOCK TABLES (together with SELECT privilege)"},
    {"Process", "Server Admin", "To view the plain text of currently executing queries"},
    {"Proxy", "Server Admin", "To make proxy user possible"},
    {"References", "Databases,Tables", "To have references on tables"},
    {"Reload", "Server Admin", "To reload or refresh tables, logs and privileges"},
    {"Replication client", "Server Admin", "To ask where the slave or master servers are"},
    {"Replication slave", "Server Admin", "To read binary log events from the master"},
    {"Select", "Tables", "To retrieve rows from table"},
    {"Show databases", "Server Admin", "To see all databases with SHOW DATABASES"},
    {"Show view", "Tables", "To see views with SHOW CREATE VIEW"},
    {"Shutdown", "Server Admin", "To shut down the server"},
    {"Super", "Server Admin", "To use KILL thread, SET GLOBAL, CHANGE REPLICATION SOURCE, etc."},
    {"Trigger", "Tables", "To use triggers"},
    {"Create tablespace", "Server Admin", "To create/alter/drop tablespaces"},
    {"Update", "Tables", "To update existing rows"},
    {"Usage", "Server Admin", "No privileges - allow connect only"},
    {"AUDIT_ABORT_EXEMPT", "Server Admin", ""},
    {"FIREWALL_EXEMPT", "Server Admin", ""},
    {"OPTIMIZE_LOCAL_TABLE", "Server Admin", ""},
    {"ALLOW_NONEXISTENT_DEFINER", "Server Admin", ""},
    {"SET_ANY_DEFINER", "Server Admin", ""},
    {"SENSITIVE_VARIABLES_OBSERVER", "Server Admin", ""},
    {"AUTHENTICATION_POLICY_ADMIN", "Server Admin", ""},
    {"GROUP_REPLICATION_STREAM", "Server Admin", ""},
    {"FLUSH_PRIVILEGES", "Server Admin", ""},
    {"XA_RECOVER_ADMIN", "Server Admin", ""},
    {"CONNECTION_ADMIN", "Server Admin", ""},
    {"CLONE_ADMIN", "Server Admin", ""},
    {"ENCRYPTION_KEY_ADMIN", "Server Admin", ""},
    {"INNODB_REDO_LOG_ARCHIVE", "Server Admin", ""},
    {"SESSION_VARIABLES_ADMIN", "Server Admin", ""},
    {"APPLICATION_PASSWORD_ADMIN", "Server Admin", ""},
    {"REPLICATION_SLAVE_ADMIN", "Server Admin", ""},
    {"BACKUP_ADMIN", "Server Admin", ""},
    {"GROUP_REPLICATION_ADMIN", "Server Admin", ""},
    {"SYSTEM_VARIABLES_ADMIN", "Server Admin", ""},
    {"BINLOG_ADMIN", "Server Admin", ""},
    {"PERSIST_RO_VARIABLES_ADMIN", "Server Admin", ""},
    {"TRANSACTION_GTID_TAG", "Server Admin", ""},
    {"PASSWORDLESS_USER_ADMIN", "Server Admin", ""},
    {"ROLE_ADMIN", "Server Admin", ""},
    {"INNODB_REDO_LOG_ENABLE", "Server Admin", ""},
    {"RESOURCE_GROUP_USER", "Server Admin", ""},
    {"BINLOG_ENCRYPTION_ADMIN", "Server Admin", ""},
    {"SERVICE_CONNECTION_ADMIN", "Server Admin", ""},
    {"SHOW_ROUTINE", "Server Admin", ""},
    {"RESOURCE_GROUP_ADMIN", "Server Admin", ""},
    {"SYSTEM_USER", "Server Admin", ""},
    {"TABLE_ENCRYPTION_ADMIN", "Server Admin", ""},
    {"TELEMETRY_LOG_ADMIN", "Server Admin", ""},
    {"FLUSH_STATUS", "Server Admin", ""},
    {"REPLICATION_APPLIER", "Server Admin", ""},
    {"FLUSH_OPTIMIZER_COSTS", "Server Admin", ""},
    {"AUDIT_ADMIN", "Server Admin", ""},
    {"FLUSH_USER_RESOURCES", "Server Admin", ""},
    {"FLUSH_TABLES", "Server Admin", ""},
};

enum table_maintenance_operation {
    TABLE_MAINTENANCE_ANALYZE = 0,
    TABLE_MAINTENANCE_CHECK = 1,
    TABLE_MAINTENANCE_OPTIMIZE = 2,
    TABLE_MAINTENANCE_REPAIR = 3,
};

struct planned_lock_tables {
    struct mylite_session_table_lock *locks;
    size_t lock_count;
};

struct table_maintenance_target {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char display_name[(MYLITE_CATALOG_IDENTIFIER_CAPACITY * 2) + 2];
    bool missing_schema;
    bool missing_table;
    bool unsupported_kind;
};

enum mylite_statement_transaction_kind {
    MYLITE_STATEMENT_TRANSACTION_NONE = 0,
    MYLITE_STATEMENT_TRANSACTION_DIRECT = 1,
    MYLITE_STATEMENT_TRANSACTION_SAVEPOINT = 2,
};

struct mylite_statement_transaction {
    enum mylite_statement_transaction_kind kind;
    bool active;
};

struct transaction_characteristics {
    bool has_isolation;
    bool has_access_mode;
    bool has_consistent_snapshot;
    enum mylite_transaction_isolation isolation;
    enum mylite_transaction_access_mode access_mode;
};

struct savepoint_control_sql_request {
    const char *prefix;
    const char *sqlite_name;
};

struct user_savepoint_values {
    const char *name;
    const char *sqlite_name;
};

struct planned_select_source;
struct planned_exists_subquery;
struct planned_in_subquery;

struct select_source_context {
    const struct table_name_resolution *source;
    char alias[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool has_alias;
    const struct planned_select_source *sources;
    size_t source_count;
};

struct select_index_hint_lookup_context {
    const char *name;
    bool exact_match;
    size_t prefix_match_count;
};

enum temporal_text_kind {
    TEMPORAL_TEXT_NORMAL = 0,
    TEMPORAL_TEXT_FULL_ZERO = 1,
    TEMPORAL_TEXT_PARTIAL_ZERO = 2,
    TEMPORAL_TEXT_ALLOW_INVALID_CANDIDATE = 3,
    TEMPORAL_TEXT_INVALID = 4,
    TEMPORAL_TEXT_DEFERRED = 5,
};

enum temporal_storage_truncation {
    TEMPORAL_STORAGE_TRUNCATION_NONE = 0,
    TEMPORAL_STORAGE_TRUNCATION_NOTE = 1,
    TEMPORAL_STORAGE_TRUNCATION_WARNING = 2,
};

struct temporal_predicate_normalization_input;

enum planned_index_type_option {
    PLANNED_INDEX_TYPE_DEFAULT = 0,
    PLANNED_INDEX_TYPE_BTREE = 1,
    PLANNED_INDEX_TYPE_HASH = 2,
    PLANNED_INDEX_TYPE_RTREE = 3,
};

struct planned_column {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char logical_type_storage[MYLITE_CATALOG_TYPE_NAME_CAPACITY];
    char physical_type_storage[MYLITE_CATALOG_TYPE_NAME_CAPACITY];
    const char *logical_type;
    const char *physical_type;
    enum mylite_sql_ast_nullability nullability;
    bool is_nullable;
    bool is_visible;
    bool is_primary_key;
    bool is_unique_key;
    bool is_auto_increment;
    bool is_serial_alias;
    bool on_update_current_timestamp;
    bool has_text_length_argument;
    uint64_t text_length_argument;
    const struct mylite_sql_ast_node *default_node;
    enum mylite_catalog_column_default_kind default_kind;
    int64_t default_integer;
    char default_text[MYLITE_CATALOG_DEFAULT_TEXT_CAPACITY];
    char character_set_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char comment[MYLITE_CATALOG_COLUMN_COMMENT_CAPACITY];
    bool is_generated;
    enum mylite_catalog_generated_column_kind generated_kind;
    const struct mylite_sql_ast_node *generation_expression_node;
    char generation_expression[MYLITE_CATALOG_GENERATION_EXPRESSION_CAPACITY];
    char sqlite_generation_expression[MYLITE_CATALOG_GENERATION_EXPRESSION_CAPACITY];
};

struct planned_secondary_index_part {
    size_t column_index;
    bool has_prefix_length;
    int64_t prefix_length;
    enum mylite_catalog_index_sort_direction sort_direction;
};

struct planned_secondary_index {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct planned_secondary_index_part *parts;
    size_t part_count;
    size_t part_capacity;
    int64_t index_id;
    char physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    enum mylite_catalog_index_kind kind;
    bool is_unique;
    bool is_visible;
    char comment[MYLITE_CATALOG_INDEX_COMMENT_CAPACITY];
    bool show_create_explicit_btree;
    bool uses_hash_index_type;
};

struct secondary_index_part_nodes {
    const struct mylite_sql_ast_node *prefix_node;
    const struct mylite_sql_ast_node *direction_node;
};

struct planned_primary_key_part {
    size_t column_index;
    enum mylite_catalog_index_sort_direction sort_direction;
};

struct planned_foreign_key_part {
    size_t child_column_index;
    struct mylite_catalog_column_descriptor parent_column;
};

struct planned_foreign_key {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char child_index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char update_rule[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char delete_rule[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct planned_foreign_key_part *parts;
    size_t part_count;
    size_t part_capacity;
    int64_t foreign_key_id;
    struct mylite_catalog_table_descriptor parent_table;
    struct mylite_catalog_index_descriptor parent_index;
    int64_t child_index_id;
    size_t child_secondary_index_index;
    bool child_index_is_primary;
    bool has_explicit_name;
};

struct planned_check_constraint {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char check_clause[MYLITE_CATALOG_CHECK_CLAUSE_CAPACITY];
    char sqlite_expression[MYLITE_CATALOG_CHECK_CLAUSE_CAPACITY];
    int64_t check_constraint_id;
    int64_t generated_ordinal;
    int64_t ordinal_position;
    bool is_enforced;
    bool name_is_generated;
};

struct create_table_check_constraint_definition {
    const struct mylite_sql_ast_node *check_constraint;
    const struct mylite_sql_ast_node *inline_enforcement;
    const struct planned_column *inline_column;
    size_t inline_column_index;
};

enum check_expression_render_work_item_kind {
    CHECK_EXPRESSION_RENDER_NODE = 0,
    CHECK_EXPRESSION_RENDER_TEXT = 1,
    CHECK_EXPRESSION_RENDER_CHAR = 2,
    CHECK_EXPRESSION_RENDER_OPERATOR = 3,
};

struct check_expression_render_work_item {
    enum check_expression_render_work_item_kind kind;
    const struct mylite_sql_ast_node *node;
    bool require_boolean;
    const char *text;
    char character;
    enum mylite_sql_ast_operator operator_kind;
};

enum generated_expression_render_work_item_kind {
    GENERATED_EXPRESSION_RENDER_NODE = 0,
    GENERATED_EXPRESSION_RENDER_TEXT = 1,
    GENERATED_EXPRESSION_RENDER_CHAR = 2,
};

struct generated_expression_render_work_item {
    enum generated_expression_render_work_item_kind kind;
    const struct mylite_sql_ast_node *node;
    const char *text;
    char character;
};

struct check_expression_render_context {
    struct mylite_db *database;
    const struct planned_create_table *plan;
    const struct planned_column *inline_column;
    size_t inline_column_index;
    const char *alter_check_constraint_name;
    struct mylite_dynamic_string *check_clause;
    struct mylite_dynamic_string *sqlite_expression;
};

struct generated_expression_render_context {
    struct mylite_db *database;
    const struct planned_create_table *plan;
    size_t generated_column_index;
    struct mylite_dynamic_string *generation_expression;
    struct mylite_dynamic_string *sqlite_expression;
};

struct planned_create_table {
    struct table_name_resolution target;
    struct planned_column *columns;
    size_t column_count;
    bool has_primary_key;
    struct planned_primary_key_part *primary_key_parts;
    size_t primary_key_part_count;
    size_t primary_key_part_capacity;
    int64_t primary_key_index_id;
    char primary_key_physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char primary_key_comment[MYLITE_CATALOG_INDEX_COMMENT_CAPACITY];
    bool primary_key_show_create_explicit_btree;
    bool primary_key_uses_hash_index_type;
    struct planned_secondary_index *secondary_indexes;
    size_t secondary_index_count;
    size_t secondary_index_capacity;
    struct planned_foreign_key *foreign_keys;
    size_t foreign_key_count;
    size_t foreign_key_capacity;
    struct planned_check_constraint *check_constraints;
    size_t check_constraint_count;
    size_t check_constraint_capacity;
    int64_t auto_increment_next;
    char default_charset[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char default_collation[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char comment[MYLITE_CATALOG_TABLE_COMMENT_CAPACITY];
    char row_format_option[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int64_t key_block_size;
    int64_t pack_keys;
    int64_t checksum;
    int64_t stats_persistent;
    int64_t stats_auto_recalc;
    int64_t stats_sample_pages;
    bool suppress_spatial_index_warnings;
};

struct schema_default_option_validation {
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char first_charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char unsupported_charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char unsupported_collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool has_charset;
    bool has_collation;
};

struct temporary_index_descriptor_positions {
    size_t index;
    size_t index_column;
};

struct planned_create_table_like {
    struct planned_create_table create_table;
    struct table_name_resolution source;
    struct mylite_catalog_table_descriptor source_table;
};

struct planned_rename_table {
    struct table_name_resolution source;
    struct table_name_resolution target;
};

struct planned_rename_table_statement {
    struct planned_rename_table *pairs;
    size_t pair_count;
};

struct zero_temporal_default_warning {
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
};

struct zero_temporal_default_warnings {
    struct zero_temporal_default_warning *items;
    size_t count;
    size_t capacity;
};

struct zero_temporal_default_warning_filter {
    int64_t skipped_column_id;
};

struct planned_alter_table_add_column {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct planned_column column;
    struct mylite_catalog_column_descriptor *columns;
    struct zero_temporal_default_warnings zero_temporal_default_warnings;
    size_t column_count;
    size_t target_column_index;
    bool changes_position;
    bool adds_inline_primary_key;
    bool adds_inline_unique_key;
    char after_column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
};

struct alter_table_action_statement_view {
    struct mylite_sql_ast_node table_proxy;
    struct mylite_sql_ast_node statement;
};

struct alter_table_multi_added_column {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
};

struct alter_table_multi_action_state {
    struct alter_table_multi_added_column *added_columns;
    size_t added_column_count;
    size_t added_column_capacity;
};

struct alter_table_multi_default_target_view {
    const struct mylite_sql_ast_node *table_node;
    const struct mylite_sql_ast_node *action;
};

struct alter_table_multi_auto_increment_key_lookup {
    const struct loaded_index_info *indexes;
    size_t index_count;
    int64_t column_id;
};

struct planned_alter_table_add_primary_key {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct loaded_index_part *parts;
    size_t part_count;
    size_t part_capacity;
    const char *rowid_alias;
    char comment[MYLITE_CATALOG_INDEX_COMMENT_CAPACITY];
    bool show_create_explicit_btree;
    bool uses_hash_index_type;
};

struct planned_alter_table_add_index {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct loaded_index_part *parts;
    size_t part_count;
    size_t part_capacity;
    char index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const char *rowid_alias;
    enum mylite_catalog_index_kind kind;
    bool is_unique;
    bool is_visible;
    char comment[MYLITE_CATALOG_INDEX_COMMENT_CAPACITY];
    bool show_create_explicit_btree;
    bool uses_hash_index_type;
    int64_t affected_rows;
};

struct planned_alter_table_add_foreign_key {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char child_index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char update_rule[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char delete_rule[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_catalog_table_descriptor parent_table;
    struct mylite_catalog_index_descriptor parent_index;
    struct mylite_catalog_index_descriptor child_index;
    struct loaded_foreign_key_part *parts;
    size_t part_count;
    size_t part_capacity;
    struct planned_alter_table_add_index child_index_plan;
    bool create_child_index;
    bool has_explicit_name;
};

enum planned_alter_table_check_constraint_action {
    PLANNED_ALTER_TABLE_CHECK_ADD = 0,
    PLANNED_ALTER_TABLE_CHECK_DROP = 1,
    PLANNED_ALTER_TABLE_CHECK_ALTER = 2,
};

struct planned_alter_table_check_constraint {
    enum planned_alter_table_check_constraint_action action;
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct planned_create_table rebuild;
    int64_t target_check_constraint_id;
    size_t check_constraint_index;
    bool has_check_constraint_index;
    bool target_enforced;
    bool catalog_change_required;
    bool physical_rebuild_required;
    bool validating_rebuild;
    int64_t affected_rows;
};

struct planned_alter_table_drop_foreign_key {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct loaded_foreign_key_info foreign_key;
};

enum planned_alter_table_drop_constraint_kind {
    PLANNED_ALTER_TABLE_DROP_CONSTRAINT_NONE = 0,
    PLANNED_ALTER_TABLE_DROP_CONSTRAINT_PRIMARY_KEY = 1,
    PLANNED_ALTER_TABLE_DROP_CONSTRAINT_UNIQUE_INDEX = 2,
    PLANNED_ALTER_TABLE_DROP_CONSTRAINT_FOREIGN_KEY = 3,
    PLANNED_ALTER_TABLE_DROP_CONSTRAINT_CHECK = 4,
};

struct planned_drop_index {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct loaded_index_info index;
    int64_t affected_rows;
};

struct planned_rename_index {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct loaded_index_info index;
    char new_index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
};

struct index_option_nodes {
    const struct mylite_sql_ast_node *index_type_node;
    const struct mylite_sql_ast_node *option_list_node;
};

struct alter_table_add_index_nodes {
    const struct mylite_sql_ast_node *index_name_node;
    struct index_option_nodes options;
    const struct mylite_sql_ast_node *part_list;
    enum mylite_catalog_index_kind kind;
    bool is_unique;
};

struct create_index_nodes {
    const struct mylite_sql_ast_node *index_name_node;
    const struct mylite_sql_ast_node *table_name_node;
    struct index_option_nodes options;
    const struct mylite_sql_ast_node *part_list_node;
    enum mylite_catalog_index_kind kind;
};

struct planned_index_options {
    enum planned_index_type_option type;
    bool is_visible;
    char comment[MYLITE_CATALOG_INDEX_COMMENT_CAPACITY];
};

struct primary_key_definition_nodes {
    struct index_option_nodes options;
    const struct mylite_sql_ast_node *part_list;
};

struct create_table_secondary_index_definition_nodes {
    const struct mylite_sql_ast_node *index_name_node;
    struct index_option_nodes options;
    const struct mylite_sql_ast_node *part_list;
};

struct create_table_secondary_index_first_part {
    size_t column_index;
    size_t part_count;
};

struct foreign_key_definition_nodes {
    const struct mylite_sql_ast_node *constraint_name_node;
    const struct mylite_sql_ast_node *index_name_node;
    const struct mylite_sql_ast_node *child_parts_node;
    const struct mylite_sql_ast_node *parent_table_node;
    const struct mylite_sql_ast_node *parent_parts_node;
    const struct mylite_sql_ast_node *action_list_node;
};

struct foreign_key_part_name {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
};

struct foreign_key_column_names {
    struct foreign_key_part_name *child_parts;
    struct foreign_key_part_name *parent_parts;
    size_t child_part_count;
    size_t parent_part_count;
};

struct alter_table_add_foreign_key_child_metadata {
    struct mylite_catalog_column_descriptor *columns;
    struct loaded_index_info *indexes;
    struct loaded_foreign_key_info *foreign_keys;
    size_t column_count;
    size_t index_count;
    size_t foreign_key_count;
};

struct drop_index_nodes {
    const struct mylite_sql_ast_node *table_name_node;
    const struct mylite_sql_ast_node *index_name_node;
};

struct planned_alter_table_drop_primary_key {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_index_descriptor primary_key;
    struct loaded_index_part *parts;
    size_t part_count;
    int64_t affected_rows;
};

struct planned_alter_table_drop_constraint {
    enum planned_alter_table_drop_constraint_kind kind;
    struct planned_alter_table_drop_primary_key primary_key;
    struct planned_drop_index unique_index;
    struct planned_alter_table_drop_foreign_key foreign_key;
    struct planned_alter_table_check_constraint check_constraint;
};

struct alter_table_drop_constraint_context {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor *columns;
    struct loaded_index_info *indexes;
    struct loaded_foreign_key_info *foreign_keys;
    struct loaded_check_constraint_info *check_constraints;
    size_t column_count;
    size_t index_count;
    size_t foreign_key_count;
    size_t check_constraint_count;
};

struct alter_table_drop_constraint_resolution {
    enum planned_alter_table_drop_constraint_kind kind;
    size_t match_count;
    size_t unique_index;
};

struct planned_alter_table_auto_increment {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor column;
    int64_t requested_next;
    int64_t effective_next;
    bool has_auto_increment_column;
    bool is_temporary;
};

struct planned_drop_column_index_update {
    struct mylite_catalog_index_descriptor index;
    struct loaded_index_part *remaining_parts;
    size_t remaining_part_count;
    int64_t removed_ordinal_position;
    bool drops_index;
};

struct planned_alter_table_drop_column {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor column;
    struct planned_drop_column_index_update *index_updates;
    size_t index_update_count;
    size_t index_update_capacity;
    const char *rowid_alias;
    int64_t affected_rows;
    size_t column_count;
    bool removes_one_part_primary_key;
};

struct planned_alter_table_rename_column {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor column;
    char new_column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool is_noop;
};

struct planned_alter_table_modify_column {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor original_column;
    struct planned_column column;
    char lookup_column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    size_t column_index;
    size_t target_column_index;
    bool has_position;
    bool position_first;
    bool changes_position;
    char after_column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool is_noop;
    bool is_metadata_only;
    bool checks_duplicate_replacement;
    bool reports_rebuild_row_count;
    bool adds_inline_primary_key;
    bool adds_inline_unique_key;
    const char *unsupported_object_message;
    const char *rowid_alias_message;
    const char *integer_support_message;
    const char *row_count_overflow_message;
    const char *failure_message;
    const char *rowid_alias;
    struct loaded_index_info *indexes;
    struct zero_temporal_default_warnings zero_temporal_default_warnings;
    size_t index_count;
    int64_t affected_rows;
};

struct planned_alter_table_set_default {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor original_column;
    struct planned_column column;
    struct zero_temporal_default_warnings zero_temporal_default_warnings;
};

struct planned_alter_table_drop_default {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor column;
};

struct planned_alter_table_column_visibility {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor column;
    bool is_visible;
};

struct planned_alter_table_index_visibility {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct loaded_index_info index;
    bool is_visible;
    int64_t affected_rows;
};

struct planned_alter_table_default_charset_collation {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    char default_charset[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char default_collation[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool changes_descriptor;
};

struct planned_alter_table_convert_character_set {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    char default_charset[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char default_collation[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool changes_descriptor;
};

struct planned_alter_table_comment {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    char comment[MYLITE_CATALOG_TABLE_COMMENT_CAPACITY];
};

struct planned_alter_schema_default_charset_collation {
    struct mylite_catalog_schema_descriptor schema;
    char default_charset[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char default_collation[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool changes_descriptor;
};

enum planned_select_order_direction {
    PLANNED_SELECT_ORDER_DEFAULT = 0,
    PLANNED_SELECT_ORDER_ASC = 1,
    PLANNED_SELECT_ORDER_DESC = 2,
};

struct planned_alter_table_order_by_item {
    struct mylite_catalog_column_descriptor column;
    enum planned_select_order_direction direction;
};

struct planned_alter_table_order_by {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    struct planned_alter_table_order_by_item *items;
    size_t item_count;
    int64_t affected_rows;
};

struct planned_alter_table_force {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
};

struct planned_alter_table_key_maintenance {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    int64_t affected_rows;
};

struct planned_truncate_table {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
};

struct planned_drop_schema_table {
    char physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
};

struct planned_drop_schema {
    struct mylite_catalog_schema_descriptor schema;
    struct planned_drop_schema_table *tables;
    size_t table_count;
    size_t table_capacity;
    size_t object_count;
};

struct planned_value {
    bool is_null;
    bool is_text;
    bool is_blob;
    bool is_real;
    int64_t integer;
    double real;
    char *text;
    size_t text_length;
};

enum integer_expression_default_text_work_item_kind {
    INTEGER_EXPRESSION_DEFAULT_TEXT_NODE = 0,
    INTEGER_EXPRESSION_DEFAULT_TEXT_BYTES = 1,
};

struct integer_expression_default_text_work_item {
    enum integer_expression_default_text_work_item_kind kind;
    const struct mylite_sql_ast_node *expression;
    const char *text;
};

struct integer_expression_default_text_stack {
    struct integer_expression_default_text_work_item *items;
    size_t count;
    size_t capacity;
};

struct bit_blob_value {
    uint64_t width;
    uint64_t magnitude;
};

struct varchar_text_validation {
    const char *text;
    size_t text_length;
    size_t row_number;
};

struct char_text_conversion {
    char *text;
    size_t text_length;
    size_t row_number;
};

struct utf8_prefix_request {
    const char *text;
    size_t text_length;
    size_t character_limit;
};

struct utf8_byte_prefix_request {
    const char *text;
    size_t text_length;
    size_t byte_limit;
};

struct insert_select_string_validation {
    const struct mylite_catalog_column_descriptor *source_column;
    const struct mylite_catalog_column_descriptor *target_column;
    size_t row_number;
    bool adjust_value;
};

struct text_family_type_info {
    const char *logical_type;
    const char *display_type;
    uint64_t maximum_length;
};

struct binary_string_type_info {
    const char *logical_type;
    const char *display_type;
    uint64_t maximum_length;
    bool fixed_length;
    bool blob_family;
};

struct decimal_type_info {
    uint64_t precision;
    uint64_t scale;
    bool is_unsigned;
};

enum approximate_type_class {
    APPROXIMATE_TYPE_FLOAT = 1,
    APPROXIMATE_TYPE_DOUBLE = 2,
};

struct approximate_type_info {
    enum approximate_type_class type_class;
    bool is_unsigned;
};

struct decimal_literal_parts {
    size_t integer_end;
    size_t fraction_start;
    size_t fraction_end;
    size_t integer_digit_start;
};

struct decimal_digit_buffer {
    char *digits;
    size_t digit_count;
    size_t digit_capacity;
    bool discarded_nonzero;
};

struct integer_column_range {
    uint64_t positive_max;
    uint64_t negative_abs_max;
};

struct integer_logical_type_range_request {
    const char *logical_type;
    const char *unsupported_message;
};

struct generated_constraint_columns {
    const struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
};

enum planned_update_assignment_value_kind {
    PLANNED_UPDATE_ASSIGNMENT_VALUE = 0,
    PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC = 1,
    PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL = 2,
};

enum {
    update_date_interval_parameter_count = 4,
};

enum update_arithmetic_range_condition {
    UPDATE_ARITHMETIC_RANGE_ANY_NON_NULL = 0,
    UPDATE_ARITHMETIC_RANGE_GREATER_THAN = 1,
    UPDATE_ARITHMETIC_RANGE_LESS_THAN = 2,
};

enum update_arithmetic_range_error_kind {
    UPDATE_ARITHMETIC_RANGE_ERROR_COLUMN = 0,
    UPDATE_ARITHMETIC_RANGE_ERROR_SIGNED_BIGINT = 1,
    UPDATE_ARITHMETIC_RANGE_ERROR_UNSIGNED_BIGINT = 2,
};

struct update_arithmetic_range_check {
    enum update_arithmetic_range_condition condition;
    int64_t threshold;
    enum update_arithmetic_range_error_kind error_kind;
};

struct mapped_integer_type {
    enum mylite_sql_ast_integer_type type;
    int is_unsigned;
    bool has_display_width;
    bool is_bool_alias;
    uint64_t display_width;
};

struct planned_insert_row {
    struct planned_value *values;
    bool generated_auto_increment;
};

struct insert_auto_increment_mode_counts {
    size_t generated_count;
    size_t explicit_count;
};

struct insert_execution_counters {
    int64_t affected_rows;
    int64_t auto_increment_next_after_rows;
    int64_t first_inserted_generated_auto_increment;
    bool inserted_generated_auto_increment;
};

enum planned_insert_duplicate_update_value_kind {
    PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_LITERAL = 0,
    PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_VALUES_REFERENCE = 1,
    PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_SAME_COLUMN_ARITHMETIC = 2,
};

struct planned_insert_duplicate_update_assignment {
    size_t assignment_column_index;
    const struct mylite_sql_ast_node *assignment_value_node;
    enum planned_insert_duplicate_update_value_kind value_kind;
    size_t values_column_index;
    enum mylite_sql_ast_operator arithmetic_operator;
    const struct mylite_sql_ast_node *arithmetic_delta_node;
    bool generated_default_noop;
};

struct planned_insert_duplicate_update {
    bool has_clause;
    struct planned_insert_duplicate_update_assignment *assignments;
    size_t assignment_count;
    size_t key_index;
    bool has_key;
};

struct insert_duplicate_projected_row_inputs {
    const struct planned_value *current_values;
    const struct planned_value *assignment_values;
};

struct insert_duplicate_arithmetic_result_request {
    const struct mylite_catalog_column_descriptor *column;
    size_t row_number;
    enum mylite_sql_ast_operator operator_kind;
    int64_t current_value;
    uint64_t delta;
};

struct insert_duplicate_arithmetic_range_error {
    const struct mylite_catalog_column_descriptor *column;
    size_t row_number;
    enum update_arithmetic_range_error_kind error_kind;
};

struct planned_insert {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    struct loaded_index_info *indexes;
    size_t index_count;
    struct loaded_foreign_key_info *foreign_keys;
    size_t foreign_key_count;
    struct planned_insert_row *rows;
    size_t row_count;
    bool ignore_errors;
    bool replace_existing_rows;
    bool has_primary_key;
    size_t primary_key_column_index;
    bool has_auto_increment;
    bool no_auto_value_on_zero;
    size_t auto_increment_column_index;
    struct integer_column_range auto_increment_range;
    int64_t auto_increment_next;
    int64_t auto_increment_next_after_statement;
    int64_t first_generated_auto_increment;
    bool generated_auto_increment;
    struct planned_insert_duplicate_update duplicate_update;
};

struct planned_load_data_infile {
    struct planned_insert insert;
    char *file_path;
    size_t file_path_length;
    size_t *target_indexes;
    size_t target_count;
    uint64_t ignore_line_count;
};

struct load_data_field {
    char *text;
    size_t text_length;
    bool is_null;
};

struct load_data_row {
    struct load_data_field *fields;
    size_t field_count;
    size_t field_capacity;
};

enum planned_select_predicate_kind {
    PLANNED_SELECT_PREDICATE_NONE = 0,
    PLANNED_SELECT_PREDICATE_COMPARISON = 1,
    PLANNED_SELECT_PREDICATE_IS_NULL = 2,
    PLANNED_SELECT_PREDICATE_AND = 3,
    PLANNED_SELECT_PREDICATE_OR = 4,
    PLANNED_SELECT_PREDICATE_NOT = 5,
    PLANNED_SELECT_PREDICATE_BETWEEN = 6,
    PLANNED_SELECT_PREDICATE_IN = 7,
    PLANNED_SELECT_PREDICATE_IS_BOOLEAN = 8,
    PLANNED_SELECT_PREDICATE_XOR = 9,
    PLANNED_SELECT_PREDICATE_EXISTS = 10,
    PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH = 11,
    PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON = 12,
    PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL = 13,
};

struct planned_select_predicate_node {
    enum planned_select_predicate_kind kind;
    enum mylite_sql_ast_operator operator_kind;
    struct mylite_catalog_column_descriptor column;
    size_t column_source_index;
    struct planned_value value;
    bool value_is_column_reference;
    struct mylite_catalog_column_descriptor value_column;
    size_t value_column_source_index;
    struct planned_value upper_value;
    struct planned_value *values;
    size_t value_count;
    bool compare_date_as_datetime;
    size_t left_index;
    size_t right_index;
    bool like_uses_escape;
    struct planned_exists_subquery *exists_subquery;
    struct planned_in_subquery *in_subquery;
    struct planned_row_scalar_expression *row_scalar_expression;
    struct planned_row_scalar_expression *row_scalar_value_expression;
};

struct planned_select_predicate {
    struct planned_select_predicate_node *nodes;
    size_t node_count;
    size_t root_index;
    bool has_root;
    bool qualify_column_references;
};

struct planned_row_scalar_expression;

enum planned_select_order_item_kind {
    PLANNED_SELECT_ORDER_ITEM_COLUMN = 0,
    PLANNED_SELECT_ORDER_ITEM_FIELD = 1,
    PLANNED_SELECT_ORDER_ITEM_RAND = 2,
};

struct select_predicate_plan_options {
    bool allow_exists;
    bool allow_in_subquery;
    bool allow_column_reference_rhs;
    bool allow_same_scope_column_reference_rhs;
    bool allow_date_format_numeric_predicate;
    const struct select_source_context *outer_source_context;
    const struct mylite_catalog_column_descriptor *outer_columns;
    size_t outer_column_count;
};

enum predicate_work_item_kind {
    PREDICATE_WORK_NODE = 0,
    PREDICATE_WORK_DEPRECATED_AND_WARNING = 1,
    PREDICATE_WORK_DEPRECATED_OR_WARNING = 2,
    PREDICATE_WORK_FINISH_LOGICAL = 3,
    PREDICATE_WORK_FINISH_NOT = 4,
};

struct predicate_work_item {
    enum predicate_work_item_kind kind;
    const struct mylite_sql_ast_node *node;
    enum mylite_sql_ast_operator operator_kind;
};

enum predicate_sql_work_item_kind {
    PREDICATE_SQL_WORK_NODE = 0,
    PREDICATE_SQL_WORK_OPERATOR = 1,
    PREDICATE_SQL_WORK_CLOSE = 2,
    PREDICATE_SQL_WORK_TEXT = 3,
};

struct predicate_sql_work_item {
    enum predicate_sql_work_item_kind kind;
    size_t node_index;
    enum mylite_sql_ast_operator operator_kind;
    const char *text;
};

struct planned_select_order_item {
    enum planned_select_order_item_kind kind;
    enum planned_select_order_direction direction;
    struct mylite_catalog_column_descriptor column;
    size_t column_source_index;
    struct planned_row_scalar_expression *expression;
};

struct planned_select_order {
    bool has_order;
    enum planned_select_order_direction direction;
    struct mylite_catalog_column_descriptor column;
    size_t column_source_index;
    bool qualify_column_reference;
    struct planned_select_order_item *items;
    size_t item_count;
};

struct select_order_ast_item_nodes {
    const struct mylite_sql_ast_node *order_key;
    const struct mylite_sql_ast_node *direction;
};

struct planned_select_limit {
    bool has_limit;
    int64_t row_count;
    bool has_offset;
    int64_t offset;
};

enum planned_grouped_having_kind {
    PLANNED_GROUPED_HAVING_NONE = 0,
    PLANNED_GROUPED_HAVING_COMPARISON = 1,
    PLANNED_GROUPED_HAVING_IS_NULL = 2,
};

enum planned_grouped_having_operand {
    PLANNED_GROUPED_HAVING_OPERAND_NONE = 0,
    PLANNED_GROUPED_HAVING_OPERAND_GROUP_COLUMN = 1,
    PLANNED_GROUPED_HAVING_OPERAND_AGGREGATE = 2,
};

struct planned_grouped_having {
    enum planned_grouped_having_kind kind;
    enum planned_grouped_having_operand operand;
    enum mylite_sql_ast_operator operator_kind;
    struct planned_value value;
    size_t group_index;
    size_t aggregate_index;
};

struct planned_diagnostics_show_limit {
    bool has_limit;
    uint64_t row_count;
    bool has_offset;
    uint64_t offset;
};

struct planned_select_source {
    struct table_name_resolution source;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    char alias[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool has_alias;
};

struct planned_exists_subquery {
    bool has_table_source;
    struct planned_select_source source;
    struct planned_select_predicate predicate;
    struct planned_select_limit limit;
};

struct planned_in_subquery {
    struct planned_select_source source;
    struct mylite_catalog_column_descriptor column;
    struct planned_select_predicate predicate;
};

struct planned_select_join_condition {
    bool has_condition;
    bool right_value_is_row_scalar_expression;
    struct mylite_catalog_column_descriptor left_column;
    struct mylite_catalog_column_descriptor right_column;
    size_t left_source_index;
    size_t right_source_index;
    struct planned_row_scalar_expression *right_row_scalar_expression;
};

struct planned_select {
    struct table_name_resolution source;
    struct mylite_catalog_table_descriptor table;
    char source_alias[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool source_has_alias;
    struct planned_select_source *sources;
    size_t source_count;
    struct mylite_catalog_column_descriptor *columns;
    const struct mylite_sql_ast_node **column_aliases;
    size_t *column_source_indexes;
    size_t column_count;
    bool is_distinct;
    bool calc_found_rows;
    bool requires_source_alias;
    enum mylite_sql_ast_join_kind join_kind;
    struct planned_select_join_condition join_condition;
    enum mylite_sql_ast_join_kind *join_kinds;
    struct planned_select_join_condition *join_conditions;
    size_t join_count;
    struct planned_select_predicate predicate;
    struct planned_select_order order;
    struct planned_select_limit limit;
};

struct select_optional_clauses {
    const struct mylite_sql_ast_node *where_clause;
    const struct mylite_sql_ast_node *having_clause;
    const struct mylite_sql_ast_node *order_clause;
    const struct mylite_sql_ast_node *limit_clause;
};

struct joined_select_ast {
    const struct mylite_sql_ast_node *statement;
    const struct mylite_sql_ast_node *from_clause;
};

struct joined_select_temp_nodes {
    const struct mylite_sql_ast_node **source_nodes;
    size_t source_node_count;
    const struct mylite_sql_ast_node **join_condition_nodes;
    size_t join_condition_node_count;
};

enum planned_row_scalar_expression_kind {
    PLANNED_ROW_SCALAR_EXPRESSION_NONE = 0,
    PLANNED_ROW_SCALAR_EXPRESSION_VALUE = 1,
    PLANNED_ROW_SCALAR_EXPRESSION_COLUMN = 2,
    PLANNED_ROW_SCALAR_EXPRESSION_CONCAT = 3,
    PLANNED_ROW_SCALAR_EXPRESSION_FIELD = 4,
    PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT = 5,
    PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL = 6,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH = 7,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE = 8,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE = 9,
    PLANNED_ROW_SCALAR_EXPRESSION_HEX = 10,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM = 11,
    PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT = 12,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH = 13,
    PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET = 14,
    PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS = 15,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID = 16,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT = 17,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE = 18,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT = 19,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY = 20,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT = 21,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE = 22,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH = 23,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS = 24,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH = 25,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE = 26,
    PLANNED_ROW_SCALAR_EXPRESSION_IF = 27,
    PLANNED_ROW_SCALAR_EXPRESSION_IFNULL = 28,
    PLANNED_ROW_SCALAR_EXPRESSION_COALESCE = 29,
    PLANNED_ROW_SCALAR_EXPRESSION_NULLIF = 30,
    PLANNED_ROW_SCALAR_EXPRESSION_ISNULL = 31,
    PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP = 32,
    PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE = 33,
    PLANNED_ROW_SCALAR_EXPRESSION_UNHEX = 34,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT = 35,
    PLANNED_ROW_SCALAR_EXPRESSION_GREATEST = 36,
    PLANNED_ROW_SCALAR_EXPRESSION_LEAST = 37,
    PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX = 38,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING = 39,
    PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF = 40,
    PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND = 41,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE = 42,
    PLANNED_ROW_SCALAR_EXPRESSION_CHAR = 43,
    PLANNED_ROW_SCALAR_EXPRESSION_STRCMP = 44,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE = 45,
    PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME = 46,
    PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME = 47,
    PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS = 48,
    PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE = 49,
    PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME = 50,
    PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC = 51,
    PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT = 52,
    PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF = 53,
    PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID = 54,
    PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN = 55,
    PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID = 56,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE = 57,
    PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF = 58,
    PLANNED_ROW_SCALAR_EXPRESSION_UUID = 59,
    PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64 = 60,
    PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64 = 61,
    PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE = 62,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS = 63,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT = 64,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET = 65,
    PLANNED_ROW_SCALAR_EXPRESSION_RAND = 66,
    PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL = 67,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE = 68,
    PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP = 69,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK = 70,
    PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION = 71,
    PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR = 72,
    PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR = 73,
    PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE = 74,
    PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX = 75,
    PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION = 76,
    PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD = 77,
    PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF = 78,
    PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ = 79,
    PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING = 80,
    PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY = 81,
    PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE = 82,
    PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE = 83,
};

enum {
    planned_row_scalar_timestampdiff_argument_count = 5,
};

enum planned_window_function_kind {
    PLANNED_WINDOW_FUNCTION_NONE = 0,
    PLANNED_WINDOW_FUNCTION_ROW_NUMBER = 1,
    PLANNED_WINDOW_FUNCTION_RANK = 2,
    PLANNED_WINDOW_FUNCTION_DENSE_RANK = 3,
    PLANNED_WINDOW_FUNCTION_PERCENT_RANK = 4,
    PLANNED_WINDOW_FUNCTION_CUME_DIST = 5,
    PLANNED_WINDOW_FUNCTION_NTILE = 6,
    PLANNED_WINDOW_FUNCTION_LAG = 7,
    PLANNED_WINDOW_FUNCTION_LEAD = 8,
    PLANNED_WINDOW_FUNCTION_FIRST_VALUE = 9,
    PLANNED_WINDOW_FUNCTION_LAST_VALUE = 10,
    PLANNED_WINDOW_FUNCTION_NTH_VALUE = 11,
};

struct window_function_argument_count_request {
    enum planned_window_function_kind kind;
    size_t argument_count;
};

enum planned_row_scalar_field_domain {
    PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE = 0,
    PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING = 1,
    PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER = 2,
};

enum planned_row_scalar_conversion_kind {
    PLANNED_ROW_SCALAR_CONVERSION_NONE = 0,
    PLANNED_ROW_SCALAR_CONVERSION_BINARY = 1,
    PLANNED_ROW_SCALAR_CONVERSION_CHAR = 2,
    PLANNED_ROW_SCALAR_CONVERSION_SIGNED = 3,
    PLANNED_ROW_SCALAR_CONVERSION_UNSIGNED = 4,
    PLANNED_ROW_SCALAR_CONVERSION_USING_BINARY = 5,
    PLANNED_ROW_SCALAR_CONVERSION_USING_CHARSET = 6,
};

enum {
    planned_row_scalar_conversion_step_capacity = 8,
};

struct planned_row_scalar_conversion_step {
    enum planned_row_scalar_conversion_kind kind;
    bool ascii_only;
    const char *charset;
    const char *collation;
};

struct string_slice_right_bounds {
    size_t text_length;
    uint64_t requested_length;
    size_t character_count;
};

struct substring_text_bounds {
    const char *text;
    size_t text_length;
    int64_t position;
    bool has_length;
    int64_t requested_length;
};

struct planned_row_scalar_expression {
    enum planned_row_scalar_expression_kind kind;
    enum planned_row_scalar_field_domain field_domain;
    enum planned_string_length_function_kind string_length_kind;
    enum planned_string_case_function_kind string_case_kind;
    enum planned_string_codepoint_function_kind string_codepoint_kind;
    enum planned_string_trim_function_kind string_trim_kind;
    enum planned_string_slice_function_kind string_slice_kind;
    enum planned_string_search_function_kind string_search_kind;
    enum planned_string_padding_function_kind string_padding_kind;
    enum planned_string_bitmask_function_kind string_bitmask_kind;
    enum planned_regexp_string_function_kind regexp_string_kind;
    enum planned_row_scalar_conversion_kind conversion_kind;
    enum planned_window_function_kind window_function_kind;
    enum mylite_temporal_extract_kind temporal_extract_kind;
    enum mylite_sql_ast_operator arithmetic_operator;
    struct planned_value value;
    enum mylite_json_sql_value_kind json_value_kind;
    enum planned_json_mutation_kind json_mutation_kind;
    bool regexp_case_sensitive;
    bool like_uses_escape;
    bool case_has_else;
    bool has_rand_seed;
    uint32_t rand_seed;
    bool window_has_partition;
    bool window_has_order;
    bool conversion_ascii_only;
    const char *conversion_charset;
    const char *conversion_collation;
    struct planned_row_scalar_conversion_step
        conversion_steps[planned_row_scalar_conversion_step_capacity];
    size_t conversion_step_count;
    enum planned_select_order_direction window_order_direction;
    struct mylite_catalog_column_descriptor window_partition_column;
    struct mylite_catalog_column_descriptor window_order_column;
    struct mylite_catalog_column_descriptor column;
    bool column_cast_as_integer;
    bool column_has_source_index;
    size_t column_source_index;
    struct planned_row_scalar_expression *arguments;
    size_t argument_count;
};

struct row_scalar_uuid_column_expression_request {
    const struct mylite_sql_ast_node *function_expression;
    const struct mylite_sql_ast_node *value_argument;
    const struct mylite_sql_ast_node *swap_argument;
};

struct row_scalar_integer_arithmetic_plan_frame {
    const struct mylite_sql_ast_node *expression;
    struct planned_row_scalar_expression *out_expression;
};

struct row_scalar_integer_arithmetic_plan_stack {
    struct row_scalar_integer_arithmetic_plan_frame *items;
    size_t count;
    size_t capacity;
};

enum row_scalar_integer_arithmetic_append_phase {
    ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_ENTER = 0,
    ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_COMMA = 1,
    ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_CLOSE = 2,
};

struct row_scalar_integer_arithmetic_append_frame {
    const struct planned_row_scalar_expression *expression;
    enum row_scalar_integer_arithmetic_append_phase phase;
};

struct row_scalar_integer_arithmetic_append_stack {
    struct row_scalar_integer_arithmetic_append_frame *items;
    size_t count;
    size_t capacity;
};

struct row_scalar_integer_arithmetic_bind_stack {
    const struct planned_row_scalar_expression **items;
    size_t count;
    size_t capacity;
};

struct planned_row_scalar_select_item {
    struct planned_row_scalar_expression expression;
    const struct mylite_sql_ast_node *source_expression;
    const struct mylite_sql_ast_node *alias;
};

struct planned_row_scalar_exists_filter {
    bool has_filter;
    bool has_scalar_predicate;
    bool negate;
    struct planned_exists_subquery subquery;
    struct planned_select_predicate predicate;
};

struct planned_row_scalar_select {
    bool has_source;
    struct table_name_resolution source;
    char source_alias[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_catalog_table_descriptor table;
    struct planned_select_source *sources;
    size_t source_count;
    enum mylite_sql_ast_join_kind *join_kinds;
    struct planned_select_join_condition *join_conditions;
    size_t join_count;
    struct planned_row_scalar_select_item *items;
    size_t item_count;
    bool source_has_alias;
    struct planned_row_scalar_exists_filter tableless_filter;
    struct planned_select_predicate predicate;
    struct planned_select_order order;
    struct planned_select_limit limit;
};

enum planned_count_having_select_projection_kind {
    PLANNED_COUNT_HAVING_SELECT_DESCRIPTOR = 0,
    PLANNED_COUNT_HAVING_SELECT_ROW_SCALAR = 1,
};

struct planned_count_having_select {
    enum planned_count_having_select_projection_kind projection_kind;
    enum mylite_sql_ast_operator operator_kind;
    struct planned_value value;
    struct planned_select descriptor_projection;
    struct planned_row_scalar_select row_projection;
};

struct row_scalar_select_clauses {
    const struct mylite_sql_ast_node *where_clause;
    const struct mylite_sql_ast_node *order_clause;
    const struct mylite_sql_ast_node *limit_clause;
};

enum planned_insert_select_source_kind {
    PLANNED_INSERT_SELECT_SOURCE_TABLE = 0,
    PLANNED_INSERT_SELECT_SOURCE_ROW_SCALAR = 1,
    PLANNED_INSERT_SELECT_SOURCE_COMPOUND = 2,
};

enum planned_insert_select_compound_branch_kind {
    PLANNED_INSERT_SELECT_COMPOUND_BRANCH_TABLE = 0,
    PLANNED_INSERT_SELECT_COMPOUND_BRANCH_ROW_SCALAR = 1,
};

struct planned_insert_select_compound_branch {
    enum planned_insert_select_compound_branch_kind kind;
    enum mylite_sql_ast_union_modifier modifier;
    struct planned_select source;
    struct planned_row_scalar_select row_source;
};

struct planned_insert_select_compound_source {
    struct planned_insert_select_compound_branch *branches;
    size_t branch_count;
};

struct planned_insert_select {
    struct planned_insert target;
    enum planned_insert_select_source_kind source_kind;
    struct planned_select source;
    struct planned_row_scalar_select row_source;
    struct planned_insert_select_compound_source compound_source;
    size_t *target_indexes;
    size_t target_count;
};

struct insert_select_execution_statements {
    sqlite3_stmt *select_statement;
    sqlite3_stmt *insert_statement;
};

struct insert_select_table_execution {
    char temporary_table_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char *materialize_sql;
    char *validation_sql;
    char *insert_sql;
    char *drop_sql;
    struct mylite_statement_transaction transaction;
    struct insert_execution_counters counters;
    bool temporary_table_created;
};

struct alter_table_modify_copy_statements {
    sqlite3_stmt *select_statement;
    sqlite3_stmt *insert_statement;
};

struct planned_create_table_select {
    struct planned_create_table create_table;
    struct planned_select source;
    enum mylite_sql_ast_select_locking_clause source_locking_clause;
};

enum planned_count_function {
    PLANNED_COUNT_NONE = 0,
    PLANNED_COUNT_STAR = 1,
    PLANNED_COUNT_COLUMN = 2,
    PLANNED_COUNT_LITERAL = 3,
    PLANNED_COUNT_DISTINCT_COLUMN = 4,
};

struct planned_count {
    bool has_source;
    const struct mylite_sql_ast_node *expression;
    const struct mylite_sql_ast_node *alias;
    enum planned_count_function function;
    struct table_name_resolution source;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor count_column;
    struct planned_value count_literal;
    struct planned_select_predicate predicate;
};

enum planned_count_expression_aggregate_item_kind {
    PLANNED_COUNT_EXPRESSION_AGGREGATE_STAR = 0,
    PLANNED_COUNT_EXPRESSION_AGGREGATE_NULLIF_PREDICATE = 1,
};

struct planned_count_expression_aggregate_item {
    enum planned_count_expression_aggregate_item_kind kind;
    const struct mylite_sql_ast_node *expression;
    const struct mylite_sql_ast_node *alias;
    struct planned_select_predicate predicate;
};

struct planned_count_expression_aggregate {
    struct planned_select source;
    struct planned_count_expression_aggregate_item *items;
    size_t item_count;
};

struct planned_count_source_nodes {
    const struct mylite_sql_ast_node *from_clause;
    const struct mylite_sql_ast_node *where_clause;
    const struct mylite_sql_ast_node *count_expression;
};

enum planned_column_aggregate_function {
    PLANNED_COLUMN_AGGREGATE_NONE = 0,
    PLANNED_COLUMN_AGGREGATE_MIN = 1,
    PLANNED_COLUMN_AGGREGATE_MAX = 2,
    PLANNED_COLUMN_AGGREGATE_SUM = 3,
    PLANNED_COLUMN_AGGREGATE_AVG = 4,
    PLANNED_COLUMN_AGGREGATE_BIT_AND = 5,
    PLANNED_COLUMN_AGGREGATE_BIT_OR = 6,
    PLANNED_COLUMN_AGGREGATE_BIT_XOR = 7,
    PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT = 8,
};

struct planned_column_aggregate {
    const struct mylite_sql_ast_node *expression;
    const struct mylite_sql_ast_node *alias;
    enum planned_column_aggregate_function function;
    struct table_name_resolution source;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor aggregate_column;
    struct planned_select_order aggregate_order;
    bool has_separator;
    struct planned_value separator;
    struct planned_select_predicate predicate;
};

enum planned_grouped_aggregate_function {
    PLANNED_GROUPED_AGGREGATE_NONE = 0,
    PLANNED_GROUPED_AGGREGATE_COUNT_STAR = 1,
    PLANNED_GROUPED_AGGREGATE_COUNT_COLUMN = 2,
    PLANNED_GROUPED_AGGREGATE_MIN = 3,
    PLANNED_GROUPED_AGGREGATE_MAX = 4,
    PLANNED_GROUPED_AGGREGATE_SUM = 5,
    PLANNED_GROUPED_AGGREGATE_AVG = 6,
    PLANNED_GROUPED_AGGREGATE_BIT_AND = 7,
    PLANNED_GROUPED_AGGREGATE_BIT_OR = 8,
    PLANNED_GROUPED_AGGREGATE_BIT_XOR = 9,
    PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT = 10,
    PLANNED_GROUPED_AGGREGATE_ANY_VALUE = 11,
};

enum { grouped_aggregate_max_results = 16 };

enum { grouped_aggregate_max_group_keys = 4 };

struct planned_grouped_aggregate_item {
    const struct mylite_sql_ast_node *expression;
    const struct mylite_sql_ast_node *alias;
    enum planned_grouped_aggregate_function function;
    struct mylite_catalog_column_descriptor aggregate_column;
    size_t aggregate_column_source_index;
    struct planned_select_order aggregate_order;
    bool has_separator;
    struct planned_value separator;
};

struct planned_grouped_key {
    const struct mylite_sql_ast_node *expression;
    const struct mylite_sql_ast_node *alias;
    struct mylite_catalog_column_descriptor column;
    size_t column_source_index;
};

struct grouped_alias_group_resolution {
    const struct mylite_sql_ast_node *select_list;
    const struct mylite_sql_ast_node *group_key;
    const struct select_source_context *source_context;
    const struct mylite_catalog_column_descriptor *table_columns;
    size_t table_column_count;
};

struct planned_grouped_projection {
    const struct mylite_sql_ast_node *expression;
    const struct mylite_sql_ast_node *alias;
    struct mylite_catalog_column_descriptor column;
    size_t column_source_index;
};

struct planned_grouped_aggregate {
    struct planned_grouped_key *groups;
    size_t group_count;
    struct planned_grouped_projection *projections;
    size_t projection_count;
    struct planned_grouped_aggregate_item *aggregates;
    size_t aggregate_count;
    struct table_name_resolution source;
    struct mylite_catalog_table_descriptor table;
    struct planned_select_source *sources;
    size_t source_count;
    enum mylite_sql_ast_join_kind join_kind;
    struct planned_select_join_condition join_condition;
    struct planned_select_predicate predicate;
    struct planned_grouped_having having;
    struct planned_select_order order;
    bool order_uses_aggregate;
    size_t order_aggregate_index;
    struct planned_select_limit limit;
};

struct planned_create_view {
    struct table_name_resolution target;
    struct planned_select source;
    char **column_names;
    size_t column_count;
    char *view_definition;
    char *show_create_sql;
};

struct grouped_aggregate_clauses {
    const struct mylite_sql_ast_node *where_clause;
    const struct mylite_sql_ast_node *group_clause;
    const struct mylite_sql_ast_node *having_clause;
    const struct mylite_sql_ast_node *order_clause;
    const struct mylite_sql_ast_node *limit_clause;
};

struct avg_accumulator {
    int64_t sum;
    int64_t count;
};

struct uint128_parts {
    uint64_t high;
    uint64_t low;
};

struct planned_show_create_table {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_view_descriptor view;
    struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    struct loaded_index_info *indexes;
    size_t index_count;
    struct loaded_foreign_key_info *foreign_keys;
    size_t foreign_key_count;
    struct loaded_check_constraint_info *check_constraints;
    size_t check_constraint_count;
    const struct mylite_execution_catalog_builtin_sys_view *builtin_sys_view;
    bool is_view;
    bool target_was_schema_qualified;
};

struct planned_delete {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct planned_select_source *sources;
    size_t source_count;
    enum mylite_sql_ast_join_kind join_kind;
    struct planned_select_join_condition join_condition;
    size_t target_source_index;
    size_t target_source_indexes[2];
    size_t target_source_count;
    struct planned_select_predicate predicate;
    struct planned_select_order order;
    struct planned_select_limit limit;
    const char *rowid_alias;
    bool is_joined;
};

struct planned_update_assignment {
    struct mylite_catalog_column_descriptor column;
    const struct mylite_sql_ast_node *value_node;
    struct planned_value value;
    bool generated_default_noop;
};

struct planned_update {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct planned_select_source *sources;
    size_t source_count;
    enum mylite_sql_ast_join_kind join_kind;
    struct planned_select_join_condition join_condition;
    size_t target_source_index;
    struct mylite_catalog_column_descriptor assignment_column;
    struct planned_update_assignment *assignments;
    size_t assignment_count;
    struct loaded_index_info *indexes;
    size_t index_count;
    const struct mylite_sql_ast_node *assignment_value_node;
    enum planned_update_assignment_value_kind assignment_value_kind;
    enum mylite_sql_ast_operator arithmetic_operator;
    const struct mylite_sql_ast_node *arithmetic_delta_node;
    uint64_t arithmetic_delta_magnitude;
    int64_t arithmetic_delta;
    enum mylite_date_interval_second_input_kind date_interval_input_kind;
    enum mylite_date_interval_unit date_interval_unit;
    int64_t date_interval_value;
    bool date_interval_is_null;
    bool date_interval_subtract;
    bool assignment_value_is_scalar_subquery;
    struct planned_select assignment_subquery;
    struct planned_value assignment_value;
    struct planned_value auto_update_value;
    struct planned_select_predicate predicate;
    struct planned_select_order order;
    struct planned_select_limit limit;
    struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    const char *rowid_alias;
    bool has_primary_key;
    size_t primary_key_column_index;
    int64_t primary_key_column_id;
    bool is_joined;
    bool low_priority;
    bool ignore_errors;
    bool generated_default_noop;
};

struct update_optional_clauses {
    const struct mylite_sql_ast_node *where_clause;
    const struct mylite_sql_ast_node *order_clause;
    const struct mylite_sql_ast_node *limit_clause;
};

struct update_unique_key_conflict_bind_request {
    const struct planned_update *executable_plan;
    const struct planned_update *plan;
    const struct loaded_index_info *index;
};

struct table_option_name_policy {
    const char *identifier_kind;
    const char *nul_message;
};

struct show_like_filter {
    bool has_pattern;
    char *pattern;
    size_t pattern_length;
};

enum show_variables_where_truth {
    SHOW_VARIABLES_WHERE_FALSE = 0,
    SHOW_VARIABLES_WHERE_TRUE = 1,
    SHOW_VARIABLES_WHERE_UNKNOWN = 2,
};

enum show_variables_where_column {
    SHOW_VARIABLES_WHERE_COLUMN_NONE = 0,
    SHOW_VARIABLES_WHERE_COLUMN_VARIABLE_NAME = 1,
    SHOW_VARIABLES_WHERE_COLUMN_VALUE = 2,
};

enum show_variables_where_eval_action {
    SHOW_VARIABLES_WHERE_VISIT = 1,
    SHOW_VARIABLES_WHERE_EVALUATE = 2,
};

struct show_variables_where_row {
    const char *variable_name;
    const char *value;
};

struct show_catalog_where_row {
    const char *const *columns;
    const char *const *values;
    size_t column_count;
};

struct show_variables_where_eval_frame {
    const struct mylite_sql_ast_node *node;
    enum show_variables_where_eval_action action;
};

struct show_variables_where_frame_stack {
    struct show_variables_where_eval_frame *items;
    size_t count;
    size_t capacity;
};

struct show_variables_where_truth_stack {
    enum show_variables_where_truth *items;
    size_t count;
    size_t capacity;
};

struct show_like_pattern_item_request {
    const char *pattern;
    size_t pattern_length;
    size_t pattern_index;
    char value_byte;
    bool case_sensitive;
    bool backslash_escapes;
};

struct result_column_metadata_context {
    struct primary_key_info primary_key;
    struct loaded_index_info *indexes;
    size_t index_count;
    bool has_single_source_metadata;
};

struct find_check_constraint_name_context {
    const char *name;
    bool found;
};

struct check_constraint_name_collision_context {
    const char *name;
    int64_t excluded_table_id;
    bool found;
};

struct validate_parent_foreign_keys_context {
    struct mylite_db *database;
    int rc;
};

struct foreign_key_exists_context {
    bool found;
};

struct foreign_key_index_exists_context {
    int64_t index_id;
    bool found;
};

struct loaded_index_column_lookup {
    const struct loaded_index_info *indexes;
    size_t index_count;
    int64_t skipped_index_id;
    int64_t column_id;
};

struct nonprimary_index_presence_context {
    bool has_nonprimary_index;
};

struct nonprimary_index_count_context {
    int64_t count;
};

struct show_tables_context {
    struct mylite_db *database;
    mylite_result *result;
    const struct show_like_filter *filter;
    const struct mylite_sql_ast_node *where_clause;
    const char *table_name_column;
    bool is_full;
};

struct show_tables_filter_nodes {
    const struct mylite_sql_ast_node *schema;
    const struct mylite_sql_ast_node *like;
    const struct mylite_sql_ast_node *where;
};

struct show_schema_resolution {
    struct mylite_catalog_schema_descriptor schema;
    const struct mylite_execution_catalog_builtin_schema_table_directory *builtin_directory;
};

struct table_status_values {
    int64_t row_count;
    char row_count_text[integer_text_capacity];
    char average_row_length_text[integer_text_capacity];
    char index_length_text[integer_text_capacity];
    char auto_increment_text[integer_text_capacity];
    char create_time_text[datetime_text_length + 1U];
    char update_time_text[datetime_text_length + 1U];
    char row_format_text[sizeof("Compressed")];
    char create_options_text[table_status_create_options_capacity];
    const char *index_length;
    const char *auto_increment;
    const char *create_time;
    const char *update_time;
    const char *row_format;
    const char *create_options;
};

struct show_table_status_context {
    struct mylite_db *database;
    mylite_result *result;
    const struct show_like_filter *filter;
    const struct mylite_sql_ast_node *where_clause;
};

struct show_table_status_filter_nodes {
    const struct mylite_sql_ast_node *schema;
    const struct mylite_sql_ast_node *like;
    const struct mylite_sql_ast_node *where;
};

struct show_table_status_where_comparison {
    int value;
};

struct show_table_status_where_operand {
    const char *value;
    bool case_sensitive;
    bool numeric;
    struct session_scalar_cell cell;
};

struct show_databases_where_comparison {
    int value;
};

struct show_metadata_regexp_messages {
    const char *literal;
    const char *nul;
    const char *ascii;
    const char *unsupported;
};

struct show_index_filter_nodes {
    const struct mylite_sql_ast_node *table;
    const struct mylite_sql_ast_node *schema;
    const struct mylite_sql_ast_node *where;
};

struct show_columns_context {
    struct mylite_db *database;
    mylite_result *result;
    const struct show_like_filter *filter;
    const struct mylite_sql_ast_node *where_clause;
    const struct mylite_catalog_table_descriptor *table;
    const struct primary_key_info *primary_key;
    const struct loaded_index_info *indexes;
    size_t index_count;
    bool full;
};

struct show_columns_target_nodes {
    const struct mylite_sql_ast_node *table;
    const struct mylite_sql_ast_node *schema;
};

struct show_columns_filter_nodes {
    const struct mylite_sql_ast_node *table;
    const struct mylite_sql_ast_node *schema;
    const struct mylite_sql_ast_node *like;
    const struct mylite_sql_ast_node *where;
};

struct information_schema_character_metadata {
    const char *character_maximum_length;
    const char *character_octet_length;
    const char *character_set_name;
    const char *collation_name;
};

struct information_schema_innodb_column_type_info {
    int64_t mtype;
    int64_t prtype;
    int64_t length;
};

struct information_schema_innodb_character_type_request {
    int64_t mtype;
    int64_t mysql_type_code;
};

struct information_schema_row_set {
    const struct mylite_execution_catalog_table_definition *definition;
    char ***rows;
    size_t row_count;
};

struct information_schema_row_order_pair {
    size_t left_row;
    size_t right_row;
};

struct information_schema_predicate_value {
    char *text;
    bool is_null;
    bool is_numeric;
};

struct enum_label_descriptor {
    const char *text;
    size_t text_length;
    size_t character_length;
};

struct enum_type_info {
    char decoded_storage[MYLITE_CATALOG_TYPE_NAME_CAPACITY];
    size_t decoded_length;
    struct enum_label_descriptor labels[enum_label_count_capacity];
    size_t label_count;
    size_t max_label_character_length;
};

struct set_type_info {
    char decoded_storage[MYLITE_CATALOG_TYPE_NAME_CAPACITY];
    size_t decoded_length;
    struct enum_label_descriptor members[set_member_count_capacity];
    size_t member_count;
    size_t max_display_character_length;
};

struct set_predicate_member_text_request {
    struct mylite_dynamic_string *display;
    const struct set_type_info *info;
    char *text;
    size_t token_start;
    size_t token_end;
    size_t member_index;
    bool found_member;
};

enum enum_string_trailing_space_policy {
    ENUM_STRING_PRESERVE_TRAILING_SPACES,
    ENUM_STRING_TRIM_TRAILING_SPACES,
};

enum information_schema_predicate_eval_action {
    INFORMATION_SCHEMA_PREDICATE_VISIT = 0,
    INFORMATION_SCHEMA_PREDICATE_EVALUATE = 1,
};

enum information_schema_truth_value {
    INFORMATION_SCHEMA_TRUTH_FALSE = 0,
    INFORMATION_SCHEMA_TRUTH_TRUE = 1,
    INFORMATION_SCHEMA_TRUTH_UNKNOWN = 2,
};

struct information_schema_predicate_eval_frame {
    const struct mylite_sql_ast_node *node;
    enum information_schema_predicate_eval_action action;
};

struct information_schema_predicate_frame_stack {
    struct information_schema_predicate_eval_frame *items;
    size_t count;
    size_t capacity;
};

struct information_schema_truth_stack {
    enum information_schema_truth_value *items;
    size_t count;
    size_t capacity;
};

enum information_schema_projection_kind {
    INFORMATION_SCHEMA_PROJECTION_COLUMN = 0,
    INFORMATION_SCHEMA_PROJECTION_UNSIGNED_INTEGER_EXPRESSION = 1,
};

struct information_schema_query {
    const struct mylite_execution_catalog_table_definition *definition;
    char alias[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool has_alias;
    bool is_count_star;
    const struct mylite_sql_ast_node *count_expression;
    const struct mylite_sql_ast_node *count_alias;
    size_t *projection_indexes;
    enum information_schema_projection_kind *projection_kinds;
    const struct mylite_sql_ast_node **projection_expressions;
    char **projection_names;
    size_t projection_count;
    bool has_order;
    size_t order_index;
    enum planned_select_order_direction order_direction;
    struct planned_select_limit limit;
};

struct information_schema_catalog_context {
    struct mylite_db *database;
    struct information_schema_row_set *rows;
    const struct mylite_catalog_schema_descriptor *schema;
};

struct mysql_system_table_catalog_context {
    struct mylite_db *database;
    struct information_schema_row_set *rows;
    const struct mylite_execution_catalog_mysql_system_table *definition;
    const struct mylite_catalog_schema_descriptor *schema;
};

struct sys_schema_auto_increment_columns_context {
    struct mylite_db *database;
    struct information_schema_row_set *rows;
    const struct mylite_catalog_schema_descriptor *schema;
};

struct sys_schema_object_overview_group {
    char db[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char object_type[sizeof("INDEX (FULLTEXT)")];
    uint64_t count;
};

struct sys_schema_object_overview_context {
    struct mylite_db *database;
    struct information_schema_row_set *rows;
    struct sys_schema_object_overview_group *groups;
    size_t group_count;
    size_t group_capacity;
    const struct mylite_catalog_schema_descriptor *schema;
};

struct sys_schema_index_statistics_context {
    struct mylite_db *database;
    struct information_schema_row_set *rows;
    bool formatted_latency;
    const struct mylite_catalog_schema_descriptor *schema;
};

struct sys_schema_unused_indexes_context {
    struct mylite_db *database;
    struct information_schema_row_set *rows;
    const struct mylite_catalog_schema_descriptor *schema;
};

struct sys_schema_table_statistics_context {
    struct mylite_db *database;
    struct information_schema_row_set *rows;
    bool formatted_metrics;
    bool include_buffer_metrics;
    const struct mylite_catalog_schema_descriptor *schema;
};

struct sys_schema_redundant_indexes_context {
    struct mylite_db *database;
    struct information_schema_row_set *rows;
    const struct mylite_catalog_schema_descriptor *schema;
};

struct sys_flattened_key_row {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool non_unique;
    bool subpart_exists;
    char *index_columns;
};

struct sys_flattened_key_row_set {
    struct sys_flattened_key_row *rows;
    size_t count;
    size_t capacity;
};

struct information_schema_innodb_foreign_action_type_flags {
    int64_t cascade;
    int64_t set_null;
    int64_t no_action;
};

struct show_database_name {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
};

struct show_databases_context {
    struct mylite_db *database;
    struct show_database_name *names;
    size_t name_count;
    size_t name_capacity;
};

struct collect_drop_schema_tables_context {
    struct mylite_db *database;
    struct planned_drop_schema *plan;
};

struct string_bitmask_scalar_text_argument {
    struct mylite_string_bitmask_slice slice;
    struct session_scalar_cell cell;
    char *owned_text;
};

struct prepared_statement_expanded_sql {
    char *text;
    size_t text_size;
    size_t parameter_count;
};

enum scalar_integer_cast_target {
    SCALAR_INTEGER_CAST_SIGNED,
    SCALAR_INTEGER_CAST_UNSIGNED,
};

struct scalar_integer_cast_parse {
    bool is_negative;
    bool saw_digits;
    bool overflowed;
    bool has_truncated_integer_warning;
    uint64_t magnitude;
};

struct scalar_integer_cast_digit_source {
    const char *text;
    size_t offset;
    uint64_t limit;
};

struct scalar_integer_cast_messages {
    const char *unsupported;
    const char *signed_value;
    const char *embedded_nul;
};

struct scalar_text_conversion_messages {
    const char *unsupported;
    const char *signed_value;
    const char *string_unsupported;
    const char *embedded_nul;
};

struct field_scalar_argument {
    enum planned_row_scalar_field_domain domain;
    bool is_null;
    int64_t integer;
    bool has_numeric_real;
    double numeric_real;
    char *text;
    size_t text_length;
};

struct field_scalar_argument_list {
    const struct field_scalar_argument *values;
    size_t count;
};

struct greatest_least_scalar_selection {
    size_t argument_count;
    enum planned_row_scalar_field_domain domain;
    bool is_greatest;
};

struct scalar_arithmetic_operation {
    enum mylite_sql_ast_operator operator_kind;
    int64_t left;
    int64_t right;
};

struct scalar_comparison_operation {
    enum mylite_sql_ast_operator operator_kind;
    int64_t left;
    int64_t right;
};

enum scalar_arithmetic_eval_frame_kind {
    SCALAR_ARITHMETIC_EVAL_ENTER = 1,
    SCALAR_ARITHMETIC_EVAL_APPLY = 2,
    SCALAR_ARITHMETIC_EVAL_APPLY_UNARY = 3,
};

struct scalar_arithmetic_eval_frame {
    enum scalar_arithmetic_eval_frame_kind kind;
    const struct mylite_sql_ast_node *expression;
    enum mylite_sql_ast_operator operator_kind;
};

struct scalar_arithmetic_eval_stack {
    struct scalar_arithmetic_eval_frame *items;
    size_t count;
    size_t capacity;
};

struct scalar_arithmetic_value_stack {
    struct scalar_arithmetic_value *items;
    size_t count;
    size_t capacity;
};

enum scalar_bitwise_eval_frame_kind {
    SCALAR_BITWISE_EVAL_ENTER = 1,
    SCALAR_BITWISE_EVAL_APPLY = 2,
    SCALAR_BITWISE_EVAL_APPLY_UNARY = 3,
    SCALAR_BITWISE_EVAL_SHORT_CIRCUIT_OR_ENTER_RIGHT = 4,
};

struct scalar_bitwise_eval_frame {
    enum scalar_bitwise_eval_frame_kind kind;
    const struct mylite_sql_ast_node *expression;
    enum mylite_sql_ast_operator operator_kind;
};

struct scalar_bitwise_eval_stack {
    struct scalar_bitwise_eval_frame *items;
    size_t count;
    size_t capacity;
};

struct scalar_bitwise_value_stack {
    struct scalar_bitwise_value *items;
    size_t count;
    size_t capacity;
};

struct scalar_arithmetic_node_stack {
    const struct mylite_sql_ast_node **items;
    size_t count;
    size_t capacity;
};

enum scalar_comparison_eval_frame_kind {
    SCALAR_COMPARISON_EVAL_ENTER = 1,
    SCALAR_COMPARISON_EVAL_APPLY = 2,
    SCALAR_COMPARISON_EVAL_SHORT_CIRCUIT_OR_ENTER_RIGHT = 3,
};

struct scalar_comparison_eval_frame {
    enum scalar_comparison_eval_frame_kind kind;
    const struct mylite_sql_ast_node *expression;
    enum mylite_sql_ast_operator operator_kind;
};

struct scalar_comparison_eval_stack {
    struct scalar_comparison_eval_frame *items;
    size_t count;
    size_t capacity;
};

enum scalar_logical_eval_frame_kind {
    SCALAR_LOGICAL_EVAL_ENTER = 1,
    SCALAR_LOGICAL_EVAL_APPLY_NOT = 2,
    SCALAR_LOGICAL_EVAL_APPLY_COMPARISON = 3,
    SCALAR_LOGICAL_EVAL_COMPARISON_SHORT_CIRCUIT_OR_ENTER_RIGHT = 4,
    SCALAR_LOGICAL_EVAL_APPLY_LOGICAL = 5,
    SCALAR_LOGICAL_EVAL_LOGICAL_SHORT_CIRCUIT_OR_ENTER_RIGHT = 6,
    SCALAR_LOGICAL_EVAL_APPLY_IS = 7,
};

struct scalar_logical_eval_frame {
    enum scalar_logical_eval_frame_kind kind;
    const struct mylite_sql_ast_node *expression;
    enum mylite_sql_ast_operator operator_kind;
};

struct scalar_logical_eval_stack {
    struct scalar_logical_eval_frame *items;
    size_t count;
    size_t capacity;
};

enum if_eval_frame_kind {
    IF_EVAL_FRAME_IF = 1,
    IF_EVAL_FRAME_IFNULL = 2,
    IF_EVAL_FRAME_COALESCE = 3,
    IF_EVAL_FRAME_NULLIF = 4,
    IF_EVAL_FRAME_ISNULL = 5,
};

struct if_eval_frame {
    enum if_eval_frame_kind kind;
    const struct mylite_sql_ast_node *first_value;
    const struct mylite_sql_ast_node *second_value;
    struct session_scalar_cell first_cell;
};

struct if_eval_stack {
    struct if_eval_frame *items;
    size_t count;
    size_t capacity;
};

struct if_validation_stack {
    const struct mylite_sql_ast_node **items;
    size_t count;
    size_t capacity;
};

struct system_variable_component {
    char text[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool quoted;
};

enum set_system_variable_scope {
    SET_SYSTEM_VARIABLE_SCOPE_NONE = 0,
    SET_SYSTEM_VARIABLE_SCOPE_SESSION = 1,
    SET_SYSTEM_VARIABLE_SCOPE_LOCAL = 2,
    SET_SYSTEM_VARIABLE_SCOPE_GLOBAL = 3,
};

struct resolved_set_system_variable_target {
    enum mylite_execution_system_variable_kind kind;
    enum set_system_variable_scope scope;
    bool is_system_variable_target;
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
};

struct set_session_snapshot {
    uint64_t sql_mode;
    uint64_t auto_increment_increment;
    uint64_t auto_increment_offset;
    uint64_t sql_select_limit;
    uint64_t group_concat_max_len;
    uint64_t information_schema_stats_expiry;
    uint64_t wait_timeout;
    uint64_t interactive_timeout;
    int64_t timestamp_override;
    int time_zone_offset_minutes;
    enum mylite_transaction_isolation session_transaction_isolation;
    enum mylite_transaction_access_mode session_transaction_access_mode;
    enum mylite_transaction_isolation next_transaction_isolation;
    enum mylite_transaction_access_mode next_transaction_access_mode;
    bool sql_mode_is_placeholder;
    bool time_zone_is_placeholder;
    bool character_set_state_is_placeholder;
    bool system_variables_are_placeholder;
    bool big_tables;
    bool foreign_key_checks_enabled;
    bool sql_require_primary_key;
    bool has_next_transaction_isolation;
    bool has_next_transaction_access_mode;
    bool next_transaction_isolation_from_system_variable;
    bool next_transaction_access_mode_from_system_variable;
    bool has_timestamp_override;
    char time_zone[MYLITE_SESSION_TIME_ZONE_CAPACITY];
    char character_set_client[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    char character_set_connection[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    char character_set_results[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    char collation_connection[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    char sql_mode_text[MYLITE_SESSION_SQL_MODE_TEXT_CAPACITY];
    struct mylite_session_user_variable *user_variables;
    size_t user_variable_count;
    size_t user_variable_capacity;
    struct mylite_session_system_variable_override *system_variable_overrides;
    size_t system_variable_override_count;
    size_t system_variable_override_capacity;
};

static int execute_parsed_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_non_prepared_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_empty_statement(struct mylite_db *database, mylite_result **out_result);
static int execute_use_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_set_names_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_set_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_prepare_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_execute_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_deallocate_prepare_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_set_transaction_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_start_transaction_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_commit_statement(struct mylite_db *database, mylite_result **out_result);
static int execute_rollback_statement(struct mylite_db *database, mylite_result **out_result);
static int execute_savepoint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_rollback_to_savepoint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_release_savepoint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_lock_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_unlock_tables_statement(struct mylite_db *database, mylite_result **out_result);
static int plan_lock_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_lock_tables *out_plan
);
static int plan_lock_table_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target_node,
    struct planned_lock_tables *plan,
    size_t target_index
);
static int copy_lock_table_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias_node,
    struct mylite_session_table_lock *out_lock
);
static int set_lock_table_mode(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *mode_node,
    struct mylite_session_table_lock *out_lock
);
static int check_lock_table_duplicate_targets(
    struct mylite_db *database,
    const struct planned_lock_tables *plan,
    size_t target_index
);
static int apply_lock_tables_plan(struct mylite_db *database, struct planned_lock_tables *plan);
static void planned_lock_tables_deinit(struct planned_lock_tables *plan);
static void clear_session_table_locks(struct mylite_db *database);
static int execute_set_connection_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool allow_collation,
    mylite_result **out_result
);
static int validate_set_connection_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool allow_collation
);
static int apply_set_connection_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool allow_collation
);
static int validate_set_connection_character_set_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target
);
static int validate_set_names_collation_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target
);
static int set_session_connection_character_set(
    struct mylite_db *database,
    const char *collation_name
);
static int apply_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int apply_set_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment
);
static int apply_set_user_variable_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment
);
static int evaluate_user_variable_assignment_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct session_scalar_cell *out_cell,
    enum mylite_session_user_variable_value_kind *out_value_kind
);
static int copy_user_variable_source_text_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_session_user_variable_value_kind value_kind,
    struct session_scalar_cell *out_cell,
    enum mylite_session_user_variable_value_kind *out_value_kind
);
static int set_session_user_variable(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int session_user_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct session_scalar_cell *out_cell
);
static int session_user_variable_value_kind(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    enum mylite_session_user_variable_value_kind *out_value_kind
);
static int copy_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *buffer,
    size_t buffer_size
);
static int copy_unquoted_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char *buffer,
    size_t buffer_size
);
static int copy_quoted_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char quote,
    char *buffer,
    size_t buffer_size
);
static int copy_string_quoted_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char *buffer,
    size_t buffer_size
);
static int copy_identifier_quoted_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char quote,
    char *buffer,
    size_t buffer_size
);
static int append_user_variable_name_byte(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size,
    size_t *inout_length,
    char byte
);
static int validate_user_variable_name(
    struct mylite_db *database,
    const char *name,
    size_t name_length
);
static int set_illegal_user_variable_name_error(
    struct mylite_db *database,
    const char *name,
    size_t name_length
);
static void fold_user_variable_name(char *text);
static struct mylite_session_user_variable *find_session_user_variable(
    struct mylite_session_state *session,
    const char *name
);
static bool text_is_decimal_integer_literal(const char *text, size_t text_size);
static enum mylite_session_user_variable_value_kind infer_user_variable_value_kind(
    const struct mylite_sql_ast_node *value_node,
    const struct session_scalar_cell *value
);
static int ensure_session_user_variable_capacity(struct mylite_db *database);
static int set_session_system_variable_override(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    const char *value,
    size_t value_size
);
static const char *session_system_variable_override_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
);
static const char *session_system_variable_override_show_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
);
static struct mylite_session_system_variable_override *find_session_system_variable_override(
    struct mylite_session_state *session,
    enum mylite_execution_system_variable_kind kind
);
static int ensure_session_system_variable_override_capacity(struct mylite_db *database);
static int prepare_statement_source_sql(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source,
    char **out_sql,
    size_t *out_sql_size
);
static int copy_session_scalar_sql_text(
    struct mylite_db *database,
    const struct session_scalar_cell *value,
    const char *null_text,
    char **out_sql,
    size_t *out_sql_size
);
static int validate_prepared_statement_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    size_t *out_parameter_count
);
static int build_prepared_statement_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    const struct mylite_sql_ast_node *using_list,
    struct prepared_statement_expanded_sql *out_sql
);
static int execute_expanded_prepared_statement_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_result **out_result
);
static int append_execute_parameter_sql(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_sql_ast_node *variable
);
static int append_execute_string_literal(
    const struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_size
);
static bool prepared_statement_disallows_statement(const struct mylite_sql_ast_node *statement);
static int copy_prepared_statement_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *buffer,
    size_t buffer_size
);
static struct mylite_session_prepared_statement *find_session_prepared_statement(
    struct mylite_session_state *session,
    const char *name
);
static size_t find_session_prepared_statement_index(
    const struct mylite_session_state *session,
    const char *name
);
static void remove_session_prepared_statement_at(
    struct mylite_session_state *session,
    size_t index
);
static void deinit_session_prepared_statement(struct mylite_session_prepared_statement *statement);
static int ensure_session_prepared_statement_capacity(struct mylite_db *database);
static void set_prepared_statement_argument_count_error(struct mylite_db *database);
static void set_unknown_prepared_statement_error(
    struct mylite_db *database,
    const char *statement_name,
    const char *command_name
);
static void set_prepared_statement_command_unsupported_error(struct mylite_db *database);
static int copy_set_session_snapshot(
    struct mylite_db *database,
    struct set_session_snapshot *out_snapshot
);
static int copy_session_user_variables(
    struct mylite_db *database,
    const struct mylite_session_state *source,
    struct mylite_session_user_variable **out_variables
);
static int copy_session_system_variable_overrides(
    struct mylite_db *database,
    const struct mylite_session_state *source,
    struct mylite_session_system_variable_override **out_overrides
);
static void restore_set_session_snapshot(
    struct mylite_db *database,
    struct set_session_snapshot *snapshot
);
static void deinit_set_session_snapshot(struct set_session_snapshot *snapshot);
static void free_session_user_variables(
    struct mylite_session_user_variable *variables,
    size_t count
);
static void free_session_system_variable_overrides(
    struct mylite_session_system_variable_override *overrides,
    size_t count
);
static int apply_set_system_variable_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment
);
static int apply_set_system_variable_user_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_text_session_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_text_session_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value
);
static bool set_system_variable_is_text_session_state(
    enum mylite_execution_system_variable_kind kind
);
static int set_text_session_system_variable(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    const char *value,
    size_t value_size
);
static const char *text_session_system_variable_default_value(
    enum mylite_execution_system_variable_kind kind
);
static int copy_set_text_system_variable_node_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_execution_system_variable_kind kind,
    const char *variable_name,
    char **out_value,
    size_t *out_value_size
);
static int apply_set_session_placeholder_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_session_placeholder_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int apply_set_session_placeholder_boolean_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    bool value
);
static int copy_set_session_placeholder_node_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct resolved_set_system_variable_target *target,
    char **out_value,
    size_t *out_value_size
);
static int apply_set_server_identity_binary_log_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int apply_set_sql_mode_cell_value(
    struct mylite_db *database,
    const struct session_scalar_cell *value
);
static int apply_set_time_zone_cell_value(
    struct mylite_db *database,
    const struct session_scalar_cell *value
);
static bool reject_invalid_read_only_system_variable_cell_target(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
);
static int apply_set_sql_select_limit_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int apply_set_group_concat_max_len_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int apply_set_information_schema_stats_expiry_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int apply_set_big_tables_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int apply_set_timeout_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int parse_set_boolean_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    bool *out_value
);
static int resolve_set_system_variable_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target,
    struct resolved_set_system_variable_target *out_target
);
static int resolve_set_system_variable_identifier_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *scope_node,
    const struct mylite_sql_ast_node *name_node,
    struct resolved_set_system_variable_target *out_target
);
static int resolve_set_system_variable_system_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_node,
    struct resolved_set_system_variable_target *out_target
);
static int apply_set_foreign_key_checks_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_sql_require_primary_key_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_sql_require_primary_key_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int apply_set_big_tables_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int parse_set_big_tables_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    bool *out_value
);
static int parse_set_big_tables_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_value
);
static int finish_parse_set_big_tables_integer(
    struct mylite_db *database,
    const char *variable_name,
    uint64_t magnitude,
    bool negative,
    const char *value_text,
    bool *out_value
);
static const struct mylite_sql_ast_node *unwrap_big_tables_value_literal(
    const struct mylite_sql_ast_node *value_node,
    bool *out_negative
);
static void copy_big_tables_value_text(
    const struct mylite_sql_ast_node *value_node,
    char *buffer,
    size_t buffer_size
);
static int parse_set_foreign_key_checks_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    bool *out_value
);
static const struct mylite_sql_ast_node *unwrap_foreign_key_checks_value_literal(
    const struct mylite_sql_ast_node *value_node,
    bool *out_negative
);
static void copy_foreign_key_checks_value_text(
    const struct mylite_sql_ast_node *value_node,
    char *buffer,
    size_t buffer_size
);
static bool fixed_global_uint64_server_identity_system_variable_value(
    enum mylite_execution_system_variable_kind kind,
    uint64_t *out_value,
    const char **out_unsupported_message
);
static int validate_set_fixed_boolean_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    bool expected_value
);
static int apply_set_max_allowed_packet_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_transaction_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_auto_increment_step_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int parse_set_auto_increment_step_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
);
static const struct mylite_sql_ast_node *unwrap_auto_increment_step_value_literal(
    const struct mylite_sql_ast_node *value_node,
    bool *out_negative
);
static void copy_auto_increment_step_value_text(
    const struct mylite_sql_ast_node *value_node,
    char *buffer,
    size_t buffer_size
);
static int set_incorrect_system_variable_argument_type_error(
    struct mylite_db *database,
    const char *variable_name
);
static int append_truncated_incorrect_auto_increment_warning(
    struct mylite_db *database,
    const char *variable_name,
    const char *value_text
);
static int apply_set_sql_select_limit_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_group_concat_max_len_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_information_schema_stats_expiry_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_timeout_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_integer_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_timeout_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t value
);
static int parse_set_timeout_system_variable_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
);
static int parse_timeout_system_variable_integer(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
);
static int parse_set_sql_select_limit_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
);
static int parse_set_group_concat_max_len_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
);
static int parse_set_information_schema_stats_expiry_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
);
static int parse_information_schema_stats_expiry_integer(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
);
static int parse_set_sql_select_limit_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
);
static int parse_set_group_concat_max_len_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
);
static int parse_set_information_schema_stats_expiry_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
);
static int parse_set_timeout_system_variable_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
);
static int append_truncated_incorrect_system_variable_warning(
    struct mylite_db *database,
    const char *variable_name,
    const char *value_text
);
static int validate_set_fixed_uint64_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    uint64_t expected_value,
    const char *unsupported_message
);
static int apply_set_sql_mode_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_timestamp_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_time_zone_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node
);
static int copy_set_time_zone_value_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    char **out_value
);
static int set_session_time_zone(struct mylite_db *database, const char *text);
static int parse_time_zone_text(
    struct mylite_db *database,
    const char *text,
    char *canonical,
    size_t canonical_size,
    int *out_offset_minutes
);
static bool parse_time_zone_offset(
    const char *text,
    char *canonical,
    size_t canonical_size,
    int *out_offset_minutes
);
static void set_unknown_or_incorrect_time_zone_error(
    struct mylite_db *database,
    const char *value_text
);
static void set_time_zone_incorrect_argument_type_error(struct mylite_db *database);
static int parse_sql_mode_text(struct mylite_db *database, const char *text, uint64_t *out_modes);
static bool sql_mode_token_matches(const char *text, size_t length, const char *expected);
static void set_invalid_sql_mode_error(struct mylite_db *database, const char *text, size_t length);
static void set_system_variable_cant_be_set_value_error(
    struct mylite_db *database,
    const char *variable_name,
    const char *value_text
);
static int append_set_sql_mode_warnings(struct mylite_db *database, uint64_t modes);
static int rebuild_sql_mode_text(
    struct mylite_db *database,
    uint64_t modes,
    char *buffer,
    size_t buffer_size
);
static int set_session_sql_mode(struct mylite_db *database, uint64_t modes);
static bool session_sql_mode_has(const struct mylite_session_state *session, uint64_t mode);
static unsigned int lexer_modes_for_session_sql_mode(const struct mylite_session_state *session);
static bool system_variable_expression_has_global_scope(
    const struct mylite_sql_ast_node *expression
);
static bool foreign_key_checks_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
);
static uint64_t foreign_key_checks_system_variable_uint64_value(
    const struct mylite_db *database,
    bool global_scope
);
static const char *foreign_key_checks_system_variable_show_value(
    const struct mylite_db *database,
    bool global_scope
);
static uint64_t sql_require_primary_key_system_variable_uint64_value(
    const struct mylite_db *database,
    bool global_scope
);
static const char *sql_require_primary_key_system_variable_show_value(
    const struct mylite_db *database,
    bool global_scope
);
static uint64_t big_tables_system_variable_uint64_value(
    const struct mylite_db *database,
    bool global_scope
);
static const char *big_tables_system_variable_show_value(
    const struct mylite_db *database,
    bool global_scope
);
static int apply_set_transaction_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int collect_transaction_characteristics(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *list,
    struct transaction_characteristics *out_characteristics
);
static int apply_session_transaction_characteristics(
    struct mylite_db *database,
    const struct transaction_characteristics *characteristics
);
static int apply_next_transaction_characteristics(
    struct mylite_db *database,
    const struct transaction_characteristics *characteristics,
    bool from_system_variable
);
static int apply_transaction_system_variable_characteristics(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct transaction_characteristics *characteristics
);
static int parse_set_transaction_isolation_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_transaction_isolation *out_isolation
);
static int parse_set_transaction_read_only_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_transaction_access_mode *out_access_mode
);
static int decode_set_transaction_string_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const char *variable_name,
    char **out_value
);
static int copy_set_transaction_identifier_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    char *value,
    size_t value_size
);
static int set_transaction_variable_invalid_node_value_error(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node
);
static const char *transaction_isolation_value_text(enum mylite_transaction_isolation isolation);
static const char *transaction_read_only_scalar_text(
    enum mylite_transaction_access_mode access_mode
);
static const char *transaction_read_only_show_text(enum mylite_transaction_access_mode access_mode);
static const char *transaction_isolation_system_variable_value(
    const struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static const char *transaction_read_only_system_variable_value(
    const struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static enum mylite_transaction_access_mode current_transaction_access_mode(
    const struct mylite_db *database
);
static enum mylite_transaction_isolation current_transaction_isolation(
    const struct mylite_db *database
);
static enum mylite_transaction_access_mode effective_start_transaction_access_mode(
    const struct mylite_db *database,
    const struct transaction_characteristics *characteristics
);
static int reserve_consistent_snapshot_ignored_warning(struct mylite_db *database);
static int append_consistent_snapshot_ignored_warning(struct mylite_db *database);
static void clear_next_transaction_characteristics(struct mylite_db *database);
static void clear_active_transaction_characteristics(struct mylite_db *database);
static void clear_next_transaction_characteristics_before_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int prepare_statement_transaction_boundary(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static bool alter_table_comment_targets_existing_temporary_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static bool statement_consumes_next_characteristics_before_execution(
    const struct mylite_sql_ast_node *statement
);
static void clear_select_consumed_next_transaction_characteristics(struct mylite_db *database);
static void clear_next_transaction_characteristics_after_statement(struct mylite_db *database);
static int reject_read_only_persistent_write(
    struct mylite_db *database,
    const struct table_name_resolution *target,
    const struct mylite_catalog_table_descriptor *table
);
static int table_resolution_is_temporary(
    struct mylite_db *database,
    const struct table_name_resolution *target,
    const struct mylite_catalog_table_descriptor *table,
    bool *out_is_temporary
);
static bool statement_requires_implicit_user_transaction_commit(
    const struct mylite_sql_ast_node *statement
);
static int commit_active_user_transaction_for_ddl(struct mylite_db *database);
static void clear_user_savepoints(struct mylite_db *database);
static int create_or_replace_user_savepoint(struct mylite_db *database, const char *name);
static int rollback_to_user_savepoint(struct mylite_db *database, const char *name);
static int release_user_savepoint(struct mylite_db *database, const char *name);
static int copy_savepoint_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size
);
static void fold_savepoint_name(const char *source, char *destination, size_t destination_size);
static bool find_user_savepoint(
    const struct mylite_session_state *session,
    const char *name,
    size_t *out_index
);
static int reserve_user_savepoints(struct mylite_db *database, size_t required_capacity);
static int format_next_sqlite_savepoint_name(
    struct mylite_db *database,
    char *destination,
    size_t destination_size
);
static int execute_sqlite_savepoint_control(
    struct mylite_db *database,
    const char *prefix,
    const char *sqlite_name
);
static int build_savepoint_control_sql(
    struct savepoint_control_sql_request request,
    char **out_sql
);
static void remove_user_savepoint_at(struct mylite_session_state *session, size_t index);
static int append_user_savepoint(struct mylite_db *database, struct user_savepoint_values values);
static int begin_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction
);
static int commit_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction
);
static void rollback_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction
);
static int normalize_sqlite_control_rc(struct mylite_db *database, int rc);
static int execute_create_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_create_temporary_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_create_table_like_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_create_temporary_table_like_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_create_table_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_create_temporary_table_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_create_view_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_create_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_create_schema_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_schema_default_charset_collation_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int maybe_finish_create_schema_if_not_exists_noop(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result *result,
    bool *out_finished
);
static bool create_schema_has_if_not_exists(const struct mylite_sql_ast_node *statement);
static int execute_drop_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_drop_temporary_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_drop_view_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int plan_drop_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool temporary_only,
    struct planned_drop_table *out_plan
);
static int plan_drop_view(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_drop_table *out_plan
);
static int execute_drop_view_from_plan(
    struct mylite_db *database,
    const struct planned_drop_table *plan
);
static int plan_drop_table_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target_node,
    struct planned_drop_table *out_plan,
    size_t target_index
);
static int check_drop_table_duplicate_targets(
    struct mylite_db *database,
    const struct planned_drop_table *plan,
    size_t target_index
);
static bool drop_table_targets_match(
    const struct planned_drop_table_target *left,
    const struct planned_drop_table_target *right
);
static int finish_drop_table_missing_targets(
    struct mylite_db *database,
    const struct planned_drop_table *plan
);
static bool drop_table_plan_requires_implicit_commit(const struct planned_drop_table *plan);
static int append_drop_table_missing_notes(
    struct mylite_db *database,
    const struct planned_drop_table *plan
);
static void planned_drop_table_deinit(struct planned_drop_table *plan);
static int execute_drop_table_from_plan(
    struct mylite_db *database,
    const struct planned_drop_table *plan
);
static bool planned_drop_table_has_temporary_targets(const struct planned_drop_table *plan);
static bool planned_drop_table_has_persistent_targets(const struct planned_drop_table *plan);
static int delete_drop_table_persistent_catalog_rows(
    struct mylite_db *database,
    const struct planned_drop_table *plan,
    struct mylite_catalog_mutation *mutation
);
static int drop_existing_physical_tables(
    struct mylite_db *database,
    const struct planned_drop_table *plan
);
static int remove_drop_table_temporary_descriptors(
    struct mylite_db *database,
    const struct planned_drop_table *plan
);
static int execute_drop_schema_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int maybe_finish_drop_schema_if_exists_noop(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool *out_finished
);
static bool drop_schema_has_if_exists(const struct mylite_sql_ast_node *statement);
static int execute_truncate_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_rename_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_rename_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_add_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_multi_action_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_multi_action(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    struct alter_table_multi_action_state *state,
    bool *out_physical_schema_changed
);
static int execute_alter_table_multi_action_add_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    struct alter_table_multi_action_state *state,
    bool *out_physical_schema_changed
);
static int execute_alter_table_multi_action_add_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    bool *out_physical_schema_changed
);
static int execute_alter_table_multi_action_add_primary_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    bool *out_physical_schema_changed
);
static int execute_alter_table_multi_action_drop_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    bool *out_physical_schema_changed
);
static int execute_alter_table_multi_action_drop_primary_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    bool *out_physical_schema_changed
);
static int execute_alter_table_multi_action_set_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    const struct alter_table_multi_action_state *state
);
static int execute_alter_table_multi_action_drop_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    const struct alter_table_multi_action_state *state
);
static int validate_alter_table_multi_action_final_state(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node
);
static int validate_alter_table_multi_action_auto_increment_keys(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table
);
static bool table_index_infos_have_leading_column(
    const struct alter_table_multi_auto_increment_key_lookup *lookup
);
static void make_alter_table_action_statement_view(
    const struct mylite_sql_ast_node *table_node,
    struct alter_table_action_statement_view *out_view,
    const struct mylite_sql_ast_node *action
);
static int reject_warning_producing_multi_action_add_column(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan
);
static int reject_unsupported_multi_action_add_index(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
);
static int reject_unsupported_multi_action_add_primary_key(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
);
static int reject_multi_action_default_target_added_in_statement(
    struct mylite_db *database,
    const struct alter_table_multi_default_target_view *target,
    const struct alter_table_multi_action_state *state
);
static int reject_alter_table_multi_action_temporary_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node
);
static bool multi_action_state_has_added_column(
    const struct alter_table_multi_action_state *state,
    const char *column_name
);
static int copy_multi_action_default_target_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *action,
    char *out_name,
    size_t out_size
);
static int copy_multi_action_table_leaf_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    char *out_name,
    size_t out_size
);
static int append_multi_action_added_column(
    struct mylite_db *database,
    struct alter_table_multi_action_state *state,
    const char *column_name
);
static int reserve_multi_action_added_columns(
    struct mylite_db *database,
    struct alter_table_multi_action_state *state,
    size_t required_capacity
);
static void alter_table_multi_action_state_deinit(struct alter_table_multi_action_state *state);
static int execute_alter_table_add_primary_key_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_add_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_add_foreign_key_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_drop_foreign_key_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_drop_constraint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_drop_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_rename_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_add_check_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_drop_check_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_alter_check_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_drop_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_drop_primary_key_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_auto_increment_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_drop_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_rename_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_modify_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_change_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_set_default_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_drop_default_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_column_visibility_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_index_visibility_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_default_charset_collation_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_convert_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_comment_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_order_by_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_force_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_disable_keys_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_enable_keys_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_key_maintenance_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int validate_alter_table_algorithm_lock_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int validate_alter_table_algorithm_lock_option_values(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm,
    enum mylite_sql_ast_alter_lock lock
);
static int validate_alter_table_column_algorithm_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm
);
static int validate_alter_table_add_index_algorithm_lock_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    enum mylite_sql_ast_alter_algorithm algorithm,
    enum mylite_sql_ast_alter_lock lock
);
static int validate_alter_table_online_metadata_algorithm_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm
);
static int validate_alter_table_add_foreign_key_algorithm_lock_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm,
    enum mylite_sql_ast_alter_lock lock
);
static int validate_alter_table_rebuild_algorithm_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm
);
static int validate_alter_table_key_maintenance_algorithm_lock_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm,
    enum mylite_sql_ast_alter_lock lock
);
static bool alter_table_statement_accepts_algorithm_lock_options(
    enum mylite_sql_ast_node_kind kind
);
static bool alter_table_add_index_statement_is_fulltext(
    const struct mylite_sql_ast_node *statement
);
static int execute_insert_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_replace_values_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int append_insert_delayed_warning_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int append_replace_delayed_warning_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int execute_planned_insert_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_insert_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_replace_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_planned_insert_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_insert_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_replace_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_planned_insert_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_load_data_infile_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_delete_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_update_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_do_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_values_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_select_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int execute_grouped_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int execute_count_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int execute_column_aggregate_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static bool select_statement_has_count_having_clause(const struct mylite_sql_ast_node *statement);
static int execute_count_having_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static bool select_statement_has_count_expression_aggregate(
    const struct mylite_sql_ast_node *statement
);
static int execute_count_expression_aggregate_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int execute_descriptor_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int execute_compound_select_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);

struct compound_row_comparison_columns {
    bool *string_collation_columns;
    bool *bytewise_collation_columns;
    size_t column_count;
};

struct compound_branch_results {
    mylite_result **items;
    size_t count;
};

static int reject_compound_select_branch_shape(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *branch,
    enum mylite_sql_ast_set_operator operator_kind
);
static int validate_compound_select_operator_chain(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *terms,
    enum mylite_sql_ast_set_operator *out_operator_kind
);
static int execute_compound_select_branch_results(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    enum mylite_sql_ast_set_operator operator_kind,
    struct compound_branch_results *out_results
);
static void deinit_compound_branch_results(struct compound_branch_results *results);
static size_t compound_select_branch_count(const struct mylite_sql_ast_node *statement);
static int init_compound_row_comparison_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct compound_branch_results *branch_results,
    struct compound_row_comparison_columns *out_columns
);
static void deinit_compound_row_comparison_columns(struct compound_row_comparison_columns *columns);
static int apply_compound_term_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *term,
    const mylite_result *branch_result,
    mylite_result **in_out_result,
    struct compound_row_comparison_columns *comparison_columns
);
static int append_compound_result_columns(
    struct mylite_db *database,
    mylite_result *target,
    const mylite_result *source
);
static int append_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    bool distinct,
    const bool *string_collation_columns
);
static int apply_intersect_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    bool distinct,
    const bool *string_collation_columns
);
static int apply_intersect_all_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    const bool *string_collation_columns
);
static int apply_except_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    bool distinct,
    const bool *string_collation_columns
);
static int apply_except_all_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    const bool *string_collation_columns
);
static int create_empty_compound_result_like(
    struct mylite_db *database,
    const mylite_result *source,
    mylite_result **out_result
);
static int deduplicate_compound_result(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const bool *string_collation_columns
);
static int append_distinct_compound_rows(
    struct mylite_db *database,
    mylite_result *target,
    const mylite_result *source,
    const bool *string_collation_columns
);
static int append_all_compound_rows(
    struct mylite_db *database,
    mylite_result *target,
    const mylite_result *source
);
static int append_compound_row(
    struct mylite_db *database,
    mylite_result *target,
    const mylite_result *source,
    size_t row_index
);
static int merge_compound_string_collation_columns(
    struct mylite_db *database,
    const mylite_result *branch_result,
    struct compound_row_comparison_columns *columns
);
static bool compound_result_column_uses_string_collation(
    const mylite_result *result,
    size_t column_index
);
static bool result_column_uses_string_collation(const struct mylite_result_column *column);
static bool compound_result_column_uses_bytewise_collation(
    const mylite_result *result,
    size_t column_index
);
static bool result_column_uses_bytewise_collation(const struct mylite_result_column *column);
static bool compound_statement_column_uses_string_collation(
    const struct mylite_sql_ast_node *statement,
    size_t column_index
);
static bool compound_select_item_uses_string_collation(
    const struct mylite_sql_ast_node *branch,
    size_t column_index
);
static bool compound_expression_uses_string_collation(const struct mylite_sql_ast_node *expression);
static bool compound_statement_column_has_binary_expression(
    const struct mylite_sql_ast_node *statement,
    size_t column_index
);
static bool compound_select_item_uses_binary_collation(
    const struct mylite_sql_ast_node *branch,
    size_t column_index
);
static bool compound_expression_uses_binary_collation(const struct mylite_sql_ast_node *expression);
static bool compound_result_contains_row(
    const mylite_result *target,
    const mylite_result *source,
    size_t source_row_index,
    const bool *string_collation_columns
);
static bool compound_result_find_unmatched_row(
    const mylite_result *target,
    const mylite_result *source,
    size_t source_row_index,
    const bool *matched_rows,
    size_t *out_row_index,
    const bool *string_collation_columns
);
static const char *compound_select_non_select_branch_error(
    enum mylite_sql_ast_set_operator operator_kind
);
static const char *compound_select_calc_found_rows_error(
    enum mylite_sql_ast_set_operator operator_kind
);
static const char *compound_select_options_error(enum mylite_sql_ast_set_operator operator_kind);
static const char *compound_select_order_by_error(enum mylite_sql_ast_set_operator operator_kind);
static const char *compound_select_limit_error(enum mylite_sql_ast_set_operator operator_kind);
static const char *compound_select_locking_clause_error(
    enum mylite_sql_ast_set_operator operator_kind
);
static bool compound_result_rows_equal(
    const mylite_result *left,
    size_t left_row_index,
    const mylite_result *right,
    size_t right_row_index,
    const bool *string_collation_columns
);
static bool compound_result_cells_equal(
    const mylite_result *left,
    size_t left_row_index,
    const mylite_result *right,
    size_t right_row_index,
    size_t column_index,
    const bool *string_collation_columns
);
static bool compound_result_cells_equal_ascii_ci(
    const void *left_bytes,
    size_t left_size,
    const void *right_bytes,
    size_t right_size
);
static int execute_scalar_or_row_scalar_select_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result,
    bool *out_handled
);
static void apply_sql_select_limit_to_plan_limit(
    const struct mylite_db *database,
    struct planned_select_limit *limit
);
static void apply_sql_select_limit_to_result(
    const struct mylite_db *database,
    mylite_result *result
);
static int reject_select_modifier_usage_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int execute_information_schema_select_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int execute_information_schema_join_compat_select_if_needed(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result,
    bool *out_handled
);
static int select_statement_targets_information_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool *out_matches
);
static int select_source_targets_information_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source,
    bool *out_matches
);
static int table_source_targets_information_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source,
    bool *out_matches
);
static int derived_source_targets_information_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source,
    bool *out_matches
);
static int execute_mysql_system_table_select_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int select_statement_targets_mysql_data_dictionary_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    char *out_table_name,
    size_t table_name_size,
    bool *out_matches
);
static int select_statement_targets_absent_mysql_enterprise_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    char *out_table_name,
    size_t table_name_size,
    bool *out_matches
);
static int select_statement_targets_mysql_system_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool *out_matches
);
static int resolve_mysql_system_table_query(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct information_schema_query *out_query,
    const struct mylite_execution_catalog_mysql_system_table **out_definition
);
static int mysql_system_table_resolve_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct information_schema_query *out_query,
    const struct mylite_execution_catalog_mysql_system_table **out_definition
);
static const struct mylite_execution_catalog_mysql_system_table *find_mysql_system_table_definition(
    const char *schema_name,
    const char *table_name
);
static bool mysql_data_dictionary_table_is_hidden(const char *table_name);
static bool mysql_enterprise_table_is_target_absent(const char *table_name);
static int execute_mysql_system_table_query(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int build_mysql_system_table_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *out_rows
);
static int append_mysql_system_table_system_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
);
static int append_mysql_schema_system_table_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
);
static int append_sys_schema_system_table_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
);
static bool append_sys_schema_summary_system_table_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows,
    int *out_status
);
static int append_sys_schema_x_system_table_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
);
static bool append_sys_schema_x_summary_system_table_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows,
    int *out_status
);
static bool mysql_system_table_definition_has_no_rows(
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static bool mysql_system_table_definition_has_catalog_rows(
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static int append_mysql_engine_cost_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_mysql_plugin_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_mysql_server_cost_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_sys_config_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_version_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_innodb_lock_waits_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_host_summary_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_host_summary_by_file_io_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_host_summary_by_file_io_type_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_host_summary_by_stages_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_host_summary_by_statement_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_host_summary_by_statement_type_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_innodb_buffer_stats_by_schema_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_innodb_buffer_stats_by_table_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_io_by_thread_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_io_global_by_file_by_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_io_global_by_file_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_io_global_by_wait_by_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_io_global_by_wait_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_latest_file_io_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_memory_by_host_by_current_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_memory_by_thread_by_current_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_memory_by_user_by_current_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_ps_check_lost_instrumentation_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_schema_auto_increment_columns_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_schema_auto_increment_columns_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_sys_schema_auto_increment_columns_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int append_sys_schema_auto_increment_columns_column_row(
    struct sys_schema_auto_increment_columns_context *context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
);
static bool sys_schema_auto_increment_columns_max_value(
    const char *data_type,
    bool is_unsigned,
    uint64_t *out_value
);
static int format_sys_schema_auto_increment_columns_ratio(
    struct mylite_db *database,
    int64_t auto_increment,
    uint64_t max_value,
    char *buffer,
    size_t buffer_size
);
static void sort_sys_schema_auto_increment_columns_default_order(
    struct information_schema_row_set *rows
);
static int compare_sys_schema_auto_increment_columns_rows(char *const *left, char *const *right);
static int append_sys_schema_index_statistics_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    bool formatted_latency
);
static int append_sys_schema_index_statistics_mysql_system_rows(
    struct sys_schema_index_statistics_context *context
);
static int append_sys_schema_index_statistics_mysql_system_table_rows(
    struct sys_schema_index_statistics_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static int append_sys_schema_index_statistics_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_sys_schema_index_statistics_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int append_sys_schema_index_statistics_base_index_rows(
    struct sys_schema_index_statistics_context *context,
    const struct mylite_catalog_table_descriptor *table
);
static int append_sys_schema_index_statistics_index_row(
    struct sys_schema_index_statistics_context *context,
    const char *schema_name,
    const char *table_name,
    const char *index_name
);
static int append_sys_x_ps_schema_table_statistics_io_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_x_ps_schema_table_statistics_io_mysql_system_rows(
    struct sys_schema_table_statistics_context *context
);
static int append_sys_x_ps_schema_table_statistics_io_mysql_system_table_row(
    struct sys_schema_table_statistics_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static int append_sys_x_ps_schema_table_statistics_io_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_sys_x_ps_schema_table_statistics_io_table_row_from_catalog(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int append_sys_x_ps_schema_table_statistics_io_table_row(
    struct sys_schema_table_statistics_context *context,
    const char *schema_name,
    const char *table_name
);
static int append_sys_schema_table_statistics_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    bool formatted_metrics,
    bool include_buffer_metrics
);
static int append_sys_schema_table_statistics_mysql_system_rows(
    struct sys_schema_table_statistics_context *context
);
static int append_sys_schema_table_statistics_mysql_system_table_row(
    struct sys_schema_table_statistics_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static int append_sys_schema_table_statistics_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_sys_schema_table_statistics_table_row_from_catalog(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int append_sys_schema_table_statistics_table_row(
    struct sys_schema_table_statistics_context *context,
    const char *schema_name,
    const char *table_name
);
static int append_sys_schema_redundant_indexes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_x_schema_flattened_keys_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_schema_redundant_indexes_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_sys_x_schema_flattened_keys_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_sys_schema_redundant_indexes_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int append_sys_x_schema_flattened_keys_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int build_sys_flattened_key_rows(
    struct sys_schema_redundant_indexes_context *context,
    const struct mylite_catalog_table_descriptor *table,
    struct sys_flattened_key_row_set *out_rows
);
static int append_sys_x_schema_flattened_key_rows(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row_set *key_rows
);
static int append_sys_schema_redundant_index_rows(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row_set *key_rows
);
static int append_sys_x_schema_flattened_key_row(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row *key_row
);
static int append_sys_schema_redundant_index_row(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row *redundant,
    const struct sys_flattened_key_row *dominant
);
static bool sys_schema_redundant_index_pair_matches(
    const struct sys_flattened_key_row *redundant,
    const struct sys_flattened_key_row *dominant
);
static bool sys_schema_redundant_columns_left_prefix(const char *left, const char *right);
static int append_sys_flattened_key_row_from_index(
    struct sys_schema_redundant_indexes_context *context,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index,
    struct sys_flattened_key_row_set *rows
);
static bool sys_flattened_key_index_supported(const struct loaded_index_info *index);
static int build_sys_flattened_key_index_columns(
    const struct loaded_index_info *index,
    char **out_columns
);
static int reserve_sys_flattened_key_rows(
    struct sys_flattened_key_row_set *rows,
    size_t required_capacity
);
static void sys_flattened_key_rows_deinit(struct sys_flattened_key_row_set *rows);
static int build_sys_redundant_drop_index_sql(
    const struct sys_flattened_key_row *redundant,
    char **out_sql
);
static int append_sys_schema_table_lock_waits_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_schema_tables_with_full_table_scans_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_schema_unused_indexes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_schema_unused_indexes_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_sys_schema_unused_indexes_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int append_sys_schema_unused_indexes_index_rows(
    struct sys_schema_unused_indexes_context *context,
    const struct mylite_catalog_table_descriptor *table
);
static int append_sys_schema_unused_indexes_index_row(
    struct sys_schema_unused_indexes_context *context,
    const char *table_name,
    const struct loaded_index_info *index
);
static bool sys_schema_unused_indexes_index_supported(const struct loaded_index_info *index);
static int append_sys_schema_object_overview_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int collect_sys_schema_object_overview_builtin_groups(
    struct sys_schema_object_overview_context *context
);
static int collect_sys_schema_object_overview_mysql_system_index_groups(
    struct sys_schema_object_overview_context *context
);
static int append_sys_schema_object_overview_schema_groups(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_sys_schema_object_overview_table_groups(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int append_sys_schema_object_overview_base_table_index_groups(
    struct sys_schema_object_overview_context *context,
    const struct mylite_catalog_table_descriptor *table
);
static int increment_sys_schema_object_overview_group(
    struct sys_schema_object_overview_context *context,
    const char *schema_name,
    const char *object_type,
    uint64_t count
);
static int grow_sys_schema_object_overview_groups(
    struct sys_schema_object_overview_context *context
);
static void sort_sys_schema_object_overview_groups(
    struct sys_schema_object_overview_context *context
);
static int compare_sys_schema_object_overview_groups(
    const struct sys_schema_object_overview_group *left,
    const struct sys_schema_object_overview_group *right
);
static int append_sys_schema_object_overview_group_rows(
    struct sys_schema_object_overview_context *context
);
static int compare_optional_text(const char *left, const char *right);
static size_t mysql_system_table_primary_key_column_count(
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static int append_mysql_innodb_index_stats_builtin_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_mysql_innodb_index_stats_builtin_index_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    const char *distinct_count,
    const char *stat_description
);
static int append_mysql_innodb_table_stats_builtin_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_mysql_innodb_table_stats_builtin_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *row_count
);
static int append_mysql_system_table_catalog_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
);
static int append_mysql_system_table_catalog_schema(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_mysql_system_table_catalog_table(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int append_mysql_innodb_table_stats_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_mysql_innodb_index_stats_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_mysql_innodb_index_stats_index_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index
);
static int append_mysql_innodb_index_stats_generated_cluster_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_mysql_innodb_index_stats_diff_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count
);
static int append_mysql_innodb_index_stats_generated_diff_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_mysql_innodb_index_stats_page_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    const char *stat_name,
    const char *stat_description
);
static int append_mysql_innodb_index_stats_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    const char *stat_name,
    const char *stat_value,
    const char *sample_size,
    const char *stat_description
);
static int mysql_innodb_index_stats_prefix_count(
    struct mylite_db *database,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t *out_count
);
static int read_mysql_innodb_index_stats_distinct_count(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count,
    int64_t *out_count
);
static int build_mysql_innodb_index_stats_distinct_sql(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count,
    char **out_sql
);
static int append_mysql_innodb_index_stats_prefix_expression(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_index
);
static int append_mysql_innodb_index_stats_prefix_description(
    struct mylite_db *database,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count,
    char **out_description
);
static int append_mysql_innodb_index_stats_prefix_name(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_index
);
static const struct loaded_index_part *mysql_innodb_index_stats_missing_clustered_part(
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t missing_index
);
static int mysql_system_current_timestamp(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
);
static int mysql_system_current_timestamp2(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
);
static bool selected_schema_is_mysql_system_schema(const struct mylite_db *database);
static bool schema_name_is_mysql_system_schema(const char *schema_name);
static int resolve_information_schema_query(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct information_schema_query *out_query
);
static void information_schema_query_deinit(struct information_schema_query *query);
static int execute_information_schema_query(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int build_information_schema_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_execution_catalog_table_definition *definition,
    struct information_schema_row_set *out_rows
);
static void information_schema_row_set_deinit(struct information_schema_row_set *rows);
static int append_information_schema_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_triggers_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_views_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_view_table_usage_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_view_routine_usage_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_catalog_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_catalog_schema(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_information_schema_catalog_table(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int append_information_schema_catalog_view_table(
    struct information_schema_catalog_context *context,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_catalog_base_table(
    struct information_schema_catalog_context *context,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *const *values
);
static int append_information_schema_schemata_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_schemata_schema_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema
);
static int append_information_schema_schemata_extensions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_schemata_extensions_schema_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema
);
static int append_information_schema_tables_extensions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_builtin_table_extension_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static int append_information_schema_tables_extensions_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_tablespaces_extensions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_tablespaces_extensions_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_files_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_innodb_datafiles_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_innodb_tablespaces_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_innodb_session_temp_tablespaces_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_innodb_cmp_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_innodb_buffer_pool_stats_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_innodb_cmpmem_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_innodb_ft_default_stopword_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_innodb_tablespaces_brief_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int copy_information_schema_tablespace_name(
    struct mylite_db *database,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    char **out_name
);
static int append_information_schema_character_sets_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_collations_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_collation_applicability_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_engines_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_plugins_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_keywords_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_user_privileges_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_user_attributes_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_resource_groups_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_st_units_of_measure_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int format_information_schema_resource_group_vcpu_ids(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
);
static long information_schema_online_processor_count(void);
static int append_information_schema_processlist_system_row(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    struct information_schema_row_set *rows
);
static int copy_information_schema_processlist_info(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    char **out_info
);
static int append_information_schema_partitions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_tables_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_builtin_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static int load_builtin_table_status_values(
    struct mylite_db *database,
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name,
    struct table_status_values *status
);
static const struct mylite_execution_catalog_builtin_schema_table_directory *find_builtin_schema_table_directory(
    const char *schema_name
);
static const char *builtin_schema_table_type(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static const char *builtin_schema_table_engine(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static const char *builtin_schema_table_version(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static const char *builtin_schema_table_row_format(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static const char *builtin_schema_table_rows(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static const char *builtin_mysql_table_rows(const char *table_name);
static const char *builtin_schema_table_average_row_length(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static const char *builtin_schema_mysql_help_average_row_length(const char *table_name);
static const char *builtin_schema_mysql_time_zone_average_row_length(const char *table_name);
static const char *builtin_schema_table_data_length(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static const char *builtin_schema_table_index_length(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static const char *builtin_schema_table_data_free(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static const char *builtin_schema_table_auto_increment(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static const char *builtin_schema_table_collation(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static const char *builtin_schema_table_create_options(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static const char *builtin_schema_table_comment(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static const char *builtin_mysql_table_comment(const char *table_name);
static bool builtin_schema_table_has_update_time(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static bool builtin_schema_table_is_mysql_stats(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static bool builtin_schema_table_is_mysql_cost(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static bool builtin_schema_table_is_mysql_component(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static bool builtin_schema_table_is_mysql_func(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static bool builtin_schema_table_is_mysql_gtid_executed(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static bool builtin_schema_table_is_mysql_help(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static bool builtin_schema_table_is_mysql_ndb_binlog_index(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static bool builtin_schema_table_is_mysql_replication_metadata(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static bool builtin_schema_table_is_mysql_plugin(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static bool builtin_schema_table_is_mysql_servers(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static bool builtin_schema_table_is_mysql_time_zone(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
);
static int append_information_schema_tables_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_tables_view_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_partitions_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int load_table_status_values(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    struct table_status_values *out_values
);
static int load_table_status_row_format(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    struct table_status_values *out_values
);
static bool table_status_row_format_is_compressed(
    const struct mylite_catalog_table_descriptor *table
);
static int load_table_status_create_options(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    struct table_status_values *out_values
);
static int append_table_status_create_option(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size,
    bool *has_option,
    const char *text
);
static int append_table_status_create_option_i64(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size,
    bool *has_option,
    const char *prefix,
    int64_t value
);
static int table_status_auto_increment_predicate_value(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct table_status_values *status,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
static int table_status_has_temporary_shadow(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    bool *out_has_temporary_shadow
);
static int table_status_has_auto_increment(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    bool *out_has_auto_increment
);
static int table_status_index_length(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t *out_index_length
);
static int note_nonprimary_index_presence(
    const struct mylite_catalog_index_descriptor *index,
    void *user_data
);
static int format_table_status_timestamp(
    struct mylite_db *database,
    int64_t epoch,
    char *buffer,
    size_t buffer_size,
    const char **out_timestamp
);
static int append_information_schema_columns_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_columns_system_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_table_definition *definition
);
static int append_information_schema_columns_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_columns_mysql_system_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static int append_information_schema_columns_extensions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_columns_extensions_system_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_table_definition *definition
);
static int append_information_schema_columns_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_st_geometry_columns_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_innodb_virtual_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_innodb_virtual_column_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t virtual_index
);
static int collect_information_schema_innodb_virtual_base_positions(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const struct mylite_catalog_column_descriptor *column,
    bool *base_positions,
    size_t *out_base_position_count
);
static int collect_information_schema_innodb_virtual_base_identifier(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *identifier,
    bool *base_positions,
    size_t *base_position_count
);
static int copy_information_schema_innodb_virtual_identifier(
    struct mylite_db *database,
    const char *expression,
    size_t *expression_index,
    char *identifier,
    size_t identifier_size
);
static int find_information_schema_innodb_virtual_base_column(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *identifier,
    const struct mylite_catalog_column_descriptor **out_column
);
static int information_schema_innodb_virtual_position(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t virtual_index,
    int64_t *out_position
);
static int append_information_schema_innodb_virtual_dependency_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    int64_t table_id,
    int64_t position,
    int64_t base_position
);
static int append_information_schema_innodb_columns_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_innodb_columns_column_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
);
static int information_schema_innodb_column_position(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    int64_t *out_position
);
static int information_schema_innodb_column_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
);
static int information_schema_innodb_column_character_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_character_type_request request,
    struct information_schema_innodb_column_type_info *out_info
);
static int information_schema_innodb_column_nonstring_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
);
static int information_schema_innodb_column_text_family_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
);
static int information_schema_innodb_column_binary_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
);
static int information_schema_innodb_column_integer_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
);
static int information_schema_innodb_column_decimal_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
);
static int information_schema_innodb_column_approximate_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
);
static int information_schema_innodb_column_bit_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
);
static int information_schema_innodb_column_enum_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
);
static int information_schema_innodb_column_set_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
);
static int information_schema_innodb_column_blob_family_length(
    struct mylite_db *database,
    uint64_t maximum_length,
    int64_t *out_length
);
static int information_schema_innodb_column_collation_prtype(
    struct mylite_db *database,
    const char *collation_name,
    int64_t mysql_type_code,
    bool is_nullable,
    int64_t *out_prtype
);
static int information_schema_innodb_column_binary_prtype(
    struct mylite_db *database,
    int64_t mysql_type_code,
    bool is_nullable,
    int64_t *out_prtype
);
static int information_schema_innodb_column_collation_type_prtype(
    struct mylite_db *database,
    const char *collation_name,
    int64_t type_flags,
    bool is_nullable,
    int64_t *out_prtype
);
static int information_schema_innodb_column_integer_mysql_type(
    struct mylite_db *database,
    const char *logical_type,
    int64_t *out_type_code
);
static int64_t information_schema_innodb_column_not_null_prtype_flag(
    const struct mylite_catalog_column_descriptor *column
);
static bool information_schema_innodb_column_logical_type_is_unsigned(const char *logical_type);
static int append_information_schema_innodb_tables_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_innodb_tablestats_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int information_schema_count_nonprimary_indexes(
    struct mylite_db *database,
    int64_t table_id,
    int64_t *out_count
);
static int count_nonprimary_index(
    const struct mylite_catalog_index_descriptor *index,
    void *user_data
);
static int information_schema_innodb_table_flag(
    struct mylite_db *database,
    const char *row_format,
    int64_t *out_flag
);
static int information_schema_innodb_table_column_count(
    struct mylite_db *database,
    size_t column_count,
    int64_t *out_column_count
);
static int information_schema_innodb_table_zip_page_size(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const char *row_format,
    int64_t *out_zip_page_size
);
static int append_information_schema_columns_extensions_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_columns_view_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_columns_base_column_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    const struct primary_key_info *primary_key,
    const struct loaded_index_info *indexes,
    size_t index_count
);
static int append_information_schema_st_geometry_columns_column_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
);
static int append_information_schema_columns_extensions_column_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
);
static int append_information_schema_views_view_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_view_table_usage_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int column_default_display_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    char *default_text,
    size_t default_text_size,
    const char **out_default_text
);
static int format_binary_default_display_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    char *default_text,
    size_t default_text_size,
    const char **out_default_text
);
static int format_binary_blob_expression_default_display_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    char *default_text,
    size_t default_text_size,
    const char **out_default_text
);
static int decode_binary_default_text(
    struct mylite_db *database,
    const char *default_text,
    char **out_bytes,
    size_t *out_byte_count
);
static size_t binary_default_display_byte_count(const char *bytes, size_t byte_count);
static int append_mysql_utf8mb4_expression_text(
    struct mylite_dynamic_string *string,
    const char *text
);
static int format_mysql_utf8mb4_expression_text(const char *text, char *buffer, size_t buffer_size);
static int append_mysql_escaped_expression_text(
    struct mylite_dynamic_string *string,
    const char *text
);
static const char *column_extra_text(const struct mylite_catalog_column_descriptor *column);
static bool column_default_is_generated_extra(
    const struct mylite_catalog_column_descriptor *column
);
static int append_information_schema_columns_numeric_metadata(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    char *numeric_precision_text,
    size_t numeric_precision_text_size,
    char *numeric_scale_text,
    size_t numeric_scale_text_size,
    const char **values
);
static int append_information_schema_table_constraints_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_innodb_indexes_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_innodb_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table,
    size_t column_count,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index
);
static int append_information_schema_innodb_generated_cluster_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table,
    size_t column_count
);
static int append_information_schema_innodb_fields_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_innodb_fields_index_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct loaded_index_info *index
);
static int append_information_schema_innodb_fields_part_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct loaded_index_info *index,
    const struct loaded_index_part *part
);
static int information_schema_innodb_index_type(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    const struct loaded_index_info *clustered_index,
    int64_t *out_type
);
static int information_schema_innodb_index_field_count(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    size_t column_count,
    const struct loaded_index_info *clustered_index,
    int64_t *out_field_count
);
static int information_schema_innodb_secondary_clustered_part_count(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    const struct loaded_index_info *clustered_index,
    size_t *out_count
);
static int information_schema_innodb_generated_cluster_field_count(
    struct mylite_db *database,
    size_t column_count,
    int64_t *out_field_count
);
static int information_schema_innodb_generated_cluster_index_id(
    struct mylite_db *database,
    int64_t table_id,
    int64_t *out_index_id
);
static const struct loaded_index_info *information_schema_innodb_clustered_index(
    const struct loaded_index_info *indexes,
    size_t index_count
);
static bool information_schema_innodb_index_is_all_not_null_unique(
    const struct loaded_index_info *index
);
static bool information_schema_innodb_index_contains_column(
    const struct loaded_index_info *index,
    int64_t column_id
);
static int append_information_schema_innodb_foreign_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_innodb_foreign_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
);
static int append_information_schema_innodb_foreign_cols_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_innodb_foreign_cols_foreign_key_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct loaded_foreign_key_info *foreign_key
);
static int append_information_schema_innodb_foreign_cols_part_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *foreign_key_id,
    const struct loaded_foreign_key_part *part
);
static int copy_information_schema_innodb_foreign_id(
    struct mylite_db *database,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct loaded_foreign_key_info *foreign_key,
    char **out_id
);
static int copy_information_schema_innodb_foreign_table_name(
    struct mylite_db *database,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    char **out_name
);
static int copy_information_schema_schema_object_name(
    struct mylite_db *database,
    const char *schema_name,
    const char *object_name,
    char **out_name
);
static int information_schema_innodb_foreign_type(
    struct mylite_db *database,
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    int64_t *out_type
);
static int information_schema_innodb_foreign_action_type(
    struct mylite_db *database,
    const char *rule,
    const struct information_schema_innodb_foreign_action_type_flags *flags,
    int64_t *out_type
);
static int append_information_schema_table_constraints_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index
);
static int append_information_schema_table_constraints_index_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *indexes,
    size_t index_count
);
static int append_information_schema_table_constraints_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
);
static int append_information_schema_table_constraints_foreign_key_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_keys,
    size_t foreign_key_count
);
static int append_information_schema_table_constraints_check_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_check_constraint_info *check_constraint
);
static int append_information_schema_table_constraints_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_table_constraints_mysql_system_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static int append_information_schema_table_constraints_extensions_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_table_constraints_extensions_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index
);
static int append_information_schema_table_constraints_extensions_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
);
static int append_information_schema_table_constraints_extensions_foreign_key_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_keys,
    size_t foreign_key_count
);
static int append_information_schema_table_constraints_extensions_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_table_constraints_extensions_mysql_system_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static int append_information_schema_check_constraints_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_check_constraints_check_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct loaded_check_constraint_info *check_constraint
);
static int append_information_schema_key_column_usage_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_key_column_usage_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index
);
static int append_information_schema_key_column_usage_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
);
static int append_information_schema_key_column_usage_foreign_key_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_keys,
    size_t foreign_key_count
);
static int append_information_schema_key_column_usage_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_key_column_usage_mysql_system_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static int append_information_schema_referential_constraints_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_referential_constraints_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
);
static int append_information_schema_statistics_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_information_schema_statistics_mysql_system_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static int append_information_schema_statistics_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_information_schema_statistics_base_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index
);
static const char *index_visibility_text(bool is_visible);
static int information_schema_append_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_query *query
);
static int information_schema_append_result_rows(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
);
static int information_schema_append_count_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
);
static int information_schema_projection_value(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    size_t projection_index,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
static int information_schema_unsigned_projection_expression_value(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    const struct mylite_sql_ast_node *expression,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
static int information_schema_numeric_projection_expression_value(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    const struct mylite_sql_ast_node *expression,
    double *out_value,
    bool *out_is_null
);
static int information_schema_numeric_text_value(
    struct mylite_db *database,
    const char *text,
    double *out_value,
    bool *out_is_null
);
static int information_schema_numeric_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    double *out_value
);
static int information_schema_matching_row_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t **out_indexes,
    size_t *out_index_count
);
static int information_schema_includes_connection_control_failed_login_attempts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    bool *out_includes_table
);
static int information_schema_predicate_includes_connection_control_failed_login_attempts(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    bool *out_includes_table
);
static bool information_schema_row_is_connection_control_failed_login_attempts(char **row);
static int information_schema_sort_row_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t *indexes,
    size_t index_count
);
static int information_schema_needs_auto_increment_null_default_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    bool *out_needs_order
);
static int information_schema_predicate_has_auto_increment_is_null(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    bool *out_has_predicate
);
static void information_schema_sort_auto_increment_null_row_indexes(
    const struct information_schema_row_set *rows,
    size_t *indexes,
    size_t index_count
);
static int information_schema_compare_auto_increment_null_rows(
    const struct information_schema_row_set *rows,
    size_t left_row,
    size_t right_row
);
static int information_schema_auto_increment_null_schema_priority(const char *schema_name);
static int information_schema_compare_rows(
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    struct information_schema_row_order_pair pair
);
static int information_schema_plan_where(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct information_schema_query *query
);
static int information_schema_validate_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
);
static int information_schema_validate_comparison_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
);
static int information_schema_validate_is_null_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
);
static int information_schema_validate_between_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
);
static int information_schema_validate_in_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
);
static int information_schema_predicate_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    char **row,
    bool *out_matches
);
static void information_schema_predicate_frame_stack_deinit(
    struct information_schema_predicate_frame_stack *stack
);
static int information_schema_predicate_frame_stack_push(
    struct mylite_db *database,
    struct information_schema_predicate_frame_stack *stack,
    struct information_schema_predicate_eval_frame frame
);
static bool information_schema_predicate_frame_stack_pop(
    struct information_schema_predicate_frame_stack *stack,
    struct information_schema_predicate_eval_frame *out_frame
);
static void information_schema_truth_stack_deinit(struct information_schema_truth_stack *stack);
static int information_schema_truth_stack_push(
    struct mylite_db *database,
    struct information_schema_truth_stack *stack,
    enum information_schema_truth_value value
);
static bool information_schema_truth_stack_pop(
    struct information_schema_truth_stack *stack,
    enum information_schema_truth_value *out_value
);
static int information_schema_visit_predicate(
    struct mylite_db *database,
    struct information_schema_predicate_frame_stack *frame_stack,
    const struct mylite_sql_ast_node *predicate_node
);
static int information_schema_evaluate_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    char **row,
    struct information_schema_truth_stack *truth_stack
);
static int information_schema_is_null_predicate_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    char **row,
    enum information_schema_truth_value *out_truth
);
static int information_schema_predicate_row_value(
    struct mylite_db *database,
    const struct information_schema_query *query,
    char **row,
    size_t column_index,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
static int information_schema_compound_predicate_matches(
    struct mylite_db *database,
    enum mylite_sql_ast_node_kind predicate_kind,
    struct information_schema_truth_stack *truth_stack,
    enum information_schema_truth_value *out_truth
);
static enum information_schema_truth_value information_schema_truth_from_bool(bool condition);
static bool information_schema_truth_is_true(enum information_schema_truth_value truth);
static int information_schema_comparison_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    char **row,
    enum information_schema_truth_value *out_truth
);
static int information_schema_comparison_predicate_value(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    enum mylite_sql_ast_operator operator_kind,
    size_t *out_column_index,
    struct information_schema_predicate_value *out_right
);
static int information_schema_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
);
static int information_schema_like_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum information_schema_truth_value *out_truth
);
static int information_schema_null_safe_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
);
static int information_schema_regular_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
);
static int information_schema_nonnull_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
);
static int information_schema_like_comparison_matches(
    struct mylite_db *database,
    const struct mylite_execution_catalog_column_definition *column,
    const char *left_text,
    const char *right_text,
    bool *out_matches
);
static int information_schema_between_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    char **row,
    enum information_schema_truth_value *out_truth
);
static int information_schema_in_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    char **row,
    enum information_schema_truth_value *out_truth
);
static int information_schema_compare_nonnull_value_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    bool *out_matches
);
static int information_schema_between_bound_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    bool *out_matches
);
static int information_schema_numeric_comparison_matches(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const char *left_text,
    const char *right_text,
    bool *out_matches
);
static int information_schema_text_comparison_matches(
    struct mylite_db *database,
    const struct mylite_execution_catalog_column_definition *column,
    enum mylite_sql_ast_operator operator_kind,
    const char *left_text,
    const char *right_text,
    bool right_is_numeric,
    bool *out_matches
);
static int information_schema_predicate_value_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
);
static int information_schema_predicate_literal_value_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
);
static int information_schema_predicate_like_pattern_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
);
static int information_schema_predicate_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
);
static int information_schema_predicate_integer_value_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
);
static int information_schema_format_signed_magnitude(
    struct mylite_db *database,
    bool is_negative,
    uint64_t magnitude,
    char *buffer,
    size_t buffer_size
);
static int information_schema_resolve_column_reference(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *column_node,
    enum column_reference_diagnostic_context diagnostic_context,
    size_t *out_column_index
);
static int information_schema_column_reference_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char **out_text
);
static int information_schema_plan_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    struct information_schema_query *out_query
);
static int information_schema_append_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    size_t column_index,
    const struct mylite_sql_ast_node *alias
);
static int information_schema_append_expression_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
);
static int information_schema_append_projection_slot(
    struct mylite_db *database,
    struct information_schema_query *query,
    size_t column_index,
    enum information_schema_projection_kind kind,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
);
static int information_schema_validate_unsigned_projection_expression(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression
);
static int information_schema_validate_numeric_projection_expression(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression
);
static int information_schema_plan_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    struct information_schema_query *out_query
);
static int information_schema_resolve_order_reference(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *order_reference,
    size_t *out_order_index
);
static int information_schema_plan_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct information_schema_query *out_query
);
static int information_schema_resolve_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct information_schema_query *out_query
);
static const struct mylite_execution_catalog_table_definition *find_information_schema_table_definition(
    const char *table_name
);
static int information_schema_table_definition_index(
    const struct mylite_execution_catalog_table_definition *definition,
    const char *column_name,
    size_t *out_index
);
static int information_schema_compare_text(
    const struct mylite_execution_catalog_table_definition *definition,
    size_t column_index,
    const char *left,
    const char *right
);
static int information_schema_compare_column_text(
    const struct mylite_execution_catalog_column_definition *column,
    const char *left,
    const char *right
);
static int compare_ascii_case_insensitive_text(const char *left, const char *right);
static bool information_schema_column_uses_case_insensitive_collation(
    const struct mylite_execution_catalog_column_definition *column
);
static bool information_schema_column_is_numeric(
    const struct mylite_execution_catalog_table_definition *definition,
    size_t column_index
);
static int information_schema_text_to_i64(const char *text, int64_t *out_value);
static int information_schema_format_i64(
    struct mylite_db *database,
    int64_t value,
    char *buffer,
    size_t buffer_size
);
static int information_schema_character_metadata_for_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    char *character_length_text,
    size_t character_length_text_size,
    char *character_octet_text,
    size_t character_octet_text_size,
    struct information_schema_character_metadata *out_metadata
);
static uint64_t column_effective_max_bytes_per_character(
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
);
static const char *information_schema_data_type_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
);
static const char *information_schema_text_data_type_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
);
static const char *information_schema_binary_data_type_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
);
static const char *information_schema_approximate_data_type_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
);
static const char *information_schema_integer_data_type_for_descriptor(const char *logical_type);
static const char *information_schema_numeric_precision_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
);
static const char *information_schema_numeric_scale_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
);
static int execute_show_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_table_status_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int resolve_show_table_status_filter_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct show_table_status_filter_nodes *out_nodes
);
static int execute_show_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_collation_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_variables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_status_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_triggers_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int resolve_show_triggers_selected_schema(
    struct mylite_db *database,
    struct mylite_catalog_schema_descriptor *out_schema
);
static int resolve_show_triggers_schema_name(
    struct mylite_db *database,
    const char *schema_name,
    struct mylite_catalog_schema_descriptor *out_schema
);
static int append_show_sys_sys_config_trigger_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct show_like_filter *filter
);
static int execute_show_events_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_open_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_routine_status_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_processlist_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_grants_statement(struct mylite_db *database, mylite_result **out_result);
static int execute_show_privileges_statement(
    struct mylite_db *database,
    mylite_result **out_result
);
static int execute_show_binary_log_status_statement(
    struct mylite_db *database,
    mylite_result **out_result
);
static int execute_show_binary_logs_statement(
    struct mylite_db *database,
    mylite_result **out_result
);
static int execute_show_replica_status_statement(
    struct mylite_db *database,
    mylite_result **out_result
);
static int execute_show_replicas_statement(struct mylite_db *database, mylite_result **out_result);
static int execute_show_warnings_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_count_warnings_statement(
    struct mylite_db *database,
    mylite_result **out_result
);
static int execute_show_errors_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_count_errors_statement(
    struct mylite_db *database,
    mylite_result **out_result
);
static int execute_show_columns_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int resolve_show_columns_mysql_system_table(
    struct mylite_db *database,
    struct show_columns_target_nodes nodes,
    const struct mylite_execution_catalog_mysql_system_table **out_definition,
    bool *out_mysql_system_target
);
static int reject_unknown_show_columns_information_schema_table(
    struct mylite_db *database,
    struct show_columns_target_nodes nodes
);
static int copy_show_columns_target_schema_and_table(
    struct mylite_db *database,
    struct show_columns_target_nodes nodes,
    char *schema_name,
    char *table_name,
    bool *out_has_target
);
static bool mysql_system_table_directory_contains(const char *table_name);
static int execute_show_columns_mysql_system_table_statement(
    struct mylite_db *database,
    const struct show_columns_filter_nodes *nodes,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    bool full,
    mylite_result **out_result
);
static int append_show_columns_mysql_system_table_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct show_like_filter *filter,
    const struct mylite_sql_ast_node *where_clause,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    bool full
);
static int load_show_columns_key_metadata(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct primary_key_info *primary_key,
    struct loaded_index_info **out_indexes,
    size_t *out_index_count
);
static int append_show_columns_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    bool full
);
static int resolve_show_columns_filter_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct show_columns_filter_nodes *out_nodes
);
static int validate_show_columns_where_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    bool full
);
static int show_columns_where_clause_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const char *const *values,
    bool full,
    bool *out_matches
);
static int evaluate_show_columns_where_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const *values,
    bool full,
    enum show_variables_where_truth *out_truth
);
static int visit_show_columns_where_predicate(
    struct mylite_db *database,
    struct show_variables_where_frame_stack *frame_stack,
    const struct mylite_sql_ast_node *predicate
);
static int evaluate_show_columns_where_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const *values,
    bool full,
    struct show_variables_where_truth_stack *truth_stack
);
static int evaluate_show_columns_where_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const *values,
    bool full,
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_columns_where_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const *values,
    bool full,
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_columns_where_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const *values,
    bool full,
    enum show_variables_where_truth *out_truth
);
static int show_columns_where_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const char *const *values,
    bool full,
    const char **out_value
);
static int resolve_show_columns_where_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    bool full,
    size_t *out_column_index
);
static int compare_show_columns_where_literal(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const char *left,
    const struct mylite_sql_ast_node *right,
    enum show_variables_where_truth *out_truth
);
static int decode_show_columns_where_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_text
);
static int execute_show_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_create_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_create_view_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_create_database_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_engines_statement(struct mylite_db *database, mylite_result **out_result);
static int execute_show_engine_status_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int validate_show_engine_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int execute_show_plugins_statement(struct mylite_db *database, mylite_result **out_result);
static int execute_show_databases_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_table_maintenance_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_checksum_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int append_checksum_table_result_columns(struct mylite_db *database, mylite_result *result);
static int append_checksum_table_target_row(
    struct mylite_db *database,
    mylite_result *result,
    const struct table_maintenance_target *target
);
static int append_checksum_table_target_warning(
    struct mylite_db *database,
    const struct table_maintenance_target *target
);
static int append_checksum_table_warning(
    struct mylite_db *database,
    int code,
    const char *sqlstate,
    const char *message
);
static enum table_maintenance_operation table_maintenance_operation_for_statement(
    const struct mylite_sql_ast_node *statement
);
static int append_table_maintenance_result_columns(
    struct mylite_db *database,
    mylite_result *result
);
static int append_table_maintenance_target_rows(
    struct mylite_db *database,
    mylite_result *result,
    enum table_maintenance_operation operation,
    const struct table_maintenance_target *target
);
static int append_table_maintenance_success_rows(
    struct mylite_db *database,
    mylite_result *result,
    enum table_maintenance_operation operation,
    const struct table_maintenance_target *target
);
static int append_table_maintenance_unknown_schema_rows(
    struct mylite_db *database,
    mylite_result *result,
    enum table_maintenance_operation operation,
    const struct table_maintenance_target *target
);
static int append_table_maintenance_unknown_table_rows(
    struct mylite_db *database,
    mylite_result *result,
    enum table_maintenance_operation operation,
    const struct table_maintenance_target *target
);
static int append_table_maintenance_row(
    struct mylite_db *database,
    mylite_result *result,
    const char *table_name,
    const char *operation,
    const char *message_type,
    const char *message_text
);
static int resolve_table_maintenance_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_maintenance_target *out_target
);
static int check_table_maintenance_duplicate_target(
    struct mylite_db *database,
    const struct table_maintenance_target *targets,
    size_t target_count,
    const struct table_maintenance_target *target
);
static int format_table_maintenance_display_name(
    struct mylite_db *database,
    struct table_maintenance_target *target
);
static const char *table_maintenance_operation_text(enum table_maintenance_operation operation);
static int64_t row_count_for_completed_statement(
    const struct mylite_sql_ast_node *statement,
    const mylite_result *result
);
static int finish_parse_failure(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result,
    int parse_rc
);
static int finish_failed_statement(struct mylite_db *database, int rc, mylite_result **out_result);
static int finish_completed_statement(
    struct mylite_db *database,
    bool completed_statement_is_select,
    int64_t completed_row_count,
    bool preserve_diagnostics_snapshot,
    mylite_result **out_result
);
static void update_found_rows_for_completed_statement(
    struct mylite_db *database,
    bool completed_statement_is_select,
    const mylite_result *result
);
static bool statement_result_is_select(
    const struct mylite_sql_ast_node *statement,
    const mylite_result *result
);
static bool statement_preserves_diagnostics_snapshot(const struct mylite_sql_ast_node *statement);
static int snapshot_current_diagnostics(struct mylite_db *database);
static int finish_successful_result(
    struct mylite_db *database,
    mylite_result *result,
    mylite_result **out_result
);
static int finish_successful_result_with_warning_count(
    mylite_result *result,
    size_t warning_count,
    mylite_result **out_result
);

static int plan_create_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_create_table *out_plan
);
static int plan_create_table_like(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_create_table_like *out_plan
);
static int plan_create_table_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_create_table_select *out_plan
);
static void planned_create_table_select_deinit(struct planned_create_table_select *plan);
static int create_table_select_from_plan(
    struct mylite_db *database,
    struct planned_create_table_select *plan,
    int64_t *out_affected_rows
);
static int create_temporary_table_select_from_plan(
    struct mylite_db *database,
    struct planned_create_table_select *plan,
    int64_t *out_affected_rows
);
static int plan_create_view(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_create_view *out_plan
);
static int create_view_from_plan(
    struct mylite_db *database,
    const struct planned_create_view *plan
);
static void planned_create_view_deinit(struct planned_create_view *plan);
static int validate_create_view_select_subset(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_statement
);
static int copy_create_view_column_names(
    struct mylite_db *database,
    struct planned_create_view *plan
);
static int build_create_view_definition_sql(
    struct mylite_db *database,
    const struct planned_create_view *plan,
    char **out_sql
);
static int build_create_view_select_sql(
    struct mylite_db *database,
    const struct planned_create_view *plan,
    bool qualify_source_schema,
    char **out_sql
);
static int build_create_view_show_create_sql(
    struct mylite_db *database,
    const struct planned_create_view *plan,
    char **out_sql
);
static int append_create_view_projection_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_view *plan,
    bool qualify_source_schema
);
static int append_create_view_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_view *plan,
    bool qualify_source_schema
);
static int infer_create_table_select_columns(
    struct mylite_db *database,
    struct planned_create_table_select *plan
);
static int validate_create_table_select_source_columns(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
);
static void copy_create_table_select_column_character_metadata(
    const struct mylite_catalog_table_descriptor *source_table,
    const struct mylite_catalog_column_descriptor *source_column,
    const struct planned_create_table *target_table,
    struct planned_column *out_column
);
static int copy_create_table_select_column_name(
    struct mylite_db *database,
    const struct planned_select *source,
    size_t column_index,
    struct planned_column *out_column
);
static int execute_create_table_select_copy(
    struct mylite_db *database,
    const struct planned_create_table_select *plan,
    const char *physical_name,
    int64_t *out_affected_rows
);
static int validate_create_table_select_rows(
    struct mylite_db *database,
    const struct planned_create_table_select *plan
);
static int validate_create_table_select_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_create_table_select *plan,
    size_t row_number
);
static int clone_create_table_like_columns(
    struct mylite_db *database,
    int64_t source_table_id,
    struct planned_create_table *out_plan
);
static int validate_create_table_like_source_columns(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
);
static int clone_create_table_like_indexes(
    struct mylite_db *database,
    int64_t source_table_id,
    const struct mylite_catalog_column_descriptor *source_columns,
    size_t source_column_count,
    struct planned_create_table *out_plan
);
static int clone_create_table_like_check_constraints(
    struct mylite_db *database,
    int64_t source_table_id,
    struct planned_create_table *out_plan
);
static void planned_create_table_like_deinit(struct planned_create_table_like *plan);
static int validate_create_table_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    const char *table_name
);
static int validate_create_table_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    const char *table_name
);
static int apply_create_table_charset_collation_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_create_table *plan
);
static int apply_schema_default_charset_collation_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_options,
    char *default_charset,
    size_t default_charset_size,
    char *default_collation,
    size_t default_collation_size
);
static int apply_create_table_auto_increment_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_create_table *plan
);
static int apply_create_table_comment_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_create_table *plan
);
static int apply_create_table_storage_statistics_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_create_table *plan
);
static int apply_table_storage_statistics_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
);
static int apply_table_row_format_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
);
static int apply_table_key_block_size_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
);
static int apply_table_pack_keys_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
);
static int apply_table_checksum_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
);
static int apply_table_stats_persistent_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
);
static int apply_table_stats_auto_recalc_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
);
static int apply_table_stats_sample_pages_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
);
static int copy_table_row_format_option_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    char *row_format_name,
    size_t row_format_name_size
);
static int parse_table_option_unsigned_integer(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
);
static int parse_table_option_default_or_integer(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    int64_t default_value,
    int64_t *out_value
);
static bool table_option_value_is_default(const struct mylite_sql_ast_node *value_node);
static bool create_table_storage_statistics_option_is_valid(
    const struct mylite_sql_ast_node *table_option
);
static int validate_create_table_storage_statistics_combination(
    struct mylite_db *database,
    const struct planned_create_table *plan
);
static int decode_table_comment_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *comment_option,
    const char *table_name,
    char *destination,
    size_t destination_size
);
static int validate_create_table_engine_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *engine_option,
    const char *table_name
);
static int append_unknown_storage_engine_warning(
    struct mylite_db *database,
    const char *engine_name
);
static int append_using_storage_engine_warning(struct mylite_db *database, const char *table_name);
static int validate_create_table_charset_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *charset_option,
    bool allow_binary
);
static int validate_create_table_collation_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *collation_option,
    bool allow_binary
);
static int validate_create_table_comment_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *comment_option
);
static int validate_table_charset_collation_option_values(
    struct mylite_db *database,
    bool has_charset,
    const char *charset_name,
    bool has_collation,
    const char *collation_name
);
static int validate_table_charset_collation_option_values_with_binary(
    struct mylite_db *database,
    bool has_charset,
    const char *charset_name,
    bool has_collation,
    const char *collation_name,
    bool allow_binary
);
static int classify_table_charset_option_value(
    struct mylite_db *database,
    bool has_charset,
    const char *charset_name,
    bool allow_binary,
    bool *out_is_binary,
    bool *out_is_supported,
    bool *out_is_known_unsupported
);
static int classify_table_collation_option_value(
    struct mylite_db *database,
    bool has_collation,
    const char *collation_name,
    bool allow_binary,
    bool *out_is_binary,
    bool *out_is_supported,
    bool *out_is_known_unsupported
);
static int validate_table_charset_collation_option_pair(
    struct mylite_db *database,
    const char *charset_name,
    bool charset_is_binary,
    bool charset_is_supported,
    bool charset_is_known_unsupported,
    const char *collation_name,
    bool collation_is_binary,
    bool collation_is_supported,
    bool collation_is_known_unsupported
);
static int copy_table_charset_option_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *charset_option,
    char *charset_name,
    size_t charset_name_size
);
static int copy_table_collation_option_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *collation_option,
    char *collation_name,
    size_t collation_name_size
);
static int apply_table_charset_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *charset_option,
    bool allow_binary,
    char *default_charset,
    size_t default_charset_size
);
static int apply_table_collation_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *collation_option,
    bool allow_binary,
    char *default_collation,
    size_t default_collation_size
);
static int validate_alter_table_default_charset_collation_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options
);
static int validate_schema_default_charset_collation_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_options
);
static int track_schema_default_charset_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_option,
    struct schema_default_option_validation *validation
);
static int track_schema_default_collation_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_option,
    struct schema_default_option_validation *validation
);
static int copy_default_collation_for_charset(
    struct mylite_db *database,
    const char *charset_name,
    char *default_collation,
    size_t default_collation_size
);
static int copy_normalized_charset_name(
    struct mylite_db *database,
    const char *charset_name,
    char *destination,
    size_t destination_size
);
static int copy_normalized_collation_name(
    struct mylite_db *database,
    const char *collation_name,
    char *destination,
    size_t destination_size
);
static int apply_schema_default_charset_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_option,
    bool has_collation_option,
    char *default_charset,
    size_t default_charset_size,
    char *default_collation,
    size_t default_collation_size
);
static int apply_schema_default_collation_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_option,
    char *default_charset,
    size_t default_charset_size,
    char *default_collation,
    size_t default_collation_size
);
static int copy_table_option_name_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option_name_node,
    char *destination,
    size_t destination_size,
    struct table_option_name_policy policy
);
static int decode_table_option_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option_name_node,
    char **out_name,
    size_t *out_name_length,
    struct table_option_name_policy policy
);
static const struct mylite_execution_catalog_collation *utf8mb4_collation_by_name(const char *name);
static const struct mylite_execution_catalog_character_set *character_set_by_name(const char *name);
static const struct mylite_execution_catalog_collation *collation_by_name(const char *name);
static bool charset_name_is_binary(const char *name);
static bool collation_name_is_binary(const char *name);
static bool charset_name_is_known_unsupported(const char *name);
static bool collation_name_is_known_unsupported(const char *name);
static int validate_sql_require_primary_key_for_planned_create_table(
    struct mylite_db *database,
    const struct planned_create_table *plan
);
static int validate_sql_require_primary_key_for_table_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table
);
static bool sql_require_primary_key_session_enabled(const struct mylite_db *database);
static int append_decoded_table_option_name_escape(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    char escaped_byte,
    struct table_option_name_policy policy
);
static void planned_create_table_deinit(struct planned_create_table *plan);
static int create_table_from_plan(struct mylite_db *database, struct planned_create_table *plan);
static int create_temporary_table_from_plan(
    struct mylite_db *database,
    struct planned_create_table *plan
);
static int assign_create_table_index_ids(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    struct planned_create_table *plan
);
static int assign_create_table_foreign_key_ids(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    struct planned_create_table *plan
);
static int assign_create_table_check_constraint_ids(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    struct planned_create_table *plan
);
static int assign_create_temporary_table_index_ids(
    struct mylite_db *database,
    struct planned_create_table *plan
);
static int insert_create_table_catalog_rows(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const char *physical_name,
    struct mylite_catalog_table_descriptor *out_table
);
static int insert_create_table_index_catalog_rows(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const int64_t *column_ids
);
static int insert_create_table_foreign_key_catalog_rows(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const int64_t *column_ids
);
static int insert_create_table_check_constraint_catalog_rows(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id
);
static int execute_physical_create_table(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const char *physical_name,
    bool temporary
);
static int execute_physical_drop_table(struct mylite_db *database, const char *physical_name);
static int build_temporary_table_descriptors(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    int64_t table_id,
    const char *physical_name,
    struct mylite_temporary_catalog_table *out_table
);
static int build_temporary_table_column_descriptors(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    int64_t table_id,
    struct mylite_temporary_catalog_table *out_table,
    int64_t **out_column_ids
);
static int build_temporary_table_index_descriptors(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    int64_t table_id,
    const int64_t *column_ids,
    struct mylite_temporary_catalog_table *out_table
);
static int append_temporary_primary_index_descriptor(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    int64_t table_id,
    const int64_t *column_ids,
    struct mylite_temporary_catalog_table *out_table,
    struct temporary_index_descriptor_positions *positions
);
static int append_temporary_secondary_index_descriptor(
    struct mylite_db *database,
    const struct planned_secondary_index *planned,
    int64_t table_id,
    const int64_t *column_ids,
    struct mylite_temporary_catalog_table *out_table,
    struct temporary_index_descriptor_positions *positions
);
static bool planned_create_table_has_fulltext_index(const struct planned_create_table *plan);
static bool planned_create_table_has_spatial_index(const struct planned_create_table *plan);
static bool planned_create_table_has_spatial_column(const struct planned_create_table *plan);
static int execute_physical_alter_table_add_column(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan
);
static int execute_physical_alter_table_drop_column(
    struct mylite_db *database,
    const struct planned_alter_table_drop_column *plan
);
static int execute_physical_alter_table_rename_column(
    struct mylite_db *database,
    const struct planned_alter_table_rename_column *plan
);
static int execute_physical_alter_table_modify_column(
    struct mylite_db *database,
    const struct planned_alter_table_modify_column *plan,
    const struct mylite_catalog_mutation *mutation
);

static int create_schema_from_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result *result
);
static int plan_drop_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_catalog_mutation *mutation,
    struct planned_drop_schema *out_plan
);
static void planned_drop_schema_deinit(struct planned_drop_schema *plan);
static int collect_drop_schema_table(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int reserve_drop_schema_tables(struct planned_drop_schema *plan, size_t required_capacity);
static int drop_schema_from_plan(
    struct mylite_db *database,
    struct mylite_catalog_mutation *mutation,
    const struct planned_drop_schema *plan,
    mylite_result *result
);

static int plan_truncate_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_truncate_table *out_plan
);
static int execute_truncate_from_plan(
    struct mylite_db *database,
    const struct planned_truncate_table *plan,
    mylite_result *result
);

static int plan_rename_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_rename_table_statement *out_plan
);
static int plan_rename_table_pair(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *pair_node,
    struct planned_rename_table *out_pair
);
static void planned_rename_table_statement_deinit(struct planned_rename_table_statement *plan);
static int rename_table_statement_from_plan(
    struct mylite_db *database,
    const struct planned_rename_table_statement *plan
);
static int plan_alter_table_rename(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_rename_table *out_plan
);
static int alter_table_rename_from_plan(
    struct mylite_db *database,
    const struct planned_rename_table *plan
);
static int plan_alter_table_add_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_add_column *out_plan
);
static int reject_unsupported_alter_add_column_planned_column(
    struct mylite_db *database,
    const struct planned_column *column
);
static int plan_alter_table_add_column_position(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *position,
    struct planned_alter_table_add_column *out_plan
);
static int finish_alter_table_add_column_plan_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *position,
    struct planned_alter_table_add_column *out_plan,
    struct mylite_catalog_column_descriptor **columns,
    size_t column_count
);
static int reject_nonempty_temporal_add_column_without_default(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct planned_column *column
);
static int collect_no_zero_date_default_warnings(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct zero_temporal_default_warning_filter filter,
    struct zero_temporal_default_warnings *out_warnings
);
static int reserve_zero_temporal_default_warnings(
    struct mylite_db *database,
    const struct zero_temporal_default_warnings *warnings
);
static int append_zero_temporal_default_warnings(
    struct mylite_db *database,
    const struct zero_temporal_default_warnings *warnings
);
static int append_zero_temporal_default_warning(
    struct mylite_db *database,
    const char *column_name
);
static bool column_descriptor_has_full_zero_temporal_default(
    const struct mylite_catalog_column_descriptor *column
);
static int append_zero_temporal_default_warning_name(
    struct mylite_db *database,
    struct zero_temporal_default_warnings *warnings,
    const char *column_name
);
static void zero_temporal_default_warnings_deinit(struct zero_temporal_default_warnings *warnings);
static void planned_alter_table_add_column_deinit(struct planned_alter_table_add_column *plan);
static int alter_table_add_column_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan
);
static int alter_table_add_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_add_column *plan
);
static int reorder_catalog_columns_after_add(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_add_column *plan,
    const struct mylite_catalog_column_descriptor *inserted_column
);
static int apply_alter_table_add_column_inline_keys(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_add_column *plan,
    const struct mylite_catalog_column_descriptor *inserted_column
);
static int apply_inline_primary_key_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct table_name_resolution *target,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *column
);
static int apply_inline_unique_key_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct table_name_resolution *target,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *column
);
static int make_inline_key_column_descriptors(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *source_columns,
    size_t source_column_count,
    size_t target_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    bool replaces_column,
    struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count
);
static int add_inline_key_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *source_columns,
    size_t source_column_count,
    size_t target_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count
);
static int replace_inline_key_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *source_columns,
    size_t source_column_count,
    size_t target_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count
);
static int apply_alter_table_modify_column_inline_keys(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_modify_column *plan
);
static int plan_alter_table_add_primary_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_add_primary_key *out_plan
);
static int append_alter_table_primary_key_parts_from_ast(
    struct mylite_db *database,
    struct planned_alter_table_add_primary_key *plan,
    const struct mylite_sql_ast_node *part_list,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
);
static void planned_alter_table_add_primary_key_deinit(
    struct planned_alter_table_add_primary_key *plan
);
static int alter_table_add_primary_key_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
);
static int alter_table_add_primary_key_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_add_primary_key *plan
);
static int validate_alter_table_primary_key_key_length(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
);
static int append_alter_table_primary_key_part(
    struct mylite_db *database,
    struct planned_alter_table_add_primary_key *plan,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *column_name,
    enum mylite_catalog_index_sort_direction sort_direction
);
static int reserve_alter_table_primary_key_parts(
    struct mylite_db *database,
    struct planned_alter_table_add_primary_key *plan,
    size_t required_capacity
);
static bool alter_table_primary_key_contains_column(
    const struct planned_alter_table_add_primary_key *plan,
    size_t column_index
);
static int validate_alter_table_add_primary_key_existing_rows(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
);
static int validate_alter_table_add_primary_key_nulls(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
);
static int validate_alter_table_add_primary_key_string_values(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
);
static int validate_alter_table_add_primary_key_duplicates(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
);
static enum mylite_catalog_column_default_kind alter_table_primary_key_default_kind(
    const struct mylite_catalog_column_descriptor *column
);
static int execute_physical_alter_table_add_primary_key(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan,
    const char *index_physical_name
);
static int plan_alter_table_add_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_add_index *out_plan
);
static int plan_create_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_add_index *out_plan
);
static int apply_index_options_to_alter_plan(
    struct mylite_db *database,
    struct index_option_nodes nodes,
    struct planned_alter_table_add_index *plan
);
static int apply_index_options_to_secondary_plan(
    struct mylite_db *database,
    struct index_option_nodes nodes,
    struct planned_secondary_index *index
);
static int apply_index_options_to_create_primary_key_plan(
    struct mylite_db *database,
    struct index_option_nodes nodes,
    struct planned_create_table *plan
);
static int apply_index_options_to_alter_primary_key_plan(
    struct mylite_db *database,
    struct index_option_nodes nodes,
    struct planned_alter_table_add_primary_key *plan
);
static int collect_primary_key_index_options(
    struct mylite_db *database,
    struct index_option_nodes nodes,
    char *comment,
    size_t comment_size,
    bool *out_show_create_explicit_btree,
    bool *out_uses_hash_index_type
);
static int collect_index_options(
    struct mylite_db *database,
    const char *index_name,
    struct index_option_nodes nodes,
    struct planned_index_options *out_options
);
static int collect_index_type_option(
    struct mylite_db *database,
    struct index_option_nodes nodes,
    enum planned_index_type_option *out_type
);
static int apply_single_index_option(
    struct mylite_db *database,
    const char *index_name,
    const struct mylite_sql_ast_node *option,
    struct planned_index_options *options
);
static int apply_index_type_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option,
    struct planned_index_options *options
);
static int apply_index_comment_option(
    struct mylite_db *database,
    const char *index_name,
    const struct mylite_sql_ast_node *option,
    struct planned_index_options *options
);
static int apply_index_visibility_option(
    const struct mylite_sql_ast_node *option,
    struct planned_index_options *options
);
static int decode_index_comment_literal(
    struct mylite_db *database,
    const char *index_name,
    const struct mylite_sql_ast_node *literal,
    char *destination,
    size_t destination_size
);
static int validate_hash_index_key_part_order(
    struct mylite_db *database,
    bool uses_hash_index_type,
    const struct mylite_sql_ast_node *part_list
);
static bool secondary_index_part_has_explicit_direction(const struct mylite_sql_ast_node *part);
static bool index_option_nodes_have_type_option(struct index_option_nodes nodes);
static bool index_option_list_has_type_option(const struct mylite_sql_ast_node *option_list);
static size_t planned_create_table_hash_index_warning_count(
    const struct planned_create_table *plan
);
static int reserve_hash_index_warnings(struct mylite_db *database, size_t additional_count);
static int append_hash_index_warnings(struct mylite_db *database, size_t warning_count);
static int reserve_hash_index_warning_if_needed(
    struct mylite_db *database,
    bool uses_hash_index_type
);
static int append_hash_index_warning_if_needed(
    struct mylite_db *database,
    bool uses_hash_index_type
);
static size_t planned_create_table_spatial_index_warning_count(
    const struct planned_create_table *plan
);
static int reserve_spatial_index_warnings(struct mylite_db *database, size_t warning_count);
static int append_spatial_index_warnings(
    struct mylite_db *database,
    const struct planned_create_table *plan
);
static int append_spatial_index_no_srid_warning(
    struct mylite_db *database,
    const char *column_name
);
static int reserve_additional_warning_capacity(struct mylite_db *database, size_t additional_count);
static int resolve_add_index_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const char *base_only_error,
    struct planned_alter_table_add_index *plan
);
static int resolve_fulltext_add_index_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const char *base_only_error,
    struct planned_alter_table_add_index *plan
);
static int resolve_non_fulltext_add_index_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const char *base_only_error,
    struct planned_alter_table_add_index *plan
);
static bool planned_add_index_is_fulltext(const struct planned_alter_table_add_index *plan);
static bool planned_add_index_is_spatial(const struct planned_alter_table_add_index *plan);
static int reject_temporary_spatial_add_index(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
);
static void planned_alter_table_add_index_deinit(struct planned_alter_table_add_index *plan);
static int extract_alter_table_add_index_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct alter_table_add_index_nodes *out_nodes
);
static int extract_create_index_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct create_index_nodes *out_nodes
);
static int extract_create_table_secondary_index_definition_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *secondary_index,
    struct create_table_secondary_index_definition_nodes *out_nodes
);
static int resolve_create_table_secondary_index_first_column(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct create_table_secondary_index_definition_nodes *nodes,
    enum mylite_catalog_index_kind index_kind,
    bool is_unique,
    struct create_table_secondary_index_first_part *out_first_part
);
static int validate_create_table_secondary_index_kind(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    struct index_option_nodes options,
    enum mylite_catalog_index_kind index_kind,
    bool is_unique,
    struct create_table_secondary_index_first_part first_part,
    enum mylite_catalog_index_kind *out_effective_index_kind
);
static int add_secondary_index_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
);
static int add_persistent_secondary_index_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_add_index *plan,
    bool *out_physical_schema_changed
);
static int add_temporary_secondary_index_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
);
static int reserve_fulltext_add_warning_if_needed(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
);
static int append_fulltext_add_warning_if_needed(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
);
static int reserve_spatial_add_warning_if_needed(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
);
static int append_spatial_add_warning_if_needed(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
);
static int plan_alter_table_add_foreign_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_add_foreign_key *out_plan
);
static void planned_alter_table_add_foreign_key_deinit(
    struct planned_alter_table_add_foreign_key *plan
);
static int alter_table_add_foreign_key_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_add_foreign_key *plan
);
static int choose_alter_table_foreign_key_child_index(
    struct mylite_db *database,
    const struct loaded_index_info *indexes,
    size_t index_count,
    struct planned_alter_table_add_foreign_key *plan
);
static int parse_alter_table_add_foreign_key_definition(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct foreign_key_definition_nodes *out_nodes,
    struct foreign_key_column_names *out_names,
    struct planned_alter_table_add_foreign_key *out_plan
);
static int plan_alter_table_add_foreign_key_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_add_foreign_key *out_plan
);
static int load_alter_table_add_foreign_key_child_metadata(
    struct mylite_db *database,
    const struct planned_alter_table_add_foreign_key *plan,
    struct alter_table_add_foreign_key_child_metadata *out_metadata
);
static void alter_table_add_foreign_key_child_metadata_deinit(
    struct alter_table_add_foreign_key_child_metadata *metadata
);
static int ensure_alter_table_foreign_key_name(
    struct mylite_db *database,
    const struct alter_table_add_foreign_key_child_metadata *metadata,
    struct planned_alter_table_add_foreign_key *plan
);
static int reject_duplicate_alter_table_foreign_key_name(
    struct mylite_db *database,
    const struct alter_table_add_foreign_key_child_metadata *metadata,
    const struct planned_alter_table_add_foreign_key *plan
);
static int validate_alter_table_foreign_key_set_null_columns(
    struct mylite_db *database,
    const struct planned_alter_table_add_foreign_key *plan
);
static int plan_alter_table_add_foreign_key_child_column(
    struct mylite_db *database,
    const struct alter_table_add_foreign_key_child_metadata *metadata,
    const struct foreign_key_column_names *names,
    struct planned_alter_table_add_foreign_key *out_plan
);
static int plan_alter_table_add_foreign_key_parent(
    struct mylite_db *database,
    const struct foreign_key_definition_nodes *nodes,
    const struct foreign_key_column_names *names,
    struct planned_alter_table_add_foreign_key *out_plan
);
static int prepare_alter_table_foreign_key_child_index_plan(
    struct mylite_db *database,
    const struct loaded_index_info *indexes,
    size_t index_count,
    struct planned_alter_table_add_foreign_key *plan
);
static int append_alter_table_foreign_key_part(
    struct mylite_db *database,
    struct planned_alter_table_add_foreign_key *plan,
    const struct mylite_catalog_column_descriptor *child_column,
    size_t child_column_index
);
static int reserve_alter_table_foreign_key_parts(
    struct mylite_db *database,
    struct planned_alter_table_add_foreign_key *plan,
    size_t required_capacity
);
static int validate_foreign_key_existing_child_rows(
    struct mylite_db *database,
    const struct planned_alter_table_add_foreign_key *plan,
    bool parent_write
);
static int insert_alter_table_foreign_key_catalog_rows(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t foreign_key_id,
    int64_t child_index_id,
    const struct planned_alter_table_add_foreign_key *plan
);
static int plan_alter_table_drop_foreign_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_drop_foreign_key *out_plan
);
static void planned_alter_table_drop_foreign_key_deinit(
    struct planned_alter_table_drop_foreign_key *plan
);
static int alter_table_drop_foreign_key_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_drop_foreign_key *plan
);
static int plan_alter_table_drop_constraint(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_drop_constraint *out_plan
);
static int load_alter_table_drop_constraint_context(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct alter_table_drop_constraint_context *context,
    char *constraint_name,
    size_t constraint_name_size
);
static int resolve_alter_table_drop_constraint(
    const struct alter_table_drop_constraint_context *context,
    const char *constraint_name,
    struct alter_table_drop_constraint_resolution *resolution
);
static void match_alter_table_drop_constraint_indexes(
    const struct alter_table_drop_constraint_context *context,
    const char *constraint_name,
    struct alter_table_drop_constraint_resolution *resolution
);
static void match_alter_table_drop_constraint_foreign_keys(
    const struct alter_table_drop_constraint_context *context,
    const char *constraint_name,
    struct alter_table_drop_constraint_resolution *resolution
);
static void match_alter_table_drop_constraint_checks(
    const struct alter_table_drop_constraint_context *context,
    const char *constraint_name,
    struct alter_table_drop_constraint_resolution *resolution
);
static int populate_alter_table_drop_constraint_plan(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct alter_table_drop_constraint_context *context,
    const struct alter_table_drop_constraint_resolution *resolution,
    struct planned_alter_table_drop_constraint *out_plan
);
static int validate_alter_table_drop_constraint_algorithm_lock_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    enum planned_alter_table_drop_constraint_kind kind
);
static void alter_table_drop_constraint_context_deinit(
    struct alter_table_drop_constraint_context *context
);
static void planned_alter_table_drop_constraint_deinit(
    struct planned_alter_table_drop_constraint *plan
);
static int find_loaded_foreign_key_by_name(
    const struct loaded_foreign_key_info *foreign_keys,
    size_t foreign_key_count,
    const char *foreign_key_name,
    size_t *out_foreign_key
);
static int reject_table_referenced_by_foreign_key(struct mylite_db *database, int64_t table_id);
static int reject_table_with_foreign_keys(struct mylite_db *database, int64_t table_id);
static int reject_foreign_key_table_alter(
    struct mylite_db *database,
    int64_t table_id,
    const char *message
);
static int reject_index_used_by_foreign_key(
    struct mylite_db *database,
    const struct mylite_catalog_index_descriptor *index
);
static int collect_foreign_key_exists(
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    void *user_data
);
static int collect_foreign_key_parent_index_exists(
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    void *user_data
);
static int collect_foreign_key_child_index_exists(
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    void *user_data
);
static int plan_alter_table_add_index_name(
    struct mylite_db *database,
    const struct loaded_index_info *indexes,
    size_t index_count,
    const struct mylite_sql_ast_node *index_name_node,
    const char *column_name,
    char *destination,
    size_t destination_size
);
static int plan_alter_table_add_index_name_from_parts(
    struct mylite_db *database,
    const struct alter_table_add_index_nodes *nodes,
    const struct loaded_index_info *indexes,
    size_t index_count,
    char *destination,
    size_t destination_size
);
static int apply_loaded_add_index_spatial_kind_from_parts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *part_list,
    struct index_option_nodes options,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_add_index *plan
);
static int validate_loaded_add_index_kind_options(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan,
    enum mylite_catalog_index_kind original_kind,
    struct index_option_nodes options,
    size_t part_count
);
static int generate_alter_table_add_index_name(
    struct mylite_db *database,
    const struct loaded_index_info *indexes,
    size_t index_count,
    const char *base_name,
    char *destination,
    size_t destination_size
);
static bool loaded_index_name_is_used(
    const struct loaded_index_info *indexes,
    size_t index_count,
    const char *index_name
);
static int validate_alter_table_add_index_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int validate_alter_table_add_fulltext_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int validate_alter_table_add_spatial_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int append_loaded_add_index_part(
    struct mylite_db *database,
    struct planned_alter_table_add_index *plan,
    const struct mylite_sql_ast_node *part,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
);
static const char *add_index_unqualified_column_error(enum mylite_catalog_index_kind kind);
static int resolve_loaded_add_index_part_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    char *column_name,
    size_t column_name_size,
    size_t *out_column_index
);
static int reject_duplicate_loaded_add_index_part_column(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan,
    const struct mylite_catalog_column_descriptor *column,
    const char *column_name
);
static int validate_loaded_add_index_part_attributes(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan,
    const struct mylite_sql_ast_node *part,
    const struct mylite_catalog_column_descriptor *column,
    const char *column_name,
    int64_t *out_prefix_length
);
static int make_loaded_add_index_column_descriptor(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan,
    const struct mylite_sql_ast_node *part,
    const struct mylite_catalog_column_descriptor *column,
    int64_t prefix_length,
    struct mylite_catalog_index_column_descriptor *out_descriptor
);
static int loaded_add_index_prefix_covers_full_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    int64_t prefix_length,
    bool *out_covers_full_column
);
static int reserve_planned_alter_table_add_index_parts(
    struct mylite_db *database,
    struct planned_alter_table_add_index *plan,
    size_t required_capacity
);
static int validate_secondary_index_prefix_for_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    int64_t prefix_length,
    uint64_t *out_key_bytes
);
static int column_descriptor_index_part_key_bytes(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    uint64_t *out_key_bytes
);
static int validate_loaded_index_part_list(
    struct mylite_db *database,
    const struct loaded_index_part *parts,
    size_t part_count,
    bool is_unique
);
static int validate_loaded_unique_index_part_list(
    struct mylite_db *database,
    const struct loaded_index_part *parts,
    size_t part_count
);
static int validate_create_unique_index_existing_rows(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
);
static int validate_create_unique_index_string_values(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
);
static size_t count_create_unique_index_string_validation_parts(
    const struct planned_alter_table_add_index *plan
);
static int validate_create_unique_index_string_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t string_part_count
);
static int validate_create_unique_index_duplicates(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
);
static int execute_physical_add_index(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan,
    const char *index_physical_name
);
static int plan_alter_table_drop_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_drop_index *out_plan
);
static int plan_drop_index(
    struct mylite_db *database,
    struct drop_index_nodes nodes,
    const char *statement_name,
    struct planned_drop_index *out_plan
);
static void planned_drop_index_deinit(struct planned_drop_index *plan);
static int drop_index_from_plan(struct mylite_db *database, const struct planned_drop_index *plan);
static int drop_persistent_index_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_drop_index *plan,
    bool *out_physical_schema_changed
);
static int plan_alter_table_rename_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_rename_index *out_plan
);
static void planned_rename_index_deinit(struct planned_rename_index *plan);
static int rename_index_from_plan(
    struct mylite_db *database,
    const struct planned_rename_index *plan
);
static bool loaded_index_kind_is_secondary_or_fulltext(enum mylite_catalog_index_kind kind);
static int plan_alter_table_add_check(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_check_constraint *out_plan
);
static int plan_alter_table_drop_check(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_check_constraint *out_plan
);
static int plan_alter_table_alter_check(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_check_constraint *out_plan
);
static int plan_alter_table_check_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    struct planned_alter_table_check_constraint *out_plan
);
static int populate_alter_table_check_rebuild_plan(
    struct mylite_db *database,
    struct planned_alter_table_check_constraint *out_plan
);
static int copy_alter_table_check_columns(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_check_constraint *out_plan
);
static int copy_alter_table_check_indexes(
    struct mylite_db *database,
    const struct loaded_index_info *indexes,
    size_t index_count,
    struct planned_alter_table_check_constraint *out_plan
);
static int copy_alter_table_check_primary_index(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    struct planned_alter_table_check_constraint *out_plan
);
static int copy_alter_table_check_secondary_index(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    struct planned_alter_table_check_constraint *out_plan
);
static int copy_alter_table_check_constraints(
    struct mylite_db *database,
    const struct loaded_check_constraint_info *check_constraints,
    size_t check_constraint_count,
    struct planned_alter_table_check_constraint *out_plan
);
static int plan_alter_table_add_check_definition(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *check_constraint,
    struct planned_alter_table_check_constraint *out_plan
);
static int next_available_planned_check_constraint_generated_ordinal(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    int64_t *out_ordinal
);
static int find_planned_check_constraint_by_name(
    const struct planned_create_table *plan,
    const char *check_constraint_name,
    size_t *out_index
);
static void remove_planned_check_constraint_at(struct planned_create_table *plan, size_t index);
static int alter_table_check_constraint_from_plan(
    struct mylite_db *database,
    struct planned_alter_table_check_constraint *plan
);
static int insert_alter_table_check_constraint_catalog_row(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    struct planned_alter_table_check_constraint *plan
);
static int execute_physical_alter_table_check_constraint(
    struct mylite_db *database,
    const struct planned_alter_table_check_constraint *plan,
    uint64_t sqlite_schema_generation,
    int64_t *out_affected_rows
);
static int execute_alter_table_check_copy_rows(
    struct mylite_db *database,
    const struct planned_alter_table_check_constraint *plan,
    const char *sql,
    int64_t *out_affected_rows,
    bool *out_copy_failed
);
static void planned_alter_table_check_constraint_deinit(
    struct planned_alter_table_check_constraint *plan
);
static bool loaded_index_name_is_used_by_other(
    const struct loaded_index_info *indexes,
    size_t index_count,
    const struct loaded_index_info *skipped_index,
    const char *index_name
);
static int find_loaded_index_by_name(
    const struct loaded_index_info *indexes,
    size_t index_count,
    const char *index_name,
    size_t *out_index
);
static int validate_drop_index_auto_increment(
    struct mylite_db *database,
    const struct loaded_index_info *target,
    const struct loaded_index_info *indexes,
    size_t index_count
);
static bool loaded_other_index_contains_column(const struct loaded_index_column_lookup *lookup);
static int execute_physical_drop_index(
    struct mylite_db *database,
    const struct planned_drop_index *plan
);
static int execute_physical_drop_index_by_name(struct mylite_db *database, const char *name);
static int plan_alter_table_drop_primary_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_drop_primary_key *out_plan
);
static int plan_alter_table_drop_primary_key_for_multi_action(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_drop_primary_key *out_plan
);
static int plan_alter_table_drop_primary_key_with_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool validate_auto_increment,
    struct planned_alter_table_drop_primary_key *out_plan
);
static int plan_alter_table_drop_primary_key_for_table_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_name_node,
    bool validate_auto_increment,
    struct planned_alter_table_drop_primary_key *out_plan
);
static void planned_alter_table_drop_primary_key_deinit(
    struct planned_alter_table_drop_primary_key *plan
);
static int validate_alter_table_drop_primary_key_auto_increment(
    struct mylite_db *database,
    const struct planned_alter_table_drop_primary_key *plan,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
);
static bool primary_key_auto_increment_column_id(
    const struct planned_alter_table_drop_primary_key *plan,
    int64_t *out_column_id
);
static int alter_table_drop_primary_key_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_drop_primary_key *plan
);
static int alter_table_drop_primary_key_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_drop_primary_key *plan
);
static int execute_physical_alter_table_drop_primary_key(
    struct mylite_db *database,
    const struct planned_alter_table_drop_primary_key *plan
);
static int plan_alter_table_auto_increment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_auto_increment *out_plan
);
static int parse_alter_table_auto_increment_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option,
    int64_t *out_next
);
static int find_alter_table_auto_increment_column(
    struct mylite_db *database,
    const struct planned_alter_table_auto_increment *plan,
    struct mylite_catalog_column_descriptor *out_column,
    bool *out_found
);
static int alter_table_auto_increment_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_auto_increment *plan
);
static int compute_alter_table_auto_increment_effective_next(
    struct mylite_db *database,
    const struct planned_alter_table_auto_increment *plan,
    int64_t *out_effective_next
);
static int read_alter_table_auto_increment_row_next(
    struct mylite_db *database,
    const struct planned_alter_table_auto_increment *plan,
    const struct integer_column_range *range,
    int64_t *out_row_next
);
static int plan_alter_table_drop_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_drop_column *out_plan
);
static int plan_alter_table_drop_column_dependencies(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    struct planned_alter_table_drop_column *out_plan
);
static void planned_alter_table_drop_column_deinit(struct planned_alter_table_drop_column *plan);
static int plan_drop_column_key_dependencies(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_drop_column *out_plan
);
static int reject_drop_column_foreign_key_dependencies(
    struct mylite_db *database,
    const struct planned_alter_table_drop_column *plan,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
);
static int append_drop_column_index_update(
    struct mylite_db *database,
    struct planned_alter_table_drop_column *plan,
    const struct loaded_index_info *index
);
static int reserve_drop_column_index_updates(
    struct planned_alter_table_drop_column *plan,
    size_t required_capacity
);
static bool loaded_index_find_column_ordinal(
    const struct loaded_index_info *index,
    int64_t column_id,
    int64_t *out_ordinal_position
);
static int validate_drop_column_unique_key_updates(
    struct mylite_db *database,
    struct planned_alter_table_drop_column *plan,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
);
static int validate_drop_column_unique_key_update(
    struct mylite_db *database,
    const struct planned_alter_table_drop_column *plan,
    const struct planned_drop_column_index_update *update
);
static int alter_table_drop_column_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_drop_column *plan
);
static int execute_physical_drop_column_prepare_indexes(
    struct mylite_db *database,
    const struct planned_alter_table_drop_column *plan
);
static int execute_physical_drop_column_recreate_indexes(
    struct mylite_db *database,
    const struct planned_alter_table_drop_column *plan
);
static int execute_physical_drop_column_create_index(
    struct mylite_db *database,
    const struct planned_alter_table_drop_column *plan,
    const struct planned_drop_column_index_update *update
);
static int plan_alter_table_rename_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_rename_column *out_plan
);
static int alter_table_rename_column_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_rename_column *plan
);
static int plan_alter_table_modify_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_modify_column *out_plan
);
static int plan_alter_table_change_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_modify_column *out_plan
);
static int plan_alter_table_modify_column_position(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *position,
    struct planned_alter_table_modify_column *out_plan
);
static int plan_alter_table_set_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_set_default *out_plan
);
static void planned_alter_table_set_default_deinit(struct planned_alter_table_set_default *plan);
static int alter_table_set_default_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_set_default *plan
);
static int alter_table_set_default_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_set_default *plan
);
static int plan_alter_table_drop_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_drop_default *out_plan
);
static int alter_table_drop_default_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_drop_default *plan
);
static int alter_table_drop_default_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_drop_default *plan
);
static int plan_alter_table_column_visibility(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_column_visibility *out_plan
);
static int alter_table_column_visibility_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_column_visibility *plan
);
static int plan_alter_table_index_visibility(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_index_visibility *out_plan
);
static int alter_table_index_visibility_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_index_visibility *plan
);
static void planned_alter_table_index_visibility_deinit(
    struct planned_alter_table_index_visibility *plan
);
static int plan_alter_table_default_charset_collation(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_default_charset_collation *out_plan
);
static int apply_alter_table_default_charset_collation_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_alter_table_default_charset_collation *plan
);
static int alter_table_default_charset_collation_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_default_charset_collation *plan
);
static int plan_alter_table_convert_character_set(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_convert_character_set *out_plan
);
static int apply_alter_table_convert_character_set_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_alter_table_convert_character_set *plan
);
static int apply_alter_table_convert_character_set_default(
    struct mylite_db *database,
    struct planned_alter_table_convert_character_set *plan
);
static int validate_alter_table_convert_character_set_columns(
    struct mylite_db *database,
    const struct planned_alter_table_convert_character_set *plan
);
static bool table_charset_option_is_default_keyword(
    const struct mylite_sql_ast_node *charset_option
);
static bool alter_table_convert_column_participates(
    const struct mylite_catalog_column_descriptor *column
);
static bool alter_table_convert_column_needs_explicit_metadata_update(
    const struct mylite_catalog_column_descriptor *column,
    const struct planned_alter_table_convert_character_set *plan
);
static int alter_table_convert_character_set_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_convert_character_set *plan
);
static void planned_alter_table_convert_character_set_deinit(
    struct planned_alter_table_convert_character_set *plan
);
static int plan_alter_table_comment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_comment *out_plan
);
static int alter_table_comment_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_comment *plan
);
static int plan_alter_schema_default_charset_collation(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_schema_default_charset_collation *out_plan
);
static int alter_schema_default_charset_collation_from_plan(
    struct mylite_db *database,
    const struct planned_alter_schema_default_charset_collation *plan
);
static int plan_alter_table_order_by(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_order_by *out_plan
);
static int plan_alter_table_order_by_items(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_items,
    const struct select_source_context *source_context,
    struct planned_alter_table_order_by *out_plan
);
static int plan_alter_table_order_by_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_alter_table_order_by_item *out_item
);
static void planned_alter_table_order_by_deinit(struct planned_alter_table_order_by *plan);
static int alter_table_order_by_from_plan(
    struct mylite_db *database,
    struct planned_alter_table_order_by *plan
);
static int execute_physical_alter_table_order_by(
    struct mylite_db *database,
    const struct planned_alter_table_order_by *plan,
    int64_t *out_affected_rows
);
static int plan_alter_table_force(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_force *out_plan
);
static void planned_alter_table_force_deinit(struct planned_alter_table_force *plan);
static int alter_table_force_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_force *plan
);
static int execute_physical_alter_table_force(
    struct mylite_db *database,
    const struct planned_alter_table_force *plan
);
static int plan_alter_table_key_maintenance(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_key_maintenance *out_plan
);
static int build_alter_table_check_temporary_physical_name(
    const struct planned_alter_table_check_constraint *plan,
    uint64_t sqlite_schema_generation,
    char *destination,
    size_t destination_size
);
static int build_alter_table_check_create_sql(
    const struct planned_alter_table_check_constraint *plan,
    const char *temporary_physical_name,
    char **out_sql
);
static int build_alter_table_check_copy_sql(
    const struct planned_alter_table_check_constraint *plan,
    const char *temporary_physical_name,
    char **out_sql
);
static int append_alter_table_check_column_list(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_check_constraint *plan
);
static void planned_column_from_catalog_descriptor(
    const struct mylite_catalog_column_descriptor *descriptor,
    const struct mylite_sql_ast_node *default_node,
    struct planned_column *out_column
);
static int resolve_alter_table_column_replacement_plan(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *out_plan
);
static int apply_alter_table_modify_primary_key_nullability(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *out_plan
);
static int validate_alter_table_modify_indexes(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *plan
);
static int refresh_alter_table_modify_index_parts(struct planned_alter_table_modify_column *plan);
static int validate_alter_table_modify_primary_index(
    struct mylite_db *database,
    const struct loaded_index_info *index
);
static int validate_alter_table_modify_fulltext_index(
    struct mylite_db *database,
    const struct loaded_index_info *index
);
static int validate_alter_table_modify_spatial_index(
    struct mylite_db *database,
    const struct loaded_index_info *index
);
static int resolve_alter_table_modify_column_position(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_modify_column *out_plan
);
static int complete_alter_table_modify_column_plan(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    struct planned_alter_table_modify_column *out_plan
);
static bool modify_column_temporal_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool modify_column_string_to_integer_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool modify_column_definition_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool modify_column_type_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool modify_column_metadata_only_replacement(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool modify_column_name_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static void planned_alter_table_modify_column_deinit(
    struct planned_alter_table_modify_column *plan
);
static int alter_table_modify_column_from_plan(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *plan
);
static int collect_modify_column_rebuild_columns(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_modify_column *out_plan
);
static int validate_modify_column_existing_rows(
    struct mylite_db *database,
    const struct planned_alter_table_modify_column *plan,
    int64_t *out_row_count
);
static int validate_modify_column_null_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
);
static int validate_existing_integer_for_column(
    struct mylite_db *database,
    int64_t value,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    const char *unsupported_message
);
static int validate_existing_text_for_column(
    struct mylite_db *database,
    const unsigned char *text,
    int byte_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number
);
static int validate_existing_text_integer_for_column(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number
);
static int validate_existing_binary_bytes_for_column(
    struct mylite_db *database,
    const void *bytes,
    int byte_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number
);
static void make_modify_target_descriptor(
    const struct planned_alter_table_modify_column *plan,
    struct mylite_catalog_column_descriptor *out_column
);
static int rename_table_from_plan_with_policy(
    struct mylite_db *database,
    const struct planned_rename_table *plan,
    bool allow_same_object_noop,
    const char *unsupported_object_message
);
static int rename_table_pair_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_rename_table *plan,
    bool allow_same_object_noop,
    const char *unsupported_object_message
);
static int reject_rename_table_check_constraint_collisions(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *source,
    const struct planned_rename_table *plan
);
static int build_renamed_check_constraint_name(
    struct mylite_db *database,
    const struct planned_rename_table *plan,
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    char *destination,
    size_t destination_size
);
static int reject_rename_table_check_constraint_schema_collision(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *source,
    const struct planned_rename_table *plan,
    const char *check_constraint_name
);
static int reject_rename_table_check_constraint_schema_collision_callback(
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    void *user_data
);

static int plan_insert(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert *out_plan
);
static int plan_insert_set(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert *out_plan
);
static int plan_load_data_infile(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_load_data_infile *out_plan
);
static void planned_load_data_infile_deinit(struct planned_load_data_infile *plan);
static int execute_load_data_infile_from_plan(
    struct mylite_db *database,
    const struct planned_load_data_infile *plan,
    mylite_result *result
);
static int collect_load_data_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    const struct planned_insert *plan,
    size_t **out_indexes,
    size_t *out_index_count
);
static int validate_load_data_target_columns(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const size_t *target_indexes,
    size_t target_count
);
static int parse_load_data_ignore_line_count(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_value
);
static int execute_load_data_file_rows(
    struct mylite_db *database,
    FILE *file,
    sqlite3_stmt *statement,
    const struct planned_load_data_infile *plan,
    struct insert_execution_counters *counters
);
static int finish_load_data_field(
    struct mylite_db *database,
    struct load_data_row *row,
    struct mylite_dynamic_string *field,
    bool *escape_pending
);
static int load_data_row_append_field(
    struct mylite_db *database,
    struct load_data_row *row,
    char *raw_text,
    size_t raw_text_length
);
static int decode_load_data_field_text(
    struct mylite_db *database,
    char *raw_text,
    size_t raw_text_length,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int process_load_data_row(
    struct mylite_db *database,
    const struct planned_load_data_infile *plan,
    sqlite3_stmt *statement,
    const struct load_data_row *row,
    uint64_t physical_row_number,
    size_t *loaded_row_count,
    struct insert_execution_counters *counters
);
static int execute_load_data_import_row(
    struct mylite_db *database,
    const struct planned_load_data_infile *plan,
    sqlite3_stmt *statement,
    const struct load_data_row *input_row,
    size_t row_number,
    struct insert_execution_counters *counters
);
static int convert_load_data_row_values(
    struct mylite_db *database,
    const struct planned_load_data_infile *plan,
    const struct load_data_row *input_row,
    size_t row_number,
    struct planned_insert_row *out_row
);
static int convert_load_data_field_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const struct load_data_field *field,
    size_t row_number,
    struct planned_value *out_value
);
static int convert_load_data_missing_field_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static bool load_data_missing_field_stores_null(
    const struct mylite_catalog_column_descriptor *column
);
static int convert_load_data_empty_temporal_field(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const struct load_data_field *field,
    size_t row_number,
    struct planned_value *out_value,
    bool *out_handled
);
static int convert_load_data_null_field(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    struct planned_value *out_value
);
static int convert_load_data_integer_field(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const struct load_data_field *field,
    size_t row_number,
    struct planned_value *out_value
);
static int convert_load_data_text_field(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const struct load_data_field *field,
    size_t row_number,
    struct planned_value *out_value
);
static char *copy_load_data_field_text(
    struct mylite_db *database,
    const struct load_data_field *field
);
static void load_data_row_deinit(struct load_data_row *row);
static void load_data_row_reset(struct load_data_row *row);
static void planned_insert_deinit(struct planned_insert *plan);
static void planned_value_deinit(struct planned_value *value);
static int execute_insert_from_plan(
    struct mylite_db *database,
    const struct planned_insert *plan,
    mylite_result *result
);
static int append_insert_duplicate_update_warnings_if_needed(
    struct mylite_db *database,
    const struct planned_insert *plan
);
static int plan_insert_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert_select *out_plan
);
static int reject_insert_select_generated_targets(
    struct mylite_db *database,
    const struct planned_insert_select *plan
);
static enum planned_insert_select_source_kind insert_select_source_kind(
    const struct mylite_sql_ast_node *source_statement
);
static bool insert_select_source_is_row_scalar(const struct mylite_sql_ast_node *select_statement);
static int plan_insert_select_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert_select *out_plan,
    struct primary_key_info *primary_key
);
static void apply_insert_select_primary_key_info(
    const struct primary_key_info *primary_key,
    struct planned_insert *target
);
static int plan_insert_select_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    enum planned_insert_select_source_kind source_kind,
    struct planned_insert_select *out_plan
);
static int plan_insert_select_row_scalar_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_statement,
    struct planned_insert_select *out_plan
);
static int plan_insert_select_table_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert_select *out_plan
);
static int plan_insert_select_compound_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert_select *out_plan
);
static int plan_insert_select_compound_branch(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *branch_statement,
    enum mylite_sql_ast_union_modifier modifier,
    struct planned_insert_select_compound_branch *out_branch,
    size_t *out_column_count
);
static int plan_insert_select_compound_table_branch(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *branch_statement,
    struct planned_insert_select_compound_branch *out_branch,
    size_t *out_column_count
);
static int plan_insert_select_compound_row_scalar_branch(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *branch_statement,
    struct planned_insert_select_compound_branch *out_branch,
    size_t *out_column_count
);
static int validate_insert_select_compound_branch_target_compatibility(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    const struct planned_select *source
);
static int validate_insert_select_compound_target_compatibility(
    struct mylite_db *database,
    const struct planned_insert_select *plan
);
static void planned_insert_select_compound_source_deinit(
    struct planned_insert_select_compound_source *source
);
static void planned_insert_select_deinit(struct planned_insert_select *plan);
static int execute_insert_select_from_plan(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    mylite_result *result
);
static int execute_insert_select_row_scalar_from_plan(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    mylite_result *result
);
static int execute_insert_select_table_from_plan(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    mylite_result *result
);
static int prepare_insert_select_table_execution(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    struct insert_select_table_execution *execution
);
static int execute_insert_select_table(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    struct insert_select_table_execution *execution
);
static int validate_insert_select_table_rows(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    const struct insert_select_table_execution *execution
);
static int finish_insert_select_table_side_effects(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    struct insert_select_table_execution *execution
);
static void cleanup_insert_select_table_execution(
    struct mylite_db *database,
    struct insert_select_table_execution *execution,
    int rc
);
static int normalize_insert_select_table_result(struct mylite_db *database, int rc);
static int finish_insert_select_table_result(
    struct mylite_db *database,
    const struct insert_select_table_execution *execution,
    mylite_result *result
);
static int prepare_insert_select_row_scalar_source(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    sqlite3_stmt **out_statement,
    char **out_sql,
    bool *out_has_row
);
static int materialize_insert_select_row_scalar_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan,
    struct planned_insert_row *row
);
static int finish_insert_select_row_scalar_result(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    bool has_row,
    struct planned_insert_row *row,
    mylite_result *result
);
static void planned_insert_row_deinit(struct planned_insert_row *row, size_t column_count);
static int normalize_insert_select_row_scalar_result(struct mylite_db *database, int rc);
static bool insert_select_allows_adjustment(
    const struct mylite_db *database,
    const struct planned_insert_select *plan
);
static int validate_insert_select_row_scalar_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan,
    size_t row_number
);
static int execute_insert_select_row_scalar_insert(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    struct planned_insert_row *row,
    mylite_result *result
);
static int execute_insert_select_materialize(
    struct mylite_db *database,
    const char *materialize_sql,
    const struct planned_insert_select *plan,
    bool *out_temporary_table_created
);
static int execute_insert_select_insert(
    struct mylite_db *database,
    const char *validation_sql,
    const char *insert_sql,
    const struct planned_insert_select *plan,
    struct insert_execution_counters *counters
);
static int execute_insert_select_insert_rows(
    struct mylite_db *database,
    struct insert_select_execution_statements *statements,
    const struct planned_insert_select *plan,
    struct insert_execution_counters *counters
);
static int execute_insert_select_insert_row(
    struct mylite_db *database,
    struct insert_select_execution_statements *statements,
    const struct planned_insert_select *plan,
    size_t row_number,
    struct insert_execution_counters *counters
);
static int plan_insert_select_auto_increment_row_value(
    struct mylite_db *database,
    struct planned_insert *row_plan,
    struct insert_execution_counters *counters
);
static int materialize_insert_select_row_values(
    struct mylite_db *database,
    sqlite3_stmt *select_statement,
    const struct planned_insert_select *plan,
    size_t row_number,
    struct planned_value *values
);
static int materialize_insert_select_selected_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    bool adjust_value,
    struct planned_value *out_value
);
static int materialize_insert_select_integer_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    bool adjust_value,
    struct planned_value *out_value
);
static int materialize_insert_select_string_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    bool adjust_value,
    struct planned_value *out_value
);
static int materialize_sqlite_binary_string_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    struct planned_value *out_value
);
static int materialize_sqlite_bit_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    struct planned_value *out_value
);
static int materialize_insert_select_raw_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    struct planned_value *out_value
);
static int materialize_insert_select_omitted_value(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    size_t column_index,
    struct planned_value *out_value
);
static int validate_insert_select_rows(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan
);
static int validate_insert_select_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan,
    size_t row_number
);
static int insert_select_source_column_for_position(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan,
    size_t target_position,
    const struct mylite_catalog_column_descriptor **out_source_column
);
static int validate_insert_select_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    bool adjust_value
);
static bool insert_select_source_target_types_are_compatible(
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column
);
static const char *insert_select_implicit_conversion_message(
    const struct mylite_catalog_column_descriptor *target_column
);
static int validate_insert_select_convertible_string_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    struct insert_select_string_validation validation
);
static int validate_insert_select_convertible_varchar_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    struct insert_select_string_validation validation
);
static int validate_insert_select_convertible_char_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    struct insert_select_string_validation validation
);
static int validate_insert_select_convertible_text_family_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    struct insert_select_string_validation validation
);
static int validate_insert_select_string_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
);
static int validate_insert_select_json_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column
);
static int validate_insert_select_binary_string_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
);
static int validate_insert_select_bit_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
);
static int validate_insert_select_decimal_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
);
static int validate_insert_select_approximate_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
);
static int validate_insert_select_date_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
);
static int validate_insert_select_time_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
);
static int validate_insert_select_datetime_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
);
static int validate_insert_select_timestamp_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
);
static int validate_insert_select_year_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
);
static int validate_insert_select_integer_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    bool adjust_value
);

static int plan_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool allow_order_by_field,
    struct planned_select *out_plan
);
static const struct mylite_sql_ast_node *from_table_alias_node(
    const struct mylite_sql_ast_node *from_table
);
static const struct mylite_sql_ast_node *from_table_index_hint_list_node(
    const struct mylite_sql_ast_node *from_table
);
static int validate_select_index_hints(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_table,
    const struct mylite_catalog_table_descriptor *table
);
static int validate_select_index_hint(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *hint,
    const struct mylite_catalog_table_descriptor *table,
    bool *has_use,
    bool *has_force
);
static const struct mylite_sql_ast_node *index_hint_name_list_node(
    const struct mylite_sql_ast_node *hint
);
static int validate_select_index_hint_names(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_list,
    const struct mylite_catalog_table_descriptor *table
);
static int select_index_hint_name_exists(
    struct mylite_db *database,
    int64_t table_id,
    const char *index_name,
    bool *out_exists
);
static int match_select_index_hint_name(
    const struct mylite_catalog_index_descriptor *index,
    void *user_data
);
static int collect_select_optional_clauses(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct select_optional_clauses *out_clauses
);
static int plan_select_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
);
static int validate_select_distinct_order(
    struct mylite_db *database,
    const struct planned_select *plan
);
static bool select_distinct_order_item_is_selected(
    const struct planned_select *plan,
    const struct planned_select_order_item *item
);
static int validate_select_distinct_columns(
    struct mylite_db *database,
    const struct planned_select *plan
);
static bool select_distinct_column_is_supported(
    const struct mylite_catalog_column_descriptor *column
);
static void copy_plan_source_alias_if_present(
    struct planned_select *plan,
    const struct select_source_context *context
);
static int plan_joined_select(
    struct mylite_db *database,
    const struct joined_select_ast *joined_select,
    struct planned_select *out_plan
);
static int prepare_joined_select_plan(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_select *out_plan,
    struct joined_select_temp_nodes *temp_nodes
);
static void joined_select_temp_nodes_deinit(struct joined_select_temp_nodes *temp_nodes);
static size_t joined_select_source_count(const struct mylite_sql_ast_node *from_clause);
static int collect_joined_select_parts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_select *out_plan,
    const struct joined_select_temp_nodes *temp_nodes
);
static int plan_joined_select_sources(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *const *source_nodes,
    struct planned_select *out_plan
);
static int reject_unsupported_multi_source_join_edges(
    struct mylite_db *database,
    const struct planned_select *plan
);
static int plan_joined_select_conditions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *const *join_condition_nodes,
    size_t join_condition_node_count,
    struct planned_select *out_plan
);
static int plan_joined_select_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node,
    struct planned_select_source *out_source
);
static int init_join_select_source_context(
    const struct planned_select_source *sources,
    size_t source_count,
    struct select_source_context *out_context
);
static int reject_duplicate_select_source_reference(
    struct mylite_db *database,
    const struct planned_select_source *sources,
    size_t source_count
);
static const char *planned_select_source_reference_name(const struct planned_select_source *source);
static int plan_joined_select_condition(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *condition_node,
    enum mylite_sql_ast_join_kind join_kind,
    const struct select_source_context *source_context,
    struct planned_select_join_condition *out_condition
);
static bool join_condition_columns_are_compatible(
    const struct mylite_catalog_column_descriptor *left,
    const struct mylite_catalog_column_descriptor *right
);
static bool join_condition_column_is_integer_family(
    const struct mylite_catalog_column_descriptor *column
);
static int append_select_column_from_source(
    struct mylite_db *database,
    struct planned_select *plan,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    const struct mylite_sql_ast_node *alias
);
static void planned_select_order_deinit(struct planned_select_order *order);
static void planned_select_join_condition_deinit(
    struct planned_select_join_condition *condition
);
static void planned_select_deinit(struct planned_select *plan);
static int execute_select_from_plan(
    struct mylite_db *database,
    const struct planned_select *plan,
    mylite_result **out_result
);
static int set_select_found_row_count(
    struct mylite_db *database,
    const struct planned_select *plan,
    mylite_result *result
);
static int found_row_count_for_select_limit_envelope(
    struct mylite_db *database,
    const struct planned_select *plan,
    size_t visible_row_count,
    uint64_t *out_found_row_count
);
static int read_select_found_row_count(
    struct mylite_db *database,
    const struct planned_select *plan,
    int64_t *out_count
);
static bool select_statement_is_row_scalar_projection_attempt(
    const struct mylite_sql_ast_node *statement
);
static int execute_row_scalar_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int plan_row_scalar_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool allow_order_by_field,
    struct planned_row_scalar_select *out_plan
);
static int plan_count_having_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_count_having_select *out_plan
);
static int collect_count_having_select_clauses(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct select_optional_clauses *out_clauses
);
static int plan_count_having_select_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_optional_clauses *clauses,
    const struct select_source_context *source_context,
    struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_count_having_select *out_plan
);
static bool select_list_is_descriptor_projection(
    const struct mylite_sql_ast_node *select_list
);
static int plan_count_having_descriptor_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_optional_clauses *clauses,
    const struct select_source_context *source_context,
    struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_count_having_select *out_plan
);
static int plan_count_having_row_scalar_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_optional_clauses *clauses,
    const struct select_source_context *source_context,
    struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_count_having_select *out_plan
);
static int plan_count_having_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *having_clause,
    enum mylite_sql_ast_operator *out_operator_kind,
    struct planned_value *out_value
);
static const struct mylite_sql_ast_node *count_having_comparison_node(
    const struct mylite_sql_ast_node *having_clause
);
static bool count_having_comparison_uses_count_star(
    const struct mylite_sql_ast_node *comparison
);
static void planned_count_having_select_deinit(struct planned_count_having_select *plan);
static int collect_row_scalar_select_clauses(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *optional_clause,
    struct row_scalar_select_clauses *out_clauses
);
static int plan_row_scalar_select_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_row_scalar_select *out_plan,
    struct select_source_context *out_source_context,
    struct mylite_catalog_column_descriptor **out_table_columns,
    size_t *out_table_column_count
);
static int plan_row_scalar_select_join_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_row_scalar_select *out_plan,
    struct select_source_context *out_source_context
);
static int plan_row_scalar_select_row_envelope(
    struct mylite_db *database,
    const struct row_scalar_select_clauses *clauses,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_order_by_field,
    struct planned_row_scalar_select *out_plan
);
static int plan_row_scalar_select_tableless_filter(
    struct mylite_db *database,
    const struct row_scalar_select_clauses *clauses,
    struct planned_row_scalar_select *out_plan
);
static int plan_row_scalar_select_exists_filter(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    struct planned_row_scalar_select *out_plan
);
static bool row_scalar_select_where_is_exists_filter(
    const struct mylite_sql_ast_node *where_clause
);
static int plan_row_scalar_select_scalar_filter(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    struct planned_row_scalar_select *out_plan
);
static bool planned_row_scalar_tableless_filter_is_supported(
    const struct planned_select_predicate *predicate
);
static bool planned_row_scalar_tableless_filter_node_is_supported(
    const struct planned_select_predicate_node *node
);
static void planned_row_scalar_select_deinit(struct planned_row_scalar_select *plan);
static int execute_row_scalar_select_from_plan(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    mylite_result **out_result
);
static int execute_count_having_select_from_plan(
    struct mylite_db *database,
    const struct planned_count_having_select *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int append_count_having_select_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_count_having_select *plan,
    const struct result_column_metadata_context *metadata_context
);
static int append_count_having_row_scalar_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_count_having_select *plan,
    const struct result_column_metadata_context *metadata_context
);
static const char *count_having_row_scalar_literal_label(
    const struct planned_row_scalar_select_item *item
);
static int row_scalar_select_step_error(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int sqlite_rc
);
static bool map_json_extract_row_scalar_step_error(struct mylite_db *database, const char *message);
static bool map_json_mutation_row_scalar_step_error(
    struct mylite_db *database,
    const char *message
);
static bool map_json_search_row_scalar_step_error(struct mylite_db *database, const char *message);
static bool map_json_type_row_scalar_step_error(struct mylite_db *database, const char *message);
static bool map_json_constructor_row_scalar_step_error(
    struct mylite_db *database,
    const char *message
);
static int append_row_scalar_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context
);
static int append_row_scalar_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context,
    const struct planned_row_scalar_select_item *item
);
static int make_row_scalar_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context,
    const struct planned_row_scalar_select_item *item,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
);
static int make_row_scalar_column_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context,
    const struct planned_row_scalar_select_item *item,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
);
static int populate_row_scalar_expression_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_window_function_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_window_value_function_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_window_value_column_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_window_value_literal_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_value *value,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_conversion_result_column_descriptor(
    const struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_date_interval_second_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int copy_row_scalar_result_column_name(
    struct mylite_db *database,
    const struct planned_row_scalar_select_item *item,
    char **out_name
);
static int set_row_scalar_select_found_row_count(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    mylite_result *result
);
static bool select_statement_has_group_by_clause(const struct mylite_sql_ast_node *statement);
static int plan_grouped_aggregate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_grouped_aggregate *out_plan
);
static void planned_grouped_aggregate_deinit(struct planned_grouped_aggregate *plan);
static int collect_grouped_aggregate_clauses(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct grouped_aggregate_clauses *out_clauses
);
static int plan_grouped_aggregate_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_grouped_aggregate *out_plan,
    struct select_source_context *out_source_context,
    struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count
);
static int plan_grouped_aggregate_join_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_grouped_aggregate *out_plan,
    struct select_source_context *out_source_context
);
static int validate_grouped_projection_references(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
);
static int validate_grouped_qualified_wildcard_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *qualified_wildcard,
    const struct select_source_context *source_context
);
static int plan_grouped_aggregate_group_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct mylite_sql_ast_node *group_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int resolve_grouped_aggregate_group_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct mylite_sql_ast_node *group_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_key *out_key
);
static int resolve_grouped_select_alias_group_column(
    struct mylite_db *database,
    const struct grouped_alias_group_resolution *resolution,
    struct planned_grouped_key *out_key,
    bool *out_matched
);
static int count_grouped_source_columns_by_unqualified_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *group_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    size_t *out_match_count
);
static size_t grouped_aggregate_group_key_count(const struct mylite_sql_ast_node *group_clause);
static int validate_grouped_aggregate_group_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_grouped_aggregate_projection_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    size_t *out_first_aggregate_index
);
static int append_grouped_projection_column(
    struct mylite_db *database,
    struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
);
static int plan_grouped_aggregate_wildcard_projection_columns(
    struct mylite_db *database,
    size_t select_expression_index,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int plan_grouped_aggregate_qualified_wildcard_projection_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *qualified_wildcard,
    size_t select_expression_index,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int append_visible_grouped_projection_columns_from_source(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    size_t source_index,
    struct planned_grouped_aggregate *out_plan
);
static int validate_grouped_projection_column(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    size_t select_expression_index
);
static int grouped_source_primary_key_is_covered(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    size_t source_index,
    bool *out_covered
);
static bool grouped_source_columns(
    const struct planned_grouped_aggregate *plan,
    size_t source_index,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count,
    int64_t *out_table_id
);
static bool grouped_projection_column_is_allowed(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    int *out_rc
);
static int plan_grouped_aggregate_functions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    size_t first_aggregate_index,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int plan_grouped_aggregate_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *aggregate_item,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    struct planned_grouped_aggregate_item *out_item
);
static int plan_grouped_count_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
);
static int plan_grouped_any_value_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
);
static int plan_grouped_column_aggregate_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    enum planned_column_aggregate_function aggregate_function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
);
static int plan_grouped_group_concat_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate_item *out_item
);
static int plan_group_concat_value_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
);
static int plan_group_concat_order_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool qualify_column_reference,
    struct planned_select_order *out_order
);
static int plan_group_concat_separator(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    bool *out_has_separator,
    struct planned_value *out_separator
);
static const struct mylite_sql_ast_node *group_concat_value_node(
    const struct mylite_sql_ast_node *function
);
static const struct mylite_sql_ast_node *group_concat_order_node(
    const struct mylite_sql_ast_node *function
);
static const struct mylite_sql_ast_node *group_concat_separator_node(
    const struct mylite_sql_ast_node *function
);
static enum planned_grouped_aggregate_function grouped_aggregate_function_from_expression(
    const struct mylite_sql_ast_node *expression
);
static enum planned_column_aggregate_function grouped_column_aggregate_function(
    enum planned_grouped_aggregate_function function
);
static bool grouped_aggregate_function_has_column(enum planned_grouped_aggregate_function function);
static bool grouped_aggregate_function_is_bitwise(enum planned_grouped_aggregate_function function);
static bool planned_grouped_aggregate_columns_match(
    const struct mylite_catalog_column_descriptor *left_column,
    size_t left_source_index,
    const struct mylite_catalog_column_descriptor *right_column,
    size_t right_source_index
);
static bool find_matching_grouped_key(
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    size_t *out_group_index
);
static bool find_grouped_key_by_unqualified_name(
    const struct planned_grouped_aggregate *plan,
    const char *name,
    size_t *out_group_index,
    bool *out_ambiguous
);
static bool find_grouped_projection_by_unqualified_name(
    const struct planned_grouped_aggregate *plan,
    const char *name,
    size_t *out_projection_index,
    bool *out_ambiguous
);
static const struct table_name_resolution *planned_grouped_aggregate_source_resolution(
    const struct planned_grouped_aggregate *plan,
    size_t source_index
);
static int plan_grouped_aggregate_having(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *having_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int plan_grouped_having_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *having_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int plan_grouped_having_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    enum planned_grouped_having_operand *out_operand
);
static int plan_grouped_having_identifier_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    enum planned_grouped_having_operand *out_operand
);
static int find_grouped_having_projection_alias_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const char *column_name,
    const struct planned_grouped_aggregate *plan,
    bool *out_matched,
    size_t *out_group_index
);
static int find_grouped_having_group_alias_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const struct planned_grouped_aggregate *plan,
    bool *out_matched,
    size_t *out_group_index
);
static int plan_grouped_having_aggregate_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int find_grouped_having_aggregate_alias_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    struct planned_grouped_aggregate *out_plan,
    bool *out_matched
);
static int convert_grouped_having_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct planned_grouped_aggregate *plan,
    enum planned_grouped_having_operand operand,
    struct planned_value *out_value
);
static int parse_grouped_having_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const char *operand_name,
    bool *out_is_negative,
    uint64_t *out_magnitude
);
static int convert_grouped_having_group_value(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_grouped_having_aggregate_value(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const char *operand_name,
    struct planned_value *out_value
);
static int plan_grouped_aggregate_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int resolve_grouped_order_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static bool grouped_order_clause_uses_item_list(const struct mylite_sql_ast_node *order_clause);
static int find_grouped_order_group_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct planned_grouped_aggregate *plan,
    bool *out_matched,
    size_t *out_group_index
);
static int find_grouped_order_projection_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct planned_grouped_aggregate *plan,
    bool *out_matched,
    size_t *out_projection_index
);
static int order_identifier_matches_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct mylite_sql_ast_node *alias,
    bool *out_matches
);
static int order_identifier_matches_unaliased_group_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct planned_grouped_aggregate *plan,
    bool *out_matches,
    size_t *out_group_index
);
static int order_identifier_matches_unaliased_projection_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct planned_grouped_aggregate *plan,
    bool *out_matches,
    size_t *out_projection_index
);
static int find_grouped_order_aggregate_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct planned_grouped_aggregate *plan,
    bool *out_matched,
    size_t *out_aggregate_index
);
static int execute_grouped_aggregate_from_plan(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    mylite_result **out_result
);
static int append_grouped_aggregate_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_grouped_aggregate *plan
);
static bool select_statement_has_count_aggregate(const struct mylite_sql_ast_node *statement);
static int plan_count(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_count *out_plan
);
static void planned_count_deinit(struct planned_count *plan);
static int plan_count_without_source(
    struct mylite_db *database,
    const struct planned_count_source_nodes *nodes,
    struct planned_count *out_plan
);
static int plan_count_table_source(
    struct mylite_db *database,
    const struct planned_count_source_nodes *nodes,
    struct planned_count *out_plan
);
static enum planned_count_function count_function_from_expression(
    const struct mylite_sql_ast_node *expression
);
static const char *count_exactly_one_message(enum planned_count_function function);
static const char *count_supported_clauses_message(enum planned_count_function function);
static const char *count_descriptor_table_message(enum planned_count_function function);
static int plan_count_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    struct planned_value *out_literal
);
static int plan_count_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
);
static int execute_count_from_plan(
    struct mylite_db *database,
    const struct planned_count *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int plan_count_expression_aggregate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_count_expression_aggregate *out_plan
);
static void planned_count_expression_aggregate_deinit(
    struct planned_count_expression_aggregate *plan
);
static int execute_count_expression_aggregate_from_plan(
    struct mylite_db *database,
    const struct planned_count_expression_aggregate *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static bool select_statement_has_column_aggregate(const struct mylite_sql_ast_node *statement);
static int plan_column_aggregate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_column_aggregate *out_plan
);
static void planned_column_aggregate_deinit(struct planned_column_aggregate *plan);
static enum planned_column_aggregate_function column_aggregate_function_from_expression(
    const struct mylite_sql_ast_node *expression
);
static enum planned_column_aggregate_function select_list_column_aggregate_function(
    const struct mylite_sql_ast_node *select_list
);
static bool column_aggregate_function_is_bitwise(enum planned_column_aggregate_function function);
static const char *column_aggregate_single_item_error(
    enum planned_column_aggregate_function function
);
static const char *column_aggregate_optional_clause_error(
    enum planned_column_aggregate_function function
);
static const char *column_aggregate_source_error(enum planned_column_aggregate_function function);
static const char *column_aggregate_column_error(enum planned_column_aggregate_function function);
static const char *column_aggregate_integer_error(enum planned_column_aggregate_function function);
static int plan_column_aggregate_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    enum planned_column_aggregate_function aggregate_function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
);
static int plan_column_group_concat_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_column_aggregate *out_plan
);
static int execute_column_aggregate_from_plan(
    struct mylite_db *database,
    const struct planned_column_aggregate *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int append_count_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_count *plan
);
static int append_column_aggregate_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_column_aggregate *plan
);
static int copy_aggregate_result_column_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_text
);
static size_t aggregate_label_extra_spaces_after_block_comments(
    const struct mylite_sql_source_span *span
);
static void copy_aggregate_label_with_spacing(
    const struct mylite_sql_source_span *span,
    char *destination
);
static bool aggregate_label_needs_space_after_block_comment(char next_byte);
static int read_count_from_source(
    struct mylite_db *database,
    const struct planned_count *plan,
    int64_t *out_count
);
static int read_column_aggregate_from_source(
    struct mylite_db *database,
    const struct planned_column_aggregate *plan,
    mylite_result *result
);
static int read_grouped_aggregate_from_source(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    mylite_result *result
);
static int step_count_statement(sqlite3_stmt *statement, int64_t *out_count);
static int step_column_aggregate_statement(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_column_aggregate *plan,
    mylite_result *result
);
static int append_grouped_aggregate_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    mylite_result *result
);
static int append_grouped_aggregate_projection_cell(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    size_t projection_index,
    struct mylite_result_cell *out_cell,
    char *buffer,
    size_t buffer_size
);
static int append_grouped_aggregate_sqlite_cell(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate_item *item,
    int *sqlite_column_index,
    struct mylite_result_cell *out_cell,
    char *buffer,
    size_t buffer_size
);
static int append_grouped_group_concat_sqlite_cell(
    sqlite3_stmt *statement,
    int sqlite_column_index,
    struct mylite_result_cell *out_cell
);
static int sqlite_integer_result_text(
    sqlite3_stmt *statement,
    int column_index,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
static int append_avg_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    mylite_result *result
);
static int append_bitwise_aggregate_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    mylite_result *result
);
static int format_avg_value(
    struct mylite_db *database,
    struct avg_accumulator accumulator,
    char *buffer,
    size_t buffer_size
);
static uint64_t absolute_int64_magnitude(int64_t value);
static int next_decimal_digit(uint64_t *remainder, uint64_t denominator);
static struct uint128_parts multiply_u64_by_decimal_radix(uint64_t value);
static bool uint128_ge_u64(const struct uint128_parts *left, uint64_t right);
static void uint128_subtract_u64(struct uint128_parts *left, uint64_t right);
static int column_aggregate_step_error(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_column_aggregate *plan,
    int sqlite_rc
);
static int grouped_aggregate_step_error(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    int sqlite_rc
);
static int format_count_value(
    struct mylite_db *database,
    int64_t count_value,
    char *buffer,
    size_t buffer_size
);
static int append_count_value_row(
    struct mylite_db *database,
    mylite_result *result,
    const char *count_text
);
static int count_execution_error(struct mylite_db *database, int rc);
static int column_aggregate_execution_error(struct mylite_db *database, int rc);
static int grouped_aggregate_execution_error(struct mylite_db *database, int rc);
static bool select_statement_is_scalar_projection(const struct mylite_sql_ast_node *statement);
static bool select_statement_is_row_function_scalar_projection(
    const struct mylite_sql_ast_node *statement
);
static bool select_statement_has_no_source_or_dual(const struct mylite_sql_ast_node *statement);
static int reject_bare_native_function_identifier_lookup_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static bool select_statement_is_scalar_projection_attempt(
    const struct mylite_sql_ast_node *statement
);
static int execute_scalar_projection_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int append_scalar_projection_columns_and_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_item,
    mylite_result *result,
    struct session_scalar_cell *cells,
    struct mylite_result_cell *values,
    size_t *out_column_count
);

struct values_statement_row_shape {
    size_t column_count;
    size_t row_number;
};

static int validate_values_statement_rows(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *rows,
    size_t *out_column_count
);
static int validate_values_statement_row(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row,
    struct values_statement_row_shape shape
);
static int validate_values_statement_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value
);
static int validate_values_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value
);
static int validate_values_order_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    size_t column_count
);
static int validate_values_order_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item,
    size_t column_count
);
static int validate_values_order_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    size_t column_count
);
static int validate_values_order_ordinal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    size_t column_count
);
static int validate_values_order_identifier(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    size_t column_count
);
static bool values_column_name_to_index(
    const char *column_name,
    size_t column_count,
    size_t *out_index
);
static int plan_values_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
);
static int append_values_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    size_t column_count
);
static int append_values_result_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *rows,
    const struct planned_select_limit *limit,
    size_t column_count
);
static int append_values_result_row(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *row,
    struct values_statement_row_shape shape
);
static int values_statement_value_cell(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value,
    struct session_scalar_cell *out_cell
);
static int values_statement_integer_cell(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value,
    struct session_scalar_cell *out_cell
);

struct scalar_binary_numeric_result_column_shape {
    enum mylite_result_logical_type logical_type;
    uint64_t display_length;
    uint16_t decimals;
    uint32_t extra_flags;
    bool nullable;
};

struct scalar_connection_string_result_column_shape {
    uint64_t display_length;
    uint32_t extra_flags;
    bool nullable;
};

static int append_scalar_projection_result_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *column_name,
    mylite_result *result
);
static int make_scalar_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
);
static int populate_scalar_literal_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_scalar_function_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static void populate_scalar_temporal_string_result_column_descriptor(
    const struct mylite_db *database,
    struct mylite_result_column_descriptor *descriptor
);
static void populate_scalar_date_result_column_descriptor(
    struct mylite_result_column_descriptor *descriptor
);
static void populate_scalar_datetime_result_column_descriptor(
    struct mylite_result_column_descriptor *descriptor
);
static int populate_scalar_string_literal_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct mylite_result_column_descriptor *descriptor
);
static int scalar_integer_literal_result_column_shape(
    const struct mylite_sql_ast_node *expression,
    struct scalar_binary_numeric_result_column_shape *out_shape
);
static int scalar_unsigned_integer_literal_result_column_shape(
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    struct scalar_binary_numeric_result_column_shape *out_shape
);
static int scalar_newdecimal_integer_literal_result_column_shape(
    const struct mylite_sql_source_span *span,
    struct scalar_binary_numeric_result_column_shape *out_shape
);
static int scalar_decimal_integer_significant_digit_count(
    const struct mylite_sql_source_span *span,
    size_t *out_significant_digit_count
);
static uint64_t scalar_connection_max_bytes_per_character(const struct mylite_db *database);
static void populate_scalar_binary_numeric_result_column_descriptor(
    struct mylite_result_column_descriptor *descriptor,
    struct scalar_binary_numeric_result_column_shape shape
);
static int populate_scalar_charset_collation_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static void populate_scalar_connection_string_result_column_descriptor(
    const struct mylite_db *database,
    struct mylite_result_column_descriptor *descriptor,
    struct scalar_connection_string_result_column_shape shape
);
static void populate_calendar_name_result_column_descriptor(
    const struct mylite_db *database,
    struct mylite_result_column_descriptor *descriptor
);
static void populate_uuid_string_result_column_descriptor(
    struct mylite_result_column_descriptor *descriptor,
    bool nullable
);
static void populate_scalar_json_result_column_descriptor(
    const struct mylite_db *database,
    struct mylite_result_column_descriptor *descriptor
);
static struct mylite_result_cell session_scalar_cell_result_cell(
    const struct session_scalar_cell *cell
);
static bool do_statement_has_only_scalar_projection_expressions(
    const struct mylite_sql_ast_node *statement
);
static int append_session_scalar_do_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int append_session_scalar_select_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list
);
static int append_session_scalar_expression_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static int append_select_modifier_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int append_found_rows_deprecation_warning(struct mylite_db *database);
static int append_sql_calc_found_rows_deprecation_warning(struct mylite_db *database);
static int append_sql_no_cache_deprecation_warning(struct mylite_db *database);
static int append_convert_using_charset_warning(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static int append_utf8_alias_warning(struct mylite_db *database);
static int append_utf8mb3_deprecation_warning(struct mylite_db *database);
static int append_session_scalar_cell_warnings(
    struct mylite_db *database,
    const struct session_scalar_cell *cell
);
static int append_session_scalar_cells_warnings(
    struct mylite_db *database,
    const struct session_scalar_cell *cells,
    size_t cell_count
);
static int accumulate_staged_division_by_zero_warnings(
    struct mylite_db *database,
    size_t cell_warning_count,
    size_t *total_warning_count
);
static int accumulate_staged_warning_count(
    struct mylite_db *database,
    size_t cell_warning_count,
    size_t *total_warning_count
);
static int append_division_by_zero_warnings(struct mylite_db *database, size_t warning_count);
static int append_invalid_logarithm_warnings(struct mylite_db *database, size_t warning_count);
static int append_truncated_incorrect_integer_warning(
    struct mylite_db *database,
    const char *value_text
);
static int append_truncated_incorrect_decimal_warning(
    struct mylite_db *database,
    const char *value_text
);
static int append_signed_complement_warnings(struct mylite_db *database, size_t warning_count);
static int append_unsigned_complement_warnings(struct mylite_db *database, size_t warning_count);
static int append_unhex_incorrect_string_warning(
    struct mylite_db *database,
    const char *value_text
);
static int copy_scalar_projection_column_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_text
);
static const char *select_statement_argument_count_error_function(
    const struct mylite_sql_ast_node *statement
);
static const char *do_statement_argument_count_error_function(
    const struct mylite_sql_ast_node *statement
);
static const char *argument_count_error_function_name(const struct mylite_sql_ast_node *expression);
static const char *argument_count_error_node_function_name(
    const struct mylite_sql_ast_node *expression
);
static int session_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int scalar_concat_operator_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_concat_operator_scalar_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cells,
    char **owned_texts,
    struct mylite_string_concat_argument *arguments,
    size_t argument_count,
    size_t *inout_next_argument
);
static int evaluate_concat_operator_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int concat_operator_scalar_arithmetic_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int scalar_concat_operator_result(
    const struct mylite_string_concat_argument *arguments,
    size_t argument_count,
    char **out_text
);
static bool concat_operator_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);
static bool concat_operator_scalar_argument_is_integer_arithmetic_expression(
    const struct mylite_sql_ast_node *expression
);
static bool concat_operator_scalar_arithmetic_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    bool *inout_saw_arithmetic,
    struct scalar_arithmetic_node_stack *stack
);
static int rand_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int rand_seed_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    uint32_t *out_seed
);
static int rand_scalar_seed_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    uint32_t *out_seed,
    struct session_scalar_cell *out_warnings
);
static int rand_scalar_seed_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool has_unary_sign,
    bool is_negative,
    uint32_t *out_seed,
    struct session_scalar_cell *out_warnings
);
static int rand_seed_numeric_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    uint32_t *out_seed
);
static int rand_seed_string_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    uint32_t *out_seed,
    struct session_scalar_cell *out_warnings
);
static int rand_seed_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    uint32_t *out_seed,
    struct session_scalar_cell *out_warnings
);
static int rand_seed_nullif_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int rand_seed_from_scalar_cell(
    struct mylite_db *database,
    struct session_scalar_cell *cell,
    uint32_t *out_seed,
    struct session_scalar_cell *out_warnings
);
static int rand_seed_from_integer_text(
    struct mylite_db *database,
    const char *text,
    uint32_t *out_seed
);
static int rand_seed_from_magnitude(uint64_t magnitude, bool is_negative, uint32_t *out_seed);
static int rand_seed_convert_string_value(
    struct mylite_db *database,
    const char *text,
    uint32_t *out_seed,
    struct session_scalar_cell *out_warnings
);
static void move_session_scalar_warning_state(
    struct session_scalar_cell *destination,
    struct session_scalar_cell *source
);
static int rand_seed_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    uint32_t *out_seed
);
static int current_timestamp_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
);
static int sysdate_scalar_value(struct mylite_db *database, struct session_scalar_cell *out_cell);
static int current_date_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
);
static int current_time_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
);
static int utc_timestamp_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
);
static int utc_date_scalar_value(struct mylite_db *database, struct session_scalar_cell *out_cell);
static int utc_time_scalar_value(struct mylite_db *database, struct session_scalar_cell *out_cell);
static const char *calendar_date_argument_count_error_function_name(
    enum mylite_sql_ast_node_kind ast_kind
);
static int scalar_subquery_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int scalar_subquery_select_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct session_scalar_cell *out_cell
);
static int scalar_subquery_inner_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int session_scalar_value_without_case(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int session_unary_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int session_binary_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int field_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int elt_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_elt_scalar_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_index,
    bool *out_is_null
);
static int evaluate_elt_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
);
static int evaluate_elt_scalar_string_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct field_scalar_argument *out_argument
);
static int evaluate_elt_scalar_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
);
static int format_elt_scalar_result(
    struct mylite_db *database,
    const struct field_scalar_argument *value,
    struct session_scalar_cell *out_cell
);
static int greatest_least_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int interval_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_interval_scalar_thresholds(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *thresholds,
    const struct field_scalar_argument *search,
    size_t threshold_count,
    int64_t *out_result
);
static int evaluate_interval_scalar_search_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
);
static int evaluate_interval_scalar_threshold_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
);
static int evaluate_interval_scalar_integer_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument,
    const char *unsupported_message
);
static int format_interval_scalar_result(
    struct mylite_db *database,
    int64_t value,
    struct session_scalar_cell *out_cell
);
static const char *greatest_least_function_name(const struct mylite_sql_ast_node *expression);
static int evaluate_greatest_least_scalar_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    struct field_scalar_argument *values,
    size_t argument_count,
    enum planned_row_scalar_field_domain *out_domain
);
static int evaluate_greatest_least_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
);
static int evaluate_greatest_least_scalar_string_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct field_scalar_argument *out_argument
);
static int evaluate_greatest_least_scalar_integer_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
);
static int evaluate_greatest_least_scalar_decimal_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
);
static int merge_greatest_least_domain(
    struct mylite_db *database,
    enum planned_row_scalar_field_domain incoming,
    enum planned_row_scalar_field_domain *inout_domain
);
static size_t greatest_least_scalar_result_index(
    const struct field_scalar_argument *values,
    struct greatest_least_scalar_selection selection
);
static int compare_greatest_least_scalar_arguments(
    const struct field_scalar_argument *left,
    const struct field_scalar_argument *right,
    enum planned_row_scalar_field_domain domain
);
static int field_ascii_text_compare_case_insensitive(
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length
);
static int format_greatest_least_scalar_result(
    struct mylite_db *database,
    const struct field_scalar_argument *value,
    struct session_scalar_cell *out_cell
);
static int evaluate_field_scalar_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    struct field_scalar_argument *values,
    size_t argument_count,
    enum planned_row_scalar_field_domain *out_domain
);
static int evaluate_field_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
);
static int evaluate_field_scalar_string_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct field_scalar_argument *out_argument
);
static int evaluate_field_scalar_integer_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
);
static int merge_field_domain(
    struct mylite_db *database,
    enum planned_row_scalar_field_domain incoming,
    enum planned_row_scalar_field_domain *inout_domain
);
static bool field_ascii_text_is_supported(const char *text, size_t text_length);
static bool field_ascii_text_equals_case_insensitive(
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length
);
static size_t field_scalar_result_position(
    const struct field_scalar_argument_list *arguments,
    enum planned_row_scalar_field_domain domain
);
static bool field_scalar_arguments_match(
    const struct field_scalar_argument *left,
    const struct field_scalar_argument *right,
    enum planned_row_scalar_field_domain domain
);
static int format_field_scalar_result_position(
    struct mylite_db *database,
    size_t result_position,
    struct session_scalar_cell *out_cell
);
static void field_scalar_argument_deinit(struct field_scalar_argument *argument);
static int cast_binary_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int cast_char_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int cast_signed_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int cast_unsigned_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int convert_binary_type_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int convert_char_type_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int convert_signed_type_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int convert_unsigned_type_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int convert_using_binary_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int convert_using_charset_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int collate_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int scalar_convert_charset_info_for_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_convert_charset_info *out_info
);
static int scalar_convert_charset_info_by_name(
    struct mylite_db *database,
    const char *charset_name,
    struct scalar_convert_charset_info *out_info
);
static int scalar_text_conversion_input_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct scalar_text_conversion_messages *messages,
    struct session_scalar_cell *out_cell
);
static int scalar_integer_cast_input_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum scalar_integer_cast_target target,
    const struct scalar_integer_cast_messages *messages,
    struct session_scalar_cell *out_cell
);
static int scalar_integer_cast_string_value(
    struct mylite_db *database,
    const char *text,
    enum scalar_integer_cast_target target,
    struct session_scalar_cell *out_cell
);
static int scalar_integer_cast_decimal_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    enum scalar_integer_cast_target target,
    struct session_scalar_cell *out_cell
);
static void parse_scalar_integer_cast_string(
    const char *text,
    struct scalar_integer_cast_parse *out_parse
);
static void parse_scalar_integer_cast_digits(
    struct scalar_integer_cast_digit_source source,
    struct scalar_integer_cast_parse *inout_parse,
    size_t *out_end_offset
);
static void parse_scalar_integer_cast_decimal_literal(
    const struct mylite_sql_source_span *span,
    uint64_t limit,
    struct scalar_integer_cast_parse *out_parse
);
static int stage_truncated_integer_warning(
    const char *input_text,
    struct session_scalar_cell *cell
);
static int stage_cast_truncated_decimal_warning(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    struct session_scalar_cell *cell
);
static int format_signed_cast_value(
    struct mylite_db *database,
    bool is_negative,
    uint64_t magnitude,
    struct session_scalar_cell *out_cell
);
static int format_negative_signed_cast_value(
    struct mylite_db *database,
    uint64_t magnitude,
    struct session_scalar_cell *out_cell
);
static int format_twos_complement_signed_cast_value(
    struct mylite_db *database,
    uint64_t magnitude,
    struct session_scalar_cell *out_cell
);
static int format_signed_cast_text(
    struct mylite_db *database,
    uint64_t magnitude,
    struct session_scalar_cell *out_cell
);
static int format_unsigned_cast_value(
    struct mylite_db *database,
    bool is_negative,
    uint64_t magnitude,
    struct session_scalar_cell *out_cell
);
static int format_unsigned_magnitude_value(
    struct mylite_db *database,
    uint64_t value,
    struct session_scalar_cell *out_cell
);
static bool scalar_integer_cast_is_ascii_space(unsigned char byte);
static int evaluate_bit_count_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
);
static int evaluate_bit_count_direct_literal_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value,
    bool *out_handled
);
static int evaluate_scalar_bitwise_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
);
static int evaluate_scalar_bitwise_frame(
    struct mylite_db *database,
    struct scalar_bitwise_eval_stack *expression_stack,
    struct scalar_bitwise_value_stack *value_stack,
    const struct scalar_bitwise_eval_frame *frame
);
static int evaluate_scalar_bitwise_apply_frame(
    struct mylite_db *database,
    struct scalar_bitwise_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_bitwise_apply_unary_frame(
    struct mylite_db *database,
    struct scalar_bitwise_value_stack *value_stack
);
static int evaluate_scalar_bitwise_short_circuit_or_enter_right_frame(
    struct mylite_db *database,
    struct scalar_bitwise_eval_stack *expression_stack,
    const struct scalar_bitwise_value_stack *value_stack,
    const struct scalar_bitwise_eval_frame *frame
);
static int evaluate_scalar_bitwise_enter_frame(
    struct mylite_db *database,
    struct scalar_bitwise_eval_stack *expression_stack,
    struct scalar_bitwise_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int evaluate_scalar_bitwise_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
);
static int apply_scalar_bitwise_operator(
    const struct scalar_bitwise_value *left,
    const struct scalar_bitwise_value *right,
    enum mylite_sql_ast_operator operator_kind,
    struct scalar_bitwise_value *out_result
);
static int scalar_bitwise_eval_stack_push(
    struct mylite_db *database,
    struct scalar_bitwise_eval_stack *stack,
    enum scalar_bitwise_eval_frame_kind kind,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
);
static void scalar_bitwise_eval_stack_deinit(struct scalar_bitwise_eval_stack *stack);
static int scalar_bitwise_value_stack_push(
    struct mylite_db *database,
    struct scalar_bitwise_value_stack *stack,
    struct scalar_bitwise_value value
);
static bool scalar_bitwise_value_stack_pop(
    struct scalar_bitwise_value_stack *stack,
    struct scalar_bitwise_value *out_value
);
static void scalar_bitwise_value_stack_deinit(struct scalar_bitwise_value_stack *stack);
static int scalar_logical_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_scalar_logical_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
);
static int evaluate_scalar_logical_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_logical_eval_frame *frame
);
static int evaluate_scalar_logical_apply_not_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack
);
static int evaluate_scalar_logical_apply_comparison_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_logical_apply_is_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_logical_comparison_short_circuit_or_enter_right_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    const struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_logical_eval_frame *frame
);
static int evaluate_scalar_logical_apply_logical_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static void evaluate_scalar_logical_and_result(
    const struct scalar_arithmetic_value *left,
    const struct scalar_arithmetic_value *right,
    struct scalar_arithmetic_value *result
);
static void evaluate_scalar_logical_or_result(
    const struct scalar_arithmetic_value *left,
    const struct scalar_arithmetic_value *right,
    struct scalar_arithmetic_value *result
);
static void evaluate_scalar_logical_xor_result(
    const struct scalar_arithmetic_value *left,
    const struct scalar_arithmetic_value *right,
    struct scalar_arithmetic_value *result
);
static bool scalar_arithmetic_truth_value(const struct scalar_arithmetic_value *value);
static int evaluate_scalar_logical_short_circuit_or_enter_right_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_logical_eval_frame *frame
);
static int evaluate_scalar_logical_enter_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int evaluate_scalar_logical_enter_unary_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int evaluate_scalar_logical_enter_logical_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_logical_enter_comparison_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_logical_enter_is_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_logical_enter_null_safe_comparison_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_logical_enter_arithmetic_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int scalar_logical_eval_stack_push(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *stack,
    enum scalar_logical_eval_frame_kind kind,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
);
static void scalar_logical_eval_stack_deinit(struct scalar_logical_eval_stack *stack);
static int scalar_comparison_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_scalar_comparison_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
);
static int evaluate_scalar_comparison_frame(
    struct mylite_db *database,
    struct scalar_comparison_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_comparison_eval_frame *frame
);
static int evaluate_scalar_comparison_apply_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_comparison_short_circuit_or_enter_right_frame(
    struct mylite_db *database,
    struct scalar_comparison_eval_stack *expression_stack,
    const struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_comparison_eval_frame *frame
);
static int evaluate_scalar_comparison_enter_frame(
    struct mylite_db *database,
    struct scalar_comparison_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static bool scalar_comparison_result_is_true(const struct scalar_comparison_operation *operation);
static int scalar_comparison_eval_stack_push(
    struct mylite_db *database,
    struct scalar_comparison_eval_stack *stack,
    enum scalar_comparison_eval_frame_kind kind,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
);
static void scalar_comparison_eval_stack_deinit(struct scalar_comparison_eval_stack *stack);
static int scalar_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_scalar_arithmetic_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
);
static int evaluate_scalar_arithmetic_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_arithmetic_eval_frame *frame
);
static int evaluate_scalar_arithmetic_apply_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_arithmetic_apply_unary_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_arithmetic_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int evaluate_scalar_arithmetic_operand_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int evaluate_scalar_arithmetic_non_arithmetic_binary_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int finish_scalar_arithmetic_result(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    struct scalar_arithmetic_value *out_value
);
static int evaluate_scalar_arithmetic_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
);
static int scalar_arithmetic_div_left_operand_short_circuits(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_short_circuits
);
static int scalar_arithmetic_div_short_circuit_visit_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack,
    bool *out_short_circuits
);
static int scalar_arithmetic_div_short_circuit_push_child(
    struct mylite_db *database,
    struct scalar_arithmetic_node_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int scalar_arithmetic_div_short_circuit_push_function_arguments(
    struct mylite_db *database,
    struct scalar_arithmetic_node_stack *stack,
    const struct mylite_sql_ast_node *arguments,
    bool *out_short_circuits
);
static int parse_scalar_arithmetic_operand(
    struct mylite_db *database,
    const struct session_scalar_cell *cell,
    struct scalar_arithmetic_value *out_value
);
static int apply_scalar_arithmetic_operator(
    struct mylite_db *database,
    const struct scalar_arithmetic_operation *operation,
    int64_t *out_result
);
static int scalar_arithmetic_eval_stack_push(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *stack,
    enum scalar_arithmetic_eval_frame_kind kind,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
);
static void scalar_arithmetic_eval_stack_deinit(struct scalar_arithmetic_eval_stack *stack);
static int scalar_arithmetic_value_stack_push(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *stack,
    struct scalar_arithmetic_value value
);
static bool scalar_arithmetic_value_stack_pop(
    struct scalar_arithmetic_value_stack *stack,
    struct scalar_arithmetic_value *out_value
);
static void scalar_arithmetic_value_stack_deinit(struct scalar_arithmetic_value_stack *stack);
static bool scalar_arithmetic_node_stack_push(
    struct scalar_arithmetic_node_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static void scalar_arithmetic_node_stack_deinit(struct scalar_arithmetic_node_stack *stack);
static bool checked_int64_add(int64_t left, int64_t right, int64_t *out_result);
static bool checked_int64_subtract(int64_t left, int64_t right, int64_t *out_result);
static bool checked_int64_multiply(int64_t left, int64_t right, int64_t *out_result);
static bool checked_int64_modulo(int64_t left, int64_t right, int64_t *out_result);
static bool checked_int64_divide(int64_t left, int64_t right, int64_t *out_result);
static bool checked_int64_negate(int64_t value, int64_t *out_result);
static void set_scalar_arithmetic_unsupported_error(struct mylite_db *database);
static void set_scalar_division_unsupported_error(struct mylite_db *database);
static void set_scalar_arithmetic_operand_out_of_range_error(struct mylite_db *database);
static void set_scalar_arithmetic_overflow_error(struct mylite_db *database);
static void set_scalar_bitwise_unsupported_error(struct mylite_db *database);
static void set_abs_signed_minimum_overflow_error(struct mylite_db *database);
static void set_abs_unsupported_error(struct mylite_db *database);
static void set_sign_unsupported_error(struct mylite_db *database);
static void set_rounding_unsupported_error(struct mylite_db *database);
static int set_rounding_signed_overflow_error(struct mylite_db *database);
static void set_sqrt_unsupported_error(struct mylite_db *database);
static void set_angle_conversion_unsupported_error(struct mylite_db *database);
static void set_inverse_trig_unsupported_error(struct mylite_db *database);
static void set_direct_trig_unsupported_error(struct mylite_db *database);
static void set_atan_unsupported_error(struct mylite_db *database);
static void set_exp_log_power_unsupported_error(struct mylite_db *database);
static void set_base_conversion_unsupported_error(struct mylite_db *database);
static void set_bit_count_unsupported_error(struct mylite_db *database);
static void set_crc32_unsupported_error(struct mylite_db *database);
static void set_hex_unsupported_error(struct mylite_db *database);
static void set_format_unsupported_error(struct mylite_db *database);
static void set_truncate_unsupported_error(struct mylite_db *database);
static void set_scalar_logical_unsupported_error(struct mylite_db *database);
static void set_scalar_comparison_unsupported_error(struct mylite_db *database);
static int literal_projection_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int if_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int ifnull_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int coalesce_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int nullif_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int isnull_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int case_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int searched_case_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int simple_case_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_case_arithmetic_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
);
static int copy_case_result_cell(
    struct mylite_db *database,
    const struct session_scalar_cell *selected_cell,
    size_t previous_warning_count,
    struct session_scalar_cell *out_cell
);
static bool case_arithmetic_values_are_equal(
    const struct scalar_arithmetic_value *left,
    const struct scalar_arithmetic_value *right
);
static int if_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int if_eval_current_expression(
    struct mylite_db *database,
    struct if_eval_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    const struct mylite_sql_ast_node **next_expression,
    struct session_scalar_cell *out_cell
);
static int if_eval_isnull_expression(
    struct mylite_db *database,
    struct if_eval_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    const struct mylite_sql_ast_node **next_expression
);
static bool if_eval_completed_value(
    struct if_eval_stack *stack,
    struct session_scalar_cell *cell,
    const struct mylite_sql_ast_node **next_expression,
    struct session_scalar_cell *out_cell
);
static void if_eval_complete_if_frame(
    const struct if_eval_frame *frame,
    struct session_scalar_cell *cell,
    const struct mylite_sql_ast_node **next_expression
);
static void if_eval_complete_ifnull_frame(
    const struct if_eval_frame *frame,
    struct session_scalar_cell *cell,
    const struct mylite_sql_ast_node **next_expression
);
static void if_eval_complete_coalesce_frame(
    struct if_eval_frame *frame,
    struct session_scalar_cell *cell,
    const struct mylite_sql_ast_node **next_expression
);
static void if_eval_complete_nullif_frame(
    struct if_eval_frame *frame,
    struct session_scalar_cell *cell,
    const struct mylite_sql_ast_node **next_expression
);
static void if_eval_complete_isnull_frame(
    struct session_scalar_cell *cell,
    const struct mylite_sql_ast_node **next_expression
);
static int if_non_function_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    struct session_scalar_cell *out_cell
);
static int if_integer_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    const char *function_name,
    struct session_scalar_cell *out_cell
);
static int if_eval_stack_push(
    struct mylite_db *database,
    struct if_eval_stack *stack,
    enum if_eval_frame_kind kind,
    const struct mylite_sql_ast_node *first_value,
    const struct mylite_sql_ast_node *second_value
);
static void if_eval_stack_deinit(struct if_eval_stack *stack);
static void copy_session_scalar_cell(
    struct session_scalar_cell *destination,
    const struct session_scalar_cell *source
);
static void session_scalar_cell_deinit(struct session_scalar_cell *cell);
static void session_scalar_cell_array_deinit(struct session_scalar_cell *cells, size_t cell_count);
static bool if_scalar_condition_is_true(const struct session_scalar_cell *cell);
static int normalize_decimal_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    bool is_negative,
    char *buffer,
    size_t buffer_size
);
static int last_insert_id_set_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int last_insert_id_set_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    bool *out_is_null,
    uint64_t *out_value
);
static int signed_last_insert_id_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    uint64_t *out_value
);
static void set_last_insert_id_argument_unsupported_error(struct mylite_db *database);
static int system_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static uint64_t information_schema_stats_expiry_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
);
static int database_character_set_system_variable_value(
    struct mylite_db *database,
    bool global_scope,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
static int database_collation_system_variable_value(
    struct mylite_db *database,
    bool global_scope,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
static int format_session_scalar_uint64_value(
    struct mylite_db *database,
    uint64_t value,
    struct session_scalar_cell *out_cell
);
static uint64_t auto_increment_step_system_variable_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    bool global_scope
);
static uint64_t sql_select_limit_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
);
static uint64_t timeout_system_variable_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    bool global_scope
);
static const char *default_sql_mode_value(void);
static const struct mylite_diagnostics *system_variable_count_diagnostics(
    const struct mylite_db *database
);
static int diagnostics_count_system_variable_value(
    const struct mylite_diagnostics *diagnostics,
    enum mylite_execution_system_variable_kind variable,
    uint64_t *out_count
);
static int resolve_session_system_variable(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_execution_system_variable_kind *out_kind
);
static bool resolve_system_variable_kind(
    const struct system_variable_component *name,
    enum mylite_execution_system_variable_kind *out_kind
);
static int show_system_variable_value(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    bool global_scope,
    char *integer_buffer,
    size_t integer_buffer_size,
    const char **out_value
);
static int format_show_system_variable_uint64_value(
    struct mylite_db *database,
    uint64_t value,
    char *integer_buffer,
    size_t integer_buffer_size,
    const char **out_value
);
static int format_timestamp_system_variable_value(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
);
static bool show_variables_scope_is_global(const struct mylite_sql_ast_node *scope);
static bool show_status_descriptor_visible(
    const struct mylite_execution_show_status_descriptor *descriptor,
    bool global_scope
);
static int append_show_status(
    struct mylite_db *database,
    mylite_result *result,
    const struct show_like_filter *filter,
    bool global_scope,
    const struct mylite_execution_show_status_descriptor *descriptor
);
static int show_catalog_filter_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct show_like_filter *filter,
    const struct show_catalog_where_row *row,
    bool *out_matches
);
static int show_catalog_where_clause_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct show_catalog_where_row *row,
    bool *out_matches
);
static int evaluate_show_catalog_where_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_catalog_where_row *row,
    enum show_variables_where_truth *out_truth
);
static int visit_show_catalog_where_predicate(
    struct mylite_db *database,
    struct show_variables_where_frame_stack *frame_stack,
    const struct mylite_sql_ast_node *predicate
);
static int evaluate_show_catalog_where_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_catalog_where_row *row,
    struct show_variables_where_truth_stack *truth_stack
);
static int evaluate_show_catalog_where_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_catalog_where_row *row,
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_catalog_where_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_catalog_where_row *row,
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_catalog_where_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_catalog_where_row *row,
    enum show_variables_where_truth *out_truth
);
static int show_catalog_where_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct show_catalog_where_row *row,
    const char **out_value
);
static int compare_show_catalog_where_literal(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const char *left,
    const struct mylite_sql_ast_node *right,
    enum show_variables_where_truth *out_truth
);
static int decode_show_catalog_where_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_text
);
static int append_show_variable(
    struct mylite_db *database,
    mylite_result *result,
    const struct show_like_filter *filter,
    const struct mylite_sql_ast_node *where_clause,
    bool global_scope,
    const struct mylite_execution_system_variable_descriptor *descriptor
);
static bool show_variable_descriptor_visible(
    const struct mylite_execution_system_variable_descriptor *descriptor,
    bool global_scope
);
static int show_variables_where_clause_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct show_variables_where_row *row,
    bool *out_matches
);
static int evaluate_show_variables_where_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    enum show_variables_where_truth *out_truth
);
static int visit_show_variables_where_predicate(
    struct mylite_db *database,
    struct show_variables_where_frame_stack *frame_stack,
    const struct mylite_sql_ast_node *predicate
);
static int evaluate_show_variables_where_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    struct show_variables_where_truth_stack *truth_stack
);
static int evaluate_show_variables_where_logical_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    struct show_variables_where_truth_stack *truth_stack,
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_variables_where_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_variables_where_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_variables_where_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    enum show_variables_where_truth *out_truth
);
static enum show_variables_where_truth negate_show_variables_where_truth(
    enum show_variables_where_truth truth
);
static enum show_variables_where_truth and_show_variables_where_truth(
    enum show_variables_where_truth left,
    enum show_variables_where_truth right
);
static enum show_variables_where_truth or_show_variables_where_truth(
    enum show_variables_where_truth left,
    enum show_variables_where_truth right
);
static int show_variables_where_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct show_variables_where_row *row,
    const char **out_value
);
static int resolve_show_variables_where_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    enum show_variables_where_column *out_column
);
static int show_variables_column_reference_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char **out_text
);
static int compare_show_variables_where_literal(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const char *left,
    const struct mylite_sql_ast_node *right,
    enum show_variables_where_truth *out_truth
);
static int decode_show_variables_where_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_text
);
static void show_variables_where_frame_stack_deinit(struct show_variables_where_frame_stack *stack);
static int show_variables_where_frame_stack_push(
    struct mylite_db *database,
    struct show_variables_where_frame_stack *stack,
    struct show_variables_where_eval_frame frame
);
static bool show_variables_where_frame_stack_pop(
    struct show_variables_where_frame_stack *stack,
    struct show_variables_where_eval_frame *out_frame
);
static void show_variables_where_truth_stack_deinit(struct show_variables_where_truth_stack *stack);
static int show_variables_where_truth_stack_push(
    struct mylite_db *database,
    struct show_variables_where_truth_stack *stack,
    enum show_variables_where_truth truth
);
static bool show_variables_where_truth_stack_pop(
    struct show_variables_where_truth_stack *stack,
    enum show_variables_where_truth *out_truth
);
static int validate_if_value_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
);
static int validate_if_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
);
static int validate_if_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
);
static int validate_ifnull_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
);
static int validate_coalesce_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
);
static int validate_nullif_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
);
static int validate_isnull_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
);
static int validate_case_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static int validate_case_value_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static int validate_case_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int validate_case_argument_count_error_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_handled
);
static int validate_case_mod_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int validate_case_unary_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int validate_case_binary_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int validate_case_literal_value_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static bool case_value_expression_is_admitted(const struct mylite_sql_ast_node *expression);
static bool is_case_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_case_when_list_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_case_when_clause_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_case_else_clause_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_case_expression_kind(enum mylite_sql_ast_node_kind kind);
static void set_case_unsupported_error(struct mylite_db *database);
static void set_if_unsupported_error(struct mylite_db *database, const char *function_name);
static const char *if_function_name(const struct mylite_sql_ast_node *expression);
static bool is_if_non_function_value_expression(const struct mylite_sql_ast_node *expression);
static bool is_if_integer_literal_in_range(const struct mylite_sql_ast_node *literal);
static int if_validation_stack_push(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static void if_validation_stack_deinit(struct if_validation_stack *stack);
static bool is_scalar_projection_select_item_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_scalar_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_collate_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_projection_expression_without_collate(
    const struct mylite_sql_ast_node *expression
);
static bool is_last_insert_id_set_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_scalar_function_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_numeric_scalar_function_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_string_scalar_function_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_temporal_scalar_function_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_json_scalar_function_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_scalar_subquery_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_abs_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_sign_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_rounding_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_sqrt_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_angle_conversion_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_inverse_trig_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_direct_trig_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_atan_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_exp_log_power_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_base_conversion_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_bit_count_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_crc32_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_hex_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_base64_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_unhex_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_weight_string_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_uuid_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_uuid_value_projection_argument_supported(
    const struct mylite_sql_ast_node *expression
);
static bool is_char_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_binary_string_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_char_projection_argument_supported(const struct mylite_sql_ast_node *expression);
static bool is_char_projection_literal_supported(const struct mylite_sql_ast_node *literal);
static bool is_char_projection_unary_supported(const struct mylite_sql_ast_node *expression);
static bool is_format_truncate_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_length_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_case_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_trim_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_slice_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_search_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_strcmp_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_bitmask_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_concat_ws_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_replace_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_insert_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_reverse_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_quote_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_charset_collation_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_string_metadata_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_conversion_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_cast_binary_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_cast_convert_basic_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_convert_binary_type_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_convert_using_binary_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_convert_using_charset_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_date_interval_second_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_time_arithmetic_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_date_format_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_temporal_extract_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_elt_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_field_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_greatest_least_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_interval_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_regexp_like_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_regexp_string_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_json_valid_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_json_contains_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_json_extract_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_json_value_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_json_introspection_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_json_unquote_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_json_quote_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_json_construction_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_json_set_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_value_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_arithmetic_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_scalar_bitwise_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_logical_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_comparison_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_scalar_concat_projection_expression(const struct mylite_sql_ast_node *expression);
static bool scalar_arithmetic_projection_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack
);
static bool scalar_bitwise_projection_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack
);
static bool scalar_logical_projection_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack
);
static bool scalar_comparison_projection_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack
);
static bool scalar_concat_projection_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack
);
static bool is_scalar_arithmetic_operator(enum mylite_sql_ast_operator operator_kind);
static bool is_scalar_bitwise_operator(enum mylite_sql_ast_operator operator_kind);
static bool is_scalar_bitwise_unary_operator(enum mylite_sql_ast_operator operator_kind);
static bool is_scalar_logical_operator(enum mylite_sql_ast_operator operator_kind);
static bool is_scalar_logical_unary_operator(enum mylite_sql_ast_operator operator_kind);
static bool is_scalar_comparison_operator(enum mylite_sql_ast_operator operator_kind);
static bool is_scalar_is_operator(enum mylite_sql_ast_operator operator_kind);
static bool expression_is_unparenthesized_scalar_is(const struct mylite_sql_ast_node *expression);
static bool is_scalar_projection_literal_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_function_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_projection_attempt_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_value_projection_attempt_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_scalar_arithmetic_attempt_expression(const struct mylite_sql_ast_node *expression);
static bool scalar_arithmetic_attempt_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack
);
static bool is_scalar_value_projection_attempt_operand(
    const struct mylite_sql_ast_node *expression
);
static bool is_scalar_numeric_function_attempt_operand(
    const struct mylite_sql_ast_node *expression
);
static int append_system_variable_read_warning(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
);
static int parse_system_variable_component(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    size_t *offset,
    struct system_variable_component *out_component
);
static int append_quoted_system_variable_byte(
    struct mylite_db *database,
    struct system_variable_component *component,
    size_t *component_length,
    char value
);
static bool system_variable_component_equals(
    const struct system_variable_component *component,
    const char *expected
);
static bool system_variable_component_is_empty(const struct system_variable_component *component);
static const struct mylite_sql_ast_node *unwrap_parenthesized_expression(
    const struct mylite_sql_ast_node *expression
);
static const struct mylite_sql_ast_node *unwrap_any_value_function_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_session_scalar_expression(const struct mylite_sql_ast_node *expression);
static int copy_source_span_text(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_text
);
static int append_show_processlist_row(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result *result
);
static int format_show_processlist_user_host(
    struct mylite_db *database,
    char *user,
    size_t user_size,
    char *host,
    size_t host_size
);
static int copy_show_processlist_info(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    char **out_info
);
static size_t statement_info_length_without_terminator(const char *sql, size_t sql_size);
static int append_show_processlist_warning(struct mylite_db *database);
static int append_information_schema_profiling_warning(struct mylite_db *database);
static int plan_diagnostics_show_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_diagnostics_show_limit *out_limit
);
static int convert_diagnostics_show_limit_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
);
static int append_show_diagnostics_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_diagnostics_show_limit *limit
);
static int append_show_diagnostics_row(
    struct mylite_db *database,
    mylite_result *result,
    const char *level,
    const struct mylite_diagnostic_record *record
);
static int append_show_count_warnings_row(struct mylite_db *database, mylite_result *result);
static int append_show_errors_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_diagnostics_show_limit *limit
);
static int append_show_count_errors_row(struct mylite_db *database, mylite_result *result);
static int previous_diagnostics_condition_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
);
static int previous_diagnostics_error_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
);
static bool diagnostics_has_error_condition(const struct mylite_diagnostics *diagnostics);
static bool diagnostic_record_level_is_error(const struct mylite_diagnostic_record *record);
static int format_uint64(
    struct mylite_db *database,
    uint64_t value,
    char *buffer,
    size_t buffer_size
);

static int plan_show_create_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_show_create_table *out_plan
);
static void planned_show_create_table_deinit(struct planned_show_create_table *plan);
static int execute_show_create_table_from_plan(
    struct mylite_db *database,
    const struct planned_show_create_table *plan,
    mylite_result *result
);
static int append_show_create_view_result(
    struct mylite_db *database,
    mylite_result *result,
    const char *view_name,
    const struct mylite_catalog_view_descriptor *view
);
static int execute_show_create_builtin_sys_view_statement(
    struct mylite_db *database,
    const struct mylite_execution_catalog_builtin_sys_view *view,
    bool schema_qualified,
    mylite_result **out_result
);
static int append_show_create_builtin_sys_view_result(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_execution_catalog_builtin_sys_view *view,
    bool schema_qualified
);
static int append_show_create_view_text_result(
    struct mylite_db *database,
    mylite_result *result,
    const char *view_name,
    const char *show_create_sql,
    const char *character_set_client,
    const char *collation_connection
);
static const struct mylite_execution_catalog_builtin_sys_view *find_builtin_sys_view_definition(
    const char *view_name
);
static const struct mylite_execution_catalog_builtin_sys_view *show_create_target_builtin_sys_view(
    const struct table_name_resolution *target
);
static bool show_create_target_was_schema_qualified(const struct mylite_sql_ast_node *statement);
static int show_create_statement_targets_selected_builtin_sys_view(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct mylite_execution_catalog_builtin_sys_view **out_view
);
static void populate_builtin_sys_view_target(
    struct table_name_resolution *target,
    const struct mylite_execution_catalog_builtin_sys_view *view
);
static int build_show_create_table_sql(
    struct mylite_db *database,
    const struct planned_show_create_table *plan,
    char **out_sql
);
static int append_show_create_table_column_definition(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    bool is_last_column
);
static int append_show_create_table_column_charset_collation(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    bool is_national
);
static int append_show_create_table_column_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
);
static int append_show_create_table_non_integer_column_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
);
static int append_show_create_table_binary_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
);
static int append_show_create_table_binary_blob_expression_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
);
static bool binary_default_bytes_use_hex_literal(const char *bytes, size_t byte_count);
static int append_binary_default_hex_literal(
    struct mylite_dynamic_string *string,
    const char *bytes,
    size_t byte_count
);
static int append_mysql_quoted_binary_default_text(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_length
);
static int append_show_create_table_expression_default(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
);
static int append_show_create_table_text_expression_default(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
);
static int append_show_create_table_column_suffix(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    bool is_last_column
);
static int append_show_create_table_indexes(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
);
static int append_show_create_table_foreign_keys(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
);
static int append_show_create_table_check_constraints(
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
);
static int append_show_create_table_table_options(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
);
static bool show_create_table_should_append_default_collation(
    const struct planned_show_create_table *plan
);
static int append_show_create_table_auto_increment_option(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
);
static int append_show_create_table_comment_option(
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
);
static int append_show_create_table_storage_statistics_options(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
);
static int append_show_create_table_integer_option(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const char *prefix,
    int64_t value
);
static bool show_create_table_has_auto_increment(const struct planned_show_create_table *plan);
static int append_show_create_table_index(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan,
    const struct loaded_index_info *index,
    bool is_last_index
);
static int append_show_create_table_index_header(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan,
    const struct loaded_index_info *index
);
static const char *show_create_table_index_display_name(
    const struct planned_show_create_table *plan,
    const struct loaded_index_info *index
);
static bool show_create_foreign_key_name_is_generated(
    const struct planned_show_create_table *plan,
    const struct loaded_foreign_key_info *foreign_key
);
static int append_show_create_table_index_options(
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *index
);
static int append_show_create_table_index_part(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *part,
    bool needs_comma
);
static int append_show_create_table_foreign_key(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key,
    bool is_last_foreign_key
);
static int append_show_create_table_foreign_key_parts(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key,
    bool child_columns
);
static int append_show_create_table_foreign_key_rule(
    struct mylite_dynamic_string *string,
    const char *timing,
    const char *rule
);
static int index_display_group(const struct loaded_index_info *index);
static int show_create_table_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char **out_type_text
);
static int show_descriptor_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
);
static int show_descriptor_numeric_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char **out_type_text,
    bool *out_matched
);
static bool show_descriptor_temporal_type_text(
    const char *logical_type,
    const char **out_type_text
);
static int format_decimal_descriptor_type_text(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size,
    const struct decimal_type_info *info,
    const char **out_type_text
);
static int format_approximate_descriptor_type_text(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size,
    const struct approximate_type_info *info,
    const char **out_type_text
);
static int format_varchar_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
);
static int format_char_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
);
static int format_enum_descriptor_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
);
static int format_set_descriptor_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
);
static int format_text_family_type_text(const char *logical_type, const char **out_type_text);
static int format_binary_string_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
);
static const char *integer_descriptor_display_text(const char *logical_type);
static int build_show_create_database_sql(
    const struct mylite_catalog_schema_descriptor *schema,
    char **out_sql
);

static int maybe_finish_create_if_not_exists_noop(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct planned_create_table *plan,
    bool *out_finished
);
static int maybe_finish_create_temporary_if_not_exists_noop(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct planned_create_table *plan,
    bool *out_finished
);
static bool create_table_has_if_not_exists(const struct mylite_sql_ast_node *statement);
static const struct mylite_sql_ast_node *create_table_options_node(
    const struct mylite_sql_ast_node *statement
);
static bool drop_table_has_if_exists(const struct mylite_sql_ast_node *statement);

static int plan_delete(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_delete *out_plan
);
static int plan_single_table_delete(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_delete *out_plan
);
static int plan_joined_delete(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_delete *out_plan
);
static int resolve_joined_delete_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target_node,
    const struct planned_select_source *sources,
    size_t source_count,
    size_t *out_source_index
);
static int resolve_joined_delete_targets(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target_node,
    const struct planned_select_source *sources,
    size_t source_count,
    size_t *out_source_indexes,
    size_t source_index_capacity,
    size_t *out_source_index_count
);
static int append_joined_delete_target_source_index(
    struct mylite_db *database,
    size_t source_index,
    size_t *out_source_indexes,
    size_t source_index_capacity,
    size_t *out_source_index_count
);
static int reject_joined_delete_mixed_physical_targets(
    struct mylite_db *database,
    const struct planned_select_source *sources,
    const size_t *target_source_indexes,
    size_t target_source_count
);
static int reject_reserved_joined_delete_target(
    struct mylite_db *database,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count
);
static bool joined_delete_target_matches_source(
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    const struct mylite_catalog_schema_descriptor *selected_schema,
    const struct planned_select_source *source
);
static void planned_delete_deinit(struct planned_delete *plan);
static int apply_parent_delete_actions(
    struct mylite_db *database,
    const struct planned_delete *plan
);
static int execute_parent_delete_cascade(
    struct mylite_db *database,
    const struct planned_delete *plan,
    const struct loaded_foreign_key_info *foreign_key
);
static int build_parent_delete_cascade_sql(
    const struct planned_delete *plan,
    const struct loaded_foreign_key_info *foreign_key,
    char **out_sql
);
static int execute_parent_delete_set_null(
    struct mylite_db *database,
    const struct planned_delete *plan,
    const struct loaded_foreign_key_info *foreign_key
);
static int build_parent_delete_set_null_sql(
    const struct planned_delete *plan,
    const struct loaded_foreign_key_info *foreign_key,
    char **out_sql
);
static int append_parent_delete_target_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter,
    bool *out_has_condition
);
static int append_joined_parent_delete_target_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter
);
static int append_parent_child_match_sql(
    struct mylite_dynamic_string *string,
    bool has_parent_condition,
    const struct loaded_foreign_key_info *foreign_key
);
static int execute_delete_from_plan(
    struct mylite_db *database,
    const struct planned_delete *plan,
    mylite_result *result
);

static int plan_update(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_update *out_plan
);
static int plan_single_table_update(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_update *out_plan
);
static int plan_single_table_update_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
);
static int validate_update_subquery_predicate_sources(
    struct mylite_db *database,
    const struct planned_update *plan
);
static int validate_update_subquery_predicate_node_source(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct planned_select_predicate_node *node
);
static int collect_single_table_update_optional_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *optional_clause,
    struct planned_update *plan,
    struct update_optional_clauses *clauses
);
static const struct mylite_sql_ast_node *single_table_update_target_name_node(
    const struct mylite_sql_ast_node *target
);
static int validate_update_index_hints(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target,
    const struct mylite_catalog_table_descriptor *table
);
static int plan_joined_update(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_update *out_plan
);
static void planned_update_deinit(struct planned_update *plan);
static bool planned_update_has_multiple_assignments(const struct planned_update *plan);
static bool planned_update_assignment_is_noop(const struct planned_update_assignment *assignment);
static size_t planned_update_executable_assignment_count(const struct planned_update *plan);
static int copy_update_assignments_for_execution(
    struct mylite_db *database,
    const struct planned_update *plan,
    struct planned_update *executable_plan
);
static void executable_update_deinit(struct planned_update *plan);
static int apply_parent_update_actions(
    struct mylite_db *database,
    const struct planned_update *plan
);
static int validate_parent_update_action_child_foreign_keys(
    struct mylite_db *database,
    const struct planned_update *plan
);
static int execute_parent_update_cascade(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct loaded_foreign_key_info *foreign_key,
    size_t updated_part_index
);
static int build_parent_update_cascade_sql(
    const struct planned_update *plan,
    const struct loaded_foreign_key_info *foreign_key,
    size_t updated_part_index,
    char **out_sql
);
static int execute_parent_update_set_null(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct loaded_foreign_key_info *foreign_key
);
static int build_parent_update_set_null_sql(
    const struct planned_update *plan,
    const struct loaded_foreign_key_info *foreign_key,
    char **out_sql
);
static int append_parent_update_cascade_assignment_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_foreign_key_info *foreign_key,
    size_t updated_part_index
);
static int append_parent_update_target_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter,
    bool *out_has_condition
);
static int append_joined_parent_update_target_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
static int bind_parent_update_cascade_parameters(
    sqlite3_stmt *statement,
    const struct planned_update *plan
);
static int bind_parent_update_set_null_parameters(
    sqlite3_stmt *statement,
    const struct planned_update *plan
);
static int handle_foreign_key_action_set_null_constraint(
    struct mylite_db *database,
    int64_t child_table_id
);
static int handle_parent_update_cascade_constraint(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct loaded_foreign_key_info *foreign_key,
    size_t updated_part_index
);
static int handle_parent_update_cascade_unique_conflict(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct loaded_foreign_key_info *foreign_key,
    size_t updated_part_index
);
static int format_parent_update_cascade_record_value(
    const struct planned_update *plan,
    char *value,
    size_t value_size
);
static bool planned_update_assigns_foreign_key_parent_part(
    const struct planned_update *plan,
    const struct loaded_foreign_key_info *foreign_key,
    size_t *out_part_index
);
static int reject_unsupported_recursive_foreign_key_action(
    struct mylite_db *database,
    const struct loaded_foreign_key_info *foreign_key
);
static int execute_update_from_plan(
    struct mylite_db *database,
    const struct planned_update *plan,
    mylite_result *result
);
static int read_limited_update_matched_row_state(
    struct mylite_db *database,
    const struct planned_update *plan,
    uint64_t *out_row_count,
    bool *out_matches_any_row
);
static int validate_update_date_interval_matched_assignment(
    struct mylite_db *database,
    const struct planned_update *plan,
    bool matches_any_row
);
static int read_update_matched_row_count(
    struct mylite_db *database,
    const struct planned_update *plan,
    uint64_t *out_row_count
);
static int read_single_table_update_matched_row_count(
    struct mylite_db *database,
    const struct planned_update *plan,
    uint64_t *out_row_count
);
static int build_single_table_update_matched_count_sql(
    const struct planned_update *plan,
    char **out_sql
);
static int append_remaining_nonstrict_update_adjustment_warnings(
    struct mylite_db *database,
    const struct planned_update *plan,
    uint64_t matched_row_count
);
static int append_update_assignment_adjustment_warning(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    bool ignore_errors,
    size_t row_number
);
static int append_update_scalar_subquery_adjustment_warning(
    struct mylite_db *database,
    const struct planned_update *plan
);
static int append_update_default_function_adjustment_warning(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    bool ignore_errors
);
static int append_update_default_assignment_adjustment_warning(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int append_update_constant_arithmetic_adjustment_warning(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column
);
static int append_update_string_truncation_diagnostic(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    bool ignore_errors,
    size_t row_number
);
static int append_update_integer_string_adjustment_diagnostic(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    bool ignore_errors,
    size_t row_number
);
static int append_update_decimal_string_adjustment_diagnostic(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    bool ignore_errors,
    size_t row_number
);
static int append_update_approximate_string_adjustment_diagnostic(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    bool ignore_errors,
    size_t row_number
);
static int prepare_executable_update_plan(
    struct mylite_db *database,
    const struct planned_update *plan,
    bool matches_any_row,
    struct planned_update *executable_plan
);
static int prepare_update_multiple_assignments(
    struct mylite_db *database,
    const struct planned_update *plan,
    struct planned_update *executable_plan
);
static int prepare_update_arithmetic_assignment(
    struct mylite_db *database,
    const struct planned_update *plan,
    struct planned_update *executable_plan
);
static int parse_update_arithmetic_delta(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_delta
);
static int validate_update_arithmetic_range(
    struct mylite_db *database,
    const struct planned_update *plan,
    uint64_t delta
);
static int validate_update_arithmetic_threshold_range(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct update_arithmetic_range_check *check
);
static int update_arithmetic_condition_matches_row(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct update_arithmetic_range_check *check,
    bool *out_matches
);
static int build_update_arithmetic_condition_sql(
    const struct planned_update *plan,
    enum update_arithmetic_range_condition condition,
    char **out_sql
);
static int bind_update_arithmetic_condition_parameters(
    sqlite3_stmt *statement,
    const struct planned_update *plan,
    const struct update_arithmetic_range_check *check
);
static uint64_t update_arithmetic_domain_width(const struct integer_column_range *range);
static int64_t update_arithmetic_addition_threshold(
    const struct integer_column_range *range,
    uint64_t delta
);
static int64_t update_arithmetic_subtraction_threshold(
    const struct integer_column_range *range,
    uint64_t delta
);
static int64_t int64_from_negative_abs(uint64_t negative_abs);
static bool update_arithmetic_column_is_unsigned(
    const struct mylite_catalog_column_descriptor *column
);
static bool update_arithmetic_column_is_signed_bigint(
    const struct mylite_catalog_column_descriptor *column
);
static enum update_arithmetic_range_error_kind update_arithmetic_range_error_kind_for_plan(
    const struct planned_update *plan
);
static void set_update_arithmetic_unsupported_error(struct mylite_db *database);
static void set_update_arithmetic_delta_out_of_range_error(struct mylite_db *database);
static void set_update_arithmetic_range_error(
    struct mylite_db *database,
    const struct planned_update *plan,
    enum update_arithmetic_range_error_kind error_kind
);
static int execute_matching_update_statement(
    struct mylite_db *database,
    const struct planned_update *executable_plan,
    int64_t *out_affected_rows,
    const struct planned_update *plan
);
static int report_update_execution_error(struct mylite_db *database, int rc);
static int step_update_statement(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_update *executable_plan,
    int64_t *out_affected_rows,
    const struct planned_update *plan
);
static int advance_auto_increment_after_update(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct planned_value *assignment_value,
    int64_t affected_rows
);
static int update_table_auto_increment_next(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t auto_increment_next
);
static int touch_persistent_table_updated_time(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t affected_rows,
    bool force
);
static int update_matches_any_row(
    struct mylite_db *database,
    const struct planned_update *plan,
    bool *out_matches
);
static int init_select_source_context(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    const struct table_name_resolution *source,
    struct select_source_context *out_context
);

static int resolve_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution
);
static int resolve_writable_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution
);
static int resolve_visible_table_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution,
    struct mylite_catalog_table_descriptor *out_table
);
static int resolve_visible_writable_table_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution,
    struct mylite_catalog_table_descriptor *out_table
);
static int resolve_table_name_allow_missing_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution,
    bool *out_missing_schema
);
static int resolve_writable_table_name_allow_missing_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution,
    bool *out_missing_schema
);
static int resolve_readable_table(
    struct mylite_db *database,
    const struct table_name_resolution *resolution,
    bool missing_schema,
    struct mylite_catalog_table_descriptor *out_table
);
static int resolve_metadata_table_reference(
    struct mylite_db *database,
    const struct table_name_resolution *resolution,
    bool missing_schema,
    struct mylite_catalog_table_descriptor *out_table
);
static int resolve_persistent_metadata_table_reference(
    struct mylite_db *database,
    const struct table_name_resolution *resolution,
    bool missing_schema,
    struct mylite_catalog_table_descriptor *out_table
);
static int resolve_show_columns_table_name(
    struct mylite_db *database,
    struct show_columns_target_nodes nodes,
    struct table_name_resolution *out_resolution,
    bool *out_missing_schema
);
static int copy_show_columns_explicit_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    char *destination,
    size_t destination_size
);
static int resolve_truncate_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution
);
static int resolve_writable_truncate_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution
);
static int require_selected_schema_for_unqualified_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node
);
static int resolve_schema_name(
    struct mylite_db *database,
    const char *schema_name,
    struct mylite_catalog_schema_descriptor *out_schema
);
static int resolve_selected_schema(
    struct mylite_db *database,
    struct mylite_catalog_schema_descriptor *out_schema
);
static int collect_identifier_parts(
    const struct mylite_sql_ast_node *node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_capacity,
    size_t *part_count,
    struct mylite_db *database
);
static int reject_builtin_schema_write_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node
);
static int reject_builtin_schema_schema_write_name(
    struct mylite_db *database,
    const char *schema_name
);
static const struct mylite_execution_catalog_builtin_schema *find_builtin_schema_descriptor(
    const char *schema_name
);
static bool selected_schema_is_builtin_schema(const struct mylite_db *database);
static const char *builtin_schema_error_name(const char *schema_name);
static void set_builtin_schema_write_error(struct mylite_db *database, const char *schema_name);
static bool selected_schema_is_information_schema(const struct mylite_db *database);
static bool schema_name_is_information_schema(const char *schema_name);
static int copy_identifier_text(
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size,
    struct mylite_db *database
);
static int copy_quoted_identifier_text(
    const char *source,
    size_t source_size,
    char *destination,
    size_t destination_size
);
static int copy_unquoted_identifier_text(
    const char *source,
    size_t source_size,
    char *destination,
    size_t destination_size
);

static int plan_create_table_items(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list,
    struct planned_create_table *out_plan
);
static int plan_create_table_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item,
    struct planned_create_table *out_plan,
    size_t *column_index,
    const struct mylite_sql_ast_node **primary_key
);
static int apply_create_table_default_binary_charset_to_column(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *column
);
static int normalize_text_length_argument_for_default_charset(
    struct mylite_db *database,
    struct planned_column *column,
    const char *default_charset
);
static int apply_default_binary_charset_to_column(
    struct mylite_db *database,
    const char *default_charset,
    struct planned_column *column
);
static bool create_table_column_has_explicit_charset_or_collation(
    const struct mylite_sql_ast_node *column_node
);
static bool planned_create_table_default_charset_is_binary(const struct planned_create_table *plan);
static int count_create_table_column_items(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list,
    size_t *out_column_count
);
static int validate_create_table_item_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list
);
static int apply_create_table_inline_primary_key(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t column_index,
    const struct mylite_sql_ast_node *column_node
);
static int apply_create_table_primary_key_definition(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct mylite_sql_ast_node *primary_key
);
static int extract_primary_key_definition_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *primary_key,
    struct primary_key_definition_nodes *out_nodes
);
static int apply_create_table_secondary_index_definitions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list,
    struct planned_create_table *plan
);
static int apply_create_table_secondary_index_definition(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct mylite_sql_ast_node *secondary_index,
    enum mylite_catalog_index_kind index_kind,
    bool is_unique
);
static int apply_create_table_foreign_key_definitions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list,
    struct planned_create_table *plan
);
static int apply_create_table_foreign_key_definition(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct mylite_sql_ast_node *foreign_key
);
static int apply_create_table_check_constraint_definitions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list,
    struct planned_create_table *plan
);
static int apply_create_table_column_check_constraint_definitions(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct mylite_sql_ast_node *column_definition,
    size_t column_index
);
static int apply_create_table_check_constraint_definition(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct create_table_check_constraint_definition *definition
);
static int64_t next_planned_check_constraint_generated_ordinal(
    const struct planned_create_table *plan
);
static int plan_create_table_check_constraint_name(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct mylite_sql_ast_node *name_node,
    struct planned_check_constraint *check_constraint
);
static int generate_create_table_check_constraint_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    int64_t generated_ordinal,
    char *destination,
    size_t destination_size
);
static bool planned_check_constraint_name_is_used(
    const struct planned_create_table *plan,
    const char *check_constraint_name
);
static int reject_duplicate_schema_check_constraint_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const char *check_constraint_name
);
static int reject_duplicate_schema_check_constraint_name_callback(
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    void *user_data
);
static int render_check_constraint_expression(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_column *inline_column,
    size_t inline_column_index,
    const struct mylite_sql_ast_node *expression,
    char *check_clause,
    size_t check_clause_size,
    char *sqlite_expression,
    size_t sqlite_expression_size
);
static int finalize_planned_generated_columns(
    struct mylite_db *database,
    struct planned_create_table *plan
);
static int finalize_planned_generated_column(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t column_index
);
static int render_generated_column_expression(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    size_t generated_column_index,
    const struct mylite_sql_ast_node *expression,
    char *generation_expression,
    size_t generation_expression_size,
    char *sqlite_expression,
    size_t sqlite_expression_size
);
static int render_generated_expression_node(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node
);
static int render_generated_expression_work_item(
    struct generated_expression_render_context *context,
    struct generated_expression_render_work_item item,
    struct generated_expression_render_work_item **items,
    size_t *item_count
);
static int render_generated_expression_column(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node
);
static int render_generated_expression_literal(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node
);
static int render_generated_expression_unary(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    struct generated_expression_render_work_item **items,
    size_t *item_count
);
static int render_generated_expression_binary(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    struct generated_expression_render_work_item **items,
    size_t *item_count
);
static const char *generated_expression_binary_operator_text(
    enum mylite_sql_ast_operator operator_kind
);
static int append_generated_expression_render_node(
    struct mylite_db *database,
    struct generated_expression_render_work_item **items,
    size_t *item_count,
    const struct mylite_sql_ast_node *node
);
static int append_generated_expression_render_text(
    struct mylite_db *database,
    struct generated_expression_render_work_item **items,
    size_t *item_count,
    const char *text
);
static int append_generated_expression_render_char(
    struct mylite_db *database,
    struct generated_expression_render_work_item **items,
    size_t *item_count,
    char character
);
static int append_generated_expression_render_work_item(
    struct mylite_db *database,
    struct generated_expression_render_work_item **items,
    size_t *item_count,
    struct generated_expression_render_work_item item
);
static int render_check_expression_node(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    bool require_boolean,
    bool *out_is_boolean
);
static int render_check_expression_work_item(
    struct check_expression_render_context *context,
    struct check_expression_render_work_item item,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    bool *out_is_boolean
);
static int render_check_expression_column(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node
);
static int render_check_expression_literal(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    bool require_boolean,
    bool *out_is_boolean
);
static int render_check_expression_unary(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    bool *out_is_boolean
);
static int render_check_expression_binary(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    bool *out_is_boolean
);
static bool check_expression_binary_operator_is_boolean(enum mylite_sql_ast_operator operator_kind);
static bool check_expression_binary_operator_is_arithmetic(
    enum mylite_sql_ast_operator operator_kind
);
static bool check_expression_binary_operator_requires_boolean_children(
    enum mylite_sql_ast_operator operator_kind
);
static const char *check_expression_check_clause_operator_sql(
    enum mylite_sql_ast_operator operator_kind
);
static const char *check_expression_sqlite_operator_sql(enum mylite_sql_ast_operator operator_kind);
static int append_check_expression_span_text(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_sql_ast_node *node
);
static int append_check_expression_render_node(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    const struct mylite_sql_ast_node *node,
    bool require_boolean
);
static int append_check_expression_render_text(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    const char *text
);
static int append_check_expression_render_char(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    char character
);
static int append_check_expression_render_operator(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    enum mylite_sql_ast_operator operator_kind
);
static int append_check_expression_render_work_item(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    struct check_expression_render_work_item item
);
static bool check_constraint_node_is_not_enforced(const struct mylite_sql_ast_node *node);
static bool check_constraint_node_is_enforcement(const struct mylite_sql_ast_node *node);
static int reserve_planned_check_constraints(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t requested_capacity
);
static int extract_foreign_key_definition_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *foreign_key,
    struct foreign_key_definition_nodes *out_nodes
);
static int record_foreign_key_definition_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
);
static int record_foreign_key_identifier_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
);
static int record_foreign_key_qualified_identifier_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
);
static int record_foreign_key_index_name_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
);
static int record_foreign_key_part_list_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
);
static int record_foreign_key_action_list_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
);
static int copy_foreign_key_index_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *index_name_node,
    char *destination,
    size_t destination_size
);
static int parse_foreign_key_actions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *action_list_node,
    char *update_rule,
    size_t update_rule_size,
    char *delete_rule,
    size_t delete_rule_size
);
static int copy_foreign_key_rule_text(
    struct mylite_db *database,
    char *destination,
    size_t destination_size,
    const char *rule
);
static bool foreign_key_rule_equals(const char *rule, const char *expected_rule);
static bool foreign_key_rule_is_cascade(const char *rule);
static bool foreign_key_rule_is_set_null(const char *rule);
static int validate_planned_foreign_key_set_null_columns(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_foreign_key *foreign_key
);
static void foreign_key_column_names_deinit(struct foreign_key_column_names *names);
static int copy_foreign_key_part_names(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *part_list,
    struct foreign_key_part_name **out_parts,
    size_t *out_part_count
);
static int validate_foreign_key_part_counts(
    struct mylite_db *database,
    size_t child_part_count,
    size_t parent_part_count
);
static int reject_duplicate_foreign_key_child_parts(
    struct mylite_db *database,
    const struct foreign_key_column_names *names
);
static int plan_create_table_foreign_key_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_sql_ast_node *constraint_name_node,
    char *destination,
    size_t destination_size
);
static int generate_create_table_foreign_key_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    char *destination,
    size_t destination_size
);
static bool planned_foreign_key_name_is_used(
    const struct planned_create_table *plan,
    const char *foreign_key_name
);
static int validate_foreign_key_integer_columns(
    struct mylite_db *database,
    const char *child_logical_type,
    const char *parent_logical_type
);
static void planned_foreign_key_deinit(struct planned_foreign_key *foreign_key);
static int append_planned_foreign_key_part(
    struct mylite_db *database,
    struct planned_foreign_key *foreign_key,
    size_t child_column_index,
    const struct mylite_catalog_column_descriptor *parent_column
);
static int reserve_planned_foreign_key_parts(
    struct mylite_db *database,
    struct planned_foreign_key *foreign_key,
    size_t required_capacity
);
static int plan_create_table_foreign_key_parent(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_sql_ast_node *parent_table_node,
    const struct foreign_key_column_names *names,
    struct planned_foreign_key *foreign_key
);
static int find_foreign_key_parent_index(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *parent_table,
    const struct planned_foreign_key *foreign_key,
    struct mylite_catalog_index_descriptor *out_index
);
static int find_alter_table_foreign_key_parent_index(
    struct mylite_db *database,
    const struct planned_alter_table_add_foreign_key *plan,
    struct mylite_catalog_index_descriptor *out_index
);
static int choose_create_table_foreign_key_child_index(
    struct mylite_db *database,
    struct planned_create_table *plan,
    struct planned_foreign_key *foreign_key
);
static int add_create_table_foreign_key_child_index(
    struct mylite_db *database,
    struct planned_create_table *plan,
    struct planned_foreign_key *foreign_key
);
static int plan_create_table_foreign_key_child_index_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_foreign_key *foreign_key,
    const char *default_name,
    char *destination,
    size_t destination_size
);
static bool planned_primary_key_starts_with_column(
    const struct planned_create_table *plan,
    const struct planned_foreign_key *foreign_key
);
static int reserve_planned_foreign_keys(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t required_capacity
);
static int append_planned_secondary_index_part(
    struct mylite_db *database,
    struct planned_create_table *plan,
    struct planned_secondary_index *index,
    const struct mylite_sql_ast_node *part
);
static int validate_planned_unique_index_part_list(
    struct mylite_db *database,
    const struct planned_secondary_index *index
);
static int reserve_planned_secondary_index_parts(
    struct mylite_db *database,
    struct planned_secondary_index *index,
    size_t required_capacity
);
static int apply_create_table_inline_unique_indexes(
    struct mylite_db *database,
    struct planned_create_table *plan
);
static int reserve_planned_secondary_indexes(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t required_capacity
);
static int plan_secondary_index_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_sql_ast_node *index_name_node,
    const char *column_name,
    char *destination,
    size_t destination_size
);
static int generate_secondary_index_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const char *base_name,
    char *destination,
    size_t destination_size
);
static bool planned_index_name_is_used(
    const struct planned_create_table *plan,
    const char *index_name
);
static int validate_secondary_index_column(
    struct mylite_db *database,
    const struct planned_column *column
);
static int validate_fulltext_index_column(
    struct mylite_db *database,
    const struct planned_column *column
);
static int validate_spatial_index_column(
    struct mylite_db *database,
    const struct planned_column *column
);
static int validate_fulltext_string_key_column(
    struct mylite_db *database,
    const struct planned_column *column
);
static bool planned_secondary_index_is_fulltext(const struct planned_secondary_index *index);
static bool planned_secondary_index_is_spatial(const struct planned_secondary_index *index);
static const char *planned_secondary_index_unqualified_column_message(
    const struct planned_secondary_index *index
);
static int validate_planned_secondary_index_part(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_secondary_index *index,
    size_t column_index,
    const char *column_name,
    struct secondary_index_part_nodes nodes,
    int64_t *out_prefix_length
);
static int validate_secondary_index_prefix_for_planned_column(
    struct mylite_db *database,
    const struct planned_column *column,
    int64_t prefix_length,
    uint64_t *out_key_bytes
);
static int text_family_prefix_key_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t prefix,
    uint64_t *out_key_bytes
);
static int binary_string_prefix_key_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t prefix,
    uint64_t *out_key_bytes
);
static int binary_string_full_key_bytes(
    struct mylite_db *database,
    const struct binary_string_type_info *info,
    const char *column_name,
    uint64_t *out_key_bytes
);
static int planned_index_part_key_bytes(
    struct mylite_db *database,
    const struct planned_column *column,
    uint64_t *out_key_bytes
);
static int validate_unique_index_column(
    struct mylite_db *database,
    const struct planned_column *column
);
static int validate_planned_secondary_index_key_length(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_secondary_index *index
);
static int validate_create_table_primary_key_key_length(
    struct mylite_db *database,
    const struct planned_create_table *plan
);
static int planned_primary_key_part_key_bytes(
    struct mylite_db *database,
    const struct planned_column *column,
    uint64_t *out_key_bytes
);
static int column_descriptor_primary_key_part_key_bytes(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    uint64_t *out_key_bytes
);
static int parse_secondary_index_part_prefix_length(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *prefix_node,
    const char *column_name,
    int64_t *out_prefix_length
);
static const char *index_part_unsupported_message(
    const struct mylite_sql_ast_node *part,
    const char *default_message
);
static bool secondary_index_part_is_multi_valued(const struct mylite_sql_ast_node *part);
static bool secondary_index_part_is_functional(const struct mylite_sql_ast_node *part);
static const struct mylite_sql_ast_node *secondary_index_part_column_node(
    const struct mylite_sql_ast_node *part
);
static const struct mylite_sql_ast_node *secondary_index_part_prefix_node(
    const struct mylite_sql_ast_node *part
);
static const struct mylite_sql_ast_node *secondary_index_part_direction_node(
    const struct mylite_sql_ast_node *part
);
static enum mylite_catalog_index_sort_direction index_part_sort_direction(
    const struct mylite_sql_ast_node *part
);
static int mark_primary_key_column(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t column_index,
    enum mylite_catalog_index_sort_direction sort_direction
);
static int reserve_planned_primary_key_parts(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t required_capacity
);
static bool planned_primary_key_contains_column(
    const struct planned_create_table *plan,
    size_t column_index
);
static int validate_primary_key_column(struct mylite_db *database, struct planned_column *column);
static int validate_create_table_auto_increment_columns(
    struct mylite_db *database,
    struct planned_create_table *plan,
    bool allow_secondary_key
);
static int validate_auto_increment_column(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    size_t column_index,
    bool allow_secondary_key
);
static bool planned_secondary_index_starts_with_column(
    const struct planned_create_table *plan,
    size_t column_index
);
static int find_planned_column_index(
    const struct planned_column *columns,
    size_t column_count,
    const char *name,
    size_t *out_index
);
static int plan_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *out_column
);
static int validate_column_attributes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct planned_column *column
);
static bool column_attribute_counts_have_duplicates(
    size_t nullability_count,
    size_t default_count,
    size_t primary_key_count,
    size_t auto_increment_count,
    size_t on_update_count,
    size_t charset_count,
    size_t collation_count,
    size_t generated_count
);
static int validate_generated_column_attributes(
    struct mylite_db *database,
    const struct planned_column *column
);
static int validate_non_generated_column_attributes(
    struct mylite_db *database,
    const struct planned_column *column
);
static int apply_column_charset_collation_attributes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *column
);
static int apply_column_comment_attributes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *column
);
static int apply_column_generated_attribute(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *column
);
static enum mylite_catalog_generated_column_kind generated_column_storage_kind(
    const struct mylite_sql_ast_node *generated_clause
);
static bool column_charset_collation_requests_binary(
    bool has_charset,
    const char *charset_name,
    bool has_collation,
    const char *collation_name
);
static int apply_column_binary_charset_collation_attributes(
    struct mylite_db *database,
    struct planned_column *column,
    bool has_charset,
    const char *charset_name,
    bool has_collation,
    const char *collation_name
);
static int validate_column_binary_charset_collation_attributes(
    struct mylite_db *database,
    bool has_charset,
    const char *charset_name,
    bool has_collation,
    const char *collation_name
);
static int normalize_column_to_binary_string_descriptor(
    struct mylite_db *database,
    struct planned_column *column
);
static int normalize_column_to_binary_text_descriptor(
    struct mylite_db *database,
    struct planned_column *column
);
static int copy_column_charset_attribute_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *charset_attribute,
    char *charset_name,
    size_t charset_name_size
);
static int copy_column_collation_attribute_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *collation_attribute,
    char *collation_name,
    size_t collation_name_size
);
static bool planned_column_allows_charset_collation_attributes(const struct planned_column *column);
static bool planned_column_is_national_char_or_varchar(const struct planned_column *column);
static int validate_column_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *default_node,
    const struct planned_column *column
);
static int validate_column_default_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *default_node,
    const struct planned_column *column
);
static int validate_binary_column_default_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *default_node,
    const struct planned_column *column
);
static int finalize_planned_column_defaults(
    struct mylite_db *database,
    struct planned_column *columns,
    size_t column_count
);
static int finalize_planned_column_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_parenthesized_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static int finalize_planned_column_type_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_string_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_string_default_value(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static int finalize_planned_column_numeric_character_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static bool column_default_value_is_numeric_character_literal(
    const struct mylite_sql_ast_node *value_node
);
static int allocate_numeric_character_default_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    char **out_text,
    size_t *out_text_length
);
static int copy_numeric_character_default_span(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    char **out_text,
    size_t *out_text_length
);
static int copy_numeric_character_default_bytes(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    char **out_text,
    size_t *out_text_length
);
static int finalize_planned_column_decimal_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_approximate_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_bit_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_binary_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_binary_blob_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static int finalize_planned_column_year_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_date_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_time_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_datetime_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_timestamp_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_enum_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_set_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_integer_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static int finalize_planned_column_bit_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static int finalize_planned_column_year_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static int finalize_planned_column_text_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static int finalize_planned_column_text_literal_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_character_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static int finalize_planned_column_parenthesized_string_literal_default(
    struct mylite_db *database,
    struct planned_column *column
);
static bool column_default_value_is_current_date_expression(
    const struct mylite_sql_ast_node *value_node
);
static bool column_default_value_is_current_time_expression(
    const struct mylite_sql_ast_node *value_node
);
static bool column_default_value_is_current_timestamp_expression(
    const struct mylite_sql_ast_node *value_node
);
static bool column_default_value_is_parenthesized_text_expression(
    const struct mylite_sql_ast_node *default_node
);
static bool column_default_value_is_parenthesized_binary_blob_expression(
    const struct mylite_sql_ast_node *default_node
);
static void planned_column_descriptor_for_default(
    const struct planned_column *column,
    struct mylite_catalog_column_descriptor *out_descriptor
);
static int copy_planned_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    struct planned_value *value
);
static int copy_planned_character_expression_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    const char *text,
    size_t text_length
);
static int copy_planned_binary_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    const struct planned_value *value
);
static int copy_planned_default_approximate_text(
    struct mylite_db *database,
    struct planned_column *column,
    const struct planned_value *value
);
static int copy_planned_integer_expression_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *expression
);
static int append_integer_expression_default_text(
    struct mylite_dynamic_string *string,
    const struct mylite_sql_ast_node *expression
);
static int append_integer_expression_default_text_item(
    struct mylite_dynamic_string *string,
    struct integer_expression_default_text_stack *stack,
    const struct integer_expression_default_text_work_item *item
);
static int append_integer_expression_default_node_text(
    struct mylite_dynamic_string *string,
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int append_integer_expression_default_parenthesized_text(
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int append_integer_expression_default_unary_text(
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int append_integer_expression_default_binary_text(
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int append_integer_expression_default_mod_text(
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static const char *integer_expression_default_binary_operator_text(
    enum mylite_sql_ast_operator operator_kind
);
static int push_integer_expression_default_text_item(
    struct integer_expression_default_text_stack *stack,
    enum integer_expression_default_text_work_item_kind kind,
    const struct mylite_sql_ast_node *expression,
    const char *text
);
static int append_integer_expression_default_literal_text(
    struct mylite_dynamic_string *string,
    const struct mylite_sql_ast_node *expression
);
static int convert_integer_expression_default_result(
    struct mylite_db *database,
    const struct planned_column *column,
    int64_t value,
    int64_t *out_value
);
static int convert_bit_expression_default_result(
    struct mylite_db *database,
    const struct planned_column *column,
    int64_t value,
    int64_t *out_value
);
static int convert_year_expression_default_result(int64_t value, int64_t *out_value);
static int convert_column_default_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct planned_column *column,
    int64_t *out_value
);
static int parse_column_default_integer_magnitude(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct planned_column *column,
    uint64_t *out_magnitude,
    bool *out_is_negative
);
static int finish_column_default_integer_value(
    struct mylite_db *database,
    const struct planned_column *column,
    const struct integer_column_range *range,
    uint64_t magnitude,
    bool is_negative,
    int64_t *out_value
);
static int parse_column_default_integer_string(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_magnitude,
    bool *out_is_negative
);
static int evaluate_integer_default_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
);
static int evaluate_integer_default_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_arithmetic_eval_frame *frame
);
static int evaluate_integer_default_apply_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_integer_default_apply_unary_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_integer_default_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int evaluate_integer_default_literal_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int evaluate_integer_default_unary_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int evaluate_integer_default_mod_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    const struct mylite_sql_ast_node *expression
);
static int evaluate_integer_default_binary_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    const struct mylite_sql_ast_node *expression
);
static bool integer_default_binary_operator_is_supported(
    enum mylite_sql_ast_operator operator_kind
);
static int parse_integer_default_literal_value(
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    struct scalar_arithmetic_value *out_value
);
static int finish_integer_default_result(
    struct scalar_arithmetic_value_stack *value_stack,
    struct scalar_arithmetic_value *out_value
);
static bool column_default_value_is_parenthesized_expression(
    const struct mylite_sql_ast_node *default_node
);
static int check_duplicate_column_names(
    struct mylite_db *database,
    const struct planned_column *columns,
    size_t column_count
);
static bool text_equals_ascii_case_insensitive(const char *left, const char *right);
static bool text_has_ascii_case_insensitive_prefix(const char *text, const char *prefix);
static char ascii_lower(unsigned char byte);
static bool enum_label_equals_ascii_case_insensitive(
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length
);
static int map_integer_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    const char **out_logical_type,
    const char **out_physical_type
);
static int map_column_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static bool ast_node_is_temporal_column_type(const struct mylite_sql_ast_node *type_node);
static int map_temporal_column_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int map_varchar_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int map_char_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    struct planned_column *out_column
);
static int append_national_character_set_warning(struct mylite_db *database);
static int map_text_family_type(
    struct mylite_db *database,
    const char *column_name,
    const struct mylite_sql_ast_node *type_node,
    struct planned_column *out_column
);
static int assign_text_family_descriptor_type(
    struct mylite_db *database,
    struct planned_column *out_column,
    const char *logical_type
);
static const char *text_family_logical_type_for_length(
    uint64_t character_length,
    uint64_t max_bytes_per_character
);
static uint64_t saturating_text_byte_requirement(
    uint64_t character_length,
    uint64_t max_bytes_per_character
);
static uint64_t max_bytes_per_character_for_charset(const char *charset_name);
static int map_json_type(struct planned_column *out_column);
static int map_spatial_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    struct planned_column *out_column
);
static int map_enum_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int map_enum_label(
    struct mylite_db *database,
    const char *column_name,
    const struct mylite_sql_ast_node *label_node,
    size_t label_index,
    struct enum_type_info *info,
    struct mylite_dynamic_string *descriptor
);
static int check_enum_label_unique(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct enum_type_info *info,
    const char *column_name
);
static int store_enum_label(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    size_t character_length,
    struct enum_type_info *info
);
static int append_enum_descriptor_label(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_length,
    bool needs_comma
);
static int append_enum_quoted_label(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_length
);
static int map_set_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int map_set_member(
    struct mylite_db *database,
    const char *column_name,
    const struct mylite_sql_ast_node *member_node,
    size_t member_index,
    struct set_type_info *info,
    struct mylite_dynamic_string *descriptor
);
static int check_set_member_unique(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct set_type_info *info,
    const char *column_name
);
static bool set_member_contains_comma(const char *text, size_t text_length);
static int store_set_member(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    size_t character_length,
    struct set_type_info *info
);
static int map_binary_string_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int map_bit_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int map_year_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    struct planned_column *out_column
);
static int map_binary_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int map_varbinary_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int map_blob_family_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int assign_binary_descriptor_type(
    struct mylite_db *database,
    struct planned_column *out_column,
    const char *format,
    uint64_t length
);
static int assign_bit_descriptor_type(
    struct mylite_db *database,
    struct planned_column *out_column,
    uint64_t length
);
static int map_decimal_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int map_approximate_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int map_integer_display_width(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    bool *out_has_display_width,
    uint64_t *out_display_width
);
static int append_integer_display_width_warning(struct mylite_db *database);
static int append_year_display_width_warning(struct mylite_db *database);
static const char *logical_type_for_mapped_integer(struct mapped_integer_type integer_type);
static bool logical_type_is_varchar(const char *logical_type);
static bool logical_type_is_char(const char *logical_type);
static bool logical_type_is_national_varchar(const char *logical_type);
static bool logical_type_is_national_char(const char *logical_type);
static bool logical_type_is_national_char_or_varchar(const char *logical_type);
static size_t char_descriptor_prefix_length(const char *logical_type);
static size_t char_descriptor_syntax_overhead(const char *logical_type);
static size_t varchar_descriptor_prefix_length(const char *logical_type);
static size_t varchar_descriptor_syntax_overhead(const char *logical_type);
static uint64_t logical_type_max_bytes_per_character(const char *logical_type);
static bool planned_column_is_varchar(const struct planned_column *column);
static bool planned_column_is_char(const struct planned_column *column);
static bool planned_column_is_char_or_varchar(const struct planned_column *column);
static bool planned_column_is_text_family(const struct planned_column *column);
static bool planned_column_is_string_family(const struct planned_column *column);
static bool planned_column_is_json(const struct planned_column *column);
static bool planned_column_is_spatial(const struct planned_column *column);
static bool planned_column_is_enum(const struct planned_column *column);
static bool planned_column_is_set(const struct planned_column *column);
static bool planned_column_is_binary_string_family(const struct planned_column *column);
static bool planned_column_is_binary_blob_family(const struct planned_column *column);
static bool planned_column_is_bit(const struct planned_column *column);
static bool planned_column_is_year(const struct planned_column *column);
static bool planned_column_is_decimal(const struct planned_column *column);
static bool planned_column_is_approximate(const struct planned_column *column);
static bool planned_column_is_date(const struct planned_column *column);
static bool planned_column_is_time(const struct planned_column *column);
static bool planned_column_is_datetime(const struct planned_column *column);
static bool planned_column_is_timestamp(const struct planned_column *column);
static bool column_default_kind_has_integer_value(
    enum mylite_catalog_column_default_kind default_kind
);
static bool column_default_kind_has_text_value(
    enum mylite_catalog_column_default_kind default_kind
);
static bool column_default_kind_is_expression(enum mylite_catalog_column_default_kind default_kind);
static bool column_default_kind_materializes_value(
    enum mylite_catalog_column_default_kind default_kind
);
static bool column_descriptor_is_varchar(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_char(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_national_char_or_varchar(
    const struct mylite_catalog_column_descriptor *column
);
static bool column_descriptor_is_char_or_varchar(
    const struct mylite_catalog_column_descriptor *column
);
static bool column_descriptor_is_text_family(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_string_family(
    const struct mylite_catalog_column_descriptor *column
);
static bool column_descriptor_is_json(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_spatial(const struct mylite_catalog_column_descriptor *column);
static bool spatial_logical_type_display_text(const char *logical_type, const char **out_type_text);
static bool column_descriptor_is_enum(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_uses_enum_implicit_missing_default(
    const struct mylite_catalog_column_descriptor *column
);
static bool column_descriptor_is_set(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_binary_string_family(
    const struct mylite_catalog_column_descriptor *column
);
static bool column_descriptor_is_binary_blob_family(
    const struct mylite_catalog_column_descriptor *column
);
static bool column_descriptor_is_bit(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_year(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_decimal(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_approximate(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_date(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_time(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_datetime(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_timestamp(const struct mylite_catalog_column_descriptor *column);
static int decimal_type_info_for_logical_type(
    const char *logical_type,
    struct decimal_type_info *out_info
);
static int approximate_type_info_for_logical_type(
    const char *logical_type,
    struct approximate_type_info *out_info
);
static int enum_type_info_for_logical_type(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    struct enum_type_info *out_info
);
static int parse_enum_descriptor_label(
    struct mylite_db *database,
    const char *logical_type,
    size_t end_index,
    size_t *index,
    struct enum_type_info *out_info
);
static int finish_parse_enum_descriptor_label(
    struct mylite_db *database,
    size_t label_start,
    struct enum_type_info *out_info
);
static int read_enum_descriptor_escaped_byte(
    struct mylite_db *database,
    const char *logical_type,
    size_t end_index,
    size_t *index,
    char *out_byte
);
static int enum_type_info_append_byte(
    struct mylite_db *database,
    struct enum_type_info *info,
    char byte
);
static bool enum_find_label(
    const struct enum_type_info *info,
    const char *text,
    size_t text_length,
    const struct enum_label_descriptor **out_label
);
static bool enum_find_ordinal(
    const struct enum_type_info *info,
    uint64_t ordinal,
    const struct enum_label_descriptor **out_label
);
static bool enum_text_is_unsigned_integer(
    const char *text,
    size_t text_length,
    uint64_t *out_value
);
static int set_type_info_for_logical_type(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    struct set_type_info *out_info
);
static int parse_set_descriptor_member(
    struct mylite_db *database,
    const char *logical_type,
    size_t end_index,
    size_t *index,
    struct set_type_info *out_info
);
static int finish_parse_set_descriptor_member(
    struct mylite_db *database,
    size_t member_start,
    struct set_type_info *out_info
);
static int read_set_descriptor_escaped_byte(
    struct mylite_db *database,
    const char *logical_type,
    size_t end_index,
    size_t *index,
    char *out_byte
);
static int set_type_info_append_byte(
    struct mylite_db *database,
    struct set_type_info *info,
    char byte
);
static bool set_find_member(
    const struct set_type_info *info,
    const char *text,
    size_t text_length,
    size_t *out_index
);
static bool set_bitmap_is_valid(const struct set_type_info *info, uint64_t bitmap);
static const struct text_family_type_info *text_family_type_info_for_logical_type(
    const char *logical_type
);
static const struct binary_string_type_info *binary_string_type_info_for_logical_type(
    const char *logical_type,
    struct binary_string_type_info *storage
);
static int parse_binary_descriptor_length(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    size_t *out_length
);
static int parse_varbinary_descriptor_length(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    size_t *out_length
);
static int bit_width_for_logical_type(const char *logical_type, uint64_t *out_width);
static size_t bit_byte_width(uint64_t width);
static uint64_t bit_width_max_value(uint64_t width);
static bool column_descriptor_is_auto_increment(
    const struct mylite_catalog_column_descriptor *column
);
static int validate_planned_table_row_size(
    struct mylite_db *database,
    const struct planned_column *columns,
    size_t column_count
);
static int validate_added_column_row_size(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const struct planned_column *added_column
);
static int add_planned_column_row_size(
    struct mylite_db *database,
    const struct planned_column *column,
    uint64_t *total
);
static int add_catalog_column_row_size(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    uint64_t *total
);
static int row_size_for_column_descriptor(
    struct mylite_db *database,
    const char *logical_type,
    const char *physical_type,
    const char *unsupported_message,
    uint64_t *out_size
);
static int binary_string_row_size_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t *out_size,
    const char *unsupported_message
);
static int text_backed_row_size_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t *out_size,
    const char *unsupported_message
);
static int set_row_size_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t *out_size,
    const char *unsupported_message
);
static int integer_row_size_bytes(const char *logical_type, uint64_t *out_size);
static int decimal_row_size_bytes(const struct decimal_type_info *info, uint64_t *out_size);
static int approximate_row_size_bytes(const struct approximate_type_info *info, uint64_t *out_size);
static uint64_t decimal_digit_group_size(uint64_t digits);
static int add_row_size_bytes(struct mylite_db *database, uint64_t addition, uint64_t *total);
static int validate_planned_string_key_column(
    struct mylite_db *database,
    const struct planned_column *column
);
static int validate_descriptor_string_key_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int validate_string_key_value(struct mylite_db *database, const struct planned_value *value);
static bool loaded_index_part_requires_string_key_validation(const struct loaded_index_part *part);
static bool text_value_is_supported_string_key(const char *text, size_t text_length);
static bool modify_column_integer_value_domain_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool modify_column_char_varchar_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool modify_column_text_family_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool modify_column_binary_string_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool column_is_nullable(const struct mylite_sql_ast_node *nullability_node);

static int reject_auto_increment_column_definition(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_definition,
    const char *message
);
static int plan_alter_table_rename_column_from_columns(
    struct mylite_db *database,
    struct planned_alter_table_rename_column *out_plan,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *old_column_name
);
static int find_column_index(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *name,
    size_t *out_index
);
static int resolve_descriptor_column_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    enum column_reference_diagnostic_context diagnostic_context,
    const char *unsupported_message,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
);
static int resolve_descriptor_column_reference_with_source_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    enum column_reference_diagnostic_context diagnostic_context,
    const char *unsupported_message,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
);
static int resolve_joined_descriptor_column_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    enum column_reference_diagnostic_context diagnostic_context,
    const char *unsupported_message,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
);
static bool column_reference_source_matches_parts(
    const struct planned_select_source *source,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count
);
static int collect_column_reference_parts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t *out_part_count
);
static int format_column_reference_name(
    struct mylite_db *database,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    char *destination,
    size_t destination_size
);
static bool column_reference_qualifier_matches_source(
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    const struct select_source_context *source_context
);
static void set_unknown_column_reference_error(
    struct mylite_db *database,
    enum column_reference_diagnostic_context context,
    const char *column_name
);
static size_t count_visible_columns(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
);
static int collect_insert_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    const struct planned_insert *plan,
    const struct mylite_sql_ast_node *row_list,
    size_t **out_indexes,
    size_t *out_index_count
);
static bool insert_column_list_is_omitted(const struct mylite_sql_ast_node *column_list);
static bool insert_row_list_first_row_is_empty(const struct mylite_sql_ast_node *row_list);
static size_t count_visible_insert_target_columns(const struct planned_insert *plan);
static void collect_visible_insert_target_indexes(
    const struct planned_insert *plan,
    size_t *indexes
);
static int collect_explicit_insert_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    const struct planned_insert *plan,
    size_t *indexes,
    size_t column_count
);
static int collect_insert_set_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct planned_insert *plan,
    const char *unsupported_qualified_target_message,
    size_t **out_indexes,
    size_t *out_index_count
);
static int check_insert_target_duplicate(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    const size_t *target_indexes,
    size_t target_count
);
static int check_insert_omitted_columns(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const size_t *target_indexes,
    size_t target_count
);
static int validate_insert_row_shapes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row_list,
    size_t target_count
);
static int initialize_insert_auto_increment_plan(
    struct mylite_db *database,
    struct planned_insert *plan
);
static int initialize_insert_auto_increment_range(
    struct mylite_db *database,
    struct planned_insert *plan
);
static bool insert_auto_increment_column_is_indexed(const struct planned_insert *plan);
static int plan_insert_rows(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row_list,
    const size_t *target_indexes,
    size_t target_count,
    struct planned_insert *plan
);
static int plan_insert_row(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row_node,
    size_t row_number,
    const size_t *target_indexes,
    size_t target_count,
    struct planned_insert *plan,
    struct planned_insert_row *out_row
);
static int plan_insert_auto_increment_values(
    struct mylite_db *database,
    struct planned_insert *plan
);
static int load_insert_target_foreign_key_infos(
    struct mylite_db *database,
    struct planned_insert *plan
);
static int plan_insert_string_key_values(
    struct mylite_db *database,
    const struct planned_insert *plan
);
static int plan_insert_duplicate_update(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert *plan
);
static int plan_insert_duplicate_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment,
    struct planned_insert *plan,
    size_t assignment_index
);
static int plan_insert_duplicate_assignment_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value,
    struct planned_insert *plan,
    struct planned_insert_duplicate_update_assignment *duplicate_assignment
);
static int plan_insert_duplicate_values_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_insert *plan,
    struct planned_insert_duplicate_update_assignment *duplicate_assignment
);
static int plan_insert_duplicate_same_column_arithmetic(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_insert *plan,
    struct planned_insert_duplicate_update_assignment *duplicate_assignment,
    bool *out_planned
);
static int validate_insert_duplicate_arithmetic_assignment_target(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t column_index
);
static int plan_insert_duplicate_update_key(
    struct mylite_db *database,
    struct planned_insert *plan
);
static int reject_insert_duplicate_parent_foreign_key_assignments(
    struct mylite_db *database,
    const struct planned_insert *plan
);
static bool insert_duplicate_assignment_targets_key(const struct planned_insert *plan);
static bool insert_duplicate_assignment_targets_auto_increment(const struct planned_insert *plan);
static bool insert_duplicate_assignment_targets_column_id(
    const struct planned_insert *plan,
    int64_t column_id
);
static bool insert_duplicate_column_index_is_keyed(
    const struct planned_insert *plan,
    size_t column_index
);
static bool insert_duplicate_assignment_is_noop(
    const struct planned_insert_duplicate_update_assignment *assignment
);
static size_t count_executable_insert_duplicate_assignments(const struct planned_insert *plan);
static void set_insert_duplicate_arithmetic_unsupported_error(struct mylite_db *database);
static int validate_update_string_key_value(
    struct mylite_db *database,
    const struct planned_update *plan
);
static int count_insert_auto_increment_modes(
    const struct planned_insert *plan,
    struct insert_auto_increment_mode_counts *out_counts
);
static int plan_insert_auto_increment_row_value(
    struct mylite_db *database,
    struct planned_insert *plan,
    const struct integer_column_range *range,
    size_t row_index,
    int64_t *next_value
);
static int generated_auto_increment_value_for_row(
    const struct planned_insert *plan,
    size_t row_index,
    bool *out_generated
);
static int assign_generated_auto_increment_value(
    struct mylite_db *database,
    struct planned_insert *plan,
    size_t row_index,
    int64_t generated_value
);
static int generated_auto_increment_value_for_lower_bound(
    struct mylite_db *database,
    int64_t lower_bound,
    const struct integer_column_range *range,
    int64_t *out_value
);
static int next_auto_increment_after_generated_value(
    const struct mylite_db *database,
    int64_t value,
    const struct integer_column_range *range,
    int64_t *out_next
);
static int next_auto_increment_after_explicit_insert_value(
    int64_t current_next,
    int64_t value,
    const struct integer_column_range *range,
    int64_t *out_next
);
static int next_auto_increment_after_update_value(
    struct mylite_db *database,
    int64_t current_next,
    int64_t value,
    const struct integer_column_range *range,
    int64_t *out_next
);
static int execute_insert_plan_rows(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    struct insert_execution_counters *counters
);
static int execute_insert_plan_row_with_retries(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
);
static int finish_successful_insert_plan_row(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
);
static int handle_insert_plan_constraint(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    size_t *replace_attempt_count,
    struct insert_execution_counters *counters,
    bool *out_row_complete
);
static int handle_replace_insert_constraint(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    size_t *replace_attempt_count,
    struct insert_execution_counters *counters
);
static int validate_insert_ignore_row_foreign_keys(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    bool *out_skip_row
);
static int validate_insert_ignore_row_foreign_key(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const struct loaded_foreign_key_info *foreign_key,
    size_t row_index,
    bool *out_violates
);
static int execute_insert_plan_row(
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    int *out_sqlite_step_rc
);
static int advance_auto_increment_after_insert_row(
    const struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    int64_t *auto_increment_next_after_rows
);
static int advance_auto_increment_after_duplicate_attempt(
    const struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    int64_t *auto_increment_next_after_rows
);
static int advance_auto_increment_after_ignored_duplicate(
    const struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    int64_t *auto_increment_next_after_rows
);
static void record_inserted_generated_auto_increment(
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
);
static int append_insert_omitted_column_warnings(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const size_t *target_indexes,
    size_t target_count
);
static int plan_insert_set_row(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const size_t *target_indexes,
    size_t target_count,
    struct planned_insert *plan
);
static int allocate_insert_row_values(
    struct mylite_db *database,
    const struct planned_insert *plan,
    struct planned_insert_row *out_row
);
static int allocate_insert_column_value(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t column_index,
    struct planned_value *out_value
);
static int make_insert_ignore_implicit_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static bool dml_allows_missing_default_adjustment(
    const struct mylite_db *database,
    bool ignore_errors
);
static bool dml_allows_string_truncation_adjustment(
    const struct mylite_db *database,
    bool ignore_errors
);
static bool insert_allows_implicit_default_adjustment(
    const struct mylite_db *database,
    const struct planned_insert *plan
);
static int convert_insert_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
);
static bool insert_value_is_unix_timestamp_arithmetic(const struct mylite_sql_ast_node *value_node);
static int convert_insert_unix_timestamp_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
);

struct unix_timestamp_arithmetic_messages {
    const char *unsupported;
    const char *delta_range;
};

static int evaluate_insert_unix_timestamp_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct unix_timestamp_arithmetic_messages *messages,
    struct scalar_arithmetic_value *out_value
);
static int evaluate_insert_unix_timestamp_delta(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct unix_timestamp_arithmetic_messages *messages,
    struct scalar_arithmetic_value *out_value
);
static int parse_insert_unix_timestamp_delta_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    const char *delta_range_message,
    struct scalar_arithmetic_value *out_value
);
static int finish_insert_unix_timestamp_integer_value(
    struct mylite_db *database,
    int64_t integer,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int finish_insert_unix_timestamp_text_value(
    struct mylite_db *database,
    int64_t integer,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
);
static int format_insert_unix_timestamp_integer_text(
    struct mylite_db *database,
    int64_t integer,
    char **out_text,
    size_t *out_text_length
);
static bool insert_unix_timestamp_now_node_is_admitted(
    const struct mylite_sql_ast_node *value_node
);
static bool insert_unix_timestamp_delta_node_is_admitted(
    const struct mylite_sql_ast_node *value_node
);
static int convert_statement_time_value_for_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value,
    bool *out_handled
);
static int convert_insert_value_for_plan(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
);
static int convert_insert_value_by_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
);
static int convert_auto_increment_insert_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_null_insert_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool ignore_errors,
    struct planned_value *out_value
);
static int materialize_dml_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool adjust_missing_default,
    struct planned_value *out_value
);
static int materialize_integer_expression_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int materialize_bit_expression_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int materialize_character_expression_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool allow_nonspace_truncation,
    struct planned_value *out_value
);
static int materialize_binary_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int validate_default_function_source_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
);
static int convert_default_function_value_for_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor *target_column,
    bool ignore_errors,
    struct planned_value *out_value
);
static int resolve_default_function_source_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_default_function_source_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *source_column,
    struct planned_value *out_value
);
static bool default_function_source_target_types_are_compatible(
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column
);
static const char *default_function_implicit_conversion_message(
    const struct mylite_catalog_column_descriptor *target_column
);
static int materialize_dml_missing_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_integer_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static bool ascii_decimal_digit(unsigned char byte);
static int copy_dml_numeric_scan_digits(
    struct mylite_db *database,
    const char *text,
    const struct mylite_execution_dml_numeric_token_scan *scan,
    char **out_digits
);
static int dml_integer_value_exceeds_column_range(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    bool *out_exceeds_range
);
static int convert_enum_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    struct planned_value *out_value
);
static int convert_enum_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_numeric_ordinal_fallback,
    bool missing_is_error,
    enum enum_string_trailing_space_policy trailing_space_policy,
    struct planned_value *out_value
);
static void trim_trailing_ascii_spaces(char *text, size_t *text_length);
static int convert_enum_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool missing_is_error,
    struct planned_value *out_value
);
static int enum_literal_ordinal(
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_ordinal,
    bool *out_is_negative
);
static int copy_enum_label_value(
    struct mylite_db *database,
    const struct enum_label_descriptor *label,
    struct planned_value *out_value
);
static int copy_enum_no_match_value(struct mylite_db *database, struct planned_value *out_value);
static int make_enum_first_label_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_set_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    struct planned_value *out_value
);
static int convert_set_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_numeric_bitmap_fallback,
    bool missing_is_error,
    bool canonical_definition_order,
    struct planned_value *out_value
);
static int convert_empty_set_string_literal(
    struct mylite_db *database,
    const struct set_type_info *info,
    bool canonical_definition_order,
    struct planned_value *out_value
);
static int convert_set_member_list_literal(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    bool has_comma,
    const struct set_type_info *info,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_numeric_bitmap_fallback,
    bool missing_is_error,
    bool canonical_definition_order,
    struct planned_value *out_value
);
static int append_set_predicate_member_text(struct set_predicate_member_text_request request);
static int finish_set_member_list_conversion(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    bool has_comma,
    const struct set_type_info *info,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_numeric_bitmap_fallback,
    bool missing_is_error,
    bool canonical_definition_order,
    bool all_members_known,
    uint64_t bitmap,
    struct mylite_dynamic_string *display,
    struct planned_value *out_value
);
static int convert_set_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool missing_is_error,
    struct planned_value *out_value
);
static int copy_set_bitmap_value(
    struct mylite_db *database,
    const struct set_type_info *info,
    uint64_t bitmap,
    struct planned_value *out_value
);
static int copy_set_no_match_value(struct mylite_db *database, struct planned_value *out_value);
static int convert_varchar_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_nonspace_truncation,
    struct planned_value *out_value
);
static int convert_char_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_nonspace_truncation,
    struct planned_value *out_value
);
static int convert_text_family_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_nonspace_truncation,
    bool warn_on_trailing_space_truncation,
    struct planned_value *out_value
);
static int convert_decimal_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_decimal_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    const struct decimal_type_info *info,
    struct planned_value *out_value
);
static int convert_decimal_scanned_string_literal(
    struct mylite_db *database,
    const char *text,
    const struct mylite_execution_dml_numeric_token_scan *scan,
    bool truncated,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool adjust_errors,
    const struct decimal_type_info *info,
    struct planned_value *out_value
);
static int build_decimal_text_from_numeric_scan(
    struct mylite_db *database,
    const char *text,
    const struct mylite_execution_dml_numeric_token_scan *scan,
    const struct decimal_type_info *info,
    char **out_text,
    bool *out_is_out_of_range
);
static int64_t dml_numeric_scan_decimal_position(
    const struct mylite_execution_dml_numeric_token_scan *scan
);
static bool dml_numeric_scan_exceeds_decimal_range(
    const struct mylite_execution_dml_numeric_token_scan *scan,
    const struct decimal_type_info *info
);
static size_t dml_numeric_scan_decimal_text_size(
    const struct mylite_execution_dml_numeric_token_scan *scan,
    const struct decimal_type_info *info
);
static char dml_numeric_scan_virtual_digit(
    const char *digits,
    const struct mylite_execution_dml_numeric_token_scan *scan,
    int64_t digit_index
);
static bool dml_numeric_scan_has_nonzero_virtual_digit_at_or_after(
    const struct mylite_execution_dml_numeric_token_scan *scan,
    int64_t digit_index
);
static int convert_approximate_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_approximate_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    const struct approximate_type_info *info,
    struct planned_value *out_value
);
static int convert_approximate_scanned_string_literal(
    struct mylite_db *database,
    const char *text,
    const struct mylite_execution_dml_numeric_token_scan *scan,
    bool truncated,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool adjust_errors,
    const struct approximate_type_info *info,
    struct planned_value *out_value
);
static int parse_approximate_scanned_value(
    struct mylite_db *database,
    const char *text,
    const struct mylite_execution_dml_numeric_token_scan *scan,
    double *out_value,
    bool *out_is_out_of_range
);
static int convert_adjusted_approximate_range_value(
    struct mylite_db *database,
    const struct approximate_type_info *info,
    bool is_negative,
    const char *column_name,
    size_t row_number,
    struct planned_value *out_value
);
static bool approximate_value_is_out_of_range(
    double value,
    const struct approximate_type_info *info
);
static double approximate_storage_value(double value, const struct approximate_type_info *info);
static double approximate_endpoint_value(const struct approximate_type_info *info, bool negative);
static int convert_date_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_time_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_datetime_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_timestamp_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_predicate_enum_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_predicate_set_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_year_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_year_integer_value(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_year_string_value(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_year_string_magnitude(const char *text, size_t text_length, uint32_t *out_year);
static int convert_year_direct_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_value *out_value
);
static int convert_year_direct_string(const char *text, size_t text_length, uint32_t *out_year);
static int parse_year_value_magnitude(
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_magnitude,
    bool *out_is_negative,
    bool *out_parse_overflow
);
static bool convert_year_numeric_magnitude(
    uint64_t magnitude,
    bool is_negative,
    uint32_t *out_year
);
static int make_year_value(
    struct mylite_db *database,
    uint32_t year,
    struct planned_value *out_value
);
static int make_zero_year_value(struct mylite_db *database, struct planned_value *out_value);
static int make_zero_year_value_with_warning(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool incorrect_integer,
    char *value_text,
    struct planned_value *out_value
);
static bool year_text_is_canonical(const char *text, size_t text_length);
static int canonicalize_date_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_relaxed_date_storage_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static bool normalize_date_storage_text(
    const struct temporal_predicate_normalization_input *input,
    char *out_text,
    enum temporal_storage_truncation *out_truncation
);
static bool datetime_storage_time_is_midnight(const char *text);
static char *copy_temporal_text(struct mylite_db *database, const char *text, size_t text_length);
static int canonicalize_time_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int canonicalize_datetime_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_relaxed_datetime_storage_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    int target_offset_minutes,
    struct planned_value *out_value
);
static int canonicalize_timestamp_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_relaxed_timestamp_storage_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    int target_offset_minutes,
    struct planned_value *out_value
);
static bool session_sql_mode_is_strict(const struct mylite_db *database);
static bool temporal_sql_mode_is_strict(const struct mylite_db *database);
static bool temporal_sql_mode_has_no_zero_date(const struct mylite_db *database);
static bool temporal_sql_mode_has_no_zero_in_date(const struct mylite_db *database);
static bool temporal_sql_mode_has_allow_invalid_dates(const struct mylite_db *database);
static enum temporal_text_kind classify_date_text(const char *text, size_t text_length);
static enum temporal_text_kind classify_datetime_text(const char *text, size_t text_length);
static enum temporal_text_kind classify_temporal_date_components(
    uint32_t year,
    uint32_t month,
    uint32_t day
);
static int make_date_zero_with_warning(
    struct mylite_db *database,
    char *text,
    const char *column_name,
    size_t row_number,
    struct planned_value *out_value
);
static int make_datetime_zero_with_warning(
    struct mylite_db *database,
    char *text,
    const char *column_name,
    size_t row_number,
    struct planned_value *out_value
);
static int make_timestamp_zero_with_warning(
    struct mylite_db *database,
    char *text,
    const char *column_name,
    size_t row_number,
    struct planned_value *out_value
);
static bool predicate_date_text_admitted(
    const struct mylite_db *database,
    const char *text,
    size_t text_length
);
static bool predicate_datetime_text_admitted(
    const struct mylite_db *database,
    const char *text,
    size_t text_length
);
static bool predicate_timestamp_text_admitted(
    const struct mylite_db *database,
    const char *text,
    size_t text_length
);
static bool date_text_has_canonical_shape(const char *text, size_t text_length);
static bool date_text_is_zero_date(const char *text, size_t text_length);
static bool time_text_is_canonical_valid(const char *text, size_t text_length);
static bool time_text_has_canonical_shape(const char *text, size_t text_length);
static bool time_text_uses_canonical_hour_width(
    const char *text,
    size_t text_length,
    const uint32_t *hour
);
static bool time_text_to_seconds(const char *text, size_t text_length, int64_t *out_seconds);
static bool time_text_to_components(
    const char *text,
    size_t text_length,
    bool *out_is_negative,
    uint32_t *out_hour,
    uint32_t *out_minute,
    uint32_t *out_second
);
static bool datetime_text_is_canonical_valid(const char *text, size_t text_length);
static bool timestamp_text_is_canonical_valid(const char *text, size_t text_length);
static bool timestamp_text_in_supported_range(const char *text, size_t text_length);
static bool datetime_text_has_canonical_shape(const char *text, size_t text_length);
static bool datetime_text_is_zero_datetime(const char *text, size_t text_length);
static bool datetime_time_components_valid(uint32_t hour, uint32_t minute, uint32_t second);
static bool date_component_text_to_u32(const char *text, size_t length, uint32_t *out_value);
static bool date_year_month_day_valid(uint32_t year, uint32_t month, uint32_t day);
static bool date_year_is_leap(uint32_t year);
static int make_zero_date_value(struct mylite_db *database, struct planned_value *out_value);
static int make_zero_time_value(struct mylite_db *database, struct planned_value *out_value);
static int make_zero_datetime_value(struct mylite_db *database, struct planned_value *out_value);
static int make_zero_timestamp_value(struct mylite_db *database, struct planned_value *out_value);
static int make_current_timestamp_value(
    struct mylite_db *database,
    struct planned_value *out_value
);
static int make_sysdate_value(struct mylite_db *database, struct planned_value *out_value);
static int make_current_date_value(struct mylite_db *database, struct planned_value *out_value);
static int make_current_time_value(struct mylite_db *database, struct planned_value *out_value);
static int make_utc_timestamp_value(struct mylite_db *database, struct planned_value *out_value);
static int make_utc_date_value(struct mylite_db *database, struct planned_value *out_value);
static int make_utc_time_value(struct mylite_db *database, struct planned_value *out_value);
static int64_t current_timestamp_epoch(const struct mylite_db *database);
static int format_current_timestamp_text(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
);
static int format_sysdate_text(struct mylite_db *database, char *buffer, size_t buffer_size);
static int format_current_date_text(struct mylite_db *database, char *buffer, size_t buffer_size);
static int format_current_time_text(struct mylite_db *database, char *buffer, size_t buffer_size);
static int format_utc_timestamp_text(struct mylite_db *database, char *buffer, size_t buffer_size);
static int format_utc_date_text(struct mylite_db *database, char *buffer, size_t buffer_size);
static int format_utc_time_text(struct mylite_db *database, char *buffer, size_t buffer_size);
static int format_session_timestamp_epoch_text(
    struct mylite_db *database,
    int64_t epoch,
    char *buffer,
    size_t buffer_size
);
static int current_timestamp_session_parts(struct mylite_db *database, struct tm *out_time_parts);
static int sysdate_session_parts(struct mylite_db *database, struct tm *out_time_parts);
static int current_timestamp_utc_parts(struct mylite_db *database, struct tm *out_time_parts);
static int session_time_parts_from_epoch(
    struct mylite_db *database,
    int64_t epoch,
    struct tm *out_time_parts
);
static int utc_time_parts_from_epoch(
    struct mylite_db *database,
    int64_t epoch,
    struct tm *out_time_parts
);
static int64_t current_wall_clock_epoch(void);
static int make_zero_approximate_value(struct planned_value *out_value);
static int parse_approximate_literal_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    bool is_negative,
    const struct approximate_type_info *info,
    const char *column_name,
    size_t row_number,
    double *out_value
);
static int assign_approximate_value(double value, struct planned_value *out_value);
static int copy_approximate_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int format_approximate_value_text(
    struct mylite_db *database,
    double value,
    const struct approximate_type_info *info,
    const char *context_name,
    char *buffer,
    size_t buffer_size
);
static int format_float_value_text(
    struct mylite_db *database,
    double value,
    const char *context_name,
    char *buffer,
    size_t buffer_size
);
static int assign_text_value(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    struct planned_value *out_value
);
static int make_json_null_value(struct mylite_db *database, struct planned_value *out_value);
static int make_empty_blob_value(struct mylite_db *database, struct planned_value *out_value);
static int convert_json_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int normalize_json_value(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    struct planned_value *out_value
);
static int assign_blob_value(
    struct mylite_db *database,
    char *bytes,
    size_t byte_count,
    struct planned_value *out_value
);
static int copy_text_value(
    struct mylite_db *database,
    const char *text,
    struct planned_value *out_value
);
static int copy_blob_value(
    struct mylite_db *database,
    const void *bytes,
    size_t byte_count,
    struct planned_value *out_value
);
static int canonicalize_decimal_text(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    bool is_negative,
    const struct decimal_type_info *info,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int parse_decimal_literal_parts(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    struct decimal_literal_parts *out_parts
);
static int build_decimal_digit_buffer(
    struct mylite_db *database,
    const char *text,
    const struct decimal_type_info *info,
    const struct decimal_literal_parts *parts,
    struct decimal_digit_buffer *out_buffer
);
static int round_decimal_digit_buffer_if_needed(
    struct mylite_db *database,
    const char *text,
    const struct decimal_type_info *info,
    const struct decimal_literal_parts *parts,
    struct decimal_digit_buffer *buffer
);
static bool decimal_value_is_out_of_range(
    const struct decimal_type_info *info,
    bool is_negative,
    const char *digits,
    size_t digit_count
);
static int handle_decimal_out_of_range(
    struct mylite_db *database,
    const struct decimal_type_info *info,
    const char *column_name,
    size_t row_number,
    bool is_negative,
    bool ignore_errors,
    struct planned_value *out_value
);
static int build_decimal_zero_value(
    struct mylite_db *database,
    const struct decimal_type_info *info,
    struct planned_value *out_value
);
static int build_decimal_endpoint_value(
    struct mylite_db *database,
    const struct decimal_type_info *info,
    bool is_negative,
    struct planned_value *out_value
);
static int assign_decimal_text_value(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    struct planned_value *out_value
);
static int copy_decimal_text_value(
    struct mylite_db *database,
    const char *text,
    struct planned_value *out_value
);
static bool decimal_digits_are_zero(const char *digits, size_t digit_count);
static int increment_decimal_digits(char *digits, size_t *digit_count, size_t capacity);
static int format_decimal_digits(
    struct mylite_db *database,
    const char *digits,
    size_t digit_count,
    bool is_negative,
    uint64_t scale,
    struct planned_value *out_value
);
static int decode_sql_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    const char *unsupported_message,
    const char *nul_message,
    char **out_text,
    size_t *out_text_length
);
static int decode_sql_string_literal_with_policy(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    const char *unsupported_message,
    const char *nul_message,
    bool allow_nul,
    char **out_text,
    size_t *out_text_length
);
static int append_decoded_sql_string_escape(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    char escaped_byte,
    const char *nul_message,
    bool allow_nul
);
static int convert_binary_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_bit_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int make_implicit_bit_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_bit_value_magnitude(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    uint64_t magnitude,
    bool is_negative,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int clip_bit_value_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool include_out_of_range_warning,
    struct planned_value *out_value
);
static int parse_bit_value_magnitude(
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_magnitude,
    bool *out_is_negative,
    bool *out_parse_overflow
);
static int parse_bit_literal_magnitude(
    const struct mylite_sql_source_span *span,
    uint64_t *out_magnitude,
    bool *out_parse_overflow
);
static int bit_string_bytes_to_magnitude(
    const char *bytes,
    size_t byte_count,
    uint64_t *out_magnitude,
    bool *out_parse_overflow
);
static int make_bit_blob_value(
    struct mylite_db *database,
    const struct bit_blob_value *bit_value,
    struct planned_value *out_value
);
static int format_bit_default_text(
    struct mylite_db *database,
    const char *column_name,
    uint64_t magnitude,
    char *buffer,
    size_t buffer_size
);
static int decode_binary_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    char **out_bytes,
    size_t *out_byte_count
);
static int decode_binary_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_bytes,
    size_t *out_byte_count
);
static int decode_quoted_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_bytes,
    size_t *out_byte_count
);
static int decode_0x_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_bytes,
    size_t *out_byte_count
);
static int hex_nibble_value(char byte, unsigned char *out_value);
static int convert_binary_bytes_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    char **bytes,
    size_t *byte_count
);
static int make_implicit_binary_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int validate_varchar_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct varchar_text_validation validation
);
static int convert_varchar_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct char_text_conversion *conversion,
    bool allow_nonspace_truncation
);
static int convert_char_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct char_text_conversion *conversion,
    bool allow_nonspace_truncation
);
static int validate_canonical_char_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct varchar_text_validation validation
);
static int validate_text_family_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct varchar_text_validation validation
);
static int convert_text_family_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct char_text_conversion *conversion,
    bool allow_nonspace_truncation,
    bool warn_on_trailing_space_truncation
);
static int utf8_prefix_byte_length_for_byte_limit(
    struct utf8_byte_prefix_request request,
    size_t *out_prefix_length
);
static int validate_canonical_decimal_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const char *text,
    size_t text_length,
    size_t row_number
);
static int validate_utf8_text(const char *text, size_t text_length, size_t *out_character_count);
static int utf8_prefix_byte_length_for_character_count(
    struct utf8_prefix_request request,
    size_t *out_prefix_length,
    size_t *out_character_count
);
static bool text_is_ascii_spaces(const char *text, size_t text_length);
static int utf8_sequence_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
);
static int utf8_two_byte_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
);
static int utf8_three_byte_e0_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
);
static int utf8_three_byte_plain_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
);
static int utf8_three_byte_surrogate_gap_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
);
static int utf8_four_byte_f0_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
);
static int utf8_four_byte_plain_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
);
static int utf8_four_byte_last_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
);
static bool utf8_byte_is_continuation(unsigned char byte);
static int varchar_length_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t *out_length
);
static int char_length_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t *out_length
);
static int parse_varchar_descriptor_length(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    size_t *out_length
);
static int parse_char_descriptor_length(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    size_t *out_length
);
static int make_empty_text_value(struct mylite_db *database, struct planned_value *out_value);
static int clip_integer_for_column(
    struct mylite_db *database,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    int64_t *out_value
);
static int parse_unsigned_integer_literal(
    const struct mylite_sql_source_span *span,
    uint64_t *out_value
);
static bool boolean_literal_magnitude(
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_value
);
static int convert_integer_for_column(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    int64_t *out_value
);
static int convert_integer_for_column_with_policy(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    int64_t *out_value
);
static int plan_select_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
);
static bool select_source_context_is_joined(const struct select_source_context *source_context);
static int plan_select_wildcard_columns(
    struct mylite_db *database,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
);
static int plan_select_qualified_wildcard_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *qualified_wildcard,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
);
static int collect_qualified_wildcard_parts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *qualified_wildcard,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t *out_part_count
);
static int set_unknown_qualified_wildcard_table_error(
    struct mylite_db *database,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count
);
static bool select_source_matches_qualified_wildcard_parts(
    const struct select_source_context *source_context,
    const char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count
);
static bool joined_source_matches_qualified_wildcard_parts(
    const struct planned_select_source *source,
    const char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count
);
static int append_visible_select_columns_from_source(
    struct mylite_db *database,
    struct planned_select *out_plan,
    size_t source_index,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
);
static int plan_joined_select_wildcard_columns(
    struct mylite_db *database,
    const struct select_source_context *source_context,
    struct planned_select *out_plan
);
static int plan_single_source_select_wildcard_columns(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
);
static int select_item_column_reference(
    const struct mylite_sql_ast_node *item,
    const struct mylite_sql_ast_node **out_column
);
static bool select_item_qualified_wildcard(
    const struct mylite_sql_ast_node *item,
    const struct mylite_sql_ast_node **out_qualified_wildcard
);
static int plan_select_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *out_predicate
);
static int plan_joined_select_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *source_context,
    struct planned_select_predicate *out_predicate
);
static int plan_select_predicate_with_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static int plan_select_having_predicate_with_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *having_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static int merge_select_predicate_with_and(
    struct mylite_db *database,
    struct planned_select_predicate *target,
    struct planned_select_predicate *addition
);
static void planned_select_predicate_deinit(struct planned_select_predicate *predicate);
static void planned_select_predicate_deinit_without_exists(
    struct planned_select_predicate *predicate
);
static void planned_exists_subquery_deinit(struct planned_exists_subquery *subquery);
static void planned_in_subquery_deinit(struct planned_in_subquery *subquery);
static int plan_select_predicate_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static int plan_select_predicate_node_without_exists(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static int plan_select_predicate_work_item(
    struct mylite_db *database,
    struct predicate_work_item item,
    struct predicate_work_item **items,
    size_t *item_count,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static int plan_select_predicate_work_item_without_exists(
    struct mylite_db *database,
    struct predicate_work_item item,
    struct predicate_work_item **items,
    size_t *item_count,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static int finish_planned_select_logical_predicate(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    size_t **result_indexes,
    size_t *result_index_count,
    struct planned_select_predicate *out_predicate
);
static int finish_planned_select_not_predicate(
    struct mylite_db *database,
    size_t **result_indexes,
    size_t *result_index_count,
    struct planned_select_predicate *out_predicate
);
static int plan_select_predicate_ast_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct predicate_work_item **items,
    size_t *item_count,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static int plan_select_predicate_ast_node_without_exists(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct predicate_work_item **items,
    size_t *item_count,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static const struct mylite_sql_ast_node *unwrap_parenthesized_predicate(
    const struct mylite_sql_ast_node *node
);
static bool is_logical_predicate_node(const struct mylite_sql_ast_node *node);
static int append_select_predicate_logical_work(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct predicate_work_item **items,
    size_t *item_count
);
static int append_select_predicate_not_work(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct predicate_work_item **items,
    size_t *item_count
);
static int append_select_predicate_deprecated_warning_work(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    struct predicate_work_item **items,
    size_t *item_count
);
static int plan_select_predicate_leaf_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static int plan_select_predicate_leaf_node_without_exists(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static bool planned_predicate_kind_for_operator(
    enum mylite_sql_ast_operator operator_kind,
    enum planned_select_predicate_kind *out_kind
);
static int plan_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_row_scalar_function_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
);
static int plan_scalar_literal_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_scalar_literal_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_literal_left_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_same_scope_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static bool same_scope_column_comparison_is_supported(
    const struct planned_select_predicate_node *node
);
static int plan_scalar_literal_is_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_constant_truth_predicate(
    struct mylite_db *database,
    bool truth,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_where_scalar_literal_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_select_predicate_node *node
);
static int plan_where_scalar_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static bool predicate_node_is_scalar_literal_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static bool predicate_node_is_column_reference(const struct mylite_sql_ast_node *node);
static bool predicate_comparison_value_is_null(const struct mylite_sql_ast_node *value_node);
static enum mylite_sql_ast_operator comparison_operator_flipped(
    enum mylite_sql_ast_operator operator_kind
);
static bool scalar_literal_is_result(
    const struct planned_value *value,
    enum mylite_sql_ast_operator operator_kind
);
static bool predicate_node_is_find_in_set_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static bool predicate_node_is_json_valid_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static bool predicate_node_is_json_contains_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static bool predicate_node_is_regexp_like_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static bool predicate_node_is_string_length_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static bool predicate_node_is_substring_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static bool predicate_node_is_date_format_numeric_predicate_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static int plan_date_format_numeric_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static bool predicate_node_is_temporal_extract_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static int plan_temporal_extract_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_temporal_extract_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_temporal_extract_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_temporal_extract_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_temporal_extract_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static bool temporal_extract_predicate_kind_is_numeric(enum mylite_temporal_extract_kind kind);
static int plan_string_length_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_string_length_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_string_length_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_string_length_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_string_length_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static int plan_substring_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_substring_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_substring_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_substring_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static int plan_find_in_set_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_find_in_set_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_find_in_set_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_find_in_set_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_find_in_set_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static int plan_json_valid_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_json_valid_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_json_valid_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_json_valid_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_json_valid_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static int plan_json_contains_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_json_contains_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_json_contains_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_json_contains_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_json_contains_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static int plan_regexp_like_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_regexp_like_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_regexp_like_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_regexp_like_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_regexp_like_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static bool comparison_predicate_rhs_is_column_reference(
    const struct mylite_sql_ast_node *predicate_node
);
static int validate_comparison_predicate_column(
    struct mylite_db *database,
    const struct planned_select_predicate_node *node
);
static int plan_predicate_row_scalar_value_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_exists_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_exists_subquery(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_exists_subquery *out_subquery
);
static int plan_exists_table_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_exists_subquery *out_subquery
);
static int validate_exists_select_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct planned_exists_subquery *subquery
);
static bool exists_select_item_is_supported_literal(const struct mylite_sql_ast_node *expression);
static int plan_exists_inner_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_exists_subquery *subquery
);
static void offset_exists_inner_predicate_source_indexes(
    struct planned_select_predicate *predicate,
    size_t source_index_offset
);
static int plan_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int resolve_exists_column_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *inner_source_context,
    const struct mylite_catalog_column_descriptor *inner_columns,
    size_t inner_column_count,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
);
static int resolve_exists_column_reference_in_source(
    struct mylite_db *database,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    const char *column_name,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct mylite_catalog_column_descriptor *out_column,
    bool *out_resolved
);
static bool exists_correlated_column_comparison_is_supported(
    const struct planned_select_predicate_node *node
);
static bool comparison_operator_is_string_predicate(enum mylite_sql_ast_operator operator_kind);
static bool comparison_operator_is_enum_predicate(enum mylite_sql_ast_operator operator_kind);
static bool comparison_operator_is_like(enum mylite_sql_ast_operator operator_kind);
static bool comparison_operator_is_regexp(enum mylite_sql_ast_operator operator_kind);
static int plan_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_is_boolean_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_between_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_in_literal_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_in_subquery_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_statement,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_select_predicate_node *node
);
static int plan_in_subquery(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_in_subquery *out_subquery
);
static int plan_in_subquery_table_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_in_subquery *out_subquery
);
static int plan_in_subquery_select_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct planned_in_subquery *subquery,
    struct mylite_catalog_column_descriptor *out_column
);
static bool in_subquery_columns_are_compatible(
    const struct mylite_catalog_column_descriptor *outer_column,
    const struct mylite_catalog_column_descriptor *inner_column
);
static int plan_in_subquery_inner_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_in_subquery *subquery
);
static int convert_predicate_in_value_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value **out_values,
    size_t *out_value_count
);
static int convert_predicate_in_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_predicate_year_in_value_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value **out_values,
    size_t *out_value_count
);
static bool year_in_value_list_uses_string_conversion(
    const struct mylite_sql_ast_node *value_list,
    size_t value_count
);
static int append_planned_select_predicate_node(
    struct mylite_db *database,
    struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *out_node_index
);
static int append_deprecated_logical_and_warning(struct mylite_db *database);
static int append_deprecated_logical_or_warning(struct mylite_db *database);
static bool planned_select_predicate_has_expression(
    const struct planned_select_predicate *predicate
);
static int append_predicate_work_node(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count,
    const struct mylite_sql_ast_node *node
);
static int append_predicate_work_deprecated_and_warning(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count
);
static int append_predicate_work_deprecated_or_warning(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count
);
static int append_predicate_work_finish_logical(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count,
    enum mylite_sql_ast_operator operator_kind
);
static int append_predicate_work_finish_not(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count
);
static int append_predicate_work_item(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count,
    struct predicate_work_item item
);
static int append_predicate_result_index(
    struct mylite_db *database,
    size_t **indexes,
    size_t *index_count,
    size_t index
);
static int pop_predicate_result_index(
    const size_t *indexes,
    size_t *index_count,
    size_t *out_index
);
static int bind_select_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate *predicate,
    int *parameter_index
);
static int bind_select_predicate_parameters_without_exists(
    sqlite3_stmt *statement,
    const struct planned_select_predicate *predicate,
    int *parameter_index
);
static int bind_select_exists_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_exists_subquery *subquery,
    int *parameter_index
);
static int bind_select_predicate_node_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_non_in_predicate_node_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_predicate_node_parameters_without_subqueries(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_in_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_in_literal_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_in_subquery_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_in_subquery *subquery,
    int *parameter_index
);
static int resolve_predicate_column_with_source_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
);
static int convert_predicate_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_predicate_value_with_date_comparison_mode(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value,
    bool *out_compare_date_as_datetime
);
static int convert_predicate_date_between_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *lower_node,
    const struct mylite_sql_ast_node *upper_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_lower_value,
    struct planned_value *out_upper_value,
    bool *out_compare_date_as_datetime
);
static int convert_predicate_date_in_value_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value **out_values,
    size_t *out_value_count,
    bool *out_compare_date_as_datetime
);
static int convert_predicate_date_in_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *values,
    size_t value_count,
    bool *out_uses_datetime
);
static int convert_predicate_date_in_values_to_datetime(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *values,
    size_t value_count
);
static bool date_in_value_already_compares_as_datetime(
    const struct mylite_sql_ast_node *value_node,
    const struct planned_value *value
);
static void planned_value_array_deinit(struct planned_value *values, size_t value_count);
static int convert_predicate_year_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_predicate_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_value *out_value
);
static int convert_predicate_string_pattern_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_value *out_value
);
static int convert_predicate_regexp_pattern_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_value *out_value
);
static int convert_predicate_date_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    bool force_datetime_comparison,
    struct planned_value *out_value,
    bool *out_compare_as_datetime
);
static bool normalize_date_datetime_predicate_text(
    const char *text,
    size_t text_length,
    char *out_text,
    bool *out_append_warning
);
static int copy_date_midnight_predicate_value(
    struct mylite_db *database,
    const char *date_text,
    struct planned_value *out_value
);
static int convert_predicate_time_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_predicate_datetime_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_predicate_timestamp_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);

struct temporal_predicate_normalization_input {
    const char *text;
    size_t text_length;
    int target_offset_minutes;
};

static bool normalize_iso_temporal_predicate_text(
    const struct temporal_predicate_normalization_input *input,
    char *out_text
);
static bool normalize_z_temporal_predicate_text(
    const char *text,
    size_t text_length,
    char *out_text
);
static bool parse_temporal_predicate_offset(const char *text, int *out_offset_minutes);
static bool shift_datetime_parts_by_minutes(
    const struct mylite_temporal_datetime_parts *parts,
    int delta_minutes,
    char *out_text
);
static bool format_datetime_parts_text(
    const struct mylite_temporal_datetime_parts *parts,
    char *out_text
);
static int convert_integer_for_predicate(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    int64_t *out_value
);
static int plan_select_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_field_order,
    bool allow_rand_order,
    struct planned_select_order *out_order
);
static int plan_select_order_item_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_items,
    const struct select_source_context *source_context,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_field_order,
    bool allow_rand_order,
    struct planned_select_order *out_order
);
static int plan_select_order_ast_item_and_append(
    struct mylite_db *database,
    struct select_order_ast_item_nodes item_nodes,
    const struct select_source_context *source_context,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_field_order,
    bool allow_rand_order,
    struct planned_select_order *out_order
);
static int plan_select_order_ast_item(
    struct mylite_db *database,
    struct select_order_ast_item_nodes item_nodes,
    const struct select_source_context *source_context,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_field_order,
    bool allow_rand_order,
    struct planned_select_order_item *out_item
);
static bool order_item_list_contains_field_order_key(const struct mylite_sql_ast_node *order_items);
static bool order_item_list_contains_rand_order_key(const struct mylite_sql_ast_node *order_items);
static bool select_order_key_is_field_function(const struct mylite_sql_ast_node *order_key);
static bool select_order_key_is_rand_function(const struct mylite_sql_ast_node *order_key);
static int plan_select_order_field_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_order_item *out_item
);
static int plan_select_order_rand_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    struct planned_select_order_item *out_item
);
static int validate_select_order_field_expression(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression
);
static void planned_select_order_item_deinit(struct planned_select_order_item *item);
static int append_planned_select_order_item(
    struct planned_select_order *order,
    struct planned_select_order_item item
);
static int resolve_order_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct planned_select *select_plan,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index,
    bool *out_resolved
);
static int resolve_order_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
);
static int plan_select_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
);
static int plan_delete_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
);
static int plan_update_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
);
static int plan_joined_update_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct select_source_context *source_context,
    struct planned_update *out_plan
);
static int reject_builtin_schema_joined_update_assignment_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list
);
static int finish_joined_update_target_plan(
    struct mylite_db *database,
    struct planned_update *out_plan
);
static int reject_builtin_schema_joined_update_target(
    struct mylite_db *database,
    const struct planned_update *plan
);
static void set_joined_update_wrong_usage_error(
    struct mylite_db *database,
    const char *clause_name
);
static int plan_update_multiple_assignments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
);
static int plan_update_multiple_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan,
    size_t assignment_index
);
static int plan_update_scalar_subquery_assignment(
    struct mylite_db *database,
    struct planned_update *out_plan
);
static int plan_update_date_interval_assignment(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
);
static int plan_update_arithmetic_assignment(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
);
static int plan_update_date_interval_source_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor *assignment_column
);
static int plan_update_date_interval_target(
    struct mylite_db *database,
    struct planned_update *plan
);
static int update_date_interval_input_kind_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    enum mylite_date_interval_second_input_kind *out_kind
);
static bool update_date_interval_column_family_is_supported(
    const struct mylite_catalog_column_descriptor *column
);
static int validate_update_date_interval_string_target(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static void set_update_date_interval_column_mismatch_error(struct mylite_db *database);
static void set_update_date_interval_target_error(struct mylite_db *database);
static void set_update_date_interval_date_time_unit_error(struct mylite_db *database);
static void set_update_date_interval_keyed_target_error(struct mylite_db *database);
static int update_value_is_constant_arithmetic_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    bool *out_is_constant
);
static int update_constant_arithmetic_visit_node(
    struct mylite_db *database,
    struct scalar_arithmetic_node_stack *stack,
    const struct mylite_sql_ast_node *value_node,
    bool *in_out_admitted
);
static bool update_constant_arithmetic_literal_is_admitted(
    enum mylite_sql_ast_literal_kind literal_kind
);
static bool update_constant_arithmetic_unary_operator_is_admitted(
    enum mylite_sql_ast_operator operator_kind
);
static bool update_constant_arithmetic_binary_operator_is_admitted(
    enum mylite_sql_ast_operator operator_kind
);
static int update_constant_arithmetic_stack_push(
    struct mylite_db *database,
    struct scalar_arithmetic_node_stack *stack,
    const struct mylite_sql_ast_node *value_node
);
static int validate_update_arithmetic_assignment_target(
    struct mylite_db *database,
    const struct planned_update *plan
);
static int validate_update_unix_timestamp_arithmetic_assignment_target(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static bool update_assignment_column_is_keyed(const struct planned_update *plan);
static bool update_column_id_is_keyed(const struct planned_update *plan, int64_t column_id);
static bool update_assignment_value_is_multi_constant_supported(
    const struct mylite_sql_ast_node *value_node
);
static bool update_assignment_unary_value_is_multi_constant_supported(
    const struct mylite_sql_ast_node *value_node
);
static int validate_update_multiple_auto_update_columns(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
);
static void set_update_multiple_assignment_unsupported_error(struct mylite_db *database);
static void set_update_duplicate_assignment_unsupported_error(struct mylite_db *database);
static int validate_update_ignore_assignment_support(
    struct mylite_db *database,
    const struct planned_update *plan
);
static int validate_update_default_function_sources(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
);
static int validate_update_default_function_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct planned_update *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
);
static int validate_update_scalar_subquery_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int convert_update_value(
    struct mylite_db *database,
    const struct planned_update *plan,
    struct planned_value *out_value
);
static int convert_update_value_for_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_update_constant_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_update_unix_timestamp_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int validate_update_constant_arithmetic_assignment_target(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int convert_update_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_update_date_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_update_datetime_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_update_timestamp_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int materialize_update_scalar_subquery_value(
    struct mylite_db *database,
    const struct planned_update *plan,
    struct planned_value *out_value
);
static void cap_update_scalar_subquery_limit(struct planned_select_limit *limit);
static int step_update_scalar_subquery_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_update *plan,
    struct planned_value *out_value
);
static int materialize_update_scalar_subquery_sqlite_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_update *plan,
    struct planned_value *out_value
);
static bool update_scalar_subquery_target_uses_text_storage(
    const struct mylite_catalog_column_descriptor *target_column
);
static bool update_scalar_subquery_target_uses_blob_storage(
    const struct mylite_catalog_column_descriptor *target_column
);
static int materialize_update_scalar_subquery_text_storage_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct mylite_catalog_column_descriptor *target_column,
    struct planned_value *out_value
);
static int materialize_update_scalar_subquery_blob_storage_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct mylite_catalog_column_descriptor *target_column,
    struct planned_value *out_value
);
static int validate_update_scalar_subquery_text_storage_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct mylite_catalog_column_descriptor *target_column
);
static int materialize_update_scalar_subquery_approximate_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct mylite_catalog_column_descriptor *target_column,
    struct planned_value *out_value
);
static int materialize_update_scalar_subquery_integer_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int sqlite_type,
    const struct mylite_catalog_column_descriptor *target_column,
    struct planned_value *out_value
);
static int copy_update_scalar_subquery_text_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    struct planned_value *out_value
);
static bool update_scalar_subquery_source_target_types_are_compatible(
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column
);
static const char *update_scalar_subquery_implicit_conversion_message(
    const struct mylite_catalog_column_descriptor *target_column
);
static int convert_update_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int plan_update_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
);
static int convert_limit_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    int64_t *out_value
);
static int integer_range_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const char *unsupported_message,
    struct integer_column_range *out_range
);
static int integer_range_for_logical_type(
    struct mylite_db *database,
    struct integer_logical_type_range_request request,
    struct integer_column_range *out_range
);
static int plan_row_scalar_select_items(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_select *out_plan
);
static int append_row_scalar_select_item(
    struct mylite_db *database,
    struct planned_row_scalar_select *plan,
    const struct mylite_sql_ast_node *select_item,
    const struct select_source_context *source_context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
);
static int plan_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_special_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    bool *out_handled
);
static int normalize_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_expression,
    const struct mylite_sql_ast_node **out_expression
);
static bool row_scalar_expression_is_rand_function(const struct mylite_sql_ast_node *expression);
static int plan_row_scalar_rand_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static bool row_scalar_expression_is_window_function(const struct mylite_sql_ast_node *expression);
static int plan_row_scalar_window_function_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static enum planned_window_function_kind planned_window_function_kind_from_ast(
    const struct mylite_sql_ast_node *expression
);
static const struct mylite_sql_ast_node *window_function_argument_list_node(
    const struct mylite_sql_ast_node *expression
);
static const struct mylite_sql_ast_node *window_function_spec_node(
    const struct mylite_sql_ast_node *expression
);
static bool window_function_has_arguments(enum planned_window_function_kind kind);
static int plan_row_scalar_window_function_arguments(
    struct mylite_db *database,
    enum planned_window_function_kind kind,
    const struct mylite_sql_ast_node *arguments,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int validate_window_function_argument_count(
    struct mylite_db *database,
    struct window_function_argument_count_request request
);
static int plan_row_scalar_window_function_argument(
    struct mylite_db *database,
    enum planned_window_function_kind kind,
    size_t argument_index,
    const struct mylite_sql_ast_node *argument,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_argument
);
static bool window_function_signed_integer_argument_is_supported(
    enum planned_window_function_kind kind,
    size_t argument_index
);
static int set_unknown_window_argument_column_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument
);
static int plan_row_scalar_window_function_column_argument(
    struct mylite_db *database,
    const char *function_name,
    size_t argument_index,
    const struct mylite_sql_ast_node *argument,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_argument
);
static bool window_function_argument_column_descriptor_is_supported(
    struct mylite_db *database,
    const char *function_name,
    size_t argument_index,
    const struct mylite_catalog_column_descriptor *column
);
static int validate_window_function_integer_argument(
    struct mylite_db *database,
    enum planned_window_function_kind kind,
    const struct planned_row_scalar_expression *expression
);
static bool window_function_null_integer_argument_is_syntax_error(
    enum planned_window_function_kind kind
);
static size_t window_function_integer_argument_index(enum planned_window_function_kind kind);
static void set_incorrect_window_function_argument_error(
    struct mylite_db *database,
    enum planned_window_function_kind kind
);
static int plan_row_scalar_window_partition_clause(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *partition_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_window_order_clause(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool window_key_column_descriptor_is_supported(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_catalog_column_descriptor *column
);
static const char *window_function_display_name(enum planned_window_function_kind kind);
static int set_window_function_unsupported_error(
    struct mylite_db *database,
    const char *function_name,
    const char *detail
);
static int copy_lowercase_function_name(const char *source, char *destination, size_t size);
static int plan_row_scalar_integer_arithmetic_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_integer_arithmetic_frame(
    struct mylite_db *database,
    const struct row_scalar_integer_arithmetic_plan_frame *frame,
    struct row_scalar_integer_arithmetic_plan_stack *stack,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
);
static int plan_row_scalar_integer_arithmetic_binary(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression,
    struct row_scalar_integer_arithmetic_plan_stack *stack
);
static void row_scalar_integer_arithmetic_plan_stack_deinit(
    struct row_scalar_integer_arithmetic_plan_stack *stack
);
static int row_scalar_integer_arithmetic_plan_stack_push(
    struct mylite_db *database,
    struct row_scalar_integer_arithmetic_plan_stack *stack,
    struct row_scalar_integer_arithmetic_plan_frame frame
);
static int plan_row_scalar_integer_arithmetic_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_integer_arithmetic_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool signed_integer_arithmetic_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool *out_cast_as_integer
);
static int plan_row_scalar_date_function_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    bool *out_handled
);
static int plan_row_scalar_temporal_function_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    bool *out_handled
);
static bool row_scalar_expression_is_string_function(enum mylite_sql_ast_node_kind kind);
static int plan_row_scalar_string_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_default_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_length_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_length_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_length_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static bool string_length_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_string_codepoint_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_codepoint_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_codepoint_identifier_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_codepoint_literal_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_codepoint_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool string_codepoint_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_string_case_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_case_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_case_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool string_case_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_string_trim_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_trim_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_trim_remove_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_trim_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool string_trim_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_string_slice_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_slice_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_slice_length_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_slice_position_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_slice_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static bool string_slice_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_string_padding_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_padding_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_padding_count_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_padding_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool string_padding_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_string_bitmask_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_export_set_bitmask_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    size_t argument_count,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_make_set_bitmask_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    size_t argument_count,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_bitmask_bit_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_bitmask_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_bitmask_count_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_bitmask_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool string_bitmask_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_string_search_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_search_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_search_position_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_search_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool string_search_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_string_replace_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_insert_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_replace_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_insert_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_insert_position_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_insert_length_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_replace_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool string_replace_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_string_insert_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool string_insert_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_string_reverse_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_reverse_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_reverse_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool string_reverse_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_soundex_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_soundex_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_soundex_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool soundex_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_string_quote_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_quote_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_quote_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_string_quote_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool string_quote_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_substring_index_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_substring_index_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_substring_index_count_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_substring_index_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool substring_index_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_find_in_set_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_find_in_set_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_find_in_set_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static bool find_in_set_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_strcmp_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_strcmp_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_strcmp_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool strcmp_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_regexp_like_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_regexp_string_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_regexp_string_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
);
static int validate_row_scalar_regexp_string_pattern(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *pattern_argument
);
static int append_row_scalar_regexp_string_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int bind_row_scalar_regexp_string_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int plan_row_scalar_regexp_like_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_regexp_like_pattern_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_regexp_like_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static bool regexp_like_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_json_valid_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_valid_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_valid_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static bool json_valid_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_json_contains_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_contains_path_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_contains_path_mode_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static bool is_row_scalar_json_expression(const struct mylite_sql_ast_node *expression);
static int plan_row_scalar_json_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_extract_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_value_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_length_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_keys_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_type_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_quote_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_unquote_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_set_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int json_mutation_kind_from_ast(
    const struct mylite_sql_ast_node *expression,
    const char **out_function_name,
    enum planned_json_mutation_kind *out_mutation_kind
);
static int plan_row_scalar_json_mutation_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    size_t argument_count,
    const char *function_name,
    enum planned_json_mutation_kind mutation_kind,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_remove_path_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    size_t argument_count,
    const char *function_name,
    struct planned_row_scalar_expression *out_arguments
);
static int plan_row_scalar_json_set_pair_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    const char *function_name,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_set_document_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_array_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_object_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_constructor_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool is_key,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_constructor_identifier(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool is_key,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int set_json_constructor_unknown_column_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static int plan_row_scalar_json_constructor_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool is_key,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_constructor_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_json_sql_value_kind json_value_kind,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_constructor_integer(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_constructor_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool is_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_extract_operator_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_extract_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *document,
    enum planned_row_scalar_expression_kind expression_kind,
    const struct mylite_sql_ast_node *path,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_introspection_document_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_value_document_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_document_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_path_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_value_path_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_set_path_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_set_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_search_path_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_length_path_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_keys_path_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_unquote_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_quote_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_json_quote_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
);
static bool json_quote_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_json_text_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
);
static bool json_text_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const char *unsupported_message
);
static int plan_row_scalar_hex_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_hex_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool hex_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_unhex_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_unhex_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_base64_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_base64_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    enum planned_row_scalar_expression_kind expression_kind,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool base64_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const char *function_name
);
static bool unhex_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_weight_string_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_weight_string_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_weight_string_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool weight_string_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_uuid_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int validate_row_scalar_uuid_expression_arity(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    size_t child_count
);
static int plan_row_scalar_uuid_source_argument_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *argument,
    const struct mylite_sql_ast_node *swap_argument,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    bool *out_handled
);
static bool is_row_scalar_uuid_nested_function_argument(const struct mylite_sql_ast_node *argument);
static int plan_row_scalar_constant_uuid_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_uuid_nested_expression(
    struct mylite_db *database,
    const struct row_scalar_uuid_column_expression_request *request,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_uuid_column_expression(
    struct mylite_db *database,
    const struct row_scalar_uuid_column_expression_request *request,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool uuid_column_descriptor_is_supported(
    struct mylite_db *database,
    enum mylite_sql_ast_node_kind function_kind,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_char_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_binary_string_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    bool *out_handled
);
static int plan_row_scalar_char_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_char_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool char_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_charset_collation_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_table_descriptor *table,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_charset_collation_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum planned_charset_collation_function_kind function_kind,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_table_descriptor *table,
    struct planned_row_scalar_expression *out_expression
);
static int charset_collation_column_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    const char **out_result
);
static int coercibility_column_result(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const char **out_result
);
static bool is_row_scalar_control_flow_expression(const struct mylite_sql_ast_node *expression);
static int plan_row_scalar_control_flow_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_nested_control_flow_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_searched_case_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_like_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_if_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_nested_if_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_ifnull_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_nested_ifnull_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_coalesce_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_nested_coalesce_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_nullif_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_nested_nullif_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_isnull_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_nested_isnull_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_control_flow_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_control_flow_leaf_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_control_flow_condition_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_control_flow_leaf_condition_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_control_flow_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_control_flow_condition_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool control_flow_value_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static bool control_flow_condition_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static enum planned_row_scalar_field_domain row_scalar_control_flow_argument_domain(
    const struct planned_row_scalar_expression *expression
);
static enum planned_row_scalar_field_domain row_scalar_control_flow_column_domain(
    const struct mylite_catalog_column_descriptor *column
);
static enum planned_row_scalar_field_domain row_scalar_control_flow_common_domain(
    const struct planned_row_scalar_expression *arguments,
    size_t argument_count
);
static enum planned_row_scalar_field_domain row_scalar_searched_case_result_domain(
    const struct planned_row_scalar_expression *expression
);
static int plan_row_scalar_non_concat_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_scalar_subquery,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_conversion_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int append_row_scalar_conversion_step(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static enum planned_row_scalar_conversion_kind row_scalar_conversion_kind_from_expression(
    const struct mylite_sql_ast_node *expression
);
static bool row_scalar_conversion_kind_is_numeric(
    enum planned_row_scalar_conversion_kind conversion_kind
);
static int plan_row_scalar_conversion_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool row_scalar_conversion_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_session_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_concat_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_concat_operator_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int count_row_scalar_concat_operator_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    size_t *inout_argument_count
);
static int plan_row_scalar_concat_operator_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *arguments,
    size_t argument_count,
    size_t *inout_next_argument
);
static bool row_scalar_expression_is_concat_operator(const struct mylite_sql_ast_node *expression);
static int plan_row_scalar_concat_ws_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_concat_ws_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_concat_ws_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static bool concat_ws_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_row_scalar_field_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_field_expression_with_context(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    const char *column_unsupported_message,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_greatest_least_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_greatest_least_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum planned_row_scalar_field_domain *inout_domain
);
static int plan_row_scalar_greatest_least_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_greatest_least_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_greatest_least_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_greatest_least_decimal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_interval_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_interval_search_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_interval_threshold_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_interval_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_interval_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression,
    const char *unsupported_message
);
static int plan_row_scalar_date_format_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_get_format_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_time_format_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_str_to_date_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_str_to_date_null_short_circuit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_expression,
    const struct mylite_sql_ast_node *format_expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool *out_handled,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_str_to_date_resolve_short_circuit_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
);
static int plan_row_scalar_str_to_date_resolve_short_circuit_references(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
);
static int plan_row_scalar_date_format_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_format_input_kind *out_input_kind
);
static int plan_row_scalar_time_format_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_format_input_kind *out_input_kind
);
static int plan_row_scalar_date_format_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_format_input_kind *out_input_kind
);
static int plan_row_scalar_time_format_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_format_input_kind *out_input_kind
);
static int plan_row_scalar_date_format_format_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_get_format_format_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *literal_nul_message,
    int (*validate_format)(struct mylite_db *, const char *, size_t),
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_time_format_format_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_str_to_date_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_str_to_date_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_str_to_date_format_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_date_format_numeric_equal_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_date_interval_second_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_date_interval_second_temporal_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_interval_second_input_kind *out_input_kind
);
static int plan_row_scalar_date_interval_second_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_interval_second_input_kind *out_input_kind
);
static int plan_row_scalar_date_interval_second_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_date_interval_second_interval_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    enum mylite_date_interval_unit unit,
    struct planned_row_scalar_expression *out_expression
);
static int set_row_scalar_date_interval_second_unsupported_error(
    struct mylite_db *database,
    const char *function_name,
    const char *suffix
);
static int plan_row_scalar_datediff_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_period_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static const char *period_function_name(enum mylite_sql_ast_node_kind ast_kind);
static enum planned_row_scalar_expression_kind period_planned_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
static int plan_row_scalar_convert_tz_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_convert_tz_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    bool temporal_argument,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_convert_tz_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    bool temporal_argument,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_datediff_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_datediff_input_kind *out_input_kind
);
static int plan_row_scalar_datediff_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_datediff_input_kind *out_input_kind
);
static int plan_row_scalar_datediff_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_timediff_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_timediff_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_timediff_input_kind *out_input_kind
);
static int plan_row_scalar_timediff_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_timediff_input_kind *out_input_kind
);
static int plan_row_scalar_timediff_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_timestampdiff_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_timestampdiff_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_timestampdiff_input_kind *out_input_kind
);
static int plan_row_scalar_timestampdiff_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_timestampdiff_input_kind *out_input_kind
);
static int plan_row_scalar_timestampdiff_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_unix_timestamp_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_unix_timestamp_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_unix_timestamp_input_kind *out_input_kind
);
static int plan_row_scalar_unix_timestamp_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_unix_timestamp_input_kind *out_input_kind
);
static int plan_row_scalar_unix_timestamp_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_timestamp_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_timestamp_temporal_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_interval_second_input_kind *out_input_kind
);
static int plan_row_scalar_timestamp_temporal_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_interval_second_input_kind *out_input_kind
);
static int plan_row_scalar_timestamp_temporal_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_timestamp_time_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_timestamp_time_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_timestamp_time_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_temporal_extract_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_temporal_extract_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_temporal_extract_kind extract_kind,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_temporal_extract_input_kind *out_input_kind
);
static int plan_row_scalar_temporal_extract_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_temporal_extract_kind extract_kind,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_temporal_extract_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_temporal_extract_kind extract_kind,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_temporal_extract_input_kind *out_input_kind
);
static int reject_date_column_temporal_extract_time_part(
    struct mylite_db *database,
    enum mylite_temporal_extract_kind extract_kind
);
static int reject_time_column_temporal_extract_date_part(
    struct mylite_db *database,
    enum mylite_temporal_extract_kind extract_kind
);
static int reject_unsupported_temporal_extract_column(
    struct mylite_db *database,
    enum mylite_temporal_extract_kind extract_kind
);
static int plan_row_scalar_sec_to_time_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_sec_to_time_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_sec_to_time_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_from_unixtime_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_from_unixtime_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_from_unixtime_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_from_unixtime_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_temporal_constructor_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_temporal_constructor_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *function_name,
    struct planned_row_scalar_expression *out_expression
);
static int plan_row_scalar_temporal_constructor_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *function_name,
    struct planned_row_scalar_expression *out_expression
);
static size_t temporal_constructor_function_argument_count(enum mylite_sql_ast_node_kind ast_kind);
static const char *temporal_constructor_function_name(enum mylite_sql_ast_node_kind ast_kind);
static enum planned_row_scalar_expression_kind temporal_constructor_planned_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
static bool is_temporal_constructor_function_kind(enum mylite_sql_ast_node_kind ast_kind);
static bool row_scalar_date_format_equal_attempt(const struct mylite_sql_ast_node *expression);
static int plan_row_scalar_date_format_numeric_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
);
static bool planned_date_format_numeric_equal_format_is_supported(
    const struct planned_row_scalar_expression *expression
);
static int plan_row_scalar_field_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    const char *column_unsupported_message,
    struct planned_row_scalar_expression *out_expression,
    enum planned_row_scalar_field_domain *inout_domain
);
static int plan_row_scalar_field_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    const char *column_unsupported_message,
    struct planned_row_scalar_expression *out_expression
);
static enum planned_row_scalar_field_domain row_scalar_field_argument_domain(
    const struct planned_row_scalar_expression *expression
);
static enum planned_row_scalar_field_domain row_scalar_field_column_domain(
    const struct mylite_catalog_column_descriptor *column
);
static void planned_row_scalar_expression_deinit(struct planned_row_scalar_expression *expression);
static void planned_row_scalar_expression_deinit_next_leaf(
    struct planned_row_scalar_expression *expression
);
static bool row_scalar_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static bool row_scalar_expression_contains_row_function(
    const struct mylite_sql_ast_node *expression
);
static bool row_scalar_expression_contains_conversion_expression(
    const struct mylite_sql_ast_node *expression
);
static bool row_scalar_expression_contains_rand_function(
    const struct mylite_sql_ast_node *expression
);
static bool row_scalar_expression_contains_any_value_function(
    const struct mylite_sql_ast_node *expression
);
static bool row_scalar_expression_contains_date_interval_second_function(
    const struct mylite_sql_ast_node *expression
);
static bool row_scalar_expression_contains_control_flow_function(
    const struct mylite_sql_ast_node *expression
);
static bool row_scalar_expression_contains_statement_time_function(
    const struct mylite_sql_ast_node *expression
);
static bool row_scalar_expression_contains_integer_arithmetic_attempt(
    const struct mylite_sql_ast_node *expression
);
static bool integer_arithmetic_operator_is_attempt(enum mylite_sql_ast_operator operator_kind);
static bool integer_arithmetic_operator_is_supported(enum mylite_sql_ast_operator operator_kind);
static bool select_list_is_wildcard(const struct mylite_sql_ast_node *select_list);
static struct result_column_metadata_context result_column_metadata_context_init(void);
static void result_column_metadata_context_deinit(struct result_column_metadata_context *context);
static int load_result_column_metadata_context(
    struct mylite_db *database,
    const struct planned_select *plan,
    struct result_column_metadata_context *context
);
static int load_result_column_metadata_context_for_table(
    struct mylite_db *database,
    int64_t table_id,
    struct result_column_metadata_context *context
);
static struct mylite_result_column_descriptor unknown_result_column_descriptor(const char *label);
static int append_select_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_select *plan,
    const struct result_column_metadata_context *metadata_context,
    size_t column_index
);
static int make_select_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_select *plan,
    const struct result_column_metadata_context *metadata_context,
    size_t column_index,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
);
static int populate_select_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    const struct result_column_metadata_context *metadata_context,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_numeric_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
);
static int populate_string_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
);
static int populate_enum_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
);
static int populate_set_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
);
static int populate_json_result_column_descriptor(
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
);
static int populate_spatial_result_column_descriptor(
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
);
static int populate_binary_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
);
static int populate_temporal_result_column_descriptor(
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
);
static int populate_bit_result_column_descriptor(
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
);
static void apply_result_column_descriptor_flags(
    const struct mylite_catalog_column_descriptor *column,
    const struct result_column_metadata_context *metadata_context,
    struct mylite_result_column_descriptor *descriptor
);
static const char *column_effective_character_set_name(
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
);
static const char *column_effective_collation_name(
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
);
static bool column_effective_collation_is_binary(
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
);
static uint32_t result_metadata_collation_id(const char *collation_name);
static bool result_metadata_integer_descriptor(
    const char *logical_type,
    enum mylite_result_logical_type *out_type,
    uint64_t *out_display_length,
    bool *out_unsigned
);
static int copy_select_item_alias_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias,
    char **out_text
);
static int copy_select_item_identifier_alias_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias,
    char **out_text
);
static int validate_select_item_alias_text(struct mylite_db *database, char **text);
static int duplicate_text(struct mylite_db *database, const char *source, char **out_text);
static int append_show_tables_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const char *column_name,
    bool is_full
);
static int resolve_show_statement_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_node,
    struct show_schema_resolution *out_resolution
);
static int resolve_show_statement_selected_schema(
    struct mylite_db *database,
    struct show_schema_resolution *out_resolution
);
static int resolve_show_statement_explicit_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_node,
    struct show_schema_resolution *out_resolution
);
static const char *show_schema_resolution_name(const struct show_schema_resolution *resolution);
static int resolve_show_tables_filter_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct show_tables_filter_nodes *out_nodes
);
static int validate_show_tables_where_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const char *table_name_column,
    bool is_full
);
static int show_tables_where_clause_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const char *const values[show_tables_result_column_count],
    const char *table_name_column,
    bool is_full,
    bool *out_matches
);
static int evaluate_show_tables_where_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_tables_result_column_count],
    const char *table_name_column,
    bool is_full,
    enum show_variables_where_truth *out_truth
);
static int visit_show_tables_where_predicate(
    struct mylite_db *database,
    struct show_variables_where_frame_stack *frame_stack,
    const struct mylite_sql_ast_node *predicate
);
static int evaluate_show_tables_where_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_tables_result_column_count],
    const char *table_name_column,
    bool is_full,
    struct show_variables_where_truth_stack *truth_stack
);
static int evaluate_show_tables_where_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_tables_result_column_count],
    const char *table_name_column,
    bool is_full,
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_tables_where_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_tables_result_column_count],
    const char *table_name_column,
    bool is_full,
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_tables_where_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_tables_result_column_count],
    const char *table_name_column,
    bool is_full,
    enum show_variables_where_truth *out_truth
);
static int show_tables_where_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const char *const values[show_tables_result_column_count],
    const char *table_name_column,
    bool is_full,
    const char **out_value
);
static int resolve_show_tables_where_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const char *table_name_column,
    bool is_full,
    size_t *out_column_index
);
static int compare_show_tables_where_literal(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const char *left,
    const struct mylite_sql_ast_node *right,
    enum show_variables_where_truth *out_truth
);
static int show_tables_where_truth_from_comparison(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    struct show_table_status_where_comparison comparison,
    enum show_variables_where_truth *out_truth
);
static int decode_show_tables_where_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_text
);
static int append_show_table(const struct mylite_catalog_table_descriptor *table, void *user_data);
static int append_show_builtin_schema_tables(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    struct show_tables_context *context
);
static int append_show_builtin_table(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name,
    struct show_tables_context *context
);
static int append_show_table_status(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int append_show_builtin_schema_table_status(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    struct show_table_status_context *context
);
static int append_show_builtin_table_status(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name,
    struct show_table_status_context *context
);
static int format_builtin_table_status_timestamp(
    struct mylite_db *database,
    struct table_status_values *status
);
static int validate_show_table_status_where_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause
);
static int show_table_status_where_clause_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const char *const values[show_table_status_result_column_count],
    bool *out_matches
);
static int evaluate_show_table_status_where_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_table_status_result_column_count],
    enum show_variables_where_truth *out_truth
);
static int visit_show_table_status_where_predicate(
    struct mylite_db *database,
    struct show_variables_where_frame_stack *frame_stack,
    const struct mylite_sql_ast_node *predicate
);
static int evaluate_show_table_status_where_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_table_status_result_column_count],
    struct show_variables_where_truth_stack *truth_stack
);
static int evaluate_show_table_status_where_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_table_status_result_column_count],
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_table_status_where_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_table_status_result_column_count],
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_table_status_where_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_table_status_result_column_count],
    enum show_variables_where_truth *out_truth
);
static int show_table_status_where_operand_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const char *const values[show_table_status_result_column_count],
    struct show_table_status_where_operand *out_operand
);
static void show_table_status_where_operand_deinit(struct show_table_status_where_operand *operand);
static int show_table_status_where_substring_operand_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const char *const values[show_table_status_result_column_count],
    struct show_table_status_where_operand *out_operand
);
static int resolve_show_table_status_where_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    size_t *out_column_index
);
static int compare_show_table_status_where_literal(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const struct show_table_status_where_operand *left,
    const struct mylite_sql_ast_node *right,
    enum show_variables_where_truth *out_truth
);
static int decode_show_table_status_where_comparable_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    bool left_is_numeric,
    const char *unsupported_message,
    char **out_text
);
static int show_table_status_where_compare_text(
    struct mylite_db *database,
    const struct show_table_status_where_operand *left,
    const char *right,
    int *out_comparison
);
static int show_table_status_where_compare_numeric_text(
    struct mylite_db *database,
    const char *left,
    const char *right,
    int *out_comparison
);
static int show_table_status_where_texts_are_equal(
    struct mylite_db *database,
    const struct show_table_status_where_operand *left,
    const char *right,
    bool *out_equal
);
static int show_table_status_where_truth_from_comparison(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    struct show_table_status_where_comparison comparison,
    enum show_variables_where_truth *out_truth
);
static bool compare_show_table_status_where_null_operand(
    enum mylite_sql_ast_operator operator_kind,
    const char *left,
    enum mylite_sql_ast_literal_kind literal_kind,
    enum show_variables_where_truth *out_truth
);
static int decode_show_table_status_where_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_text
);
static int evaluate_show_metadata_where_regexp_predicate(
    struct mylite_db *database,
    const char *left,
    const struct mylite_sql_ast_node *right,
    const struct show_metadata_regexp_messages *messages,
    bool case_sensitive,
    enum show_variables_where_truth *out_truth
);
static int compare_show_table_status_where_text(
    const char *left,
    const char *right,
    bool case_sensitive
);
static bool parse_show_table_status_where_unsigned_text(const char *text, uint64_t *out_value);
static bool show_table_status_where_column_is_case_sensitive(size_t column_index);
static bool show_table_status_where_column_is_numeric(size_t column_index);
static int append_show_column(
    const struct mylite_catalog_column_descriptor *column,
    void *user_data
);
static int resolve_show_index_filter_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct show_index_filter_nodes *out_nodes
);
static int execute_show_index_mysql_system_target(
    struct mylite_db *database,
    const struct show_index_filter_nodes *nodes,
    mylite_result **out_result,
    bool *out_handled
);
static int resolve_show_index_mysql_system_table(
    struct mylite_db *database,
    struct show_columns_target_nodes nodes,
    const struct mylite_execution_catalog_mysql_system_table **out_definition,
    bool *out_mysql_system_target
);
static int execute_show_index_mysql_system_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    mylite_result **out_result
);
static int append_show_index_mysql_system_table_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *where_clause,
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static int append_show_index_mysql_system_table_primary_key_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *where_clause,
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static int append_show_index_mysql_system_table_secondary_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *where_clause,
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static int append_show_index_mysql_system_table_row_if_matches(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *where_clause,
    const char *const values[show_index_result_column_count]
);
static const char *mysql_system_table_primary_key_cardinality(
    const struct mylite_execution_catalog_mysql_system_table *definition,
    size_t sequence
);
static const char *mysql_system_table_primary_key_cardinality_for_name(
    const char *table_name,
    size_t sequence
);
static const char *mysql_system_table_primary_key_collation(
    const struct mylite_execution_catalog_mysql_system_table *definition,
    size_t sequence
);
static const char *mysql_system_table_primary_key_collation_for_name(
    const char *table_name,
    size_t sequence
);
static int validate_show_index_where_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause
);
static int show_index_where_clause_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const char *const values[show_index_result_column_count],
    bool *out_matches
);
static int evaluate_show_index_where_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_index_result_column_count],
    enum show_variables_where_truth *out_truth
);
static int visit_show_index_where_predicate(
    struct mylite_db *database,
    struct show_variables_where_frame_stack *frame_stack,
    const struct mylite_sql_ast_node *predicate
);
static int evaluate_show_index_where_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_index_result_column_count],
    struct show_variables_where_truth_stack *truth_stack
);
static int evaluate_show_index_where_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_index_result_column_count],
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_index_where_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_index_result_column_count],
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_index_where_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_index_result_column_count],
    enum show_variables_where_truth *out_truth
);
static int show_index_where_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const char *const values[show_index_result_column_count],
    const char **out_value,
    size_t *out_column_index
);
static int resolve_show_index_where_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    size_t *out_column_index
);
static int compare_show_index_where_literal(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const char *left,
    size_t column_index,
    const struct mylite_sql_ast_node *right,
    enum show_variables_where_truth *out_truth
);
static int show_index_where_compare_text(
    struct mylite_db *database,
    const char *left,
    const char *right,
    size_t column_index,
    int *out_comparison
);
static int show_index_where_compare_numeric_text(
    struct mylite_db *database,
    const char *left,
    const char *right,
    int *out_comparison
);
static int show_index_where_texts_are_equal(
    struct mylite_db *database,
    const char *left,
    const char *right,
    size_t column_index,
    bool *out_equal
);
static int show_index_where_truth_from_comparison(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    struct show_table_status_where_comparison comparison,
    enum show_variables_where_truth *out_truth
);
static bool compare_show_index_where_null_operand(
    enum mylite_sql_ast_operator operator_kind,
    const char *left,
    enum mylite_sql_ast_literal_kind literal_kind,
    enum show_variables_where_truth *out_truth
);
static int decode_show_index_where_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_text
);
static int decode_show_index_where_comparable_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    size_t column_index,
    char **out_text
);
static int compare_show_index_where_text(const char *left, const char *right);
static bool show_index_where_column_is_numeric(size_t column_index);
static int append_show_index(
    struct mylite_db *database,
    mylite_result *result,
    const struct table_name_resolution *target,
    const struct loaded_index_info *index,
    const struct mylite_sql_ast_node *where_clause
);
static const char *show_index_part_nullable_text(const struct loaded_index_part *part);
static const char *show_index_part_collation_text(
    const struct loaded_index_info *index,
    const struct loaded_index_part *part
);
static int show_index_part_prefix_value(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    const struct loaded_index_part *part,
    char *prefix_text,
    size_t prefix_text_size,
    const char **out_prefix_value
);
static const char *show_index_index_type_text(const struct loaded_index_info *index);
static int show_column_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char **out_type_text
);
static const char *show_column_collation_text(
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
);
static int collect_show_database(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_builtin_show_database_names(struct show_databases_context *context);
static int show_database_list_append(
    struct mylite_db *database,
    struct show_databases_context *context,
    const char *name
);
static int append_sorted_show_database_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct show_like_filter *filter,
    const struct mylite_sql_ast_node *where_clause,
    struct show_databases_context *context
);
static int compare_show_database_names(const void *left, const void *right);
static int validate_show_databases_where_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause
);
static int show_databases_where_clause_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const char *name,
    bool *out_matches
);
static int evaluate_show_databases_where_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *name,
    enum show_variables_where_truth *out_truth
);
static int visit_show_databases_where_predicate(
    struct mylite_db *database,
    struct show_variables_where_frame_stack *frame_stack,
    const struct mylite_sql_ast_node *predicate
);
static int evaluate_show_databases_where_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *name,
    struct show_variables_where_truth_stack *truth_stack
);
static int evaluate_show_databases_where_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *name,
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_databases_where_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *name,
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_databases_where_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *name,
    enum show_variables_where_truth *out_truth
);
static int show_databases_where_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const char *name,
    const char **out_value
);
static int compare_show_databases_where_literal(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const char *left,
    const struct mylite_sql_ast_node *right,
    enum show_variables_where_truth *out_truth
);
static int show_databases_where_truth_from_comparison(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    struct show_databases_where_comparison comparison,
    enum show_variables_where_truth *out_truth
);
static int decode_show_databases_where_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_text
);

static int build_physical_table_name(int64_t table_id, char *destination, size_t destination_size);
static int build_physical_view_name(int64_t table_id, char *destination, size_t destination_size);
static int build_physical_index_name(int64_t index_id, char *destination, size_t destination_size);
static int build_physical_check_constraint_name(
    int64_t check_constraint_id,
    char *destination,
    size_t destination_size
);
static int build_create_table_sql(
    const struct planned_create_table *plan,
    const char *physical_name,
    bool temporary,
    char **out_sql
);
static int build_create_table_definition_sql(
    const struct planned_create_table *plan,
    const char *physical_name,
    bool temporary,
    char **out_sql
);
static int append_create_table_definition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan,
    const char *physical_name,
    bool temporary
);
static int build_create_table_indexes_sql(
    const struct planned_create_table *plan,
    const char *physical_name,
    char **out_sql
);
static int append_create_table_columns_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan
);
static int append_create_table_check_constraints_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan
);
static int append_create_table_generated_column_constraints_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan
);
static int append_generated_column_range_check_constraint_sql(
    struct mylite_dynamic_string *string,
    const struct planned_column *column,
    size_t column_index
);
static int append_generated_column_range_check_constraint_name(
    struct mylite_dynamic_string *string,
    size_t column_index
);
static int append_create_table_primary_key_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan,
    const char *physical_name
);
static int append_create_table_secondary_indexes_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan,
    const char *physical_name
);
static int append_create_table_index_sql(
    struct mylite_dynamic_string *string,
    bool is_unique,
    const char *index_physical_name,
    const char *table_physical_name
);
static int append_planned_key_part_sql(
    struct mylite_dynamic_string *string,
    const struct planned_column *column
);
static int append_planned_secondary_key_part_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan,
    const struct planned_secondary_index_part *part
);
static int append_loaded_key_part_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *part,
    const char *qualifier
);
static int append_loaded_prefix_key_part_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *part,
    const char *qualifier
);
static int append_loaded_key_part_parameter_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *part,
    size_t parameter_index
);
static int append_string_key_collation_sql(struct mylite_dynamic_string *string);
static int append_select_projection_column_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select *plan,
    size_t column_index
);
static int append_create_table_index_sql_close(struct mylite_dynamic_string *string);
static int build_drop_table_sql(const char *physical_name, char **out_sql);
static int build_alter_table_add_column_sql(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan,
    char **out_sql
);
static int build_alter_table_add_primary_key_null_validation_sql(
    const struct planned_alter_table_add_primary_key *plan,
    char **out_sql
);
static int build_alter_table_add_primary_key_duplicate_validation_sql(
    const struct planned_alter_table_add_primary_key *plan,
    char **out_sql
);
static int build_alter_table_add_primary_key_string_validation_sql(
    const struct planned_alter_table_add_primary_key *plan,
    char **out_sql
);
static int build_alter_table_add_primary_key_index_sql(
    const struct planned_alter_table_add_primary_key *plan,
    const char *index_physical_name,
    char **out_sql
);
static int build_add_index_sql(
    const struct planned_alter_table_add_index *plan,
    const char *index_physical_name,
    char **out_sql
);
static int build_create_unique_index_string_validation_sql(
    const struct planned_alter_table_add_index *plan,
    char **out_sql
);
static int append_create_unique_index_string_validation_select_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_index *plan
);
static int append_create_unique_index_string_validation_where_sql(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_index *plan
);
static int build_create_unique_index_duplicate_validation_sql(
    const struct planned_alter_table_add_index *plan,
    char **out_sql
);
static int append_loaded_key_part_list_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *parts,
    size_t part_count
);
static int append_loaded_key_part_not_null_filter_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *parts,
    size_t part_count
);
static int build_drop_index_sql(const char *physical_name, char **out_sql);
static int append_alter_table_primary_key_expression_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_primary_key *plan
);
static int build_alter_table_auto_increment_max_sql(
    const struct planned_alter_table_auto_increment *plan,
    char **out_sql
);
static int append_alter_table_add_column_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
);
static int append_alter_table_add_column_explicit_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
);
static int append_alter_table_add_column_current_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
);
static int append_alter_table_add_column_character_expression_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
);
static int append_alter_table_add_column_enum_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
);
static int append_alter_table_add_column_bit_expression_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
);
static int append_alter_table_add_column_year_expression_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
);
static int append_alter_table_add_column_bit_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
);
static int append_alter_table_add_column_binary_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
);
static int append_alter_table_add_column_decimal_zero(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
);
static int append_alter_table_add_column_binary_zero(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
);
static int append_alter_table_add_column_bit_zero(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
);
static int append_sqlite_blob_default(
    struct mylite_dynamic_string *string,
    const char *bytes,
    size_t byte_count
);
static int append_quoted_sql_text(struct mylite_dynamic_string *string, const char *text);
static int append_mysql_quoted_text(struct mylite_dynamic_string *string, const char *text);
static int append_mysql_quoted_comment_text(struct mylite_dynamic_string *string, const char *text);
static int append_mysql_quoted_default_text(struct mylite_dynamic_string *string, const char *text);
static int build_alter_table_drop_column_sql(
    const struct planned_alter_table_drop_column *plan,
    char **out_sql
);
static int build_alter_table_rename_column_sql(
    const struct planned_alter_table_rename_column *plan,
    char **out_sql
);
static int build_modify_temporary_physical_name(
    const struct planned_alter_table_modify_column *plan,
    const struct mylite_catalog_mutation *mutation,
    char *destination,
    size_t destination_size
);
static int build_alter_table_modify_validation_sql(
    const struct planned_alter_table_modify_column *plan,
    char **out_sql
);
static int build_alter_table_modify_create_sql(
    const struct planned_alter_table_modify_column *plan,
    const char *temporary_physical_name,
    char **out_sql
);
static int build_alter_table_modify_copy_sql(
    const struct planned_alter_table_modify_column *plan,
    const char *temporary_physical_name,
    char **out_sql
);
static bool alter_table_modify_needs_binary_copy_materialization(
    const struct planned_alter_table_modify_column *plan
);
static int execute_physical_alter_table_modify_copy_rows(
    struct mylite_db *database,
    const struct planned_alter_table_modify_column *plan,
    const char *temporary_physical_name
);
static int build_alter_table_modify_select_sql(
    const struct planned_alter_table_modify_column *plan,
    char **out_sql
);
static int build_alter_table_modify_insert_sql(
    const struct planned_alter_table_modify_column *plan,
    const char *temporary_physical_name,
    char **out_sql
);
static int execute_alter_table_modify_copy_row(
    struct mylite_db *database,
    struct alter_table_modify_copy_statements *statements,
    const struct planned_alter_table_modify_column *plan,
    size_t row_number,
    struct planned_value *values
);
static int materialize_alter_table_modify_copy_row(
    struct mylite_db *database,
    sqlite3_stmt *select_statement,
    const struct planned_alter_table_modify_column *plan,
    size_t row_number,
    struct planned_value *values
);
static int bind_alter_table_modify_copy_row(
    sqlite3_stmt *insert_statement,
    const struct planned_alter_table_modify_column *plan,
    const struct planned_value *values
);
static void deinit_alter_table_modify_copy_row(struct planned_value *values, size_t column_count);
static int build_alter_table_modify_indexes_sql(
    const struct planned_alter_table_modify_column *plan,
    char **out_sql
);
static int append_alter_table_modify_index_sql(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_modify_column *plan,
    const struct loaded_index_info *index
);
static int build_alter_table_order_temporary_physical_name(
    const struct planned_alter_table_order_by *plan,
    uint64_t sqlite_schema_generation,
    char *destination,
    size_t destination_size
);
static int build_alter_table_order_create_sql(
    const struct planned_alter_table_order_by *plan,
    const char *temporary_physical_name,
    char **out_sql
);
static int build_alter_table_order_copy_sql(
    const struct planned_alter_table_order_by *plan,
    const char *temporary_physical_name,
    char **out_sql
);
static int append_alter_table_order_column_list(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_order_by *plan
);
static int append_alter_table_order_order_list(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_order_by *plan
);
static int build_alter_table_force_temporary_physical_name(
    const struct planned_alter_table_force *plan,
    uint64_t sqlite_schema_generation,
    char *destination,
    size_t destination_size
);
static int build_alter_table_force_create_sql(
    const struct planned_alter_table_force *plan,
    const char *temporary_physical_name,
    char **out_sql
);
static int build_alter_table_force_copy_sql(
    const struct planned_alter_table_force *plan,
    const char *temporary_physical_name,
    char **out_sql
);
static int append_alter_table_force_column_list(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_force *plan
);
static int build_alter_table_rename_physical_table_sql(
    const char *source_physical_name,
    const char *target_physical_name,
    char **out_sql
);
static int build_truncate_table_sql(const struct planned_truncate_table *plan, char **out_sql);
static int build_insert_sql(const struct planned_insert *plan, char **out_sql);
static int append_insert_column_names(
    struct mylite_dynamic_string *string,
    const struct planned_insert *plan
);
static size_t count_physical_insert_columns(const struct planned_insert *plan);
static int append_insert_parameters(struct mylite_dynamic_string *string, size_t column_count);
static int append_numbered_parameter(struct mylite_dynamic_string *string, size_t parameter_index);
static int append_size_literal(struct mylite_dynamic_string *string, size_t value);
static int append_uint64_literal(struct mylite_dynamic_string *string, uint64_t value);
static int build_insert_select_temp_table_name(
    const struct mylite_db *database,
    char *destination,
    size_t destination_size
);
static int build_insert_select_materialize_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
);
static int build_insert_select_table_materialize_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
);
static int build_insert_select_compound_materialize_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
);
static int append_insert_select_compound_branch_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select_compound_branch *branch,
    size_t branch_index,
    bool alias_columns,
    size_t *next_parameter
);
static int append_insert_select_compound_table_branch_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select_compound_branch *branch,
    size_t branch_index,
    bool alias_columns,
    size_t *next_parameter
);
static int append_insert_select_compound_branch_marker_sql(
    struct mylite_dynamic_string *string,
    size_t branch_index,
    bool alias_columns
);
static int append_insert_select_compound_column_projection_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select *source,
    size_t column_index
);
static int append_insert_select_compound_row_scalar_branch_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select_compound_branch *branch,
    size_t branch_index,
    bool alias_columns,
    size_t *next_parameter
);
static int append_insert_select_compound_row_scalar_projection_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static bool row_scalar_expression_uses_string_collation(
    const struct planned_row_scalar_expression *expression
);
static int build_insert_select_validation_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
);
static int build_insert_select_table_validation_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
);
static int build_insert_select_compound_validation_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
);
static bool insert_select_compound_last_distinct_branch(
    const struct planned_insert_select *plan,
    size_t *out_branch_index
);
static int append_insert_select_compound_distinct_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    size_t last_distinct_branch
);
static int append_insert_select_compound_distinct_equality_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select *plan,
    size_t column_index
);
static int append_insert_select_compound_qualified_temp_column_name(
    struct mylite_dynamic_string *string,
    const char *alias,
    size_t column_index
);
static int append_insert_select_compound_qualified_branch_column_name(
    struct mylite_dynamic_string *string,
    const char *alias
);
static bool insert_select_compound_output_uses_string_collation(
    const struct planned_insert_select *plan,
    size_t column_index
);
static int build_drop_temp_table_sql(const char *temporary_table_name, char **out_sql);
static int append_insert_select_source_projection(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select *plan
);
static int append_insert_select_temp_column_name(
    struct mylite_dynamic_string *string,
    size_t column_index
);
static int append_insert_select_temp_table_name(
    struct mylite_dynamic_string *string,
    const char *temporary_table_name
);
static bool find_insert_select_target_position(
    const struct planned_insert_select *plan,
    size_t column_index,
    size_t *out_target_position
);
static int build_create_table_select_insert_sql(
    const struct planned_create_table_select *plan,
    const char *physical_name,
    char **out_sql
);
static int append_create_table_select_target_column_names(
    struct mylite_dynamic_string *string,
    const struct planned_create_table_select *plan
);
static int append_create_table_select_source_projection(
    struct mylite_dynamic_string *string,
    const struct planned_create_table_select *plan
);
static int build_select_sql(const struct planned_select *plan, char **out_sql);
static int build_row_scalar_select_sql(
    const struct planned_row_scalar_select *plan,
    char **out_sql
);
static int build_count_having_select_sql(
    const struct planned_count_having_select *plan,
    char **out_sql
);
static int append_count_having_outer_select_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_count_having_select *plan
);
static int append_count_having_inner_select_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_count_having_select *plan,
    size_t *next_parameter
);
static int append_count_having_projection_alias(
    struct mylite_dynamic_string *string,
    size_t projection_index
);
static int append_count_having_sql(
    struct mylite_dynamic_string *string,
    const struct planned_count_having_select *plan,
    size_t *next_parameter
);
static int append_row_scalar_tableless_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_select *plan,
    size_t *next_parameter
);
static int append_row_scalar_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_column_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression
);
static int append_row_scalar_window_function_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_window_function_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *argument,
    size_t *next_parameter
);
static const char *window_function_sql_name(enum planned_window_function_kind kind);
static int append_row_scalar_rand_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_conversion_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_conversion_base_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static const char *row_scalar_conversion_sql_function_name(
    const struct planned_row_scalar_conversion_step *step
);
static int append_row_scalar_non_concat_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_integer_arithmetic_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_integer_arithmetic_frame_sql(
    struct mylite_dynamic_string *string,
    struct row_scalar_integer_arithmetic_append_stack *stack,
    const struct row_scalar_integer_arithmetic_append_frame *frame,
    size_t *next_parameter
);
static int append_row_scalar_integer_arithmetic_enter_sql(
    struct mylite_dynamic_string *string,
    struct row_scalar_integer_arithmetic_append_stack *stack,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_integer_arithmetic_function_call_sql(
    struct mylite_dynamic_string *string,
    struct row_scalar_integer_arithmetic_append_stack *stack,
    const struct planned_row_scalar_expression *expression
);
static const char *row_scalar_integer_arithmetic_function_name(
    const struct planned_row_scalar_expression *expression
);
static void row_scalar_integer_arithmetic_append_stack_deinit(
    struct row_scalar_integer_arithmetic_append_stack *stack
);
static int row_scalar_integer_arithmetic_append_stack_push(
    struct row_scalar_integer_arithmetic_append_stack *stack,
    struct row_scalar_integer_arithmetic_append_frame frame
);
static int append_row_scalar_concat_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_concat_ws_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_char_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_field_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_greatest_least_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_interval_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_interval_result_sql(
    struct mylite_dynamic_string *string,
    int64_t value
);
static int append_row_scalar_date_format_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_date_format_numeric_equal_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_date_format_numeric_operand_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_date_interval_second_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_datediff_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_timediff_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_timestampdiff_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_timestamp_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_field_operand_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter,
    bool use_string_collation
);
static int append_row_scalar_string_length_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_string_length_operand_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter,
    bool force_blob
);
static bool row_scalar_string_length_argument_is_binary(
    const struct planned_row_scalar_expression *expression
);
static int append_row_scalar_string_codepoint_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static const char *row_scalar_string_codepoint_sql_function(
    enum planned_string_codepoint_function_kind function_kind
);
static int append_row_scalar_string_case_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static const char *row_scalar_string_case_sql_function(
    enum planned_string_case_function_kind function_kind
);
static int append_row_scalar_string_trim_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static const char *row_scalar_string_trim_sql_function(
    enum planned_string_trim_function_kind function_kind
);
static int append_row_scalar_unix_timestamp_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_temporal_extract_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_sec_to_time_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_from_unixtime_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_temporal_constructor_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_string_slice_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_string_padding_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static const char *row_scalar_string_padding_sql_function(
    enum planned_string_padding_function_kind function_kind
);
static int append_row_scalar_string_bitmask_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static const char *row_scalar_string_bitmask_sql_function(
    enum planned_string_bitmask_function_kind function_kind
);
static int append_row_scalar_string_search_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_string_replace_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_string_insert_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_string_reverse_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_soundex_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_string_quote_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_substring_index_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_find_in_set_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_strcmp_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_regexp_like_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_valid_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_search_expression_sql(
    struct mylite_dynamic_string *string,
    const char *function_name,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_extract_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_value_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_unquote_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_quote_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_unquote_extract_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_extract_document_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_introspection_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_length_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_keys_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_type_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_set_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int json_mutation_sqlite_function_name(
    const struct planned_row_scalar_expression *expression,
    const char **out_function_name
);
static int append_row_scalar_json_remove_arguments_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_set_pair_arguments_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_set_document_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_set_value_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *argument,
    size_t *next_parameter
);
static int append_row_scalar_json_value_kind_sql(
    struct mylite_dynamic_string *string,
    enum mylite_json_sql_value_kind value_kind
);
static int append_row_scalar_json_constructor_expression_sql(
    struct mylite_dynamic_string *string,
    const char *function_name,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_constructor_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *argument,
    size_t *next_parameter
);
static int append_row_scalar_control_flow_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_searched_case_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_like_predicate_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_nested_control_flow_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_if_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_nested_if_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_ifnull_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_nested_ifnull_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_coalesce_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_nested_coalesce_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_nullif_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_nested_nullif_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_isnull_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_nested_isnull_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_control_flow_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_control_flow_leaf_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_control_flow_collated_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_control_flow_leaf_collated_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_left_right_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_substring_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_substring_null_guard_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *value,
    const struct planned_row_scalar_expression *position,
    const struct planned_row_scalar_expression *length,
    size_t *next_parameter
);
static int append_row_scalar_substring_nonpositive_length_guard_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *length,
    size_t *next_parameter
);
static int append_row_scalar_substring_negative_start_guard_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *value,
    const struct planned_row_scalar_expression *position,
    size_t *next_parameter
);
static int append_row_scalar_substring_call_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *value,
    const struct planned_row_scalar_expression *position,
    const struct planned_row_scalar_expression *length,
    size_t *next_parameter
);
static int append_row_scalar_hex_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_unhex_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_base64_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_registered_function_expression_sql(
    struct mylite_dynamic_string *string,
    const char *function_name,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_uuid_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_uuid_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *argument,
    size_t argument_index,
    size_t *next_parameter
);
static int append_row_scalar_uuid_inner_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_uuid_leaf_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *argument,
    size_t *next_parameter
);
static const char *row_scalar_uuid_sql_function_name(enum planned_row_scalar_expression_kind kind);
static int append_row_scalar_string_slice_operand_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter,
    bool negate
);
static int build_select_found_rows_sql(const struct planned_select *plan, char **out_sql);
static int build_count_sql(const struct planned_count *plan, char **out_sql);
static int build_column_aggregate_sql(const struct planned_column_aggregate *plan, char **out_sql);
static int append_column_aggregate_select_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_column_aggregate *plan,
    size_t *next_parameter
);
static int append_group_concat_call_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *value_column,
    size_t value_source_index,
    const struct planned_select_order *order,
    bool qualify_column_references,
    bool has_separator,
    size_t *next_parameter
);
static const char *column_aggregate_sql_function(enum planned_column_aggregate_function function);
static int build_grouped_aggregate_sql(
    const struct planned_grouped_aggregate *plan,
    char **out_sql
);
static int append_grouped_aggregate_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan
);
static int append_grouped_aggregate_select_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t *next_parameter
);
static int append_grouped_aggregate_select_item_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    const struct planned_grouped_aggregate_item *item,
    size_t *next_parameter
);
static int append_grouped_aggregate_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    const struct planned_grouped_aggregate_item *item
);
static const char *grouped_aggregate_sql_function(enum planned_grouped_aggregate_function function);
static int append_grouped_having_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t *next_parameter
);
static int append_grouped_having_operand_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan
);
static int append_grouped_having_aggregate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan
);
static int append_grouped_order_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan
);
static int append_select_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
);
static int append_select_predicate_sql_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
);
static int append_select_predicate_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
);
static int append_select_predicate_expression_sql_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
);
static int append_select_predicate_expression_work_item(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    struct predicate_sql_work_item item,
    struct predicate_sql_work_item **items,
    size_t *item_count,
    size_t *next_parameter
);
static int append_select_predicate_expression_work_item_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    struct predicate_sql_work_item item,
    struct predicate_sql_work_item **items,
    size_t *item_count,
    size_t *next_parameter
);
static int append_select_predicate_logical_operator_sql(
    struct mylite_dynamic_string *string,
    enum mylite_sql_ast_operator operator_kind
);
static int append_select_predicate_expression_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t node_index,
    size_t *next_parameter,
    struct predicate_sql_work_item **items,
    size_t *item_count
);
static int append_select_predicate_expression_node_sql_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t node_index,
    size_t *next_parameter,
    struct predicate_sql_work_item **items,
    size_t *item_count
);
static int append_select_predicate_logical_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    struct predicate_sql_work_item **items,
    size_t *item_count
);
static int append_select_predicate_not_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    struct predicate_sql_work_item **items,
    size_t *item_count
);
static int append_select_predicate_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);
static int append_select_predicate_node_sql_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);
static int append_select_predicate_non_exists_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);
static int append_select_predicate_non_subquery_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);
static int append_select_predicate_column_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);
static int append_select_predicate_column_term_sql_without_subqueries(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);
static int append_select_row_scalar_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);
static int append_select_exists_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_exists_subquery *subquery,
    size_t *next_parameter
);
static int append_exists_subquery_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_exists_subquery *subquery
);
static int append_descriptor_value_sql_for_source(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify
);
static int append_select_predicate_subject_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    bool qualify
);
static int append_date_midnight_value_sql_for_source(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify
);
static int append_descriptor_column_name_sql_for_source(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify
);
static int append_time_seconds_value_sql_for_source(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify
);
static int append_select_comparison_predicate_term_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);
static int append_select_like_predicate_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);
static int append_select_regexp_predicate_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);
static int append_select_is_null_predicate_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node
);
static int append_select_is_boolean_predicate_term_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node
);
static int append_is_boolean_rhs_term_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node,
    const char *suffix
);
static int append_select_between_predicate_term_sql(
    struct mylite_dynamic_string *string,
    size_t *next_parameter
);
static int append_select_in_predicate_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);
static int append_select_in_literal_predicate_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);
static int append_select_in_subquery_sql(
    struct mylite_dynamic_string *string,
    const struct planned_in_subquery *subquery,
    size_t *next_parameter
);
static int append_in_subquery_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_in_subquery *subquery
);
static int append_predicate_sql_work_node(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    size_t node_index
);
static int append_predicate_sql_work_operator(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    enum mylite_sql_ast_operator operator_kind
);
static int append_predicate_sql_work_close(
    struct predicate_sql_work_item **items,
    size_t *item_count
);
static int append_predicate_sql_work_text(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    const char *text
);
static int append_predicate_sql_work_item(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    struct predicate_sql_work_item item
);
static int append_select_order_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_order *order,
    size_t *next_parameter
);
static int append_select_order_legacy_column_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_order *order
);
static int append_select_order_item_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_order *order,
    size_t item_index,
    size_t *next_parameter
);
static int append_select_order_direction_sql(
    struct mylite_dynamic_string *string,
    enum planned_select_order_direction direction
);
static int append_select_source_alias(struct mylite_dynamic_string *string, size_t source_index);
static bool planned_select_qualifies_source_references(const struct planned_select *plan);
static int append_select_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select *plan,
    size_t *next_parameter
);
static int append_select_join_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_join_condition *condition,
    size_t *next_parameter
);
static int append_select_limit_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_limit *limit,
    size_t *next_parameter
);
static int build_delete_sql(const struct planned_delete *plan, char **out_sql);
static int append_delete_rowid_limited_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter
);
static int append_joined_delete_rowid_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter
);
static int append_joined_delete_rowid_subquery_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter
);
static int append_joined_delete_rowid_select_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter,
    size_t target_source_index
);
static int append_joined_delete_target_rowid_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t target_source_index
);
static int append_joined_delete_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter
);
static int build_update_sql(const struct planned_update *plan, char **out_sql);
static int append_single_update_target_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
);
static bool planned_update_needs_target_alias(const struct planned_update *plan);
static size_t update_assignment_parameter_count(const struct planned_update *plan);
static int append_update_assignment_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
);
static int append_update_date_interval_assignment_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
);
static int append_update_multiple_assignments_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
);
static int append_update_auto_update_columns_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
static bool planned_update_column_has_auto_update(
    const struct planned_update *plan,
    const struct mylite_catalog_column_descriptor *column
);
static size_t planned_update_auto_update_column_count(const struct planned_update *plan);
static int build_update_matched_sql(const struct planned_update *plan, char **out_sql);
static int append_single_update_matched_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
);
static int bind_update_matched_count_parameters(
    sqlite3_stmt *statement,
    const struct planned_update *plan
);
static int append_joined_update_matched_target_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
static bool planned_update_has_row_filter(const struct planned_update *plan);
static int append_update_row_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
static int append_update_rowid_limited_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
static int append_single_update_rowid_source_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
);
static int append_joined_update_rowid_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
static int append_joined_update_rowid_subquery_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
static int append_joined_update_target_rowid_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
);
static int append_joined_update_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
static int append_update_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
static int append_update_multiple_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
static int append_update_assignment_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    const struct planned_value *value,
    size_t *next_parameter
);
static int append_update_arithmetic_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
static int append_update_date_interval_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
static int append_update_date_interval_function_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t first_parameter
);
static int validate_child_foreign_keys_after_write(
    struct mylite_db *database,
    int64_t child_table_id
);
static int validate_parent_foreign_keys_after_write(
    struct mylite_db *database,
    int64_t parent_table_id
);
static int validate_parent_foreign_key_descriptor(
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    void *user_data
);
static int validate_foreign_key_references(
    struct mylite_db *database,
    const struct loaded_foreign_key_info *foreign_key,
    bool parent_write
);
static int set_row_is_referenced_foreign_key_error(
    struct mylite_db *database,
    const struct loaded_foreign_key_info *foreign_key
);
static int append_row_is_referenced_foreign_key_detail(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct loaded_foreign_key_info *foreign_key
);
static int append_row_is_referenced_foreign_key_columns(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key,
    bool child_columns
);
static int append_row_is_referenced_foreign_key_rules(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key
);
static int build_foreign_key_validation_sql(
    const struct loaded_foreign_key_info *foreign_key,
    char **out_sql
);
static int append_foreign_key_child_not_null_checks(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key
);
static int append_foreign_key_child_set_null_assignments_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key
);
static int append_foreign_key_parent_match_checks(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key
);
static int append_foreign_key_column_reference(
    struct mylite_dynamic_string *string,
    const char *table_alias,
    const char *column_name
);
static const char *comparison_operator_sql(enum mylite_sql_ast_operator operator_kind);
static int execute_sqlite_schema_sql(struct mylite_db *database, const char *sql);
static int execute_sqlite_control_sql(const struct mylite_db *database, const char *sql);
static int prepare_sqlite_statement(
    const struct mylite_db *database,
    const char *sql,
    sqlite3_stmt **out_statement
);
static int finalize_sqlite_statement(sqlite3_stmt *statement, int rc);
static int bind_insert_row(sqlite3_stmt *statement, const struct planned_insert *plan, size_t row);
static int handle_insert_unique_key_conflict(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index
);
static int handle_insert_duplicate_key_update(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *insert_statement,
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
);
static int handle_insert_duplicate_update_constraint(
    struct mylite_db *database,
    int sqlite_rc,
    const struct planned_insert *plan,
    size_t row_index
);
static int handle_replace_unique_key_conflict(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *insert_statement,
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
);
static int replace_conflicting_row_matches_plan(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    bool *out_matches
);
static int build_replace_conflicting_row_select_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    char **out_sql
);
static int compare_replace_conflicting_row(
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    bool *out_matches
);
static bool sqlite_column_matches_planned_value(
    sqlite3_stmt *statement,
    int column_index,
    const struct planned_value *value
);
static int delete_replace_conflicting_row(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    size_t row_index,
    sqlite3_int64 *out_deleted_rows
);
static int build_replace_conflicting_row_delete_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    char **out_sql
);
static int step_insert_row(sqlite3_stmt *statement, int *out_sqlite_rc);
static bool sqlite_status_is_constraint(int sqlite_rc);
static int reset_insert_statement_after_constraint(sqlite3_stmt *statement);
static int handle_check_constraint_violation(
    struct mylite_db *database,
    int64_t table_id,
    bool ignore_errors,
    bool *out_was_check_violation
);
static int handle_generated_not_null_violation(
    struct mylite_db *database,
    int sqlite_rc,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    bool *out_was_generated_not_null_violation
);
static int handle_generated_range_constraint_violation(
    struct mylite_db *database,
    struct generated_constraint_columns columns,
    size_t row_number,
    bool *out_was_generated_range_violation
);
static bool sqlite_error_message_has_check_constraint(
    const char *message,
    const char **out_physical_name
);
static bool sqlite_error_message_has_not_null_column(
    const char *message,
    const char **out_column_name,
    size_t *out_column_name_length
);
static bool generated_range_constraint_column_index(
    const char *physical_name,
    size_t *out_column_index
);
static int set_or_warn_check_constraint_violation(
    struct mylite_db *database,
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    bool ignore_errors
);
static int find_insert_unique_key_conflict(
    struct mylite_db *database,
    int sqlite_step_rc,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info **out_conflicting_index,
    char *value_text,
    size_t value_text_size
);
static int insert_unique_key_row_has_null_part(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *index,
    bool *out_has_null_part
);
static int apply_insert_duplicate_key_update(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    struct insert_execution_counters *counters
);
static int validate_insert_duplicate_key_assignment_conflicts(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    const struct planned_value *assignment_values
);
static int fetch_insert_duplicate_current_row_values(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    struct planned_value **out_values
);
static int build_insert_duplicate_current_row_select_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    char **out_sql
);
static int project_insert_duplicate_updated_row_values(
    struct mylite_db *database,
    const struct planned_insert *plan,
    struct insert_duplicate_projected_row_inputs inputs,
    struct planned_value **out_values
);
static bool insert_duplicate_index_has_assigned_part(
    const struct planned_insert *plan,
    const struct loaded_index_info *index
);
static int insert_duplicate_key_tuple_has_null_part(
    const struct planned_insert *plan,
    const struct loaded_index_info *index,
    const struct planned_value *values,
    bool *out_has_null_part
);
static int unique_key_tuple_exists_excluding_insert_duplicate_row(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *candidate,
    const struct loaded_index_info *conflicting_index,
    const struct planned_value *projected_values,
    bool *out_exists
);
static int build_unique_key_lookup_excluding_insert_duplicate_row_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *candidate,
    const struct loaded_index_info *conflicting_index,
    char **out_sql
);
static int append_insert_duplicate_key_predicate_sql_from_parameters(
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *index,
    size_t first_parameter
);
static int bind_insert_duplicate_key_tuple_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct loaded_index_info *index,
    const struct planned_value *values,
    size_t value_count
);
static int convert_insert_duplicate_update_value(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct planned_value *current_values,
    const struct planned_insert_duplicate_update_assignment *duplicate_assignment,
    struct planned_value *out_value
);
static int convert_insert_duplicate_arithmetic_value(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct planned_value *current_values,
    const struct planned_insert_duplicate_update_assignment *duplicate_assignment,
    struct planned_value *out_value
);
static int parse_insert_duplicate_arithmetic_delta(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_delta
);
static int assign_insert_duplicate_arithmetic_result(
    struct mylite_db *database,
    const struct insert_duplicate_arithmetic_result_request *request,
    struct planned_value *out_value
);
static void set_insert_duplicate_arithmetic_delta_out_of_range_error(struct mylite_db *database);
static void set_insert_duplicate_arithmetic_range_error(
    struct mylite_db *database,
    const struct insert_duplicate_arithmetic_range_error *error
);
static bool insert_duplicate_update_has_same_column_arithmetic(const struct planned_insert *plan);
static bool insert_duplicate_update_allows_null_adjustment(
    const struct mylite_db *database,
    const struct planned_insert *plan,
    const struct mylite_catalog_column_descriptor *column
);
static int convert_insert_duplicate_update_values(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    struct planned_value **out_values
);
static void insert_duplicate_update_values_deinit(
    struct planned_value **values,
    size_t value_count
);
static int copy_planned_value(
    struct mylite_db *database,
    const struct planned_value *source,
    struct planned_value *out_value
);
static int build_insert_duplicate_update_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    const struct planned_value *assignment_values,
    char **out_sql
);
static int append_insert_duplicate_key_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index
);
static int append_insert_duplicate_assignments_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert *plan
);
static int append_insert_duplicate_changed_conditions_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert *plan,
    const struct planned_value *assignment_values,
    size_t *next_parameter
);
static int append_insert_duplicate_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    const struct planned_value *assignment_value,
    size_t *next_parameter
);
static int bind_insert_duplicate_update_parameters(
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    const struct planned_value *assignment_values
);
static int bind_insert_duplicate_assignment_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_insert *plan,
    const struct planned_value *assignment_values
);
static int bind_insert_duplicate_changed_condition_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_insert *plan,
    const struct planned_value *assignment_values
);
static int handle_update_unique_key_conflict(
    struct mylite_db *database,
    const struct planned_update *executable_plan,
    const struct planned_update *plan,
    int sqlite_rc
);
static int unique_key_tuple_exists(
    struct mylite_db *database,
    const char *physical_table_name,
    const struct loaded_index_info *index,
    const struct planned_value *values,
    size_t value_count,
    bool *out_exists
);
static int build_unique_key_lookup_sql(
    const char *physical_table_name,
    const struct loaded_index_info *index,
    char **out_sql
);
static bool loaded_index_contains_column_id(
    const struct loaded_index_info *index,
    int64_t column_id
);
static int unique_key_single_value_exists(
    struct mylite_db *database,
    const char *physical_table_name,
    const struct loaded_index_info *index,
    const struct planned_value *value,
    bool *out_exists
);
static int find_update_unique_key_conflict_tuple(
    struct mylite_db *database,
    const struct planned_update *executable_plan,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    char *destination,
    size_t destination_size,
    bool *out_found
);
static int build_update_unique_key_conflict_sql(
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    char **out_sql
);
static int build_update_unique_key_internal_conflict_sql(
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    char **out_sql
);
static int append_update_unique_key_internal_conflict_alias_list(
    struct mylite_dynamic_string *string,
    size_t part_count
);
static int append_update_unique_key_internal_conflict_select_list(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    size_t *next_parameter
);
static int append_update_unique_key_internal_conflict_not_null_filter(
    struct mylite_dynamic_string *string,
    size_t part_count
);
static int append_update_unique_key_conflict_select_list(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    size_t *next_parameter
);
static int append_update_unique_key_conflict_candidate_source(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
);
static int append_update_unique_key_conflict_target_filter(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
static int append_update_unique_key_conflict_exists_clause(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    const char *conflict_alias,
    size_t *next_parameter
);
static int append_update_unique_key_conflict_expression(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_part *part,
    size_t *next_parameter
);
static int bind_update_unique_key_conflict_parameters(
    sqlite3_stmt *statement,
    const struct update_unique_key_conflict_bind_request *request
);
static int bind_update_unique_key_internal_conflict_parameters(
    sqlite3_stmt *statement,
    const struct update_unique_key_conflict_bind_request *request
);
static int bind_update_unique_key_assignment_parameters(
    sqlite3_stmt *statement,
    const struct update_unique_key_conflict_bind_request *request,
    int *parameter_index
);
static int format_sqlite_key_tuple(
    sqlite3_stmt *statement,
    const struct loaded_index_part *parts,
    size_t part_count,
    char *destination,
    size_t destination_size
);
static int format_sqlite_key_part(
    sqlite3_stmt *statement,
    const struct loaded_index_part *part,
    int column_index,
    char *destination,
    size_t destination_size
);
static int append_formatted_key_part(
    char *destination,
    size_t destination_size,
    size_t *used,
    size_t part_index,
    const char *value_text
);
static int bind_unique_key_tuple_parameters(
    sqlite3_stmt *statement,
    const struct loaded_index_info *index,
    const struct planned_value *values,
    size_t value_count
);
static int format_key_tuple(
    const struct loaded_index_info *index,
    const struct planned_value *values,
    size_t value_count,
    char *destination,
    size_t destination_size
);
static int format_key_part_value(
    const struct loaded_index_part *part,
    const struct planned_value *value,
    char *destination,
    size_t destination_size
);
static int format_blob_key_part_value(
    const struct loaded_index_part *part,
    const struct planned_value *value,
    bool trim_fixed_binary_padding,
    char *destination,
    size_t destination_size
);
static int format_text_prefix_key_part_value(
    const struct loaded_index_part *part,
    const struct planned_value *value,
    char *destination,
    size_t destination_size
);
static int index_part_prefix_length(
    const struct loaded_index_part *part,
    size_t *out_prefix_length
);
static size_t limited_copy_length(size_t value_length, size_t maximum_length);
static int format_key_value(
    const struct planned_value *value,
    char *destination,
    size_t destination_size
);
static bool loaded_index_part_uses_binary_key_display(const struct loaded_index_part *part);
static size_t binary_key_display_byte_count(
    const struct loaded_index_part *part,
    const void *bytes,
    size_t byte_count
);
static int format_binary_key_bytes(
    const void *bytes,
    size_t byte_count,
    char *destination,
    size_t destination_size
);
static int bind_select_parameters(sqlite3_stmt *statement, const struct planned_select *plan);
static int bind_select_parameters_at(
    sqlite3_stmt *statement,
    const struct planned_select *plan,
    int *parameter_index
);
static int bind_select_join_condition_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_join_condition *condition,
    int *parameter_index
);
static int bind_row_scalar_select_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_select *plan
);
static int bind_count_having_select_parameters(
    sqlite3_stmt *statement,
    const struct planned_count_having_select *plan
);
static int bind_row_scalar_select_parameters_at(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_select *plan,
    int *parameter_index
);
static int bind_select_order_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_order *order,
    int *parameter_index
);
static int bind_insert_select_materialize_parameters(
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan
);
static int bind_insert_select_compound_parameters(
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan
);
static int bind_row_scalar_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_non_concat_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_window_function_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_window_function_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *argument,
    int *parameter_index
);
static int bind_row_scalar_rand_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_conversion_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_conversion_base_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_integer_arithmetic_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_integer_arithmetic_frame_parameters(
    sqlite3_stmt *statement,
    struct row_scalar_integer_arithmetic_bind_stack *stack,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static void row_scalar_integer_arithmetic_bind_stack_deinit(
    struct row_scalar_integer_arithmetic_bind_stack *stack
);
static int row_scalar_integer_arithmetic_bind_stack_push(
    struct row_scalar_integer_arithmetic_bind_stack *stack,
    const struct planned_row_scalar_expression *expression
);
static int bind_row_scalar_field_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_reversed_arguments_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_interval_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_char_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_date_format_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_date_interval_second_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_datediff_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_timediff_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_timestampdiff_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_timestamp_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_length_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_codepoint_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_case_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_trim_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_unix_timestamp_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_temporal_extract_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_sec_to_time_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_from_unixtime_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_temporal_constructor_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_slice_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_padding_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_bitmask_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_search_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_replace_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_insert_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_reverse_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_soundex_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_quote_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_substring_index_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_find_in_set_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_strcmp_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_regexp_like_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_valid_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_search_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_extract_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_value_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_unquote_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_quote_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_extract_document_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_introspection_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_length_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_keys_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_type_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_set_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_set_document_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_set_value_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_constructor_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_control_flow_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_nested_control_flow_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_control_flow_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_control_flow_leaf_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_left_right_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_substring_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_hex_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_unhex_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_base64_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_registered_function_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_uuid_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_uuid_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *argument,
    size_t argument_index,
    int *parameter_index
);
static int bind_row_scalar_uuid_inner_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_uuid_leaf_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *argument,
    int *parameter_index
);
static int bind_count_parameters(sqlite3_stmt *statement, const struct planned_count *plan);
static int bind_column_aggregate_parameters(
    sqlite3_stmt *statement,
    const struct planned_column_aggregate *plan
);
static int bind_grouped_aggregate_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan
);
static int bind_delete_parameters(sqlite3_stmt *statement, const struct planned_delete *plan);
static int bind_update_parameters(sqlite3_stmt *statement, const struct planned_update *plan);
static int bind_update_multiple_assignment_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
);
static int bind_update_assignment_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct planned_update *plan
);
static int bind_update_date_interval_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
);
static int bind_update_auto_update_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
);
static int bind_update_multiple_changed_condition_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
);
static int bind_update_changed_condition_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct planned_update *plan
);
static int bind_update_matched_parameters(
    sqlite3_stmt *statement,
    const struct planned_update *plan
);
static int bind_planned_value_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct planned_value *value
);
static int bind_int64_parameter(sqlite3_stmt *statement, int parameter_index, int64_t value);
static int bind_text_parameter(sqlite3_stmt *statement, int parameter_index, const char *value);
static int append_selected_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    mylite_result *result,
    const struct mylite_catalog_column_descriptor *columns,
    size_t descriptor_count
);
static int append_selected_sqlite_row_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *columns,
    size_t descriptor_count,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
);
static int append_selected_sqlite_row_value_with_descriptor(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *column,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
);
static int append_selected_sqlite_integer_value(
    sqlite3_stmt *statement,
    size_t column_index,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
);
static int append_selected_sqlite_float_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *column,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
);
static int append_selected_sqlite_text_value(
    sqlite3_stmt *statement,
    size_t column_index,
    struct mylite_result_cell *out_value
);
static int append_selected_sqlite_blob_value(
    sqlite3_stmt *statement,
    size_t column_index,
    struct mylite_result_cell *out_value
);
static int choose_sqlite_rowid_alias(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *unsupported_message,
    const char **out_alias
);
static bool column_name_exists(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *name
);
static int make_show_like_filter(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *pattern_node,
    struct show_like_filter *out_filter
);
static void show_like_filter_deinit(struct show_like_filter *filter);
static bool show_like_filter_matches(
    const struct show_like_filter *filter,
    const char *value,
    bool case_sensitive
);
static int build_show_databases_column_name(const struct show_like_filter *filter, char **out_name);
static int build_show_tables_column_name(
    const char *schema_name,
    const struct show_like_filter *filter,
    char **out_name
);
static int read_show_table_status_row_count(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t *out_count
);
static int build_show_table_status_count_sql(
    const struct mylite_catalog_table_descriptor *table,
    char **out_sql
);
static int format_show_table_status_integer(
    struct mylite_db *database,
    int64_t value,
    char *buffer,
    size_t buffer_size
);
static int decode_show_like_pattern(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *pattern_node,
    char **out_pattern
);
static int append_decoded_string_escape(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    char escaped_byte
);
static bool show_like_pattern_matches(
    const char *pattern,
    size_t pattern_length,
    const char *value,
    size_t value_length,
    bool case_sensitive
);
static bool show_like_pattern_matches_with_escape(
    const char *pattern,
    size_t pattern_length,
    const char *value,
    size_t value_length,
    bool case_sensitive,
    bool backslash_escapes
);
static size_t show_like_skip_percent_run(
    const char *pattern,
    size_t pattern_length,
    size_t pattern_index
);
static bool show_like_pattern_item_matches(
    struct show_like_pattern_item_request request,
    size_t *out_next_pattern_index
);
static bool show_like_bytes_equal(char left, char right, bool case_sensitive);
static char show_like_ascii_lower(char byte);

static const struct mylite_sql_ast_node *child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
);
static const struct mylite_sql_ast_node *child_with_kind(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
);
static int script_statement_count(const struct mylite_sql_ast_node *root, size_t *out_count);
static void set_parse_error(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result
);
struct normalized_mysql_compat_sql {
    const char *sql;
    size_t sql_size;
    char *owned_sql;
};

static int normalize_mysql_compat_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    struct normalized_mysql_compat_sql *out_sql
);
static void normalized_mysql_compat_sql_deinit(struct normalized_mysql_compat_sql *sql);
static int extract_mysql_executable_comment_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    char **out_sql,
    size_t *out_sql_size,
    bool *out_changed
);
static int rewrite_set_user_variable_increment_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    char **out_sql,
    size_t *out_sql_size,
    bool *out_changed
);
static int quote_set_bare_compat_values_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    char **out_sql,
    size_t *out_sql_size,
    bool *out_changed
);
static bool trim_sql_span(
    const char *sql,
    size_t sql_size,
    size_t *out_start,
    size_t *out_end
);
static bool sql_span_equals_ascii_case_insensitive(
    const char *text,
    size_t text_size,
    const char *expected
);
static bool sql_byte_is_identifier(char byte);
static bool sql_byte_starts_identifier(char byte);
static void copy_folded_sql_identifier(
    const char *text,
    size_t text_size,
    char *buffer,
    size_t buffer_size
);
static bool set_target_needs_bare_value_quoting(const char *target_name);
static bool set_assignment_target_before_equal(
    const char *sql,
    size_t assignment_start,
    size_t equal_index,
    char *buffer,
    size_t buffer_size
);
static bool set_assignment_value_word(
    const char *sql,
    size_t sql_size,
    size_t value_start,
    size_t *out_value_end
);

static int normalize_mysql_compat_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    struct normalized_mysql_compat_sql *out_sql
) {
    const char *current_sql = sql;
    size_t current_size = sql_size;
    char *owned_sql = NULL;
    char *next_sql = NULL;
    size_t next_size = 0U;
    bool changed = false;
    int rc = MYLITE_OK;

    if (out_sql == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = (struct normalized_mysql_compat_sql){.sql = sql, .sql_size = sql_size};

    rc = extract_mysql_executable_comment_sql(
        database,
        current_sql,
        current_size,
        &next_sql,
        &next_size,
        &changed
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (changed) {
        owned_sql = next_sql;
        current_sql = owned_sql;
        current_size = next_size;
    }

    rc = rewrite_set_user_variable_increment_sql(
        database,
        current_sql,
        current_size,
        &next_sql,
        &next_size,
        &changed
    );
    if (rc != MYLITE_OK) {
        free(owned_sql);
        return rc;
    }
    if (changed) {
        free(owned_sql);
        owned_sql = next_sql;
        current_sql = owned_sql;
        current_size = next_size;
    }

    rc = quote_set_bare_compat_values_sql(
        database,
        current_sql,
        current_size,
        &next_sql,
        &next_size,
        &changed
    );
    if (rc != MYLITE_OK) {
        free(owned_sql);
        return rc;
    }
    if (changed) {
        free(owned_sql);
        owned_sql = next_sql;
        current_sql = owned_sql;
        current_size = next_size;
    }

    out_sql->sql = current_sql;
    out_sql->sql_size = current_size;
    out_sql->owned_sql = owned_sql;
    return MYLITE_OK;
}

static void normalized_mysql_compat_sql_deinit(struct normalized_mysql_compat_sql *sql) {
    if (sql == NULL) {
        return;
    }
    free(sql->owned_sql);
    *sql = (struct normalized_mysql_compat_sql){0};
}

static int extract_mysql_executable_comment_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    char **out_sql,
    size_t *out_sql_size,
    bool *out_changed
) {
    size_t start = 0U;
    size_t end = 0U;
    size_t body_start = 0U;
    size_t body_end = 0U;
    size_t close = 0U;
    size_t tail = 0U;
    char *copy = NULL;

    if (out_sql == NULL || out_sql_size == NULL || out_changed == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    *out_sql_size = 0U;
    *out_changed = false;
    if (!trim_sql_span(sql, sql_size, &start, &end) || end - start < 5U ||
        sql[start] != '/' || sql[start + 1U] != '*' || sql[start + 2U] != '!') {
        return MYLITE_OK;
    }

    close = start + 3U;
    while (close + 1U < end && !(sql[close] == '*' && sql[close + 1U] == '/')) {
        ++close;
    }
    if (close + 1U >= end) {
        return MYLITE_OK;
    }
    tail = close + 2U;
    while (tail < end && isspace((unsigned char)sql[tail])) {
        ++tail;
    }
    if (tail < end && sql[tail] == ';') {
        ++tail;
        while (tail < end && isspace((unsigned char)sql[tail])) {
            ++tail;
        }
    }
    if (tail != end) {
        return MYLITE_OK;
    }

    body_start = start + 3U;
    while (body_start < close && isdigit((unsigned char)sql[body_start])) {
        ++body_start;
    }
    while (body_start < close && isspace((unsigned char)sql[body_start])) {
        ++body_start;
    }
    body_end = close;
    while (body_end > body_start && isspace((unsigned char)sql[body_end - 1U])) {
        --body_end;
    }

    copy = (char *)malloc(body_end - body_start + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(copy, sql + body_start, body_end - body_start);
    copy[body_end - body_start] = '\0';
    *out_sql = copy;
    *out_sql_size = body_end - body_start;
    *out_changed = true;
    return MYLITE_OK;
}

static int rewrite_set_user_variable_increment_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    char **out_sql,
    size_t *out_sql_size,
    bool *out_changed
) {
    size_t start = 0U;
    size_t end = 0U;
    size_t index = 0U;
    size_t name_start = 0U;
    size_t name_end = 0U;
    size_t rhs_name_start = 0U;
    size_t rhs_name_end = 0U;
    size_t delta_start = 0U;
    size_t delta_end = 0U;
    char name[MYLITE_SESSION_USER_VARIABLE_NAME_CAPACITY];
    char rhs_name[MYLITE_SESSION_USER_VARIABLE_NAME_CAPACITY];
    struct mylite_session_user_variable *variable = NULL;
    intmax_t parsed_current_value = 0;
    int64_t current_value = 0;
    int64_t delta = 0;
    int64_t result_value = 0;
    uint64_t delta_magnitude = 0U;
    char *endptr = NULL;
    char result_text[64];
    int written = 0;

    if (out_sql == NULL || out_sql_size == NULL || out_changed == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    *out_sql_size = 0U;
    *out_changed = false;
    if (!trim_sql_span(sql, sql_size, &start, &end) || end - start < 3U ||
        !sql_span_equals_ascii_case_insensitive(sql + start, 3U, "SET")) {
        return MYLITE_OK;
    }
    index = start + 3U;
    while (index < end && isspace((unsigned char)sql[index])) {
        ++index;
    }
    if (index >= end || sql[index] != '@') {
        return MYLITE_OK;
    }
    name_start = index + 1U;
    index = name_start;
    while (index < end && sql_byte_is_identifier(sql[index])) {
        ++index;
    }
    name_end = index;
    if (name_end == name_start || name_end - name_start >= sizeof(name)) {
        return MYLITE_OK;
    }
    while (index < end && isspace((unsigned char)sql[index])) {
        ++index;
    }
    if (index >= end || sql[index] != '=') {
        return MYLITE_OK;
    }
    ++index;
    while (index < end && isspace((unsigned char)sql[index])) {
        ++index;
    }
    if (index >= end || sql[index] != '@') {
        return MYLITE_OK;
    }
    rhs_name_start = index + 1U;
    index = rhs_name_start;
    while (index < end && sql_byte_is_identifier(sql[index])) {
        ++index;
    }
    rhs_name_end = index;
    while (index < end && isspace((unsigned char)sql[index])) {
        ++index;
    }
    if (index >= end || sql[index] != '+') {
        return MYLITE_OK;
    }
    ++index;
    while (index < end && isspace((unsigned char)sql[index])) {
        ++index;
    }
    delta_start = index;
    while (index < end && isdigit((unsigned char)sql[index])) {
        ++index;
    }
    delta_end = index;
    while (index < end && isspace((unsigned char)sql[index])) {
        ++index;
    }
    if (index < end && sql[index] == ';') {
        ++index;
        while (index < end && isspace((unsigned char)sql[index])) {
            ++index;
        }
    }
    if (delta_end == delta_start || index != end) {
        return MYLITE_OK;
    }

    copy_folded_sql_identifier(sql + name_start, name_end - name_start, name, sizeof(name));
    copy_folded_sql_identifier(
        sql + rhs_name_start,
        rhs_name_end - rhs_name_start,
        rhs_name,
        sizeof(rhs_name)
    );
    if (strcmp(name, rhs_name) != 0) {
        return MYLITE_OK;
    }
    variable = find_session_user_variable(&database->session, name);
    if (variable == NULL || variable->is_null || variable->value == NULL) {
        return MYLITE_OK;
    }

    errno = 0;
    parsed_current_value = strtoimax(variable->value, &endptr, 10);
    if (errno != 0 || endptr == variable->value || *endptr != '\0') {
        return MYLITE_OK;
    }
    if (parsed_current_value < (intmax_t)INT64_MIN ||
        parsed_current_value > (intmax_t)INT64_MAX) {
        return MYLITE_OK;
    }
    current_value = (int64_t)parsed_current_value;
    for (size_t digit = delta_start; digit < delta_end; ++digit) {
        uint64_t next_digit = (uint64_t)(sql[digit] - '0');

        if (delta_magnitude > (UINT64_MAX - next_digit) / 10U) {
            return MYLITE_OK;
        }
        delta_magnitude = delta_magnitude * 10U + next_digit;
    }
    if (delta_magnitude > (uint64_t)INT64_MAX) {
        return MYLITE_OK;
    }
    delta = (int64_t)delta_magnitude;
    if ((delta > 0 && current_value > INT64_MAX - delta) ||
        (delta < 0 && current_value < INT64_MIN - delta)) {
        return MYLITE_OK;
    }
    result_value = current_value + delta;
    written = snprintf(
        result_text,
        sizeof(result_text),
        "SET @%.*s = %" PRId64,
        (int)(name_end - name_start),
        sql + name_start,
        result_value
    );
    if (written < 0 || (size_t)written >= sizeof(result_text)) {
        set_runtime_error(database, "failed to rewrite SET user variable assignment");
        return MYLITE_ERROR;
    }
    *out_sql = (char *)malloc((size_t)written + 1U);
    if (*out_sql == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(*out_sql, result_text, (size_t)written + 1U);
    *out_sql_size = (size_t)written;
    *out_changed = true;
    return MYLITE_OK;
}

static int quote_set_bare_compat_values_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    char **out_sql,
    size_t *out_sql_size,
    bool *out_changed
) {
    struct mylite_dynamic_string rewritten;
    size_t start = 0U;
    size_t end = 0U;
    size_t cursor = 0U;
    size_t scan = 0U;
    bool changed = false;

    if (out_sql == NULL || out_sql_size == NULL || out_changed == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    *out_sql_size = 0U;
    *out_changed = false;
    if (!trim_sql_span(sql, sql_size, &start, &end) || end - start < 3U ||
        !sql_span_equals_ascii_case_insensitive(sql + start, 3U, "SET")) {
        return MYLITE_OK;
    }

    mylite_dynamic_string_init(&rewritten);
    scan = start + 3U;
    while (scan < sql_size) {
        char target_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        size_t equal_index = scan;
        size_t value_start = 0U;
        size_t value_end = 0U;

        while (equal_index < sql_size && sql[equal_index] != '=') {
            ++equal_index;
        }
        if (equal_index >= sql_size) {
            break;
        }
        if (!set_assignment_target_before_equal(
                sql,
                scan,
                equal_index,
                target_name,
                sizeof(target_name)
            ) ||
            !set_target_needs_bare_value_quoting(target_name)) {
            scan = equal_index + 1U;
            continue;
        }

        value_start = equal_index + 1U;
        while (value_start < sql_size && isspace((unsigned char)sql[value_start])) {
            ++value_start;
        }
        if (!set_assignment_value_word(sql, sql_size, value_start, &value_end) ||
            sql_span_equals_ascii_case_insensitive(
                sql + value_start,
                value_end - value_start,
                "DEFAULT"
            )) {
            scan = equal_index + 1U;
            continue;
        }

        if (mylite_dynamic_string_append_bytes(&rewritten, sql + cursor, value_start - cursor) !=
            MYLITE_OK ||
            mylite_dynamic_string_append_char(&rewritten, '\'') != MYLITE_OK ||
            mylite_dynamic_string_append_bytes(
                &rewritten,
                sql + value_start,
                value_end - value_start
            ) != MYLITE_OK ||
            mylite_dynamic_string_append_char(&rewritten, '\'') != MYLITE_OK) {
            mylite_dynamic_string_deinit(&rewritten);
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        cursor = value_end;
        scan = value_end;
        changed = true;
    }

    if (!changed) {
        mylite_dynamic_string_deinit(&rewritten);
        return MYLITE_OK;
    }
    if (mylite_dynamic_string_append_bytes(&rewritten, sql + cursor, sql_size - cursor) !=
        MYLITE_OK) {
        mylite_dynamic_string_deinit(&rewritten);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    *out_sql = mylite_dynamic_string_take(&rewritten);
    if (*out_sql == NULL) {
        mylite_dynamic_string_deinit(&rewritten);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    *out_sql_size = strlen(*out_sql);
    *out_changed = true;
    mylite_dynamic_string_deinit(&rewritten);
    return MYLITE_OK;
}

static bool trim_sql_span(
    const char *sql,
    size_t sql_size,
    size_t *out_start,
    size_t *out_end
) {
    size_t start = 0U;
    size_t end = sql_size;

    if (sql == NULL || out_start == NULL || out_end == NULL) {
        return false;
    }
    while (start < end && isspace((unsigned char)sql[start])) {
        ++start;
    }
    while (end > start && isspace((unsigned char)sql[end - 1U])) {
        --end;
    }
    *out_start = start;
    *out_end = end;
    return start < end;
}

static bool sql_span_equals_ascii_case_insensitive(
    const char *text,
    size_t text_size,
    const char *expected
) {
    size_t expected_size = expected == NULL ? 0U : strlen(expected);

    if (text == NULL || expected == NULL || text_size != expected_size) {
        return false;
    }
    for (size_t index = 0U; index < text_size; ++index) {
        if (ascii_lower((unsigned char)text[index]) !=
            ascii_lower((unsigned char)expected[index])) {
            return false;
        }
    }
    return true;
}

static bool sql_byte_is_identifier(char byte) {
    return isalnum((unsigned char)byte) || byte == '_' || byte == '$';
}

static bool sql_byte_starts_identifier(char byte) {
    return isalpha((unsigned char)byte) || byte == '_' || byte == '$';
}

static void copy_folded_sql_identifier(
    const char *text,
    size_t text_size,
    char *buffer,
    size_t buffer_size
) {
    size_t copied = 0U;

    if (buffer == NULL || buffer_size == 0U) {
        return;
    }
    copied = text_size < buffer_size ? text_size : buffer_size - 1U;
    for (size_t index = 0U; index < copied; ++index) {
        buffer[index] = ascii_lower((unsigned char)text[index]);
    }
    buffer[copied] = '\0';
}

static bool set_target_needs_bare_value_quoting(const char *target_name) {
    static const char *const targets[] = {
        "character_set_client",
        "character_set_connection",
        "character_set_results",
        "collation_connection",
        "default_collation_for_utf8mb4",
        "default_storage_engine",
        "default_tmp_storage_engine",
        "resultset_metadata",
        "session_track_gtids",
        "session_track_transaction_info",
        "use_secondary_engine",
    };

    if (target_name == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(targets) / sizeof(targets[0]); ++index) {
        if (strcmp(target_name, targets[index]) == 0) {
            return true;
        }
    }
    return false;
}

static bool set_assignment_target_before_equal(
    const char *sql,
    size_t assignment_start,
    size_t equal_index,
    char *buffer,
    size_t buffer_size
) {
    size_t name_end = equal_index;
    size_t name_start = 0U;

    if (sql == NULL || buffer == NULL || buffer_size == 0U || assignment_start >= equal_index) {
        return false;
    }
    buffer[0] = '\0';
    while (name_end > assignment_start && isspace((unsigned char)sql[name_end - 1U])) {
        --name_end;
    }
    name_start = name_end;
    while (name_start > assignment_start && sql_byte_is_identifier(sql[name_start - 1U])) {
        --name_start;
    }
    if (name_start == name_end || name_end - name_start >= buffer_size) {
        return false;
    }
    copy_folded_sql_identifier(sql + name_start, name_end - name_start, buffer, buffer_size);
    return true;
}

static bool set_assignment_value_word(
    const char *sql,
    size_t sql_size,
    size_t value_start,
    size_t *out_value_end
) {
    size_t index = value_start;

    if (sql == NULL || out_value_end == NULL || index >= sql_size ||
        !sql_byte_starts_identifier(sql[index])) {
        return false;
    }
    ++index;
    while (index < sql_size && sql_byte_is_identifier(sql[index])) {
        ++index;
    }
    *out_value_end = index;
    return true;
}

int mylite_execute(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_result **out_result
) {
    struct mylite_statement_context context;
    struct mylite_sql_parse_result parse_result;
    struct normalized_mysql_compat_sql normalized_sql;
    const struct mylite_sql_ast_node *statement = NULL;
    int64_t completed_row_count = -1;
    size_t statement_count = 0U;
    bool preserve_diagnostics_snapshot = false;
    bool completed_statement_is_select = false;
    int rc = MYLITE_OK;

    if (out_result == NULL) {
        if (database != NULL) {
            mylite_diagnostics_set_error(
                mylite_connection_diagnostics(database),
                MYLITE_MISUSE,
                "HY000",
                mylite_diagnostics_misuse_message()
            );
        }
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    if (database == NULL || sql == NULL) {
        if (database != NULL) {
            mylite_diagnostics_set_error(
                mylite_connection_diagnostics(database),
                MYLITE_MISUSE,
                "HY000",
                mylite_diagnostics_misuse_message()
            );
        }
        return MYLITE_MISUSE;
    }

    normalized_sql = (struct normalized_mysql_compat_sql){0};
    rc = normalize_mysql_compat_sql(database, sql, sql_size, &normalized_sql);
    if (rc != MYLITE_OK) {
        return rc;
    }

    mylite_statement_context_init(&context);
    rc = mylite_statement_context_begin(
        &context,
        database,
        normalized_sql.sql,
        normalized_sql.sql_size
    );
    if (rc != MYLITE_OK) {
        normalized_mysql_compat_sql_deinit(&normalized_sql);
        mylite_statement_context_deinit(&context);
        return rc;
    }
    mylite_statement_context_set_previous_row_count(&context, database->session.previous_row_count);
    mylite_statement_context_set_previous_found_rows(&context, database->session.found_rows);

    rc = status_from_parse_status(mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = normalized_sql.sql,
            .length = normalized_sql.sql_size,
            .modes = lexer_modes_for_session_sql_mode(&database->session),
        },
        &parse_result
    ));
    if (rc != MYLITE_OK) {
        rc = finish_parse_failure(database, &parse_result, rc);
        mylite_sql_parse_result_deinit(&parse_result);
        (void)mylite_statement_context_end(&context, rc);
        mylite_statement_context_deinit(&context);
        normalized_mysql_compat_sql_deinit(&normalized_sql);
        return rc;
    }

    rc = script_statement_count(parse_result.root, &statement_count);
    if (rc == MYLITE_OK && statement_count == 0U) {
        rc = execute_empty_statement(database, out_result);
    } else if (rc == MYLITE_OK && statement_count == 1U) {
        statement = child_at(parse_result.root, 0U);
        rc = execute_parsed_statement(database, &context, statement, out_result);
    } else if (rc == MYLITE_OK) {
        set_unsupported_error(database, "multiple statements are not supported");
        rc = MYLITE_ERROR;
    }

    if (rc == MYLITE_OK) {
        completed_row_count = row_count_for_completed_statement(statement, *out_result);
        preserve_diagnostics_snapshot = statement_preserves_diagnostics_snapshot(statement);
        completed_statement_is_select = statement_result_is_select(statement, *out_result);
    }
    mylite_sql_parse_result_deinit(&parse_result);
    if (rc != MYLITE_OK) {
        rc = finish_failed_statement(database, rc, out_result);
    } else {
        rc = finish_completed_statement(
            database,
            completed_statement_is_select,
            completed_row_count,
            preserve_diagnostics_snapshot,
            out_result
        );
    }
    (void)mylite_statement_context_end(&context, rc);
    mylite_statement_context_deinit(&context);
    normalized_mysql_compat_sql_deinit(&normalized_sql);

    return rc;
}

static int finish_parse_failure(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result,
    int parse_rc
) {
    int rc = parse_rc;
    int snapshot_rc = MYLITE_OK;

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else {
        set_parse_error(database, parse_result);
    }
    database->session.previous_row_count = -1;

    snapshot_rc = snapshot_current_diagnostics(database);
    return snapshot_rc == MYLITE_OK ? rc : snapshot_rc;
}

const struct mylite_sql_ast_node *mylite_execution_child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
) {
    return child_at(node, index);
}

const struct mylite_sql_ast_node *mylite_execution_unwrap_parenthesized_expression(
    const struct mylite_sql_ast_node *expression
) {
    return unwrap_parenthesized_expression(expression);
}

int mylite_execution_parse_unsigned_integer_literal(
    const struct mylite_sql_source_span *span,
    uint64_t *out_value
) {
    return parse_unsigned_integer_literal(span, out_value);
}

bool mylite_execution_is_scalar_arithmetic_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    return is_scalar_arithmetic_projection_expression(expression);
}

bool mylite_execution_is_scalar_bitwise_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    return is_scalar_bitwise_projection_expression(expression);
}

int mylite_execution_evaluate_scalar_arithmetic_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
) {
    return evaluate_scalar_arithmetic_expression(database, expression, out_value);
}

int mylite_execution_evaluate_scalar_bitwise_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
) {
    return evaluate_scalar_bitwise_expression(database, expression, out_value);
}

int mylite_execution_evaluate_bit_count_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
) {
    return evaluate_bit_count_operand(database, expression, out_value);
}

int mylite_execution_scalar_hex_numeric_runtime_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value,
    bool *out_handled
) {
    enum mylite_execution_system_variable_kind variable = MYLITE_EXECUTION_SYSTEM_VARIABLE_NONE;
    int64_t timestamp = 0;
    int rc = MYLITE_OK;

    if (out_value == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_bitwise_value){.is_null = false, .integer = 0U};
    *out_handled = true;
    if (expression == NULL) {
        *out_handled = false;
        return MYLITE_OK;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
        out_value->integer = database->session.connection_id;
        return MYLITE_OK;
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
        out_value->integer = (uint64_t)database->session.previous_row_count;
        return MYLITE_OK;
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION:
        out_value->integer = database->session.found_rows;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
        out_value->integer = database->session.last_insert_id;
        return MYLITE_OK;
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        rc = resolve_session_system_variable(database, expression, &variable);
        if (rc != MYLITE_OK) {
            return rc;
        }
        break;
    default:
        *out_handled = false;
        return MYLITE_OK;
    }

    switch (variable) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_INCREMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_OFFSET:
        out_value->integer = auto_increment_step_system_variable_value(
            database,
            variable,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WAIT_TIMEOUT:
        out_value->integer = timeout_system_variable_value(
            database,
            variable,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN:
        if (system_variable_expression_has_global_scope(expression)) {
            out_value->integer = group_concat_max_len_default_value;
        } else {
            out_value->integer = database->session.group_concat_max_len;
        }
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INFORMATION_SCHEMA_STATS_EXPIRY:
        if (system_variable_expression_has_global_scope(expression)) {
            out_value->integer = information_schema_stats_expiry_default_value;
        } else {
            out_value->integer = database->session.information_schema_stats_expiry;
        }
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS:
        out_value->integer = foreign_key_checks_system_variable_uint64_value(
            database,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BIG_TABLES:
        out_value->integer = big_tables_system_variable_uint64_value(
            database,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOCOMMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_QUOTE_SHOW_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UNIQUE_CHECKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_NOTES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BIG_SELECTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLICIT_DEFAULTS_FOR_TIMESTAMP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_BIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN:
        out_value->integer = 1U;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_WARNINGS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BUFFER_RESULT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_GENERATE_INVISIBLE_PRIMARY_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_OFF:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REPLICA_SKIP_COUNTER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REQUIRE_PRIMARY_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER:
        out_value->integer = 0U;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID:
        out_value->integer = server_id_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID_BITS:
        out_value->integer = server_id_bits_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PORT:
        out_value->integer = port_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_VERSION:
        out_value->integer = protocol_version_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SELECT_LIMIT:
        out_value->integer = sql_select_limit_system_variable_value(
            database,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TIMESTAMP:
        timestamp = current_timestamp_epoch(database);
        out_value->integer = (uint64_t)timestamp;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WARNING_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ERROR_COUNT:
        return diagnostics_count_system_variable_value(
            system_variable_count_diagnostics(database),
            variable,
            &out_value->integer
        );
    default:
        break;
    }

    *out_handled = false;
    return MYLITE_OK;
}

int mylite_execution_accumulate_staged_division_by_zero_warnings(
    struct mylite_db *database,
    size_t staged_count,
    size_t *inout_warning_count
) {
    return accumulate_staged_division_by_zero_warnings(database, staged_count, inout_warning_count);
}

int mylite_execution_accumulate_staged_warning_count(
    struct mylite_db *database,
    size_t staged_count,
    size_t *inout_warning_count
) {
    return accumulate_staged_warning_count(database, staged_count, inout_warning_count);
}

int mylite_execution_append_division_by_zero_warnings(
    struct mylite_db *database,
    size_t warning_count
) {
    return append_division_by_zero_warnings(database, warning_count);
}

int mylite_execution_decode_sql_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    const char *unsupported_message,
    const char *nul_message,
    char **out_text,
    size_t *out_text_length
) {
    return decode_sql_string_literal(
        database,
        literal_node,
        unsupported_message,
        nul_message,
        out_text,
        out_text_length
    );
}

int mylite_execution_current_timestamp_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    return current_timestamp_scalar_value(database, out_cell);
}

int mylite_execution_sysdate_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    return sysdate_scalar_value(database, out_cell);
}

int mylite_execution_current_date_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    return current_date_scalar_value(database, out_cell);
}

int mylite_execution_current_time_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    return current_time_scalar_value(database, out_cell);
}

int mylite_execution_utc_date_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    return utc_date_scalar_value(database, out_cell);
}

int mylite_execution_utc_time_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    return utc_time_scalar_value(database, out_cell);
}

int mylite_execution_utc_timestamp_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    return utc_timestamp_scalar_value(database, out_cell);
}

int mylite_execution_system_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return system_variable_value(database, expression, out_cell);
}

int mylite_execution_decode_sql_string_literal_with_policy(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    const char *unsupported_message,
    const char *nul_message,
    bool allow_nul,
    char **out_text,
    size_t *out_text_length
) {
    return decode_sql_string_literal_with_policy(
        database,
        literal_node,
        unsupported_message,
        nul_message,
        allow_nul,
        out_text,
        out_text_length
    );
}

int mylite_execution_decode_binary_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_bytes,
    size_t *out_byte_count
) {
    return decode_binary_hex_literal(database, literal_node, out_bytes, out_byte_count);
}

int mylite_execution_copy_source_span_text(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_text
) {
    return copy_source_span_text(database, span, out_text);
}

int mylite_execution_copy_identifier_text(
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size,
    struct mylite_db *database
) {
    return copy_identifier_text(node, destination, destination_size, database);
}

int mylite_execution_normalize_decimal_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    bool is_negative,
    char *buffer,
    size_t buffer_size
) {
    return normalize_decimal_integer_literal(database, span, is_negative, buffer, buffer_size);
}

int mylite_execution_format_uint64(
    struct mylite_db *database,
    uint64_t value,
    char *buffer,
    size_t buffer_size
) {
    return format_uint64(database, value, buffer, buffer_size);
}

int mylite_execution_duplicate_text(
    struct mylite_db *database,
    const char *source,
    char **out_text
) {
    return duplicate_text(database, source, out_text);
}

int mylite_execution_cast_binary_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return cast_binary_value(database, expression, out_cell);
}

int mylite_execution_convert_binary_type_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return convert_binary_type_value(database, expression, out_cell);
}

int mylite_execution_convert_using_binary_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return convert_using_binary_value(database, expression, out_cell);
}

int mylite_execution_convert_using_charset_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return convert_using_charset_value(database, expression, out_cell);
}

int mylite_execution_collate_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return collate_expression_value(database, expression, out_cell);
}

int mylite_execution_scalar_convert_charset_info_for_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_convert_charset_info *out_info
) {
    return scalar_convert_charset_info_for_expression(database, expression, out_info);
}

int mylite_execution_rand_seed_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    uint32_t *out_seed
) {
    return rand_seed_value(database, expression, out_seed);
}

int64_t mylite_execution_current_timestamp_epoch(const struct mylite_db *database) {
    return current_timestamp_epoch(database);
}

int mylite_execution_date_add_set_unknown_identifier_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_source_span *span = expression == NULL ? NULL : &expression->span;
    const char *source = NULL;
    size_t source_size = 0U;
    size_t destination_index = 0U;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

    if (span == NULL || span->text == NULL || span->length == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    source = span->text;
    source_size = span->length;
    if (source[0] != '`' && source[0] != '"') {
        if (source_size >= sizeof(column_name)) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        memcpy(column_name, source, source_size);
        column_name[source_size] = '\0';
        set_unknown_column_error(database, column_name);
        return MYLITE_ERROR;
    }
    if (source_size < 2U || source[source_size - 1U] != source[0]) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    for (size_t source_index = 1U; source_index + 1U < source_size; ++source_index) {
        if (destination_index + 1U >= sizeof(column_name)) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        if (source[source_index] == source[0] && source[source_index + 1U] == source[0]) {
            column_name[destination_index] = source[0];
            ++source_index;
        } else {
            column_name[destination_index] = source[source_index];
        }
        ++destination_index;
    }
    if (destination_index == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    column_name[destination_index] = '\0';
    set_unknown_column_error(database, column_name);
    return MYLITE_ERROR;
}

size_t mylite_execution_temporal_constructor_function_argument_count(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return temporal_constructor_function_argument_count(ast_kind);
}

const char *mylite_execution_temporal_constructor_function_name(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return temporal_constructor_function_name(ast_kind);
}

bool mylite_execution_is_temporal_constructor_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return is_temporal_constructor_function_kind(ast_kind);
}

int mylite_execution_copy_identifier_name_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_node,
    char *destination,
    size_t destination_size,
    const char *identifier_kind,
    const char *nul_message
) {
    return copy_table_option_name_text(
        database,
        name_node,
        destination,
        destination_size,
        (struct table_option_name_policy){
            .identifier_kind = identifier_kind,
            .nul_message = nul_message,
        }
    );
}

const char *mylite_execution_national_character_set_name(void) {
    return national_character_set_name;
}

const char *mylite_execution_national_collation_name(void) {
    return national_collation_name;
}

void mylite_execution_set_parse_error(struct mylite_db *database) {
    set_parse_error(database, NULL);
}

void mylite_execution_set_unsupported_error(struct mylite_db *database, const char *message) {
    set_unsupported_error(database, message);
}

void mylite_execution_set_native_function_parameter_count_error(
    struct mylite_db *database,
    const char *function_name
) {
    set_native_function_parameter_count_error(database, function_name);
}

void mylite_execution_set_scalar_division_unsupported_error(struct mylite_db *database) {
    set_scalar_division_unsupported_error(database);
}

void mylite_execution_set_abs_signed_minimum_overflow_error(struct mylite_db *database) {
    set_abs_signed_minimum_overflow_error(database);
}

void mylite_execution_set_abs_unsupported_error(struct mylite_db *database) {
    set_abs_unsupported_error(database);
}

void mylite_execution_set_sign_unsupported_error(struct mylite_db *database) {
    set_sign_unsupported_error(database);
}

void mylite_execution_set_rounding_unsupported_error(struct mylite_db *database) {
    set_rounding_unsupported_error(database);
}

int mylite_execution_set_rounding_signed_overflow_error(struct mylite_db *database) {
    return set_rounding_signed_overflow_error(database);
}

void mylite_execution_set_sqrt_unsupported_error(struct mylite_db *database) {
    set_sqrt_unsupported_error(database);
}

void mylite_execution_set_angle_conversion_unsupported_error(struct mylite_db *database) {
    set_angle_conversion_unsupported_error(database);
}

void mylite_execution_set_inverse_trig_unsupported_error(struct mylite_db *database) {
    set_inverse_trig_unsupported_error(database);
}

void mylite_execution_set_direct_trig_unsupported_error(struct mylite_db *database) {
    set_direct_trig_unsupported_error(database);
}

void mylite_execution_set_atan_unsupported_error(struct mylite_db *database) {
    set_atan_unsupported_error(database);
}

void mylite_execution_set_exp_log_power_unsupported_error(struct mylite_db *database) {
    set_exp_log_power_unsupported_error(database);
}

void mylite_execution_set_format_unsupported_error(struct mylite_db *database) {
    set_format_unsupported_error(database);
}

void mylite_execution_set_truncate_unsupported_error(struct mylite_db *database) {
    set_truncate_unsupported_error(database);
}

void mylite_execution_set_base_conversion_unsupported_error(struct mylite_db *database) {
    set_base_conversion_unsupported_error(database);
}

void mylite_execution_set_bit_count_unsupported_error(struct mylite_db *database) {
    set_bit_count_unsupported_error(database);
}

void mylite_execution_set_crc32_unsupported_error(struct mylite_db *database) {
    set_crc32_unsupported_error(database);
}

void mylite_execution_set_hex_unsupported_error(struct mylite_db *database) {
    set_hex_unsupported_error(database);
}

void mylite_execution_set_invalid_json_function_text_error(
    struct mylite_db *database,
    size_t position
) {
    set_invalid_json_function_text_error(database, position);
}

int mylite_execution_append_invalid_json_value_warning(
    struct mylite_db *database,
    const struct mylite_json_normalize_result *result
) {
    return append_invalid_json_value_warning(database, result);
}

void mylite_execution_set_invalid_json_path_error(struct mylite_db *database, size_t position) {
    set_invalid_json_path_error(database, position);
}

void mylite_execution_set_json_path_not_allowed_error(struct mylite_db *database) {
    set_json_path_not_allowed_error(database);
}

void mylite_execution_set_invalid_json_data_type_error(
    struct mylite_db *database,
    const char *function_name
) {
    set_invalid_json_data_type_error(database, function_name);
}

void mylite_execution_set_invalid_json_one_or_all_error(struct mylite_db *database) {
    set_invalid_json_one_or_all_error(database);
}

void mylite_execution_set_json_unquote_incorrect_type_error(struct mylite_db *database) {
    set_json_unquote_incorrect_type_error(database);
}

void mylite_execution_set_json_quote_incorrect_type_error(struct mylite_db *database) {
    set_json_quote_incorrect_type_error(database);
}

void mylite_execution_set_json_binary_charset_error(struct mylite_db *database) {
    set_json_binary_charset_error(database);
}

void mylite_execution_set_json_null_member_name_error(struct mylite_db *database) {
    set_json_null_member_name_error(database);
}

bool mylite_execution_text_equals_ascii_case_insensitive(const char *left, const char *right) {
    return text_equals_ascii_case_insensitive(left, right);
}

bool mylite_execution_text_value_is_supported_string_key(const char *text, size_t text_length) {
    return text_value_is_supported_string_key(text, text_length);
}

const char *mylite_execution_scalar_pi_text(void) {
    return scalar_pi_text;
}

int mylite_execution_scalar_rand_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return rand_function_value(database, expression, out_cell);
}

int mylite_execution_literal_projection_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return literal_projection_value(database, expression, out_cell);
}

int mylite_execution_format_session_scalar_uint64_value(
    struct mylite_db *database,
    uint64_t value,
    struct session_scalar_cell *out_cell
) {
    return format_session_scalar_uint64_value(database, value, out_cell);
}

int mylite_execution_validate_utf8_text(
    const char *text,
    size_t text_length,
    size_t *out_character_count
) {
    return validate_utf8_text(text, text_length, out_character_count);
}

int mylite_execution_utf8_sequence_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
) {
    return utf8_sequence_width(text, text_length, index, out_width);
}

bool mylite_execution_is_session_scalar_expression(const struct mylite_sql_ast_node *expression) {
    return is_session_scalar_expression(expression);
}

int mylite_execution_set_unknown_column_reference_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = collect_column_reference_parts(database, expression, parts, &part_count);

    if (rc == MYLITE_OK) {
        rc = format_column_reference_name(
            database,
            parts,
            part_count,
            column_name,
            sizeof(column_name)
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    set_unknown_column_error(database, column_name);
    return MYLITE_ERROR;
}

void mylite_execution_set_illegal_mix_of_collations_error(
    struct mylite_db *database,
    const char *first_collation,
    const char *second_collation,
    const char *operation
) {
    set_illegal_mix_of_collations_error(database, first_collation, second_collation, operation);
}

void mylite_execution_set_unknown_collation_error(
    struct mylite_db *database,
    const char *collation_name
) {
    set_unknown_collation_error(database, collation_name);
}

void mylite_execution_set_collation_not_valid_for_charset_error(
    struct mylite_db *database,
    const char *collation_name,
    const char *charset_name
) {
    set_collation_not_valid_for_charset_error(database, collation_name, charset_name);
}

void mylite_execution_set_nomem_error(struct mylite_db *database) {
    set_nomem_error(database);
}

void mylite_execution_set_runtime_error(struct mylite_db *database, const char *message) {
    set_runtime_error(database, message);
}

void mylite_execution_set_regexp_illegal_argument_error(struct mylite_db *database) {
    set_regexp_illegal_argument_error(database);
}

void mylite_execution_set_regexp_error(struct mylite_db *database, const char *message) {
    set_regexp_error(database, message);
}

void mylite_execution_set_regexp_character_range_error(
    struct mylite_db *database,
    const char *message
) {
    set_regexp_character_range_error(database, message);
}

void mylite_execution_session_scalar_cell_deinit(struct session_scalar_cell *cell) {
    session_scalar_cell_deinit(cell);
}

#include "mylite_execution_statement_entry.inc"

#include "mylite_execution_statement_session_handlers.inc"

#include "mylite_execution_prepared_statement_execution.inc"

#include "mylite_execution_transaction_characteristics.inc"

#include "mylite_execution_statement_transaction_boundaries.inc"

#include "mylite_execution_transaction_statements.inc"

#include "mylite_execution_lock_tables.inc"

#include "mylite_execution_statement_implicit_commits.inc"

#include "mylite_execution_session_savepoints.inc"

#include "mylite_execution_statement_sqlite_transactions.inc"

#include "mylite_execution_set_connection_charset.inc"

#include "mylite_execution_set_assignments.inc"

#include "mylite_execution_prepared_statement_support.inc"

#include "mylite_execution_set_session_snapshot.inc"

#include "mylite_execution_set_system_variable_dispatch.inc"

#include "mylite_execution_set_boolean_variables.inc"

#include "mylite_execution_set_numeric_transaction_variables.inc"

#include "mylite_execution_set_limit_size_expiry_variables.inc"

#include "mylite_execution_set_timeout_variables.inc"

#include "mylite_execution_set_sql_mode_timestamp_time_zone.inc"

#include "mylite_execution_ddl_create_table_statements.inc"

#include "mylite_execution_ddl_create_view_statements.inc"

#include "mylite_execution_ddl_create_schema_index_statements.inc"

#include "mylite_execution_ddl_drop_existence_statements.inc"

#include "mylite_execution_ddl_table_action_statements.inc"

#include "mylite_execution_ddl_alter_table_index_constraint_statements.inc"

#include "mylite_execution_ddl_alter_table_column_statements.inc"

#include "mylite_execution_ddl_alter_table_schema_option_statements.inc"

#include "mylite_execution_ddl_alter_table_maintenance_statements.inc"

#include "mylite_execution_dml_statements.inc"

#include "mylite_execution_metadata_queries.inc"

#include "mylite_execution_information_schema_join_compat.inc"

#include "mylite_execution_mysql_system_query_dispatch.inc"

#include "mylite_execution_mysql_system_sys_auto_increment_rows.inc"

#include "mylite_execution_mysql_system_sys_statistics_rows.inc"

#include "mylite_execution_mysql_system_sys_table_index_health_rows.inc"

#include "mylite_execution_mysql_system_sys_object_overview_rows.inc"

#include "mylite_execution_mysql_system_innodb_stats_rows.inc"

#include "mylite_execution_information_schema_query_execution.inc"

#include "mylite_execution_information_schema_system_dispatch_rows.inc"

#include "mylite_execution_information_schema_catalog_dispatch_rows.inc"

#include "mylite_execution_information_schema_row_helpers.inc"

#include "mylite_execution_information_schema_static_core_rows.inc"

#include "mylite_execution_information_schema_static_storage_rows.inc"

#include "mylite_execution_information_schema_builtin_table_status_helpers.inc"

#include "mylite_execution_information_schema_base_table_status_rows.inc"

#include "mylite_execution_information_schema_columns_system_rows.inc"

#include "mylite_execution_information_schema_columns_base_rows.inc"

#include "mylite_execution_information_schema_innodb_virtual_rows.inc"

#include "mylite_execution_information_schema_innodb_column_rows.inc"

#include "mylite_execution_information_schema_innodb_table_rows.inc"

#include "mylite_execution_information_schema_innodb_index_rows.inc"

#include "mylite_execution_information_schema_innodb_foreign_rows.inc"

#include "mylite_execution_information_schema_constraint_rows.inc"

#include "mylite_execution_information_schema_key_constraint_rows.inc"

#include "mylite_execution_information_schema_statistics_rows.inc"

#include "mylite_execution_information_schema_result_rows.inc"

#include "mylite_execution_information_schema_predicate_validation.inc"

#include "mylite_execution_information_schema_predicate_evaluation.inc"

#include "mylite_execution_information_schema_predicate_comparison.inc"

#include "mylite_execution_information_schema_predicate_values.inc"

#include "mylite_execution_information_schema_query_planning.inc"

#include "mylite_execution_information_schema_compare_format_helpers.inc"

#include "mylite_execution_information_schema_descriptor_metadata.inc"

#include "mylite_execution_table_maintenance_queries.inc"

#include "mylite_execution_show_tables_status_general.inc"

#include "mylite_execution_show_charset_variables_status.inc"

#include "mylite_execution_show_variables_where_eval.inc"

#include "mylite_execution_show_schema_objects_processlist_privileges.inc"

#include "mylite_execution_show_replication_metadata.inc"

#include "mylite_execution_show_diagnostics_output.inc"

#include "mylite_execution_show_columns_indexes.inc"

#include "mylite_execution_show_create.inc"

#include "mylite_execution_result_completion.inc"

#include "mylite_execution_create_table_planning_core.inc"

#include "mylite_execution_create_table_column_default_charset.inc"

#include "mylite_execution_create_table_item_validation.inc"

#include "mylite_execution_primary_key_definition_planning.inc"

#include "mylite_execution_create_table_secondary_index_planning.inc"

#include "mylite_execution_create_table_foreign_key_planning.inc"

#include "mylite_execution_create_table_check_constraint_planning.inc"

#include "mylite_execution_create_table_generated_expression_rendering.inc"

#include "mylite_execution_check_expression_rendering.inc"

#include "mylite_execution_create_table_constraints.inc"

#include "mylite_execution_create_table_variants.inc"

#include "mylite_execution_table_options_planning.inc"

#include "mylite_execution_create_table_execution.inc"

#include "mylite_execution_schema_table_admin.inc"

#include "mylite_execution_alter_table_add_column.inc"

#include "mylite_execution_alter_table_add_index.inc"

#include "mylite_execution_alter_table_foreign_key_index.inc"

#include "mylite_execution_alter_table_check_constraints.inc"

#include "mylite_execution_alter_table_drop_rename_column.inc"

#include "mylite_execution_alter_table_modify_column_entry.inc"

#include "mylite_execution_alter_table_default_visibility_options.inc"

#include "mylite_execution_alter_table_charset_conversion_options.inc"

#include "mylite_execution_alter_table_table_option_actions.inc"

#include "mylite_execution_alter_table_modify_column_resolution.inc"

#include "mylite_execution_alter_table_modify_column_execution.inc"

#include "mylite_execution_alter_table_check_rebuild_sql.inc"

#include "mylite_execution_alter_table_rename_check_constraints.inc"

#include "mylite_execution_load_data_planning.inc"

#include "mylite_execution_dml_planning.inc"

#include "mylite_execution_insert_execution.inc"

#include "mylite_execution_insert_select_planning.inc"

#include "mylite_execution_insert_select_table_execution.inc"

#include "mylite_execution_insert_select_row_scalar_execution.inc"

#include "mylite_execution_insert_select_table_rows.inc"

#include "mylite_execution_insert_select_value_materialization.inc"

#include "mylite_execution_insert_select_validation_core.inc"

#include "mylite_execution_insert_select_string_validation.inc"

#include "mylite_execution_insert_select_type_validation.inc"

#include "mylite_execution_update_planning.inc"

#include "mylite_execution_update_execution.inc"

#include "mylite_execution_select_planning_core.inc"

#include "mylite_execution_grouped_aggregate_entry.inc"

#include "mylite_execution_grouped_aggregate_source_planning.inc"

#include "mylite_execution_grouped_aggregate_group_columns.inc"

#include "mylite_execution_grouped_aggregate_projection_columns.inc"

#include "mylite_execution_grouped_aggregate_function_planning.inc"

#include "mylite_execution_grouped_aggregate_having_planning.inc"

#include "mylite_execution_grouped_aggregate_literal_conversion.inc"

#include "mylite_execution_grouped_aggregate_order_planning.inc"

#include "mylite_execution_select_execution.inc"

#include "mylite_execution_aggregate_execution.inc"

#include "mylite_execution_scalar_projection_classification.inc"

#include "mylite_execution_values_statement.inc"

#include "mylite_execution_scalar_projection_select_execution.inc"

#include "mylite_execution_scalar_result_metadata.inc"

#include "mylite_execution_session_scalar_result_helpers.inc"

#include "mylite_execution_session_scalar_warnings.inc"

#include "mylite_execution_scalar_projection_argument_diagnostics.inc"

#include "mylite_execution_scalar.inc"

#include "mylite_execution_scalar_string_core.inc"

#include "mylite_execution_scalar_temporal_core.inc"

#include "mylite_execution_scalar_string_extended.inc"

#include "mylite_execution_scalar_misc.inc"

#include "mylite_execution_scalar_conversion.inc"

#include "mylite_execution_scalar_temporal_format.inc"

#include "mylite_execution_scalar_bitwise_eval.inc"

#include "mylite_execution_scalar_logical_eval.inc"

#include "mylite_execution_scalar_comparison_eval.inc"

#include "mylite_execution_scalar_arithmetic_eval.inc"

#include "mylite_execution_scalar_diagnostic_helpers.inc"

#include "mylite_execution_scalar_control_case_entry.inc"

#include "mylite_execution_scalar_control_if_eval.inc"

#include "mylite_execution_scalar_literal_projection.inc"

#include "mylite_execution_scalar_system_variables.inc"

#include "mylite_execution_scalar_control_validation.inc"

#include "mylite_execution_scalar_projection.inc"

#include "mylite_execution_delete_planning.inc"

#include "mylite_execution_column_plan_entry.inc"

#include "mylite_execution_column_default_finalization.inc"

#include "mylite_execution_column_default_text.inc"

#include "mylite_execution_column_default_integer_eval.inc"

#include "mylite_execution_column_type_mapping.inc"

#include "mylite_execution_column_type_predicates.inc"

#include "mylite_execution_column_descriptor_parsing.inc"

#include "mylite_execution_column_row_size_validation.inc"

#include "mylite_execution_column_key_modify_validation.inc"

#include "mylite_execution_descriptor_helpers.inc"

#include "mylite_execution_insert_row_planning.inc"

#include "mylite_execution_insert_value_conversion.inc"

#include "mylite_execution_dml_default_values.inc"

#include "mylite_execution_dml_integer_conversion.inc"

#include "mylite_execution_dml_enum_set_conversion.inc"

#include "mylite_execution_dml_string_binary_conversion.inc"

#include "mylite_execution_dml_decimal_approx_conversion.inc"

#include "mylite_execution_dml_temporal_defaults.inc"

#include "mylite_execution_dml_value_helpers.inc"

#include "mylite_execution_dml_string_validation.inc"

#include "mylite_execution_dml_implicit_values.inc"

#include "mylite_execution_row_scalar_select_items.inc"

#include "mylite_execution_query_planning.inc"

#include "mylite_execution_row_scalar_string_basic_planning.inc"

#include "mylite_execution_row_scalar_string_shape_planning.inc"

#include "mylite_execution_row_scalar_string_bitmask_search_planning.inc"

#include "mylite_execution_row_scalar_string_edit_planning.inc"

#include "mylite_execution_row_scalar_string_transform_planning.inc"

#include "mylite_execution_row_scalar_string_compare_set_planning.inc"

#include "mylite_execution_row_scalar_string_regexp_planning.inc"

#include "mylite_execution_row_scalar_json_planning.inc"

#include "mylite_execution_row_scalar_binary_value_planning.inc"

#include "mylite_execution_row_scalar_char_charset_planning.inc"

#include "mylite_execution_row_scalar_control_flow_planning.inc"

#include "mylite_execution_row_scalar_conversion_value_planning.inc"

#include "mylite_execution_row_scalar_concat_planning.inc"

#include "mylite_execution_row_scalar_temporal_format_planning.inc"

#include "mylite_execution_row_scalar_temporal_interval_extract_planning.inc"

#include "mylite_execution_row_scalar_temporal_conversion_planning.inc"

#include "mylite_execution_row_scalar_temporal_period_timezone_weight_planning.inc"

#include "mylite_execution_row_scalar_temporal_diff_planning.inc"

#include "mylite_execution_row_scalar_temporal_timestamp_planning.inc"

#include "mylite_execution_row_scalar_misc_planning.inc"

#include "mylite_execution_select_column_planning.inc"

#include "mylite_execution_select_predicate_entry.inc"

#include "mylite_execution_select_predicate_leaf_comparison.inc"

#include "mylite_execution_select_predicate_temporal_extract.inc"

#include "mylite_execution_select_predicate_string_functions.inc"

#include "mylite_execution_select_predicate_json_regexp_functions.inc"

#include "mylite_execution_select_predicate_subquery_correlation.inc"

#include "mylite_execution_select_predicate_special_in.inc"

#include "mylite_execution_select_predicate_work_helpers.inc"

#include "mylite_execution_select_predicate_value_conversion.inc"

#include "mylite_execution_select_predicate_temporal_literals.inc"

#include "mylite_execution_select_order_planning.inc"

#include "mylite_execution_update_planning_helpers.inc"

#include "mylite_execution_show_tables_helpers.inc"

#include "mylite_execution_show_table_status_rows_helpers.inc"

#include "mylite_execution_show_table_status_where_helpers.inc"

#include "mylite_execution_show_columns_helpers.inc"

#include "mylite_execution_show_index_rows_helpers.inc"

#include "mylite_execution_show_index_where_helpers.inc"

#include "mylite_execution_show_column_display_helpers.inc"

#include "mylite_execution_show_databases_helpers.inc"

#include "mylite_execution_show_filter_helpers.inc"

#include "mylite_execution_show_result_name_helpers.inc"

#include "mylite_execution_show_table_status_count_helpers.inc"

#include "mylite_execution_show_like_pattern_helpers.inc"

#include "mylite_execution_sql_builder_create_table_index_helpers.inc"

#include "mylite_execution_sql_builder_drop_alter_add_column_index.inc"

#include "mylite_execution_sql_builder_alter_column_defaults.inc"

#include "mylite_execution_sql_builder_alter_modify_copy.inc"

#include "mylite_execution_sql_builder_alter_order_force_rename_truncate.inc"

#include "mylite_execution_insert_sql_builders.inc"

#include "mylite_execution_select_sql_builders.inc"

#include "mylite_execution_row_scalar_sql_core.inc"

#include "mylite_execution_row_scalar_sql_functions.inc"

#include "mylite_execution_row_scalar_sql_json_control.inc"

#include "mylite_execution_aggregate_predicate_sql_builders.inc"

#include "mylite_execution_dml_sql_builders.inc"

#include "mylite_execution_sqlite_write_statements.inc"

#include "mylite_execution_insert_duplicate_write_helpers.inc"

#include "mylite_execution_update_unique_key_write_conflicts.inc"

#include "mylite_execution_foreign_key_write_validation.inc"

#include "mylite_execution_unique_key_write_lookup.inc"

#include "mylite_execution_key_tuple_formatting.inc"

#include "mylite_execution_row_scalar_select_parameter_binding.inc"

#include "mylite_execution_count_having_select.inc"

#include "mylite_execution_count_expression_aggregate.inc"

#include "mylite_execution_row_scalar_expression_parameter_dispatch.inc"

#include "mylite_execution_row_scalar_window_parameter_binding.inc"

#include "mylite_execution_row_scalar_conversion_parameter_binding.inc"

#include "mylite_execution_row_scalar_arithmetic_parameter_binding.inc"

#include "mylite_execution_row_scalar_temporal_string_parameter_binding.inc"

#include "mylite_execution_row_scalar_string_regexp_parameter_binding.inc"

#include "mylite_execution_row_scalar_json_parameter_binding.inc"

#include "mylite_execution_row_scalar_control_flow_parameter_binding.inc"

#include "mylite_execution_row_scalar_encoding_uuid_char_parameter_binding.inc"

#include "mylite_execution_predicate_dml_parameter_binding.inc"

#include "mylite_execution_sqlite_result_extraction.inc"
