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
static int execute_insert_from_plan(
    struct mylite_db *database,
    struct planned_insert *plan,
    mylite_result *result
);
static int rebase_insert_auto_increment_after_write_lock(
    struct mylite_db *database,
    struct planned_insert *plan
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
    struct planned_insert_select *out_plan
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
