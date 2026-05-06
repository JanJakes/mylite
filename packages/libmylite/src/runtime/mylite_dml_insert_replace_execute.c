#include "mylite_dml_insert_replace_execute.h"

#include "mylite_diagnostics.h"
#include "mylite_dml.h"
#include "mylite_dml_insert_conflict.h"
#include "mylite_dml_insert_set_row_resolve.h"
#include "sqlite3.h"

#include <stdlib.h>

static int delete_replace_conflict_row(
    mylite_db *database,
    sqlite3_stmt *delete_stmt,
    sqlite3_int64 rowid,
    struct mylite_insert_execution_state *state
);

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
int mylite_dml_write_replace_candidate_row(
    mylite_db *database,
    sqlite3_stmt *insert,
    sqlite3_stmt *delete_stmt,
    const struct mylite_insert_table *table,
    struct mylite_insert_execution_state *state,
    const struct mylite_insert_bound_value *values
) {
    if (database == NULL || insert == NULL || delete_stmt == NULL || table == NULL ||
        state == NULL || values == NULL) {
        return MYLITE_MISUSE;
    }

    for (;;) {
        struct mylite_insert_unique_conflict conflict = {0};
        int status = mylite_dml_find_insert_unique_conflict(database, table, values, &conflict);

        if (status != MYLITE_OK) {
            return status;
        }
        if (!conflict.conflicts) {
            break;
        }
        status = delete_replace_conflict_row(database, delete_stmt, conflict.rowid, state);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return mylite_dml_write_insert_candidate_row(database, insert, table, values, state);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
int mylite_dml_execute_replace_row(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const char *schema_name,
    sqlite3_stmt *insert,
    sqlite3_stmt *delete_stmt,
    const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    struct mylite_insert_execution_state *state,
    size_t row_index,
    const struct mylite_dml_expression_callbacks *callbacks
) {
    struct mylite_insert_bound_value *values = NULL;
    int status = MYLITE_OK;

    if (database == NULL || plan == NULL || insert == NULL || delete_stmt == NULL ||
        table == NULL || column_indexes == NULL || state == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->column_count == 0U) {
        (void)mylite_diagnostics_set_error_message(database, "REPLACE target table has no columns");
        return MYLITE_EXEC_ERROR;
    }

    values = calloc(table->column_count, sizeof(*values));
    if (values == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_dml_resolve_insert_row_values(
        database,
        plan,
        schema_name,
        table,
        column_indexes->insert_columns,
        plan->row_count,
        state,
        row_index,
        values,
        callbacks
    );
    if (status == MYLITE_OK) {
        status = mylite_dml_write_replace_candidate_row(
            database,
            insert,
            delete_stmt,
            table,
            state,
            values
        );
    }

    mylite_dml_insert_bound_values_deinit(values, table->column_count);
    return status;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
int mylite_dml_execute_replace_set_row(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_set_plan *set_plan,
    sqlite3_stmt *insert,
    sqlite3_stmt *delete_stmt,
    const struct mylite_insert_table *table,
    const size_t *column_indexes,
    size_t column_index_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *values,
    struct mylite_insert_set_row_state *row_state,
    const struct mylite_dml_expression_callbacks *callbacks
) {
    int status = MYLITE_OK;

    if (database == NULL || values_plan == NULL || set_plan == NULL || insert == NULL ||
        delete_stmt == NULL || table == NULL || state == NULL || values == NULL ||
        row_state == NULL) {
        return MYLITE_MISUSE;
    }

    status = mylite_dml_resolve_insert_set_row_values(
        database,
        schema_name,
        values_plan,
        set_plan,
        table,
        column_indexes,
        column_index_count,
        1U,
        state,
        values,
        row_state,
        callbacks
    );
    if (status != MYLITE_OK) {
        return status;
    }
    return mylite_dml_write_replace_candidate_row(
        database,
        insert,
        delete_stmt,
        table,
        state,
        values
    );
}

static int delete_replace_conflict_row(
    mylite_db *database,
    sqlite3_stmt *delete_stmt,
    sqlite3_int64 rowid,
    struct mylite_insert_execution_state *state
) {
    int rc = SQLITE_OK;

    if (database == NULL || delete_stmt == NULL || state == NULL) {
        return MYLITE_MISUSE;
    }

    sqlite3_reset(delete_stmt);
    sqlite3_clear_bindings(delete_stmt);
    rc = sqlite3_bind_int64(delete_stmt, 1, rowid);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    rc = sqlite3_step(delete_stmt);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    ++state->duplicate_count;
    return MYLITE_OK;
}
