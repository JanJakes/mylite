#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_TRANSACTION_CONTROL_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_TRANSACTION_CONTROL_H

#include "mylite_connection.h"
#include "mylite_execution_plan_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_catalog_table_descriptor;
struct mylite_db;
struct mylite_result;
struct mylite_sql_ast_node;
struct resolved_set_system_variable_target;

struct transaction_characteristics {
    bool has_isolation;
    bool has_access_mode;
    bool has_consistent_snapshot;
    enum mylite_transaction_isolation isolation;
    enum mylite_transaction_access_mode access_mode;
};

int mylite_execution_execute_set_transaction_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result
);
int mylite_execution_execute_start_transaction_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result
);
int mylite_execution_execute_start_transaction_with_characteristics(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *characteristics,
    struct mylite_result **out_result
);
int mylite_execution_execute_commit_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result
);
int mylite_execution_execute_commit_with_chain(
    struct mylite_db *database,
    bool chain,
    struct mylite_result **out_result
);
int mylite_execution_execute_rollback_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result
);
int mylite_execution_execute_rollback_with_chain(
    struct mylite_db *database,
    bool chain,
    struct mylite_result **out_result
);
int mylite_execution_execute_savepoint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result
);
int mylite_execution_execute_rollback_to_savepoint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result
);
int mylite_execution_execute_release_savepoint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result
);
int mylite_execution_execute_lock_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result
);
int mylite_execution_execute_unlock_tables_statement(
    struct mylite_db *database,
    struct mylite_result **out_result
);
int mylite_execution_apply_transaction_system_variable_characteristics(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct transaction_characteristics *characteristics
);
int mylite_execution_parse_set_transaction_isolation_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_transaction_isolation *out_isolation
);
int mylite_execution_parse_set_transaction_read_only_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_transaction_access_mode *out_access_mode
);
void mylite_execution_clear_active_transaction_characteristics(struct mylite_db *database);
void mylite_execution_clear_next_transaction_characteristics_after_statement(
    struct mylite_db *database
);
void mylite_execution_clear_select_consumed_next_transaction_characteristics(
    struct mylite_db *database
);
void mylite_execution_clear_persistent_auto_increment_high_waters(struct mylite_db *database);
void mylite_execution_clear_user_savepoints(struct mylite_db *database);
int mylite_execution_prepare_statement_transaction_boundary(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
int mylite_execution_reject_read_only_persistent_write(
    struct mylite_db *database,
    const struct table_name_resolution *target,
    const struct mylite_catalog_table_descriptor *table
);
int mylite_execution_ensure_persistent_auto_increment_high_water_slot(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table
);
int mylite_execution_record_persistent_auto_increment_high_water(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t next_value
);
int mylite_execution_read_current_auto_increment_next_after_write_lock(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t planned_next,
    bool use_session_insert_id,
    int64_t *out_next
);
int mylite_execution_reconcile_persistent_auto_increment_high_waters(struct mylite_db *database);
int mylite_execution_commit_active_user_transaction_for_ddl(struct mylite_db *database);
int mylite_execution_commit_active_user_transaction(struct mylite_db *database);
const char *mylite_execution_transaction_isolation_value_text(
    enum mylite_transaction_isolation isolation
);
const char *mylite_execution_transaction_read_only_scalar_text(
    enum mylite_transaction_access_mode access_mode
);
const char *mylite_execution_transaction_read_only_show_text(
    enum mylite_transaction_access_mode access_mode
);

#endif
