#ifndef MYLITE_SQL_MYLITE_EXPRESSION_H
#define MYLITE_SQL_MYLITE_EXPRESSION_H

#include "mylite_ast.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum mylite_expression_value_kind {
    MYLITE_EXPRESSION_VALUE_NULL = 0,
    MYLITE_EXPRESSION_VALUE_INT64 = 1,
    MYLITE_EXPRESSION_VALUE_UINT64 = 2,
    MYLITE_EXPRESSION_VALUE_REAL = 3,
    MYLITE_EXPRESSION_VALUE_TEXT = 4,
};

enum mylite_expression_warning_level {
    MYLITE_EXPRESSION_WARNING_LEVEL_WARNING = 0,
    MYLITE_EXPRESSION_WARNING_LEVEL_ERROR = 1,
    MYLITE_EXPRESSION_WARNING_LEVEL_NOTE = 2,
};

enum mylite_expression_temporal_type {
    MYLITE_EXPRESSION_TEMPORAL_NONE = 0,
    MYLITE_EXPRESSION_TEMPORAL_DATE = 1,
    MYLITE_EXPRESSION_TEMPORAL_TIME = 2,
    MYLITE_EXPRESSION_TEMPORAL_DATETIME = 3,
    MYLITE_EXPRESSION_TEMPORAL_TIMESTAMP = 4,
};

enum mylite_expression_text_charset {
    MYLITE_EXPRESSION_TEXT_CHARSET_UNKNOWN = 0,
    MYLITE_EXPRESSION_TEXT_CHARSET_BINARY = 1,
    MYLITE_EXPRESSION_TEXT_CHARSET_LATIN1 = 2,
    MYLITE_EXPRESSION_TEXT_CHARSET_UTF8MB4 = 3,
    MYLITE_EXPRESSION_TEXT_CHARSET_UTF8MB3 = 4,
    MYLITE_EXPRESSION_TEXT_CHARSET_ASCII = 5,
};

struct mylite_expression_warning {
    unsigned int code;
    char *message;
    enum mylite_expression_warning_level level;
};

struct mylite_expression_warnings {
    struct mylite_expression_warning *items;
    size_t count;
};

struct mylite_expression_value {
    enum mylite_expression_value_kind kind;
    int64_t int64_value;
    uint64_t uint64_value;
    double real_value;
    bool compact_real_text;
    bool preserve_real_text;
    bool suppress_text_numeric_warnings;
    bool has_literal_numeric_value;
    bool literal_numeric_unsigned;
    uint64_t literal_numeric_value;
    bool preserve_temporal_fraction_digits;
    enum mylite_expression_temporal_type temporal_type;
    enum mylite_expression_text_charset text_charset;
    char *text_value;
    size_t text_length;
};

struct mylite_expression_eval_context;

typedef int (*mylite_expression_resolve_identifier_fn)(
    void *user_data,
    const struct mylite_sql_ast_node *identifier,
    struct mylite_expression_value *out_value
);
typedef int (*mylite_expression_eval_constant_fn)(
    void *user_data,
    const struct mylite_sql_ast_node *expression,
    struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value
);
typedef int (*mylite_expression_eval_aggregate_fn)(
    void *user_data,
    const struct mylite_sql_ast_node *aggregate,
    struct mylite_expression_value *out_value
);
typedef int (*mylite_expression_eval_subquery_fn)(
    void *user_data,
    const struct mylite_sql_ast_node *subquery,
    struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value
);
typedef int (*mylite_expression_eval_in_subquery_fn)(
    void *user_data,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left,
    struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value
);
typedef int (*mylite_expression_eval_quantified_subquery_fn)(
    void *user_data,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left,
    struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value
);
typedef int (*mylite_expression_eval_row_subquery_fn)(
    void *user_data,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_eval_context *context,
    struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value
);
typedef int (*mylite_expression_assign_user_variable_fn)(
    void *user_data,
    const struct mylite_sql_ast_node *assignment,
    const struct mylite_expression_eval_context *context,
    struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value
);
typedef int (*mylite_expression_eval_session_function_fn)(
    void *user_data,
    const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *context,
    struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value
);
typedef int (*mylite_expression_eval_default_function_fn)(
    void *user_data,
    const struct mylite_sql_ast_node *function_call,
    struct mylite_expression_value *out_value
);

struct mylite_expression_eval_context {
    void *user_data;
    bool real_as_float;
    const char *character_set_connection;
    mylite_expression_resolve_identifier_fn resolve_identifier;
    mylite_expression_eval_constant_fn eval_constant;
    mylite_expression_eval_aggregate_fn eval_aggregate;
    mylite_expression_eval_subquery_fn eval_subquery;
    mylite_expression_eval_in_subquery_fn eval_in_subquery;
    mylite_expression_eval_quantified_subquery_fn eval_quantified_subquery;
    mylite_expression_eval_row_subquery_fn eval_row_subquery;
    mylite_expression_assign_user_variable_fn assign_user_variable;
    mylite_expression_eval_session_function_fn eval_session_function;
    mylite_expression_eval_default_function_fn eval_default_function;
};

void mylite_expression_value_deinit(struct mylite_expression_value *value);
void mylite_expression_warnings_deinit(struct mylite_expression_warnings *warnings);
int mylite_expression_warnings_append(
    struct mylite_expression_warnings *warnings,
    unsigned int code,
    const char *message
);
int mylite_expression_warnings_append_condition(
    struct mylite_expression_warnings *warnings,
    enum mylite_expression_warning_level level,
    unsigned int code,
    const char *message
);

int mylite_expression_eval(
    const struct mylite_sql_ast_node *expression,
    struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value
);
int mylite_expression_eval_with_context(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_eval_context *context,
    struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value
);
int mylite_expression_value_copy(
    const struct mylite_expression_value *value,
    struct mylite_expression_value *out_value
);
char *mylite_expression_value_to_text(const struct mylite_expression_value *value);
int64_t mylite_expression_value_to_int64(const struct mylite_expression_value *value);
int mylite_expression_value_compare(
    const struct mylite_expression_value *left,
    const struct mylite_expression_value *right,
    struct mylite_expression_warnings *warnings,
    int *out_compare
);
int mylite_expression_value_truth(
    const struct mylite_expression_value *value,
    struct mylite_expression_warnings *warnings,
    int *out_truth
);
int mylite_format_compact_real_text(double value, char *buffer, size_t buffer_size);
int mylite_format_storage_real_text(double value, char *buffer, size_t buffer_size);
bool mylite_expression_is_supported_no_table(const struct mylite_sql_ast_node *expression);
bool mylite_expression_is_cacheable_no_table(const struct mylite_sql_ast_node *expression);
bool mylite_expression_is_supported_function_call(const struct mylite_sql_ast_node *expression);

#endif
