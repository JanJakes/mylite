#include "mylite_select_union.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_expression_descriptor.h"
#include "mylite_expression_validation.h"
#include "mylite_metadata.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_resolve.h"
#include "mylite_span.h"
#include "sqlite3.h"

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
static int bind_union_global_order_by_clause(
    mylite_db *database, const struct mylite_sql_ast_node *order_by_clause,
    struct mylite_select_plan *plan, const struct mylite_select_union_prepare_callbacks *callbacks);
static int
bind_union_global_order_item(mylite_db *database, const struct mylite_sql_ast_node *order_item,
                             struct mylite_select_plan *plan,
                             const struct mylite_select_union_prepare_callbacks *callbacks);
static int bind_union_global_order_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan, const struct mylite_select_union_prepare_callbacks *callbacks);
static int bind_union_global_order_function_call(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan, const struct mylite_select_union_prepare_callbacks *callbacks);
static int
resolve_union_order_reference(mylite_db *database, const struct mylite_select_plan *plan,
                              const struct mylite_sql_ast_node *expression,
                              enum mylite_select_order_key_kind *out_kind, size_t *out_index,
                              const struct mylite_select_union_prepare_callbacks *callbacks);
static int set_union_column_count_error(mylite_db *database);
static int set_union_global_order_table_error(mylite_db *database, const char *table_name);

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
        status = bind_union_global_order_by_clause(database, order_by_clause, &stmt->select_plan,
                                                   callbacks);
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

    operands = (mylite_stmt **)realloc((void *)plan->operands,
                                       (plan->operand_count + 1U) * sizeof(*plan->operands));
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

static int bind_union_global_order_by_clause(
    mylite_db *database, const struct mylite_sql_ast_node *order_by_clause,
    struct mylite_select_plan *plan, const struct mylite_select_union_prepare_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *items = mylite_ast_child_at(order_by_clause, 0U);

    if (order_by_clause == NULL || order_by_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE ||
        items == NULL || items->kind != MYLITE_SQL_AST_ORDER_ITEM_LIST) {
        return callbacks->set_unsupported_order_error(database);
    }

    for (const struct mylite_sql_ast_node *item = items->first_child; item != NULL;
         item = item->next_sibling) {
        int status = bind_union_global_order_item(database, item, plan, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return plan->order_key_count == 0U ? callbacks->set_unsupported_order_error(database)
                                       : MYLITE_OK;
}

static int
bind_union_global_order_item(mylite_db *database, const struct mylite_sql_ast_node *order_item,
                             struct mylite_select_plan *plan,
                             const struct mylite_select_union_prepare_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *expression = mylite_ast_child_at(order_item, 0U);
    struct mylite_select_order_key order_key = {
        .kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION,
        .direction = MYLITE_SQL_AST_KEY_PART_ORDER_ASC,
        .expression = expression,
    };

    if (order_item == NULL || order_item->kind != MYLITE_SQL_AST_ORDER_ITEM || expression == NULL) {
        return callbacks->set_unsupported_order_error(database);
    }
    if (order_item->key_part_order == MYLITE_SQL_AST_KEY_PART_ORDER_DESC) {
        order_key.direction = MYLITE_SQL_AST_KEY_PART_ORDER_DESC;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        expression->literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        uint64_t ordinal = 0U;

        if (!mylite_select_parse_uint64_span(expression->span, &ordinal) || ordinal == 0U ||
            ordinal > plan->output_count) {
            char *reference = mylite_copy_span_text(expression->span.text, expression->span.length);
            int status = MYLITE_OK;

            if (reference == NULL) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
            status = mylite_select_set_unknown_order_column_error(database, reference);
            free(reference);
            return status;
        }
        order_key.kind = MYLITE_SELECT_ORDER_KEY_OUTPUT;
        order_key.output_index = (size_t)(ordinal - 1U);
        order_key.expression = NULL;
        mylite_select_plan_mark_output_order_reference(plan, order_key.output_index);
        return mylite_select_plan_add_order_key(plan, &order_key);
    }

    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        enum mylite_select_order_key_kind kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
        size_t index = 0U;
        int status =
            resolve_union_order_reference(database, plan, expression, &kind, &index, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
        if (kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
            order_key.kind = kind;
            order_key.output_index = index;
            order_key.expression = NULL;
            mylite_select_plan_mark_output_order_reference(plan, order_key.output_index);
            return mylite_select_plan_add_order_key(plan, &order_key);
        }
    }

    {
        int status = bind_union_global_order_expression(database, expression, plan, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return mylite_select_plan_add_order_key(plan, &order_key);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_union_global_order_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan, const struct mylite_select_union_prepare_callbacks *callbacks)
{
    if (expression == NULL) {
        return callbacks->set_unsupported_order_error(database);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
        return MYLITE_OK;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER: {
        enum mylite_select_order_key_kind kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
        size_t index = 0U;
        int status =
            resolve_union_order_reference(database, plan, expression, &kind, &index, callbacks);

        if (status == MYLITE_OK && kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
            mylite_select_plan_mark_output_order_reference(plan, index);
        }
        return status;
    }
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
    case MYLITE_SQL_AST_CAST_EXPRESSION:
        if (expression->kind == MYLITE_SQL_AST_CAST_EXPRESSION) {
            int status = mylite_expression_validate_cast_target_charset(database, expression);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
             child = child->next_sibling) {
            int status = bind_union_global_order_expression(database, child, plan, callbacks);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_FUNCTION_CALL:
        return bind_union_global_order_function_call(database, expression, plan, callbacks);
    case MYLITE_SQL_AST_AGGREGATE_CALL:
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_QUERY_EXPRESSION:
    case MYLITE_SQL_AST_UNION_EXPRESSION:
    case MYLITE_SQL_AST_QUERY_PRIMARY:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
    case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
    default:
        return callbacks->set_unsupported_order_error(database);
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_union_global_order_function_call(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan, const struct mylite_select_union_prepare_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);

    if (!mylite_expression_is_supported_function_call(expression)) {
        return callbacks->set_unsupported_order_error(database);
    }
    {
        int status = mylite_expression_validate_char_function_charset(database, expression);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL; child = child->next_sibling) {
        int status = bind_union_global_order_expression(database, child, plan, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int
resolve_union_order_reference(mylite_db *database, const struct mylite_select_plan *plan,
                              const struct mylite_sql_ast_node *expression,
                              enum mylite_select_order_key_kind *out_kind, size_t *out_index,
                              const struct mylite_select_union_prepare_callbacks *callbacks)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = mylite_copy_identifier_parts(expression, parts, &part_count);

    *out_kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
    *out_index = 0U;
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }

    if (part_count != 1U) {
        const char *table_name = part_count == 2U ? parts[0] : parts[1];

        status = set_union_global_order_table_error(database, table_name);
        goto cleanup;
    }

    {
        size_t output_index = 0U;
        size_t output_matches = mylite_select_output_label_count(plan, parts[0], &output_index);

        if (output_matches > 1U) {
            status = callbacks->set_ambiguous_order_column_error(database, parts[0]);
            goto cleanup;
        }
        if (output_matches == 1U) {
            *out_kind = MYLITE_SELECT_ORDER_KEY_OUTPUT;
            *out_index = output_index;
            goto cleanup;
        }
    }

    status = mylite_select_set_unknown_order_column_error(database, parts[0]);

cleanup:
    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    return status;
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

static int set_union_global_order_table_error(mylite_db *database, const char *table_name)
{
    char *message =
        sqlite3_mprintf("Table '%q' from one of the SELECTs cannot be used in global ORDER clause",
                        table_name == NULL ? "" : table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database, MYLITE_MYSQL_ER_TABLENAME_NOT_ALLOWED_HERE, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}
