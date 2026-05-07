#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_dml_insert_conflict.h"
#include "mylite_dml_insert_set_row_resolve.h"
#include "mylite_error_codes.h"

#include <stdlib.h>

static int execute_insert_update_bound_row(
    mylite_db *database,
    const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan,
    sqlite3_stmt *insert,
    const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    struct mylite_insert_execution_state *state,
    uint64_t row_number,
    const struct mylite_insert_bound_value *values,
    const struct mylite_dml_expression_callbacks *callbacks
);

static int append_insert_values_deprecated_warning(mylite_db *database);

int mylite_dml_execute_insert_update_values_row(
    mylite_db *database,
    const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan,
    sqlite3_stmt *insert,
    const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    struct mylite_insert_execution_state *state,
    size_t row_index,
    const struct mylite_dml_expression_callbacks *callbacks
) {
    struct mylite_insert_bound_value *values = NULL;
    const char *schema_name = NULL;
    int status = MYLITE_OK;

    if (database == NULL || values_plan == NULL || update_plan == NULL || insert == NULL ||
        table == NULL || column_indexes == NULL || state == NULL ||
        (values_plan->schema_name == NULL && selected_schema == NULL)) {
        return MYLITE_MISUSE;
    }
    if (table->column_count == 0U) {
        (void)mylite_diagnostics_set_error_message(database, "INSERT target table has no columns");
        return MYLITE_EXEC_ERROR;
    }

    values = calloc(table->column_count, sizeof(*values));
    if (values == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    schema_name = values_plan->schema_name == NULL ? selected_schema : values_plan->schema_name;
    status = mylite_dml_resolve_insert_row_values(
        database,
        values_plan,
        schema_name,
        table,
        column_indexes->insert_columns,
        values_plan->row_count,
        state,
        row_index,
        values,
        callbacks
    );
    if (status == MYLITE_OK) {
        status = execute_insert_update_bound_row(
            database,
            selected_schema,
            values_plan,
            update_plan,
            insert,
            table,
            column_indexes,
            state,
            row_index + 1U,
            values,
            callbacks
        );
    }

    mylite_dml_insert_bound_values_deinit(values, table->column_count);
    return status;
}

int mylite_dml_execute_insert_update_set_row(
    mylite_db *database,
    const char *selected_schema,
    const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_set_plan *set_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan,
    sqlite3_stmt *insert,
    const struct mylite_insert_table *table,
    const size_t *column_indexes,
    size_t column_index_count,
    const struct mylite_insert_row_column_indexes *row_column_indexes,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *values,
    struct mylite_insert_set_row_state *row_state,
    const struct mylite_dml_expression_callbacks *callbacks
) {
    int status = MYLITE_OK;

    if (database == NULL || schema_name == NULL || values_plan == NULL || set_plan == NULL ||
        update_plan == NULL || insert == NULL || table == NULL || row_column_indexes == NULL ||
        state == NULL || values == NULL || row_state == NULL ||
        (values_plan->schema_name == NULL && selected_schema == NULL)) {
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
    if (status == MYLITE_OK) {
        return execute_insert_update_bound_row(
            database,
            selected_schema,
            values_plan,
            update_plan,
            insert,
            table,
            row_column_indexes,
            state,
            1U,
            values,
            callbacks
        );
    }
    return status;
}

static int execute_insert_update_bound_row(
    mylite_db *database,
    const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan,
    sqlite3_stmt *insert,
    const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    struct mylite_insert_execution_state *state,
    uint64_t row_number,
    const struct mylite_insert_bound_value *values,
    const struct mylite_dml_expression_callbacks *callbacks
) {
    struct mylite_insert_unique_conflict conflict = {0};
    struct mylite_insert_bound_value *stored_values = NULL;
    struct mylite_insert_bound_value *updated_values = NULL;
    bool update_conflicts = false;
    bool row_changed = false;
    bool ignored = false;
    int status = MYLITE_OK;
    const char *schema_name =
        values_plan->schema_name == NULL ? selected_schema : values_plan->schema_name;

    if (database == NULL || values_plan == NULL || update_plan == NULL || insert == NULL ||
        table == NULL || column_indexes == NULL || column_indexes->update_columns == NULL ||
        state == NULL || values == NULL ||
        (values_plan->schema_name == NULL && selected_schema == NULL)) {
        return MYLITE_MISUSE;
    }

    status = mylite_dml_validate_insert_check_constraints(
        database,
        schema_name,
        values_plan->table_name,
        values_plan->ignore,
        table,
        values,
        &ignored
    );
    if (status != MYLITE_OK || ignored) {
        return status;
    }

    status = mylite_dml_find_insert_unique_conflict(database, table, values, &conflict);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!conflict.conflicts) {
        status = mylite_dml_validate_insert_child_foreign_keys(
            database,
            schema_name,
            values_plan->table_name,
            values_plan->ignore,
            table,
            values,
            &ignored
        );
        if (status != MYLITE_OK || ignored) {
            return status;
        }
        return mylite_dml_write_insert_candidate_row(database, insert, table, values, state);
    }

    ++state->duplicate_count;
    stored_values = calloc(table->column_count, sizeof(*stored_values));
    if (stored_values == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_dml_load_insert_conflict_row(database, table, conflict.rowid, stored_values);
    if (status == MYLITE_OK) {
        status = mylite_dml_copy_insert_bound_values(
            database,
            stored_values,
            table->column_count,
            &updated_values
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_apply_insert_update_assignments(
            database,
            selected_schema,
            values_plan,
            update_plan,
            table,
            column_indexes,
            state,
            row_number,
            values,
            updated_values,
            callbacks
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_apply_insert_on_update_current_timestamps(
            database,
            table,
            column_indexes->update_columns,
            update_plan->assignment_count,
            stored_values,
            updated_values,
            &state->timestamp_state,
            &row_changed
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_insert_check_constraints(
            database,
            schema_name,
            values_plan->table_name,
            values_plan->ignore,
            table,
            updated_values,
            &ignored
        );
    }
    if (status == MYLITE_OK && !ignored) {
        status = mylite_dml_validate_insert_update_unique_indexes(
            database,
            values_plan->table_name,
            values_plan->ignore,
            table,
            updated_values,
            conflict.rowid,
            &update_conflicts
        );
    }
    if (status == MYLITE_OK && !update_conflicts && row_changed) {
        status = mylite_dml_validate_insert_child_foreign_keys(
            database,
            schema_name,
            values_plan->table_name,
            values_plan->ignore,
            table,
            updated_values,
            &ignored
        );
    }
    if (status == MYLITE_OK && !ignored && !update_conflicts && row_changed) {
        status = mylite_dml_write_insert_update_candidate(
            database,
            table,
            conflict.rowid,
            updated_values,
            state
        );
        if (status == MYLITE_OK) {
            state->accepted_row_count += 2U;
        }
    }

    mylite_dml_insert_bound_values_deinit(stored_values, table->column_count);
    mylite_dml_insert_bound_values_deinit(updated_values, table->column_count);
    return status;
}

int mylite_dml_append_insert_update_deprecated_warnings(
    mylite_db *database,
    const struct mylite_insert_duplicate_update_plan *plan
) {
    size_t warning_count = 0U;

    if (database == NULL || plan == NULL) {
        return MYLITE_MISUSE;
    }
    if (!plan->has_clause) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; index < plan->assignment_count; ++index) {
        warning_count += plan->assignments[index].value.values_function_count;
    }
    for (size_t index = 0U; index < warning_count; ++index) {
        int status = append_insert_values_deprecated_warning(database);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int append_insert_values_deprecated_warning(mylite_db *database) {
    static const char message[] =
        "'VALUES function' is deprecated and will be removed in a future release. Please use an "
        "alias (INSERT INTO ... VALUES (...) AS alias) and replace VALUES(col) in the ON "
        "DUPLICATE KEY UPDATE clause with alias.col instead";

    return mylite_diagnostics_append_warning(
        database,
        MYLITE_MYSQL_ER_WARN_DEPRECATED_SYNTAX,
        message
    );
}
