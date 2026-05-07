#include "mylite_dml.h"

#include "mylite_catalog.h"
#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_select_types.h"
#include "mylite_transactions.h"
#include "sql/mylite_expression.h"
#include "sqlite3.h"

#include <stdlib.h>

static int execute_update_row(
    mylite_db *database,
    sqlite3_stmt *update,
    const struct mylite_select_table *table,
    const struct mylite_insert_table *write_table,
    bool ignore,
    const struct mylite_update_bound_assignment *assignments,
    size_t assignment_count,
    const size_t *explicit_column_indexes,
    size_t explicit_column_count,
    const struct mylite_dml_expression_callbacks *callbacks,
    const struct mylite_update_row *stored,
    struct mylite_dml_timestamp_state *timestamp_state,
    uint64_t *next_auto_increment,
    int64_t *affected_rows
);

static int write_update_candidate(
    mylite_db *database,
    sqlite3_stmt *update,
    const struct mylite_insert_table *write_table,
    const struct mylite_update_row *candidate,
    uint64_t *next_auto_increment,
    int64_t *affected_rows
);

static int apply_update_assignments(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_insert_table *write_table,
    bool ignore,
    const struct mylite_update_bound_assignment *assignments,
    size_t assignment_count,
    const struct mylite_dml_expression_callbacks *callbacks,
    struct mylite_update_row *candidate
);

static int evaluate_update_assignment_value(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_insert_table *write_table,
    const struct mylite_update_row *candidate,
    bool ignore,
    size_t target_column,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_dml_expression_callbacks *callbacks,
    struct mylite_expression_value *out_value
);

static int copy_update_assignment_column_indexes(
    const struct mylite_update_bound_assignment *assignments,
    size_t assignment_count,
    size_t **out_column_indexes
);

int mylite_dml_execute_update_rows_transaction(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_insert_table *write_table,
    bool ignore,
    const struct mylite_update_bound_assignment *assignments,
    size_t assignment_count,
    const struct mylite_dml_expression_callbacks *callbacks,
    const struct mylite_update_rowset *rowset,
    int64_t *out_affected_rows
) {
    sqlite3_stmt *update = NULL;
    char *update_sql = NULL;
    struct mylite_statement_atomicity atomicity = {0};
    struct mylite_dml_timestamp_state timestamp_state = {0};
    size_t *explicit_column_indexes = NULL;
    uint64_t next_auto_increment = 0U;
    int64_t affected_rows = 0;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (database == NULL || table == NULL || write_table == NULL || callbacks == NULL ||
        rowset == NULL || out_affected_rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (assignment_count != 0U && assignments == NULL) {
        return MYLITE_MISUSE;
    }

    *out_affected_rows = 0;
    status = copy_update_assignment_column_indexes(
        assignments,
        assignment_count,
        &explicit_column_indexes
    );
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }

    next_auto_increment = write_table->next_auto_increment;
    status = mylite_transaction_begin_statement_atomicity(database, &atomicity);
    if (status != MYLITE_OK) {
        free(explicit_column_indexes);
        return status;
    }

    update_sql = mylite_dml_build_update_physical_sql(database, table);
    if (update_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        mylite_transaction_rollback_statement_atomicity(database, &atomicity);
        free(explicit_column_indexes);
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(
        database->sqlite,
        update_sql,
        -1,
        SQLITE_PREPARE_PERSISTENT,
        &update,
        NULL
    );
    sqlite3_free(update_sql);
    if (rc != SQLITE_OK) {
        mylite_transaction_rollback_statement_atomicity(database, &atomicity);
        free(explicit_column_indexes);
        return mylite_diagnostics_set_sqlite_error(database);
    }

    for (size_t index = 0U; index < rowset->row_count; ++index) {
        status = execute_update_row(
            database,
            update,
            table,
            write_table,
            ignore,
            assignments,
            assignment_count,
            explicit_column_indexes,
            assignment_count,
            callbacks,
            &rowset->rows[index],
            &timestamp_state,
            &next_auto_increment,
            &affected_rows
        );
        if (status != MYLITE_OK) {
            break;
        }
    }
    sqlite3_finalize(update);

    if (status == MYLITE_OK && write_table->has_auto_increment &&
        next_auto_increment > write_table->next_auto_increment) {
        status = mylite_transaction_update_table_auto_increment(
            database,
            table->schema_name,
            table->table_name,
            next_auto_increment
        );
    }
    if (status == MYLITE_OK && affected_rows > 0) {
        status = mylite_catalog_refresh_table_statistics_after_write(
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
            free(explicit_column_indexes);
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_statement_atomicity(database, &atomicity);
    free(explicit_column_indexes);
    return status;
}

static int execute_update_row(
    mylite_db *database,
    sqlite3_stmt *update,
    const struct mylite_select_table *table,
    const struct mylite_insert_table *write_table,
    bool ignore,
    const struct mylite_update_bound_assignment *assignments,
    size_t assignment_count,
    const size_t *explicit_column_indexes,
    size_t explicit_column_count,
    const struct mylite_dml_expression_callbacks *callbacks,
    const struct mylite_update_row *stored,
    struct mylite_dml_timestamp_state *timestamp_state,
    uint64_t *next_auto_increment,
    int64_t *affected_rows
) {
    struct mylite_update_row candidate = {0};
    bool ignored = false;
    bool row_changed = false;
    int status = mylite_dml_copy_update_candidate_values(database, stored, &candidate);

    if (status == MYLITE_OK) {
        status = apply_update_assignments(
            database,
            table,
            write_table,
            ignore,
            assignments,
            assignment_count,
            callbacks,
            &candidate
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_apply_update_on_update_current_timestamps(
            database,
            write_table,
            explicit_column_indexes,
            explicit_column_count,
            stored,
            &candidate,
            timestamp_state,
            &row_changed
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_update_check_constraints(
            database,
            table,
            write_table,
            &candidate,
            ignore,
            &ignored
        );
    }
    if (status == MYLITE_OK && ignored) {
        mylite_dml_update_row_deinit(&candidate);
        return MYLITE_OK;
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_update_unique_indexes(
            database,
            table,
            write_table,
            &candidate,
            ignore,
            &ignored
        );
    }
    if (status == MYLITE_OK && ignored) {
        mylite_dml_update_row_deinit(&candidate);
        return MYLITE_OK;
    }
    if (status == MYLITE_OK && row_changed) {
        status = mylite_dml_validate_update_child_foreign_keys(
            database,
            table,
            write_table,
            stored,
            &candidate,
            ignore,
            &ignored
        );
    }
    if (status == MYLITE_OK && ignored) {
        mylite_dml_update_row_deinit(&candidate);
        return MYLITE_OK;
    }
    if (status == MYLITE_OK && row_changed) {
        status = mylite_dml_validate_parent_update_foreign_keys(
            database,
            table,
            stored,
            &candidate,
            ignore,
            &ignored
        );
    }
    if (status == MYLITE_OK && ignored) {
        mylite_dml_update_row_deinit(&candidate);
        return MYLITE_OK;
    }
    if (status == MYLITE_OK && row_changed) {
        status =
            mylite_dml_apply_parent_update_foreign_key_actions(database, table, stored, &candidate);
    }
    if (status == MYLITE_OK && row_changed) {
        status = write_update_candidate(
            database,
            update,
            write_table,
            &candidate,
            next_auto_increment,
            affected_rows
        );
    }

    mylite_dml_update_row_deinit(&candidate);
    return status;
}

static int copy_update_assignment_column_indexes(
    const struct mylite_update_bound_assignment *assignments,
    size_t assignment_count,
    size_t **out_column_indexes
) {
    size_t *column_indexes = NULL;

    if (out_column_indexes == NULL) {
        return MYLITE_MISUSE;
    }
    *out_column_indexes = NULL;
    if (assignment_count == 0U) {
        return MYLITE_OK;
    }
    if (assignments == NULL) {
        return MYLITE_MISUSE;
    }

    column_indexes = calloc(assignment_count, sizeof(*column_indexes));
    if (column_indexes == NULL) {
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < assignment_count; ++index) {
        column_indexes[index] = assignments[index].column_index;
    }
    *out_column_indexes = column_indexes;
    return MYLITE_OK;
}

static int write_update_candidate(
    mylite_db *database,
    sqlite3_stmt *update,
    const struct mylite_insert_table *write_table,
    const struct mylite_update_row *candidate,
    uint64_t *next_auto_increment,
    int64_t *affected_rows
) {
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    sqlite3_reset(update);
    sqlite3_clear_bindings(update);
    status = mylite_dml_bind_update_row_values(database, update, candidate);
    if (status != MYLITE_OK) {
        return status;
    }

    rc = sqlite3_bind_int64(update, (int)candidate->value_count + 1, candidate->rowid);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    rc = sqlite3_step(update);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    ++*affected_rows;
    return mylite_dml_advance_update_auto_increment(
        database,
        write_table,
        candidate,
        next_auto_increment
    );
}

static int apply_update_assignments(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_insert_table *write_table,
    bool ignore,
    const struct mylite_update_bound_assignment *assignments,
    size_t assignment_count,
    const struct mylite_dml_expression_callbacks *callbacks,
    struct mylite_update_row *candidate
) {
    for (size_t index = 0U; index < assignment_count; ++index) {
        size_t column_index = assignments[index].column_index;
        struct mylite_expression_value value = {0};
        int status = MYLITE_OK;

        if (write_table->columns == NULL || candidate->values == NULL ||
            column_index >= write_table->column_count || column_index >= candidate->value_count) {
            return mylite_dml_set_update_unsupported_assignment_error(database);
        }

        status = evaluate_update_assignment_value(
            database,
            table,
            write_table,
            candidate,
            ignore,
            column_index,
            assignments[index].value,
            callbacks,
            &value
        );

        if (status != MYLITE_OK) {
            mylite_expression_value_deinit(&value);
            return status;
        }

        value.suppress_text_numeric_warnings = false;
        mylite_expression_value_deinit(&candidate->values[column_index]);
        candidate->values[column_index] = value;
    }
    return MYLITE_OK;
}

static int evaluate_update_assignment_value(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_insert_table *write_table,
    const struct mylite_update_row *candidate,
    bool ignore,
    size_t target_column,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_dml_expression_callbacks *callbacks,
    struct mylite_expression_value *out_value
) {
    const struct mylite_insert_table_column *column = &write_table->columns[target_column];
    struct mylite_update_expression_context user_context = {
        .database = database,
        .table = table,
        .write_table = write_table,
        .row = candidate,
        .callbacks = callbacks,
    };
    struct mylite_expression_eval_context context = {
        .user_data = &user_context,
        .real_as_float = mylite_connection_sql_mode_has_real_as_float(database),
        .character_set_connection = mylite_connection_character_set_connection(database),
        .resolve_identifier = mylite_dml_resolve_update_expression_identifier,
        .eval_session_function = mylite_dml_evaluate_session_function,
        .eval_subquery = mylite_dml_evaluate_subquery,
        .eval_default_function = mylite_dml_evaluate_default_function,
    };
    int status = MYLITE_OK;

    if (expression != NULL && expression->kind == MYLITE_SQL_AST_DEFAULT) {
        status = mylite_dml_resolve_update_default_value(database, column, out_value);
    } else if (
        expression != NULL && (expression->kind == MYLITE_SQL_AST_LITERAL &&
                               (expression->literal_kind == MYLITE_SQL_AST_LITERAL_HEX ||
                                expression->literal_kind == MYLITE_SQL_AST_LITERAL_BIT))
    ) {
        status = mylite_dml_resolve_update_binary_literal_value(
            database,
            column,
            expression,
            ignore,
            out_value
        );
    } else {
        size_t warning_start = database->warnings.count;
        int eval_status = mylite_expression_eval_with_context(
            expression,
            &context,
            &database->warnings,
            out_value
        );

        if (eval_status == 0) {
            status = MYLITE_OK;
        } else if (eval_status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            status = MYLITE_NOMEM;
        } else {
            status = mylite_dml_set_expression_condition_error(database, warning_start);
            if (status == MYLITE_OK) {
                status = mylite_dml_set_update_unsupported_assignment_error(database);
            }
        }
        if (status == MYLITE_OK) {
            status = mylite_dml_promote_expression_warnings(database, warning_start);
        }
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_update_assignment_value(database, column, ignore, out_value);
    }
    return status;
}
