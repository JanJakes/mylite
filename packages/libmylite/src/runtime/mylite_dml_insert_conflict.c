#include "mylite_dml_insert_conflict.h"

#include "mylite_diagnostics.h"
#include "mylite_dml_insert_bound_value.h"
#include "mylite_dml_insert_diagnostics.h"
#include "mylite_dml_insert_unique_probe.h"
#include "sqlite3.h"

#include <string.h>

static char *build_insert_conflict_row_sql(
    mylite_db *database,
    const struct mylite_insert_table *table
);

int mylite_dml_validate_insert_unique_indexes(
    mylite_db *database,
    const char *table_name,
    bool ignore,
    const struct mylite_insert_table *table,
    const struct mylite_insert_bound_value *values,
    struct mylite_insert_execution_state *state,
    bool *out_ignored
) {
    if (database == NULL || table_name == NULL || table == NULL || values == NULL ||
        state == NULL || out_ignored == NULL) {
        return MYLITE_MISUSE;
    }

    *out_ignored = false;
    for (size_t index = 0U; index < table->unique_index_count; ++index) {
        bool conflicts = false;
        int status = mylite_dml_insert_unique_index_conflicts(
            database,
            table,
            &table->unique_indexes[index],
            values,
            &conflicts
        );

        if (status != MYLITE_OK) {
            return status;
        }
        if (conflicts) {
            if (ignore) {
                ++state->duplicate_count;
                *out_ignored = true;
                return mylite_dml_insert_append_duplicate_entry_warning(
                    database,
                    table_name,
                    &table->unique_indexes[index],
                    values
                );
            }
            state->advance_auto_increment_on_failure = true;
            return mylite_dml_insert_set_duplicate_entry_error(
                database,
                table_name,
                &table->unique_indexes[index],
                values
            );
        }
    }
    return MYLITE_OK;
}

int mylite_dml_find_insert_unique_conflict(
    mylite_db *database,
    const struct mylite_insert_table *table,
    const struct mylite_insert_bound_value *values,
    struct mylite_insert_unique_conflict *out_conflict
) {
    if (database == NULL || table == NULL || values == NULL || out_conflict == NULL) {
        return MYLITE_MISUSE;
    }

    *out_conflict = (struct mylite_insert_unique_conflict){0};
    for (size_t index = 0U; index < table->unique_index_count; ++index) {
        bool conflicts = false;
        sqlite3_int64 rowid = 0;
        int status = mylite_dml_insert_unique_index_conflict_rowid(
            database,
            table,
            &table->unique_indexes[index],
            values,
            0,
            false,
            &rowid,
            &conflicts
        );

        if (status != MYLITE_OK) {
            return status;
        }
        if (conflicts) {
            *out_conflict = (struct mylite_insert_unique_conflict){
                .index = &table->unique_indexes[index],
                .rowid = rowid,
                .conflicts = true,
            };
            return MYLITE_OK;
        }
    }
    return MYLITE_OK;
}

int mylite_dml_validate_insert_update_unique_indexes(
    mylite_db *database,
    const char *table_name,
    bool ignore,
    const struct mylite_insert_table *table,
    const struct mylite_insert_bound_value *values,
    sqlite3_int64 rowid,
    bool *out_conflicts
) {
    if (database == NULL || table_name == NULL || table == NULL || values == NULL ||
        out_conflicts == NULL) {
        return MYLITE_MISUSE;
    }

    *out_conflicts = false;
    for (size_t index = 0U; index < table->unique_index_count; ++index) {
        bool conflicts = false;
        sqlite3_int64 conflict_rowid = 0;
        int status = mylite_dml_insert_unique_index_conflict_rowid(
            database,
            table,
            &table->unique_indexes[index],
            values,
            rowid,
            true,
            &conflict_rowid,
            &conflicts
        );

        (void)conflict_rowid;
        if (status != MYLITE_OK) {
            return status;
        }
        if (conflicts) {
            *out_conflicts = true;
            if (ignore) {
                return mylite_dml_insert_append_duplicate_entry_warning(
                    database,
                    table_name,
                    &table->unique_indexes[index],
                    values
                );
            }
            return mylite_dml_insert_set_duplicate_entry_error(
                database,
                table_name,
                &table->unique_indexes[index],
                values
            );
        }
    }
    return MYLITE_OK;
}

int mylite_dml_load_insert_conflict_row(
    mylite_db *database,
    const struct mylite_insert_table *table,
    sqlite3_int64 rowid,
    struct mylite_insert_bound_value *values
) {
    sqlite3_stmt *select = NULL;
    char *sql = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (database == NULL || table == NULL || values == NULL) {
        return MYLITE_MISUSE;
    }

    sql = build_insert_conflict_row_sql(database, table);
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    rc = sqlite3_bind_int64(select, 1, rowid);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(select);
        return mylite_diagnostics_set_sqlite_error(database);
    }

    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW) {
        for (size_t index = 0U; index < table->column_count; ++index) {
            if (mylite_dml_copy_insert_sqlite_column_value(select, (int)index, &values[index]) !=
                0) {
                status = MYLITE_NOMEM;
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                break;
            }
        }
    } else {
        status =
            rc == SQLITE_DONE ? MYLITE_EXEC_ERROR : mylite_diagnostics_set_sqlite_error(database);
        if (status == MYLITE_EXEC_ERROR) {
            (void)mylite_diagnostics_set_error_message(
                database,
                "Duplicate row disappeared during INSERT"
            );
        }
    }

    sqlite3_finalize(select);
    return status;
}

static char *build_insert_conflict_row_sql(
    mylite_db *database,
    const struct mylite_insert_table *table
) {
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_append(sql, "SELECT ", (int)strlen("SELECT "));
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "\"%w\"", table->columns[index].name);
    }
    sqlite3_str_appendf(sql, " FROM \"%w\" WHERE rowid = ?", table->physical_name);
    return sqlite3_str_finish(sql);
}
