#ifndef MYLITE_RUNTIME_MYLITE_SELECT_STATEMENT_H
#define MYLITE_RUNTIME_MYLITE_SELECT_STATEMENT_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

struct mylite_select_aggregate_bind_callbacks;
struct mylite_select_eval_callbacks;
struct mylite_select_metadata_callbacks;

struct mylite_select_statement_callbacks {
    const struct mylite_select_aggregate_bind_callbacks *aggregate_callbacks;
    const struct mylite_select_metadata_callbacks *metadata_callbacks;
};

int mylite_select_prepare_custom_table_statement(
    mylite_db *database, const struct mylite_sql_ast_node *where_clause, const char *sql,
    size_t sql_length, struct mylite_select_plan *plan, mylite_stmt **out_stmt,
    const struct mylite_select_statement_callbacks *callbacks);
int mylite_select_execute_table_statement(mylite_stmt *stmt,
                                          const struct mylite_select_eval_callbacks *callbacks);
int mylite_select_clone_order_expressions(mylite_stmt *stmt, const char *sql, size_t sql_length);

#endif
