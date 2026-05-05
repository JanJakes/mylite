#ifndef MYLITE_RUNTIME_MYLITE_DML_INSERT_COPY_VALUE_H
#define MYLITE_RUNTIME_MYLITE_DML_INSERT_COPY_VALUE_H

#include "mylite_dml_types.h"

int mylite_dml_copy_insert_value(const struct mylite_sql_ast_node *value_node,
                                 struct mylite_insert_value *out_value);
int mylite_dml_copy_insert_column_reference(const struct mylite_sql_ast_node *identifier,
                                            struct mylite_insert_column_reference *out_reference);

#endif
