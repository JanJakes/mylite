#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_JSON_INTERNAL_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_JSON_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

struct mylite_db;
struct mylite_json_normalize_result;
struct mylite_json_sql_value;
struct mylite_sql_ast_node;

int mylite_execution_scalar_json_evaluate_constructor_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_json_sql_value *out_value,
    char **out_owned_text
);
int mylite_execution_scalar_json_path_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
int mylite_execution_scalar_json_finish_extract_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
);

#endif
