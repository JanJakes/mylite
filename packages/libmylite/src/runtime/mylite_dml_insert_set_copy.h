#ifndef MYLITE_RUNTIME_MYLITE_DML_INSERT_SET_COPY_H
#define MYLITE_RUNTIME_MYLITE_DML_INSERT_SET_COPY_H

#include "mylite_dml_types.h"

int mylite_dml_copy_insert_set_assignments(const struct mylite_sql_ast_node *assignments,
                                           struct mylite_insert_set_plan *plan);

#endif
