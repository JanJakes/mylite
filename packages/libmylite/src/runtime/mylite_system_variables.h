#ifndef MYLITE_RUNTIME_MYLITE_SYSTEM_VARIABLES_H
#define MYLITE_RUNTIME_MYLITE_SYSTEM_VARIABLES_H

#include <mylite/mylite.h>

#include "mylite_field_descriptor.h"
#include "sql/mylite_ast.h"
#include "sql/mylite_expression.h"

#include <stdbool.h>

bool mylite_system_variable_identifier_is_system_variable(
    const struct mylite_sql_ast_node *identifier);
int mylite_system_variable_eval_identifier(mylite_db *database,
                                           const struct mylite_sql_ast_node *identifier,
                                           struct mylite_expression_value *out_value);
int mylite_system_variable_infer_identifier(mylite_db *database,
                                            const struct mylite_sql_ast_node *identifier,
                                            struct mylite_field_descriptor *out_descriptor);

#endif
