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
static int reject_joined_select_order_ambiguity_for_literal_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context
);
static int reject_ambiguous_joined_order_references(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context
);
static int reject_ambiguous_joined_order_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context
);
static int reject_unsupported_joined_select_having(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct mylite_sql_ast_node *having_clause
);
static int reject_joined_select_having_reference_error(
    struct mylite_db *database,
    struct joined_select_having_reference_request request
);
static const struct mylite_sql_ast_node *joined_select_having_reference_node(
    const struct mylite_sql_ast_node *having_node
);
static int count_select_list_output_name_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const char *name,
    size_t *out_match_count
);
static int select_item_output_name_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_item,
    const char *name,
    bool *out_matches
);
static size_t joined_source_unqualified_column_match_count(
    const struct select_source_context *source_context,
    const char *column_name
);
static bool select_list_has_only_literal_projection_items(
    const struct mylite_sql_ast_node *select_list
);
static bool select_item_is_literal_projection(const struct mylite_sql_ast_node *select_item);
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
static int plan_joined_select_table_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node,
    struct planned_select_source *out_source
);
static int plan_joined_select_derived_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node,
    struct planned_select_source *out_source
);
static int plan_derived_select_source_child(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_statement,
    struct planned_select_source *out_source
);
static int plan_derived_compound_select_source_child(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *compound_statement,
    struct planned_select_source *out_source
);
static int validate_derived_compound_select_terms(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *terms
);
static int plan_derived_compound_select_branch(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *branch,
    struct planned_select *out_branch
);
static int validate_derived_compound_select_branch_shape(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *branch
);
static int copy_derived_select_source_columns(
    struct mylite_db *database,
    const struct planned_select *derived_plan,
    struct planned_select_source *out_source
);
static int copy_derived_row_scalar_select_source_columns(
    struct mylite_db *database,
    const struct planned_row_scalar_select *derived_plan,
    struct planned_select_source *out_source
);
static int make_derived_row_scalar_source_column(
    struct mylite_db *database,
    const struct planned_row_scalar_select_item *item,
    size_t output_index,
    struct mylite_catalog_column_descriptor *out_column
);
static int copy_derived_compound_select_source_columns(
    struct mylite_db *database,
    const struct planned_compound_select *compound_plan,
    struct planned_select_source *out_source
);
static int validate_derived_grouped_aggregate_output_columns(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *derived_plan
);
static int copy_derived_grouped_aggregate_source_columns(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *derived_plan,
    struct planned_select_source *out_source
);
static int copy_derived_grouped_projection_source_column(
    struct mylite_db *database,
    const struct planned_grouped_projection *projection,
    size_t output_index,
    struct mylite_catalog_column_descriptor *out_column
);
static int make_derived_grouped_aggregate_source_column(
    struct mylite_db *database,
    const struct planned_grouped_aggregate_item *item,
    size_t output_index,
    struct mylite_catalog_column_descriptor *out_column
);
static int copy_derived_grouped_output_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias,
    char *destination,
    size_t destination_size
);
static int validate_derived_select_output_columns(
    struct mylite_db *database,
    const struct planned_select *derived_plan
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
static int plan_joined_select_condition_with_extra_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *condition_node,
    const struct select_source_context *source_context,
    struct planned_select_join_condition *out_condition
);
static int plan_joined_select_primary_condition(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *condition_node,
    const struct select_source_context *source_context,
    struct planned_select_join_condition *out_condition
);
static int plan_joined_select_extra_on_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    struct planned_select_join_condition *out_condition
);
static bool joined_select_on_node_is_and(const struct mylite_sql_ast_node *node);
static bool joined_select_on_node_is_descriptor_equality(const struct mylite_sql_ast_node *node);
static bool joined_select_on_node_is_primary_condition(const struct mylite_sql_ast_node *node);
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
static void planned_compound_select_deinit(struct planned_compound_select *plan);
static void planned_select_source_deinit(struct planned_select_source *source);
static void planned_select_source_deinit_shallow(struct planned_select_source *source);
static void planned_select_join_condition_deinit(struct planned_select_join_condition *condition);
static void planned_select_deinit(struct planned_select *plan);
static void planned_select_deinit_shallow(struct planned_select *plan);
static struct planned_select_source *planned_select_find_deepest_derived_source(
    struct planned_select *plan
);
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
static bool select_list_is_descriptor_projection(const struct mylite_sql_ast_node *select_list);
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
static bool count_having_comparison_uses_count_star(const struct mylite_sql_ast_node *comparison);
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
static int plan_row_scalar_select_derived_source(
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
static int plan_row_scalar_select_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_order_by_field,
    struct planned_row_scalar_select *out_plan
);
static int plan_row_scalar_select_order_ordinals(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    size_t selected_item_count,
    struct planned_select_order *out_order,
    bool *out_handled
);
static int plan_row_scalar_select_order_ordinal_item_and_append(
    struct mylite_db *database,
    struct select_order_ast_item_nodes item_nodes,
    size_t selected_item_count,
    struct planned_select_order *out_order
);
static int parse_select_order_ordinal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    size_t selected_item_count,
    size_t *out_ordinal
);
static int plan_row_scalar_select_order_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct planned_row_scalar_select *select_plan,
    struct planned_select_order *out_order,
    bool *out_handled
);
static int plan_row_scalar_select_order_alias_item(
    struct mylite_db *database,
    struct select_order_ast_item_nodes item_nodes,
    const struct planned_row_scalar_select *select_plan,
    struct planned_select_order *out_order,
    bool *out_handled
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
static bool row_scalar_select_where_is_exists_filter(const struct mylite_sql_ast_node *where_clause
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
static int populate_row_scalar_source_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_select_item *item,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_special_source_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_integer_expression_result_column_descriptor(
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_abs_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_scalar_subquery_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_concat_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static void populate_row_scalar_year_result_column_descriptor(
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_conversion_source_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_coalesce_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_searched_case_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int row_scalar_source_string_display_length(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_matched,
    uint64_t *out_display_length
);
static int row_scalar_source_binary_display_length(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_matched,
    uint64_t *out_display_length
);
static int scalar_binary_literal_byte_count(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    size_t *out_byte_count
);
static bool row_scalar_source_expression_is_string_literal(
    const struct mylite_sql_ast_node *expression
);
static int row_scalar_integer_expression_token_width(
    const struct mylite_sql_ast_node *expression,
    bool *out_matched,
    uint64_t *out_width
);
static int row_scalar_integer_width_enter_frame(
    struct row_scalar_integer_width_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int row_scalar_integer_width_finish_unary_frame(struct row_scalar_integer_width_stack *stack
);
static int row_scalar_integer_width_finish_binary_frame(struct row_scalar_integer_width_stack *stack
);
static int row_scalar_integer_width_push_frame(
    struct row_scalar_integer_width_stack *stack,
    struct row_scalar_integer_width_frame frame
);
static int row_scalar_integer_width_push_result(
    struct row_scalar_integer_width_stack *stack,
    struct row_scalar_integer_width_result result
);
static int row_scalar_integer_width_pop_result(
    struct row_scalar_integer_width_stack *stack,
    struct row_scalar_integer_width_result *out_result
);
static void row_scalar_integer_width_stack_deinit(struct row_scalar_integer_width_stack *stack);
static bool row_scalar_expression_is_date_target_conversion(
    const struct mylite_sql_ast_node *expression
);
static bool row_scalar_metadata_byte_is_identifier(char byte);
static bool row_scalar_metadata_text_is_integer(const char *text, size_t text_length);
static bool row_scalar_descriptor_has_known_type(
    const struct mylite_result_column_descriptor *descriptor
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
static void populate_row_scalar_digest_result_column_descriptor(
    const struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_compress_result_column_descriptor(
    struct mylite_db *database,
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
static int plan_grouped_aggregate_derived_source(
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
static int resolve_grouped_aggregate_group_temporal_expression(
    struct mylite_db *database,
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
static int resolve_grouped_select_alias_group_temporal_expression(
    struct mylite_db *database,
    const struct grouped_alias_group_resolution *resolution,
    const struct mylite_sql_ast_node *matched_expression,
    const struct mylite_sql_ast_node *matched_alias,
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
static bool grouped_aggregate_group_clause_has_rollup(const struct mylite_sql_ast_node *group_clause
);
static int validate_grouped_aggregate_group_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static bool grouped_expression_is_temporal_group_key(const struct mylite_sql_ast_node *expression);
static int plan_grouped_temporal_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
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
static int append_grouped_projection_row_scalar_expression(
    struct mylite_db *database,
    struct planned_grouped_aggregate *plan,
    struct planned_row_scalar_expression *row_scalar_expression,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
);
static int plan_grouped_aggregate_temporal_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int plan_grouped_aggregate_literal_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int plan_grouped_aggregate_integer_arithmetic_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int validate_grouped_projection_row_scalar_expression(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct planned_row_scalar_expression *expression
);
static int validate_grouped_projection_row_scalar_column(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct planned_row_scalar_expression *expression
);
static int plan_grouped_aggregate_relaxed_row_scalar_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static bool grouped_expression_is_relaxed_row_scalar_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static bool grouped_relaxed_row_scalar_projection_is_allowed(
    const struct planned_grouped_aggregate *plan,
    const struct planned_row_scalar_expression *expression
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
static bool grouped_row_scalar_projection_is_allowed(
    const struct planned_grouped_aggregate *plan,
    const struct planned_row_scalar_expression *expression
);
static bool planned_grouped_row_scalar_expressions_match(
    const struct planned_row_scalar_expression *left,
    const struct planned_row_scalar_expression *right
);
static bool grouped_plan_has_temporal_key_on_column(
    const struct planned_grouped_aggregate *plan,
    enum mylite_temporal_extract_kind temporal_extract_kind,
    const struct planned_row_scalar_expression *column_argument
);
static bool planned_grouped_temporal_key_uses_week_mode_one(
    const struct planned_row_scalar_expression *expression
);
static bool planned_grouped_row_scalar_columns_match(
    const struct planned_row_scalar_expression *left,
    const struct planned_row_scalar_expression *right
);
static bool planned_grouped_row_scalar_value_arguments_match(
    const struct planned_row_scalar_expression *left,
    const struct planned_row_scalar_expression *right
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
static int plan_grouped_sum_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate_item *out_item
);
static int plan_grouped_sum_row_scalar_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
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
static int plan_grouped_having_row_scalar_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    enum planned_grouped_having_operand *out_operand
);
static int validate_grouped_having_row_scalar_expression(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct planned_row_scalar_expression *expression
);
static int validate_grouped_having_row_scalar_column(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct planned_row_scalar_expression *expression
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
    struct planned_grouped_aggregate *out_plan,
    struct planned_select_order_item *out_item,
    bool *out_uses_aggregate,
    size_t *out_aggregate_index
);
static int plan_grouped_aggregate_order_ast_item_and_append(
    struct mylite_db *database,
    struct select_order_ast_item_nodes item_nodes,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int validate_grouped_order_row_scalar_expression(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct planned_row_scalar_expression *expression
);
static int validate_grouped_order_row_scalar_column(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct planned_row_scalar_expression *expression
);
static int append_planned_grouped_order_item(
    struct planned_grouped_aggregate *plan,
    struct planned_select_order_item item,
    bool uses_aggregate,
    size_t aggregate_index
);
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
static int plan_count_derived_table_source(
    struct mylite_db *database,
    const struct planned_count_source_nodes *nodes,
    struct planned_count *out_plan
);
static int plan_count_derived_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_count *out_plan
);
static int plan_count_derived_compound_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_count *out_plan
);
static int plan_count_derived_compound_branch(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *branch,
    struct planned_count_derived_compound_branch *out_branch
);
static void planned_count_derived_compound_deinit(struct planned_count_derived_compound *compound);
static void planned_count_derived_compound_branch_deinit(
    struct planned_count_derived_compound_branch *branch
);
static int validate_count_order_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
);
static int validate_count_order_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
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
static void apply_count_limit_to_result(mylite_result *result, const struct planned_count *plan);
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
static int plan_column_aggregate_without_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *aggregate_expression,
    enum planned_column_aggregate_function function,
    const struct mylite_sql_ast_node *where_clause,
    struct planned_column_aggregate *out_plan
);
static void planned_column_aggregate_deinit(struct planned_column_aggregate *plan);
static enum planned_column_aggregate_function column_aggregate_function_from_expression(
    const struct mylite_sql_ast_node *expression
);
static enum planned_column_aggregate_function column_aggregate_function_from_projection_expression(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node **out_aggregate_expression,
    enum mylite_sql_ast_operator *out_operator,
    const struct mylite_sql_ast_node **out_rhs_expression
);
static enum planned_column_aggregate_function select_list_column_aggregate_function(
    const struct mylite_sql_ast_node *select_list
);
static bool column_aggregate_argument_is_row_scalar_expression(
    const struct mylite_sql_ast_node *function
);
static bool column_aggregate_function_is_bitwise(enum planned_column_aggregate_function function);
static bool column_aggregate_function_is_min_max(enum planned_column_aggregate_function function);
static const char *column_aggregate_single_item_error(
    enum planned_column_aggregate_function function
);
static const char *column_aggregate_optional_clause_error(
    enum planned_column_aggregate_function function
);
static const char *column_aggregate_source_error(enum planned_column_aggregate_function function);
static const char *column_aggregate_column_error(enum planned_column_aggregate_function function);
static const char *column_aggregate_integer_error(enum planned_column_aggregate_function function);
static int validate_column_aggregate_descriptor_column(
    struct mylite_db *database,
    enum planned_column_aggregate_function aggregate_function,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_column_aggregate_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    enum planned_column_aggregate_function aggregate_function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
);
static int plan_column_aggregate_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_column_aggregate *out_plan
);
static bool column_aggregate_sum_argument_is_row_scalar_expression(
    const struct mylite_sql_ast_node *function
);
static bool column_aggregate_sum_argument_is_string_length_expression(
    const struct mylite_sql_ast_node *argument
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
static void apply_column_aggregate_limit_to_result(
    mylite_result *result,
    const struct planned_column_aggregate *plan
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
static int set_grouped_aggregate_found_row_count(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    mylite_result *result
);
static int read_grouped_aggregate_found_row_count(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    int64_t *out_count
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
static bool select_statement_has_only_scalar_projection_metadata_clauses(
    const struct mylite_sql_ast_node *statement
);
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
static int scalar_concat_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_concat_function_scalar_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cells,
    char **owned_texts,
    struct mylite_string_concat_argument *arguments,
    size_t argument_count,
    size_t *inout_next_argument
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
    char **out_text,
    size_t *out_text_length
);
static bool concat_operator_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression
);
static bool concat_operator_scalar_argument_is_nested_scalar_value(
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
static int validate_supported_temporal_fractional_precision(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *subject_name,
    uint64_t *out_precision
);
static int sysdate_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
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
static int apply_scalar_placeholder_cast(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum scalar_placeholder_cast_target default_target,
    bool convert_syntax,
    struct session_scalar_cell *inout_cell
);
static int parse_scalar_placeholder_cast_info(
    const struct mylite_sql_ast_node *expression,
    enum scalar_placeholder_cast_target default_target,
    bool convert_syntax,
    struct scalar_placeholder_cast_info *out_info
);
static int scalar_placeholder_target_span(
    const struct mylite_sql_ast_node *expression,
    bool convert_syntax,
    struct mylite_sql_source_span *out_span
);
static int apply_scalar_placeholder_text_length(
    struct mylite_db *database,
    const struct scalar_placeholder_cast_info *info,
    struct session_scalar_cell *inout_cell
);
static int apply_scalar_placeholder_date(
    struct mylite_db *database,
    struct session_scalar_cell *inout_cell
);
static int apply_scalar_placeholder_time(
    struct mylite_db *database,
    struct session_scalar_cell *inout_cell
);
static int apply_scalar_placeholder_datetime(
    struct mylite_db *database,
    struct session_scalar_cell *inout_cell
);
static int apply_scalar_placeholder_decimal(
    struct mylite_db *database,
    const struct scalar_placeholder_cast_info *info,
    struct session_scalar_cell *inout_cell
);
static int apply_scalar_placeholder_json(
    struct mylite_db *database,
    struct session_scalar_cell *inout_cell
);
static int assign_scalar_cell_text(
    struct mylite_db *database,
    struct session_scalar_cell *inout_cell,
    const char *text,
    size_t text_length
);
static int assign_scalar_cell_owned_text(
    struct mylite_db *database,
    struct session_scalar_cell *inout_cell,
    char *text,
    size_t text_length
);
static size_t scalar_cell_text_length(const struct session_scalar_cell *cell);
static void trim_scalar_span(struct mylite_sql_source_span *span);
static bool scalar_span_consume_keyword(struct mylite_sql_source_span *span, const char *keyword);
static bool scalar_span_parse_optional_length(
    struct mylite_sql_source_span *span,
    size_t *out_length
);
static bool scalar_span_parse_decimal_scale(struct mylite_sql_source_span *span, size_t *out_scale);
static bool scalar_span_parse_unsigned(
    const struct mylite_sql_source_span *span,
    size_t *inout_index,
    size_t *out_value
);
static bool scalar_span_keyword_at(
    const char *text,
    size_t text_length,
    size_t index,
    const char *keyword,
    size_t *out_end
);
static bool scalar_span_is_identifier_byte(unsigned char byte);
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
    struct scalar_arithmetic_value_stack *value_stack,
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
static int row_constructor_scalar_value(
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
static int evaluate_scalar_comparison_apply_not_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack
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
    const struct mylite_sql_ast_node *where_clause,
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
static enum show_variables_where_truth show_catalog_null_literal_truth(
    enum mylite_sql_ast_operator operator_kind,
    const char *left
);
static enum show_variables_where_truth show_catalog_null_left_truth(
    enum mylite_sql_ast_operator operator_kind
);
static int show_catalog_where_truth_from_comparison(
    struct mylite_db *database,
    struct show_catalog_where_comparison comparison,
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
static bool is_scalar_projection_select_item_expression(const struct mylite_sql_ast_node *expression
);
static bool is_scalar_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_collate_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_projection_expression_without_collate(
    const struct mylite_sql_ast_node *expression
);
static bool is_row_constructor_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_user_variable_assignment_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_last_insert_id_set_projection_expression(const struct mylite_sql_ast_node *expression
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
static bool is_digest_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_hex_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_base64_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_compression_random_projection_expression(const struct mylite_sql_ast_node *expression
);
static bool scalar_projection_expression_contains_compression_random_function(
    const struct mylite_sql_ast_node *expression
);
static bool is_unhex_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_weight_string_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_uuid_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_uuid_value_projection_argument_supported(const struct mylite_sql_ast_node *expression
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
static bool is_charset_collation_projection_expression(const struct mylite_sql_ast_node *expression
);
static bool is_string_metadata_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_conversion_projection_expression(const struct mylite_sql_ast_node *expression
);
static bool is_cast_binary_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_cast_convert_basic_projection_expression(const struct mylite_sql_ast_node *expression
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
static bool is_json_introspection_projection_expression(const struct mylite_sql_ast_node *expression
);
static bool is_json_unquote_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_json_quote_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_json_construction_projection_expression(const struct mylite_sql_ast_node *expression
);
static bool is_json_set_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_value_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_arithmetic_projection_expression(const struct mylite_sql_ast_node *expression
);
static bool is_scalar_bitwise_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_logical_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_comparison_projection_expression(const struct mylite_sql_ast_node *expression
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
static bool is_scalar_sounds_like_operand_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_regexp_comparison_operand_expression(
    const struct mylite_sql_ast_node *expression
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
static bool is_scalar_value_projection_attempt_operand(const struct mylite_sql_ast_node *expression
);
static bool is_scalar_numeric_function_attempt_operand(const struct mylite_sql_ast_node *expression
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
    const struct mylite_processlist_session_snapshot *session,
    mylite_result *result
);
static int format_processlist_user_host(
    struct mylite_db *database,
    const char *identity,
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
