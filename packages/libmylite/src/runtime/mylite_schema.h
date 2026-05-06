#ifndef MYLITE_RUNTIME_MYLITE_SCHEMA_H
#define MYLITE_RUNTIME_MYLITE_SCHEMA_H

#include <mylite/mylite.h>

#include "mylite_schema_types.h"

struct mylite_sql_ast_node;

void mylite_schema_options_deinit(struct mylite_schema_options *options);
int mylite_schema_normalize_options(mylite_db *database, struct mylite_schema_options *options);
int mylite_schema_copy_statement_name(
    const struct mylite_sql_ast_node *statement,
    char **out_schema_name
);
int mylite_schema_copy_options(
    const struct mylite_sql_ast_node *statement,
    struct mylite_schema_options *options
);
int mylite_schema_execute_create_statement(mylite_stmt *stmt);
int mylite_schema_execute_alter_statement(mylite_stmt *stmt);
int mylite_schema_execute_drop_statement(mylite_stmt *stmt);
int mylite_schema_execute_use_statement(mylite_stmt *stmt);

#endif
