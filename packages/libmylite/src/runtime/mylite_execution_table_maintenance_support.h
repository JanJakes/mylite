#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_TABLE_MAINTENANCE_SUPPORT_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_TABLE_MAINTENANCE_SUPPORT_H

#include <stddef.h>
#include <stdint.h>

struct mylite_db;
struct mylite_sql_ast_node;

uint32_t mylite_execution_table_maintenance_result_collation_id(const char *collation_name);
int mylite_execution_table_maintenance_resolve_target_names(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *schema_name,
    size_t schema_name_size,
    char *table_name,
    size_t table_name_size
);

#endif
