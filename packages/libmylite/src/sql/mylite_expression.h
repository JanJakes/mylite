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
    char *text_value;
};

struct mylite_expression_eval_context;

typedef int (*mylite_expression_resolve_identifier_fn)(void *user_data,
                                                       const struct mylite_sql_ast_node *identifier,
                                                       struct mylite_expression_value *out_value);
typedef int (*mylite_expression_eval_constant_fn)(void *user_data,
                                                  const struct mylite_sql_ast_node *expression,
                                                  struct mylite_expression_warnings *warnings,
                                                  struct mylite_expression_value *out_value);
typedef int (*mylite_expression_eval_aggregate_fn)(void *user_data,
                                                   const struct mylite_sql_ast_node *aggregate,
                                                   struct mylite_expression_value *out_value);
typedef int (*mylite_expression_eval_subquery_fn)(void *user_data,
                                                  const struct mylite_sql_ast_node *subquery,
                                                  struct mylite_expression_warnings *warnings,
                                                  struct mylite_expression_value *out_value);
typedef int (*mylite_expression_eval_in_subquery_fn)(void *user_data,
                                                     const struct mylite_sql_ast_node *expression,
                                                     const struct mylite_expression_value *left,
                                                     struct mylite_expression_warnings *warnings,
                                                     struct mylite_expression_value *out_value);
typedef int (*mylite_expression_eval_quantified_subquery_fn)(
    void *user_data, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left, struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value);
typedef int (*mylite_expression_eval_row_subquery_fn)(
    void *user_data, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_eval_context *context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value);
typedef int (*mylite_expression_eval_session_function_fn)(
    void *user_data, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value);

struct mylite_expression_eval_context {
    void *user_data;
    mylite_expression_resolve_identifier_fn resolve_identifier;
    mylite_expression_eval_constant_fn eval_constant;
    mylite_expression_eval_aggregate_fn eval_aggregate;
    mylite_expression_eval_subquery_fn eval_subquery;
    mylite_expression_eval_in_subquery_fn eval_in_subquery;
    mylite_expression_eval_quantified_subquery_fn eval_quantified_subquery;
    mylite_expression_eval_row_subquery_fn eval_row_subquery;
    mylite_expression_eval_session_function_fn eval_session_function;
};

void mylite_expression_value_deinit(struct mylite_expression_value *value);
void mylite_expression_warnings_deinit(struct mylite_expression_warnings *warnings);
int mylite_expression_warnings_append(struct mylite_expression_warnings *warnings,
                                      unsigned int code, const char *message);
int mylite_expression_warnings_append_condition(struct mylite_expression_warnings *warnings,
                                                enum mylite_expression_warning_level level,
                                                unsigned int code, const char *message);

int mylite_expression_eval(const struct mylite_sql_ast_node *expression,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value);
int mylite_expression_eval_with_context(const struct mylite_sql_ast_node *expression,
                                        const struct mylite_expression_eval_context *context,
                                        struct mylite_expression_warnings *warnings,
                                        struct mylite_expression_value *out_value);
int mylite_expression_value_copy(const struct mylite_expression_value *value,
                                 struct mylite_expression_value *out_value);
char *mylite_expression_value_to_text(const struct mylite_expression_value *value);
int64_t mylite_expression_value_to_int64(const struct mylite_expression_value *value);
int mylite_expression_value_compare(const struct mylite_expression_value *left,
                                    const struct mylite_expression_value *right,
                                    struct mylite_expression_warnings *warnings, int *out_compare);
int mylite_expression_value_truth(const struct mylite_expression_value *value,
                                  struct mylite_expression_warnings *warnings, int *out_truth);
bool mylite_expression_is_supported_no_table(const struct mylite_sql_ast_node *expression);
bool mylite_expression_is_cacheable_no_table(const struct mylite_sql_ast_node *expression);
bool mylite_expression_is_supported_function_call(const struct mylite_sql_ast_node *expression);

#endif
