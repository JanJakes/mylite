#ifndef MYLITE_RUNTIME_MYLITE_SELECT_SCALAR_H
#define MYLITE_RUNTIME_MYLITE_SELECT_SCALAR_H

#include <mylite/mylite.h>

#include "mylite_expression.h"
#include "mylite_field_descriptor.h"

struct mylite_sql_ast_node;

struct mylite_select_scalar_eval_callbacks {
    int (*infer_expression_descriptor)(
        mylite_db *database,
        const struct mylite_sql_ast_node *expression,
        const struct mylite_expression_value *value,
        struct mylite_field_descriptor *out_descriptor
    );
    int (*eval_session_function)(
        mylite_stmt *stmt,
        const struct mylite_sql_ast_node *function_call,
        const struct mylite_expression_eval_context *expression_context,
        struct mylite_expression_warnings *warnings,
        struct mylite_expression_value *out_value
    );
    int (*eval_subquery)(
        mylite_stmt *stmt,
        const struct mylite_sql_ast_node *subquery,
        struct mylite_expression_warnings *warnings,
        struct mylite_expression_value *out_value
    );
    int (*eval_in_subquery)(
        mylite_stmt *stmt,
        const struct mylite_sql_ast_node *expression,
        const struct mylite_expression_value *left,
        struct mylite_expression_warnings *warnings,
        struct mylite_expression_value *out_value
    );
    int (*eval_quantified_subquery)(
        mylite_stmt *stmt,
        const struct mylite_sql_ast_node *expression,
        const struct mylite_expression_value *left,
        struct mylite_expression_warnings *warnings,
        struct mylite_expression_value *out_value
    );
    int (*eval_row_subquery)(
        mylite_stmt *stmt,
        const struct mylite_sql_ast_node *expression,
        const struct mylite_expression_eval_context *expression_context,
        struct mylite_expression_warnings *warnings,
        struct mylite_expression_value *out_value
    );
    int (*set_unsupported_order_error)(mylite_db *database);
    int (*set_ambiguous_order_column_error)(mylite_db *database, const char *column_name);
};

int mylite_select_scalar_copy_statement(
    const struct mylite_sql_ast_node *statement,
    mylite_stmt *stmt,
    const struct mylite_select_scalar_eval_callbacks *callbacks
);
int mylite_select_scalar_execute_statement(
    mylite_stmt *stmt,
    const struct mylite_select_scalar_eval_callbacks *callbacks
);
int mylite_select_scalar_evaluate_expression(
    mylite_stmt *stmt,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_select_scalar_eval_callbacks *callbacks,
    struct mylite_expression_value *out_value
);
int mylite_select_scalar_append_warnings_to_database(mylite_stmt *stmt);

#endif
