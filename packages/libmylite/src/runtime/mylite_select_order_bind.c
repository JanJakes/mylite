#include "mylite_select_order_bind.h"

#include "mylite_diagnostics.h"
#include "mylite_select.h"
#include "mylite_select_distinct_validate.h"
#include "mylite_select_order_expression_bind.h"
#include "mylite_select_order_resolve.h"
#include "mylite_span.h"
#include "mylite_system_variables.h"

#include <stdlib.h>

static bool
order_bind_callbacks_are_valid(const struct mylite_select_order_bind_callbacks *callbacks);
static int bind_order_item(mylite_db *database, const struct mylite_sql_ast_node *order_item,
                           struct mylite_select_plan *plan,
                           const struct mylite_select_order_bind_callbacks *callbacks);
int mylite_select_bind_order_by_clause(mylite_db *database,
                                       const struct mylite_sql_ast_node *order_by_clause,
                                       struct mylite_select_plan *plan,
                                       const struct mylite_select_order_bind_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *items = mylite_ast_child_at(order_by_clause, 0U);

    if (!order_bind_callbacks_are_valid(callbacks)) {
        return MYLITE_MISUSE;
    }
    if (order_by_clause == NULL || order_by_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE ||
        items == NULL || items->kind != MYLITE_SQL_AST_ORDER_ITEM_LIST) {
        return callbacks->set_unsupported_order_error(database);
    }

    for (const struct mylite_sql_ast_node *item = items->first_child; item != NULL;
         item = item->next_sibling) {
        int status = bind_order_item(database, item, plan, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (plan->order_key_count == 0U) {
        return callbacks->set_unsupported_order_error(database);
    }
    return mylite_select_validate_distinct_order(database, plan);
}

static bool
order_bind_callbacks_are_valid(const struct mylite_select_order_bind_callbacks *callbacks)
{
    if (callbacks == NULL || callbacks->aggregate_callbacks == NULL ||
        callbacks->subquery_callbacks == NULL || callbacks->set_unsupported_order_error == NULL) {
        return false;
    }
    return true;
}

static int bind_order_item(mylite_db *database, const struct mylite_sql_ast_node *order_item,
                           struct mylite_select_plan *plan,
                           const struct mylite_select_order_bind_callbacks *callbacks)
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

    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER &&
        !mylite_system_variable_identifier_is_system_variable(expression)) {
        enum mylite_select_order_key_kind kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
        size_t index = 0U;
        int status =
            mylite_select_resolve_order_reference(database, plan, expression, &kind, &index);

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
        int status = mylite_select_bind_order_expression(database, expression, plan, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return mylite_select_plan_add_order_key(plan, &order_key);
}
