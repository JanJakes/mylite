#ifndef MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_STRING_H
#define MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_STRING_H

#include <mylite/mylite.h>

#include "mylite_field_descriptor.h"

#include <stdbool.h>

struct mylite_expression_value;
struct mylite_select_plan;
struct mylite_sql_ast_node;

struct mylite_expression_descriptor_string_callbacks {
    int (*infer_expression_descriptor)(
        mylite_db *database,
        const struct mylite_select_plan *plan,
        const struct mylite_sql_ast_node *expression,
        const struct mylite_expression_value *value,
        struct mylite_field_descriptor *out_descriptor
    );
};

int mylite_expression_descriptor_infer_char_function(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor
);
int mylite_expression_descriptor_infer_string_encoding_function(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_string_callbacks *callbacks,
    bool *out_matched
);
int mylite_expression_descriptor_infer_concat_function(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    bool nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_string_callbacks *callbacks,
    bool *out_matched
);
int mylite_expression_descriptor_infer_slice_string_function(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    bool nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_string_callbacks *callbacks,
    bool *out_matched
);

#endif
