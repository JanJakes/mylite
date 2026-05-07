#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_CHECK_VALIDATE_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_CHECK_VALIDATE_H

#include "sql/mylite_ast.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>

struct mylite_table_ddl_check_column {
    const char *name;
    bool auto_increment;
};

enum mylite_table_ddl_check_validation_context {
    MYLITE_TABLE_DDL_CHECK_VALIDATE_CREATE_TABLE = 0,
    MYLITE_TABLE_DDL_CHECK_VALIDATE_ALTER_TABLE = 1,
};

struct mylite_table_ddl_check_validation_input {
    const char *constraint_name;
    const struct mylite_sql_ast_node *expression;
    const struct mylite_table_ddl_check_column *columns;
    size_t column_count;
    enum mylite_table_ddl_check_validation_context context;
};

int mylite_table_ddl_validate_check_expression(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input
);

#endif
