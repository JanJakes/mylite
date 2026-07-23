#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_TRANSACTION_CONTROL_INTERNAL_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_TRANSACTION_CONTROL_INTERNAL_H

struct planned_lock_tables {
    struct mylite_session_table_lock *locks;
    size_t lock_count;
};

struct savepoint_control_sql_request {
    const char *prefix;
    const char *sqlite_name;
};

struct user_savepoint_values {
    const char *name;
    const char *sqlite_name;
};

static int execute_set_transaction_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
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
static const char *transaction_read_only_scalar_text(enum mylite_transaction_access_mode access_mode
);
static const char *transaction_read_only_show_text(enum mylite_transaction_access_mode access_mode);
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
static int execute_start_transaction_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_start_transaction_statement_with_characteristics(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *characteristic_list,
    mylite_result **out_result
);
static int execute_commit_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_commit_statement_with_chain(
    struct mylite_db *database,
    bool chain,
    mylite_result **out_result
);
static int execute_rollback_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_rollback_statement_with_chain(
    struct mylite_db *database,
    bool chain,
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
static int read_current_auto_increment_next_after_write_lock(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t planned_next,
    bool use_session_insert_id,
    int64_t *out_next
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
static void rollback_persistent_auto_increment_high_water_reconciliation(struct mylite_db *database
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

#endif
