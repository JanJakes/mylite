#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_CREATE_COLUMN_COPY_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_CREATE_COLUMN_COPY_H

#include "mylite_table_ddl_types.h"
#include "sql/mylite_ast.h"

int mylite_table_ddl_copy_create_table_column(
    const struct mylite_sql_ast_node *column_node,
    struct mylite_create_table_plan *plan
);
int mylite_table_ddl_add_create_table_check(
    struct mylite_create_table_plan *plan,
    const struct mylite_sql_ast_node *constraint_name,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_constraint_enforcement enforcement
);

#endif
