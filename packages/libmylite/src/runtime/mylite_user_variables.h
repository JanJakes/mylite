#ifndef MYLITE_RUNTIME_MYLITE_USER_VARIABLES_H
#define MYLITE_RUNTIME_MYLITE_USER_VARIABLES_H

#include <mylite/mylite.h>

#include "mylite_field_descriptor.h"
#include "mylite_user_variables_types.h"
#include "sql/mylite_ast.h"
#include "sql/mylite_expression.h"

#include <stdbool.h>

bool mylite_user_variable_identifier_is_user_variable(const struct mylite_sql_ast_node *identifier);
int mylite_user_variable_copy_identifier_name(
    const struct mylite_sql_ast_node *identifier,
    char **out_name
);
int mylite_user_variable_eval_identifier(
    mylite_db *database,
    const struct mylite_sql_ast_node *identifier,
    struct mylite_expression_value *out_value
);
int mylite_user_variable_eval_name(
    mylite_db *database,
    const char *name,
    struct mylite_expression_value *out_value
);
int mylite_user_variable_infer_identifier(
    mylite_db *database,
    const struct mylite_sql_ast_node *identifier,
    struct mylite_field_descriptor *out_descriptor
);
int mylite_user_variable_prepare_set_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
);
int mylite_user_variable_execute_set_statement(mylite_stmt *stmt);
void mylite_user_variable_store_deinit(struct mylite_user_variable_store *store);
void mylite_user_variable_set_plan_deinit(struct mylite_set_user_variable_plan *plan);

#endif
