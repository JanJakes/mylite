#if !defined(MYLITE_EXECUTION_SET_MODULE) && !defined(MYLITE_EXECUTION_SESSION_PROGRAM_MODULE)

static int execute_parsed_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static bool statement_defers_catalog_synchronization(const struct mylite_sql_ast_node *statement);
static int reject_poisoned_connection(struct mylite_db *database);
static int reject_command_with_active_cursor(struct mylite_db *database);
static int validate_stmt_binding_index(mylite_stmt *stmt, size_t index);
static int bind_stmt_bytes(mylite_stmt *stmt, const struct stmt_bytes_binding_request *request);
static int reset_cursor_execution(mylite_stmt *stmt);
static int start_cursor_execution(mylite_stmt *stmt);
static int validate_stmt_bindings_complete(mylite_stmt *stmt);
static int copy_active_stmt_parameter_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *parameter,
    struct planned_value *out_value
);
static int copy_active_stmt_parameter_cell(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *parameter,
    struct session_scalar_cell *out_cell
);
static bool prepared_statement_kind_is_supported(const struct mylite_sql_ast_node *statement);
static bool prepared_statement_returns_rows(const struct mylite_sql_ast_node *statement);
static bool prepared_statement_requires_object_validation(
    const struct mylite_sql_ast_node *statement
);
static int validate_prepared_statement_objects(mylite_stmt *stmt);
static int validate_prepared_select_statement_objects(mylite_stmt *stmt);
static int execute_prepared_materialized_statement(mylite_stmt *stmt);
static void release_stmt_bindings(mylite_stmt *stmt);
static int prepare_cursor_select_statement(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_stmt *stmt
);
static int prepare_statement(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    bool buffered_results,
    mylite_stmt **out_stmt
);
static int prepare_cursor_select_execution(mylite_stmt *stmt);
static int select_statement_uses_materialized_cursor_dispatch(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool *out_uses_materialized_dispatch
);
static enum cursor_plan_attempt_result prepare_cursor_select_plan(mylite_stmt *stmt, int *out_rc);
static int analyze_prepared_select(mylite_stmt *stmt);
static int ensure_prepared_statement_parse_tree(mylite_stmt *stmt);
static void release_prepared_statement_parse_tree(mylite_stmt *stmt);
static struct mylite_select_analysis_session_key current_select_analysis_session_key(
    const struct mylite_db *database
);
static void deinit_analyzed_select_plan(struct mylite_analyzed_select_plan *analyzed);
static int prepare_cursor_materialized_select_statement(mylite_stmt *stmt);
static int publish_buffered_statement_completion(mylite_stmt *stmt);
static int finish_cursor_sqlite_statement(mylite_stmt *stmt, int rc);
static void clear_cursor_select_plan_resources(mylite_stmt *stmt, int rc);
static int finish_cursor_statement(mylite_stmt *stmt, bool exhausted);
static int finish_failed_cursor_statement(mylite_stmt *stmt, int rc);
static void register_live_statement(mylite_stmt *stmt);
static void unregister_live_statement(mylite_stmt *stmt);
static int detach_cursor_statement(mylite_stmt *stmt, int rc);
static int release_cursor_statement_resources(mylite_stmt *stmt, int rc);
static void destroy_cursor_statement(mylite_stmt *stmt);
static int set_materialized_cursor_found_row_count(mylite_stmt *stmt);
static int set_cursor_found_row_count(mylite_stmt *stmt);
static int execute_non_prepared_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_parenthesized_query_expression(
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
static int execute_admin_noop_statement(struct mylite_db *database, mylite_result **out_result);
static int execute_unsupported_stored_program_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_utility_noop_statement(struct mylite_db *database, mylite_result **out_result);
static int execute_unsupported_utility_statement(struct mylite_db *database);
static bool set_statement_is_persist_noop(const struct mylite_sql_ast_node *statement);
static bool set_assignment_target_is_persist_noop(const struct mylite_sql_ast_node *target);
static bool span_starts_with_ascii_case_insensitive(
    const struct mylite_sql_source_span *span,
    const char *prefix
);
#endif

#if defined(MYLITE_EXECUTION_SET_MODULE)

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
static int apply_set_connection_character_set_tail_assignments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list
);
static const struct mylite_sql_ast_node *set_connection_character_set_collation_clause(
    const struct mylite_sql_ast_node *statement
);
static const struct mylite_sql_ast_node *set_connection_character_set_assignment_list(
    const struct mylite_sql_ast_node *statement
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
    const char *charset_name,
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
    size_t buffer_size,
    bool allow_empty_name
);
static int copy_user_variable_display_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *buffer,
    size_t buffer_size,
    bool allow_empty_name
);
static int copy_user_variable_name_with_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *buffer,
    size_t buffer_size,
    bool allow_empty_name,
    bool fold_name
);
static int copy_unquoted_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char *buffer,
    size_t buffer_size,
    bool allow_empty_name
);
static int copy_quoted_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char quote,
    char *buffer,
    size_t buffer_size,
    bool allow_empty_name
);
static int copy_string_quoted_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char *buffer,
    size_t buffer_size,
    bool allow_empty_name
);
static int copy_identifier_quoted_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char quote,
    char *buffer,
    size_t buffer_size,
    bool allow_empty_name
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
static void clear_session_system_variable_override(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
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

#endif

#if defined(MYLITE_EXECUTION_SESSION_PROGRAM_MODULE)

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
    unsigned int lexer_modes,
    size_t *out_parameter_count
);
static int execute_session_prepared_statement(
    struct mylite_db *database,
    struct mylite_session_prepared_statement *prepared,
    const struct mylite_sql_ast_node *using_list,
    mylite_result **out_result
);
static int bind_execute_parameters(
    struct mylite_db *database,
    mylite_stmt *statement,
    const struct mylite_sql_ast_node *using_list
);
static int bind_execute_user_variable(
    struct mylite_db *database,
    mylite_stmt *statement,
    size_t parameter_index,
    const struct mylite_sql_ast_node *variable
);
static int bind_execute_integer_user_variable(
    mylite_stmt *statement,
    size_t parameter_index,
    const struct mylite_session_user_variable *variable
);
static int transfer_executed_prepared_statement_result(
    mylite_stmt *statement,
    mylite_result **out_result
);
static int execute_expanded_prepared_statement_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    const struct mylite_session_prepared_statement *prepared,
    mylite_result **out_result
);
static int append_execute_string_literal(
    struct mylite_dynamic_string *string,
    unsigned int lexer_modes,
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
static int execute_create_procedure_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_drop_procedure_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_call_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int try_execute_stored_procedure_local_variables_placeholder_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result,
    bool *out_handled
);
static int execute_show_create_procedure_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int append_show_create_definer(struct mylite_dynamic_string *string, const char *identity);
static struct mylite_session_stored_procedure *find_session_stored_procedure(
    struct mylite_session_state *session,
    const char *schema_name,
    const char *name
);
static size_t find_session_stored_procedure_index(
    const struct mylite_session_state *session,
    const char *schema_name,
    const char *name
);
static void remove_session_stored_procedure_at(struct mylite_session_state *session, size_t index);
static void deinit_session_stored_procedure(struct mylite_session_stored_procedure *procedure);
static int ensure_session_stored_procedure_capacity(struct mylite_db *database);
static void set_prepared_statement_argument_count_error(struct mylite_db *database);
static void set_unknown_prepared_statement_error(
    struct mylite_db *database,
    const char *statement_name,
    const char *command_name
);
static void set_prepared_statement_command_unsupported_error(struct mylite_db *database);

#endif

#if defined(MYLITE_EXECUTION_SET_MODULE)

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
static bool set_system_variable_target_is_read_only(enum mylite_execution_system_variable_kind kind
);
static int apply_set_system_variable_user_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_global_read_only_toggle_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_global_server_security_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_global_admin_listener_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_global_mysqlx_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_global_server_tls_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_global_server_logging_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_replication_global_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_performance_schema_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_global_authentication_password_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_internal_session_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_internal_session_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
);
static bool internal_session_system_variable_handles_kind(
    enum mylite_execution_system_variable_kind kind
);
static int apply_set_internal_session_system_variable_text(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *value,
    size_t value_size
);
static int apply_set_internal_session_system_variable_uint64(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *value,
    size_t value_size
);
static int apply_set_internal_session_system_variable_rbr_exec_mode(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *value
);
static bool parse_internal_session_system_variable_uint64_text(
    const char *text,
    size_t text_size,
    uint64_t *out_value
);
static int set_internal_session_system_variable_uint64_override(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t value
);
static int append_internal_session_system_variable_set_warning_if_needed(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
);
static int apply_set_resource_tuning_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_resource_tuning_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
);
static bool resource_tuning_system_variable_handles_kind(
    enum mylite_execution_system_variable_kind kind
);
static bool resource_tuning_system_variable_is_global_only(
    enum mylite_execution_system_variable_kind kind
);
static bool resource_tuning_system_variable_is_session_read_only(
    enum mylite_execution_system_variable_kind kind
);
static int copy_resource_tuning_system_variable_node_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    char **out_value,
    size_t *out_value_size
);
static int copy_resource_tuning_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    char **out_value,
    size_t *out_value_size
);
static int copy_resource_tuning_system_variable_node_text(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    char **out_value,
    size_t *out_value_size
);
static int copy_resource_tuning_node_span_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    char **out_value,
    size_t *out_value_size
);
static int canonicalize_resource_tuning_system_variable_text(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *text,
    size_t text_size,
    char **out_value,
    size_t *out_value_size
);
static int copy_resource_tuning_terminology_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *text,
    size_t text_size,
    char **out_value,
    size_t *out_value_size
);
static int copy_resource_tuning_decimal_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *text,
    size_t text_size,
    char **out_value,
    size_t *out_value_size
);
static int copy_resource_tuning_uint64_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *text,
    size_t text_size,
    char **out_value,
    size_t *out_value_size
);
static bool parse_resource_tuning_uint64_text(
    const char *text,
    size_t text_size,
    uint64_t *out_value
);
static int validate_resource_tuning_fixed_global_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *value,
    size_t value_size
);
static int append_resource_tuning_system_variable_set_warning_if_needed(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
);
static int copy_resource_tuning_static_text(
    struct mylite_db *database,
    const char *text,
    char **out_value,
    size_t *out_value_size
);
static int copy_resource_tuning_text(
    struct mylite_db *database,
    const char *text,
    size_t text_size,
    char **out_value,
    size_t *out_value_size
);
static int apply_set_global_session_placeholder_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static bool set_global_session_placeholder_assignment_handles_kind(
    enum mylite_execution_system_variable_kind kind
);
static int apply_set_default_engine_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_default_engine_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
);
static bool set_system_variable_is_default_engine(enum mylite_execution_system_variable_kind kind);
static int apply_set_server_database_character_set_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_server_database_character_set_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
);
static bool set_system_variable_is_server_database_character_set_pair(
    enum mylite_execution_system_variable_kind kind
);
static bool set_system_variable_is_character_set_pair_variable(
    enum mylite_execution_system_variable_kind kind
);
static bool set_system_variable_is_collation_pair_variable(
    enum mylite_execution_system_variable_kind kind
);
static enum mylite_execution_system_variable_kind set_system_variable_pair_character_set_kind(
    enum mylite_execution_system_variable_kind kind
);
static enum mylite_execution_system_variable_kind set_system_variable_pair_collation_kind(
    enum mylite_execution_system_variable_kind kind
);
static int set_default_server_database_character_set_pair(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    char *charset_buffer,
    size_t charset_buffer_size,
    char *collation_buffer,
    size_t collation_buffer_size,
    const char **out_charset,
    const char **out_collation
);
static int set_server_database_character_set_pair_from_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *value,
    size_t value_size,
    bool value_is_catalog_id,
    const char **out_charset,
    const char **out_collation
);
static int set_server_database_character_set_pair(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *charset,
    const char *collation
);
static int append_server_database_character_set_pair_warnings(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *input_value,
    bool value_is_catalog_id,
    const char *charset,
    const char *collation
);
static int append_utf8mb3_collation_deprecation_warning(
    struct mylite_db *database,
    const char *collation
);
static int append_database_character_set_assignment_deprecation_warning(
    struct mylite_db *database,
    const char *variable_name
);
static int apply_set_character_set_filesystem_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_character_set_filesystem_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
);
static int set_session_character_set_filesystem_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *value,
    size_t value_size,
    bool value_is_collation_id
);
static bool set_character_set_value_is_catalog_id(const struct mylite_sql_ast_node *value_node);
static bool set_character_set_value_is_decimal(const struct mylite_sql_ast_node *value_node);
static enum mylite_sql_ast_literal_kind set_character_set_value_literal_kind(
    const struct mylite_sql_ast_node *value_node,
    bool unwrap_negative_unary
);
static const struct mylite_execution_catalog_character_set *mysql_character_set_by_filesystem_value(
    const char *value,
    bool value_is_collation_id
);
static int append_character_set_filesystem_assignment_warning(
    struct mylite_db *database,
    const char *value,
    bool value_is_collation_id,
    const struct mylite_execution_catalog_character_set *charset
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
static int copy_set_text_system_variable_default_value(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    char **out_value,
    size_t *out_value_size
);
static int copy_set_text_system_variable_identifier_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    char **out_value,
    size_t *out_value_size
);
static int copy_set_text_system_variable_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
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
static int apply_set_innodb_core_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_innodb_storage_system_variable_assignment(
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
static int apply_set_log_bin_trust_function_creators_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value
);
static int apply_set_fixed_boolean_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    bool expected_value
);
static int apply_set_performance_schema_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
);
static int apply_set_innodb_core_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
);
static int apply_set_innodb_storage_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
);
static int apply_set_fixed_global_server_security_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
);
static int apply_set_fixed_global_authentication_password_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
);
static int apply_set_fixed_global_admin_listener_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
);
static int apply_set_fixed_global_connection_system_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
);
static int apply_set_fixed_global_mysqlx_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
);
static int apply_set_fixed_global_server_tls_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
);
static int apply_set_fixed_global_server_logging_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
);
static int apply_set_replication_global_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    bool *out_handled
);
static int apply_set_sql_mode_cell_value(
    struct mylite_db *database,
    const struct session_scalar_cell *value
);
static int apply_set_time_zone_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value
);
static int apply_set_autocommit_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_autocommit_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int apply_set_autocommit_boolean_value(struct mylite_db *database, bool value);
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
static int apply_set_max_error_count_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int apply_set_m_session_limit_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_m_session_limit_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
);
static int parse_set_m_session_limit_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
);
static int parse_set_m_session_limit_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
);
static int parse_m_session_limit_system_variable_integer(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
);
static int apply_m_session_limit_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t value
);
static uint64_t m_session_limit_system_variable_default_value(
    enum mylite_execution_system_variable_kind kind
);
static uint64_t m_session_limit_system_variable_min_value(
    enum mylite_execution_system_variable_kind kind
);
static uint64_t m_session_limit_system_variable_max_value(
    enum mylite_execution_system_variable_kind kind
);
static bool m_session_limit_system_variable_is_zero_or_default_only(
    enum mylite_execution_system_variable_kind kind
);
static int apply_set_o_optimizer_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_o_optimizer_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
);
static int apply_set_big_tables_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int apply_set_low_priority_updates_cell_value(
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
static int apply_set_last_insert_id_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_last_insert_id_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static bool set_system_variable_is_last_insert_id(enum mylite_execution_system_variable_kind kind);
static int apply_set_remaining_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_remaining_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
);
static bool remaining_system_variable_handles_kind(enum mylite_execution_system_variable_kind kind);
static int apply_set_insert_id_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_insert_id_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int apply_set_pseudo_thread_id_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_pseudo_thread_id_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int apply_set_rand_seed_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_rand_seed_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int apply_set_temptable_max_ram_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_temptable_max_ram_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int apply_set_jl_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_jl_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
);
static int apply_set_myisam_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_myisam_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
);
static int apply_set_internal_tmp_mem_storage_engine_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_internal_tmp_mem_storage_engine_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int parse_set_internal_tmp_mem_storage_engine_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_session_internal_tmp_mem_storage_engine *out_value
);
static int parse_set_internal_tmp_mem_storage_engine_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    enum mylite_session_internal_tmp_mem_storage_engine *out_value
);
static int parse_internal_tmp_mem_storage_engine_text(
    struct mylite_db *database,
    const char *variable_name,
    const char *text,
    enum mylite_session_internal_tmp_mem_storage_engine *out_value
);
static int parse_internal_tmp_mem_storage_engine_integer(
    struct mylite_db *database,
    const char *variable_name,
    uint64_t magnitude,
    const char *value_text,
    enum mylite_session_internal_tmp_mem_storage_engine *out_value
);
static int apply_set_join_buffer_size_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_join_buffer_size_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int parse_set_join_buffer_size_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
);
static int parse_set_join_buffer_size_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
);
static int parse_join_buffer_size_integer(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
);
static int validate_set_jl_fixed_placeholder_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int validate_set_jl_fixed_placeholder_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int validate_set_jl_fixed_uint64_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t expected_value,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int validate_set_jl_fixed_text_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    const char *expected_value
);
static int validate_set_lc_messages_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node
);
static int validate_set_lc_messages_cell_value(
    struct mylite_db *database,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static void set_unknown_locale_error(struct mylite_db *database, const char *value_text);
static int parse_set_last_insert_id_system_variable_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
);
static int parse_set_last_insert_id_system_variable_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
);
static int parse_last_insert_id_system_variable_integer(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
);
static int parse_remaining_uint64_system_variable_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    bool allow_default,
    bool default_value,
    uint64_t *out_value
);
static int parse_remaining_uint64_system_variable_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
);
static int parse_remaining_uint64_system_variable_integer(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
);
static int finish_remaining_nonnegative_uint64_system_variable(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_source_span *unsigned_span,
    uint64_t *out_value
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
static int apply_set_low_priority_updates_value(
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
static int validate_set_fixed_text_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const char *expected_value
);
static int validate_set_fixed_text_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    const char *expected_value
);
static int validate_set_default_only_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const char *unsupported_message
);
static int validate_set_fixed_null_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const char *unsupported_message
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
static int apply_set_max_error_count_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_timeout_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_connection_memory_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_global_connection_system_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_binary_log_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_bootstrap_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_compatibility_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
);
static int apply_set_long_query_time_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_sort_buffer_size_value(
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
static int validate_set_fixed_timeout_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int parse_set_timeout_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
);
static int parse_set_long_query_time_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
);
static int parse_set_sort_buffer_size_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
);
static int validate_set_innodb_core_fixed_global_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int validate_set_innodb_core_fixed_global_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int validate_set_innodb_core_fixed_uint64_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t expected_value,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int validate_set_innodb_core_fixed_text_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *expected_value,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int validate_set_innodb_storage_fixed_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int validate_set_innodb_storage_fixed_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int validate_set_innodb_storage_fixed_uint64_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t expected_value,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int validate_set_innodb_storage_fixed_null_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int validate_set_innodb_storage_fixed_decimal_text_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const char *expected_value
);
static int validate_set_innodb_storage_fixed_decimal_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t expected_integral_value,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int validate_set_innodb_storage_fixed_decimal_text_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *expected_value,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static bool fixed_decimal_text_matches_ascii(
    struct mylite_sql_source_span text,
    const char *expected_value
);
static int validate_set_innodb_storage_fixed_text_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *expected_value,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int validate_set_performance_schema_fixed_uint64_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t expected_value,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static bool innodb_storage_system_variable_allows_session_assignment(
    enum mylite_execution_system_variable_kind kind
);
static int apply_set_innodb_parallel_read_threads_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_innodb_parallel_read_threads_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int apply_set_innodb_tmpdir_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_innodb_tmpdir_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int parse_set_innodb_parallel_read_threads_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
);
static int parse_set_innodb_parallel_read_threads_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
);
static int parse_innodb_parallel_read_threads_integer(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
);
static bool innodb_tmpdir_text_is_supported(const char *text, size_t text_size);
static int validate_set_innodb_storage_nullable_table_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int validate_set_innodb_storage_monitor_switch_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int validate_set_innodb_storage_nullable_table_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int validate_set_innodb_storage_monitor_switch_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static bool innodb_monitor_switch_text_is_supported(const char *text);
static int validate_set_myisam_fixed_global_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int validate_set_myisam_fixed_global_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int validate_set_myisam_fixed_uint64_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t expected_value,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int apply_set_myisam_sort_buffer_size_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_myisam_sort_buffer_size_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int parse_set_myisam_sort_buffer_size_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
);
static int parse_set_myisam_sort_buffer_size_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
);
static int parse_myisam_sort_buffer_size_integer(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
);
static int apply_set_myisam_stats_method_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_myisam_stats_method_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int parse_set_myisam_stats_method_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_session_myisam_stats_method *out_value
);
static int parse_set_myisam_stats_method_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    enum mylite_session_myisam_stats_method *out_value
);
static int parse_myisam_stats_method_text(
    struct mylite_db *database,
    const char *variable_name,
    const char *text,
    enum mylite_session_myisam_stats_method *out_value
);
static int parse_myisam_stats_method_integer(
    struct mylite_db *database,
    const char *variable_name,
    uint64_t magnitude,
    const char *value_text,
    enum mylite_session_myisam_stats_method *out_value
);
static const char *myisam_stats_method_text(enum mylite_session_myisam_stats_method value);
static int parse_sort_buffer_size_integer(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
);
static int parse_timeout_system_variable_integer(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
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
static int parse_set_max_error_count_value(
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
static int parse_set_max_error_count_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
);
static int parse_set_timeout_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
);
static int apply_set_connection_memory_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int apply_connection_memory_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t value
);
static int parse_set_connection_memory_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
);
static int parse_set_connection_memory_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
);
static uint64_t connection_memory_system_variable_default_value_for_kind(
    enum mylite_execution_system_variable_kind kind
);
static int parse_connection_memory_system_variable_integer(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
);
static uint64_t connection_memory_system_variable_min_value_for_kind(
    enum mylite_execution_system_variable_kind kind
);
static uint64_t connection_memory_system_variable_max_value_for_kind(
    enum mylite_execution_system_variable_kind kind
);
static int apply_set_binary_log_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
);
static int validate_set_binary_log_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static bool binary_log_system_variable_is_boolean(enum mylite_execution_system_variable_kind kind);
static bool binary_log_system_variable_is_uint64(enum mylite_execution_system_variable_kind kind);
static uint64_t binary_log_system_variable_uint64_value(
    enum mylite_execution_system_variable_kind kind
);
static int apply_set_bootstrap_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
);
static bool bootstrap_system_variable_is_global_only(enum mylite_execution_system_variable_kind kind
);
static int validate_set_bootstrap_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_compatibility_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
);
static int validate_set_compatibility_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static bool compatibility_system_variable_is_boolean(enum mylite_execution_system_variable_kind kind
);
static bool compatibility_system_variable_is_uint64(enum mylite_execution_system_variable_kind kind
);
static uint64_t compatibility_system_variable_uint64_value(
    enum mylite_execution_system_variable_kind kind
);
static int append_compatibility_system_variable_set_warning_if_needed(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
);
static int append_binary_log_set_warning_if_needed(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
);
static int apply_set_long_query_time_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int parse_set_long_query_time_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
);
static int parse_long_query_time_text(
    struct mylite_db *database,
    const char *text,
    size_t text_size,
    const char *variable_name,
    uint64_t *out_value
);
static int apply_set_sort_buffer_size_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int parse_set_sort_buffer_size_cell_value(
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
static int validate_set_fixed_decimal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    uint64_t expected_integral_value,
    const char *unsupported_message
);
static bool fixed_decimal_text_matches_uint64(
    struct mylite_sql_source_span text,
    uint64_t expected_value
);
static int apply_set_sql_mode_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node
);
static int apply_set_timestamp_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node
);
static int parse_set_timestamp_system_variable_arithmetic(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled,
    uint64_t *out_magnitude
);
static int apply_set_time_zone_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int validate_set_global_time_zone_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node
);
static int validate_set_global_time_zone_text(struct mylite_db *database, const char *text);
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
static unsigned int lexer_modes_for_statement(const struct mylite_db *database);
#endif
