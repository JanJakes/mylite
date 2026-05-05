#include "mylite_select_union.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_expression_descriptor.h"
#include "mylite_metadata.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_span.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static int validate_union_operand_column_counts(mylite_db *database,
                                                const struct mylite_union_plan *plan);
static int bind_union_query_clauses(mylite_db *database,
                                    const struct mylite_sql_ast_node *statement, const char *sql,
                                    size_t sql_length, mylite_stmt *stmt,
                                    const struct mylite_select_union_prepare_callbacks *callbacks);
static int
collect_union_query_operands(mylite_db *database, const struct mylite_sql_ast_node *node,
                             struct mylite_union_plan *plan,
                             const struct mylite_select_union_prepare_callbacks *callbacks);
static int append_union_query_operand(mylite_db *database, struct mylite_union_plan *plan,
                                      mylite_stmt *operand,
                                      enum mylite_sql_ast_set_duplicate_mode duplicate_mode,
                                      bool has_operator);
static bool union_operand_calc_found_rows(const mylite_stmt *operand);
static int set_union_sql_calc_found_rows_placement_error(mylite_db *database);
static int
prepare_union_query_operand(mylite_db *database, const struct mylite_sql_ast_node *node,
                            mylite_stmt **out_operand,
                            const struct mylite_select_union_prepare_callbacks *callbacks);
static const struct mylite_sql_ast_node *
unwrap_union_query_primary(const struct mylite_sql_ast_node *node);
static int attach_union_result_metadata(mylite_stmt *stmt);
static int initialize_union_output_plan(mylite_stmt *stmt);
static int add_union_output_column(mylite_db *database, struct mylite_select_plan *plan,
                                   const char *label);
static int aggregate_union_result_metadata(mylite_stmt *stmt);
static int set_union_column_count_error(mylite_db *database);

int mylite_select_union_prepare_query_expression(
    mylite_db *database, const struct mylite_sql_ast_node *statement, const char *sql,
    size_t sql_length, mylite_stmt **out_stmt,
    const struct mylite_select_union_prepare_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *body = NULL;
    mylite_stmt *stmt = NULL;
    int status = MYLITE_OK;

    if (callbacks == NULL || callbacks->prepare_select_subquery == NULL ||
        callbacks->clone_order_expressions == NULL ||
        callbacks->set_ambiguous_order_column_error == NULL ||
        callbacks->set_unsupported_order_error == NULL) {
        return MYLITE_MISUSE;
    }
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_QUERY_EXPRESSION) {
        return MYLITE_UNSUPPORTED;
    }
    body = mylite_ast_child_at(statement, 0U);
    if (body == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    stmt = calloc(1U, sizeof(*stmt));
    if (stmt == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *stmt = (mylite_stmt){
        .database = database,
        .kind = MYLITE_STMT_UNION_QUERY,
        .affected_rows = -1,
    };

    status = collect_union_query_operands(database, body, &stmt->union_plan, callbacks);
    if (status == MYLITE_OK && stmt->union_plan.operand_count < 2U) {
        status = MYLITE_UNSUPPORTED;
    }
    if (status == MYLITE_OK) {
        status = validate_union_operand_column_counts(database, &stmt->union_plan);
    }
    if (status == MYLITE_OK) {
        status = attach_union_result_metadata(stmt);
    }
    if (status == MYLITE_OK) {
        status = initialize_union_output_plan(stmt);
    }
    if (status == MYLITE_OK) {
        status = bind_union_query_clauses(database, statement, sql, sql_length, stmt, callbacks);
    }
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        mylite_finalize(stmt);
        return status;
    }

    stmt->preserve_prepare_warnings = database->warnings.count > 0U;
    *out_stmt = stmt;
    return MYLITE_OK;
}

static int validate_union_operand_column_counts(mylite_db *database,
                                                const struct mylite_union_plan *plan)
{
    int column_count = 0;

    if (plan == NULL || plan->operand_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    column_count = mylite_column_count(plan->operands[0]);
    if (column_count <= 0) {
        return MYLITE_UNSUPPORTED;
    }

    for (size_t index = 1U; index < plan->operand_count; ++index) {
        if (mylite_column_count(plan->operands[index]) != column_count) {
            return set_union_column_count_error(database);
        }
    }
    return MYLITE_OK;
}

static int bind_union_query_clauses(mylite_db *database,
                                    const struct mylite_sql_ast_node *statement, const char *sql,
                                    size_t sql_length, mylite_stmt *stmt,
                                    const struct mylite_select_union_prepare_callbacks *callbacks)
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
        status = mylite_select_union_bind_global_order_by_clause(database, order_by_clause,
                                                                 &stmt->select_plan, callbacks);
    }
    if (status == MYLITE_OK && stmt->select_plan.order_key_count != 0U) {
        stmt->select_sql_text = mylite_copy_span_text(sql, sql_length);
        if (stmt->select_sql_text == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK) {
        status = callbacks->clone_order_expressions(stmt, sql, sql_length);
    }
    return status;
}

static int collect_union_query_operands( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_sql_ast_node *node, struct mylite_union_plan *plan,
    const struct mylite_select_union_prepare_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *unwrapped = unwrap_union_query_primary(node);

    if (unwrapped == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (unwrapped->kind == MYLITE_SQL_AST_UNION_EXPRESSION) {
        const struct mylite_sql_ast_node *left = mylite_ast_child_at(unwrapped, 0U);
        const struct mylite_sql_ast_node *right = mylite_ast_child_at(unwrapped, 1U);
        mylite_stmt *right_operand = NULL;
        int status = collect_union_query_operands(database, left, plan, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
        status = prepare_union_query_operand(database, right, &right_operand, callbacks);
        if (status != MYLITE_OK) {
            return status;
        }
        status = append_union_query_operand(database, plan, right_operand,
                                            unwrapped->set_duplicate_mode, true);
        if (status != MYLITE_OK) {
            mylite_finalize(right_operand);
        }
        return status;
    }

    {
        mylite_stmt *operand = NULL;
        int status = prepare_union_query_operand(database, unwrapped, &operand, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
        status = append_union_query_operand(database, plan, operand,
                                            MYLITE_SQL_AST_SET_DUPLICATES_DISTINCT, false);
        if (status != MYLITE_OK) {
            mylite_finalize(operand);
        }
        return status;
    }
}

static int append_union_query_operand(mylite_db *database, struct mylite_union_plan *plan,
                                      mylite_stmt *operand,
                                      enum mylite_sql_ast_set_duplicate_mode duplicate_mode,
                                      bool has_operator)
{
    mylite_stmt **operands = NULL;

    if (plan == NULL || operand == NULL || (has_operator && plan->operand_count == 0U)) {
        return MYLITE_UNSUPPORTED;
    }
    if (union_operand_calc_found_rows(operand)) {
        if (plan->operand_count != 0U) {
            return set_union_sql_calc_found_rows_placement_error(database);
        }
        plan->calc_found_rows = true;
    }

    operands = (mylite_stmt **)realloc(
        (void *)plan->operands,
        (plan->operand_count + 1U) * sizeof(*plan->operands)); // NOLINT(bugprone-sizeof-expression)
    if (operands == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    plan->operands = operands;

    if (has_operator) {
        enum mylite_sql_ast_set_duplicate_mode *operators =
            (enum mylite_sql_ast_set_duplicate_mode *)realloc(
                (void *)plan->operators, plan->operand_count * sizeof(*plan->operators));

        if (operators == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        plan->operators = operators;
        plan->operators[plan->operand_count - 1U] = duplicate_mode;
    }

    plan->operands[plan->operand_count++] = operand;
    return MYLITE_OK;
}

static bool union_operand_calc_found_rows(const mylite_stmt *operand)
{
    if (operand == NULL) {
        return false;
    }
    switch (operand->kind) {
    case MYLITE_STMT_SCALAR_SELECT:
    case MYLITE_STMT_TABLE_SELECT:
        return operand->select_plan.calc_found_rows;
    case MYLITE_STMT_SQLITE:
    case MYLITE_STMT_CREATE_SCHEMA:
    case MYLITE_STMT_ALTER_SCHEMA:
    case MYLITE_STMT_DROP_SCHEMA:
    case MYLITE_STMT_USE_SCHEMA:
    case MYLITE_STMT_SET_NAMES:
    case MYLITE_STMT_SET_CHARACTER_SET:
    case MYLITE_STMT_SET_SQL_MODE:
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
        return false;
    }
    return false;
}

static int set_union_sql_calc_found_rows_placement_error(mylite_db *database)
{
    static const char message[] = "Incorrect usage/placement of 'SQL_CALC_FOUND_ROWS'";
    int status = mylite_diagnostics_append_warning(
        database, MYLITE_MYSQL_ER_WARN_DEPRECATED_SYNTAX,
        "SQL_CALC_FOUND_ROWS is deprecated and will be removed in a future release. "
        "Consider using two separate queries instead.");

    if (status != MYLITE_OK) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database, MYLITE_MYSQL_ER_WRONG_USAGE_OF_SQL_CALC_FOUND_ROWS, message);
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int
prepare_union_query_operand(mylite_db *database, const struct mylite_sql_ast_node *node,
                            mylite_stmt **out_operand,
                            const struct mylite_select_union_prepare_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *operand = unwrap_union_query_primary(node);

    *out_operand = NULL;
    if (operand == NULL || operand->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return MYLITE_UNSUPPORTED;
    }
    return callbacks->prepare_select_subquery(database, operand, out_operand);
}

static const struct mylite_sql_ast_node *
unwrap_union_query_primary(const struct mylite_sql_ast_node *node)
{
    const struct mylite_sql_ast_node *current = node;

    while (current != NULL && current->kind == MYLITE_SQL_AST_QUERY_PRIMARY) {
        current = mylite_ast_child_at(current, 0U);
    }
    return current;
}

static int attach_union_result_metadata(mylite_stmt *stmt)
{
    const mylite_stmt *first_operand = NULL;
    struct mylite_result_metadata metadata = {0};
    int column_count = 0;

    if (stmt == NULL || stmt->union_plan.operand_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    first_operand = stmt->union_plan.operands[0];
    column_count = mylite_column_count(first_operand);
    if (column_count <= 0) {
        return MYLITE_UNSUPPORTED;
    }

    metadata.columns = calloc((size_t)column_count, sizeof(*metadata.columns));
    if (metadata.columns == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    metadata.column_count = (size_t)column_count;

    for (size_t index = 0U; index < metadata.column_count; ++index) {
        const struct mylite_result_column_metadata *source =
            mylite_result_metadata_column(first_operand, (int)index);
        const char *label = mylite_column_name(first_operand, (int)index);
        int status =
            mylite_result_metadata_copy_text(stmt->database, &metadata.columns[index].name, label);

        if (status == MYLITE_OK) {
            metadata.columns[index].descriptor =
                source == NULL ? mylite_expression_descriptor_defaults() : source->descriptor;
        }
        if (status != MYLITE_OK) {
            mylite_result_metadata_deinit(&metadata);
            return status;
        }
    }

    mylite_result_metadata_deinit(&stmt->result_metadata);
    stmt->result_metadata = metadata;
    return aggregate_union_result_metadata(stmt);
}

static int initialize_union_output_plan(mylite_stmt *stmt)
{
    if (stmt == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    for (size_t index = 0U; index < stmt->result_metadata.column_count; ++index) {
        int status = add_union_output_column(stmt->database, &stmt->select_plan,
                                             stmt->result_metadata.columns[index].name);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return stmt->select_plan.output_count == stmt->result_metadata.column_count ? MYLITE_OK
                                                                                : MYLITE_NOMEM;
}

static int add_union_output_column(mylite_db *database, struct mylite_select_plan *plan,
                                   const char *label)
{
    struct mylite_select_output_column output = {
        .kind = MYLITE_SELECT_OUTPUT_EXPRESSION,
        .column_index = plan->output_count,
    };

    output.label = label == NULL ? NULL : mylite_copy_span_text(label, strlen(label));
    if (label != NULL && output.label == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    {
        int status = mylite_select_plan_add_output_column(plan, &output);

        if (status != MYLITE_OK) {
            mylite_select_output_column_deinit(&output);
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }
}

static int aggregate_union_result_metadata(mylite_stmt *stmt)
{
    for (size_t operand_index = 1U; operand_index < stmt->union_plan.operand_count;
         ++operand_index) {
        const mylite_stmt *operand = stmt->union_plan.operands[operand_index];

        for (size_t column_index = 0U; column_index < stmt->result_metadata.column_count;
             ++column_index) {
            const struct mylite_result_column_metadata *source =
                mylite_result_metadata_column(operand, (int)column_index);
            struct mylite_field_descriptor descriptor =
                source == NULL ? mylite_expression_descriptor_defaults() : source->descriptor;

            mylite_expression_descriptor_merge_union_operand(
                stmt->database, &stmt->result_metadata.columns[column_index].descriptor,
                &descriptor);
        }
    }
    return MYLITE_OK;
}

static int set_union_column_count_error(mylite_db *database)
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
