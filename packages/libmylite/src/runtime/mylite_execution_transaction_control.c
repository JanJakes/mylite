#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_collation.h"
#include "mylite_connection.h"
#include "mylite_date_interval_second.h"
#include "mylite_diagnostics.h"
#include "mylite_dynamic_string.h"
#include "mylite_execution_ast_internal.h"
#include "mylite_execution_catalog.h"
#include "mylite_execution_completion.h"
#include "mylite_execution_diagnostics.h"
#include "mylite_execution_dml_numeric.h"
#include "mylite_execution_information_schema_join_plan.h"
#include "mylite_execution_information_schema_plan.h"
#include "mylite_execution_loaded_catalog.h"
#include "mylite_execution_plan_types.h"
#include "mylite_execution_result_rows.h"
#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_regexp.h"
#include "mylite_execution_scalar_string_position.h"
#include "mylite_execution_select_analysis.h"
#include "mylite_execution_select_order_plan.h"
#include "mylite_execution_sql_normalization.h"
#include "mylite_execution_sqlite_internal.h"
#include "mylite_execution_statement_transaction.h"
#include "mylite_execution_transaction_control.h"
#include "mylite_execution_transaction_control_support.h"
#include "mylite_execution_value.h"
#include "mylite_mysql_error_codes.h"
#include "mylite_result.h"
#include "mylite_spatial.h"
#include "mylite_sqlite_registration.h"
#include "mylite_statement_completion.h"
#include "mylite_statement_context.h"
#include "mylite_string_bitmask.h"
#include "mylite_sys_functions.h"
#include "sqlite3.h"

#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mylite_execution_declarations_00_constants.inc"
#include "mylite_execution_declarations_01_types.inc"
#include "mylite_execution_transaction_control_internal.h"

int mylite_execution_execute_set_transaction_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_set_transaction_statement(database, statement, out_result);
}

int mylite_execution_execute_start_transaction_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_start_transaction_statement(database, statement, out_result);
}

int mylite_execution_execute_start_transaction_with_characteristics(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *characteristics,
    mylite_result **out_result
) {
    return execute_start_transaction_statement_with_characteristics(
        database,
        characteristics,
        out_result
    );
}

int mylite_execution_execute_commit_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_commit_statement(database, statement, out_result);
}

int mylite_execution_execute_commit_with_chain(
    struct mylite_db *database,
    bool chain,
    mylite_result **out_result
) {
    return execute_commit_statement_with_chain(database, chain, out_result);
}

int mylite_execution_execute_rollback_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_rollback_statement(database, statement, out_result);
}

int mylite_execution_execute_rollback_with_chain(
    struct mylite_db *database,
    bool chain,
    mylite_result **out_result
) {
    return execute_rollback_statement_with_chain(database, chain, out_result);
}

int mylite_execution_execute_savepoint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_savepoint_statement(database, statement, out_result);
}

int mylite_execution_execute_rollback_to_savepoint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_rollback_to_savepoint_statement(database, statement, out_result);
}

int mylite_execution_execute_release_savepoint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_release_savepoint_statement(database, statement, out_result);
}

int mylite_execution_execute_lock_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_lock_tables_statement(database, statement, out_result);
}

int mylite_execution_execute_unlock_tables_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    return execute_unlock_tables_statement(database, out_result);
}

int mylite_execution_apply_transaction_system_variable_characteristics(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct transaction_characteristics *characteristics
) {
    return apply_transaction_system_variable_characteristics(database, target, characteristics);
}

int mylite_execution_parse_set_transaction_isolation_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_transaction_isolation *out_isolation
) {
    return parse_set_transaction_isolation_value(database, value_node, out_isolation);
}

int mylite_execution_parse_set_transaction_read_only_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_transaction_access_mode *out_access_mode
) {
    return parse_set_transaction_read_only_value(database, value_node, out_access_mode);
}

void mylite_execution_clear_active_transaction_characteristics(struct mylite_db *database) {
    clear_active_transaction_characteristics(database);
}

void mylite_execution_clear_next_transaction_characteristics_after_statement(
    struct mylite_db *database
) {
    clear_next_transaction_characteristics_after_statement(database);
}

void mylite_execution_clear_select_consumed_next_transaction_characteristics(
    struct mylite_db *database
) {
    clear_select_consumed_next_transaction_characteristics(database);
}

void mylite_execution_clear_persistent_auto_increment_high_waters(struct mylite_db *database) {
    clear_persistent_auto_increment_high_waters(database);
}

void mylite_execution_clear_user_savepoints(struct mylite_db *database) {
    clear_user_savepoints(database);
}

int mylite_execution_prepare_statement_transaction_boundary(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    return prepare_statement_transaction_boundary(database, statement);
}

int mylite_execution_reject_read_only_persistent_write(
    struct mylite_db *database,
    const struct table_name_resolution *target,
    const struct mylite_catalog_table_descriptor *table
) {
    return reject_read_only_persistent_write(database, target, table);
}

int mylite_execution_ensure_persistent_auto_increment_high_water_slot(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table
) {
    return ensure_persistent_auto_increment_high_water_slot(database, table);
}

int mylite_execution_record_persistent_auto_increment_high_water(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t next_value
) {
    return record_persistent_auto_increment_high_water(database, table, next_value);
}

int mylite_execution_read_current_auto_increment_next_after_write_lock(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t planned_next,
    bool use_session_insert_id,
    int64_t *out_next
) {
    return read_current_auto_increment_next_after_write_lock(
        database,
        table,
        planned_next,
        use_session_insert_id,
        out_next
    );
}

int mylite_execution_reconcile_persistent_auto_increment_high_waters(struct mylite_db *database) {
    return reconcile_persistent_auto_increment_high_waters(database);
}

int mylite_execution_commit_active_user_transaction_for_ddl(struct mylite_db *database) {
    return commit_active_user_transaction_for_ddl(database);
}

int mylite_execution_commit_active_user_transaction(struct mylite_db *database) {
    return commit_active_user_transaction(database);
}

const char *mylite_execution_transaction_isolation_value_text(
    enum mylite_transaction_isolation isolation
) {
    return transaction_isolation_value_text(isolation);
}

const char *mylite_execution_transaction_read_only_scalar_text(
    enum mylite_transaction_access_mode access_mode
) {
    return transaction_read_only_scalar_text(access_mode);
}

const char *mylite_execution_transaction_read_only_show_text(
    enum mylite_transaction_access_mode access_mode
) {
    return transaction_read_only_show_text(access_mode);
}

#include "mylite_execution_lock_tables.inc"
#include "mylite_execution_session_savepoints.inc"
#include "mylite_execution_statement_implicit_commits.inc"
#include "mylite_execution_statement_transaction_boundaries.inc"
#include "mylite_execution_transaction_characteristics.inc"
#include "mylite_execution_transaction_statements.inc"
