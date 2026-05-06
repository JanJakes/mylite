#ifndef MYLITE_RUNTIME_MYLITE_DML_INSERT_ROWS_COPY_H
#define MYLITE_RUNTIME_MYLITE_DML_INSERT_ROWS_COPY_H

#include "mylite_dml_types.h"

int mylite_dml_copy_insert_rows(
    const struct mylite_sql_ast_node *rows,
    struct mylite_insert_values_plan *plan
);

#endif
