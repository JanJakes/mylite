#ifndef MYLITE_RUNTIME_MYLITE_SELECT_CONTEXT_H
#define MYLITE_RUNTIME_MYLITE_SELECT_CONTEXT_H

#include <mylite/mylite.h>

struct mylite_select_subquery_bind_callbacks;
struct mylite_select_eval_callbacks;
struct mylite_select_predicate_bind_callbacks;
struct mylite_sql_ast_node;
struct mylite_statement_execute_callbacks;
struct mylite_statement_prepare_callbacks;

extern const struct mylite_select_subquery_bind_callbacks
    mylite_select_context_subquery_bind_callbacks;

const struct mylite_statement_prepare_callbacks *
mylite_select_context_statement_prepare_callbacks(void);
const struct mylite_statement_execute_callbacks *
mylite_select_context_statement_execute_callbacks(void);
const struct mylite_select_eval_callbacks *
mylite_select_context_table_select_eval_callbacks(void);
const struct mylite_select_predicate_bind_callbacks *
mylite_select_context_predicate_bind_callbacks(void);
int mylite_select_context_prepare_subquery(mylite_db *database,
                                           const struct mylite_sql_ast_node *statement,
                                           mylite_stmt **out_stmt);

#endif
