#ifndef MYLITE_EXECUTION_ROW_SCALAR_SQL_ONLY
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
static uint64_t character_set_max_bytes_per_character(const char *character_set_name);
static uint64_t result_metadata_collation_max_bytes_per_character(const char *collation_name);
static uint64_t result_metadata_display_length_cap(uint64_t display_length);
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
static int load_show_table_status_values(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    struct table_status_values *status
);
static void fill_show_table_status_row_values(
    const struct mylite_catalog_table_descriptor *table,
    const struct table_status_values *status,
    const char **values
);
static int show_table_status_row_matches(
    struct show_table_status_context *context,
    const struct mylite_catalog_table_descriptor *table,
    const struct table_status_values *status,
    const char *values[show_table_status_result_column_count],
    bool *out_matches
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
static size_t mysql_system_table_secondary_index_sequence(
    const struct mylite_execution_catalog_mysql_system_table *definition,
    size_t secondary_index_index
);
static bool mysql_system_table_secondary_index_name_seen_before(
    const struct mylite_execution_catalog_mysql_system_table *definition,
    size_t secondary_index_index
);
static const char *mysql_system_table_index_nullable_value(
    const struct mylite_execution_catalog_column_definition *column
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
static const char *mysql_system_table_primary_key_index_type(
    const struct mylite_execution_catalog_mysql_system_table *definition
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
static int index_part_display_prefix_value(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    const struct loaded_index_part *part,
    char *prefix_text,
    size_t prefix_text_size,
    const char **out_prefix_value
);
static int index_part_prefix_matches_full_column_length(
    struct mylite_db *database,
    const struct loaded_index_part *part,
    bool *out_matches
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
    const struct planned_create_table *plan,
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
static bool planned_column_uses_string_key_collation(
    const struct planned_create_table *plan,
    const struct planned_column *column,
    bool include_text_family
);
static bool column_descriptor_uses_string_key_collation(
    const struct mylite_catalog_column_descriptor *column,
    bool include_text_family
);
static bool string_key_collation_name_is_binary(const char *name);
static int append_string_key_collation_sql(struct mylite_dynamic_string *string);
static int append_named_string_key_collation_sql(
    struct mylite_dynamic_string *string,
    const char *collation_name
);
int append_select_projection_column_sql(
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
    uint64_t generation,
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
static bool alter_table_modify_changes_binary_to_string(
    const struct planned_alter_table_modify_column *plan,
    const struct mylite_catalog_column_descriptor *column
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
static int materialize_alter_table_modify_binary_to_text_value(
    struct mylite_db *database,
    sqlite3_stmt *select_statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    struct planned_value *out_value
);
static int materialize_alter_table_modify_decimal_value(
    struct mylite_db *database,
    sqlite3_stmt *select_statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    struct planned_value *out_value
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
int build_insert_sql(const struct planned_insert *plan, char **out_sql);
int append_insert_column_names(
    struct mylite_dynamic_string *string,
    const struct planned_insert *plan
);
int append_insert_parameters(struct mylite_dynamic_string *string, size_t column_count);
int append_numbered_parameter(struct mylite_dynamic_string *string, size_t parameter_index);
int append_size_literal(struct mylite_dynamic_string *string, size_t value);
int append_uint64_literal(struct mylite_dynamic_string *string, uint64_t value);
int build_insert_select_temp_table_name(
    const struct mylite_db *database,
    char *destination,
    size_t destination_size
);
int build_insert_select_materialize_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
);
bool row_scalar_expression_uses_string_collation(
    const struct planned_row_scalar_expression *expression
);
int build_insert_select_validation_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
);
int build_drop_temp_table_sql(const char *temporary_table_name, char **out_sql);
bool find_insert_select_target_position(
    const struct planned_insert_select *plan,
    size_t column_index,
    size_t *out_target_position
);
int build_create_table_select_insert_sql(
    const struct planned_create_table_select *plan,
    const char *physical_name,
    char **out_sql
);
int build_select_sql(const struct planned_select *plan, char **out_sql);
int build_row_scalar_select_sql(
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
#endif

#ifdef MYLITE_EXECUTION_ROW_SCALAR_SQL_IMPLEMENTATION
static int append_row_scalar_collate_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static const char *row_scalar_sql_collation_name(const char *collation_name);
static bool row_scalar_mysql_collation_is_binary_sensitive(const char *collation_name);
static bool row_scalar_mysql_collation_is_binary(const char *collation_name);
static bool row_scalar_mysql_collation_is_case_sensitive(const char *collation_name);
static bool text_has_ascii_case_insensitive_suffix(const char *text, const char *suffix);
static int append_row_scalar_sum_column_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression
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
static int append_row_scalar_avg_window_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_aggregate_window_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_window_function_call_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    const char *function_name,
    bool count_star,
    size_t *next_parameter
);
static int append_row_scalar_window_over_clause_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression
);
static int append_row_scalar_window_function_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *argument,
    size_t *next_parameter
);
static int append_row_scalar_window_frame_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression
);
static int append_row_scalar_window_frame_bound_sql(
    struct mylite_dynamic_string *string,
    const struct planned_window_frame_bound *bound
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
static int append_row_scalar_non_control_flow_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_integer_arithmetic_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_bitwise_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static const char *row_scalar_bitwise_sql_name(
    const struct planned_row_scalar_expression *expression
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
static int append_row_scalar_integer_arithmetic_column_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression
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
static int append_row_scalar_elt_expression_sql(
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
static int append_row_scalar_date_format_numeric_comparison_sql(
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
static int append_row_scalar_addtime_subtime_expression_sql(
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
static int append_row_scalar_json_schema_expression_sql(
    struct mylite_dynamic_string *string,
    const char *function_name,
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
static int append_row_scalar_json_member_of_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_length_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_depth_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_storage_expression_sql(
    struct mylite_dynamic_string *string,
    const char *sqlite_function_name,
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
static int append_row_scalar_json_pretty_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_set_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_merge_preserve_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int json_mutation_sqlite_function_name(
    const struct planned_row_scalar_expression *expression,
    const char **out_function_name
);
static int append_row_scalar_json_merge_function_call_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t argument_count,
    size_t *next_parameter
);
static int append_row_scalar_json_remove_arguments_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_json_merge_arguments_sql(
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
static int append_row_scalar_nested_searched_case_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_like_predicate_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_leaf_like_predicate_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_logical_condition_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_leaf_logical_condition_expression_sql(
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
static int append_row_scalar_control_flow_leaf_condition_argument_sql(
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
static int append_row_scalar_spatial_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static int append_row_scalar_numeric_function_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static const char *row_scalar_numeric_function_sql_name(
    enum planned_row_scalar_numeric_function_kind kind
);
static int append_row_scalar_base_conversion_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static const char *row_scalar_base_conversion_sql_name(
    enum planned_row_scalar_base_conversion_kind kind
);
static int append_row_scalar_numeric_extra_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static const char *row_scalar_numeric_extra_sql_name(enum planned_row_scalar_numeric_extra_kind kind
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
#endif

#ifndef MYLITE_EXECUTION_ROW_SCALAR_SQL_ONLY
int build_select_found_rows_sql(const struct planned_select *plan, char **out_sql);
int build_count_sql(const struct planned_count *plan, char **out_sql);
int build_column_aggregate_sql(
    const struct planned_column_aggregate_select *plan,
    char **out_sql
);
int append_column_aggregate_select_item_sql(
    struct mylite_dynamic_string *string,
    const struct planned_column_aggregate *item,
    size_t *next_parameter
);
int build_grouped_aggregate_sql(
    const struct planned_grouped_aggregate *plan,
    char **out_sql
);
int build_grouped_aggregate_rollup_sql(
    const struct planned_grouped_aggregate *plan,
    char **out_sql
);
int build_grouped_aggregate_found_rows_sql(
    const struct planned_grouped_aggregate *plan,
    char **out_sql
);
int append_select_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
);
int append_select_predicate_expression_sql_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
);

enum quantified_subquery_exists_kind {
    QUANTIFIED_SUBQUERY_EXISTS_MATCH = 0,
    QUANTIFIED_SUBQUERY_EXISTS_UNKNOWN = 1,
    QUANTIFIED_SUBQUERY_EXISTS_FALSE = 2,
};

int append_predicate_sql_work_node(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    size_t node_index
);
int append_predicate_sql_work_item(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    struct predicate_sql_work_item item
);
int append_select_order_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_order *order,
    size_t *next_parameter
);
int append_select_order_direction_sql(
    struct mylite_dynamic_string *string,
    enum planned_select_order_direction direction
);
int append_select_source_alias(struct mylite_dynamic_string *string, size_t source_index);
int append_select_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select *plan,
    size_t *next_parameter
);
static int append_json_table_source_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_source *source,
    size_t source_index,
    size_t *next_parameter
);
int append_select_limit_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_limit *limit
);
int build_delete_sql(const struct planned_delete *plan, char **out_sql);
int append_joined_delete_rowid_subquery_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter
);
int build_update_sql(const struct planned_update *plan, char **out_sql);
bool planned_update_column_has_auto_update(
    const struct planned_update *plan,
    const struct mylite_catalog_column_descriptor *column
);
size_t planned_update_auto_update_column_count(const struct planned_update *plan);
int append_single_update_matched_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
);
int append_joined_update_matched_target_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
bool planned_update_has_row_filter(const struct planned_update *plan);
int append_update_row_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
int append_update_rowid_limited_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
int append_joined_update_rowid_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
int append_joined_update_rowid_subquery_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
int append_joined_update_target_rowid_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
);
int append_joined_update_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
int append_update_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
int append_update_assignment_row_scalar_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
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
const char *comparison_operator_sql(enum mylite_sql_ast_operator operator_kind);
#endif
