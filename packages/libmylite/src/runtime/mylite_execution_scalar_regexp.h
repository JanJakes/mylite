#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_REGEXP_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_REGEXP_H

#include "mylite_ast.h"

#include <stdbool.h>
#include <stddef.h>

struct mylite_db;
struct mylite_sql_ast_node;
struct session_scalar_cell;

enum planned_regexp_string_function_kind {
    PLANNED_REGEXP_STRING_FUNCTION_NONE = 0,
    PLANNED_REGEXP_STRING_FUNCTION_INSTR = 1,
    PLANNED_REGEXP_STRING_FUNCTION_SUBSTR = 2,
    PLANNED_REGEXP_STRING_FUNCTION_REPLACE = 3,
};

struct regexp_like_text_argument_messages {
    const char *unsupported;
    const char *string_unsupported;
    const char *embedded_nul;
    const char *non_ascii;
};

int mylite_execution_scalar_regexp_like_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_regexp_string_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_evaluate_regexp_like_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool allow_session_scalar,
    const struct regexp_like_text_argument_messages *messages,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
int mylite_execution_evaluate_regexp_like_match_type_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_is_null,
    bool *out_case_sensitive
);
int mylite_execution_validate_regexp_like_pattern(
    struct mylite_db *database,
    const char *pattern,
    size_t pattern_length,
    bool case_sensitive,
    const char *unsupported_message
);
enum planned_regexp_string_function_kind mylite_execution_regexp_string_function_kind(
    enum mylite_sql_ast_node_kind kind
);
const char *mylite_execution_regexp_string_function_name(
    enum planned_regexp_string_function_kind kind
);
const char *mylite_execution_regexp_string_function_argument_count_error_name(
    enum mylite_sql_ast_node_kind kind
);
size_t mylite_execution_regexp_string_function_argument_count(
    const struct mylite_sql_ast_node *expression
);
const struct mylite_sql_ast_node *mylite_execution_regexp_string_function_argument_at(
    const struct mylite_sql_ast_node *expression,
    size_t index
);

#endif
