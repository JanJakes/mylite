#include "mylite_dml.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_select.h"
#include "mylite_select_types.h"
#include "mylite_transactions.h"
#include "sqlite3.h"

static char *build_delete_physical_sql(
    mylite_db *database,
    const struct mylite_select_table *table
);

static int execute_delete_rowset(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_update_rowset *rowset,
    int64_t *affected_rows
);

static int validate_delete_rowset_parent_foreign_keys(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_update_rowset *rowset
);

static int load_delete_row_if_needed(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_update_row *row,
    struct mylite_update_row *loaded,
    const struct mylite_update_row **out_row,
    bool *out_found
);

static char *build_delete_row_scan_sql(
    mylite_db *database,
    const struct mylite_select_table *table
);

static size_t multi_delete_row_count(
    const struct mylite_update_rowset *rowsets,
    size_t rowset_count
);

int mylite_dml_execute_delete_rows_transaction(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_update_rowset *rowset,
    int64_t *out_affected_rows
) {
    sqlite3_stmt *delete_stmt = NULL;
    char *delete_sql = NULL;
    struct mylite_statement_atomicity atomicity = {0};
    int64_t affected_rows = 0;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (database == NULL || table == NULL || rowset == NULL || out_affected_rows == NULL) {
        return MYLITE_MISUSE;
    }

    *out_affected_rows = 0;
    if (rowset->row_count == 0U) {
        return MYLITE_OK;
    }

    status = validate_delete_rowset_parent_foreign_keys(database, table, rowset);
    if (status != MYLITE_OK) {
        return status;
    }

    status = mylite_transaction_begin_statement_atomicity(database, &atomicity);
    if (status != MYLITE_OK) {
        return status;
    }

    delete_sql = build_delete_physical_sql(database, table);
    if (delete_sql == NULL) {
        mylite_transaction_rollback_statement_atomicity(database, &atomicity);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(
        database->sqlite,
        delete_sql,
        -1,
        SQLITE_PREPARE_PERSISTENT,
        &delete_stmt,
        NULL
    );
    sqlite3_free(delete_sql);
    if (rc != SQLITE_OK) {
        mylite_transaction_rollback_statement_atomicity(database, &atomicity);
        return mylite_diagnostics_set_sqlite_error(database);
    }

    for (size_t index = 0U; index < rowset->row_count; ++index) {
        struct mylite_update_row loaded = {0};
        const struct mylite_update_row *stored = NULL;

        bool found = false;

        status = load_delete_row_if_needed(
            database,
            table,
            &rowset->rows[index],
            &loaded,
            &stored,
            &found
        );
        if (status == MYLITE_OK && !found) {
            mylite_dml_update_row_deinit(&loaded);
            continue;
        }
        if (status == MYLITE_OK) {
            status = mylite_dml_apply_parent_delete_foreign_key_actions(database, table, stored);
        }
        mylite_dml_update_row_deinit(&loaded);
        if (status != MYLITE_OK) {
            break;
        }
        sqlite3_reset(delete_stmt);
        sqlite3_clear_bindings(delete_stmt);
        rc = sqlite3_bind_int64(delete_stmt, 1, rowset->rows[index].rowid);
        if (rc == SQLITE_OK) {
            rc = sqlite3_step(delete_stmt);
        }
        if (rc != SQLITE_DONE) {
            status = mylite_diagnostics_set_sqlite_error(database);
            break;
        }
        ++affected_rows;
    }
    sqlite3_finalize(delete_stmt);

    if (status == MYLITE_OK) {
        status = mylite_catalog_refresh_table_statistics(
            database,
            table->schema_name,
            table->table_name,
            table->physical_name
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_statement_atomicity(database, &atomicity);
        if (status == MYLITE_OK) {
            *out_affected_rows = affected_rows;
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_statement_atomicity(database, &atomicity);
    return status;
}

int mylite_dml_execute_multi_delete_rows_transaction(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const size_t *target_table_indexes,
    const struct mylite_update_rowset *rowsets,
    size_t target_count,
    int64_t *out_affected_rows
) {
    struct mylite_statement_atomicity atomicity = {0};
    int64_t affected_rows = 0;
    int status = MYLITE_OK;

    if (database == NULL || plan == NULL || target_table_indexes == NULL || rowsets == NULL ||
        out_affected_rows == NULL) {
        return MYLITE_MISUSE;
    }

    *out_affected_rows = 0;
    if (target_count == 0U || multi_delete_row_count(rowsets, target_count) == 0U) {
        return MYLITE_OK;
    }

    for (size_t index = 0U; status == MYLITE_OK && index < target_count; ++index) {
        const struct mylite_select_table *table =
            mylite_select_plan_table_const(plan, target_table_indexes[index]);

        if (table == NULL) {
            status = MYLITE_UNSUPPORTED;
            break;
        }
        status = validate_delete_rowset_parent_foreign_keys(database, table, &rowsets[index]);
    }
    if (status != MYLITE_OK) {
        return status;
    }

    status = mylite_transaction_begin_statement_atomicity(database, &atomicity);
    if (status != MYLITE_OK) {
        return status;
    }

    for (size_t index = 0U; status == MYLITE_OK && index < target_count; ++index) {
        const struct mylite_select_table *table =
            mylite_select_plan_table_const(plan, target_table_indexes[index]);

        if (table == NULL) {
            status = MYLITE_UNSUPPORTED;
            break;
        }
        status = execute_delete_rowset(database, table, &rowsets[index], &affected_rows);
    }

    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_statement_atomicity(database, &atomicity);
        if (status == MYLITE_OK) {
            *out_affected_rows = affected_rows;
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_statement_atomicity(database, &atomicity);
    return status;
}

static int execute_delete_rowset(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_update_rowset *rowset,
    int64_t *affected_rows
) {
    sqlite3_stmt *delete_stmt = NULL;
    char *delete_sql = build_delete_physical_sql(database, table);
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (delete_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(
        database->sqlite,
        delete_sql,
        -1,
        SQLITE_PREPARE_PERSISTENT,
        &delete_stmt,
        NULL
    );
    sqlite3_free(delete_sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    for (size_t index = 0U; index < rowset->row_count; ++index) {
        struct mylite_update_row loaded = {0};
        const struct mylite_update_row *stored = NULL;

        bool found = false;

        status = load_delete_row_if_needed(
            database,
            table,
            &rowset->rows[index],
            &loaded,
            &stored,
            &found
        );
        if (status == MYLITE_OK && !found) {
            mylite_dml_update_row_deinit(&loaded);
            continue;
        }
        if (status == MYLITE_OK) {
            status = mylite_dml_apply_parent_delete_foreign_key_actions(database, table, stored);
        }
        mylite_dml_update_row_deinit(&loaded);
        if (status != MYLITE_OK) {
            break;
        }
        sqlite3_reset(delete_stmt);
        sqlite3_clear_bindings(delete_stmt);
        rc = sqlite3_bind_int64(delete_stmt, 1, rowset->rows[index].rowid);
        if (rc == SQLITE_OK) {
            rc = sqlite3_step(delete_stmt);
        }
        if (rc != SQLITE_DONE) {
            status = mylite_diagnostics_set_sqlite_error(database);
            break;
        }
        *affected_rows += sqlite3_changes(database->sqlite);
    }
    sqlite3_finalize(delete_stmt);
    if (status == MYLITE_OK) {
        status = mylite_catalog_refresh_table_statistics(
            database,
            table->schema_name,
            table->table_name,
            table->physical_name
        );
    }
    return status;
}

static int validate_delete_rowset_parent_foreign_keys(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_update_rowset *rowset
) {
    for (size_t index = 0U; index < rowset->row_count; ++index) {
        struct mylite_update_row loaded = {0};
        const struct mylite_update_row *stored = NULL;
        bool found = false;
        int status = load_delete_row_if_needed(
            database,
            table,
            &rowset->rows[index],
            &loaded,
            &stored,
            &found
        );

        if (status == MYLITE_OK && !found) {
            (void)mylite_diagnostics_set_error_message(database, "DELETE row disappeared");
            status = MYLITE_EXEC_ERROR;
        }
        if (status == MYLITE_OK) {
            status = mylite_dml_validate_parent_delete_foreign_keys(database, table, stored);
        }
        mylite_dml_update_row_deinit(&loaded);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int load_delete_row_if_needed(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_update_row *row,
    struct mylite_update_row *loaded,
    const struct mylite_update_row **out_row,
    bool *out_found
) {
    sqlite3_stmt *scan = NULL;
    char *sql = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    *out_row = row;
    *out_found = true;
    if (row->values != NULL && row->value_count == table->column_count) {
        return MYLITE_OK;
    }

    sql = build_delete_row_scan_sql(database, table);
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &scan, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    rc = sqlite3_bind_int64(scan, 1, row->rowid);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(scan);
        return mylite_diagnostics_set_sqlite_error(database);
    }
    rc = sqlite3_step(scan);
    if (rc == SQLITE_ROW) {
        status = mylite_dml_copy_update_sqlite_row(database, table, scan, loaded);
        if (status == MYLITE_OK) {
            *out_row = loaded;
        }
    } else if (rc == SQLITE_DONE) {
        *out_found = false;
    } else {
        status = mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_finalize(scan);
    return status;
}

static char *build_delete_row_scan_sql(
    mylite_db *database,
    const struct mylite_select_table *table
) {
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }
    sqlite3_str_appendall(sql, "SELECT rowid");
    for (size_t index = 0U; index < table->column_count; ++index) {
        sqlite3_str_appendf(sql, ",\"%w\"", table->columns[index].name);
    }
    sqlite3_str_appendf(sql, " FROM \"%w\" WHERE rowid = ?", table->physical_name);
    return sqlite3_str_finish(sql);
}

static size_t multi_delete_row_count(
    const struct mylite_update_rowset *rowsets,
    size_t rowset_count
) {
    size_t row_count = 0U;

    for (size_t index = 0U; index < rowset_count; ++index) {
        row_count += rowsets[index].row_count;
    }
    return row_count;
}

static char *build_delete_physical_sql(
    mylite_db *database,
    const struct mylite_select_table *table
) {
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }
    sqlite3_str_appendf(sql, "DELETE FROM \"%w\" WHERE rowid = ?", table->physical_name);
    return sqlite3_str_finish(sql);
}
