#ifndef MYLITE_RUNTIME_MYLITE_STATEMENT_STRCMP_H
#define MYLITE_RUNTIME_MYLITE_STATEMENT_STRCMP_H

#include <mylite/mylite.h>

struct mylite_expression_collation_callbacks;
struct mylite_expression_eval_context;
struct mylite_expression_value;
struct mylite_expression_warnings;
struct mylite_select_table;
struct mylite_sql_ast_node;

int mylite_statement_evaluate_strcmp_function(
    mylite_stmt *stmt,
    const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings,
    const struct mylite_select_table *table,
    const struct mylite_expression_collation_callbacks *collation_callbacks,
    struct mylite_expression_value *out_value
);

#endif
