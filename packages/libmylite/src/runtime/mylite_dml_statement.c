#include "mylite_dml_statement.h"

#include "mylite_diagnostics.h"
#include "mylite_dml.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_catalog.h"

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

int mylite_dml_execute_update_statement(
    mylite_stmt *stmt, const struct mylite_dml_expression_callbacks *expression_callbacks)
{
    struct mylite_select_table table = {0};
    struct mylite_insert_table write_table = {0};
    struct mylite_update_order_plan order_plan = {0};
    struct mylite_update_bound_assignment *assignments = NULL;
    struct mylite_update_rowset rowset = {0};
    size_t assignment_count = stmt->update.assignment_count;
    int64_t affected_rows = 0;
    int status = MYLITE_OK;

    stmt->affected_rows = 0;
    stmt->matched_rows = 0U;

    status = mylite_dml_copy_update_target_to_select_table(stmt->database, &stmt->update, &table);
    if (status == MYLITE_OK) {
        status = mylite_select_resolve_table_target(stmt->database, &table);
    }
    if (status == MYLITE_OK) {
        status = mylite_select_load_table_columns(stmt->database, &table);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_load_write_table(stmt->database, table.schema_name, table.table_name,
                                             &write_table);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_bind_update_subset(stmt->database, &stmt->update, &table, &assignments);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_bind_update_order_by_clause(stmt->database, &stmt->update, &table,
                                                        &order_plan);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_materialize_update_rows(stmt->database, &stmt->update, &table,
                                                    &order_plan, expression_callbacks, &rowset);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_sort_update_rowset(&rowset, &order_plan);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        }
    }
    if (status == MYLITE_OK) {
        mylite_dml_apply_update_limit(stmt->update.limit_clause, &rowset);
        stmt->matched_rows = rowset.row_count;
        status = mylite_dml_execute_update_rows_transaction(
            stmt->database, &table, &write_table, assignments, assignment_count,
            expression_callbacks, &rowset, &affected_rows);
        if (status == MYLITE_OK) {
            stmt->affected_rows = affected_rows;
        }
    }

    free(assignments);
    mylite_dml_update_rowset_deinit(&rowset);
    mylite_dml_update_order_plan_deinit(&order_plan);
    mylite_dml_insert_table_deinit(&write_table);
    mylite_select_table_deinit(&table);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
    }
    return status;
}

int mylite_dml_execute_delete_statement(
    mylite_stmt *stmt, const struct mylite_dml_expression_callbacks *expression_callbacks)
{
    struct mylite_select_table table = {0};
    struct mylite_update_order_plan order_plan = {0};
    struct mylite_update_rowset rowset = {0};
    int64_t affected_rows = 0;
    int status = MYLITE_OK;

    stmt->affected_rows = 0;

    status =
        mylite_dml_copy_delete_target_to_select_table(stmt->database, &stmt->delete_plan, &table);
    if (status == MYLITE_OK) {
        status = mylite_dml_resolve_delete_target(stmt->database, &table);
    }
    if (status == MYLITE_OK) {
        status = mylite_select_load_table_columns(stmt->database, &table);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_bind_delete_subset(stmt->database, &stmt->delete_plan, &table);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_bind_delete_order_by_clause(stmt->database, &stmt->delete_plan, &table,
                                                        &order_plan);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_materialize_delete_rows(stmt->database, &stmt->delete_plan, &table,
                                                    &order_plan, expression_callbacks, &rowset);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_sort_update_rowset(&rowset, &order_plan);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        }
    }
    if (status == MYLITE_OK) {
        mylite_dml_apply_update_limit(stmt->delete_plan.limit_clause, &rowset);
        status = mylite_dml_execute_delete_rows_transaction(stmt->database, &table, &rowset,
                                                            &affected_rows);
        if (status == MYLITE_OK) {
            stmt->affected_rows = affected_rows;
        }
    }

    mylite_dml_update_rowset_deinit(&rowset);
    mylite_dml_update_order_plan_deinit(&order_plan);
    mylite_select_table_deinit(&table);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
    }
    return status;
}
