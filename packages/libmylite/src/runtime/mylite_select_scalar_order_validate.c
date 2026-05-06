#include "mylite_select_scalar_order_validate.h"

#include "mylite_diagnostics.h"
#include "mylite_expression_validation.h"
#include "mylite_metadata.h"
#include "mylite_select.h"
#include "mylite_select_order_resolve.h"
#include "mylite_select_resolve.h"
#include "mylite_select_scalar.h"
#include "mylite_span.h"
#include "mylite_system_variables.h"
#include "mylite_user_variables.h"
#include "sql/mylite_ast.h"

#include <stdint.h>
#include <stdlib.h>

static int
validate_scalar_select_order_item(mylite_db *database, const struct mylite_sql_ast_node *order_item,
                                  const struct mylite_result_metadata *metadata,
                                  const struct mylite_select_scalar_eval_callbacks *callbacks);
static int validate_scalar_select_order_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_result_metadata *metadata,
    const struct mylite_select_scalar_eval_callbacks *callbacks);
static int validate_scalar_select_order_function_call(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_result_metadata *metadata,
    const struct mylite_select_scalar_eval_callbacks *callbacks);
static int
resolve_scalar_select_order_reference(mylite_db *database,
                                      const struct mylite_result_metadata *metadata,
                                      const struct mylite_sql_ast_node *expression,
                                      const struct mylite_select_scalar_eval_callbacks *callbacks);

int mylite_select_scalar_validate_order_by_clause(
    mylite_db *database, const struct mylite_sql_ast_node *order_by_clause,
    const struct mylite_result_metadata *metadata,
    const struct mylite_select_scalar_eval_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *items = mylite_ast_child_at(order_by_clause, 0U);

    if (order_by_clause == NULL || order_by_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE ||
        items == NULL || items->kind != MYLITE_SQL_AST_ORDER_ITEM_LIST) {
        return callbacks->set_unsupported_order_error(database);
    }

    for (const struct mylite_sql_ast_node *item = items->first_child; item != NULL;
         item = item->next_sibling) {
        int status = validate_scalar_select_order_item(database, item, metadata, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int
validate_scalar_select_order_item(mylite_db *database, const struct mylite_sql_ast_node *order_item,
                                  const struct mylite_result_metadata *metadata,
                                  const struct mylite_select_scalar_eval_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *expression = mylite_ast_child_at(order_item, 0U);

    if (order_item == NULL || order_item->kind != MYLITE_SQL_AST_ORDER_ITEM || expression == NULL) {
        return callbacks->set_unsupported_order_error(database);
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        expression->literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        uint64_t ordinal = 0U;

        if (!mylite_select_parse_uint64_span(expression->span, &ordinal) || ordinal == 0U ||
            ordinal > metadata->column_count) {
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
        return MYLITE_OK;
    }

    return validate_scalar_select_order_expression(database, expression, metadata, callbacks);
}

static int validate_scalar_select_order_expression( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_result_metadata *metadata,
    const struct mylite_select_scalar_eval_callbacks *callbacks)
{
    if (expression == NULL) {
        return callbacks->set_unsupported_order_error(database);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return MYLITE_OK;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        if (mylite_system_variable_identifier_is_system_variable(expression)) {
            return MYLITE_OK;
        }
        if (mylite_user_variable_identifier_is_user_variable(expression)) {
            return MYLITE_OK;
        }
        return resolve_scalar_select_order_reference(database, metadata, expression, callbacks);
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
            int status =
                validate_scalar_select_order_expression(database, child, metadata, callbacks);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_FUNCTION_CALL:
        return validate_scalar_select_order_function_call(database, expression, metadata,
                                                          callbacks);
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
static int validate_scalar_select_order_function_call(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_result_metadata *metadata,
    const struct mylite_select_scalar_eval_callbacks *callbacks)
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
        int status = validate_scalar_select_order_expression(database, child, metadata, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int
resolve_scalar_select_order_reference(mylite_db *database,
                                      const struct mylite_result_metadata *metadata,
                                      const struct mylite_sql_ast_node *expression,
                                      const struct mylite_select_scalar_eval_callbacks *callbacks)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = mylite_copy_identifier_parts(expression, parts, &part_count);

    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }

    if (part_count != 1U) {
        const char *table_name = part_count == 2U ? parts[0] : parts[1];

        status = mylite_select_set_unknown_table_error(database, table_name);
        goto cleanup;
    }

    {
        size_t output_index = 0U;
        size_t output_matches =
            mylite_result_metadata_label_count(metadata, parts[0], &output_index);

        (void)output_index;
        if (output_matches > 1U) {
            status = callbacks->set_ambiguous_order_column_error(database, parts[0]);
            goto cleanup;
        }
        if (output_matches == 0U) {
            status = mylite_select_set_unknown_order_column_error(database, parts[0]);
        }
    }

cleanup:
    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    return status;
}
