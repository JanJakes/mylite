#ifndef MYLITE_RUNTIME_MYLITE_SESSION_FUNCTIONS_H
#define MYLITE_RUNTIME_MYLITE_SESSION_FUNCTIONS_H

#include <mylite/mylite.h>

struct mylite_expression_eval_context;
struct mylite_expression_value;
struct mylite_expression_warnings;
struct mylite_sql_ast_node;

int mylite_session_evaluate_core_function(
    mylite_stmt *stmt,
    const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value
);
int mylite_session_set_text_function_value(
    mylite_db *database,
    const char *text,
    struct mylite_expression_value *out_value
);

#endif
