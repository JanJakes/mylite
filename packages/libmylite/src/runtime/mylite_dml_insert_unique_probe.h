#ifndef MYLITE_RUNTIME_MYLITE_DML_INSERT_UNIQUE_PROBE_H
#define MYLITE_RUNTIME_MYLITE_DML_INSERT_UNIQUE_PROBE_H

#include "mylite_dml_types.h"

int mylite_dml_insert_unique_index_conflicts(
    mylite_db *database,
    const struct mylite_insert_table *table,
    const struct mylite_insert_unique_index *index,
    const struct mylite_insert_bound_value *values,
    bool *out_conflicts
);
int mylite_dml_insert_unique_index_conflict_rowid(
    mylite_db *database,
    const struct mylite_insert_table *table,
    const struct mylite_insert_unique_index *index,
    const struct mylite_insert_bound_value *values,
    sqlite3_int64 excluded_rowid,
    bool has_excluded_rowid,
    sqlite3_int64 *out_rowid,
    bool *out_conflicts
);

#endif
