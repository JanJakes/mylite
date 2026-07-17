static int append_show_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const char *label
);
static int append_show_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const char *const *labels,
    const struct show_result_column_definition *definitions,
    size_t column_count
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
static int information_schema_try_execute_tables_exact_base_table_query(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_catalog_filter *filter,
    bool apply_sql_select_limit,
    mylite_result **out_result,
    bool *out_handled
);
static int information_schema_tables_exact_base_table_query_is_supported(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_catalog_filter *filter,
    bool *out_supported
);
static int information_schema_exact_table_lookup_predicate_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_catalog_filter *filter,
    const struct mylite_sql_ast_node *predicate,
    bool *out_matches
);
static int information_schema_exact_table_lookup_comparison_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_catalog_filter *filter,
    const struct mylite_sql_ast_node *predicate,
    bool *out_matches
);
static bool information_schema_exact_table_lookup_value_matches(
    const struct information_schema_query *query,
    const struct information_schema_catalog_filter *filter,
    size_t column_index,
    const struct information_schema_predicate_value *value
);
static int information_schema_append_fast_table_existence_result(
    struct mylite_db *database,
    const struct information_schema_query *query,
    bool table_exists,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int information_schema_append_fast_table_existence_count_row(
    struct mylite_db *database,
    const struct information_schema_query *query,
    bool table_exists,
    mylite_result *result
);
static int information_schema_append_fast_table_existence_literal_row(
    struct mylite_db *database,
    const struct information_schema_query *query,
    mylite_result *result
);
static int build_information_schema_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_execution_catalog_table_definition *definition,
    struct information_schema_row_set *out_rows
);
static int build_information_schema_rows_with_filter(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct mylite_execution_catalog_table_definition *definition,
    const struct information_schema_catalog_filter *filter,
    mylite_result *stream_result,
    struct information_schema_result_projection_buffers *stream_buffers,
    struct information_schema_row_set *out_rows
);
static int information_schema_row_set_init(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct mylite_execution_catalog_table_definition *definition,
    mylite_result *stream_result,
    struct information_schema_result_projection_buffers *stream_buffers
);
static void information_schema_row_set_deinit(struct information_schema_row_set *rows);
static int append_information_schema_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct information_schema_catalog_filter *filter
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
    struct information_schema_row_set *rows,
    const struct information_schema_catalog_filter *filter
);
static int try_append_information_schema_exact_catalog_table(
    struct information_schema_catalog_context *context,
    bool *out_handled
);
static int information_schema_catalog_filter_init_from_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    struct information_schema_catalog_filter *out_filter
);
static bool information_schema_catalog_filter_resolve_schema_table_columns(
    const struct mylite_execution_catalog_table_definition *definition,
    size_t *out_schema_column_index,
    size_t *out_table_column_index
);
static void information_schema_catalog_filter_deinit(
    struct information_schema_catalog_filter *filter
);
static int information_schema_catalog_filter_collect_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate,
    struct information_schema_catalog_filter *filter
);
static int information_schema_catalog_filter_collect_comparison(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate,
    struct information_schema_catalog_filter *filter
);
static int information_schema_catalog_filter_set_value(
    struct information_schema_catalog_filter *filter,
    size_t column_index,
    struct information_schema_predicate_value *value
);
static bool information_schema_catalog_filter_allows_system_rows(
    const struct mylite_execution_catalog_table_definition *definition,
    const struct information_schema_catalog_filter *filter
);
static bool information_schema_catalog_filter_allows_user_catalog_rows(
    const struct mylite_execution_catalog_table_definition *definition,
    const struct information_schema_catalog_filter *filter
);
static bool information_schema_catalog_filter_allows_schema(
    const struct mylite_execution_catalog_table_definition *definition,
    const struct information_schema_catalog_filter *filter,
    const char *schema_name
);
static bool information_schema_catalog_filter_allows_table(
    const struct mylite_execution_catalog_table_definition *definition,
    const struct information_schema_catalog_filter *filter,
    const char *table_name
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
static int reserve_information_schema_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    size_t required_capacity
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
static int append_information_schema_processlist_session_row(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_processlist_session_snapshot *session,
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
static int table_status_auto_increment_display_value(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
static int table_status_auto_increment_next_value(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    char *buffer,
    size_t buffer_size,
    const char **out_value
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
static int format_scalar_expression_default_display_text(
    const struct mylite_catalog_column_descriptor *column,
    char *default_text,
    size_t default_text_size,
    const char **out_default_text
);
static int format_concat_scalar_expression_default_display_text(
    const char *source,
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
static int append_information_schema_columns_decimal_numeric_metadata(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const struct information_schema_columns_numeric_metadata_buffers *buffers
);
static int append_information_schema_columns_bit_numeric_metadata(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const struct information_schema_columns_numeric_metadata_buffers *buffers
);
static int append_information_schema_columns_approximate_numeric_metadata(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const struct information_schema_columns_numeric_metadata_buffers *buffers
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
static int escape_information_schema_check_clause(
    struct mylite_db *database,
    const char *check_clause,
    char **out_check_clause
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
static int information_schema_make_result_column_descriptor(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t projection_index,
    struct mylite_result_column_descriptor *out_descriptor
);
static int information_schema_make_source_column_descriptor(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t projection_index,
    struct mylite_result_column_descriptor *out_descriptor
);
static int information_schema_initialize_catalog_column_descriptor(
    struct mylite_db *database,
    const struct mylite_execution_catalog_column_definition *column_definition,
    struct mylite_catalog_column_descriptor *out_column
);
static int information_schema_copy_descriptor_text(
    struct mylite_db *database,
    const char *source,
    char *destination,
    size_t destination_size
);
static void information_schema_apply_connection_text_metadata(
    const struct mylite_db *database,
    const struct mylite_execution_catalog_column_definition *column_definition,
    struct mylite_result_column_descriptor *descriptor
);
static void information_schema_apply_temporal_precision_metadata(
    const struct mylite_execution_catalog_column_definition *column_definition,
    struct mylite_result_column_descriptor *descriptor
);
static void information_schema_apply_system_key_flags(
    const struct information_schema_query *query,
    size_t column_index,
    struct mylite_result_column_descriptor *descriptor
);
static bool information_schema_source_column_has_binary_numeric_flag(
    const struct information_schema_query *query,
    const struct mylite_result_column_descriptor *descriptor
);
static void information_schema_make_count_result_column_descriptor(
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
);
static void information_schema_make_unsigned_result_column_descriptor(
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
);
static void information_schema_make_group_concat_result_column_descriptor(
    const struct mylite_db *database,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
);
static void information_schema_make_logical_result_column_descriptor(
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
);
static int information_schema_append_result_rows(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
);
static int information_schema_append_streamed_result_rows(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
);
static bool information_schema_result_rows_can_stream(
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *where_clause
);
static int information_schema_append_grouped_statistics_result_rows(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
);
static int information_schema_sort_grouped_statistics_row_indexes(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t *indexes,
    size_t index_count
);
static int information_schema_sort_row_indexes_by(
    struct mylite_db *database,
    const struct information_schema_row_index_sort *sort,
    size_t *indexes,
    size_t index_count
);
static void information_schema_insertion_sort_row_indexes_by(
    const struct information_schema_row_index_sort *sort,
    size_t *indexes,
    size_t index_count
);
static void information_schema_merge_row_index_runs(
    const struct information_schema_row_index_sort *sort,
    size_t *indexes,
    size_t *scratch,
    struct information_schema_merge_range range
);
static int information_schema_compare_row_index_sort(
    const struct information_schema_row_index_sort *sort,
    size_t left_row,
    size_t right_row
);
static int information_schema_compare_grouped_statistics_rows(
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t left_row,
    size_t right_row
);
static bool information_schema_grouped_statistics_rows_same_group(
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t left_row,
    size_t right_row
);
static int information_schema_compare_row_values(
    const struct mylite_execution_catalog_table_definition *definition,
    size_t column_index,
    const char *left,
    const char *right
);
static int information_schema_group_concat_ordered_column_value(
    struct mylite_db *database,
    const struct information_schema_row_set *rows,
    const size_t *indexes,
    struct information_schema_group_concat_order order,
    char **out_value
);
static int information_schema_append_grouped_statistics_projected_row(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    const size_t *indexes,
    size_t start_index,
    size_t end_index,
    const struct information_schema_result_projection_buffers *buffers,
    mylite_result *result
);
static int information_schema_result_projection_buffers_init(
    struct mylite_db *database,
    const struct information_schema_query *query,
    struct information_schema_result_projection_buffers *buffers
);
static bool information_schema_query_needs_computed_projection_buffers(
    const struct information_schema_query *query
);
static bool information_schema_projection_needs_computed_buffer(
    enum information_schema_projection_kind kind
);
static void information_schema_result_projection_buffers_deinit(
    struct information_schema_result_projection_buffers *buffers
);
static int information_schema_append_projected_result_row(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    const struct information_schema_result_projection_buffers *buffers,
    mylite_result *result
);
static int information_schema_append_count_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
);
static char *information_schema_projection_computed_buffer(
    const struct information_schema_result_projection_buffers *buffers,
    size_t projection_index
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
static int information_schema_logical_not_column_projection_value(
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
static int information_schema_integer_literal_projection_value(
    struct mylite_db *database,
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
static int information_schema_numeric_projection_enter_frame(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    const struct mylite_sql_ast_node *expression,
    struct information_schema_numeric_eval_frame_stack *frames,
    struct information_schema_numeric_eval_value_stack *values
);
static int information_schema_numeric_projection_apply_divide_frame(
    struct mylite_db *database,
    struct information_schema_numeric_eval_value_stack *values
);
static int information_schema_numeric_eval_frame_stack_push(
    struct mylite_db *database,
    struct information_schema_numeric_eval_frame_stack *stack,
    struct information_schema_numeric_eval_frame frame
);
static void information_schema_numeric_eval_frame_stack_deinit(
    struct information_schema_numeric_eval_frame_stack *stack
);
static int information_schema_numeric_eval_value_stack_push(
    struct mylite_db *database,
    struct information_schema_numeric_eval_value_stack *stack,
    struct information_schema_numeric_eval_value value
);
static void information_schema_numeric_eval_value_stack_deinit(
    struct information_schema_numeric_eval_value_stack *stack
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
    struct information_schema_row_index_set *out_indexes
);
static const struct mylite_sql_ast_node *information_schema_statement_where_clause(
    const struct mylite_sql_ast_node *statement
);
static void information_schema_row_index_set_init(struct information_schema_row_index_set *indexes);
static int information_schema_row_index_set_append(
    struct mylite_db *database,
    struct information_schema_row_index_set *indexes,
    size_t value
);
static void information_schema_row_index_set_deinit(
    struct information_schema_row_index_set *indexes
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
static int information_schema_predicate_marks_connection_control_failed_login_attempts(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate,
    bool *out_includes_table
);
static int information_schema_predicate_stack_push_binary_children(
    struct mylite_db *database,
    struct scalar_arithmetic_node_stack *stack,
    const struct mylite_sql_ast_node *predicate
);
static bool information_schema_row_is_connection_control_failed_login_attempts(
    const char *const *row
);
static int information_schema_sort_row_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t *indexes,
    size_t index_count
);
static int information_schema_sort_tables_default_row_indexes(
    struct mylite_db *database,
    const struct information_schema_row_set *rows,
    size_t *indexes,
    size_t index_count
);
static int information_schema_compare_tables_default_rows(
    const struct information_schema_row_set *rows,
    size_t left_row,
    size_t right_row
);
static int information_schema_tables_default_schema_priority(const char *schema_name);
static int information_schema_tables_default_table_priority(
    const char *schema_name,
    const char *table_name
);
static int information_schema_compare_rows(
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    struct information_schema_row_order_pair pair
);
static int information_schema_compare_rows_by_column(
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    struct information_schema_row_order_pair pair,
    struct information_schema_row_order_column order
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
    const char *const *row,
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
    const char *const *row,
    struct information_schema_truth_stack *truth_stack
);
static int information_schema_is_null_predicate_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    const char *const *row,
    enum information_schema_truth_value *out_truth
);
static int information_schema_predicate_row_value(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
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
    const char *const *row,
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
    const struct mylite_execution_catalog_column_definition *column,
    const char *left_text,
    const char *right_text,
    char escape_character,
    bool *out_matches
);
static int information_schema_between_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    const char *const *row,
    enum information_schema_truth_value *out_truth
);
static int information_schema_in_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    const char *const *row,
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
static int information_schema_plan_wildcard_projection(
    struct mylite_db *database,
    struct information_schema_query *out_query
);
static int information_schema_plan_select_item_projection(
    struct mylite_db *database,
    bool select_list_has_one_item,
    const struct mylite_sql_ast_node *item,
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
static int information_schema_append_integer_literal_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
);
static int information_schema_append_group_concat_ordered_column_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
);
static int information_schema_append_logical_not_column_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
);
static int information_schema_append_projection_slot(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct information_schema_projection_slot_request *request
);
static int information_schema_plan_group(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *group_clause,
    struct information_schema_query *out_query
);
static bool information_schema_projection_is_grouped_statistics_compatible(
    const struct information_schema_query *query,
    size_t projection_index
);
static bool information_schema_group_contains_column(
    const struct information_schema_query *query,
    size_t column_index
);
static int information_schema_validate_group_concat_ordered_column_expression(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    size_t *out_column_index
);
static int information_schema_validate_logical_not_column_expression(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    size_t *out_column_index
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
static int execute_show_grants_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static bool show_grants_statement_targets_embedded_root(
    const struct mylite_sql_ast_node *statement
);
static const struct mylite_sql_ast_node *show_grants_statement_target(
    const struct mylite_sql_ast_node *statement
);
static const struct mylite_sql_ast_node *show_grants_statement_role_list(
    const struct mylite_sql_ast_node *statement
);
static bool show_grants_target_is_embedded_root(const struct mylite_sql_ast_node *target);
static int set_show_grants_no_such_grant_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int set_show_grants_role_not_granted_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static bool show_grants_user_span_equals(
    const struct mylite_sql_source_span *span,
    const char *expected
);
static bool show_grants_host_span_equals(
    const struct mylite_sql_source_span *span,
    const char *expected
);
static bool show_grants_quoted_span_text_matches(
    const struct mylite_sql_source_span *span,
    size_t offset,
    const char *expected
);
static int copy_show_grants_account_part(
    const struct mylite_sql_source_span *span,
    bool skip_at,
    const char *default_text,
    char *buffer,
    size_t buffer_size
);
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
static int execute_show_binlog_events_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int resolve_show_binlog_events_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct show_binlog_events_options *out_options
);
static int apply_show_binlog_events_log_name_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option
);
static int apply_show_binlog_events_from_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option,
    struct show_binlog_events_options *inout_options
);
static int apply_show_binlog_events_limit_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option,
    struct show_binlog_events_options *inout_options
);
static int read_show_log_events_unsigned_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option,
    const char *context,
    uint64_t *out_value
);
static void set_binlog_events_target_log_error(struct mylite_db *database);
static int execute_show_relaylog_events_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int validate_show_relaylog_events_channel(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static void set_replica_channel_does_not_exist_error(
    struct mylite_db *database,
    const char *channel_name
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
static bool builtin_system_schema_table_directory_contains(
    const char *schema_name,
    const char *table_name
);
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
    struct loaded_table_key_metadata_reference *out_reference
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
static int execute_show_create_procedure_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_create_function_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_create_trigger_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_create_event_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_create_user_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_create_database_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int set_show_create_function_missing_error(
    struct mylite_db *database,
    const struct table_name_resolution *target
);
static int set_show_create_event_missing_error(
    struct mylite_db *database,
    const struct table_name_resolution *target
);
static int set_show_create_trigger_missing_error(struct mylite_db *database);
static int append_show_create_user_embedded_root_result(
    struct mylite_db *database,
    mylite_result *result
);
static int set_show_create_user_failed_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target
);
static int execute_show_engines_statement(struct mylite_db *database, mylite_result **out_result);
static int execute_show_engine_status_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_engine_logs_statement(
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
    const struct table_maintenance_target *target,
    enum checksum_table_option option
);
static enum checksum_table_option checksum_table_option_for_statement(
    const struct mylite_sql_ast_node *statement
);
static int checksum_table_target_value(
    struct mylite_db *database,
    const struct table_maintenance_target *target,
    enum checksum_table_option option,
    char *value_buffer,
    size_t value_buffer_size,
    const char **out_value
);
static int compute_checksum_table_target_value(
    struct mylite_db *database,
    const struct table_maintenance_target *target,
    char *value_buffer,
    size_t value_buffer_size,
    const char **out_value
);
static int build_checksum_table_scan_sql(
    struct mylite_db *database,
    const struct table_maintenance_target *target,
    char **out_sql
);
static int scan_checksum_table_rows(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    uint64_t *out_checksum,
    bool *out_has_rows
);
static void checksum_table_update_byte(uint64_t *checksum, unsigned char byte);
static void checksum_table_update_bytes(
    uint64_t *checksum,
    const unsigned char *bytes,
    size_t byte_count
);
static void checksum_table_update_u64(uint64_t *checksum, uint64_t value);
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
