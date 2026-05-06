#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_context.h"
#include "mylite_select_eval.h"
#include "mylite_select_eval_expression.h"
#include "mylite_select_from.h"
#include "mylite_select_join_materialize.h"
#include "mylite_select_predicate_bind.h"
#include "mylite_select_predicate_expression_bind.h"
#include "mylite_select_rowset.h"
#include "mylite_span.h"
#include "mylite_transactions.h"
#include "sql/mylite_expression.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct mylite_joined_update_assignment {
    size_t table_index;
    size_t column_index;
    const struct mylite_sql_ast_node *value;
};

struct mylite_joined_update_target {
    size_t table_index;
    const struct mylite_select_table *table;
    struct mylite_insert_table write_table;
    struct mylite_joined_update_assignment *assignments;
    size_t assignment_count;
    sqlite3_stmt *update;
    sqlite3_int64 *seen_rowids;
    size_t seen_rowid_count;
    uint64_t next_auto_increment;
    int64_t matched_rows;
    int64_t affected_rows;
};

static int bind_joined_update_select_plan(mylite_stmt *stmt, mylite_stmt *joined_stmt);

static int bind_joined_update_assignments(
    mylite_db *database,
    const struct mylite_update_plan *update_plan,
    const struct mylite_select_plan *select_plan,
    struct mylite_joined_update_target **out_targets,
    size_t *out_target_count
);

static int bind_joined_update_assignment(
    mylite_db *database,
    const struct mylite_update_assignment *assignment,
    const struct mylite_select_plan *select_plan,
    struct mylite_joined_update_target **targets,
    size_t *target_count
);

static int resolve_joined_update_assignment_target(
    mylite_db *database,
    const struct mylite_update_column_reference *reference,
    const struct mylite_select_plan *select_plan,
    size_t *out_table_index,
    size_t *out_column_index
);

static int add_joined_update_assignment(
    mylite_db *database,
    struct mylite_joined_update_target **targets,
    size_t *target_count,
    const struct mylite_select_plan *select_plan,
    struct mylite_joined_update_assignment assignment
);

static int find_or_add_joined_update_target(
    mylite_db *database,
    struct mylite_joined_update_target **targets,
    size_t *target_count,
    const struct mylite_select_plan *select_plan,
    size_t table_index,
    size_t *out_target_index
);

static int bind_joined_update_assignment_value(
    mylite_db *database,
    const struct mylite_sql_ast_node *value,
    const struct mylite_select_plan *select_plan
);

static bool joined_update_reference_matches_table(
    const struct mylite_update_column_reference *reference,
    const struct mylite_select_table *table
);

static size_t joined_update_column_index(
    const struct mylite_select_table *table,
    const char *column_name
);

static char *joined_update_column_reference_name(
    const struct mylite_update_column_reference *reference
);

static int set_joined_update_ambiguous_column_error(mylite_db *database, const char *column_name);

static int set_joined_update_unknown_column_error(
    mylite_db *database,
    const struct mylite_update_column_reference *reference
);

static int initialize_joined_update_targets(
    mylite_db *database,
    struct mylite_joined_update_target *targets,
    size_t target_count
);

static int initialize_joined_update_target(
    mylite_db *database,
    struct mylite_joined_update_target *target
);

static int execute_joined_update_transaction(
    mylite_stmt *stmt,
    mylite_stmt *joined_stmt,
    struct mylite_joined_update_target *targets,
    size_t target_count
);

static int execute_joined_update_result_row(
    mylite_db *database,
    mylite_stmt *joined_stmt,
    const struct mylite_table_select_row *joined_row,
    struct mylite_joined_update_target *targets,
    size_t target_count
);

static int execute_joined_update_target_row(
    mylite_db *database,
    mylite_stmt *joined_stmt,
    const struct mylite_table_select_row *joined_row,
    struct mylite_joined_update_target *target
);

static bool joined_update_target_row_is_present(
    const struct mylite_table_select_row *joined_row,
    size_t table_index,
    sqlite3_int64 *out_rowid
);

static bool joined_update_target_seen(
    const struct mylite_joined_update_target *target,
    sqlite3_int64 rowid
);

static int remember_joined_update_target_row(
    mylite_db *database,
    struct mylite_joined_update_target *target,
    sqlite3_int64 rowid
);

static int copy_joined_update_target_row(
    mylite_db *database,
    const struct mylite_table_select_row *joined_row,
    const struct mylite_joined_update_target *target,
    sqlite3_int64 rowid,
    struct mylite_update_row *out_row
);

static int apply_joined_update_assignments(
    mylite_db *database,
    mylite_stmt *joined_stmt,
    const struct mylite_table_select_row *joined_row,
    const struct mylite_joined_update_target *target,
    struct mylite_update_row *candidate
);

static int evaluate_joined_update_assignment(
    mylite_db *database,
    mylite_stmt *joined_stmt,
    const struct mylite_table_select_row *joined_row,
    const struct mylite_joined_update_target *target,
    const struct mylite_joined_update_assignment *assignment,
    struct mylite_expression_value *out_value
);

static int write_joined_update_candidate(
    mylite_db *database,
    struct mylite_joined_update_target *target,
    const struct mylite_update_row *candidate
);

static int finish_joined_update_auto_increment(
    mylite_db *database,
    struct mylite_joined_update_target *targets,
    size_t target_count
);

static void joined_update_targets_deinit(
    struct mylite_joined_update_target *targets,
    size_t target_count
);

int mylite_dml_execute_joined_update_statement(
    mylite_stmt *stmt,
    const struct mylite_dml_expression_callbacks *expression_callbacks
) {
    mylite_stmt joined_stmt = {
        .database = stmt->database,
        .kind = MYLITE_STMT_TABLE_SELECT,
    };
    struct mylite_joined_update_target *targets = NULL;
    size_t target_count = 0U;
    int status = MYLITE_OK;

    (void)expression_callbacks;
    stmt->affected_rows = 0;
    stmt->matched_rows = 0U;

    status = bind_joined_update_select_plan(stmt, &joined_stmt);
    if (status == MYLITE_OK) {
        status = bind_joined_update_assignments(
            stmt->database,
            &stmt->update,
            &joined_stmt.select_plan,
            &targets,
            &target_count
        );
    }
    if (status == MYLITE_OK) {
        status = initialize_joined_update_targets(stmt->database, targets, target_count);
    }
    if (status == MYLITE_OK) {
        status = mylite_select_materialize_joined_table_result(
            &joined_stmt,
            mylite_select_context_table_select_eval_callbacks()
        );
    }
    if (status == MYLITE_OK) {
        status = execute_joined_update_transaction(stmt, &joined_stmt, targets, target_count);
    }
    if (status == MYLITE_OK) {
        for (size_t index = 0U; index < target_count; ++index) {
            stmt->matched_rows += (uint64_t)targets[index].matched_rows;
            stmt->affected_rows += targets[index].affected_rows;
        }
    }

    joined_update_targets_deinit(targets, target_count);
    mylite_select_result_deinit(&joined_stmt.select_result);
    mylite_select_plan_deinit(&joined_stmt.select_plan);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
    }
    return status;
}

static int bind_joined_update_select_plan(mylite_stmt *stmt, mylite_stmt *joined_stmt) {
    const struct mylite_select_predicate_bind_callbacks *predicate_callbacks =
        mylite_select_context_predicate_bind_callbacks();
    int status = mylite_select_bind_from_clause(
        stmt->database,
        stmt->update.from_clause,
        &joined_stmt->select_plan
    );

    if (status == MYLITE_OK) {
        status = mylite_select_bind_join_predicates(
            stmt->database,
            &joined_stmt->select_plan,
            predicate_callbacks
        );
    }
    if (status == MYLITE_OK && stmt->update.where_clause != NULL) {
        status = mylite_select_bind_where_clause(
            stmt->database,
            stmt->update.where_clause,
            &joined_stmt->select_plan,
            predicate_callbacks
        );
        joined_stmt->select_predicate = mylite_ast_child_at(stmt->update.where_clause, 0U);
    }
    return status;
}

static int bind_joined_update_assignments(
    mylite_db *database,
    const struct mylite_update_plan *update_plan,
    const struct mylite_select_plan *select_plan,
    struct mylite_joined_update_target **out_targets,
    size_t *out_target_count
) {
    struct mylite_joined_update_target *targets = NULL;
    size_t target_count = 0U;
    int status = MYLITE_OK;

    *out_targets = NULL;
    *out_target_count = 0U;
    if (update_plan->assignment_count == 0U) {
        return mylite_dml_set_update_unsupported_assignment_error(database);
    }

    for (size_t index = 0U; status == MYLITE_OK && index < update_plan->assignment_count; ++index) {
        status = bind_joined_update_assignment(
            database,
            &update_plan->assignments[index],
            select_plan,
            &targets,
            &target_count
        );
    }
    if (status != MYLITE_OK) {
        joined_update_targets_deinit(targets, target_count);
        return status;
    }

    *out_targets = targets;
    *out_target_count = target_count;
    return MYLITE_OK;
}

static int bind_joined_update_assignment(
    mylite_db *database,
    const struct mylite_update_assignment *assignment,
    const struct mylite_select_plan *select_plan,
    struct mylite_joined_update_target **targets,
    size_t *target_count
) {
    struct mylite_joined_update_assignment bound = {
        .value = assignment->value,
    };
    int status = resolve_joined_update_assignment_target(
        database,
        &assignment->target,
        select_plan,
        &bound.table_index,
        &bound.column_index
    );

    if (status == MYLITE_OK) {
        status = bind_joined_update_assignment_value(database, assignment->value, select_plan);
    }
    if (status == MYLITE_OK) {
        status = add_joined_update_assignment(database, targets, target_count, select_plan, bound);
    }
    return status;
}

static int resolve_joined_update_assignment_target(
    mylite_db *database,
    const struct mylite_update_column_reference *reference,
    const struct mylite_select_plan *select_plan,
    size_t *out_table_index,
    size_t *out_column_index
) {
    bool matched = false;

    for (size_t table_index = 0U; table_index < mylite_select_plan_table_count(select_plan);
         ++table_index) {
        const struct mylite_select_table *table =
            mylite_select_plan_table_const(select_plan, table_index);
        size_t column_index = 0U;

        if (!joined_update_reference_matches_table(reference, table)) {
            continue;
        }
        column_index = joined_update_column_index(table, reference->column_name);
        if (column_index == table->column_count) {
            continue;
        }
        if (matched) {
            return set_joined_update_ambiguous_column_error(database, reference->column_name);
        }
        *out_table_index = table_index;
        *out_column_index = column_index;
        matched = true;
    }

    return matched ? MYLITE_OK : set_joined_update_unknown_column_error(database, reference);
}

static int add_joined_update_assignment(
    mylite_db *database,
    struct mylite_joined_update_target **targets,
    size_t *target_count,
    const struct mylite_select_plan *select_plan,
    struct mylite_joined_update_assignment assignment
) {
    size_t target_index = 0U;
    struct mylite_joined_update_target *target = NULL;
    struct mylite_joined_update_assignment *assignments = NULL;
    int status = find_or_add_joined_update_target(
        database,
        targets,
        target_count,
        select_plan,
        assignment.table_index,
        &target_index
    );

    if (status != MYLITE_OK) {
        return status;
    }
    target = &(*targets)[target_index];
    assignments = realloc(
        target->assignments,
        (target->assignment_count + 1U) * sizeof(*target->assignments)
    );
    if (assignments == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    target->assignments = assignments;
    target->assignments[target->assignment_count++] = assignment;
    return MYLITE_OK;
}

static int find_or_add_joined_update_target(
    mylite_db *database,
    struct mylite_joined_update_target **targets,
    size_t *target_count,
    const struct mylite_select_plan *select_plan,
    size_t table_index,
    size_t *out_target_index
) {
    struct mylite_joined_update_target *new_targets = NULL;

    for (size_t index = 0U; index < *target_count; ++index) {
        if ((*targets)[index].table_index == table_index) {
            *out_target_index = index;
            return MYLITE_OK;
        }
    }

    new_targets = realloc(*targets, (*target_count + 1U) * sizeof(**targets));
    if (new_targets == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *targets = new_targets;
    (*targets)[*target_count] = (struct mylite_joined_update_target){
        .table_index = table_index,
        .table = mylite_select_plan_table_const(select_plan, table_index),
    };
    *out_target_index = (*target_count)++;
    return MYLITE_OK;
}

static int bind_joined_update_assignment_value(
    mylite_db *database,
    const struct mylite_sql_ast_node *value,
    const struct mylite_select_plan *select_plan
) {
    if (value != NULL && value->kind == MYLITE_SQL_AST_DEFAULT) {
        return MYLITE_OK;
    }
    return mylite_select_bind_predicate_expression_in_clause(
        database,
        value,
        select_plan,
        "field list",
        0U,
        mylite_select_plan_table_count(select_plan),
        mylite_select_context_predicate_bind_callbacks()
    );
}

static bool joined_update_reference_matches_table(
    const struct mylite_update_column_reference *reference,
    const struct mylite_select_table *table
) {
    if (reference == NULL || table == NULL) {
        return false;
    }
    if (reference->schema_name != NULL) {
        return table->alias == NULL && table->schema_name != NULL &&
               mylite_ascii_case_equal(reference->schema_name, table->schema_name) &&
               reference->table_name != NULL &&
               mylite_ascii_case_equal(reference->table_name, table->table_name);
    }
    if (reference->table_name != NULL) {
        const char *visible_name = table->alias == NULL ? table->table_name : table->alias;

        return visible_name != NULL && mylite_ascii_case_equal(reference->table_name, visible_name);
    }
    return true;
}

static size_t joined_update_column_index(
    const struct mylite_select_table *table,
    const char *column_name
) {
    if (table == NULL || column_name == NULL) {
        return 0U;
    }
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (mylite_ascii_case_equal(table->columns[index].name, column_name)) {
            return index;
        }
    }
    return table->column_count;
}

static char *joined_update_column_reference_name(
    const struct mylite_update_column_reference *reference
) {
    sqlite3_str *text = sqlite3_str_new(NULL);

    if (text == NULL) {
        return NULL;
    }
    if (reference->schema_name != NULL) {
        sqlite3_str_appendf(text, "%s.", reference->schema_name);
    }
    if (reference->table_name != NULL) {
        sqlite3_str_appendf(text, "%s.", reference->table_name);
    }
    sqlite3_str_append(
        text,
        reference->column_name == NULL ? "" : reference->column_name,
        reference->column_name == NULL ? 0 : (int)strlen(reference->column_name)
    );
    return sqlite3_str_finish(text);
}

static int set_joined_update_ambiguous_column_error(mylite_db *database, const char *column_name) {
    return mylite_select_set_ambiguous_column_error(database, column_name, "field list");
}

static int set_joined_update_unknown_column_error(
    mylite_db *database,
    const struct mylite_update_column_reference *reference
) {
    char *name = joined_update_column_reference_name(reference);
    int status = MYLITE_OK;

    if (name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_dml_set_update_unknown_column_error(database, name, "field list");
    sqlite3_free(name);
    return status;
}

static int initialize_joined_update_targets(
    mylite_db *database,
    struct mylite_joined_update_target *targets,
    size_t target_count
) {
    for (size_t index = 0U; index < target_count; ++index) {
        int status = initialize_joined_update_target(database, &targets[index]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return target_count == 0U ? mylite_dml_set_update_unsupported_assignment_error(database)
                              : MYLITE_OK;
}

static int initialize_joined_update_target(
    mylite_db *database,
    struct mylite_joined_update_target *target
) {
    char *update_sql = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (target->table == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    status = mylite_dml_load_write_table(
        database,
        target->table->schema_name,
        target->table->table_name,
        &target->write_table
    );
    if (status != MYLITE_OK) {
        return status;
    }
    target->next_auto_increment = target->write_table.next_auto_increment;

    update_sql = mylite_dml_build_update_physical_sql(database, target->table);
    if (update_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(
        database->sqlite,
        update_sql,
        -1,
        SQLITE_PREPARE_PERSISTENT,
        &target->update,
        NULL
    );
    sqlite3_free(update_sql);
    return rc == SQLITE_OK ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int execute_joined_update_transaction(
    mylite_stmt *stmt,
    mylite_stmt *joined_stmt,
    struct mylite_joined_update_target *targets,
    size_t target_count
) {
    struct mylite_statement_atomicity atomicity = {0};
    int status = mylite_transaction_begin_statement_atomicity(stmt->database, &atomicity);

    if (status != MYLITE_OK) {
        return status;
    }

    for (size_t row_index = 0U;
         status == MYLITE_OK && row_index < joined_stmt->select_result.row_count;
         ++row_index) {
        status = execute_joined_update_result_row(
            stmt->database,
            joined_stmt,
            &joined_stmt->select_result.rows[row_index],
            targets,
            target_count
        );
    }
    if (status == MYLITE_OK) {
        status = finish_joined_update_auto_increment(stmt->database, targets, target_count);
    }
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_statement_atomicity(stmt->database, &atomicity);
        if (status == MYLITE_OK) {
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_statement_atomicity(stmt->database, &atomicity);
    return status;
}

static int execute_joined_update_result_row(
    mylite_db *database,
    mylite_stmt *joined_stmt,
    const struct mylite_table_select_row *joined_row,
    struct mylite_joined_update_target *targets,
    size_t target_count
) {
    for (size_t index = 0U; index < target_count; ++index) {
        int status =
            execute_joined_update_target_row(database, joined_stmt, joined_row, &targets[index]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int execute_joined_update_target_row(
    mylite_db *database,
    mylite_stmt *joined_stmt,
    const struct mylite_table_select_row *joined_row,
    struct mylite_joined_update_target *target
) {
    struct mylite_update_row stored = {0};
    struct mylite_update_row candidate = {0};
    sqlite3_int64 rowid = 0;
    bool row_changed = false;
    int status = MYLITE_OK;

    if (!joined_update_target_row_is_present(joined_row, target->table_index, &rowid) ||
        joined_update_target_seen(target, rowid)) {
        return MYLITE_OK;
    }
    status = remember_joined_update_target_row(database, target, rowid);
    if (status != MYLITE_OK) {
        return status;
    }
    ++target->matched_rows;

    status = copy_joined_update_target_row(database, joined_row, target, rowid, &stored);
    if (status == MYLITE_OK) {
        status = mylite_dml_copy_update_candidate_values(database, &stored, &candidate);
    }
    if (status == MYLITE_OK) {
        status =
            apply_joined_update_assignments(database, joined_stmt, joined_row, target, &candidate);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_update_unique_indexes(
            database,
            target->table,
            &target->write_table,
            &candidate
        );
    }
    if (status == MYLITE_OK) {
        row_changed = mylite_dml_update_row_changed(&stored, &candidate);
    }
    if (status == MYLITE_OK && row_changed) {
        status = mylite_dml_validate_update_child_foreign_keys(
            database,
            target->table,
            &target->write_table,
            &stored,
            &candidate
        );
    }
    if (status == MYLITE_OK && row_changed) {
        status = mylite_dml_validate_parent_update_foreign_keys(
            database,
            target->table,
            &stored,
            &candidate
        );
    }
    if (status == MYLITE_OK && row_changed) {
        status = write_joined_update_candidate(database, target, &candidate);
    }

    mylite_dml_update_row_deinit(&candidate);
    mylite_dml_update_row_deinit(&stored);
    return status;
}

static bool joined_update_target_row_is_present(
    const struct mylite_table_select_row *joined_row,
    size_t table_index,
    sqlite3_int64 *out_rowid
) {
    if (joined_row == NULL || table_index >= joined_row->source_row_index_count ||
        table_index >= joined_row->source_rowid_count ||
        joined_row->source_row_indexes[table_index] == SIZE_MAX) {
        return false;
    }
    *out_rowid = joined_row->source_rowids[table_index];
    return true;
}

static bool joined_update_target_seen(
    const struct mylite_joined_update_target *target,
    sqlite3_int64 rowid
) {
    for (size_t index = 0U; index < target->seen_rowid_count; ++index) {
        if (target->seen_rowids[index] == rowid) {
            return true;
        }
    }
    return false;
}

static int remember_joined_update_target_row(
    mylite_db *database,
    struct mylite_joined_update_target *target,
    sqlite3_int64 rowid
) {
    sqlite3_int64 *rowids =
        realloc(target->seen_rowids, (target->seen_rowid_count + 1U) * sizeof(*rowids));

    if (rowids == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    target->seen_rowids = rowids;
    target->seen_rowids[target->seen_rowid_count++] = rowid;
    return MYLITE_OK;
}

static int copy_joined_update_target_row(
    mylite_db *database,
    const struct mylite_table_select_row *joined_row,
    const struct mylite_joined_update_target *target,
    sqlite3_int64 rowid,
    struct mylite_update_row *out_row
) {
    size_t first_column = target->table->first_column_index;

    *out_row = (struct mylite_update_row){
        .rowid = rowid,
        .value_count = target->table->column_count,
    };
    if (out_row->value_count == 0U) {
        return MYLITE_OK;
    }
    out_row->values = calloc(out_row->value_count, sizeof(*out_row->values));
    if (out_row->values == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < out_row->value_count; ++index) {
        if (first_column + index >= joined_row->value_count ||
            mylite_expression_value_copy(
                &joined_row->values[first_column + index],
                &out_row->values[index]
            ) != 0) {
            mylite_dml_update_row_deinit(out_row);
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

static int apply_joined_update_assignments(
    mylite_db *database,
    mylite_stmt *joined_stmt,
    const struct mylite_table_select_row *joined_row,
    const struct mylite_joined_update_target *target,
    struct mylite_update_row *candidate
) {
    for (size_t index = 0U; index < target->assignment_count; ++index) {
        const struct mylite_joined_update_assignment *assignment = &target->assignments[index];
        struct mylite_expression_value value = {0};
        int status = MYLITE_OK;

        if (assignment->column_index >= target->write_table.column_count ||
            assignment->column_index >= candidate->value_count) {
            return mylite_dml_set_update_unsupported_assignment_error(database);
        }

        status = evaluate_joined_update_assignment(
            database,
            joined_stmt,
            joined_row,
            target,
            assignment,
            &value
        );
        if (status != MYLITE_OK) {
            mylite_expression_value_deinit(&value);
            return status;
        }

        value.suppress_text_numeric_warnings = false;
        mylite_expression_value_deinit(&candidate->values[assignment->column_index]);
        candidate->values[assignment->column_index] = value;
    }
    return MYLITE_OK;
}

static int evaluate_joined_update_assignment(
    mylite_db *database,
    mylite_stmt *joined_stmt,
    const struct mylite_table_select_row *joined_row,
    const struct mylite_joined_update_target *target,
    const struct mylite_joined_update_assignment *assignment,
    struct mylite_expression_value *out_value
) {
    const struct mylite_insert_table_column *column =
        &target->write_table.columns[assignment->column_index];
    int status = MYLITE_OK;

    if (assignment->value != NULL && assignment->value->kind == MYLITE_SQL_AST_DEFAULT) {
        status = mylite_dml_resolve_update_default_value(database, column, out_value);
    } else {
        const struct mylite_select_eval_callbacks *callbacks =
            mylite_select_context_table_select_eval_callbacks();
        struct mylite_table_select_eval_context user_context = {0};
        struct mylite_expression_eval_context expression_context = {0};
        size_t warning_start = database->warnings.count;
        int eval_status = 0;

        mylite_select_eval_context_init(
            &user_context,
            joined_stmt,
            joined_row,
            callbacks,
            false,
            false
        );
        mylite_select_eval_expression_context_init(&expression_context, &user_context);
        eval_status = mylite_expression_eval_with_context(
            assignment->value,
            &expression_context,
            &database->warnings,
            out_value
        );
        status = mylite_select_eval_map_expression_status(joined_stmt, eval_status, callbacks);
        if (status == MYLITE_OK) {
            status = mylite_dml_promote_expression_warnings(database, warning_start);
        }
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_update_assignment_value(database, column, out_value);
    }
    return status;
}

static int write_joined_update_candidate(
    mylite_db *database,
    struct mylite_joined_update_target *target,
    const struct mylite_update_row *candidate
) {
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    sqlite3_reset(target->update);
    sqlite3_clear_bindings(target->update);
    status = mylite_dml_bind_update_row_values(database, target->update, candidate);
    if (status != MYLITE_OK) {
        return status;
    }
    rc = sqlite3_bind_int64(target->update, (int)candidate->value_count + 1, candidate->rowid);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    rc = sqlite3_step(target->update);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    ++target->affected_rows;
    return mylite_dml_advance_update_auto_increment(
        database,
        &target->write_table,
        candidate,
        &target->next_auto_increment
    );
}

static int finish_joined_update_auto_increment(
    mylite_db *database,
    struct mylite_joined_update_target *targets,
    size_t target_count
) {
    for (size_t index = 0U; index < target_count; ++index) {
        const struct mylite_joined_update_target *target = &targets[index];

        if (target->write_table.has_auto_increment &&
            target->next_auto_increment > target->write_table.next_auto_increment) {
            int status = mylite_transaction_update_table_auto_increment(
                database,
                target->table->schema_name,
                target->table->table_name,
                target->next_auto_increment
            );

            if (status != MYLITE_OK) {
                return status;
            }
        }
    }
    return MYLITE_OK;
}

static void joined_update_targets_deinit(
    struct mylite_joined_update_target *targets,
    size_t target_count
) {
    if (targets == NULL) {
        return;
    }
    for (size_t index = 0U; index < target_count; ++index) {
        sqlite3_finalize(targets[index].update);
        mylite_dml_insert_table_deinit(&targets[index].write_table);
        free(targets[index].assignments);
        free(targets[index].seen_rowids);
    }
    free(targets);
}
