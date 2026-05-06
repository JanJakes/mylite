#ifndef MYLITE_RUNTIME_MYLITE_DML_INSERT_COLUMN_REFERENCE_H
#define MYLITE_RUNTIME_MYLITE_DML_INSERT_COLUMN_REFERENCE_H

#include <stddef.h>

struct mylite_insert_column_reference;
struct mylite_insert_table;

size_t mylite_dml_insert_table_column_index(
    const struct mylite_insert_table *table,
    const char *column_name
);
size_t mylite_dml_insert_table_column_reference_index(
    const struct mylite_insert_table *table,
    const char *schema_name,
    const char *table_name,
    const struct mylite_insert_column_reference *reference
);

#endif
