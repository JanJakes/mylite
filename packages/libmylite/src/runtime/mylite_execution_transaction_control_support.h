#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_TRANSACTION_CONTROL_SUPPORT_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_TRANSACTION_CONTROL_SUPPORT_H

#include "mylite_catalog.h"

#include <stdbool.h>
#include <stddef.h>

struct mylite_db;
struct mylite_sql_ast_node;
struct planned_column;
struct planned_create_table;
struct planned_secondary_index;
struct table_name_resolution;

int mylite_execution_transaction_collect_identifier_parts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t capacity,
    size_t *out_part_count
);
int mylite_execution_transaction_execute_physical_create_table(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const char *physical_name,
    bool temporary
);
int mylite_execution_transaction_execute_physical_drop_table(
    struct mylite_db *database,
    const char *physical_name
);
void mylite_execution_transaction_planned_column_from_catalog_descriptor(
    const struct mylite_catalog_column_descriptor *column,
    const struct mylite_sql_ast_node *default_node,
    struct planned_column *out_column
);
void mylite_execution_transaction_planned_create_table_deinit(struct planned_create_table *plan);
int mylite_execution_transaction_reserve_primary_key_parts(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t required_capacity
);
int mylite_execution_transaction_reserve_secondary_index_parts(
    struct mylite_db *database,
    struct planned_secondary_index *index,
    size_t required_capacity
);
int mylite_execution_transaction_reserve_secondary_indexes(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t required_capacity
);
int mylite_execution_transaction_resolve_lock_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution,
    struct mylite_catalog_table_descriptor *out_table
);
void mylite_execution_transaction_set_system_variable_value_error(
    struct mylite_db *database,
    const char *variable_name,
    const char *value
);

#endif
