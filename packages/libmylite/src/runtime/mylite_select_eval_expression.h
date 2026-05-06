#ifndef MYLITE_RUNTIME_MYLITE_SELECT_EVAL_EXPRESSION_H
#define MYLITE_RUNTIME_MYLITE_SELECT_EVAL_EXPRESSION_H

#include <mylite/mylite.h>

#include "mylite_select_eval.h"

#include <stdbool.h>
#include <stddef.h>

struct mylite_expression_eval_context;
struct mylite_expression_value;

struct mylite_table_select_eval_context {
    mylite_stmt *stmt;
    const struct mylite_table_select_row *row;
    const struct mylite_select_eval_callbacks *callbacks;
    bool order_resolution;
    bool having_resolution;
};

void mylite_select_eval_context_init(
    struct mylite_table_select_eval_context *context,
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    bool order_resolution,
    bool having_resolution
);
void mylite_select_eval_expression_context_init(
    struct mylite_expression_eval_context *expression_context,
    struct mylite_table_select_eval_context *user_context
);
int mylite_select_eval_cached_output_value(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    size_t output_index,
    const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_expression_value *out_value
);
int mylite_select_eval_output_value(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    size_t output_index,
    const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_expression_value *out_value
);
int mylite_select_eval_map_expression_status(
    mylite_stmt *stmt,
    int status,
    const struct mylite_select_eval_callbacks *callbacks
);

#endif
