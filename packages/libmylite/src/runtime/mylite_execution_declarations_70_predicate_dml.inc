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
static bool delete_where_clause_needs_source_context(const struct mylite_sql_ast_node *where_clause
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
static int validate_create_table_visible_columns(
    struct mylite_db *database,
    const struct planned_create_table *plan
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
static int apply_column_binary_collation_shorthand(
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
static int render_check_expression_function(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    bool *out_is_boolean
);
static int render_check_expression_identifier_argument_function(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *argument,
    const char *check_clause_function,
    const char *sqlite_function,
    bool is_boolean,
    bool *out_is_boolean
);
static int append_check_expression_mysql_string_literal(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_length
);
static int append_check_expression_sqlite_string_literal(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_length
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
    size_t parent_column_index,
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
static bool create_table_foreign_key_parent_is_self_reference(
    const struct planned_create_table *plan,
    const struct table_name_resolution *parent_resolution
);
static int plan_create_table_self_referencing_foreign_key_parent(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct table_name_resolution *parent_resolution,
    const struct foreign_key_column_names *names,
    struct planned_foreign_key *foreign_key
);
static int copy_planned_column_to_self_foreign_key_parent(
    struct mylite_db *database,
    const struct planned_column *planned,
    size_t column_index,
    struct mylite_catalog_column_descriptor *out_column
);
static int find_create_table_self_foreign_key_parent_index(
    struct mylite_db *database,
    const struct planned_create_table *plan,
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
static bool planned_primary_key_starts_with_column_index(
    const struct planned_create_table *plan,
    size_t column_index
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
static const struct mylite_sql_ast_node *last_column_default_node(
    const struct mylite_sql_ast_node *column_node
);
static const struct mylite_sql_ast_node *last_column_nullability_node(
    const struct mylite_sql_ast_node *column_node
);
static int validate_column_attributes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct planned_column *column
);
static bool column_attribute_counts_have_duplicates(
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
static bool column_collation_attribute_is_binary_shorthand(
    const struct mylite_sql_ast_node *collation_attribute
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
static int finalize_planned_column_now_scalar_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static int finalize_planned_column_temporal_scalar_expression_default(
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
static int finalize_planned_column_concat_scalar_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *expression
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
static bool current_timestamp_default_should_preserve_now_text(
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
static int copy_planned_date_add_current_timestamp_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *expression
);
static int copy_planned_concat_scalar_expression_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *expression
);
static int append_concat_scalar_expression_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    struct mylite_dynamic_string *string,
    const struct mylite_sql_ast_node *expression
);
static int append_concat_scalar_expression_argument_text(
    struct mylite_db *database,
    struct planned_column *column,
    struct mylite_dynamic_string *string,
    const struct mylite_sql_ast_node *argument
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
static bool integer_default_binary_operator_is_supported(enum mylite_sql_ast_operator operator_kind
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
static int decode_enum_label_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *label_node,
    char **out_text,
    size_t *out_text_length
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
static int decode_set_member_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *member_node,
    char **out_text,
    size_t *out_text_length
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
static enum approximate_type_class approximate_type_class_for_type_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node
);
static int apply_approximate_type_precision(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct approximate_type_mapping_state *state
);
static int apply_approximate_type_scaled_precision(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct approximate_type_mapping_state *state
);
static int apply_approximate_type_float_precision(
    struct mylite_db *database,
    const char *column_name,
    struct approximate_type_mapping_state *state
);
static int append_approximate_unsigned_warning_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node
);
static int assign_approximate_type_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const struct approximate_type_mapping_state *state,
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
static int append_approximate_display_width_warning(struct mylite_db *database);
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
static bool column_default_kind_has_text_value(enum mylite_catalog_column_default_kind default_kind
);
static bool column_default_kind_is_expression(enum mylite_catalog_column_default_kind default_kind);
static bool column_default_kind_materializes_value(
    enum mylite_catalog_column_default_kind default_kind
);
static bool column_descriptor_physical_type_equals(
    const struct mylite_catalog_column_descriptor *column,
    const char *physical_type
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
static bool column_descriptor_is_string_family(const struct mylite_catalog_column_descriptor *column
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
static int approximate_display_type_info_for_logical_type(
    const char *logical_type,
    const char *prefix,
    enum approximate_type_class type_class,
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
static int plan_insert_duplicate_row_scalar_assignment(
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
static bool insert_duplicate_assignment_is_same_column_values_reference(
    const struct planned_insert_duplicate_update_assignment *assignment
);
static size_t count_executable_insert_duplicate_assignments(const struct planned_insert *plan);
static size_t insert_duplicate_update_assignment_parameter_count(const struct planned_insert *plan);
static void set_insert_duplicate_arithmetic_unsupported_error(struct mylite_db *database);
static int validate_update_string_key_value(
    struct mylite_db *database,
    const struct planned_update *plan
);
static int validate_update_multiple_string_key_values(
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
static void record_inserted_explicit_auto_increment(
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
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
    const char *diagnostic_column_name,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
);
static int convert_dml_constant_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    const char *diagnostic_column_name,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    bool warn_on_trailing_space_truncation,
    bool auto_increment_null_generates,
    bool allow_constant_arithmetic,
    bool *out_handled,
    struct planned_value *out_value
);
static bool dml_value_is_constant_scalar_expression(
    const struct mylite_sql_ast_node *value_node,
    bool allow_constant_arithmetic
);
static bool dml_scalar_expression_is_constant_arithmetic(
    const struct mylite_sql_ast_node *value_node
);
static bool dml_scalar_expression_contains_unsupported_source(
    const struct mylite_sql_ast_node *value_node
);
static bool dml_scalar_expression_child_is_syntax_metadata(
    const struct mylite_sql_ast_node *value_node,
    size_t child_index,
    const struct mylite_sql_ast_node *child
);
static int convert_dml_scalar_cell_for_column(
    struct mylite_db *database,
    const struct session_scalar_cell *cell,
    const struct mylite_catalog_column_descriptor *column,
    const char *diagnostic_column_name,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    bool warn_on_trailing_space_truncation,
    struct planned_value *out_value
);
static int convert_dml_scalar_cell_text_value(
    struct mylite_db *database,
    const struct session_scalar_cell *cell,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_string_truncation_adjustment,
    bool warn_on_trailing_space_truncation,
    struct planned_value *out_value
);
static int convert_dml_scalar_cell_binary_value(
    struct mylite_db *database,
    const struct session_scalar_cell *cell,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_dml_scalar_cell_bit_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int copy_dml_scalar_cell_text(
    struct mylite_db *database,
    const struct session_scalar_cell *cell,
    const char *nul_message,
    bool allow_nul,
    char **out_text,
    size_t *out_text_length
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
static bool insert_unix_timestamp_now_node_is_admitted(const struct mylite_sql_ast_node *value_node
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
    const char *diagnostic_column_name,
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
static int materialize_scalar_expression_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool allow_nonspace_truncation,
    struct planned_value *out_value
);
static int materialize_scalar_expression_date_add_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int materialize_scalar_expression_concat_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool allow_nonspace_truncation,
    struct planned_value *out_value
);
static int materialize_scalar_expression_text_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool allow_nonspace_truncation,
    char *text,
    size_t text_length,
    struct planned_value *out_value
);
static bool parse_scalar_expression_date_add_default_text(
    const char *text,
    int64_t *out_interval,
    enum mylite_date_interval_unit *out_unit
);
static int append_scalar_expression_concat_literal_value(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const char **in_out_cursor
);
static const char *scalar_expression_concat_literal_end(const char *literal);
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
static int convert_integer_string_text(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int parse_dml_hex_literal_magnitude(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_magnitude,
    bool *out_overflow
);
static int convert_integer_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool is_negative,
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
static int decode_dml_character_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const char *unsupported_message,
    const char *nul_message,
    bool allow_nul,
    char **out_text,
    size_t *out_text_length
);
static int copy_dml_character_literal_text(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    bool is_negative,
    char **out_text,
    size_t *out_text_length
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
static int convert_decimal_string_text(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    const struct decimal_type_info *info,
    struct planned_value *out_value
);
static int convert_decimal_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool is_negative,
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
static int convert_approximate_string_text(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    const struct approximate_type_info *info,
    struct planned_value *out_value
);
static int convert_approximate_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool is_negative,
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
static bool year_string_accepts_trailing_hyphen(struct year_string_trailing_hyphen_request request);
static int convert_year_string_magnitude_by_length(
    struct year_string_magnitude_conversion conversion,
    uint32_t *out_year
);
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
static bool normalize_fractional_datetime_storage_text(
    const struct temporal_predicate_normalization_input *input,
    char *out_text
);
static bool normalize_date_only_datetime_storage_text(
    const struct temporal_predicate_normalization_input *input,
    char *out_text
);
static bool normalize_single_digit_hour_datetime_storage_text(
    const struct temporal_predicate_normalization_input *input,
    char *out_text
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
static bool normalize_time_numeric_text(
    const char *text,
    size_t text_length,
    char *out_text,
    size_t *out_text_length
);
static bool normalize_fractional_time_text(
    const char *text,
    size_t text_length,
    char *out_text,
    size_t *out_text_length
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
static int format_sysdate_text_with_precision(
    struct mylite_db *database,
    uint64_t precision,
    char *buffer,
    size_t buffer_size
);
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
static struct wall_clock_timestamp current_wall_clock_timestamp(void);
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
static int format_scaled_approximate_value_text(
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
    const char *diagnostic_column_name,
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

struct string_literal_decode_messages {
    const char *unsupported;
    const char *nul;
};

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
static int append_decoded_sql_string_literal_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    struct mylite_dynamic_string *string,
    const struct string_literal_decode_messages *messages,
    bool allow_nul
);
static int append_decoded_sql_string_literal_span(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    struct mylite_dynamic_string *string,
    const struct string_literal_decode_messages *messages,
    bool allow_nul
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
static int decode_binary_bit_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
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
static void planned_scalar_aggregate_subquery_deinit(
    struct planned_scalar_aggregate_subquery *subquery
);
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
static bool predicate_node_is_sounds_like_expression(const struct mylite_sql_ast_node *node);
static int plan_sounds_like_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_row_scalar_expression_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_row_scalar_expression_between_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
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
static int plan_where_row_scalar_predicate_literal_value(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *row_scalar_expression,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static bool row_scalar_predicate_literal_prefers_text(
    const struct planned_row_scalar_expression *expression
);
static int plan_where_scalar_hex_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct planned_value *out_value
);
static int plan_where_scalar_exact_integer_string_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct planned_value *out_value
);
static int plan_where_scalar_decimal_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static bool where_decimal_string_literal_is_supported(const char *text, size_t text_length);
static bool where_decimal_span_is_supported(const struct mylite_sql_source_span *span);
static bool predicate_row_scalar_cast_expression_decimal_scale(
    const struct mylite_sql_ast_node *expression,
    size_t *out_scale
);
static bool predicate_row_scalar_cast_expression_is_binary(
    const struct mylite_sql_ast_node *expression
);
static bool predicate_node_is_scalar_literal_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static bool predicate_node_is_supported_row_scalar_expression(
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
static bool predicate_node_is_json_valid_expression(const struct mylite_sql_ast_node *predicate_node
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
static bool predicate_node_is_substring_expression(const struct mylite_sql_ast_node *predicate_node
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
static int plan_temporal_extract_between_predicate(
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
static int plan_row_scalar_expression_is_null_predicate(
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
static int plan_scalar_literal_or_literal_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
);
static int plan_rhs_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
);
static int plan_comparison_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_scalar_count_subquery_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
);
static int plan_scalar_aggregate_subquery_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
);
static int plan_scalar_aggregate_subquery_between_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
);
static int plan_scalar_aggregate_subquery_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *scalar_subquery,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_scalar_aggregate_subquery *out_subquery,
    size_t outer_source_count
);
static bool scalar_subquery_is_column_aggregate(const struct mylite_sql_ast_node *scalar_subquery);
static int scalar_aggregate_subquery_select_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct mylite_sql_ast_node **out_expression,
    enum planned_column_aggregate_function *out_function
);
static int plan_scalar_aggregate_subquery_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum planned_column_aggregate_function function,
    const struct planned_exists_subquery *subquery,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
);
static int scalar_aggregate_subquery_source_context(
    const struct planned_exists_subquery *subquery,
    struct select_source_context *out_source_context,
    const struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count
);
static int allocate_scalar_aggregate_subquery(
    struct mylite_db *database,
    struct planned_scalar_aggregate_subquery **out_subquery
);
static int plan_scalar_count_subquery(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *scalar_subquery,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    size_t outer_source_count,
    struct planned_exists_subquery *out_subquery
);
static int validate_scalar_count_subquery_select_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list
);
static bool scalar_count_subquery_select_expression_is_supported(
    const struct mylite_sql_ast_node *expression
);
static int plan_comparison_predicate_like_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int like_escape_character_from_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    char *out_escape_character
);
static bool comparison_predicate_rhs_requires_row_scalar_value(
    const struct mylite_sql_ast_node *predicate_node,
    const struct planned_select_predicate_node *node
);
static bool predicate_value_is_integer_literal(const struct mylite_sql_ast_node *value_node);
static void planned_select_predicate_node_deinit_runtime_values(
    struct planned_select_predicate_node *node
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
    const struct exists_subquery_outer_context *outer_context,
    struct planned_exists_subquery *out_subquery
);
static int plan_exists_table_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_exists_subquery *out_subquery
);
static int plan_exists_join_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_exists_subquery *out_subquery
);
static int validate_exists_join_source(
    struct mylite_db *database,
    const struct planned_exists_subquery *subquery
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
    const struct select_predicate_plan_options *options,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
);
static int resolve_exists_column_reference_in_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    const char *column_name,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index,
    bool *out_resolved
);
static int resolve_exists_column_reference_in_joined_source(
    struct mylite_db *database,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    const char *column_name,
    const struct select_source_context *source_context,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index,
    bool *out_resolved
);
static bool exists_correlated_column_comparison_is_supported(
    const struct planned_select_predicate_node *node,
    size_t inner_source_index,
    size_t inner_source_count
);
static bool exists_source_index_is_inner(
    size_t source_index,
    size_t inner_source_index,
    size_t inner_source_count
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
static int plan_scalar_subquery_in_literal_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_in_subquery_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_statement,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    size_t outer_source_count,
    struct planned_select_predicate_node *node
);
static int plan_in_subquery(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    size_t outer_source_count,
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
    struct planned_in_subquery *subquery,
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
static bool planned_select_predicate_has_expression(const struct planned_select_predicate *predicate
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
static int bind_select_scalar_subquery_in_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_scalar_aggregate_subquery_comparison_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_scalar_aggregate_subquery_between_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_scalar_aggregate_subquery_parameters(
    sqlite3_stmt *statement,
    const struct planned_scalar_aggregate_subquery *subquery,
    int *parameter_index
);
static int bind_scalar_aggregate_subquery_or_value_parameters(
    sqlite3_stmt *statement,
    const struct planned_scalar_aggregate_subquery *subquery,
    const struct planned_value *value,
    int *parameter_index
);
static int bind_select_in_value_list_parameters(
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
static int convert_predicate_integer_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    bool is_negative,
    struct planned_value *out_value
);
static int convert_predicate_integer_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int scan_predicate_integer_string_prefix(
    const char *text,
    size_t text_length,
    uint64_t *out_magnitude,
    bool *out_is_negative,
    bool *out_has_digits
);
static int append_predicate_truncated_double_warning(
    struct mylite_db *database,
    const char *value,
    size_t value_length
);
static int convert_predicate_string_numeric_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static bool predicate_integer_string_ascii_whitespace(unsigned char byte);
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
    bool case_sensitive,
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
static int validate_single_table_dml_order_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause
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
static int plan_select_order_ordinal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct planned_select *select_plan,
    struct planned_select_order_item *out_item
);
static void apply_select_order_item_direction(
    const struct mylite_sql_ast_node *direction,
    struct planned_select_order_item *out_item
);
static bool select_order_column_needs_integer_validation(
    const struct mylite_catalog_column_descriptor *column
);
static bool order_item_list_contains_field_order_key(const struct mylite_sql_ast_node *order_items);
static bool order_item_list_contains_rand_order_key(const struct mylite_sql_ast_node *order_items);
static bool select_order_key_is_integer_ordinal(const struct mylite_sql_ast_node *order_key);
static bool select_order_key_is_field_function(const struct mylite_sql_ast_node *order_key);
static bool select_order_key_is_rand_function(const struct mylite_sql_ast_node *order_key);
static bool select_order_key_is_searched_case_expression(const struct mylite_sql_ast_node *order_key
);
static bool select_order_key_is_like_predicate(const struct mylite_sql_ast_node *order_key);
static bool select_order_key_is_collate_expression(const struct mylite_sql_ast_node *order_key);
static bool select_order_key_is_supported_joined_row_scalar_order(
    const struct mylite_sql_ast_node *order_key
);
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
static int plan_select_order_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
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
static int plan_joined_update_multiple_assignments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct select_source_context *source_context,
    struct planned_update *out_plan
);
static int plan_joined_update_multiple_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment,
    const struct select_source_context *source_context,
    struct planned_update *out_plan,
    size_t assignment_index
);
static int validate_joined_update_multiple_assignment_targets(
    struct mylite_db *database,
    const struct planned_update *plan
);
static int reject_builtin_schema_joined_update_assignment_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list
);
static int plan_joined_update_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node,
    struct planned_select_source *out_source
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
static int plan_update_scalar_aggregate_subquery_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_update *out_plan,
    bool *out_handled
);
static int plan_update_scalar_aggregate_subquery_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_update *out_plan,
    enum planned_column_aggregate_function *out_function,
    const struct mylite_sql_ast_node **out_aggregate_expression,
    const struct mylite_sql_ast_node **out_rhs_expression
);
static int plan_update_scalar_aggregate_rhs_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *rhs_expression,
    struct planned_update *out_plan
);
static int plan_update_scalar_aggregate_subquery_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    enum planned_column_aggregate_function function,
    const struct mylite_sql_ast_node *aggregate_expression,
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
static int plan_update_column_copy_assignment(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
);
static bool update_column_copy_source_target_types_are_compatible(
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column
);
static int plan_update_row_scalar_assignment(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
);
static int plan_update_assignment_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor *table_columns,
    struct planned_update *plan,
    struct planned_row_scalar_expression *out_expression,
    bool *out_planned
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
static int materialize_update_scalar_aggregate_subquery_value(
    struct mylite_db *database,
    const struct planned_update *plan,
    struct planned_value *out_value
);
static int materialize_update_scalar_aggregate_sqlite_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_update *plan,
    struct planned_value *out_value
);
static int build_update_scalar_aggregate_assignment_sql(
    const struct planned_update *plan,
    char **out_sql
);
static int append_update_scalar_aggregate_assignment_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
static int bind_update_scalar_aggregate_assignment_parameters(
    sqlite3_stmt *statement,
    const struct planned_update *plan
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
