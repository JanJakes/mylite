#ifndef MYLITE_RUNTIME_MYLITE_DML_INSERT_DIAGNOSTICS_H
#define MYLITE_RUNTIME_MYLITE_DML_INSERT_DIAGNOSTICS_H

#include "mylite_dml_types.h"

int mylite_dml_insert_set_wrong_value_count_error(mylite_db *database, size_t row_index);
int mylite_dml_insert_set_no_default_error(mylite_db *database, const char *column_name);
int mylite_dml_insert_set_unsupported_generated_default_error(mylite_db *database,
                                                              const char *column_name);
int mylite_dml_insert_set_unsupported_expression_error(mylite_db *database);
int mylite_dml_insert_append_no_default_warning(mylite_db *database, const char *column_name);
int mylite_dml_insert_append_no_default_warning_once(
    mylite_db *database, const struct mylite_insert_table_column *column,
    struct mylite_insert_execution_state *state, size_t column_index);
int mylite_dml_insert_append_null_warning(mylite_db *database, const char *column_name);
int mylite_dml_insert_append_null_warning_once(mylite_db *database,
                                               const struct mylite_insert_table_column *column,
                                               struct mylite_insert_execution_state *state,
                                               size_t column_index);
int mylite_dml_insert_set_duplicate_entry_error(mylite_db *database, const char *table_name,
                                                const struct mylite_insert_unique_index *index,
                                                const struct mylite_insert_bound_value *values);
int mylite_dml_insert_append_duplicate_entry_warning(
    mylite_db *database, const char *table_name, const struct mylite_insert_unique_index *index,
    const struct mylite_insert_bound_value *values);

#endif
