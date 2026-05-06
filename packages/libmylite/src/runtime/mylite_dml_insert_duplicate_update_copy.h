#ifndef MYLITE_RUNTIME_MYLITE_DML_INSERT_DUPLICATE_UPDATE_COPY_H
#define MYLITE_RUNTIME_MYLITE_DML_INSERT_DUPLICATE_UPDATE_COPY_H

#include "mylite_dml_types.h"

int mylite_dml_copy_insert_duplicate_update_clause(
    const struct mylite_sql_ast_node *clause,
    struct mylite_insert_duplicate_update_plan *plan
);

#endif
