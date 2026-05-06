#ifndef MYLITE_RUNTIME_MYLITE_USER_VARIABLES_TYPES_H
#define MYLITE_RUNTIME_MYLITE_USER_VARIABLES_TYPES_H

#include "mylite_field_descriptor.h"
#include "sql/mylite_ast.h"
#include "sql/mylite_expression.h"

#include <stddef.h>

struct mylite_user_variable {
    char *name;
    struct mylite_expression_value value;
    struct mylite_field_descriptor descriptor;
};

struct mylite_user_variable_store {
    struct mylite_user_variable *items;
    size_t count;
};

struct mylite_user_variable_assignment_plan {
    char *name;
    char *sql_text;
    struct mylite_sql_ast expression_ast;
    const struct mylite_sql_ast_node *expression;
};

struct mylite_set_user_variable_plan {
    struct mylite_user_variable_assignment_plan *assignments;
    size_t assignment_count;
};

#endif
