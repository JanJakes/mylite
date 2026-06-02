#include "mylite_catalog_internal.h"

#include "mylite_sqlite_registration.h"
#include "sqlite3.h"

#include <stdint.h>
#include <string.h>

enum {
    sqlite_use_nul_terminated_string = -1,
};

int mylite_catalog_execute_sql(sqlite3 *sqlite, const char *sql) {
    int sqlite_rc = sqlite3_exec(sqlite, sql, NULL, NULL, NULL);

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

int mylite_catalog_prepare_statement(
    sqlite3 *sqlite,
    const char *sql,
    sqlite3_stmt **out_statement
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;

    *out_statement = NULL;
    sqlite_rc = sqlite3_prepare_v2(sqlite, sql, sqlite_use_nul_terminated_string, &statement, NULL);
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    *out_statement = statement;

    return MYLITE_OK;
}

int mylite_catalog_bind_text(sqlite3_stmt *statement, int index, const char *value) {
    int sqlite_rc = sqlite3_bind_text(
        statement,
        index,
        value,
        sqlite_use_nul_terminated_string,
        SQLITE_TRANSIENT
    );

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

int mylite_catalog_bind_nullable_text(
    sqlite3_stmt *statement,
    int index,
    bool has_value,
    const char *value
) {
    int sqlite_rc = SQLITE_OK;

    if (!has_value) {
        sqlite_rc = sqlite3_bind_null(statement, index);
    } else {
        sqlite_rc = sqlite3_bind_text(
            statement,
            index,
            value,
            sqlite_use_nul_terminated_string,
            SQLITE_TRANSIENT
        );
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

int mylite_catalog_bind_i64(sqlite3_stmt *statement, int index, int64_t value) {
    int sqlite_rc = sqlite3_bind_int64(statement, index, (sqlite3_int64)value);

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

int mylite_catalog_bind_nullable_i64(
    sqlite3_stmt *statement,
    int index,
    bool has_value,
    int64_t value
) {
    int sqlite_rc = SQLITE_OK;

    if (!has_value) {
        sqlite_rc = sqlite3_bind_null(statement, index);
    } else {
        sqlite_rc = sqlite3_bind_int64(statement, index, (sqlite3_int64)value);
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): mirrors SQLite bind helper order.
int mylite_catalog_bind_u64(sqlite3_stmt *statement, int index, uint64_t value) {
    int64_t signed_value = 0;
    int rc = mylite_catalog_u64_to_i64(value, &signed_value);

    if (rc != MYLITE_OK) {
        return rc;
    }

    return mylite_catalog_bind_i64(statement, index, signed_value);
}

int64_t mylite_catalog_bool_value(bool value) {
    if (value) {
        return 1;
    }

    return 0;
}

int mylite_catalog_step_done(sqlite3_stmt *statement) {
    int sqlite_rc = sqlite3_step(statement);

    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

int mylite_catalog_require_changed_row(sqlite3 *sqlite) {
    if (sqlite3_changes(sqlite) != 1) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

int mylite_catalog_finalize_statement(sqlite3_stmt *statement, int rc) {
    int sqlite_rc = SQLITE_OK;

    if (statement == NULL) {
        return rc;
    }

    sqlite_rc = sqlite3_finalize(statement);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

int mylite_catalog_checked_column_i64(sqlite3_stmt *statement, int index, int64_t *out_value) {
    if (sqlite3_column_type(statement, index) != SQLITE_INTEGER) {
        return MYLITE_ERROR;
    }

    *out_value = (int64_t)sqlite3_column_int64(statement, index);

    return MYLITE_OK;
}

int mylite_catalog_checked_column_u64(sqlite3_stmt *statement, int index, uint64_t *out_value) {
    int64_t signed_value = 0;
    int rc = mylite_catalog_checked_column_i64(statement, index, &signed_value);

    if (rc != MYLITE_OK) {
        return rc;
    }

    return mylite_catalog_i64_to_u64(signed_value, out_value);
}

int mylite_catalog_checked_nullable_column_i64(
    sqlite3_stmt *statement,
    int index,
    bool *out_has_value,
    int64_t *out_value
) {
    int column_type = sqlite3_column_type(statement, index);

    *out_has_value = false;
    *out_value = 0;
    if (column_type == SQLITE_NULL) {
        return MYLITE_OK;
    }
    if (column_type != SQLITE_INTEGER) {
        return MYLITE_ERROR;
    }

    *out_has_value = true;
    *out_value = (int64_t)sqlite3_column_int64(statement, index);

    return MYLITE_OK;
}

int mylite_catalog_checked_column_text(
    sqlite3_stmt *statement,
    int index,
    char *destination,
    size_t destination_size
) {
    const unsigned char *source = NULL;
    int byte_count = 0;

    if (sqlite3_column_type(statement, index) != SQLITE_TEXT) {
        return MYLITE_ERROR;
    }

    source = sqlite3_column_text(statement, index);
    byte_count = sqlite3_column_bytes(statement, index);
    if (source == NULL || byte_count < 0 || (size_t)byte_count >= destination_size) {
        return MYLITE_ERROR;
    }

    memcpy(destination, source, (size_t)byte_count);
    destination[(size_t)byte_count] = '\0';

    return MYLITE_OK;
}

int mylite_catalog_checked_nullable_column_text(
    sqlite3_stmt *statement,
    int index,
    bool *out_has_value,
    char *destination,
    size_t destination_size
) {
    *out_has_value = false;
    if (destination != NULL && destination_size > 0U) {
        destination[0] = '\0';
    }
    if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
        return MYLITE_OK;
    }
    if (destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }

    int rc = mylite_catalog_checked_column_text(statement, index, destination, destination_size);

    if (rc == MYLITE_OK) {
        *out_has_value = true;
    }
    return rc;
}

int mylite_catalog_u64_to_i64(uint64_t value, int64_t *out_value) {
    if (value > INT64_MAX) {
        return MYLITE_ERROR;
    }

    *out_value = (int64_t)value;

    return MYLITE_OK;
}

int mylite_catalog_i64_to_u32(int64_t value, uint32_t *out_value) {
    if (value < 0 || value > UINT32_MAX) {
        return MYLITE_ERROR;
    }

    *out_value = (uint32_t)value;

    return MYLITE_OK;
}

int mylite_catalog_i64_to_u64(int64_t value, uint64_t *out_value) {
    if (value < 0) {
        return MYLITE_ERROR;
    }

    *out_value = (uint64_t)value;

    return MYLITE_OK;
}
