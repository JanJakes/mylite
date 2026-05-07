#include "mylite_dml_statement.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_dml.h"
#include "mylite_error_codes.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_catalog.h"
#include "mylite_select_context.h"
#include "mylite_select_from.h"
#include "mylite_select_join_materialize.h"
#include "mylite_select_predicate_bind.h"
#include "mylite_select_rowset.h"
#include "mylite_span.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct multi_delete_target_duplicate_search {
    size_t target_index;
    size_t table_index;
};

static int execute_single_delete_statement(
    mylite_stmt *stmt,
    const struct mylite_dml_expression_callbacks *expression_callbacks
);

static int execute_multi_delete_statement(mylite_stmt *stmt);

static int bind_multi_delete_select_plan(mylite_stmt *stmt, mylite_stmt *joined_stmt);

static int resolve_multi_delete_targets(
    mylite_db *database,
    const struct mylite_delete_plan *delete_plan,
    const struct mylite_select_plan *plan,
    size_t **out_target_table_indexes
);

static int resolve_multi_delete_target(
    mylite_db *database,
    const struct mylite_delete_target *target,
    const struct mylite_select_plan *plan,
    size_t *out_table_index
);

static bool multi_delete_target_matches_table(
    const struct mylite_delete_target *target,
    const struct mylite_select_table *table
);

static bool multi_delete_target_is_duplicate(
    const size_t *target_table_indexes,
    struct multi_delete_target_duplicate_search search
);

static int materialize_multi_delete_rowsets(
    mylite_db *database,
    const struct mylite_table_select_result *result,
    const size_t *target_table_indexes,
    size_t target_count,
    struct mylite_update_rowset **out_rowsets
);

static int add_multi_delete_result_row(
    mylite_db *database,
    const struct mylite_table_select_row *row,
    const size_t *target_table_indexes,
    size_t target_count,
    struct mylite_update_rowset *rowsets
);

static int add_unique_multi_delete_rowid(
    mylite_db *database,
    struct mylite_update_rowset *rowset,
    int64_t rowid
);

static bool multi_delete_rowset_contains_rowid(
    const struct mylite_update_rowset *rowset,
    int64_t rowid
);

static int set_multi_delete_unknown_target_error(
    mylite_db *database,
    const struct mylite_delete_target *target
);

static int set_multi_delete_nonunique_target_error(
    mylite_db *database,
    const struct mylite_delete_target *target
);

int mylite_dml_execute_insert_values_statement(
    mylite_stmt *stmt,
    const struct mylite_dml_expression_callbacks *expression_callbacks
) {
    const char *schema_name = NULL;
    struct mylite_insert_table table = {0};
    struct mylite_insert_transaction_result result = {0};
    size_t *column_indexes = NULL;
    size_t *update_column_indexes = NULL;
    unsigned int write_table_flags = 0U;
    int status = mylite_dml_validate_insert_target(
        stmt->database,
        stmt->database->selected_schema,
        &stmt->insert_values,
        &schema_name
    );

    stmt->affected_rows = 0;
    if (status != MYLITE_OK) {
        return status;
    }

    if (stmt->insert_values.ignore) {
        write_table_flags |= MYLITE_DML_WRITE_TABLE_ALLOW_ZERO_TEMPORAL;
    }
    status = mylite_dml_load_write_table(
        stmt->database,
        schema_name,
        stmt->insert_values.table_name,
        write_table_flags,
        &table
    );
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_insert_column_list(
            stmt->database,
            &stmt->insert_values,
            &table,
            &column_indexes
        );
    }
    if (status == MYLITE_OK) {
        size_t source_column_count = table.column_count;

        if (stmt->insert_values.has_column_list) {
            source_column_count = stmt->insert_values.column_count;
        }

        status = mylite_dml_validate_insert_row_alias(
            stmt->database,
            &stmt->insert_values,
            source_column_count
        );
        if (status != MYLITE_OK) {
            goto cleanup;
        }
        status = mylite_dml_validate_insert_update_assignments(
            stmt->database,
            &stmt->insert_values,
            &stmt->insert_update,
            &table,
            schema_name,
            column_indexes,
            source_column_count,
            &update_column_indexes
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_execute_insert_values_transaction(
            stmt->database,
            stmt->database->selected_schema,
            schema_name,
            &stmt->insert_values,
            &stmt->insert_update,
            &table,
            column_indexes,
            update_column_indexes,
            expression_callbacks,
            &result
        );
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
            mylite_connection_set_last_insert_id(stmt->database, result.last_insert_id);
        }
    }
    return status;
}

int mylite_dml_execute_insert_set_statement(
    mylite_stmt *stmt,
    const struct mylite_dml_expression_callbacks *expression_callbacks
) {
    const char *schema_name = NULL;
    struct mylite_insert_table table = {0};
    struct mylite_insert_transaction_result result = {0};
    size_t *column_indexes = NULL;
    size_t *update_column_indexes = NULL;
    size_t column_index_count = 0U;
    unsigned int write_table_flags = 0U;
    int status = mylite_dml_validate_insert_target(
        stmt->database,
        stmt->database->selected_schema,
        &stmt->insert_values,
        &schema_name
    );

    stmt->affected_rows = 0;
    if (status != MYLITE_OK) {
        return status;
    }

    if (stmt->insert_values.ignore) {
        write_table_flags |= MYLITE_DML_WRITE_TABLE_ALLOW_ZERO_TEMPORAL;
    }
    status = mylite_dml_load_write_table(
        stmt->database,
        schema_name,
        stmt->insert_values.table_name,
        write_table_flags,
        &table
    );
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_insert_set_assignments(
            stmt->database,
            &stmt->insert_values,
            &stmt->insert_set,
            &table,
            schema_name,
            &column_indexes,
            &column_index_count
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_insert_row_alias(
            stmt->database,
            &stmt->insert_values,
            column_index_count
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_insert_update_assignments(
            stmt->database,
            &stmt->insert_values,
            &stmt->insert_update,
            &table,
            schema_name,
            column_indexes,
            column_index_count,
            &update_column_indexes
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_execute_insert_set_transaction(
            stmt->database,
            stmt->database->selected_schema,
            schema_name,
            &stmt->insert_values,
            &stmt->insert_set,
            &stmt->insert_update,
            &table,
            column_indexes,
            column_index_count,
            update_column_indexes,
            expression_callbacks,
            &result
        );
    }

    free(update_column_indexes);
    free(column_indexes);
    mylite_dml_insert_table_deinit(&table);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
    } else {
        stmt->affected_rows = result.affected_rows;
        if (result.generated_insert_id) {
            mylite_connection_set_last_insert_id(stmt->database, result.last_insert_id);
        }
    }
    return status;
}

int mylite_dml_append_replace_delayed_warning(mylite_stmt *stmt) {
    if (!stmt->insert_values.replace_delayed) {
        return MYLITE_OK;
    }
    return mylite_diagnostics_append_warning(
        stmt->database,
        MYLITE_MYSQL_ER_WARN_LEGACY_SYNTAX_CONVERTED,
        "REPLACE DELAYED is no longer supported. The statement was "
        "converted to REPLACE."
    );
}

int mylite_dml_execute_replace_values_statement(
    mylite_stmt *stmt,
    const struct mylite_dml_expression_callbacks *expression_callbacks
) {
    const char *schema_name = NULL;
    struct mylite_insert_table table = {0};
    struct mylite_insert_transaction_result result = {0};
    size_t *column_indexes = NULL;
    int status = mylite_dml_validate_insert_target(
        stmt->database,
        stmt->database->selected_schema,
        &stmt->insert_values,
        &schema_name
    );

    stmt->affected_rows = 0;
    if (status != MYLITE_OK) {
        return status;
    }

    status = mylite_dml_load_write_table(
        stmt->database,
        schema_name,
        stmt->insert_values.table_name,
        0U,
        &table
    );
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_insert_column_list(
            stmt->database,
            &stmt->insert_values,
            &table,
            &column_indexes
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_execute_replace_values_transaction(
            stmt->database,
            schema_name,
            &stmt->insert_values,
            &table,
            column_indexes,
            expression_callbacks,
            &result
        );
    }

    free(column_indexes);
    mylite_dml_insert_table_deinit(&table);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
    } else {
        stmt->affected_rows = result.affected_rows;
        if (result.generated_insert_id) {
            mylite_connection_set_last_insert_id(stmt->database, result.last_insert_id);
        }
    }
    return status;
}

int mylite_dml_execute_replace_set_statement(
    mylite_stmt *stmt,
    const struct mylite_dml_expression_callbacks *expression_callbacks
) {
    const char *schema_name = NULL;
    struct mylite_insert_table table = {0};
    struct mylite_insert_transaction_result result = {0};
    size_t *column_indexes = NULL;
    size_t column_index_count = 0U;
    int status = mylite_dml_validate_insert_target(
        stmt->database,
        stmt->database->selected_schema,
        &stmt->insert_values,
        &schema_name
    );

    stmt->affected_rows = 0;
    if (status != MYLITE_OK) {
        return status;
    }

    status = mylite_dml_load_write_table(
        stmt->database,
        schema_name,
        stmt->insert_values.table_name,
        0U,
        &table
    );
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_insert_set_assignments(
            stmt->database,
            &stmt->insert_values,
            &stmt->insert_set,
            &table,
            schema_name,
            &column_indexes,
            &column_index_count
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_execute_replace_set_transaction(
            stmt->database,
            schema_name,
            &stmt->insert_values,
            &stmt->insert_set,
            &table,
            column_indexes,
            column_index_count,
            expression_callbacks,
            &result
        );
    }

    free(column_indexes);
    mylite_dml_insert_table_deinit(&table);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
    } else {
        stmt->affected_rows = result.affected_rows;
        if (result.generated_insert_id) {
            mylite_connection_set_last_insert_id(stmt->database, result.last_insert_id);
        }
    }
    return status;
}

int mylite_dml_execute_update_statement(
    mylite_stmt *stmt,
    const struct mylite_dml_expression_callbacks *expression_callbacks
) {
    struct mylite_select_table table = {0};
    struct mylite_insert_table write_table = {0};
    struct mylite_update_order_plan order_plan = {0};
    struct mylite_update_bound_assignment *assignments = NULL;
    struct mylite_update_rowset rowset = {0};
    size_t assignment_count = stmt->update.assignment_count;
    int64_t affected_rows = 0;
    int status = MYLITE_OK;

    if (stmt->update.form == MYLITE_UPDATE_JOINED_TABLES) {
        return mylite_dml_execute_joined_update_statement(stmt, expression_callbacks);
    }

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
        status = mylite_dml_load_write_table(
            stmt->database,
            table.schema_name,
            table.table_name,
            0U,
            &write_table
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_bind_update_subset(stmt->database, &stmt->update, &table, &assignments);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_bind_update_order_by_clause(
            stmt->database,
            &stmt->update,
            &table,
            &order_plan
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_materialize_update_rows(
            stmt->database,
            &stmt->update,
            &table,
            &order_plan,
            expression_callbacks,
            &rowset
        );
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
            stmt->database,
            &table,
            &write_table,
            assignments,
            assignment_count,
            expression_callbacks,
            &rowset,
            &affected_rows
        );
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
    mylite_stmt *stmt,
    const struct mylite_dml_expression_callbacks *expression_callbacks
) {
    if (stmt->delete_plan.form == MYLITE_SQL_AST_DELETE_SINGLE_TABLE) {
        return execute_single_delete_statement(stmt, expression_callbacks);
    }
    return execute_multi_delete_statement(stmt);
}

static int execute_single_delete_statement(
    mylite_stmt *stmt,
    const struct mylite_dml_expression_callbacks *expression_callbacks
) {
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
        status = mylite_dml_bind_delete_order_by_clause(
            stmt->database,
            &stmt->delete_plan,
            &table,
            &order_plan
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_materialize_delete_rows(
            stmt->database,
            &stmt->delete_plan,
            &table,
            &order_plan,
            expression_callbacks,
            &rowset
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_sort_update_rowset(&rowset, &order_plan);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        }
    }
    if (status == MYLITE_OK) {
        mylite_dml_apply_update_limit(stmt->delete_plan.limit_clause, &rowset);
        status = mylite_dml_execute_delete_rows_transaction(
            stmt->database,
            &table,
            &rowset,
            &affected_rows
        );
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

static int execute_multi_delete_statement(mylite_stmt *stmt) {
    mylite_stmt joined_stmt = {
        .database = stmt->database,
        .kind = MYLITE_STMT_TABLE_SELECT,
    };
    struct mylite_update_rowset *rowsets = NULL;
    size_t *target_table_indexes = NULL;
    int64_t affected_rows = 0;
    int status = MYLITE_OK;

    stmt->affected_rows = 0;

    status = bind_multi_delete_select_plan(stmt, &joined_stmt);
    if (status == MYLITE_OK) {
        status = resolve_multi_delete_targets(
            stmt->database,
            &stmt->delete_plan,
            &joined_stmt.select_plan,
            &target_table_indexes
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_select_materialize_joined_table_result(
            &joined_stmt,
            mylite_select_context_table_select_eval_callbacks()
        );
    }
    if (status == MYLITE_OK) {
        status = materialize_multi_delete_rowsets(
            stmt->database,
            &joined_stmt.select_result,
            target_table_indexes,
            stmt->delete_plan.target_count,
            &rowsets
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_execute_multi_delete_rows_transaction(
            stmt->database,
            &joined_stmt.select_plan,
            target_table_indexes,
            rowsets,
            stmt->delete_plan.target_count,
            &affected_rows
        );
        if (status == MYLITE_OK) {
            stmt->affected_rows = affected_rows;
        }
    }

    if (rowsets != NULL) {
        for (size_t index = 0U; index < stmt->delete_plan.target_count; ++index) {
            mylite_dml_update_rowset_deinit(&rowsets[index]);
        }
    }
    free(rowsets);
    free(target_table_indexes);
    mylite_select_result_deinit(&joined_stmt.select_result);
    mylite_select_plan_deinit(&joined_stmt.select_plan);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
    }
    return status;
}

static int bind_multi_delete_select_plan(mylite_stmt *stmt, mylite_stmt *joined_stmt) {
    const struct mylite_select_predicate_bind_callbacks *predicate_callbacks =
        mylite_select_context_predicate_bind_callbacks();
    const struct mylite_sql_ast_node *where_clause = stmt->delete_plan.where_clause;
    int status = mylite_select_bind_from_clause(
        stmt->database,
        stmt->delete_plan.from_clause,
        &joined_stmt->select_plan
    );

    if (status == MYLITE_OK) {
        status = mylite_select_bind_join_predicates(
            stmt->database,
            &joined_stmt->select_plan,
            predicate_callbacks
        );
    }
    if (status == MYLITE_OK && where_clause != NULL) {
        status = mylite_select_bind_where_clause(
            stmt->database,
            where_clause,
            &joined_stmt->select_plan,
            predicate_callbacks
        );
        joined_stmt->select_predicate = mylite_ast_child_at(where_clause, 0U);
    }
    return status;
}

static int resolve_multi_delete_targets(
    mylite_db *database,
    const struct mylite_delete_plan *delete_plan,
    const struct mylite_select_plan *plan,
    size_t **out_target_table_indexes
) {
    size_t *target_table_indexes = NULL;

    *out_target_table_indexes = NULL;
    if (delete_plan->target_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    target_table_indexes = calloc(delete_plan->target_count, sizeof(*target_table_indexes));
    if (target_table_indexes == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    for (size_t index = 0U; index < delete_plan->target_count; ++index) {
        size_t table_index = 0U;
        int status =
            resolve_multi_delete_target(database, &delete_plan->targets[index], plan, &table_index);

        if (status != MYLITE_OK) {
            free(target_table_indexes);
            return status;
        }
        if (multi_delete_target_is_duplicate(
                target_table_indexes,
                (struct multi_delete_target_duplicate_search){
                    .target_index = index,
                    .table_index = table_index,
                }
            )) {
            status =
                set_multi_delete_nonunique_target_error(database, &delete_plan->targets[index]);
            free(target_table_indexes);
            return status;
        }
        target_table_indexes[index] = table_index;
    }

    *out_target_table_indexes = target_table_indexes;
    return MYLITE_OK;
}

static int resolve_multi_delete_target(
    mylite_db *database,
    const struct mylite_delete_target *target,
    const struct mylite_select_plan *plan,
    size_t *out_table_index
) {
    size_t table_count = mylite_select_plan_table_count(plan);

    for (size_t table_index = 0U; table_index < table_count; ++table_index) {
        const struct mylite_select_table *table = mylite_select_plan_table_const(plan, table_index);

        if (multi_delete_target_matches_table(target, table)) {
            *out_table_index = table_index;
            return MYLITE_OK;
        }
    }
    return set_multi_delete_unknown_target_error(database, target);
}

static bool multi_delete_target_matches_table(
    const struct mylite_delete_target *target,
    const struct mylite_select_table *table
) {
    if (target == NULL || table == NULL || target->table_name == NULL) {
        return false;
    }
    if (target->schema_name != NULL) {
        if (table->alias != NULL || table->schema_name == NULL) {
            return false;
        }
        if (strcmp(target->schema_name, table->schema_name) != 0) {
            return false;
        }
        if (strcmp(target->table_name, table->table_name) == 0) {
            return true;
        }
        return false;
    }

    const char *visible_name = table->alias == NULL ? table->table_name : table->alias;

    if (visible_name == NULL) {
        return false;
    }
    if (strcmp(target->table_name, visible_name) == 0) {
        return true;
    }
    return false;
}

static bool multi_delete_target_is_duplicate(
    const size_t *target_table_indexes,
    struct multi_delete_target_duplicate_search search
) {
    for (size_t index = 0U; index < search.target_index; ++index) {
        if (target_table_indexes[index] == search.table_index) {
            return true;
        }
    }
    return false;
}

static int materialize_multi_delete_rowsets(
    mylite_db *database,
    const struct mylite_table_select_result *result,
    const size_t *target_table_indexes,
    size_t target_count,
    struct mylite_update_rowset **out_rowsets
) {
    struct mylite_update_rowset *rowsets = NULL;
    int status = MYLITE_OK;

    *out_rowsets = NULL;
    if (target_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    rowsets = calloc(target_count, sizeof(*rowsets));
    if (rowsets == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    for (size_t row_index = 0U; status == MYLITE_OK && row_index < result->row_count; ++row_index) {
        status = add_multi_delete_result_row(
            database,
            &result->rows[row_index],
            target_table_indexes,
            target_count,
            rowsets
        );
    }
    if (status != MYLITE_OK) {
        for (size_t index = 0U; index < target_count; ++index) {
            mylite_dml_update_rowset_deinit(&rowsets[index]);
        }
        free(rowsets);
        return status;
    }

    *out_rowsets = rowsets;
    return MYLITE_OK;
}

static int add_multi_delete_result_row(
    mylite_db *database,
    const struct mylite_table_select_row *row,
    const size_t *target_table_indexes,
    size_t target_count,
    struct mylite_update_rowset *rowsets
) {
    for (size_t target_index = 0U; target_index < target_count; ++target_index) {
        size_t table_index = target_table_indexes[target_index];

        if (table_index >= row->source_row_index_count ||
            row->source_row_indexes[table_index] == SIZE_MAX) {
            continue;
        }
        if (table_index >= row->source_rowid_count) {
            return MYLITE_UNSUPPORTED;
        }
        int status = add_unique_multi_delete_rowid(
            database,
            &rowsets[target_index],
            row->source_rowids[table_index]
        );

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int add_unique_multi_delete_rowid(
    mylite_db *database,
    struct mylite_update_rowset *rowset,
    int64_t rowid
) {
    struct mylite_update_row row = {
        .rowid = rowid,
    };

    if (multi_delete_rowset_contains_rowid(rowset, rowid)) {
        return MYLITE_OK;
    }
    return mylite_dml_append_update_row(database, rowset, &row);
}

static bool multi_delete_rowset_contains_rowid(
    const struct mylite_update_rowset *rowset,
    int64_t rowid
) {
    for (size_t index = 0U; index < rowset->row_count; ++index) {
        if (rowset->rows[index].rowid == rowid) {
            return true;
        }
    }
    return false;
}

static int set_multi_delete_unknown_target_error(
    mylite_db *database,
    const struct mylite_delete_target *target
) {
    const char *table_name = target == NULL || target->table_name == NULL ? "" : target->table_name;
    int status = mylite_diagnostics_set_error_message_parts(
        database,
        "Unknown table '",
        table_name,
        "' in MULTI DELETE"
    );

    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_UNKNOWN_TABLE,
            mylite_error_message(database)
        );
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_multi_delete_nonunique_target_error(
    mylite_db *database,
    const struct mylite_delete_target *target
) {
    const char *table_name = target == NULL || target->table_name == NULL ? "" : target->table_name;
    int status = mylite_diagnostics_set_error_message_parts(
        database,
        "Not unique table/alias: '",
        table_name,
        "'"
    );

    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_NONUNIQ_TABLE,
            mylite_error_message(database)
        );
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}
