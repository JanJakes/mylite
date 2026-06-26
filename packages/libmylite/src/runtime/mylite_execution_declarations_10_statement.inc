static int execute_parsed_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
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
static int execute_unsupported_stored_program_statement(struct mylite_db *database);
static int execute_utility_noop_statement(struct mylite_db *database, mylite_result **out_result);
static int execute_unsupported_utility_statement(struct mylite_db *database);
static bool set_statement_is_persist_noop(const struct mylite_sql_ast_node *statement);
static bool set_assignment_target_is_persist_noop(const struct mylite_sql_ast_node *target);
static bool span_starts_with_ascii_case_insensitive(
    const struct mylite_sql_source_span *span,
    const char *prefix
);
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
static int execute_set_transaction_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_start_transaction_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_commit_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_rollback_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static bool transaction_completion_requests_chain(const struct mylite_sql_ast_node *statement);
static int begin_chained_transaction(
    struct mylite_db *database,
    enum mylite_transaction_isolation isolation,
    enum mylite_transaction_access_mode access_mode
);
static int ensure_persistent_auto_increment_high_water_slot(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table
);
static int record_persistent_auto_increment_high_water(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t next_value
);
static int reconcile_persistent_auto_increment_high_waters(struct mylite_db *database);
static bool has_persistent_auto_increment_high_water_updates(
    const struct mylite_session_state *session
);
static int begin_persistent_auto_increment_high_water_reconciliation(
    struct mylite_db *database,
    bool *out_started_transaction
);
static int commit_persistent_auto_increment_high_water_reconciliation(struct mylite_db *database);
static void rollback_persistent_auto_increment_high_water_reconciliation(
    const struct mylite_db *database
);
static int apply_persistent_auto_increment_high_waters(
    struct mylite_db *database,
    const struct mylite_session_auto_increment_high_water *high_waters,
    size_t high_water_count
);
static int apply_persistent_auto_increment_high_water(
    sqlite3_stmt *statement,
    const struct mylite_session_auto_increment_high_water *high_water
);
static void clear_persistent_auto_increment_high_waters(struct mylite_db *database);
static int reconcile_temporary_physical_tables_after_user_rollback(struct mylite_db *database);
static int drop_orphaned_temporary_physical_tables(
    struct mylite_db *database,
    bool *out_schema_changed
);
static int find_orphaned_temporary_physical_table(
    struct mylite_db *database,
    char *physical_name,
    size_t physical_name_size,
    bool *out_found
);
static bool is_mylite_temporary_physical_table_name(const char *name);
static bool temporary_catalog_has_physical_table_name(
    const struct mylite_temporary_catalog *catalog,
    const char *physical_name
);
static int temporary_physical_table_exists(
    struct mylite_db *database,
    const char *physical_name,
    bool *out_exists
);
static int rebuild_temporary_physical_table(
    struct mylite_db *database,
    const struct mylite_temporary_catalog_table *table
);
static int rebuild_temporary_create_table_plan(
    struct mylite_db *database,
    const struct mylite_temporary_catalog_table *table,
    struct planned_create_table *out_plan
);
static int rebuild_temporary_create_table_columns(
    struct mylite_db *database,
    const struct mylite_temporary_catalog_table *table,
    struct planned_create_table *out_plan
);
static int rebuild_temporary_create_table_indexes(
    struct mylite_db *database,
    const struct mylite_temporary_catalog_table *table,
    struct planned_create_table *out_plan
);
static int rebuild_temporary_create_table_primary_index(
    struct mylite_db *database,
    const struct mylite_temporary_catalog_table *table,
    const struct mylite_catalog_index_descriptor *index,
    struct planned_create_table *out_plan
);
static int rebuild_temporary_create_table_secondary_index(
    struct mylite_db *database,
    const struct mylite_temporary_catalog_table *table,
    const struct mylite_catalog_index_descriptor *index,
    struct planned_create_table *out_plan
);
static size_t temporary_index_part_count(
    const struct mylite_temporary_catalog_table *table,
    int64_t index_id
);
static const struct mylite_catalog_index_column_descriptor *temporary_index_part_by_ordinal(
    const struct mylite_temporary_catalog_table *table,
    int64_t index_id,
    int64_t ordinal_position
);
static bool temporary_column_index_by_id(
    const struct mylite_temporary_catalog_table *table,
    int64_t column_id,
    size_t *out_column_index
);
static int execute_savepoint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_rollback_to_savepoint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_release_savepoint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_lock_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_unlock_tables_statement(struct mylite_db *database, mylite_result **out_result);
static int plan_lock_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_lock_tables *out_plan
);
static int plan_lock_table_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target_node,
    struct planned_lock_tables *plan,
    size_t target_index
);
static int copy_lock_table_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias_node,
    struct mylite_session_table_lock *out_lock
);
static int set_lock_table_mode(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *mode_node,
    struct mylite_session_table_lock *out_lock
);
static int check_lock_table_duplicate_targets(
    struct mylite_db *database,
    const struct planned_lock_tables *plan,
    size_t target_index
);
static int apply_lock_tables_plan(struct mylite_db *database, struct planned_lock_tables *plan);
static void planned_lock_tables_deinit(struct planned_lock_tables *plan);
static void clear_session_table_locks(struct mylite_db *database);
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
static int user_variable_assignment_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int copy_user_variable_assignment_result_cell(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell,
    const struct session_scalar_cell *value
);
static int append_user_variable_assignment_deprecation_warning(struct mylite_db *database);
static int copy_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *buffer,
    size_t buffer_size,
    bool allow_empty_name
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
    size_t *out_parameter_count
);
static int build_prepared_statement_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    const struct mylite_sql_ast_node *using_list,
    struct prepared_statement_expanded_sql *out_sql
);
static int execute_expanded_prepared_statement_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_result **out_result
);
static int append_execute_parameter_sql(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_sql_ast_node *variable
);
static int append_execute_string_literal(
    const struct mylite_db *database,
    struct mylite_dynamic_string *string,
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
static bool set_system_variable_target_is_read_only(
    enum mylite_execution_system_variable_kind kind
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
static int apply_set_global_authentication_password_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
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
static int apply_set_sql_mode_cell_value(
    struct mylite_db *database,
    const struct session_scalar_cell *value
);
static int apply_set_time_zone_cell_value(
    struct mylite_db *database,
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
static bool set_system_variable_is_last_insert_id(
    enum mylite_execution_system_variable_kind kind
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
static int validate_set_innodb_storage_fixed_decimal_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t expected_integral_value,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static int validate_set_innodb_storage_fixed_text_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *expected_value,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
static bool innodb_storage_system_variable_allows_session_assignment(
    enum mylite_execution_system_variable_kind kind
);
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
static bool bootstrap_system_variable_is_global_only(
    enum mylite_execution_system_variable_kind kind
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
static bool compatibility_system_variable_is_boolean(
    enum mylite_execution_system_variable_kind kind
);
static bool compatibility_system_variable_is_uint64(
    enum mylite_execution_system_variable_kind kind
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
    const struct mylite_sql_ast_node *value_node
);
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
static bool system_variable_expression_has_global_scope(
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
static int apply_set_transaction_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int collect_transaction_characteristics(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *list,
    struct transaction_characteristics *out_characteristics
);
static int apply_session_transaction_characteristics(
    struct mylite_db *database,
    const struct transaction_characteristics *characteristics
);
static int apply_next_transaction_characteristics(
    struct mylite_db *database,
    const struct transaction_characteristics *characteristics,
    bool from_system_variable
);
static int apply_transaction_system_variable_characteristics(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct transaction_characteristics *characteristics
);
static int parse_set_transaction_isolation_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_transaction_isolation *out_isolation
);
static int parse_set_transaction_read_only_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_transaction_access_mode *out_access_mode
);
static int decode_set_transaction_string_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const char *variable_name,
    char **out_value
);
static int copy_set_transaction_identifier_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    char *value,
    size_t value_size
);
static int set_transaction_variable_invalid_node_value_error(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node
);
static const char *transaction_isolation_value_text(enum mylite_transaction_isolation isolation);
static const char *transaction_read_only_scalar_text(
    enum mylite_transaction_access_mode access_mode
);
static const char *transaction_read_only_show_text(enum mylite_transaction_access_mode access_mode);
static const char *transaction_isolation_system_variable_value(
    const struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static const char *transaction_read_only_system_variable_value(
    const struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static enum mylite_transaction_access_mode current_transaction_access_mode(
    const struct mylite_db *database
);
static enum mylite_transaction_isolation current_transaction_isolation(
    const struct mylite_db *database
);
static enum mylite_transaction_access_mode effective_start_transaction_access_mode(
    const struct mylite_db *database,
    const struct transaction_characteristics *characteristics
);
static int reserve_consistent_snapshot_ignored_warning(struct mylite_db *database);
static int append_consistent_snapshot_ignored_warning(struct mylite_db *database);
static void clear_next_transaction_characteristics(struct mylite_db *database);
static void clear_active_transaction_characteristics(struct mylite_db *database);
static void clear_next_transaction_characteristics_before_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int prepare_statement_transaction_boundary(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static bool alter_table_comment_targets_existing_temporary_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static bool statement_consumes_next_characteristics_before_execution(
    const struct mylite_sql_ast_node *statement
);
static void clear_select_consumed_next_transaction_characteristics(struct mylite_db *database);
static void clear_next_transaction_characteristics_after_statement(struct mylite_db *database);
static int reject_read_only_persistent_write(
    struct mylite_db *database,
    const struct table_name_resolution *target,
    const struct mylite_catalog_table_descriptor *table
);
static int table_resolution_is_temporary(
    struct mylite_db *database,
    const struct table_name_resolution *target,
    const struct mylite_catalog_table_descriptor *table,
    bool *out_is_temporary
);
static bool statement_requires_implicit_user_transaction_commit(
    const struct mylite_sql_ast_node *statement
);
static int commit_active_user_transaction(struct mylite_db *database);
static int commit_active_user_transaction_for_ddl(struct mylite_db *database);
static void clear_user_savepoints(struct mylite_db *database);
static int create_or_replace_user_savepoint(struct mylite_db *database, const char *name);
static int rollback_to_user_savepoint(struct mylite_db *database, const char *name);
static int release_user_savepoint(struct mylite_db *database, const char *name);
static int copy_savepoint_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size
);
static void fold_savepoint_name(const char *source, char *destination, size_t destination_size);
static bool find_user_savepoint(
    const struct mylite_session_state *session,
    const char *name,
    size_t *out_index
);
static int reserve_user_savepoints(struct mylite_db *database, size_t required_capacity);
static int format_next_sqlite_savepoint_name(
    struct mylite_db *database,
    char *destination,
    size_t destination_size
);
static int execute_sqlite_savepoint_control(
    struct mylite_db *database,
    const char *prefix,
    const char *sqlite_name
);
static int build_savepoint_control_sql(
    struct savepoint_control_sql_request request,
    char **out_sql
);
static void remove_user_savepoint_at(struct mylite_session_state *session, size_t index);
static int append_user_savepoint(struct mylite_db *database, struct user_savepoint_values values);
