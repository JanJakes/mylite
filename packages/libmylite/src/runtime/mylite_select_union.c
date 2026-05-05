#include "mylite_select_union.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_rowset.h"
#include "mylite_select_rowset_distinct.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdlib.h>

struct mylite_union_expression_context {
    mylite_stmt *stmt;
    const struct mylite_table_select_row *row;
    const struct mylite_select_union_callbacks *callbacks;
};

static int materialize_union_query_result(mylite_stmt *stmt,
                                          const struct mylite_select_union_callbacks *callbacks);
static int scan_union_operand(mylite_stmt *stmt, mylite_stmt *operand,
                              enum mylite_sql_ast_set_duplicate_mode duplicate_mode,
                              const struct mylite_select_union_callbacks *callbacks);
static int append_union_operand_current_row(mylite_stmt *stmt, mylite_stmt *operand, bool distinct,
                                            const struct mylite_select_union_callbacks *callbacks);
static int execute_union_operand_statement(mylite_stmt *operand,
                                           const struct mylite_select_union_callbacks *callbacks);
static int copy_union_operand_current_row(mylite_stmt *stmt, mylite_stmt *operand,
                                          struct mylite_table_select_row *out_row,
                                          const struct mylite_select_union_callbacks *callbacks);
static int append_union_distinct_row(mylite_stmt *stmt, struct mylite_table_select_row *row);
static int deduplicate_union_result_rows(mylite_stmt *stmt);
static int evaluate_union_order_values(mylite_stmt *stmt, struct mylite_table_select_row *row,
                                       const struct mylite_select_union_callbacks *callbacks);
static int evaluate_union_order_key(mylite_stmt *stmt, const struct mylite_table_select_row *row,
                                    const struct mylite_select_order_key *order_key,
                                    struct mylite_expression_value *out_value,
                                    const struct mylite_select_union_callbacks *callbacks);
static int resolve_union_expression_identifier(void *user_data,
                                               const struct mylite_sql_ast_node *identifier,
                                               struct mylite_expression_value *out_value);
static int
evaluate_union_session_function(void *user_data, const struct mylite_sql_ast_node *function_call,
                                const struct mylite_expression_eval_context *expression_context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);
static int
append_and_clear_union_database_warnings(mylite_db *database,
                                         struct mylite_expression_warnings *warnings,
                                         const struct mylite_select_union_callbacks *callbacks);

int mylite_select_union_execute_query(mylite_stmt *stmt,
                                      const struct mylite_select_union_callbacks *callbacks)
{
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
        stmt, &stmt->select_result.rows[stmt->select_result.next_row],
        callbacks->select_eval_callbacks);
    if (status != MYLITE_OK) {
        return status;
    }
    ++stmt->select_result.next_row;
    return MYLITE_ROW;
}

static int materialize_union_query_result(mylite_stmt *stmt,
                                          const struct mylite_select_union_callbacks *callbacks)
{
    struct mylite_expression_warnings saved_warnings = {0};
    struct mylite_expression_warnings accumulated_warnings = {0};
    int status = MYLITE_OK;

    if (stmt->select_result.materialized) {
        return MYLITE_OK;
    }

    saved_warnings = stmt->database->warnings;
    stmt->database->warnings = (struct mylite_expression_warnings){0};

    for (size_t index = 0U; status == MYLITE_OK && index < stmt->union_plan.operand_count;
         ++index) {
        enum mylite_sql_ast_set_duplicate_mode duplicate_mode =
            index == 0U ? MYLITE_SQL_AST_SET_DUPLICATES_ALL
                        : stmt->union_plan.operators[index - 1U];

        status =
            scan_union_operand(stmt, stmt->union_plan.operands[index], duplicate_mode, callbacks);
        if (append_and_clear_union_database_warnings(stmt->database, &accumulated_warnings,
                                                     callbacks) != MYLITE_OK) {
            status = MYLITE_NOMEM;
        }
    }

    if (status == MYLITE_OK && stmt->select_plan.order_key_count != 0U) {
        for (size_t index = 0U; status == MYLITE_OK && index < stmt->select_result.row_count;
             ++index) {
            status = evaluate_union_order_values(stmt, &stmt->select_result.rows[index], callbacks);
        }
        if (status == MYLITE_OK) {
            status = mylite_select_result_sort_rows(stmt->database, &stmt->select_result,
                                                    &stmt->select_plan);
        }
    }
    if (status == MYLITE_OK) {
        status = mylite_select_result_apply_limit(&stmt->select_result, &stmt->select_plan.limit);
    }
    if (status == MYLITE_OK) {
        stmt->select_result.materialized = true;
    }

    if (append_and_clear_union_database_warnings(stmt->database, &accumulated_warnings,
                                                 callbacks) != MYLITE_OK) {
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

static int scan_union_operand(mylite_stmt *stmt, mylite_stmt *operand,
                              enum mylite_sql_ast_set_duplicate_mode duplicate_mode,
                              const struct mylite_select_union_callbacks *callbacks)
{
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

static int append_union_operand_current_row(mylite_stmt *stmt, mylite_stmt *operand, bool distinct,
                                            const struct mylite_select_union_callbacks *callbacks)
{
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

static int execute_union_operand_statement(mylite_stmt *operand,
                                           const struct mylite_select_union_callbacks *callbacks)
{
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
    case MYLITE_STMT_CREATE_SCHEMA:
    case MYLITE_STMT_ALTER_SCHEMA:
    case MYLITE_STMT_DROP_SCHEMA:
    case MYLITE_STMT_USE_SCHEMA:
    case MYLITE_STMT_SET_NAMES:
    case MYLITE_STMT_SET_CHARACTER_SET:
    case MYLITE_STMT_CREATE_TABLE:
    case MYLITE_STMT_DROP_TABLE:
    case MYLITE_STMT_RENAME_TABLE:
    case MYLITE_STMT_TRUNCATE_TABLE:
    case MYLITE_STMT_ALTER_TABLE:
    case MYLITE_STMT_CREATE_INDEX:
    case MYLITE_STMT_DROP_INDEX:
    case MYLITE_STMT_INSERT_VALUES:
    case MYLITE_STMT_INSERT_SET:
    case MYLITE_STMT_REPLACE_VALUES:
    case MYLITE_STMT_REPLACE_SET:
    case MYLITE_STMT_UNION_QUERY:
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
static int copy_union_operand_current_row(mylite_stmt *stmt, mylite_stmt *operand,
                                          struct mylite_table_select_row *out_row,
                                          const struct mylite_select_union_callbacks *callbacks)
{
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

static int append_union_distinct_row(mylite_stmt *stmt, struct mylite_table_select_row *row)
{
    if (mylite_select_result_distinct_row_exists(&stmt->select_result, &stmt->select_plan,
                                                 &stmt->result_metadata, row)) {
        mylite_select_row_deinit(row);
        return MYLITE_OK;
    }
    return mylite_select_result_append_row(stmt->database, &stmt->select_result, row);
}

static int deduplicate_union_result_rows(mylite_stmt *stmt)
{
    size_t kept = 0U;

    for (size_t index = 0U; index < stmt->select_result.row_count; ++index) {
        bool duplicate = false;

        for (size_t compare = 0U; compare < kept; ++compare) {
            if (mylite_select_output_values_equal(&stmt->select_plan, &stmt->result_metadata,
                                                  &stmt->select_result.rows[compare],
                                                  &stmt->select_result.rows[index])) {
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

static int evaluate_union_order_values(mylite_stmt *stmt, struct mylite_table_select_row *row,
                                       const struct mylite_select_union_callbacks *callbacks)
{
    row->order_values = calloc(stmt->select_plan.order_key_count, sizeof(*row->order_values));
    if (row->order_values == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    row->order_value_count = stmt->select_plan.order_key_count;

    for (size_t index = 0U; index < stmt->select_plan.order_key_count; ++index) {
        int status = evaluate_union_order_key(stmt, row, &stmt->select_plan.order_keys[index],
                                              &row->order_values[index], callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int evaluate_union_order_key(mylite_stmt *stmt, const struct mylite_table_select_row *row,
                                    const struct mylite_select_order_key *order_key,
                                    struct mylite_expression_value *out_value,
                                    const struct mylite_select_union_callbacks *callbacks)
{
    if (order_key->kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
        if (order_key->output_index >= row->output_value_count ||
            mylite_expression_value_copy(&row->output_values[order_key->output_index], out_value) !=
                0) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }

    {
        struct mylite_union_expression_context user_context = {
            .stmt = stmt,
            .row = row,
            .callbacks = callbacks,
        };
        struct mylite_expression_eval_context context = {
            .user_data = &user_context,
            .resolve_identifier = resolve_union_expression_identifier,
            .eval_session_function = evaluate_union_session_function,
        };
        int status = mylite_expression_eval_with_context(order_key->expression, &context,
                                                         &stmt->database->warnings, out_value);

        if (status == 0) {
            return MYLITE_OK;
        }
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        if (stmt->database->error_message != NULL) {
            return MYLITE_EXEC_ERROR;
        }
    }
    return callbacks->set_unsupported_order_error(stmt->database);
}

static int resolve_union_expression_identifier(void *user_data,
                                               const struct mylite_sql_ast_node *identifier,
                                               struct mylite_expression_value *out_value)
{
    struct mylite_union_expression_context *context = user_data;
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = MYLITE_OK;

    if (context == NULL || context->stmt == NULL || context->row == NULL) {
        return -1;
    }

    status = mylite_copy_identifier_parts(identifier, parts, &part_count);
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(context->stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        return -1;
    }
    if (part_count == 1U) {
        size_t output_index = 0U;
        size_t output_matches =
            mylite_select_output_label_count(&context->stmt->select_plan, parts[0], &output_index);

        if (output_matches == 1U && output_index < context->row->output_value_count) {
            status = mylite_expression_value_copy(&context->row->output_values[output_index],
                                                  out_value) == 0
                         ? 0
                         : MYLITE_NOMEM;
        } else {
            status = -1;
        }
    } else {
        status = -1;
    }

    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(context->stmt->database, "out of memory");
    }
    return status;
}

static int
append_and_clear_union_database_warnings(mylite_db *database,
                                         struct mylite_expression_warnings *warnings,
                                         const struct mylite_select_union_callbacks *callbacks)
{
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

static int
evaluate_union_session_function(void *user_data, const struct mylite_sql_ast_node *function_call,
                                const struct mylite_expression_eval_context *expression_context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    struct mylite_union_expression_context *context = user_data;

    if (context == NULL || context->callbacks == NULL ||
        context->callbacks->select_eval_callbacks == NULL ||
        context->callbacks->select_eval_callbacks->eval_session_function == NULL) {
        return MYLITE_EXEC_ERROR;
    }
    return context->callbacks->select_eval_callbacks->eval_session_function(
        context->stmt, function_call, expression_context, warnings, NULL, out_value);
}
