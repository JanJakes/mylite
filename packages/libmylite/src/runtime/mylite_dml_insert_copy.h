#ifndef MYLITE_RUNTIME_MYLITE_DML_INSERT_COPY_H
#define MYLITE_RUNTIME_MYLITE_DML_INSERT_COPY_H

#include "mylite_dml_types.h"

int mylite_dml_copy_insert_values_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_insert_values_plan *values_plan,
    struct mylite_insert_duplicate_update_plan *update_plan
);
int mylite_dml_copy_insert_set_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_insert_values_plan *values_plan,
    struct mylite_insert_set_plan *set_plan,
    struct mylite_insert_duplicate_update_plan *update_plan
);
int mylite_dml_copy_replace_values_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_insert_values_plan *values_plan
);
int mylite_dml_copy_replace_set_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_insert_values_plan *values_plan,
    struct mylite_insert_set_plan *set_plan
);

#endif
