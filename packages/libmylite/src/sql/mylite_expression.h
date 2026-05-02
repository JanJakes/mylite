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

struct mylite_expression_warning {
    unsigned int code;
    char *message;
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

void mylite_expression_value_deinit(struct mylite_expression_value *value);
void mylite_expression_warnings_deinit(struct mylite_expression_warnings *warnings);

int mylite_expression_eval(const struct mylite_sql_ast_node *expression,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value);
int mylite_expression_value_copy(const struct mylite_expression_value *value,
                                 struct mylite_expression_value *out_value);
char *mylite_expression_value_to_text(const struct mylite_expression_value *value);
int64_t mylite_expression_value_to_int64(const struct mylite_expression_value *value);
bool mylite_expression_is_supported_no_table(const struct mylite_sql_ast_node *expression);

#endif
