#ifndef MYLITE_RUNTIME_MYLITE_TABLE_MAINTENANCE_H
#define MYLITE_RUNTIME_MYLITE_TABLE_MAINTENANCE_H

#include <mylite/mylite.h>

struct mylite_sql_ast_node;

int mylite_table_maintenance_prepare_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
);

#endif
