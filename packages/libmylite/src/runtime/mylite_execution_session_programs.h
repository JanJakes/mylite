#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SESSION_PROGRAMS_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SESSION_PROGRAMS_H

#include <stdbool.h>

struct mylite_db;
struct mylite_dynamic_string;
struct mylite_result;
struct mylite_sql_ast_node;

int mylite_execution_execute_prepare_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result
);
int mylite_execution_execute_prepared_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result
);
int mylite_execution_execute_deallocate_prepare_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result
);
int mylite_execution_execute_create_procedure_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result
);
int mylite_execution_execute_drop_procedure_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result
);
int mylite_execution_execute_call_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result
);
bool mylite_execution_prepared_statement_disallows_statement(
    const struct mylite_sql_ast_node *statement
);
int mylite_execution_append_show_create_definer(
    struct mylite_dynamic_string *string,
    const char *identity
);
int mylite_execution_try_stored_procedure_local_variables_placeholder(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result,
    bool *out_handled
);
int mylite_execution_execute_show_create_procedure_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result
);

#endif
