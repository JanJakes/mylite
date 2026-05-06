#ifndef MYLITE_RUNTIME_MYLITE_STATEMENT_EXECUTE_H
#define MYLITE_RUNTIME_MYLITE_STATEMENT_EXECUTE_H

#include <mylite/mylite.h>

struct mylite_expression_eval_context;
struct mylite_expression_value;
struct mylite_expression_warnings;
struct mylite_select_table;
struct mylite_select_scalar_eval_callbacks;
struct mylite_select_union_callbacks;
struct mylite_sql_ast_node;

struct mylite_statement_execute_callbacks {
    int (*execute_scalar_select)(mylite_stmt *stmt);
    int (*execute_table_select)(mylite_stmt *stmt);
    const struct mylite_select_scalar_eval_callbacks *scalar_callbacks;
    const struct mylite_select_union_callbacks *union_callbacks;
    int (*eval_dml_materialize_session_function)(
        void *user_data, const struct mylite_select_table *table,
        const struct mylite_sql_ast_node *function_call,
        const struct mylite_expression_eval_context *expression_context,
        struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value);
    int (*set_dml_materialize_where_predicate_eval_error)(void *user_data);
};

int mylite_statement_execute_custom_with_callbacks(
    mylite_stmt *stmt, const struct mylite_statement_execute_callbacks *callbacks);

#endif
