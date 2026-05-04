#include "mylite_dml_statement.h"

#include "mylite_dml.h"
#include "mylite_runtime.h"

#include <stdlib.h>

int mylite_dml_execute_insert_values_statement(mylite_stmt *stmt)
{
    const char *schema_name = NULL;
    struct mylite_insert_table table = {0};
    struct mylite_insert_transaction_result result = {0};
    size_t *column_indexes = NULL;
    size_t *update_column_indexes = NULL;
    int status = mylite_dml_validate_insert_target(stmt->database, stmt->database->selected_schema,
                                                   &stmt->insert_values, &schema_name);

    stmt->affected_rows = 0;
    if (status != MYLITE_OK) {
        return status;
    }

    status = mylite_dml_load_write_table(stmt->database, schema_name,
                                         stmt->insert_values.table_name, &table);
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_insert_column_list(stmt->database, &stmt->insert_values,
                                                        &table, &column_indexes);
    }
    if (status == MYLITE_OK) {
        size_t source_column_count = table.column_count;

        if (stmt->insert_values.has_column_list) {
            source_column_count = stmt->insert_values.column_count;
        }

        status = mylite_dml_validate_insert_row_alias(stmt->database, &stmt->insert_values,
                                                      source_column_count);
        if (status != MYLITE_OK) {
            goto cleanup;
        }
        status = mylite_dml_validate_insert_update_assignments(
            stmt->database, &stmt->insert_values, &stmt->insert_update, &table, schema_name,
            column_indexes, source_column_count, &update_column_indexes);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_execute_insert_values_transaction(
            stmt->database, stmt->database->selected_schema, schema_name, &stmt->insert_values,
            &stmt->insert_update, &table, column_indexes, update_column_indexes, &result);
    }

cleanup:
    free(update_column_indexes);
    free(column_indexes);
    mylite_dml_insert_table_deinit(&table);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
    } else {
        stmt->affected_rows = result.affected_rows;
        if (result.generated_insert_id) {
            stmt->database->last_insert_id = result.last_insert_id;
        }
    }
    return status;
}

int mylite_dml_execute_insert_set_statement(mylite_stmt *stmt)
{
    const char *schema_name = NULL;
    struct mylite_insert_table table = {0};
    struct mylite_insert_transaction_result result = {0};
    size_t *column_indexes = NULL;
    size_t *update_column_indexes = NULL;
    size_t column_index_count = 0U;
    int status = mylite_dml_validate_insert_target(stmt->database, stmt->database->selected_schema,
                                                   &stmt->insert_values, &schema_name);

    stmt->affected_rows = 0;
    if (status != MYLITE_OK) {
        return status;
    }

    status = mylite_dml_load_write_table(stmt->database, schema_name,
                                         stmt->insert_values.table_name, &table);
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_insert_set_assignments(stmt->database, &stmt->insert_values,
                                                            &stmt->insert_set, &table, schema_name,
                                                            &column_indexes, &column_index_count);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_insert_row_alias(stmt->database, &stmt->insert_values,
                                                      column_index_count);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_insert_update_assignments(
            stmt->database, &stmt->insert_values, &stmt->insert_update, &table, schema_name,
            column_indexes, column_index_count, &update_column_indexes);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_execute_insert_set_transaction(
            stmt->database, stmt->database->selected_schema, schema_name, &stmt->insert_values,
            &stmt->insert_set, &stmt->insert_update, &table, column_indexes, column_index_count,
            update_column_indexes, &result);
    }

    free(update_column_indexes);
    free(column_indexes);
    mylite_dml_insert_table_deinit(&table);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
    } else {
        stmt->affected_rows = result.affected_rows;
        if (result.generated_insert_id) {
            stmt->database->last_insert_id = result.last_insert_id;
        }
    }
    return status;
}

int mylite_dml_execute_replace_values_statement(mylite_stmt *stmt)
{
    const char *schema_name = NULL;
    struct mylite_insert_table table = {0};
    struct mylite_insert_transaction_result result = {0};
    size_t *column_indexes = NULL;
    int status = mylite_dml_validate_insert_target(stmt->database, stmt->database->selected_schema,
                                                   &stmt->insert_values, &schema_name);

    stmt->affected_rows = 0;
    if (status != MYLITE_OK) {
        return status;
    }

    status = mylite_dml_load_write_table(stmt->database, schema_name,
                                         stmt->insert_values.table_name, &table);
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_insert_column_list(stmt->database, &stmt->insert_values,
                                                        &table, &column_indexes);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_execute_replace_values_transaction(
            stmt->database, schema_name, &stmt->insert_values, &table, column_indexes, &result);
    }

    free(column_indexes);
    mylite_dml_insert_table_deinit(&table);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
    } else {
        stmt->affected_rows = result.affected_rows;
        if (result.generated_insert_id) {
            stmt->database->last_insert_id = result.last_insert_id;
        }
    }
    return status;
}

int mylite_dml_execute_replace_set_statement(mylite_stmt *stmt)
{
    const char *schema_name = NULL;
    struct mylite_insert_table table = {0};
    struct mylite_insert_transaction_result result = {0};
    size_t *column_indexes = NULL;
    size_t column_index_count = 0U;
    int status = mylite_dml_validate_insert_target(stmt->database, stmt->database->selected_schema,
                                                   &stmt->insert_values, &schema_name);

    stmt->affected_rows = 0;
    if (status != MYLITE_OK) {
        return status;
    }

    status = mylite_dml_load_write_table(stmt->database, schema_name,
                                         stmt->insert_values.table_name, &table);
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_insert_set_assignments(stmt->database, &stmt->insert_values,
                                                            &stmt->insert_set, &table, schema_name,
                                                            &column_indexes, &column_index_count);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_execute_replace_set_transaction(
            stmt->database, schema_name, &stmt->insert_values, &stmt->insert_set, &table,
            column_indexes, column_index_count, &result);
    }

    free(column_indexes);
    mylite_dml_insert_table_deinit(&table);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
    } else {
        stmt->affected_rows = result.affected_rows;
        if (result.generated_insert_id) {
            stmt->database->last_insert_id = result.last_insert_id;
        }
    }
    return status;
}
