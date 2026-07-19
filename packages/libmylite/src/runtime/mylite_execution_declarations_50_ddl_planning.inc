static bool statement_result_is_select(
    const struct mylite_sql_ast_node *statement,
    const mylite_result *result
);

static int plan_create_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool is_temporary_target,
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
    bool is_temporary_target,
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
static int execute_create_or_alter_view_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int plan_create_view(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_create_view *out_plan
);
static int plan_create_view_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_create_view *out_plan
);
static int apply_create_view_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option,
    struct planned_create_view *plan
);
static int copy_view_option_keyword(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size
);
static int copy_view_definer_account(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *account,
    char *destination,
    size_t destination_size
);
static int copy_view_definer_user(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size
);
static int copy_view_definer_host(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size
);
static int copy_view_account_component_span(
    struct mylite_db *database,
    const char *text,
    size_t text_size,
    char *destination,
    size_t destination_size
);
static int append_view_account(
    struct mylite_db *database,
    const char *user,
    const char *host,
    char *destination,
    size_t destination_size
);
static void copy_view_check_option(
    const struct mylite_sql_ast_node *check_option_node,
    char *destination,
    size_t destination_size
);
static bool view_check_option_span_has_keyword(
    const struct mylite_sql_ast_node *check_option_node,
    const char *keyword
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
    const struct mylite_sql_ast_node *statement,
    struct planned_create_view *plan
);
static int copy_create_view_inferred_column_names(
    struct mylite_db *database,
    struct planned_create_view *plan
);
static int copy_create_view_explicit_column_names(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    struct planned_create_view *plan
);
static size_t create_view_explicit_column_count(const struct mylite_sql_ast_node *column_list);
static int copy_create_view_select_column_names(
    struct mylite_db *database,
    struct planned_create_view *plan
);
static int copy_create_view_inferred_column_name(
    struct mylite_db *database,
    struct planned_create_view *plan,
    size_t column_index,
    char **out_name
);
static void set_view_column_count_error(struct mylite_db *database);
static void set_view_check_option_non_updatable_error(
    struct mylite_db *database,
    const struct planned_create_view *plan
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
static int append_create_view_column_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_view *plan
);
static int append_create_view_check_option_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_view *plan
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
    const struct mylite_catalog_column_descriptor *const *columns,
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
    const char *table_name,
    bool is_temporary_target
);
static int validate_create_table_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    const char *table_name,
    bool is_temporary_target
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
static int apply_table_min_rows_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
);
static int apply_table_max_rows_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
);
static int apply_table_avg_row_length_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
);
static int apply_table_delay_key_write_option(
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
static bool create_table_placeholder_option_is_valid(const struct mylite_sql_ast_node *table_option
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
    const char *table_name,
    bool is_temporary_target
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
static int copy_binary_collation_for_charset(
    struct mylite_db *database,
    const char *charset_name,
    char *binary_collation,
    size_t binary_collation_size
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
    int64_t table_id,
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
    uint64_t generation
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
static int plan_alter_table_add_column_for_multi_action(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_add_column *out_plan
);
static int plan_alter_table_add_column_with_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool defer_auto_increment_key_validation,
    struct planned_alter_table_add_column *out_plan
);
static int validate_alter_table_add_column_target(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan
);
static int reject_unsupported_alter_add_column_planned_column(
    struct mylite_db *database,
    const struct planned_column *column
);
static int validate_alter_table_add_column_auto_increment_definition(
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
static int add_persistent_column_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan
);
static int add_temporary_column_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan
);
static int build_temporary_added_column_descriptor(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan,
    struct mylite_catalog_column_descriptor *out_column
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
    struct add_inline_key_descriptor_request request,
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
static size_t planned_create_table_hash_index_warning_count(const struct planned_create_table *plan
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
static int plan_alter_table_modify_column_for_multi_action(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_modify_column *out_plan
);
static int plan_alter_table_modify_column_with_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool defer_auto_increment_key_validation,
    struct planned_alter_table_modify_column *out_plan
);
static int plan_alter_table_change_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_modify_column *out_plan
);
static int plan_alter_table_change_column_for_multi_action(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_modify_column *out_plan
);
static int plan_alter_table_change_column_with_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool defer_auto_increment_key_validation,
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
static bool table_charset_option_is_default_keyword(const struct mylite_sql_ast_node *charset_option
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
static int plan_alter_table_storage_statistics(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_storage_statistics *out_plan
);
static int alter_table_storage_statistics_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_storage_statistics *plan
);
static int alter_table_storage_statistics_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_storage_statistics *plan
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
static int resolve_alter_table_modify_target_table(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *out_plan
);
static int finalize_alter_table_modify_column_definition(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *out_plan
);
static int reject_alter_table_modify_restricted_tables(
    struct mylite_db *database,
    const struct planned_alter_table_modify_column *plan
);
static int resolve_alter_table_modify_original_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_modify_column *out_plan
);
static int load_alter_table_modify_indexes(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_modify_column *out_plan
);
static int complete_alter_table_modify_inline_key_columns(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
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
static int validate_alter_table_modify_auto_increment(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const struct planned_alter_table_modify_column *plan
);
static bool alter_table_modify_auto_increment_has_leading_key(
    const struct planned_alter_table_modify_column *plan
);
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
static bool modify_column_numeric_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool modify_column_string_to_integer_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool modify_column_integer_to_string_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool column_descriptor_is_numeric_replacement_family(
    const struct mylite_catalog_column_descriptor *column
);
static bool planned_column_is_numeric_replacement_family(const struct planned_column *column);
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
static bool modify_column_char_varchar_widening_metadata_only(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool parse_char_varchar_descriptor_length_literal(
    const char *logical_type,
    size_t prefix_length,
    size_t *out_length
);
static bool modify_column_name_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static void planned_alter_table_modify_column_deinit(struct planned_alter_table_modify_column *plan
);
static int alter_table_modify_column_from_plan(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *plan
);
static int modify_temporary_column_from_plan(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *plan
);
static int replace_temporary_column_descriptor(
    struct mylite_db *database,
    const struct planned_alter_table_modify_column *plan
);
static int alter_table_modify_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    struct planned_alter_table_modify_column *plan,
    bool *out_physical_schema_changed
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
static int validate_existing_approximate_for_column(
    struct mylite_db *database,
    double value,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number
);
static int validate_existing_decimal_text_for_column(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number
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
static int own_temporary_column_descriptor_strings(
    struct mylite_db *database,
    struct mylite_catalog_column_descriptor *column
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
