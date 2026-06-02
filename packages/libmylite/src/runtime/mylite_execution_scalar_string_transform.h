#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_STRING_TRANSFORM_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_STRING_TRANSFORM_H

#include "mylite_ast.h"

#include <stdbool.h>

struct mylite_db;
struct mylite_sql_ast_node;
struct session_scalar_cell;

int mylite_execution_scalar_concat_ws_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_string_replace_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_string_insert_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_string_reverse_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_soundex_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_quote_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_substring_index_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
bool mylite_execution_concat_ws_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);
bool mylite_execution_string_replace_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);
bool mylite_execution_string_reverse_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);
bool mylite_execution_soundex_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);
bool mylite_execution_quote_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);

#endif
