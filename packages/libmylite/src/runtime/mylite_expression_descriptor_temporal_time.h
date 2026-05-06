#ifndef MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_TEMPORAL_TIME_H
#define MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_TEMPORAL_TIME_H

#include <mylite/mylite.h>

struct mylite_expression_descriptor_temporal_callbacks;
struct mylite_expression_value;
struct mylite_field_descriptor;
struct mylite_select_plan;
struct mylite_sql_ast_node;

int mylite_expression_descriptor_infer_time_function(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks);
int mylite_expression_descriptor_infer_sec_to_time_function(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks);
int mylite_expression_descriptor_infer_timediff_function(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks);
int mylite_expression_descriptor_infer_addsubtime_function(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks);
int mylite_expression_descriptor_infer_timestamp_function(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks);

#endif
