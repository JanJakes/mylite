#ifndef MYLITE_RUNTIME_MYLITE_DML_INSERT_CONFLICT_H
#define MYLITE_RUNTIME_MYLITE_DML_INSERT_CONFLICT_H

#include "mylite_dml_types.h"

int mylite_dml_validate_insert_unique_indexes(mylite_db *database, const char *table_name,
                                              bool ignore, const struct mylite_insert_table *table,
                                              const struct mylite_insert_bound_value *values,
                                              struct mylite_insert_execution_state *state,
                                              bool *out_ignored);
int mylite_dml_find_insert_unique_conflict(mylite_db *database,
                                           const struct mylite_insert_table *table,
                                           const struct mylite_insert_bound_value *values,
                                           struct mylite_insert_unique_conflict *out_conflict);
int mylite_dml_validate_insert_update_unique_indexes(mylite_db *database, const char *table_name,
                                                     bool ignore,
                                                     const struct mylite_insert_table *table,
                                                     const struct mylite_insert_bound_value *values,
                                                     sqlite3_int64 rowid, bool *out_conflicts);
int mylite_dml_load_insert_conflict_row(mylite_db *database,
                                        const struct mylite_insert_table *table,
                                        sqlite3_int64 rowid,
                                        struct mylite_insert_bound_value *values);

#endif
