#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_TABLE_MAINTENANCE_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_TABLE_MAINTENANCE_H

struct mylite_db;
struct mylite_result;
struct mylite_sql_ast_node;

int mylite_execution_execute_checksum_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result
);
int mylite_execution_execute_table_maintenance_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result
);

#endif
