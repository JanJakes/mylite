#ifndef MYLITE_RUNTIME_MYLITE_TRANSACTIONS_H
#define MYLITE_RUNTIME_MYLITE_TRANSACTIONS_H

#include <mylite/mylite.h>

#include "mylite_transaction_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_sql_ast_node;

int mylite_transaction_begin_explicit(
    mylite_db *database,
    enum mylite_transaction_access_mode access_mode,
    bool consistent_snapshot
);
int mylite_transaction_begin_explicit_immediate(mylite_db *database);
int mylite_transaction_commit_explicit(mylite_db *database);
int mylite_transaction_rollback_explicit(mylite_db *database);
int mylite_transaction_begin_storage(mylite_db *database);
int mylite_transaction_commit_storage(mylite_db *database);
void mylite_transaction_rollback_storage(mylite_db *database);
int mylite_transaction_copy_statement(
    const struct mylite_sql_ast_node *statement,
    mylite_stmt *stmt
);
int mylite_transaction_copy_savepoint_statement(
    const struct mylite_sql_ast_node *statement,
    mylite_stmt *stmt
);
int mylite_transaction_execute_statement(mylite_stmt *stmt);
int mylite_transaction_execute_savepoint_statement(mylite_stmt *stmt);
int mylite_transaction_execute_rollback_to_savepoint_statement(mylite_stmt *stmt);
int mylite_transaction_execute_release_savepoint_statement(mylite_stmt *stmt);
void mylite_transaction_savepoint_plan_deinit(struct mylite_savepoint_plan *plan);
int mylite_transaction_create_savepoint(
    mylite_db *database,
    const char *name,
    const char *normalized_name
);
size_t mylite_transaction_find_savepoint(const mylite_db *database, const char *normalized_name);
int mylite_transaction_rollback_to_savepoint(mylite_db *database, size_t index);
int mylite_transaction_release_savepoint(mylite_db *database, size_t index);
void mylite_transaction_savepoint_state_deinit(struct mylite_savepoint_state *state);
void mylite_transaction_clear_user_savepoints(mylite_db *database);
int mylite_transaction_reapply_pending_auto_increments(mylite_db *database);
int mylite_transaction_update_table_auto_increment(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    uint64_t next_auto_increment
);
void mylite_transaction_clear_pending_auto_increments(mylite_db *database);
int mylite_transaction_begin_statement_atomicity(
    mylite_db *database,
    struct mylite_statement_atomicity *atomicity
);
int mylite_transaction_commit_statement_atomicity(
    mylite_db *database,
    struct mylite_statement_atomicity *atomicity
);
void mylite_transaction_rollback_statement_atomicity(
    mylite_db *database,
    const struct mylite_statement_atomicity *atomicity
);
int mylite_transaction_set_read_only_error(mylite_db *database);

#endif
