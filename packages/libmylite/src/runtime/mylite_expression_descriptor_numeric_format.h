#ifndef MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_NUMERIC_FORMAT_H
#define MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_NUMERIC_FORMAT_H

#include <mylite/mylite.h>

struct mylite_expression_descriptor_numeric_callbacks;
struct mylite_field_descriptor;
struct mylite_select_plan;
struct mylite_sql_ast_node;

int mylite_expression_descriptor_infer_format_function(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks
);

#endif
