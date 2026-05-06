#ifndef MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_TEMPORAL_H
#define MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_TEMPORAL_H

#include <mylite/mylite.h>

#include "mylite_field_descriptor.h"
#include "sql/mylite_ast.h"

#include <stdbool.h>

struct mylite_expression_value;
struct mylite_select_plan;

struct mylite_expression_descriptor_temporal_callbacks {
    int (*infer_expression_descriptor)(
        mylite_db *database,
        const struct mylite_select_plan *plan,
        const struct mylite_sql_ast_node *expression,
        const struct mylite_expression_value *value,
        struct mylite_field_descriptor *out_descriptor
    );
};

struct mylite_field_descriptor mylite_expression_descriptor_current_datetime_function(
    unsigned int fsp
);
bool mylite_expression_descriptor_infer_current_temporal_function(
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor
);
int mylite_expression_descriptor_infer_temporal_function(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks
);
bool mylite_expression_descriptor_infer_temporal_scalar_function(
    const struct mylite_sql_ast_node *name,
    bool arguments_nullable,
    struct mylite_field_descriptor *out_descriptor
);
bool mylite_expression_descriptor_infer_temporal_part_function(
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor
);
bool mylite_expression_descriptor_interval_unit_has_time_part(
    enum mylite_sql_ast_interval_unit unit
);

#endif
