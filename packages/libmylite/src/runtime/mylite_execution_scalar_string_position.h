#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_STRING_POSITION_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_STRING_POSITION_H

#include "mylite_ast.h"

#include <stdbool.h>
#include <stdint.h>

struct mylite_db;
struct mylite_sql_ast_node;
struct session_scalar_cell;

enum planned_string_slice_function_kind {
    PLANNED_STRING_SLICE_FUNCTION_NONE = 0,
    PLANNED_STRING_SLICE_FUNCTION_LEFT = 1,
    PLANNED_STRING_SLICE_FUNCTION_RIGHT = 2,
    PLANNED_STRING_SLICE_FUNCTION_SUBSTRING = 3,
};

enum planned_string_search_function_kind {
    PLANNED_STRING_SEARCH_FUNCTION_NONE = 0,
    PLANNED_STRING_SEARCH_FUNCTION_LOCATE = 1,
    PLANNED_STRING_SEARCH_FUNCTION_INSTR = 2,
    PLANNED_STRING_SEARCH_FUNCTION_POSITION = 3,
};

enum planned_string_padding_function_kind {
    PLANNED_STRING_PADDING_FUNCTION_NONE = 0,
    PLANNED_STRING_PADDING_FUNCTION_LPAD = 1,
    PLANNED_STRING_PADDING_FUNCTION_RPAD = 2,
    PLANNED_STRING_PADDING_FUNCTION_REPEAT = 3,
    PLANNED_STRING_PADDING_FUNCTION_SPACE = 4,
};

enum planned_string_bitmask_function_kind {
    PLANNED_STRING_BITMASK_FUNCTION_NONE = 0,
    PLANNED_STRING_BITMASK_FUNCTION_EXPORT_SET = 1,
    PLANNED_STRING_BITMASK_FUNCTION_MAKE_SET = 2,
};

enum {
    string_bitmask_export_set_min_argument_count = 3,
    string_bitmask_export_set_max_argument_count = 5,
    string_bitmask_make_set_min_argument_count = 2,
};

int mylite_execution_scalar_string_slice_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_string_padding_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_string_bitmask_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_string_search_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_find_in_set_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_strcmp_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
enum planned_string_slice_function_kind mylite_execution_string_slice_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
bool mylite_execution_is_string_slice_function_kind(enum mylite_sql_ast_node_kind ast_kind);
bool mylite_execution_string_slice_scalar_text_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);
bool mylite_execution_string_slice_length_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);
int mylite_execution_string_slice_signed_length_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_is_null
);
int mylite_execution_string_slice_signed_position_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_is_null
);
int mylite_execution_string_slice_substring_text_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    int64_t position,
    bool has_length,
    int64_t requested_length,
    struct session_scalar_cell *out_cell
);
int mylite_execution_string_slice_signed_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_message,
    const char *range_message,
    int64_t *out_value,
    bool *out_is_null
);
enum planned_string_padding_function_kind mylite_execution_string_padding_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
bool mylite_execution_is_string_padding_function_kind(enum mylite_sql_ast_node_kind ast_kind);
enum planned_string_bitmask_function_kind mylite_execution_string_bitmask_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
bool mylite_execution_is_string_bitmask_function_kind(enum mylite_sql_ast_node_kind ast_kind);
enum planned_string_search_function_kind mylite_execution_string_search_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
bool mylite_execution_is_string_search_function_kind(enum mylite_sql_ast_node_kind ast_kind);

#endif
