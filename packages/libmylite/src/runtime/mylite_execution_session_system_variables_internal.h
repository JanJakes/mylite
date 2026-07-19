#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SESSION_SYSTEM_VARIABLES_INTERNAL_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SESSION_SYSTEM_VARIABLES_INTERNAL_H

static int system_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static const char *performance_schema_scalar_system_variable_value(
    enum mylite_execution_system_variable_kind kind
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
static int format_long_query_time_microseconds(
    struct mylite_db *database,
    uint64_t microseconds,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
static uint64_t boolean_session_placeholder_system_variable_uint64_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    bool global_scope
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
static uint64_t timeout_system_variable_default_value_for_kind(
    enum mylite_execution_system_variable_kind kind
);
static uint64_t timeout_system_variable_min_value_for_kind(
    enum mylite_execution_system_variable_kind kind
);
static uint64_t timeout_system_variable_max_value_for_kind(
    enum mylite_execution_system_variable_kind kind
);
static uint64_t innodb_parallel_read_threads_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
);
static uint64_t connection_memory_system_variable_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    bool global_scope
);
static uint64_t global_connection_memory_tracking_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
);
static const char *internal_tmp_mem_storage_engine_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
);
static uint64_t join_buffer_size_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
);
static uint64_t myisam_sort_buffer_size_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
);
static const char *myisam_stats_method_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
);
static uint64_t sort_buffer_size_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
);
static uint64_t open_files_limit_system_variable_value(void);
static uint64_t temptable_max_ram_default_value(void);
static uint64_t temptable_max_ram_system_variable_value(const struct mylite_db *database);
static uint64_t low_priority_updates_system_variable_uint64_value(
    const struct mylite_db *database,
    bool global_scope
);
static const char *low_priority_updates_system_variable_show_value(
    const struct mylite_db *database,
    bool global_scope
);
static const char *transaction_isolation_system_variable_value(
    const struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static const char *transaction_read_only_system_variable_value(
    const struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static bool system_variable_expression_has_global_scope(const struct mylite_sql_ast_node *expression
);
static bool system_variable_expression_has_session_scope(
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
static const char *performance_schema_show_system_variable_value(
    enum mylite_execution_system_variable_kind kind
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
static const char *session_system_variable_override_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
);
static int format_uint64(
    struct mylite_db *database,
    uint64_t value,
    char *buffer,
    size_t buffer_size
);
static int previous_diagnostics_condition_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
);
static int previous_diagnostics_error_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
);
static const struct mylite_execution_catalog_builtin_schema *find_builtin_schema_descriptor(
    const char *schema_name
);
static int resolve_schema_name(
    struct mylite_db *database,
    const char *schema_name,
    struct mylite_catalog_schema_descriptor *out_schema
);
static const char *session_system_variable_override_show_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
);
static const char *myisam_stats_method_text(enum mylite_session_myisam_stats_method value);
static const char *transaction_isolation_value_text(enum mylite_transaction_isolation isolation);
static const char *transaction_read_only_scalar_text(enum mylite_transaction_access_mode access_mode
);
static const char *transaction_read_only_show_text(enum mylite_transaction_access_mode access_mode);
static bool sql_mode_token_matches(const char *text, size_t length, const char *expected);
static int64_t current_timestamp_epoch(const struct mylite_db *database);
static bool text_equals_ascii_case_insensitive(const char *left, const char *right);

#endif
