#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_connection.h"
#include "mylite_date_interval_second.h"
#include "mylite_execution_ast_internal.h"
#include "mylite_execution_diagnostics.h"
#include "mylite_execution_plan_types.h"
#include "mylite_execution_result_rows.h"
#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_regexp.h"
#include "mylite_execution_scalar_string_position.h"
#include "mylite_execution_select_order_plan.h"
#include "mylite_execution_select_order_support.h"
#include "mylite_execution_sql_normalization.h"
#include "mylite_execution_statement_transaction.h"
#include "mylite_result.h"
#include "mylite_spatial.h"
#include "mylite_statement_completion.h"
#include "mylite_statement_context.h"
#include "mylite_string_bitmask.h"
#include "mylite_sys_functions.h"

#include <float.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "mylite_execution_declarations_00_constants.inc"
#include "mylite_execution_declarations_01_types.inc"

static int validate_single_table_dml_order_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause
);
static int plan_select_order(
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
static int plan_select_order_item_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_items,
    const struct select_source_context *source_context,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_field_order,
    bool allow_rand_order,
    struct planned_select_order *out_order
);
static int plan_select_order_ast_item_and_append(
    struct mylite_db *database,
    struct select_order_ast_item_nodes item_nodes,
    const struct select_source_context *source_context,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_field_order,
    bool allow_rand_order,
    struct planned_select_order *out_order
);
static int plan_select_order_ast_item(
    struct mylite_db *database,
    struct select_order_ast_item_nodes item_nodes,
    const struct select_source_context *source_context,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_field_order,
    bool allow_rand_order,
    struct planned_select_order_item *out_item
);
static int plan_select_order_ordinal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor **out_column,
    size_t *out_source_index
);
static void apply_select_order_item_direction(
    const struct mylite_sql_ast_node *direction,
    struct planned_select_order_item *out_item
);
static bool order_item_list_contains_field_order_key(const struct mylite_sql_ast_node *order_items);
static bool order_item_list_contains_rand_order_key(const struct mylite_sql_ast_node *order_items);
static bool select_order_key_is_row_constant_noop(const struct mylite_sql_ast_node *order_key);
static bool select_order_key_is_integer_ordinal(const struct mylite_sql_ast_node *order_key);
static bool select_order_key_is_field_function(const struct mylite_sql_ast_node *order_key);
static bool select_order_key_is_rand_function(const struct mylite_sql_ast_node *order_key);
static bool select_order_key_is_like_predicate(const struct mylite_sql_ast_node *order_key);
static bool select_order_key_is_supported_row_scalar_expression(
    const struct mylite_sql_ast_node *order_key
);
static bool select_order_key_is_supported_joined_row_scalar_expression(
    const struct mylite_sql_ast_node *order_key
);
static int plan_select_order_field_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_order_item *out_item
);
static int plan_select_order_rand_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    struct planned_select_order_item *out_item
);
static int plan_select_order_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_order_item *out_item
);
static int validate_select_order_field_expression(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression
);
static void planned_select_order_item_deinit(struct planned_select_order_item *item);
static int copy_planned_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *source,
    struct mylite_catalog_column_descriptor **out_column
);
static int append_planned_select_order_item(
    struct planned_select_order *order,
    struct planned_select_order_item item
);
static int resolve_order_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor **out_column,
    size_t *out_source_index,
    bool *out_resolved
);
static int resolve_order_column_pointer(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor **out_column,
    size_t *out_source_index
);
static int plan_select_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
);
static int plan_delete_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
);

int mylite_execution_validate_single_table_dml_order_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause
) {
    return validate_single_table_dml_order_clause(database, order_clause);
}

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
) {
    return plan_select_order(
        database,
        order_clause,
        source_context,
        select_plan,
        table_columns,
        table_column_count,
        allow_field_order,
        allow_rand_order,
        out_order
    );
}

int mylite_execution_plan_select_order_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_order_item *out_item
) {
    return plan_select_order_row_scalar_expression(
        database,
        order_key,
        source_context,
        table_columns,
        table_column_count,
        out_item
    );
}

bool mylite_execution_select_order_key_is_like_predicate(const struct mylite_sql_ast_node *order_key
) {
    return select_order_key_is_like_predicate(order_key);
}

bool mylite_execution_select_order_key_is_supported_row_scalar_expression(
    const struct mylite_sql_ast_node *order_key
) {
    return select_order_key_is_supported_row_scalar_expression(order_key);
}

bool mylite_execution_select_order_key_is_row_constant_noop(
    const struct mylite_sql_ast_node *order_key
) {
    return select_order_key_is_row_constant_noop(order_key);
}

bool mylite_execution_select_order_key_is_integer_ordinal(
    const struct mylite_sql_ast_node *order_key
) {
    return select_order_key_is_integer_ordinal(order_key);
}

void mylite_execution_apply_select_order_item_direction(
    const struct mylite_sql_ast_node *direction,
    struct planned_select_order_item *out_item
) {
    apply_select_order_item_direction(direction, out_item);
}

int mylite_execution_append_planned_select_order_item(
    struct planned_select_order *order,
    struct planned_select_order_item item
) {
    return append_planned_select_order_item(order, item);
}

void mylite_execution_planned_select_order_item_deinit(struct planned_select_order_item *item) {
    planned_select_order_item_deinit(item);
}

int mylite_execution_copy_planned_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *source,
    struct mylite_catalog_column_descriptor **out_column
) {
    return copy_planned_column_descriptor(database, source, out_column);
}

int mylite_execution_resolve_select_order_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
) {
    const struct mylite_catalog_column_descriptor *resolved_column = NULL;
    const int rc = resolve_order_column_pointer(
        database,
        column_node,
        source_context,
        table_columns,
        table_column_count,
        &resolved_column,
        out_source_index
    );

    *out_column = (struct mylite_catalog_column_descriptor){0};
    if (rc == MYLITE_OK) {
        *out_column = *resolved_column;
    }
    return rc;
}

int mylite_execution_plan_select_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
) {
    return plan_select_limit(database, limit_clause, out_limit);
}

int mylite_execution_plan_delete_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
) {
    return plan_delete_limit(database, limit_clause, out_limit);
}

#include "mylite_execution_select_order_planning.inc"
