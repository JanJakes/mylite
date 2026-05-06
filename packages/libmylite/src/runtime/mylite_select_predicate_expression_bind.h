#ifndef MYLITE_RUNTIME_MYLITE_SELECT_PREDICATE_EXPRESSION_BIND_H
#define MYLITE_RUNTIME_MYLITE_SELECT_PREDICATE_EXPRESSION_BIND_H

#include <mylite/mylite.h>

#include <stddef.h>

struct mylite_select_plan;
struct mylite_select_predicate_bind_callbacks;
struct mylite_sql_ast_node;

int mylite_select_bind_predicate_expression_in_clause(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_select_plan *plan,
    const char *clause_context,
    size_t first_table,
    size_t table_count,
    const struct mylite_select_predicate_bind_callbacks *callbacks
);

#endif
