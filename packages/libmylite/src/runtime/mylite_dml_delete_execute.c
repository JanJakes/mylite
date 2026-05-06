#include "mylite_dml.h"

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
    return status;
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
