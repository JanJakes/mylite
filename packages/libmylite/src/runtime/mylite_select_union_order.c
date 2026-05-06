#include "mylite_select_union.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_expression_validation.h"
#include "mylite_select.h"
#include "mylite_select_order_resolve.h"
#include "mylite_select_resolve.h"
#include "mylite_span.h"
#include "mylite_system_variables.h"
#include "mylite_user_variables.h"
#include "sqlite3.h"

#include <stdlib.h>

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
static int set_union_global_order_table_error(mylite_db *database, const char *table_name);

int mylite_select_union_bind_global_order_by_clause(
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

    if ((expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
         expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) &&
        !mylite_system_variable_identifier_is_system_variable(expression) &&
        !mylite_user_variable_identifier_is_user_variable(expression)) {
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
        if (mylite_system_variable_identifier_is_system_variable(expression)) {
            return MYLITE_OK;
        }
        if (mylite_user_variable_identifier_is_user_variable(expression)) {
            return MYLITE_OK;
        }
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
