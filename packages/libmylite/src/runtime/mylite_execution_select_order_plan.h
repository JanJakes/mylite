#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SELECT_ORDER_PLAN_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SELECT_ORDER_PLAN_H

#include <stdbool.h>
#include <stddef.h>

struct mylite_catalog_column_descriptor;
struct mylite_db;
struct mylite_sql_ast_node;
struct planned_row_scalar_expression;
struct planned_select;
struct planned_select_limit;
struct planned_select_order;
struct planned_select_order_item;
struct select_source_context;

int mylite_execution_validate_single_table_dml_order_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause
);
int mylite_execution_plan_select_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_field_order,
    bool allow_rand_order,
    struct planned_select_order *out_order
);
int mylite_execution_plan_select_order_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_order_item *out_item
);
bool mylite_execution_select_order_key_is_like_predicate(const struct mylite_sql_ast_node *order_key
);
bool mylite_execution_select_order_key_is_supported_row_scalar_expression(
    const struct mylite_sql_ast_node *order_key
);
bool mylite_execution_select_order_key_is_row_constant_noop(
    const struct mylite_sql_ast_node *order_key
);
bool mylite_execution_select_order_key_is_integer_ordinal(
    const struct mylite_sql_ast_node *order_key
);
void mylite_execution_apply_select_order_item_direction(
    const struct mylite_sql_ast_node *direction,
    struct planned_select_order_item *out_item
);
int mylite_execution_append_planned_select_order_item(
    struct planned_select_order *order,
    struct planned_select_order_item item
);
void mylite_execution_planned_select_order_item_deinit(struct planned_select_order_item *item);
int mylite_execution_copy_planned_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *source,
    struct mylite_catalog_column_descriptor **out_column
);
int mylite_execution_resolve_select_order_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
);
int mylite_execution_plan_select_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
);
int mylite_execution_plan_delete_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
);

#endif
