#include "mylite_values_query.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_expression_descriptor.h"
#include "mylite_metadata.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_eval.h"
#include "mylite_select_rowset.h"
#include "mylite_select_rowset_sort.h"
#include "mylite_select_scalar.h"
#include "mylite_select_statement.h"
#include "mylite_select_union.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "mylite_statement_ast.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

static int validate_values_query_prepare_callbacks(
    const struct mylite_select_scalar_eval_callbacks *scalar_callbacks,
    const struct mylite_select_union_prepare_callbacks *order_callbacks);
static int validate_values_query_execute_callbacks(
    const struct mylite_select_scalar_eval_callbacks *scalar_callbacks,
    const struct mylite_select_union_callbacks *order_callbacks);
static int count_values_rows(mylite_db *database, const struct mylite_sql_ast_node *row_list,
                             size_t *out_row_count, size_t *out_column_count);
static int set_values_column_count_error(mylite_db *database);
static int copy_values_query_sql(mylite_stmt *stmt, const struct mylite_sql_ast_node *statement,
                                 const char *sql, size_t sql_length);
static int clone_values_query_expressions(mylite_stmt *stmt,
                                          const struct mylite_sql_ast_node *row_list,
                                          const char *sql, size_t sql_length);
static int attach_values_query_metadata(
    mylite_stmt *stmt, const struct mylite_select_scalar_eval_callbacks *scalar_callbacks);
static int add_values_query_output_column(mylite_db *database, struct mylite_select_plan *plan,
                                          const char *label);
static int bind_values_query_clauses(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *statement, const char *sql,
    size_t sql_length, const struct mylite_select_union_prepare_callbacks *order_callbacks);
static int materialize_values_query_result(
    mylite_stmt *stmt, const struct mylite_select_scalar_eval_callbacks *scalar_callbacks,
    const struct mylite_select_union_callbacks *order_callbacks);
static int append_values_query_row(
    mylite_stmt *stmt, size_t row_index,
    const struct mylite_select_scalar_eval_callbacks *scalar_callbacks);
static void record_values_found_rows(mylite_stmt *stmt, uint64_t pre_limit_count);
static uint64_t values_found_rows_count_after_limit(uint64_t pre_limit_count,
                                                    const struct mylite_select_limit *limit,
                                                    size_t returned_count);

int mylite_values_query_prepare_statement(
    mylite_db *database, const struct mylite_sql_ast_node *statement, const char *sql,
    size_t sql_length, mylite_stmt **out_stmt,
    const struct mylite_select_scalar_eval_callbacks *scalar_callbacks,
    const struct mylite_select_union_prepare_callbacks *order_callbacks)
{
    const struct mylite_sql_ast_node *row_list = mylite_ast_child_at(statement, 0U);
    mylite_stmt *stmt = NULL;
    size_t row_count = 0U;
    size_t column_count = 0U;
    int status = MYLITE_OK;

    if (out_stmt == NULL) {
        return MYLITE_MISUSE;
    }
    *out_stmt = NULL;
    if (database == NULL || statement == NULL ||
        statement->kind != MYLITE_SQL_AST_VALUES_STATEMENT ||
        validate_values_query_prepare_callbacks(scalar_callbacks, order_callbacks) != MYLITE_OK) {
        return MYLITE_UNSUPPORTED;
    }

    status = count_values_rows(database, row_list, &row_count, &column_count);
    if (status != MYLITE_OK) {
        return status;
    }

    stmt = calloc(1U, sizeof(*stmt));
    if (stmt == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *stmt = (mylite_stmt){
        .database = database,
        .kind = MYLITE_STMT_VALUES_QUERY,
        .affected_rows = -1,
    };
    stmt->values_query.row_count = row_count;
    stmt->values_query.column_count = column_count;

    status = copy_values_query_sql(stmt, statement, sql, sql_length);
    if (status == MYLITE_OK) {
        stmt->values_query.expressions =
            calloc(row_count * column_count, sizeof(*stmt->values_query.expressions));
        if (stmt->values_query.expressions == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK) {
        status = clone_values_query_expressions(stmt, row_list, sql, sql_length);
    }
    if (status == MYLITE_OK) {
        status = attach_values_query_metadata(stmt, scalar_callbacks);
    }
    if (status == MYLITE_OK) {
        status = bind_values_query_clauses(stmt, statement, sql, sql_length, order_callbacks);
    }
    if (status != MYLITE_OK) {
        mylite_finalize(stmt);
        return status;
    }

    stmt->preserve_prepare_warnings = database->warnings.count > 0U;
    *out_stmt = stmt;
    return MYLITE_OK;
}

int mylite_values_query_execute_statement(
    mylite_stmt *stmt, const struct mylite_select_scalar_eval_callbacks *scalar_callbacks,
    const struct mylite_select_union_callbacks *order_callbacks)
{
    int status = MYLITE_OK;

    if (stmt == NULL ||
        validate_values_query_execute_callbacks(scalar_callbacks, order_callbacks) != MYLITE_OK) {
        return MYLITE_MISUSE;
    }
    stmt->executed = true;
    stmt->affected_rows = -1;

    status = materialize_values_query_result(stmt, scalar_callbacks, order_callbacks);
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
        order_callbacks->select_eval_callbacks);
    if (status != MYLITE_OK) {
        return status;
    }
    ++stmt->select_result.next_row;
    return MYLITE_ROW;
}

static int validate_values_query_prepare_callbacks(
    const struct mylite_select_scalar_eval_callbacks *scalar_callbacks,
    const struct mylite_select_union_prepare_callbacks *order_callbacks)
{
    if (scalar_callbacks == NULL || scalar_callbacks->infer_expression_descriptor == NULL ||
        order_callbacks == NULL || order_callbacks->clone_order_expressions == NULL ||
        order_callbacks->set_ambiguous_order_column_error == NULL ||
        order_callbacks->set_unsupported_order_error == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    return MYLITE_OK;
}

static int validate_values_query_execute_callbacks(
    const struct mylite_select_scalar_eval_callbacks *scalar_callbacks,
    const struct mylite_select_union_callbacks *order_callbacks)
{
    if (scalar_callbacks == NULL || order_callbacks == NULL ||
        order_callbacks->select_eval_callbacks == NULL ||
        order_callbacks->scalar_callbacks == NULL ||
        order_callbacks->set_unsupported_order_error == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    return MYLITE_OK;
}

static int count_values_rows(mylite_db *database, const struct mylite_sql_ast_node *row_list,
                             size_t *out_row_count, size_t *out_column_count)
{
    static const char message[] = "VALUES ROW() must contain at least one value";
    size_t row_count = 0U;
    size_t column_count = 0U;

    if (row_list == NULL || row_list->kind != MYLITE_SQL_AST_INSERT_ROW_LIST) {
        return MYLITE_UNSUPPORTED;
    }
    for (const struct mylite_sql_ast_node *row = row_list->first_child; row != NULL;
         row = row->next_sibling) {
        size_t current_columns = 0U;

        if (row->kind != MYLITE_SQL_AST_INSERT_ROW) {
            return MYLITE_UNSUPPORTED;
        }
        for (const struct mylite_sql_ast_node *expression = row->first_child; expression != NULL;
             expression = expression->next_sibling) {
            ++current_columns;
        }
        if (current_columns == 0U) {
            int status = mylite_diagnostics_set_error_message(database, message);

            if (status == MYLITE_NOMEM) {
                return MYLITE_NOMEM;
            }
            return MYLITE_EXEC_ERROR;
        }
        if (row_count == 0U) {
            column_count = current_columns;
        } else if (current_columns != column_count) {
            return set_values_column_count_error(database);
        }
        ++row_count;
    }

    if (row_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }
    *out_row_count = row_count;
    *out_column_count = column_count;
    return MYLITE_OK;
}

static int set_values_column_count_error(mylite_db *database)
{
    static const char message[] = "The used SELECT statements have a different number of columns";
    int status = mylite_diagnostics_set_error_message(database, message);

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(
        database, MYLITE_MYSQL_ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT, message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int copy_values_query_sql(mylite_stmt *stmt, const struct mylite_sql_ast_node *statement,
                                 const char *sql, size_t sql_length)
{
    const char *source = sql == NULL ? statement->span.text : sql;
    size_t source_length = sql == NULL ? statement->span.length : sql_length;

    stmt->scalar_select_sql_text = mylite_copy_span_text(source, source_length);
    if (stmt->scalar_select_sql_text == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int clone_values_query_expressions(mylite_stmt *stmt,
                                          const struct mylite_sql_ast_node *row_list,
                                          const char *sql, size_t sql_length)
{
    const char *source = sql == NULL ? row_list->span.text : sql;
    size_t source_length = sql == NULL ? row_list->span.length : sql_length;
    size_t index = 0U;

    for (const struct mylite_sql_ast_node *row = row_list->first_child; row != NULL;
         row = row->next_sibling) {
        for (const struct mylite_sql_ast_node *expression = row->first_child; expression != NULL;
             expression = expression->next_sibling, ++index) {
            struct mylite_sql_ast_node *clone = NULL;
            int status = mylite_statement_ast_clone_subtree(
                &stmt->scalar_select_ast, expression, source, stmt->scalar_select_sql_text,
                source_length, &clone);

            if (status == MYLITE_NOMEM) {
                (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            }
            if (status != MYLITE_OK) {
                return status;
            }
            stmt->values_query.expressions[index] = clone;
        }
    }
    return MYLITE_OK;
}

static int
attach_values_query_metadata(mylite_stmt *stmt,
                             const struct mylite_select_scalar_eval_callbacks *scalar_callbacks)
{
    struct mylite_result_metadata metadata = {0};
    char label[32];

    metadata.columns = calloc(stmt->values_query.column_count, sizeof(*metadata.columns));
    if (metadata.columns == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    metadata.column_count = stmt->values_query.column_count;

    for (size_t column = 0U; column < stmt->values_query.column_count; ++column) {
        int written = snprintf(label, sizeof(label), "column_%zu", column);
        int status = MYLITE_OK;

        if (written < 0 || (size_t)written >= sizeof(label)) {
            mylite_result_metadata_deinit(&metadata);
            return MYLITE_UNSUPPORTED;
        }
        status = mylite_result_metadata_copy_text(stmt->database, &metadata.columns[column].name,
                                                  label);
        if (status == MYLITE_OK) {
            metadata.columns[column].descriptor = mylite_expression_descriptor_defaults();
        }
        if (status == MYLITE_OK) {
            status = add_values_query_output_column(stmt->database, &stmt->select_plan, label);
        }
        if (status != MYLITE_OK) {
            mylite_result_metadata_deinit(&metadata);
            return status;
        }
    }

    for (size_t row_index = 0U; row_index < stmt->values_query.row_count; ++row_index) {
        for (size_t column = 0U; column < stmt->values_query.column_count; ++column) {
            const struct mylite_sql_ast_node *expression =
                stmt->values_query.expressions[row_index * stmt->values_query.column_count +
                                               column];
            struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
            int status = scalar_callbacks->infer_expression_descriptor(
                stmt->database, expression, NULL, &descriptor);

            if (status != MYLITE_OK) {
                mylite_result_metadata_deinit(&metadata);
                return status;
            }
            if (row_index == 0U) {
                metadata.columns[column].descriptor = descriptor;
            } else {
                mylite_expression_descriptor_merge_union_operand(
                    stmt->database, &metadata.columns[column].descriptor, &descriptor);
            }
        }
    }

    mylite_result_metadata_deinit(&stmt->result_metadata);
    stmt->result_metadata = metadata;
    return MYLITE_OK;
}

static int add_values_query_output_column(mylite_db *database, struct mylite_select_plan *plan,
                                          const char *label)
{
    struct mylite_select_output_column output = {
        .kind = MYLITE_SELECT_OUTPUT_EXPRESSION,
    };
    int status = mylite_result_metadata_copy_text(database, &output.label, label);

    if (status == MYLITE_OK) {
        status = mylite_select_plan_add_output_column(plan, &output);
    }
    if (status != MYLITE_OK) {
        mylite_select_output_column_deinit(&output);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
    }
    return status;
}

static int bind_values_query_clauses(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *statement, const char *sql,
    size_t sql_length, const struct mylite_select_union_prepare_callbacks *order_callbacks)
{
    const struct mylite_sql_ast_node *order_by_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    const struct mylite_sql_ast_node *limit_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    int status = MYLITE_OK;

    if (limit_clause != NULL) {
        status = mylite_select_bind_limit_clause(limit_clause, &stmt->select_plan);
    }
    if (status == MYLITE_OK && order_by_clause != NULL) {
        status = mylite_select_union_bind_global_order_by_clause(
            stmt->database, order_by_clause, &stmt->select_plan, order_callbacks);
    }
    if (status == MYLITE_OK && stmt->select_plan.order_key_count != 0U) {
        status = order_callbacks->clone_order_expressions(stmt, sql, sql_length);
    }
    return status;
}

static int materialize_values_query_result(
    mylite_stmt *stmt, const struct mylite_select_scalar_eval_callbacks *scalar_callbacks,
    const struct mylite_select_union_callbacks *order_callbacks)
{
    uint64_t pre_limit_count = 0U;
    int status = MYLITE_OK;

    if (stmt->select_result.materialized) {
        return MYLITE_OK;
    }
    if (stmt->select_plan.limit.has_limit && stmt->select_plan.limit.row_count == 0U) {
        stmt->database->previous_found_rows = 0U;
        stmt->previous_found_rows_recorded = true;
        stmt->select_result.materialized = true;
        return MYLITE_OK;
    }

    for (size_t row = 0U; status == MYLITE_OK && row < stmt->values_query.row_count; ++row) {
        status = append_values_query_row(stmt, row, scalar_callbacks);
    }
    if (status == MYLITE_OK && stmt->select_plan.order_key_count != 0U) {
        for (size_t index = 0U; status == MYLITE_OK && index < stmt->select_result.row_count;
             ++index) {
            status = mylite_select_union_evaluate_order_values(
                stmt, &stmt->select_result.rows[index], order_callbacks);
        }
        if (status == MYLITE_OK) {
            status = mylite_select_result_sort_rows(stmt->database, &stmt->select_result,
                                                    &stmt->select_plan);
        }
    }
    if (status == MYLITE_OK) {
        pre_limit_count = stmt->select_result.row_count;
        status = mylite_select_result_apply_limit(&stmt->select_result, &stmt->select_plan.limit);
    }
    if (status == MYLITE_OK) {
        record_values_found_rows(stmt, pre_limit_count);
        stmt->select_result.materialized = true;
    }
    if (mylite_select_scalar_append_warnings_to_database(stmt) != MYLITE_OK) {
        status = MYLITE_NOMEM;
    }
    return status;
}

static int append_values_query_row(
    mylite_stmt *stmt, size_t row_index,
    const struct mylite_select_scalar_eval_callbacks *scalar_callbacks)
{
    struct mylite_table_select_row row = {0};
    int status = MYLITE_OK;

    row.output_values = calloc(stmt->values_query.column_count, sizeof(*row.output_values));
    if (row.output_values == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    row.output_value_count = stmt->values_query.column_count;

    for (size_t column = 0U; status == MYLITE_OK && column < stmt->values_query.column_count;
         ++column) {
        const struct mylite_sql_ast_node *expression =
            stmt->values_query.expressions[row_index * stmt->values_query.column_count + column];

        status = mylite_select_scalar_evaluate_expression(stmt, expression, scalar_callbacks,
                                                          &row.output_values[column]);
    }
    if (status == MYLITE_OK) {
        status = mylite_select_result_append_row(stmt->database, &stmt->select_result, &row);
    }
    mylite_select_row_deinit(&row);
    return status;
}

static void record_values_found_rows(mylite_stmt *stmt, uint64_t pre_limit_count)
{
    stmt->database->previous_found_rows =
        values_found_rows_count_after_limit(pre_limit_count, &stmt->select_plan.limit,
                                            stmt->select_result.row_count);
    stmt->previous_found_rows_recorded = true;
}

static uint64_t values_found_rows_count_after_limit(uint64_t pre_limit_count,
                                                    const struct mylite_select_limit *limit,
                                                    size_t returned_count)
{
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
