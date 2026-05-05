#ifndef MYLITE_RUNTIME_MYLITE_DML_INSERT_UPDATE_REFERENCE_H
#define MYLITE_RUNTIME_MYLITE_DML_INSERT_UPDATE_REFERENCE_H

#include "mylite_dml_types.h"

int mylite_dml_resolve_insert_update_column_reference(
    mylite_db *database, const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_table *table, const char *schema_name,
    const size_t *source_column_indexes, size_t source_column_count,
    const struct mylite_insert_column_reference *ref, bool *out_candidate,
    size_t *out_column_index);
int mylite_dml_set_insert_update_unknown_column_error(mylite_db *database, const char *column_name);

#endif
