#ifndef MYLITE_RUNTIME_MYLITE_CONNECTION_STATEMENT_H
#define MYLITE_RUNTIME_MYLITE_CONNECTION_STATEMENT_H

#include <mylite/mylite.h>

#include "mylite_connection_statement_types.h"

struct mylite_sql_ast_node;

int mylite_connection_prepare_charset_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
);
int mylite_connection_prepare_system_variable_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
);
int mylite_connection_copy_system_variable_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_connection_system_variable_plan *plan
);
int mylite_connection_resolve_system_variable_plan(
    mylite_db *database,
    const struct mylite_connection_system_variable_plan *plan,
    struct mylite_connection_system_variable_plan *out_plan
);
int mylite_connection_execute_set_names_statement(mylite_stmt *stmt);
int mylite_connection_execute_set_character_set_statement(mylite_stmt *stmt);
int mylite_connection_execute_set_system_variable_statement(mylite_stmt *stmt);
int mylite_connection_execute_system_variable_plan(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
);
void mylite_connection_charset_plan_deinit(struct mylite_connection_charset_plan *plan);
void mylite_connection_system_variable_plan_deinit(
    struct mylite_connection_system_variable_plan *plan
);

#endif
