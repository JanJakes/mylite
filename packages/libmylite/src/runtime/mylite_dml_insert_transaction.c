#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_dml_insert_replace_execute.h"
#include "mylite_dml_insert_transaction_finish.h"
#include "mylite_transactions.h"
#include "sqlite3.h"

#include <stdlib.h>

int mylite_dml_execute_insert_values_transaction(
    mylite_db *database,
    const char *selected_schema,
    const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan,
    const struct mylite_insert_table *table,
    const size_t *column_indexes,
    const size_t *update_column_indexes,
    const struct mylite_dml_expression_callbacks *callbacks,
    struct mylite_insert_transaction_result *out_result
) {
    size_t source_column_count = table == NULL ? 0U : table->column_count;
    struct mylite_insert_execution_state state = {
        .next_auto_increment = table == NULL ? 0U : table->next_auto_increment,
    };
    sqlite3_stmt *insert = NULL;
    char *insert_sql = NULL;
    struct mylite_statement_atomicity atomicity = {0};
    struct mylite_insert_row_column_indexes row_column_indexes = {
        .insert_columns = column_indexes,
        .update_columns = update_column_indexes,
    };
    int status = MYLITE_OK;
    int rc = SQLITE_OK;

    if (database == NULL || schema_name == NULL || values_plan == NULL || update_plan == NULL ||
        table == NULL || out_result == NULL ||
        (update_plan->has_clause && values_plan->schema_name == NULL && selected_schema == NULL)) {
        return MYLITE_MISUSE;
    }

    *out_result = (struct mylite_insert_transaction_result){0};
    status = mylite_transaction_begin_statement_atomicity(database, &atomicity);
    if (status != MYLITE_OK) {
        return status;
    }
    if (values_plan->has_column_list) {
        source_column_count = values_plan->column_count;
    }
    row_column_indexes.source_column_count = source_column_count;

    status =
        mylite_dml_initialize_insert_ignore_warning_state(database, values_plan, table, &state);
    if (status != MYLITE_OK) {
        mylite_transaction_rollback_statement_atomicity(database, &atomicity);
        return status;
    }
    status = mylite_dml_append_insert_update_deprecated_warnings(database, update_plan);
    if (status != MYLITE_OK) {
        mylite_dml_insert_execution_state_deinit(&state);
        mylite_transaction_rollback_statement_atomicity(database, &atomicity);
        return status;
    }

    insert_sql = mylite_dml_build_insert_physical_sql(database, table);
    if (insert_sql == NULL) {
        mylite_dml_insert_execution_state_deinit(&state);
        mylite_transaction_rollback_statement_atomicity(database, &atomicity);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(
        database->sqlite,
        insert_sql,
        -1,
        SQLITE_PREPARE_PERSISTENT,
        &insert,
        NULL
    );
    sqlite3_free(insert_sql);
    if (rc != SQLITE_OK) {
        mylite_dml_insert_execution_state_deinit(&state);
        mylite_transaction_rollback_statement_atomicity(database, &atomicity);
        return mylite_diagnostics_set_sqlite_error(database);
    }

    for (size_t row_index = 0U; row_index < values_plan->row_count; ++row_index) {
        if (update_plan->has_clause) {
            status = mylite_dml_execute_insert_update_values_row(
                database,
                selected_schema,
                values_plan,
                update_plan,
                insert,
                table,
                &row_column_indexes,
                &state,
                row_index,
                callbacks
            );
        } else {
            status = mylite_dml_execute_insert_row(
                database,
                values_plan,
                schema_name,
                insert,
                table,
                &row_column_indexes,
                &state,
                row_index,
                callbacks
            );
        }
        if (status != MYLITE_OK) {
            break;
        }
    }
    sqlite3_finalize(insert);

    if (status != MYLITE_OK) {
        int final_status = mylite_dml_finish_failed_insert_transaction(
            database,
            schema_name,
            values_plan->table_name,
            table,
            &state,
            &atomicity,
            status
        );

        mylite_dml_insert_execution_state_deinit(&state);
        return final_status;
    }

    status = mylite_dml_finish_successful_insert_transaction(
        database,
        schema_name,
        values_plan->table_name,
        table,
        &state,
        &atomicity,
        out_result
    );
    mylite_dml_insert_execution_state_deinit(&state);
    return status;
}

int mylite_dml_execute_insert_set_transaction(
    mylite_db *database,
    const char *selected_schema,
    const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_set_plan *set_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan,
    const struct mylite_insert_table *table,
    const size_t *column_indexes,
    size_t column_index_count,
    const size_t *update_column_indexes,
    const struct mylite_dml_expression_callbacks *callbacks,
    struct mylite_insert_transaction_result *out_result
) {
    struct mylite_insert_execution_state state = {
        .next_auto_increment = table == NULL ? 0U : table->next_auto_increment,
    };
    struct mylite_insert_set_row_state row_state = {0};
    struct mylite_insert_bound_value *values = NULL;
    const struct mylite_insert_row_column_indexes row_column_indexes = {
        .insert_columns = column_indexes,
        .update_columns = update_column_indexes,
        .source_column_count = column_index_count,
    };
    sqlite3_stmt *insert = NULL;
    char *insert_sql = NULL;
    struct mylite_statement_atomicity atomicity = {0};
    int status = MYLITE_OK;
    int rc = SQLITE_OK;

    if (database == NULL || schema_name == NULL || values_plan == NULL || set_plan == NULL ||
        update_plan == NULL || table == NULL || out_result == NULL ||
        (update_plan->has_clause && values_plan->schema_name == NULL && selected_schema == NULL)) {
        return MYLITE_MISUSE;
    }

    *out_result = (struct mylite_insert_transaction_result){0};
    if (table->column_count == 0U) {
        (void)mylite_diagnostics_set_error_message(database, "INSERT target table has no columns");
        return MYLITE_EXEC_ERROR;
    }

    status = mylite_transaction_begin_statement_atomicity(database, &atomicity);
    if (status != MYLITE_OK) {
        return status;
    }
    status =
        mylite_dml_initialize_insert_ignore_warning_state(database, values_plan, table, &state);
    if (status != MYLITE_OK) {
        mylite_transaction_rollback_statement_atomicity(database, &atomicity);
        return status;
    }
    status = mylite_dml_append_insert_update_deprecated_warnings(database, update_plan);
    if (status != MYLITE_OK) {
        mylite_dml_insert_execution_state_deinit(&state);
        mylite_transaction_rollback_statement_atomicity(database, &atomicity);
        return status;
    }

    values = calloc(table->column_count, sizeof(*values));
    row_state.generate_auto_increment =
        calloc(table->column_count, sizeof(*row_state.generate_auto_increment));
    row_state.assigned_columns = calloc(table->column_count, sizeof(*row_state.assigned_columns));
    if (values == NULL || row_state.generate_auto_increment == NULL ||
        row_state.assigned_columns == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        status = MYLITE_NOMEM;
        goto cleanup;
    }

    insert_sql = mylite_dml_build_insert_physical_sql(database, table);
    if (insert_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        status = MYLITE_NOMEM;
        goto cleanup;
    }

    rc = sqlite3_prepare_v3(
        database->sqlite,
        insert_sql,
        -1,
        SQLITE_PREPARE_PERSISTENT,
        &insert,
        NULL
    );
    sqlite3_free(insert_sql);
    insert_sql = NULL;
    if (rc != SQLITE_OK) {
        status = mylite_diagnostics_set_sqlite_error(database);
        goto cleanup;
    }

    if (update_plan->has_clause) {
        status = mylite_dml_execute_insert_update_set_row(
            database,
            selected_schema,
            schema_name,
            values_plan,
            set_plan,
            update_plan,
            insert,
            table,
            column_indexes,
            column_index_count,
            &row_column_indexes,
            &state,
            values,
            &row_state,
            callbacks
        );
    } else {
        status = mylite_dml_execute_insert_set_row(
            database,
            schema_name,
            values_plan,
            set_plan,
            insert,
            table,
            column_indexes,
            column_index_count,
            &state,
            values,
            &row_state,
            callbacks
        );
    }

cleanup:
    sqlite3_free(insert_sql);
    sqlite3_finalize(insert);
    mylite_dml_insert_bound_values_deinit(values, table->column_count);
    free(row_state.generate_auto_increment);
    free(row_state.assigned_columns);

    if (status != MYLITE_OK) {
        int final_status = mylite_dml_finish_failed_insert_transaction(
            database,
            schema_name,
            values_plan->table_name,
            table,
            &state,
            &atomicity,
            status
        );

        mylite_dml_insert_execution_state_deinit(&state);
        return final_status;
    }

    status = mylite_dml_finish_successful_insert_transaction(
        database,
        schema_name,
        values_plan->table_name,
        table,
        &state,
        &atomicity,
        out_result
    );
    mylite_dml_insert_execution_state_deinit(&state);
    return status;
}

int mylite_dml_execute_replace_values_transaction(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_table *table,
    const size_t *column_indexes,
    const struct mylite_dml_expression_callbacks *callbacks,
    struct mylite_insert_transaction_result *out_result
) {
    size_t source_column_count = table == NULL ? 0U : table->column_count;
    struct mylite_insert_execution_state state = {
        .next_auto_increment = table == NULL ? 0U : table->next_auto_increment,
    };
    struct mylite_insert_row_column_indexes row_column_indexes = {
        .insert_columns = column_indexes,
    };
    sqlite3_stmt *insert = NULL;
    sqlite3_stmt *delete_stmt = NULL;
    char *insert_sql = NULL;
    char *delete_sql = NULL;
    struct mylite_statement_atomicity atomicity = {0};
    int status = MYLITE_OK;
    int rc = SQLITE_OK;

    if (database == NULL || schema_name == NULL || values_plan == NULL || table == NULL ||
        out_result == NULL) {
        return MYLITE_MISUSE;
    }

    *out_result = (struct mylite_insert_transaction_result){0};
    status = mylite_transaction_begin_statement_atomicity(database, &atomicity);
    if (status != MYLITE_OK) {
        return status;
    }
    if (values_plan->has_column_list) {
        source_column_count = values_plan->column_count;
    }
    row_column_indexes.source_column_count = source_column_count;

    insert_sql = mylite_dml_build_insert_physical_sql(database, table);
    delete_sql = mylite_dml_build_replace_delete_sql(database, table);
    if (insert_sql == NULL || delete_sql == NULL) {
        sqlite3_free(insert_sql);
        sqlite3_free(delete_sql);
        mylite_transaction_rollback_statement_atomicity(database, &atomicity);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(
        database->sqlite,
        insert_sql,
        -1,
        SQLITE_PREPARE_PERSISTENT,
        &insert,
        NULL
    );
    sqlite3_free(insert_sql);
    insert_sql = NULL;
    if (rc == SQLITE_OK) {
        rc = sqlite3_prepare_v3(
            database->sqlite,
            delete_sql,
            -1,
            SQLITE_PREPARE_PERSISTENT,
            &delete_stmt,
            NULL
        );
    }
    sqlite3_free(delete_sql);
    delete_sql = NULL;
    if (rc != SQLITE_OK) {
        sqlite3_finalize(insert);
        mylite_transaction_rollback_statement_atomicity(database, &atomicity);
        return mylite_diagnostics_set_sqlite_error(database);
    }

    for (size_t row_index = 0U; row_index < values_plan->row_count; ++row_index) {
        status = mylite_dml_execute_replace_row(
            database,
            values_plan,
            schema_name,
            insert,
            delete_stmt,
            table,
            &row_column_indexes,
            &state,
            row_index,
            callbacks
        );
        if (status != MYLITE_OK) {
            break;
        }
    }
    sqlite3_finalize(delete_stmt);
    sqlite3_finalize(insert);

    if (status != MYLITE_OK) {
        return mylite_dml_finish_failed_insert_transaction(
            database,
            schema_name,
            values_plan->table_name,
            table,
            &state,
            &atomicity,
            status
        );
    }
    return mylite_dml_finish_successful_replace_transaction(
        database,
        schema_name,
        values_plan->table_name,
        table,
        &state,
        &atomicity,
        out_result
    );
}

int mylite_dml_execute_replace_set_transaction(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_set_plan *set_plan,
    const struct mylite_insert_table *table,
    const size_t *column_indexes,
    size_t column_index_count,
    const struct mylite_dml_expression_callbacks *callbacks,
    struct mylite_insert_transaction_result *out_result
) {
    struct mylite_insert_execution_state state = {
        .next_auto_increment = table == NULL ? 0U : table->next_auto_increment,
    };
    struct mylite_insert_set_row_state row_state = {0};
    struct mylite_insert_bound_value *values = NULL;
    sqlite3_stmt *insert = NULL;
    sqlite3_stmt *delete_stmt = NULL;
    char *insert_sql = NULL;
    char *delete_sql = NULL;
    struct mylite_statement_atomicity atomicity = {0};
    int status = MYLITE_OK;
    int rc = SQLITE_OK;

    if (database == NULL || schema_name == NULL || values_plan == NULL || set_plan == NULL ||
        table == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }

    *out_result = (struct mylite_insert_transaction_result){0};
    if (table->column_count == 0U) {
        (void)mylite_diagnostics_set_error_message(database, "REPLACE target table has no columns");
        return MYLITE_EXEC_ERROR;
    }

    status = mylite_transaction_begin_statement_atomicity(database, &atomicity);
    if (status != MYLITE_OK) {
        return status;
    }
    values = calloc(table->column_count, sizeof(*values));
    row_state.generate_auto_increment =
        calloc(table->column_count, sizeof(*row_state.generate_auto_increment));
    row_state.assigned_columns = calloc(table->column_count, sizeof(*row_state.assigned_columns));
    if (values == NULL || row_state.generate_auto_increment == NULL ||
        row_state.assigned_columns == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        status = MYLITE_NOMEM;
        goto cleanup;
    }

    insert_sql = mylite_dml_build_insert_physical_sql(database, table);
    delete_sql = mylite_dml_build_replace_delete_sql(database, table);
    if (insert_sql == NULL || delete_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        status = MYLITE_NOMEM;
        goto cleanup;
    }

    rc = sqlite3_prepare_v3(
        database->sqlite,
        insert_sql,
        -1,
        SQLITE_PREPARE_PERSISTENT,
        &insert,
        NULL
    );
    if (rc == SQLITE_OK) {
        rc = sqlite3_prepare_v3(
            database->sqlite,
            delete_sql,
            -1,
            SQLITE_PREPARE_PERSISTENT,
            &delete_stmt,
            NULL
        );
    }
    if (rc != SQLITE_OK) {
        status = mylite_diagnostics_set_sqlite_error(database);
        goto cleanup;
    }

    status = mylite_dml_execute_replace_set_row(
        database,
        schema_name,
        values_plan,
        set_plan,
        insert,
        delete_stmt,
        table,
        column_indexes,
        column_index_count,
        &state,
        values,
        &row_state,
        callbacks
    );

cleanup:
    sqlite3_free(insert_sql);
    sqlite3_free(delete_sql);
    sqlite3_finalize(delete_stmt);
    sqlite3_finalize(insert);
    mylite_dml_insert_bound_values_deinit(values, table->column_count);
    free(row_state.generate_auto_increment);
    free(row_state.assigned_columns);

    if (status != MYLITE_OK) {
        return mylite_dml_finish_failed_insert_transaction(
            database,
            schema_name,
            values_plan->table_name,
            table,
            &state,
            &atomicity,
            status
        );
    }
    return mylite_dml_finish_successful_replace_transaction(
        database,
        schema_name,
        values_plan->table_name,
        table,
        &state,
        &atomicity,
        out_result
    );
}
