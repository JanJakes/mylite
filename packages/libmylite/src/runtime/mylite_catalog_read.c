#include "mylite_catalog.h"

#include "mylite_catalog_internal.h"

#include "mylite_sqlite_registration.h"
#include "sqlite3.h"

#include <stdint.h>

enum catalog_next_table_id_column_index {
    catalog_next_table_id_column = 0,
};

int mylite_catalog_read_next_table_id(sqlite3 *sqlite, int64_t *out_table_id) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT COALESCE(MAX(table_id), 0) + 1 FROM _mylite_catalog_tables",
        &statement
    );

    *out_table_id = 0;
    if (rc == MYLITE_OK) {
        sqlite_rc = mylite_catalog_sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = mylite_catalog_checked_column_i64(
                statement,
                catalog_next_table_id_column,
                out_table_id
            );
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(*out_table_id);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_next_index_id(sqlite3 *sqlite, int64_t *out_index_id) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT COALESCE(MAX(index_id), 0) + 1 FROM _mylite_catalog_indexes",
        &statement
    );

    *out_index_id = 0;
    if (rc == MYLITE_OK) {
        sqlite_rc = mylite_catalog_sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = mylite_catalog_checked_column_i64(statement, 0, out_index_id);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(*out_index_id);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_next_foreign_key_id(sqlite3 *sqlite, int64_t *out_foreign_key_id) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT COALESCE(MAX(foreign_key_id), 0) + 1 FROM _mylite_catalog_foreign_keys",
        &statement
    );

    *out_foreign_key_id = 0;
    if (rc == MYLITE_OK) {
        sqlite_rc = mylite_catalog_sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = mylite_catalog_checked_column_i64(statement, 0, out_foreign_key_id);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(*out_foreign_key_id);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_next_check_constraint_id(
    sqlite3 *sqlite,
    int64_t *out_check_constraint_id
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT COALESCE(MAX(check_constraint_id), 0) + 1 "
        "FROM _mylite_catalog_check_constraints",
        &statement
    );

    *out_check_constraint_id = 0;
    if (rc == MYLITE_OK) {
        sqlite_rc = mylite_catalog_sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = mylite_catalog_checked_column_i64(statement, 0, out_check_constraint_id);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(*out_check_constraint_id);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}
