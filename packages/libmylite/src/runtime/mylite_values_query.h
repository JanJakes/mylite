#ifndef MYLITE_RUNTIME_MYLITE_VALUES_QUERY_H
#define MYLITE_RUNTIME_MYLITE_VALUES_QUERY_H

#include <mylite/mylite.h>

#include <stddef.h>

struct mylite_select_scalar_eval_callbacks;
struct mylite_select_union_callbacks;
struct mylite_select_union_prepare_callbacks;
struct mylite_sql_ast_node;

int mylite_values_query_prepare_statement(
    mylite_db *database, const struct mylite_sql_ast_node *statement, const char *sql,
    size_t sql_length, mylite_stmt **out_stmt,
    const struct mylite_select_scalar_eval_callbacks *scalar_callbacks,
    const struct mylite_select_union_prepare_callbacks *order_callbacks);
int mylite_values_query_execute_statement(
    mylite_stmt *stmt, const struct mylite_select_scalar_eval_callbacks *scalar_callbacks,
    const struct mylite_select_union_callbacks *order_callbacks);

#endif
