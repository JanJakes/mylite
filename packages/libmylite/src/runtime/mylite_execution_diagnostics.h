#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_DIAGNOSTICS_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_DIAGNOSTICS_H

#include "mylite_ast.h"
#include "mylite_execution_plan_types.h"
#include "mylite_json.h"
#include "mylite_parser.h"

#include <stddef.h>
#include <stdint.h>

struct mylite_db;

void mylite_execution_diagnostics_set_unsupported_error(
    struct mylite_db *database,
    const char *message
);
void mylite_execution_diagnostics_set_alter_table_instant_lock_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_alter_table_instant_algorithm_error(struct mylite_db *database
);
void mylite_execution_diagnostics_set_alter_table_rebuild_instant_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_alter_table_add_foreign_key_instant_error(
    struct mylite_db *database
);
void mylite_execution_diagnostics_set_alter_table_add_foreign_key_inplace_error(
    struct mylite_db *database
);
void mylite_execution_diagnostics_set_alter_table_add_foreign_key_lock_none_error(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm
);
void mylite_execution_diagnostics_set_alter_table_add_fulltext_instant_error(
    struct mylite_db *database
);
void mylite_execution_diagnostics_set_alter_table_copy_lock_none_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_alter_table_key_maintenance_lock_error(
    struct mylite_db *database
);
void mylite_execution_diagnostics_set_alter_table_add_fulltext_lock_none_error(
    struct mylite_db *database
);
void mylite_execution_diagnostics_set_no_tables_used_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_in_subquery_limit_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_scalar_subquery_column_count_error(struct mylite_db *database
);
void mylite_execution_diagnostics_set_scalar_subquery_row_count_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_union_column_count_mismatch_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_update_table_used_error(
    struct mylite_db *database,
    const char *table_name
);
void mylite_execution_diagnostics_set_safe_update_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_session_variable_only_error(
    struct mylite_db *database,
    const char *variable_name
);
void mylite_execution_diagnostics_set_global_variable_only_error(
    struct mylite_db *database,
    const char *variable_name
);
void mylite_execution_diagnostics_set_global_variable_set_global_required_error(
    struct mylite_db *database,
    const char *variable_name
);
void mylite_execution_diagnostics_set_session_read_only_system_variable_error(
    struct mylite_db *database,
    const char *variable_name
);
void mylite_execution_diagnostics_set_read_only_system_variable_error(
    struct mylite_db *database,
    const char *variable_name
);
void mylite_execution_diagnostics_set_unknown_system_variable_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
void mylite_execution_diagnostics_set_unknown_system_variable_name_error(
    struct mylite_db *database,
    const char *variable_name
);
void mylite_execution_diagnostics_set_native_function_parameter_count_error(
    struct mylite_db *database,
    const char *function_name
);
void mylite_execution_diagnostics_set_invalid_json_function_text_error(
    struct mylite_db *database,
    size_t position
);
int mylite_execution_diagnostics_append_invalid_json_value_warning(
    struct mylite_db *database,
    const struct mylite_json_normalize_result *result
);
void mylite_execution_diagnostics_set_invalid_json_path_error(
    struct mylite_db *database,
    size_t position
);
void mylite_execution_diagnostics_set_json_path_not_allowed_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_json_path_not_array_cell_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_invalid_json_data_type_error(
    struct mylite_db *database,
    const char *function_name
);
void mylite_execution_diagnostics_set_invalid_json_one_or_all_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_json_unquote_incorrect_type_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_json_quote_incorrect_type_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_json_binary_charset_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_json_null_member_name_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_no_database_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_database_access_denied_error(
    struct mylite_db *database,
    const char *schema_name
);
void mylite_execution_diagnostics_set_system_schema_access_error(
    struct mylite_db *database,
    const char *schema_name
);
void mylite_execution_diagnostics_set_mysql_data_dictionary_table_access_error(
    struct mylite_db *database,
    const char *table_name
);
void mylite_execution_diagnostics_set_database_exists_error(
    struct mylite_db *database,
    const char *schema_name
);
int mylite_execution_diagnostics_append_database_exists_note(
    struct mylite_db *database,
    const char *schema_name
);
void mylite_execution_diagnostics_set_cant_drop_database_error(
    struct mylite_db *database,
    const char *schema_name
);
void mylite_execution_diagnostics_set_unknown_database_error(
    struct mylite_db *database,
    const char *schema_name
);
void mylite_execution_diagnostics_set_database_does_not_exist_error(
    struct mylite_db *database,
    const char *schema_name
);
void mylite_execution_diagnostics_set_table_exists_error(
    struct mylite_db *database,
    const char *table_name
);
int mylite_execution_diagnostics_append_table_exists_note(
    struct mylite_db *database,
    const char *table_name
);
void mylite_execution_diagnostics_set_create_table_select_locking_clause_error(
    struct mylite_db *database,
    const char *source_table_name,
    const char *target_table_name
);
void mylite_execution_diagnostics_set_unknown_table_error(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
);
void mylite_execution_diagnostics_set_not_view_error(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
);
void mylite_execution_diagnostics_set_unknown_table_name_error(
    struct mylite_db *database,
    const char *table_name
);
void mylite_execution_diagnostics_set_unknown_multi_delete_table_error(
    struct mylite_db *database,
    const char *table_name
);
void mylite_execution_diagnostics_set_wrong_usage_error(
    struct mylite_db *database,
    const char *left,
    const char *right
);
int mylite_execution_diagnostics_set_unknown_drop_tables_error(
    struct mylite_db *database,
    const struct planned_drop_table *plan
);
int mylite_execution_diagnostics_append_unknown_table_note(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
);
void mylite_execution_diagnostics_set_table_does_not_exist_error(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
);
void mylite_execution_diagnostics_set_unknown_storage_engine_error(
    struct mylite_db *database,
    const char *engine_name
);
void mylite_execution_diagnostics_set_table_storage_engine_option_error(
    struct mylite_db *database,
    const char *table_name
);
int mylite_execution_diagnostics_append_table_storage_engine_option_note(
    struct mylite_db *database,
    const char *table_name
);
void mylite_execution_diagnostics_set_failed_read_auto_increment_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_unknown_character_set_error(
    struct mylite_db *database,
    const char *charset_name
);
void mylite_execution_diagnostics_set_unknown_collation_error(
    struct mylite_db *database,
    const char *collation_name
);
void mylite_execution_diagnostics_set_savepoint_does_not_exist_error(
    struct mylite_db *database,
    const char *savepoint_name
);
void mylite_execution_diagnostics_set_collation_not_valid_for_charset_error(
    struct mylite_db *database,
    const char *collation_name,
    const char *charset_name
);
void mylite_execution_diagnostics_set_illegal_mix_of_collations_error(
    struct mylite_db *database,
    const char *first_collation,
    const char *second_collation,
    const char *operation
);
void mylite_execution_diagnostics_set_conflicting_character_set_declarations_error(
    struct mylite_db *database,
    const char *first_charset,
    const char *second_charset
);
void mylite_execution_diagnostics_set_duplicate_column_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_duplicated_enum_value_error(
    struct mylite_db *database,
    const char *column_name,
    const char *value,
    size_t value_length
);
void mylite_execution_diagnostics_set_duplicated_set_value_error(
    struct mylite_db *database,
    const char *column_name,
    const char *value,
    size_t value_length
);
void mylite_execution_diagnostics_set_illegal_set_value_error(
    struct mylite_db *database,
    const char *value,
    size_t value_length
);
void mylite_execution_diagnostics_set_multiple_primary_key_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_sql_require_primary_key_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_wrong_auto_key_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_column_length_too_big_error(
    struct mylite_db *database,
    const char *column_name,
    uint64_t maximum_length
);
void mylite_execution_diagnostics_set_row_size_too_large_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_incorrect_column_specifier_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_key_column_missing_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_invalid_use_of_null_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_primary_key_part_null_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_duplicate_key_name_error(
    struct mylite_db *database,
    const char *index_name
);
void mylite_execution_diagnostics_set_incorrect_index_name_error(
    struct mylite_db *database,
    const char *index_name
);
void mylite_execution_diagnostics_set_index_hint_use_force_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_key_does_not_exist_in_table_error(
    struct mylite_db *database,
    const char *index_name,
    const char *table_name
);
void mylite_execution_diagnostics_set_primary_key_index_invisible_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_storage_engine_cant_index_column_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_fulltext_column_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_fulltext_explicit_order_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_temporary_fulltext_index_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_spatial_index_non_geometric_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_spatial_index_must_be_not_null_error(
    struct mylite_db *database
);
void mylite_execution_diagnostics_set_spatial_unique_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_spatial_index_type_not_supported_error(
    struct mylite_db *database,
    const char *index_type
);
void mylite_execution_diagnostics_set_spatial_too_many_key_parts_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_blob_key_without_length_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_json_key_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_incorrect_prefix_key_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_key_part_length_cannot_be_zero_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_key_too_long_error(
    struct mylite_db *database,
    uint64_t maximum_key_length_bytes
);
void mylite_execution_diagnostics_set_table_comment_too_long_error(
    struct mylite_db *database,
    const char *table_name
);
void mylite_execution_diagnostics_set_column_comment_too_long_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_index_comment_too_long_error(
    struct mylite_db *database,
    const char *index_name
);
void mylite_execution_diagnostics_set_non_ascii_string_key_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_duplicate_key_error(
    struct mylite_db *database,
    const char *table_name,
    const char *index_name,
    const char *value
);
void mylite_execution_diagnostics_set_no_referenced_row_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_row_is_referenced_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_cannot_drop_index_needed_foreign_key_error(
    struct mylite_db *database,
    const char *index_name
);
void mylite_execution_diagnostics_set_failed_to_open_referenced_table_error(
    struct mylite_db *database,
    const char *table_name
);
void mylite_execution_diagnostics_set_incorrect_foreign_key_definition_error(
    struct mylite_db *database
);
void mylite_execution_diagnostics_set_foreign_key_column_incompatible_error(
    struct mylite_db *database
);
void mylite_execution_diagnostics_set_foreign_key_missing_unique_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_duplicate_foreign_key_error(
    struct mylite_db *database,
    const char *foreign_key_name
);
void mylite_execution_diagnostics_set_drop_column_foreign_key_child_error(
    struct mylite_db *database,
    const char *column_name,
    const char *foreign_key_name
);
void mylite_execution_diagnostics_set_drop_column_foreign_key_parent_error(
    struct mylite_db *database,
    const char *column_name,
    const char *foreign_key_name,
    const char *child_table_name
);
void mylite_execution_diagnostics_set_foreign_key_set_null_not_nullable_error(
    struct mylite_db *database,
    const char *column_name,
    const char *foreign_key_name
);
void mylite_execution_diagnostics_set_foreign_key_cascade_duplicate_error(
    struct mylite_db *database,
    const char *parent_table_name,
    const char *record_value,
    const char *child_table_name,
    const char *index_name
);
void mylite_execution_diagnostics_set_check_constraint_non_boolean_error(struct mylite_db *database
);
void mylite_execution_diagnostics_set_check_constraint_column_ref_error(
    struct mylite_db *database,
    const char *constraint_name,
    const char *column_name
);
void mylite_execution_diagnostics_set_check_constraint_function_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_check_constraint_subquery_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_check_constraint_variable_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_check_constraint_auto_increment_error(
    struct mylite_db *database
);
void mylite_execution_diagnostics_set_check_constraint_violated_error(
    struct mylite_db *database,
    const char *constraint_name
);
void mylite_execution_diagnostics_set_check_constraint_not_found_error(
    struct mylite_db *database,
    const char *constraint_name
);
void mylite_execution_diagnostics_set_drop_constraint_ambiguous_error(
    struct mylite_db *database,
    const char *constraint_name
);
void mylite_execution_diagnostics_set_constraint_does_not_exist_error(
    struct mylite_db *database,
    const char *constraint_name
);
void mylite_execution_diagnostics_set_check_constraint_unknown_column_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_alter_check_constraint_unknown_column_error(
    struct mylite_db *database,
    const char *column_name,
    const char *constraint_name
);
void mylite_execution_diagnostics_set_duplicate_check_constraint_error(
    struct mylite_db *database,
    const char *check_constraint_name
);
int mylite_execution_diagnostics_append_check_constraint_warning(
    struct mylite_db *database,
    const char *constraint_name
);
int mylite_execution_diagnostics_append_duplicate_key_warning(
    struct mylite_db *database,
    const char *table_name,
    const char *index_name,
    const char *value
);
int mylite_execution_diagnostics_append_no_referenced_row_warning(struct mylite_db *database);
void mylite_execution_diagnostics_set_duplicate_table_alias_error(
    struct mylite_db *database,
    const char *table_name
);
void mylite_execution_diagnostics_set_cant_drop_field_or_key_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_cant_remove_all_fields_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_must_have_visible_column_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_unknown_column_in_table_error(
    struct mylite_db *database,
    const char *column_name,
    const char *table_name
);
void mylite_execution_diagnostics_set_unknown_information_schema_table_error(
    struct mylite_db *database,
    const char *table_name
);
void mylite_execution_diagnostics_set_unknown_column_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_unknown_where_column_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_unknown_order_column_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_unknown_group_column_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_unknown_having_column_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_unknown_on_column_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_ambiguous_column_reference_error(
    struct mylite_db *database,
    enum column_reference_diagnostic_context context,
    const char *column_name
);
void mylite_execution_diagnostics_set_ambiguous_order_column_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_not_unique_table_alias_error(
    struct mylite_db *database,
    const char *alias
);
void mylite_execution_diagnostics_set_only_full_group_by_error(
    struct mylite_db *database,
    size_t expression_index,
    const char *clause_name,
    const struct table_name_resolution *source,
    const struct mylite_catalog_column_descriptor *column
);
void mylite_execution_diagnostics_set_column_specified_twice_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_column_count_mismatch_error(
    struct mylite_db *database,
    size_t row_number
);
void mylite_execution_diagnostics_set_values_empty_row_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_values_default_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_values_integer_out_of_range_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_bad_null_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_load_data_file_error(
    struct mylite_db *database,
    const char *file_path,
    int os_error
);
void mylite_execution_diagnostics_set_load_data_local_disabled_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_load_data_row_missing_error(
    struct mylite_db *database,
    size_t row_number
);
int mylite_execution_diagnostics_append_load_data_row_missing_warnings(
    struct mylite_db *database,
    struct load_data_missing_warning_request request
);
void mylite_execution_diagnostics_set_load_data_row_truncated_error(
    struct mylite_db *database,
    size_t row_number
);
int mylite_execution_diagnostics_append_load_data_row_truncated_warning(
    struct mylite_db *database,
    size_t row_number
);
void mylite_execution_diagnostics_set_load_data_null_to_not_null_error(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
);
int mylite_execution_diagnostics_append_load_data_null_to_not_null_warning(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
);
void mylite_execution_diagnostics_set_spatial_bad_null_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_generated_column_value_error(
    struct mylite_db *database,
    const char *column_name,
    const char *table_name
);
void mylite_execution_diagnostics_set_data_truncated_error(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
);
void mylite_execution_diagnostics_set_data_too_long_error(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
);
void mylite_execution_diagnostics_set_invalid_default_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_json_cant_have_default_error(
    struct mylite_db *database,
    const char *column_name
);
int mylite_execution_diagnostics_append_blob_text_cant_have_default_warning(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_invalid_json_text_error(
    struct mylite_db *database,
    size_t position,
    const char *column_name
);
void mylite_execution_diagnostics_set_no_default_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_default_function_expression_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_out_of_range_error(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
);
void mylite_execution_diagnostics_set_invalid_column_size_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_incorrect_date_value_error(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
);
void mylite_execution_diagnostics_set_incorrect_time_value_error(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
);
void mylite_execution_diagnostics_set_incorrect_datetime_value_error(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
);
void mylite_execution_diagnostics_set_incorrect_date_literal_error(
    struct mylite_db *database,
    const char *value_text
);
void mylite_execution_diagnostics_set_incorrect_datetime_literal_error(
    struct mylite_db *database,
    const char *value_text
);
void mylite_execution_diagnostics_set_incorrect_timestamp_value_error(
    struct mylite_db *database,
    const char *value_text
);
int mylite_execution_diagnostics_append_incorrect_datetime_predicate_warning(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name
);
int mylite_execution_diagnostics_append_incorrect_date_predicate_warning(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name
);
int mylite_execution_diagnostics_append_incorrect_date_value_note(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
);
int mylite_execution_diagnostics_append_bad_null_warning(
    struct mylite_db *database,
    const char *column_name
);
int mylite_execution_diagnostics_append_no_default_warning(
    struct mylite_db *database,
    const char *column_name
);
int mylite_execution_diagnostics_append_out_of_range_warning(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
);
void mylite_execution_diagnostics_set_incorrect_integer_value_error(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
);
void mylite_execution_diagnostics_set_incorrect_decimal_value_error(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
);
int mylite_execution_diagnostics_append_incorrect_integer_value_warning(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
);
int mylite_execution_diagnostics_append_incorrect_decimal_value_warning(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
);
int mylite_execution_diagnostics_append_data_truncated_warning(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
);
int mylite_execution_diagnostics_append_data_too_long_warning(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
);
int mylite_execution_diagnostics_append_data_truncated_note(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
);
int mylite_execution_diagnostics_append_decimal_truncated_note(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
);
void mylite_execution_diagnostics_set_display_width_out_of_range_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_text_display_width_out_of_range_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_bit_display_width_out_of_range_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_invalid_year_display_width_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_decimal_precision_too_big_error(
    struct mylite_db *database,
    const char *column_name,
    uint64_t precision
);
void mylite_execution_diagnostics_set_temporal_precision_too_big_error(
    struct mylite_db *database,
    const char *subject_name,
    uint64_t precision
);
void mylite_execution_diagnostics_set_decimal_scale_too_big_error(
    struct mylite_db *database,
    const char *column_name,
    uint64_t scale
);
void mylite_execution_diagnostics_set_decimal_scale_greater_than_precision_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_predicate_out_of_range_error(
    struct mylite_db *database,
    const char *column_name
);
void mylite_execution_diagnostics_set_having_out_of_range_error(
    struct mylite_db *database,
    const char *operand_name
);
void mylite_execution_diagnostics_set_limit_out_of_range_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_regexp_error(struct mylite_db *database, const char *message);
void mylite_execution_diagnostics_set_regexp_illegal_argument_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_regexp_character_range_error(
    struct mylite_db *database,
    const char *message
);
void mylite_execution_diagnostics_set_identifier_too_long_error(
    struct mylite_db *database,
    const char *kind
);
void mylite_execution_diagnostics_set_reserved_name_error(
    struct mylite_db *database,
    const char *kind,
    const char *name
);
void mylite_execution_diagnostics_set_nomem_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_physical_sqlite_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_physical_sqlite_row_error(struct mylite_db *database);
void mylite_execution_diagnostics_set_runtime_error(
    struct mylite_db *database,
    const char *message
);
void mylite_execution_diagnostics_set_internal_error_if_clear(
    struct mylite_db *database,
    int rc,
    const char *message
);
int mylite_execution_diagnostics_status_from_parse_status(enum mylite_sql_parse_status status);

#ifndef MYLITE_EXECUTION_DIAGNOSTICS_NO_SHORT_NAMES
#  define set_unsupported_error mylite_execution_diagnostics_set_unsupported_error
#  define set_alter_table_instant_lock_error                                                       \
      mylite_execution_diagnostics_set_alter_table_instant_lock_error
#  define set_alter_table_instant_algorithm_error                                                  \
      mylite_execution_diagnostics_set_alter_table_instant_algorithm_error
#  define set_alter_table_rebuild_instant_error                                                    \
      mylite_execution_diagnostics_set_alter_table_rebuild_instant_error
#  define set_alter_table_add_foreign_key_instant_error                                            \
      mylite_execution_diagnostics_set_alter_table_add_foreign_key_instant_error
#  define set_alter_table_add_foreign_key_inplace_error                                            \
      mylite_execution_diagnostics_set_alter_table_add_foreign_key_inplace_error
#  define set_alter_table_add_foreign_key_lock_none_error                                          \
      mylite_execution_diagnostics_set_alter_table_add_foreign_key_lock_none_error
#  define set_alter_table_add_fulltext_instant_error                                               \
      mylite_execution_diagnostics_set_alter_table_add_fulltext_instant_error
#  define set_alter_table_copy_lock_none_error                                                     \
      mylite_execution_diagnostics_set_alter_table_copy_lock_none_error
#  define set_alter_table_key_maintenance_lock_error                                               \
      mylite_execution_diagnostics_set_alter_table_key_maintenance_lock_error
#  define set_alter_table_add_fulltext_lock_none_error                                             \
      mylite_execution_diagnostics_set_alter_table_add_fulltext_lock_none_error
#  define set_no_tables_used_error mylite_execution_diagnostics_set_no_tables_used_error
#  define set_in_subquery_limit_error mylite_execution_diagnostics_set_in_subquery_limit_error
#  define set_scalar_subquery_column_count_error                                                   \
      mylite_execution_diagnostics_set_scalar_subquery_column_count_error
#  define set_scalar_subquery_row_count_error                                                      \
      mylite_execution_diagnostics_set_scalar_subquery_row_count_error
#  define set_union_column_count_mismatch_error                                                    \
      mylite_execution_diagnostics_set_union_column_count_mismatch_error
#  define set_update_table_used_error mylite_execution_diagnostics_set_update_table_used_error
#  define set_session_variable_only_error                                                          \
      mylite_execution_diagnostics_set_session_variable_only_error
#  define set_global_variable_only_error mylite_execution_diagnostics_set_global_variable_only_error
#  define set_global_variable_set_global_required_error                                            \
      mylite_execution_diagnostics_set_global_variable_set_global_required_error
#  define set_session_read_only_system_variable_error                                              \
      mylite_execution_diagnostics_set_session_read_only_system_variable_error
#  define set_read_only_system_variable_error                                                      \
      mylite_execution_diagnostics_set_read_only_system_variable_error
#  define set_unknown_system_variable_error                                                        \
      mylite_execution_diagnostics_set_unknown_system_variable_error
#  define set_unknown_system_variable_name_error                                                   \
      mylite_execution_diagnostics_set_unknown_system_variable_name_error
#  define set_native_function_parameter_count_error                                                \
      mylite_execution_diagnostics_set_native_function_parameter_count_error
#  define set_invalid_json_function_text_error                                                     \
      mylite_execution_diagnostics_set_invalid_json_function_text_error
#  define append_invalid_json_value_warning                                                        \
      mylite_execution_diagnostics_append_invalid_json_value_warning
#  define set_invalid_json_path_error mylite_execution_diagnostics_set_invalid_json_path_error
#  define set_json_path_not_allowed_error                                                          \
      mylite_execution_diagnostics_set_json_path_not_allowed_error
#  define set_json_path_not_array_cell_error                                                       \
      mylite_execution_diagnostics_set_json_path_not_array_cell_error
#  define set_invalid_json_data_type_error                                                         \
      mylite_execution_diagnostics_set_invalid_json_data_type_error
#  define set_invalid_json_one_or_all_error                                                        \
      mylite_execution_diagnostics_set_invalid_json_one_or_all_error
#  define set_json_unquote_incorrect_type_error                                                    \
      mylite_execution_diagnostics_set_json_unquote_incorrect_type_error
#  define set_json_quote_incorrect_type_error                                                      \
      mylite_execution_diagnostics_set_json_quote_incorrect_type_error
#  define set_json_binary_charset_error mylite_execution_diagnostics_set_json_binary_charset_error
#  define set_json_null_member_name_error                                                          \
      mylite_execution_diagnostics_set_json_null_member_name_error
#  define set_no_database_error mylite_execution_diagnostics_set_no_database_error
#  define set_database_access_denied_error                                                         \
      mylite_execution_diagnostics_set_database_access_denied_error
#  define set_system_schema_access_error mylite_execution_diagnostics_set_system_schema_access_error
#  define set_mysql_data_dictionary_table_access_error                                             \
      mylite_execution_diagnostics_set_mysql_data_dictionary_table_access_error
#  define set_database_exists_error mylite_execution_diagnostics_set_database_exists_error
#  define append_database_exists_note mylite_execution_diagnostics_append_database_exists_note
#  define set_cant_drop_database_error mylite_execution_diagnostics_set_cant_drop_database_error
#  define set_unknown_database_error mylite_execution_diagnostics_set_unknown_database_error
#  define set_database_does_not_exist_error                                                        \
      mylite_execution_diagnostics_set_database_does_not_exist_error
#  define set_table_exists_error mylite_execution_diagnostics_set_table_exists_error
#  define append_table_exists_note mylite_execution_diagnostics_append_table_exists_note
#  define set_create_table_select_locking_clause_error                                             \
      mylite_execution_diagnostics_set_create_table_select_locking_clause_error
#  define set_unknown_table_error mylite_execution_diagnostics_set_unknown_table_error
#  define set_not_view_error mylite_execution_diagnostics_set_not_view_error
#  define set_unknown_table_name_error mylite_execution_diagnostics_set_unknown_table_name_error
#  define set_unknown_multi_delete_table_error                                                     \
      mylite_execution_diagnostics_set_unknown_multi_delete_table_error
#  define set_wrong_usage_error mylite_execution_diagnostics_set_wrong_usage_error
#  define set_unknown_drop_tables_error mylite_execution_diagnostics_set_unknown_drop_tables_error
#  define append_unknown_table_note mylite_execution_diagnostics_append_unknown_table_note
#  define set_table_does_not_exist_error mylite_execution_diagnostics_set_table_does_not_exist_error
#  define set_unknown_storage_engine_error                                                         \
      mylite_execution_diagnostics_set_unknown_storage_engine_error
#  define set_table_storage_engine_option_error                                                    \
      mylite_execution_diagnostics_set_table_storage_engine_option_error
#  define append_table_storage_engine_option_note                                                  \
      mylite_execution_diagnostics_append_table_storage_engine_option_note
#  define set_failed_read_auto_increment_error                                                     \
      mylite_execution_diagnostics_set_failed_read_auto_increment_error
#  define set_unknown_character_set_error                                                          \
      mylite_execution_diagnostics_set_unknown_character_set_error
#  define set_unknown_collation_error mylite_execution_diagnostics_set_unknown_collation_error
#  define set_savepoint_does_not_exist_error                                                       \
      mylite_execution_diagnostics_set_savepoint_does_not_exist_error
#  define set_collation_not_valid_for_charset_error                                                \
      mylite_execution_diagnostics_set_collation_not_valid_for_charset_error
#  define set_illegal_mix_of_collations_error                                                      \
      mylite_execution_diagnostics_set_illegal_mix_of_collations_error
#  define set_conflicting_character_set_declarations_error                                         \
      mylite_execution_diagnostics_set_conflicting_character_set_declarations_error
#  define set_duplicate_column_error mylite_execution_diagnostics_set_duplicate_column_error
#  define set_duplicated_enum_value_error                                                          \
      mylite_execution_diagnostics_set_duplicated_enum_value_error
#  define set_duplicated_set_value_error mylite_execution_diagnostics_set_duplicated_set_value_error
#  define set_illegal_set_value_error mylite_execution_diagnostics_set_illegal_set_value_error
#  define set_multiple_primary_key_error mylite_execution_diagnostics_set_multiple_primary_key_error
#  define set_sql_require_primary_key_error                                                        \
      mylite_execution_diagnostics_set_sql_require_primary_key_error
#  define set_wrong_auto_key_error mylite_execution_diagnostics_set_wrong_auto_key_error
#  define set_column_length_too_big_error                                                          \
      mylite_execution_diagnostics_set_column_length_too_big_error
#  define set_row_size_too_large_error mylite_execution_diagnostics_set_row_size_too_large_error
#  define set_incorrect_column_specifier_error                                                     \
      mylite_execution_diagnostics_set_incorrect_column_specifier_error
#  define set_key_column_missing_error mylite_execution_diagnostics_set_key_column_missing_error
#  define set_invalid_use_of_null_error mylite_execution_diagnostics_set_invalid_use_of_null_error
#  define set_primary_key_part_null_error                                                          \
      mylite_execution_diagnostics_set_primary_key_part_null_error
#  define set_duplicate_key_name_error mylite_execution_diagnostics_set_duplicate_key_name_error
#  define set_incorrect_index_name_error mylite_execution_diagnostics_set_incorrect_index_name_error
#  define set_index_hint_use_force_error mylite_execution_diagnostics_set_index_hint_use_force_error
#  define set_key_does_not_exist_in_table_error                                                    \
      mylite_execution_diagnostics_set_key_does_not_exist_in_table_error
#  define set_primary_key_index_invisible_error                                                    \
      mylite_execution_diagnostics_set_primary_key_index_invisible_error
#  define set_storage_engine_cant_index_column_error                                               \
      mylite_execution_diagnostics_set_storage_engine_cant_index_column_error
#  define set_fulltext_column_error mylite_execution_diagnostics_set_fulltext_column_error
#  define set_fulltext_explicit_order_error                                                        \
      mylite_execution_diagnostics_set_fulltext_explicit_order_error
#  define set_temporary_fulltext_index_error                                                       \
      mylite_execution_diagnostics_set_temporary_fulltext_index_error
#  define set_spatial_index_non_geometric_error                                                    \
      mylite_execution_diagnostics_set_spatial_index_non_geometric_error
#  define set_spatial_index_must_be_not_null_error                                                 \
      mylite_execution_diagnostics_set_spatial_index_must_be_not_null_error
#  define set_spatial_unique_error mylite_execution_diagnostics_set_spatial_unique_error
#  define set_spatial_index_type_not_supported_error                                               \
      mylite_execution_diagnostics_set_spatial_index_type_not_supported_error
#  define set_spatial_too_many_key_parts_error                                                     \
      mylite_execution_diagnostics_set_spatial_too_many_key_parts_error
#  define set_blob_key_without_length_error                                                        \
      mylite_execution_diagnostics_set_blob_key_without_length_error
#  define set_json_key_error mylite_execution_diagnostics_set_json_key_error
#  define set_incorrect_prefix_key_error mylite_execution_diagnostics_set_incorrect_prefix_key_error
#  define set_key_part_length_cannot_be_zero_error                                                 \
      mylite_execution_diagnostics_set_key_part_length_cannot_be_zero_error
#  define set_key_too_long_error mylite_execution_diagnostics_set_key_too_long_error
#  define set_table_comment_too_long_error                                                         \
      mylite_execution_diagnostics_set_table_comment_too_long_error
#  define set_column_comment_too_long_error                                                        \
      mylite_execution_diagnostics_set_column_comment_too_long_error
#  define set_index_comment_too_long_error                                                         \
      mylite_execution_diagnostics_set_index_comment_too_long_error
#  define set_non_ascii_string_key_error mylite_execution_diagnostics_set_non_ascii_string_key_error
#  define set_duplicate_key_error mylite_execution_diagnostics_set_duplicate_key_error
#  define set_no_referenced_row_error mylite_execution_diagnostics_set_no_referenced_row_error
#  define set_row_is_referenced_error mylite_execution_diagnostics_set_row_is_referenced_error
#  define set_cannot_drop_index_needed_foreign_key_error                                           \
      mylite_execution_diagnostics_set_cannot_drop_index_needed_foreign_key_error
#  define set_failed_to_open_referenced_table_error                                                \
      mylite_execution_diagnostics_set_failed_to_open_referenced_table_error
#  define set_incorrect_foreign_key_definition_error                                               \
      mylite_execution_diagnostics_set_incorrect_foreign_key_definition_error
#  define set_foreign_key_column_incompatible_error                                                \
      mylite_execution_diagnostics_set_foreign_key_column_incompatible_error
#  define set_foreign_key_missing_unique_error                                                     \
      mylite_execution_diagnostics_set_foreign_key_missing_unique_error
#  define set_duplicate_foreign_key_error                                                          \
      mylite_execution_diagnostics_set_duplicate_foreign_key_error
#  define set_drop_column_foreign_key_child_error                                                  \
      mylite_execution_diagnostics_set_drop_column_foreign_key_child_error
#  define set_drop_column_foreign_key_parent_error                                                 \
      mylite_execution_diagnostics_set_drop_column_foreign_key_parent_error
#  define set_foreign_key_set_null_not_nullable_error                                              \
      mylite_execution_diagnostics_set_foreign_key_set_null_not_nullable_error
#  define set_foreign_key_cascade_duplicate_error                                                  \
      mylite_execution_diagnostics_set_foreign_key_cascade_duplicate_error
#  define set_check_constraint_non_boolean_error                                                   \
      mylite_execution_diagnostics_set_check_constraint_non_boolean_error
#  define set_check_constraint_column_ref_error                                                    \
      mylite_execution_diagnostics_set_check_constraint_column_ref_error
#  define set_check_constraint_function_error                                                      \
      mylite_execution_diagnostics_set_check_constraint_function_error
#  define set_check_constraint_subquery_error                                                      \
      mylite_execution_diagnostics_set_check_constraint_subquery_error
#  define set_check_constraint_variable_error                                                      \
      mylite_execution_diagnostics_set_check_constraint_variable_error
#  define set_check_constraint_auto_increment_error                                                \
      mylite_execution_diagnostics_set_check_constraint_auto_increment_error
#  define set_check_constraint_violated_error                                                      \
      mylite_execution_diagnostics_set_check_constraint_violated_error
#  define set_check_constraint_not_found_error                                                     \
      mylite_execution_diagnostics_set_check_constraint_not_found_error
#  define set_drop_constraint_ambiguous_error                                                      \
      mylite_execution_diagnostics_set_drop_constraint_ambiguous_error
#  define set_constraint_does_not_exist_error                                                      \
      mylite_execution_diagnostics_set_constraint_does_not_exist_error
#  define set_check_constraint_unknown_column_error                                                \
      mylite_execution_diagnostics_set_check_constraint_unknown_column_error
#  define set_alter_check_constraint_unknown_column_error                                          \
      mylite_execution_diagnostics_set_alter_check_constraint_unknown_column_error
#  define set_duplicate_check_constraint_error                                                     \
      mylite_execution_diagnostics_set_duplicate_check_constraint_error
#  define append_check_constraint_warning                                                          \
      mylite_execution_diagnostics_append_check_constraint_warning
#  define append_duplicate_key_warning mylite_execution_diagnostics_append_duplicate_key_warning
#  define append_no_referenced_row_warning                                                         \
      mylite_execution_diagnostics_append_no_referenced_row_warning
#  define set_duplicate_table_alias_error                                                          \
      mylite_execution_diagnostics_set_duplicate_table_alias_error
#  define set_cant_drop_field_or_key_error                                                         \
      mylite_execution_diagnostics_set_cant_drop_field_or_key_error
#  define set_cant_remove_all_fields_error                                                         \
      mylite_execution_diagnostics_set_cant_remove_all_fields_error
#  define set_must_have_visible_column_error                                                       \
      mylite_execution_diagnostics_set_must_have_visible_column_error
#  define set_unknown_column_in_table_error                                                        \
      mylite_execution_diagnostics_set_unknown_column_in_table_error
#  define set_unknown_information_schema_table_error                                               \
      mylite_execution_diagnostics_set_unknown_information_schema_table_error
#  define set_unknown_column_error mylite_execution_diagnostics_set_unknown_column_error
#  define set_unknown_where_column_error mylite_execution_diagnostics_set_unknown_where_column_error
#  define set_unknown_order_column_error mylite_execution_diagnostics_set_unknown_order_column_error
#  define set_unknown_group_column_error mylite_execution_diagnostics_set_unknown_group_column_error
#  define set_unknown_having_column_error                                                          \
      mylite_execution_diagnostics_set_unknown_having_column_error
#  define set_unknown_on_column_error mylite_execution_diagnostics_set_unknown_on_column_error
#  define set_ambiguous_column_reference_error                                                     \
      mylite_execution_diagnostics_set_ambiguous_column_reference_error
#  define set_ambiguous_order_column_error                                                         \
      mylite_execution_diagnostics_set_ambiguous_order_column_error
#  define set_not_unique_table_alias_error                                                         \
      mylite_execution_diagnostics_set_not_unique_table_alias_error
#  define set_only_full_group_by_error mylite_execution_diagnostics_set_only_full_group_by_error
#  define set_column_specified_twice_error                                                         \
      mylite_execution_diagnostics_set_column_specified_twice_error
#  define set_column_count_mismatch_error                                                          \
      mylite_execution_diagnostics_set_column_count_mismatch_error
#  define set_values_empty_row_error mylite_execution_diagnostics_set_values_empty_row_error
#  define set_values_default_error mylite_execution_diagnostics_set_values_default_error
#  define set_values_integer_out_of_range_error                                                    \
      mylite_execution_diagnostics_set_values_integer_out_of_range_error
#  define set_bad_null_error mylite_execution_diagnostics_set_bad_null_error
#  define set_load_data_file_error mylite_execution_diagnostics_set_load_data_file_error
#  define set_load_data_local_disabled_error                                                       \
      mylite_execution_diagnostics_set_load_data_local_disabled_error
#  define set_load_data_row_missing_error                                                          \
      mylite_execution_diagnostics_set_load_data_row_missing_error
#  define append_load_data_row_missing_warnings                                                    \
      mylite_execution_diagnostics_append_load_data_row_missing_warnings
#  define set_load_data_row_truncated_error                                                        \
      mylite_execution_diagnostics_set_load_data_row_truncated_error
#  define append_load_data_row_truncated_warning                                                   \
      mylite_execution_diagnostics_append_load_data_row_truncated_warning
#  define set_load_data_null_to_not_null_error                                                     \
      mylite_execution_diagnostics_set_load_data_null_to_not_null_error
#  define append_load_data_null_to_not_null_warning                                                \
      mylite_execution_diagnostics_append_load_data_null_to_not_null_warning
#  define set_spatial_bad_null_error mylite_execution_diagnostics_set_spatial_bad_null_error
#  define set_generated_column_value_error                                                         \
      mylite_execution_diagnostics_set_generated_column_value_error
#  define set_data_truncated_error mylite_execution_diagnostics_set_data_truncated_error
#  define set_data_too_long_error mylite_execution_diagnostics_set_data_too_long_error
#  define set_invalid_default_error mylite_execution_diagnostics_set_invalid_default_error
#  define set_json_cant_have_default_error                                                         \
      mylite_execution_diagnostics_set_json_cant_have_default_error
#  define append_blob_text_cant_have_default_warning                                               \
      mylite_execution_diagnostics_append_blob_text_cant_have_default_warning
#  define set_invalid_json_text_error mylite_execution_diagnostics_set_invalid_json_text_error
#  define set_no_default_error mylite_execution_diagnostics_set_no_default_error
#  define set_default_function_expression_error                                                    \
      mylite_execution_diagnostics_set_default_function_expression_error
#  define set_out_of_range_error mylite_execution_diagnostics_set_out_of_range_error
#  define set_invalid_column_size_error mylite_execution_diagnostics_set_invalid_column_size_error
#  define set_incorrect_date_value_error mylite_execution_diagnostics_set_incorrect_date_value_error
#  define set_incorrect_time_value_error mylite_execution_diagnostics_set_incorrect_time_value_error
#  define set_incorrect_datetime_value_error                                                       \
      mylite_execution_diagnostics_set_incorrect_datetime_value_error
#  define set_incorrect_date_literal_error                                                         \
      mylite_execution_diagnostics_set_incorrect_date_literal_error
#  define set_incorrect_datetime_literal_error                                                     \
      mylite_execution_diagnostics_set_incorrect_datetime_literal_error
#  define set_incorrect_timestamp_value_error                                                      \
      mylite_execution_diagnostics_set_incorrect_timestamp_value_error
#  define append_incorrect_datetime_predicate_warning                                              \
      mylite_execution_diagnostics_append_incorrect_datetime_predicate_warning
#  define append_incorrect_date_predicate_warning                                                  \
      mylite_execution_diagnostics_append_incorrect_date_predicate_warning
#  define append_incorrect_date_value_note                                                         \
      mylite_execution_diagnostics_append_incorrect_date_value_note
#  define append_bad_null_warning mylite_execution_diagnostics_append_bad_null_warning
#  define append_no_default_warning mylite_execution_diagnostics_append_no_default_warning
#  define append_out_of_range_warning mylite_execution_diagnostics_append_out_of_range_warning
#  define set_incorrect_integer_value_error                                                        \
      mylite_execution_diagnostics_set_incorrect_integer_value_error
#  define set_incorrect_decimal_value_error                                                        \
      mylite_execution_diagnostics_set_incorrect_decimal_value_error
#  define append_incorrect_integer_value_warning                                                   \
      mylite_execution_diagnostics_append_incorrect_integer_value_warning
#  define append_incorrect_decimal_value_warning                                                   \
      mylite_execution_diagnostics_append_incorrect_decimal_value_warning
#  define append_data_truncated_warning mylite_execution_diagnostics_append_data_truncated_warning
#  define append_data_too_long_warning mylite_execution_diagnostics_append_data_too_long_warning
#  define append_data_truncated_note mylite_execution_diagnostics_append_data_truncated_note
#  define append_decimal_truncated_note mylite_execution_diagnostics_append_decimal_truncated_note
#  define set_display_width_out_of_range_error                                                     \
      mylite_execution_diagnostics_set_display_width_out_of_range_error
#  define set_text_display_width_out_of_range_error                                                \
      mylite_execution_diagnostics_set_text_display_width_out_of_range_error
#  define set_bit_display_width_out_of_range_error                                                 \
      mylite_execution_diagnostics_set_bit_display_width_out_of_range_error
#  define set_invalid_year_display_width_error                                                     \
      mylite_execution_diagnostics_set_invalid_year_display_width_error
#  define set_decimal_precision_too_big_error                                                      \
      mylite_execution_diagnostics_set_decimal_precision_too_big_error
#  define set_temporal_precision_too_big_error                                                     \
      mylite_execution_diagnostics_set_temporal_precision_too_big_error
#  define set_decimal_scale_too_big_error                                                          \
      mylite_execution_diagnostics_set_decimal_scale_too_big_error
#  define set_decimal_scale_greater_than_precision_error                                           \
      mylite_execution_diagnostics_set_decimal_scale_greater_than_precision_error
#  define set_predicate_out_of_range_error                                                         \
      mylite_execution_diagnostics_set_predicate_out_of_range_error
#  define set_having_out_of_range_error mylite_execution_diagnostics_set_having_out_of_range_error
#  define set_limit_out_of_range_error mylite_execution_diagnostics_set_limit_out_of_range_error
#  define set_regexp_error mylite_execution_diagnostics_set_regexp_error
#  define set_regexp_illegal_argument_error                                                        \
      mylite_execution_diagnostics_set_regexp_illegal_argument_error
#  define set_regexp_character_range_error                                                         \
      mylite_execution_diagnostics_set_regexp_character_range_error
#  define set_identifier_too_long_error mylite_execution_diagnostics_set_identifier_too_long_error
#  define set_reserved_name_error mylite_execution_diagnostics_set_reserved_name_error
#  define set_nomem_error mylite_execution_diagnostics_set_nomem_error
#  define set_physical_sqlite_error mylite_execution_diagnostics_set_physical_sqlite_error
#  define set_physical_sqlite_row_error mylite_execution_diagnostics_set_physical_sqlite_row_error
#  define set_runtime_error mylite_execution_diagnostics_set_runtime_error
#  define set_internal_error_if_clear mylite_execution_diagnostics_set_internal_error_if_clear
#  define status_from_parse_status mylite_execution_diagnostics_status_from_parse_status
#endif

#endif
