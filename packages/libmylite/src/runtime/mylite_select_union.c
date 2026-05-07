#include "mylite_select_union.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_select_rowset.h"
#include "mylite_select_rowset_distinct.h"
#include "mylite_values_query.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

static int materialize_union_query_result(
    mylite_stmt *stmt,
    const struct mylite_select_union_callbacks *callbacks
);

static int scan_union_operand(
    mylite_stmt *stmt,
    mylite_stmt *operand,
    enum mylite_sql_ast_set_duplicate_mode duplicate_mode,
    const struct mylite_select_union_callbacks *callbacks
);

static int apply_intersect_operand(
    mylite_stmt *stmt,
    mylite_stmt *operand,
    enum mylite_sql_ast_set_duplicate_mode duplicate_mode,
    const struct mylite_select_union_callbacks *callbacks
);

static int apply_except_operand(
    mylite_stmt *stmt,
    mylite_stmt *operand,
    enum mylite_sql_ast_set_duplicate_mode duplicate_mode,
    const struct mylite_select_union_callbacks *callbacks
);

static int materialize_set_operand_rows(
    mylite_stmt *stmt,
    mylite_stmt *operand,
    struct mylite_table_select_result *out_result,
    const struct mylite_select_union_callbacks *callbacks
);

static int append_union_operand_current_row_to_result(
    mylite_stmt *stmt,
    mylite_stmt *operand,
    struct mylite_table_select_result *result,
    const struct mylite_select_union_callbacks *callbacks
);

static int keep_intersect_distinct_rows(
    mylite_stmt *stmt,
    const struct mylite_table_select_result *right
);

static int keep_except_distinct_rows(
    mylite_stmt *stmt,
    const struct mylite_table_select_result *right
);

static int keep_intersect_all_rows(
    mylite_stmt *stmt,
    const struct mylite_table_select_result *right
);

static int keep_except_all_rows(mylite_stmt *stmt, const struct mylite_table_select_result *right);

static bool rowset_contains_row(
    const mylite_stmt *stmt,
    const struct mylite_table_select_result *rowset,
    const struct mylite_table_select_row *row
);

static bool consume_matching_row(
    const mylite_stmt *stmt,
    const struct mylite_table_select_result *rowset,
    bool *used,
    const struct mylite_table_select_row *row
);

static int append_union_operand_current_row(
    mylite_stmt *stmt,
    mylite_stmt *operand,
    bool distinct,
    const struct mylite_select_union_callbacks *callbacks
);

static int execute_union_operand_statement(
    mylite_stmt *operand,
    const struct mylite_select_union_callbacks *callbacks
);

static int copy_union_operand_current_row(
    mylite_stmt *stmt,
    mylite_stmt *operand,
    struct mylite_table_select_row *out_row,
    const struct mylite_select_union_callbacks *callbacks
);

static int append_union_distinct_row(mylite_stmt *stmt, struct mylite_table_select_row *row);

static int deduplicate_union_result_rows(mylite_stmt *stmt);

static void record_union_found_rows(mylite_stmt *stmt, uint64_t pre_limit_count);

static uint64_t found_rows_count_after_limit(
    uint64_t pre_limit_count,
    const struct mylite_select_limit *limit,
    size_t returned_count
);

static int append_and_clear_union_database_warnings(
    mylite_db *database,
    struct mylite_expression_warnings *warnings,
    const struct mylite_select_union_callbacks *callbacks
);

int mylite_select_union_execute_query(
    mylite_stmt *stmt,
    const struct mylite_select_union_callbacks *callbacks
) {
    int status = MYLITE_OK;

    stmt->executed = true;
    stmt->affected_rows = -1;

    status = materialize_union_query_result(stmt, callbacks);
    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->select_result.next_row >= stmt->select_result.row_count) {
        mylite_select_result_current_values_deinit(&stmt->select_result);
        stmt->select_result.has_current_row = false;
        return MYLITE_DONE;
    }

    status = mylite_select_eval_set_current_row(
        stmt,
        &stmt->select_result.rows[stmt->select_result.next_row],
        callbacks->select_eval_callbacks
    );
    if (status != MYLITE_OK) {
        return status;
    }
    ++stmt->select_result.next_row;
    return MYLITE_ROW;
}

static int materialize_union_query_result(
    mylite_stmt *stmt,
    const struct mylite_select_union_callbacks *callbacks
) {
    struct mylite_expression_warnings saved_warnings = {0};
    struct mylite_expression_warnings accumulated_warnings = {0};
    uint64_t pre_limit_count = 0U;
    int status = MYLITE_OK;

    if (stmt->select_result.materialized) {
        return MYLITE_OK;
    }
    if (!stmt->select_plan.calc_found_rows && stmt->select_plan.limit.has_limit &&
        stmt->select_plan.limit.row_count == 0U) {
        stmt->found_rows = 0U;
        stmt->database->previous_found_rows = 0U;
        stmt->previous_found_rows_recorded = true;
        stmt->select_result.materialized = true;
        return MYLITE_OK;
    }

    saved_warnings = stmt->database->warnings;
    stmt->database->warnings = (struct mylite_expression_warnings){0};

    for (size_t index = 0U; status == MYLITE_OK && index < stmt->union_plan.operand_count;
         ++index) {
        enum mylite_sql_ast_set_duplicate_mode duplicate_mode =
            index == 0U ? MYLITE_SQL_AST_SET_DUPLICATES_ALL
                        : stmt->union_plan.operators[index - 1U];
        enum mylite_sql_ast_set_operation operation = index == 0U
                                                          ? MYLITE_SQL_AST_SET_OPERATION_UNION
                                                          : stmt->union_plan.operations[index - 1U];

        switch (operation) {
        case MYLITE_SQL_AST_SET_OPERATION_UNION:
            status = scan_union_operand(
                stmt,
                stmt->union_plan.operands[index],
                duplicate_mode,
                callbacks
            );
            break;
        case MYLITE_SQL_AST_SET_OPERATION_INTERSECT:
            status = apply_intersect_operand(
                stmt,
                stmt->union_plan.operands[index],
                duplicate_mode,
                callbacks
            );
            break;
        case MYLITE_SQL_AST_SET_OPERATION_EXCEPT:
            status = apply_except_operand(
                stmt,
                stmt->union_plan.operands[index],
                duplicate_mode,
                callbacks
            );
            break;
        }
        if (append_and_clear_union_database_warnings(
                stmt->database,
                &accumulated_warnings,
                callbacks
            ) != MYLITE_OK) {
            status = MYLITE_NOMEM;
        }
    }

    if (status == MYLITE_OK && stmt->select_plan.order_key_count != 0U) {
        for (size_t index = 0U; status == MYLITE_OK && index < stmt->select_result.row_count;
             ++index) {
            status = mylite_select_union_evaluate_order_values(
                stmt,
                &stmt->select_result.rows[index],
                callbacks
            );
        }
        if (status == MYLITE_OK) {
            status = mylite_select_result_sort_rows(
                stmt->database,
                &stmt->select_result,
                &stmt->select_plan
            );
        }
    }
    if (status == MYLITE_OK) {
        pre_limit_count = stmt->select_result.row_count;
    }
    if (status == MYLITE_OK) {
        status = mylite_select_result_apply_limit(&stmt->select_result, &stmt->select_plan.limit);
    }
    if (status == MYLITE_OK) {
        record_union_found_rows(stmt, pre_limit_count);
    }
    if (status == MYLITE_OK) {
        stmt->select_result.materialized = true;
    }

    if (append_and_clear_union_database_warnings(
            stmt->database,
            &accumulated_warnings,
            callbacks
        ) != MYLITE_OK) {
        status = MYLITE_NOMEM;
    }
    stmt->database->warnings = saved_warnings;
    if (callbacks->append_warnings(&stmt->database->warnings, &accumulated_warnings) != MYLITE_OK) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        status = MYLITE_NOMEM;
    }
    mylite_expression_warnings_deinit(&accumulated_warnings);
    return status;
}

static int scan_union_operand(
    mylite_stmt *stmt,
    mylite_stmt *operand,
    enum mylite_sql_ast_set_duplicate_mode duplicate_mode,
    const struct mylite_select_union_callbacks *callbacks
) {
    bool distinct = duplicate_mode == MYLITE_SQL_AST_SET_DUPLICATES_DISTINCT;

    if (distinct) {
        int status = deduplicate_union_result_rows(stmt);

        if (status != MYLITE_OK) {
            return status;
        }
    }

    for (;;) {
        int status = execute_union_operand_statement(operand, callbacks);

        if (status == MYLITE_DONE) {
            return MYLITE_OK;
        }
        if (status != MYLITE_ROW) {
            return status;
        }
        status = append_union_operand_current_row(stmt, operand, distinct, callbacks);
        if (status != MYLITE_OK) {
            return status;
        }
    }
}

static int apply_intersect_operand(
    mylite_stmt *stmt,
    mylite_stmt *operand,
    enum mylite_sql_ast_set_duplicate_mode duplicate_mode,
    const struct mylite_select_union_callbacks *callbacks
) {
    struct mylite_table_select_result right = {0};
    int status = materialize_set_operand_rows(stmt, operand, &right, callbacks);

    if (status == MYLITE_OK && duplicate_mode == MYLITE_SQL_AST_SET_DUPLICATES_DISTINCT) {
        status = keep_intersect_distinct_rows(stmt, &right);
    } else if (status == MYLITE_OK) {
        status = keep_intersect_all_rows(stmt, &right);
    }
    mylite_select_result_deinit(&right);
    return status;
}

static int apply_except_operand(
    mylite_stmt *stmt,
    mylite_stmt *operand,
    enum mylite_sql_ast_set_duplicate_mode duplicate_mode,
    const struct mylite_select_union_callbacks *callbacks
) {
    struct mylite_table_select_result right = {0};
    int status = materialize_set_operand_rows(stmt, operand, &right, callbacks);

    if (status == MYLITE_OK && duplicate_mode == MYLITE_SQL_AST_SET_DUPLICATES_DISTINCT) {
        status = keep_except_distinct_rows(stmt, &right);
    } else if (status == MYLITE_OK) {
        status = keep_except_all_rows(stmt, &right);
    }
    mylite_select_result_deinit(&right);
    return status;
}

static int materialize_set_operand_rows(
    mylite_stmt *stmt,
    mylite_stmt *operand,
    struct mylite_table_select_result *out_result,
    const struct mylite_select_union_callbacks *callbacks
) {
    for (;;) {
        int status = execute_union_operand_statement(operand, callbacks);

        if (status == MYLITE_DONE) {
            return MYLITE_OK;
        }
        if (status != MYLITE_ROW) {
            return status;
        }
        status = append_union_operand_current_row_to_result(stmt, operand, out_result, callbacks);
        if (status != MYLITE_OK) {
            return status;
        }
    }
}

static int append_union_operand_current_row_to_result(
    mylite_stmt *stmt,
    mylite_stmt *operand,
    struct mylite_table_select_result *result,
    const struct mylite_select_union_callbacks *callbacks
) {
    struct mylite_table_select_row row = {0};
    int status = copy_union_operand_current_row(stmt, operand, &row, callbacks);

    if (status == MYLITE_OK) {
        status = mylite_select_result_append_row(stmt->database, result, &row);
    }
    mylite_select_row_deinit(&row);
    return status;
}

static int keep_intersect_distinct_rows(
    mylite_stmt *stmt,
    const struct mylite_table_select_result *right
) {
    size_t kept = 0U;
    int status = deduplicate_union_result_rows(stmt);

    if (status != MYLITE_OK) {
        return status;
    }
    for (size_t index = 0U; index < stmt->select_result.row_count; ++index) {
        if (!rowset_contains_row(stmt, right, &stmt->select_result.rows[index])) {
            mylite_select_row_deinit(&stmt->select_result.rows[index]);
            continue;
        }
        if (kept != index) {
            stmt->select_result.rows[kept] = stmt->select_result.rows[index];
            stmt->select_result.rows[index] = (struct mylite_table_select_row){0};
        }
        ++kept;
    }
    stmt->select_result.row_count = kept;
    return MYLITE_OK;
}

static int keep_except_distinct_rows(
    mylite_stmt *stmt,
    const struct mylite_table_select_result *right
) {
    size_t kept = 0U;
    int status = deduplicate_union_result_rows(stmt);

    if (status != MYLITE_OK) {
        return status;
    }
    for (size_t index = 0U; index < stmt->select_result.row_count; ++index) {
        if (rowset_contains_row(stmt, right, &stmt->select_result.rows[index])) {
            mylite_select_row_deinit(&stmt->select_result.rows[index]);
            continue;
        }
        if (kept != index) {
            stmt->select_result.rows[kept] = stmt->select_result.rows[index];
            stmt->select_result.rows[index] = (struct mylite_table_select_row){0};
        }
        ++kept;
    }
    stmt->select_result.row_count = kept;
    return MYLITE_OK;
}

static int keep_intersect_all_rows(
    mylite_stmt *stmt,
    const struct mylite_table_select_result *right
) {
    bool *used = NULL;
    size_t kept = 0U;

    if (right->row_count != 0U) {
        used = calloc(right->row_count, sizeof(*used));
        if (used == NULL) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    for (size_t index = 0U; index < stmt->select_result.row_count; ++index) {
        if (!consume_matching_row(stmt, right, used, &stmt->select_result.rows[index])) {
            mylite_select_row_deinit(&stmt->select_result.rows[index]);
            continue;
        }
        if (kept != index) {
            stmt->select_result.rows[kept] = stmt->select_result.rows[index];
            stmt->select_result.rows[index] = (struct mylite_table_select_row){0};
        }
        ++kept;
    }
    free(used);
    stmt->select_result.row_count = kept;
    return MYLITE_OK;
}

static int keep_except_all_rows(mylite_stmt *stmt, const struct mylite_table_select_result *right) {
    bool *used = NULL;
    size_t kept = 0U;

    if (right->row_count != 0U) {
        used = calloc(right->row_count, sizeof(*used));
        if (used == NULL) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    for (size_t index = 0U; index < stmt->select_result.row_count; ++index) {
        if (consume_matching_row(stmt, right, used, &stmt->select_result.rows[index])) {
            mylite_select_row_deinit(&stmt->select_result.rows[index]);
            continue;
        }
        if (kept != index) {
            stmt->select_result.rows[kept] = stmt->select_result.rows[index];
            stmt->select_result.rows[index] = (struct mylite_table_select_row){0};
        }
        ++kept;
    }
    free(used);
    stmt->select_result.row_count = kept;
    return MYLITE_OK;
}

static bool rowset_contains_row(
    const mylite_stmt *stmt,
    const struct mylite_table_select_result *rowset,
    const struct mylite_table_select_row *row
) {
    for (size_t index = 0U; index < rowset->row_count; ++index) {
        if (mylite_select_output_values_equal(
                &stmt->select_plan,
                &stmt->result_metadata,
                &rowset->rows[index],
                row
            )) {
            return true;
        }
    }
    return false;
}

static bool consume_matching_row(
    const mylite_stmt *stmt,
    const struct mylite_table_select_result *rowset,
    bool *used,
    const struct mylite_table_select_row *row
) {
    for (size_t index = 0U; index < rowset->row_count; ++index) {
        if (used != NULL && used[index]) {
            continue;
        }
        if (mylite_select_output_values_equal(
                &stmt->select_plan,
                &stmt->result_metadata,
                &rowset->rows[index],
                row
            )) {
            if (used != NULL) {
                used[index] = true;
            }
            return true;
        }
    }
    return false;
}

static int append_union_operand_current_row(
    mylite_stmt *stmt,
    mylite_stmt *operand,
    bool distinct,
    const struct mylite_select_union_callbacks *callbacks
) {
    struct mylite_table_select_row row = {0};
    int status = copy_union_operand_current_row(stmt, operand, &row, callbacks);

    if (status == MYLITE_OK) {
        if (distinct) {
            status = append_union_distinct_row(stmt, &row);
        } else {
            status = mylite_select_result_append_row(stmt->database, &stmt->select_result, &row);
        }
    }
    mylite_select_row_deinit(&row);
    return status;
}

static int execute_union_operand_statement(
    mylite_stmt *operand,
    const struct mylite_select_union_callbacks *callbacks
) {
    if (operand == NULL) {
        return MYLITE_MISUSE;
    }

    switch (operand->kind) {
    case MYLITE_STMT_SQLITE: {
        int rc = sqlite3_step(operand->sqlite_stmt);

        if (rc == SQLITE_ROW) {
            return MYLITE_ROW;
        }
        if (rc == SQLITE_DONE) {
            return MYLITE_DONE;
        }
        return mylite_diagnostics_set_sqlite_error(operand->database);
    }
    case MYLITE_STMT_SCALAR_SELECT:
        return callbacks->execute_scalar_select(operand);
    case MYLITE_STMT_TABLE_SELECT:
        return callbacks->execute_table_select(operand);
    case MYLITE_STMT_UNION_QUERY:
        return mylite_select_union_execute_query(operand, callbacks);
    case MYLITE_STMT_VALUES_QUERY:
        return mylite_values_query_execute_statement(
            operand,
            callbacks->scalar_callbacks,
            callbacks
        );
    case MYLITE_STMT_CREATE_SCHEMA:
    case MYLITE_STMT_ALTER_SCHEMA:
    case MYLITE_STMT_DROP_SCHEMA:
    case MYLITE_STMT_USE_SCHEMA:
    case MYLITE_STMT_SET_NAMES:
    case MYLITE_STMT_SET_CHARACTER_SET:
    case MYLITE_STMT_SET_SYSTEM_VARIABLE:
    case MYLITE_STMT_SET_USER_VARIABLE:
    case MYLITE_STMT_PREPARE_STATEMENT:
    case MYLITE_STMT_EXECUTE_PREPARED:
    case MYLITE_STMT_DEALLOCATE_PREPARE:
    case MYLITE_STMT_CALL_PLACEHOLDER:
    case MYLITE_STMT_CREATE_PROCEDURE_PLACEHOLDER:
    case MYLITE_STMT_CREATE_FUNCTION_PLACEHOLDER:
    case MYLITE_STMT_CREATE_TRIGGER_PLACEHOLDER:
    case MYLITE_STMT_CREATE_EVENT_PLACEHOLDER:
    case MYLITE_STMT_DROP_PROCEDURE_PLACEHOLDER:
    case MYLITE_STMT_DROP_FUNCTION_PLACEHOLDER:
    case MYLITE_STMT_DROP_TRIGGER_PLACEHOLDER:
    case MYLITE_STMT_DROP_EVENT_PLACEHOLDER:
    case MYLITE_STMT_SIGNAL_PLACEHOLDER:
    case MYLITE_STMT_EXPLAIN_PLACEHOLDER:
    case MYLITE_STMT_ALTER_USER_PLACEHOLDER:
    case MYLITE_STMT_CREATE_USER_PLACEHOLDER:
    case MYLITE_STMT_CREATE_ROLE_PLACEHOLDER:
    case MYLITE_STMT_DROP_USER_PLACEHOLDER:
    case MYLITE_STMT_DROP_ROLE_PLACEHOLDER:
    case MYLITE_STMT_GRANT_PLACEHOLDER:
    case MYLITE_STMT_RENAME_USER_PLACEHOLDER:
    case MYLITE_STMT_REVOKE_PLACEHOLDER:
    case MYLITE_STMT_SET_DEFAULT_ROLE_PLACEHOLDER:
    case MYLITE_STMT_SET_PASSWORD_PLACEHOLDER:
    case MYLITE_STMT_SET_ROLE_PLACEHOLDER:
    case MYLITE_STMT_SHOW_GRANTS_PLACEHOLDER:
    case MYLITE_STMT_SHOW_PRIVILEGES_PLACEHOLDER:
    case MYLITE_STMT_TABLE_PARTITIONING_PLACEHOLDER:
    case MYLITE_STMT_CTE_PLACEHOLDER:
    case MYLITE_STMT_LOCK_TABLES_PLACEHOLDER:
    case MYLITE_STMT_UNLOCK_TABLES_PLACEHOLDER:
    case MYLITE_STMT_CREATE_TABLE:
    case MYLITE_STMT_DROP_TABLE:
    case MYLITE_STMT_RENAME_TABLE:
    case MYLITE_STMT_TRUNCATE_TABLE:
    case MYLITE_STMT_ALTER_TABLE:
    case MYLITE_STMT_CREATE_INDEX:
    case MYLITE_STMT_DROP_INDEX:
    case MYLITE_STMT_INSERT_VALUES:
    case MYLITE_STMT_INSERT_SET:
    case MYLITE_STMT_INSERT_SELECT:
    case MYLITE_STMT_REPLACE_VALUES:
    case MYLITE_STMT_REPLACE_SET:
    case MYLITE_STMT_UPDATE:
    case MYLITE_STMT_DELETE:
    case MYLITE_STMT_START_TRANSACTION:
    case MYLITE_STMT_BEGIN_TRANSACTION:
    case MYLITE_STMT_COMMIT:
    case MYLITE_STMT_ROLLBACK:
    case MYLITE_STMT_SAVEPOINT:
    case MYLITE_STMT_ROLLBACK_TO_SAVEPOINT:
    case MYLITE_STMT_RELEASE_SAVEPOINT:
        break;
    }
    return MYLITE_UNSUPPORTED;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static int copy_union_operand_current_row(
    mylite_stmt *stmt,
    mylite_stmt *operand,
    struct mylite_table_select_row *out_row,
    const struct mylite_select_union_callbacks *callbacks
) {
    size_t column_count = stmt->select_plan.output_count;

    *out_row = (struct mylite_table_select_row){0};
    out_row->output_values = calloc(column_count, sizeof(*out_row->output_values));
    if (out_row->output_values == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_row->output_value_count = column_count;

    for (size_t index = 0U; index < column_count; ++index) {
        int status =
            callbacks->copy_operand_row_value(operand, index, &out_row->output_values[index]);

        if (status != MYLITE_OK) {
            if (status == MYLITE_NOMEM) {
                (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            }
            return status;
        }
    }
    return MYLITE_OK;
}

static int append_union_distinct_row(mylite_stmt *stmt, struct mylite_table_select_row *row) {
    if (mylite_select_result_distinct_row_exists(
            &stmt->select_result,
            &stmt->select_plan,
            &stmt->result_metadata,
            row
        )) {
        mylite_select_row_deinit(row);
        return MYLITE_OK;
    }
    return mylite_select_result_append_row(stmt->database, &stmt->select_result, row);
}

static int deduplicate_union_result_rows(mylite_stmt *stmt) {
    size_t kept = 0U;

    for (size_t index = 0U; index < stmt->select_result.row_count; ++index) {
        bool duplicate = false;

        for (size_t compare = 0U; compare < kept; ++compare) {
            if (mylite_select_output_values_equal(
                    &stmt->select_plan,
                    &stmt->result_metadata,
                    &stmt->select_result.rows[compare],
                    &stmt->select_result.rows[index]
                )) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            mylite_select_row_deinit(&stmt->select_result.rows[index]);
            continue;
        }
        if (kept != index) {
            stmt->select_result.rows[kept] = stmt->select_result.rows[index];
            stmt->select_result.rows[index] = (struct mylite_table_select_row){0};
        }
        ++kept;
    }
    stmt->select_result.row_count = kept;
    return MYLITE_OK;
}

static void record_union_found_rows(mylite_stmt *stmt, uint64_t pre_limit_count) {
    uint64_t found_rows;

    if (stmt->union_plan.calc_found_rows) {
        found_rows = pre_limit_count;
    } else {
        found_rows = found_rows_count_after_limit(
            pre_limit_count,
            &stmt->select_plan.limit,
            stmt->select_result.row_count
        );
    }

    stmt->database->previous_found_rows = found_rows;
    stmt->previous_found_rows_recorded = true;
}

static uint64_t found_rows_count_after_limit(
    uint64_t pre_limit_count,
    const struct mylite_select_limit *limit,
    size_t returned_count
) {
    uint64_t returned = (uint64_t)returned_count;
    uint64_t limited_count;

    if (limit == NULL || !limit->has_limit) {
        return pre_limit_count;
    }
    if (limit->offset > UINT64_MAX - returned) {
        limited_count = UINT64_MAX;
    } else {
        limited_count = limit->offset + returned;
    }
    return limited_count < pre_limit_count ? limited_count : pre_limit_count;
}

static int append_and_clear_union_database_warnings(
    mylite_db *database,
    struct mylite_expression_warnings *warnings,
    const struct mylite_select_union_callbacks *callbacks
) {
    struct mylite_expression_warnings current = database->warnings;
    int status = MYLITE_OK;

    database->warnings = (struct mylite_expression_warnings){0};
    status = callbacks->append_warnings(warnings, &current);
    mylite_expression_warnings_deinit(&current);
    if (status != MYLITE_OK) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    return status;
}
