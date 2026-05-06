#ifndef MYLITE_RUNTIME_MYLITE_DML_INSERT_BOUND_VALUE_H
#define MYLITE_RUNTIME_MYLITE_DML_INSERT_BOUND_VALUE_H

#include "mylite_dml_types.h"

int mylite_dml_copy_insert_sqlite_column_value(
    sqlite3_stmt *scan,
    int column,
    struct mylite_insert_bound_value *out_value
);
int mylite_dml_copy_insert_bound_value(
    const struct mylite_insert_bound_value *value,
    struct mylite_insert_bound_value *out_value
);
int mylite_dml_copy_insert_bound_values(
    mylite_db *database,
    const struct mylite_insert_bound_value *values,
    size_t value_count,
    struct mylite_insert_bound_value **out_values
);
void mylite_dml_insert_bound_values_deinit(
    struct mylite_insert_bound_value *values,
    size_t value_count
);
void mylite_dml_insert_bound_value_deinit(struct mylite_insert_bound_value *value);
bool mylite_dml_insert_bound_value_is_numeric(
    const struct mylite_insert_bound_value *value,
    double *out_value,
    bool *out_is_integer
);
bool mylite_dml_parse_insert_integer_text(const char *text, int64_t *out_value);
bool mylite_dml_parse_insert_real_text(const char *text, double *out_value);

#endif
