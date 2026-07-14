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
static int validate_single_table_delete_safe_updates(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct planned_delete *plan
);
static int plan_joined_delete(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_delete *out_plan
);
static int reject_builtin_schema_joined_delete_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node
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
static int validate_single_table_update_safe_updates(
    struct mylite_db *database,
    const struct planned_update *plan
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
static int reject_information_schema_joined_update_ambiguous_where(
    struct mylite_db *database,
    const struct information_schema_joined_update_clauses *clauses
);
static int count_information_schema_joined_update_sources(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node,
    size_t *in_out_count
);
static int information_schema_joined_update_source_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node,
    bool *out_matches
);
static int ast_subtree_has_unqualified_identifier(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    const char *identifier,
    bool *out_has_identifier
);
static bool source_span_equals_ascii_case_insensitive(
    const struct mylite_sql_source_span *span,
    const char *text
);
static int lowercase_ascii_byte(int byte);
static void planned_update_deinit(struct planned_update *plan);
static bool planned_update_has_multiple_assignments(const struct planned_update *plan);
static bool planned_update_assignment_is_noop(const struct planned_update_assignment *assignment);
static const struct planned_update_assignment *planned_update_assignment_for_column(
    const struct planned_update *plan,
    int64_t column_id
);
static bool planned_update_assigns_auto_increment(const struct planned_update *plan);
static const struct planned_update_assignment *planned_update_auto_increment_assignment(
    const struct planned_update *plan
);
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
static bool sql_safe_updates_is_enabled(const struct mylite_db *database);
static int validate_sql_safe_updates_predicate(
    struct mylite_db *database,
    const struct planned_select_predicate *predicate,
    const struct loaded_index_info *indexes,
    size_t index_count
);
static bool sql_safe_updates_predicate_uses_key(
    const struct planned_select_predicate *predicate,
    size_t node_index,
    const struct loaded_index_info *indexes,
    size_t index_count
);
static bool sql_safe_updates_leaf_node_uses_key(
    const struct planned_select_predicate_node *node,
    const struct loaded_index_info *indexes,
    size_t index_count
);
static bool sql_safe_updates_column_id_is_keyed(
    int64_t column_id,
    const struct loaded_index_info *indexes,
    size_t index_count
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
static int build_single_table_update_matched_count_sql(
    const struct planned_update *plan,
    char **out_sql
);
static int build_joined_update_matched_count_sql(const struct planned_update *plan, char **out_sql);
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
    const struct planned_update *executable_plan,
    int64_t affected_rows,
    int64_t *out_auto_increment_next
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
