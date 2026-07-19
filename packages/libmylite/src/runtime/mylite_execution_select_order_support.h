#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SELECT_ORDER_SUPPORT_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SELECT_ORDER_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_catalog_column_descriptor;
struct mylite_db;
struct mylite_sql_ast_node;
struct planned_row_scalar_expression;
struct select_source_context;

enum mylite_execution_select_order_expression_support {
    MYLITE_EXECUTION_SELECT_ORDER_EXPRESSION_UNSUPPORTED = 0,
    MYLITE_EXECUTION_SELECT_ORDER_EXPRESSION_SINGLE_SOURCE,
    MYLITE_EXECUTION_SELECT_ORDER_EXPRESSION_JOINED,
};

bool mylite_execution_select_order_source_context_is_joined(
    const struct select_source_context *source_context
);
enum mylite_execution_select_order_expression_support mylite_execution_select_order_expression_support(
    const struct mylite_sql_ast_node *expression
);
int mylite_execution_select_order_plan_field_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
int mylite_execution_select_order_plan_rand_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    struct planned_row_scalar_expression *out_expression
);
int mylite_execution_select_order_plan_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
void mylite_execution_select_order_row_scalar_expression_deinit(
    struct planned_row_scalar_expression *expression
);
int mylite_execution_select_order_copy_identifier_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char **out_alias
);
int mylite_execution_select_order_resolve_column_pointer(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor **out_column,
    size_t *out_source_index
);
int mylite_execution_select_order_parse_ordinal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    size_t column_count,
    size_t *out_ordinal
);
int mylite_execution_select_order_validate_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
int mylite_execution_select_order_convert_limit_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    int64_t *out_value
);

#endif
