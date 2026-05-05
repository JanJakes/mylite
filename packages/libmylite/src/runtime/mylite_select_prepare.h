#ifndef MYLITE_RUNTIME_MYLITE_SELECT_PREPARE_H
#define MYLITE_RUNTIME_MYLITE_SELECT_PREPARE_H

#include <mylite/mylite.h>

#include <stddef.h>

struct mylite_select_group_bind_callbacks;
struct mylite_select_metadata_callbacks;
struct mylite_select_order_bind_callbacks;
struct mylite_select_predicate_bind_callbacks;
struct mylite_select_projection_callbacks;
struct mylite_select_scalar_eval_callbacks;
struct mylite_select_statement_callbacks;
struct mylite_sql_ast_node;

struct mylite_select_prepare_callbacks {
    const struct mylite_select_projection_callbacks *projection_callbacks;
    const struct mylite_select_statement_callbacks *statement_callbacks;
    const struct mylite_select_metadata_callbacks *metadata_callbacks;
    const struct mylite_select_predicate_bind_callbacks *predicate_callbacks;
    const struct mylite_select_group_bind_callbacks *group_callbacks;
    const struct mylite_select_order_bind_callbacks *order_callbacks;
    const struct mylite_select_scalar_eval_callbacks *scalar_callbacks;
};

int mylite_select_prepare_statement(mylite_db *database,
                                    const struct mylite_sql_ast_node *statement, const char *sql,
                                    size_t sql_length, mylite_stmt **out_stmt,
                                    const struct mylite_select_prepare_callbacks *callbacks);
int mylite_select_prepare_subquery(mylite_db *database, const struct mylite_sql_ast_node *statement,
                                   mylite_stmt **out_stmt,
                                   const struct mylite_select_prepare_callbacks *callbacks);

#endif
