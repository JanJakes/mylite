#include "mylite_dml_insert_sqlite_bind.h"

#include "mylite_diagnostics.h"

#include <limits.h>

static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_dml_bind_insert_row_values(
    mylite_db *database,
    sqlite3_stmt *insert,
    const struct mylite_insert_bound_value *values,
    size_t value_count
) {
    if (database == NULL || insert == NULL || values == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < value_count; ++index) {
        int rc = mylite_dml_bind_insert_bound_value(insert, (int)index + 1, &values[index]);

        if (rc != SQLITE_OK) {
            return mylite_diagnostics_set_sqlite_error(database);
        }
    }
    return MYLITE_OK;
}

int mylite_dml_bind_insert_bound_value(
    sqlite3_stmt *stmt,
    int index,
    const struct mylite_insert_bound_value *value
) {
    if (stmt == NULL || value == NULL) {
        return SQLITE_MISUSE;
    }

    switch (value->kind) {
    case MYLITE_INSERT_BOUND_NULL:
        return sqlite3_bind_null(stmt, index);
    case MYLITE_INSERT_BOUND_INTEGER:
        return sqlite3_bind_int64(stmt, index, (sqlite3_int64)value->integer_value);
    case MYLITE_INSERT_BOUND_REAL:
        return sqlite3_bind_double(stmt, index, value->real_value);
    case MYLITE_INSERT_BOUND_TEXT:
        if (value->text_length > (size_t)INT_MAX) {
            return SQLITE_TOOBIG;
        }
        return sqlite3_bind_text(
            stmt,
            index,
            value->text_value,
            (int)value->text_length,
            sqlite_transient_destructor()
        );
    case MYLITE_INSERT_BOUND_BLOB:
        if (value->text_length > (size_t)INT_MAX) {
            return SQLITE_TOOBIG;
        }
        return sqlite3_bind_blob(
            stmt,
            index,
            value->text_value,
            (int)value->text_length,
            sqlite_transient_destructor()
        );
    }

    return SQLITE_MISUSE;
}

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
