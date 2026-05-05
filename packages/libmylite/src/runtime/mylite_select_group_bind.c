#include "mylite_select_group_bind.h"

#include "mylite_diagnostics.h"
#include "mylite_select.h"
#include "mylite_select_aggregate_bind.h"
#include "mylite_select_group_validate.h"
#include "mylite_select_predicate_bind.h"
#include "mylite_select_resolve.h"
#include "mylite_span.h"

#include <stdlib.h>

static bool
group_bind_callbacks_are_valid(const struct mylite_select_group_bind_callbacks *callbacks);
static int bind_group_item(mylite_db *database, const struct mylite_sql_ast_node *group_item,
                           struct mylite_select_plan *plan,
                           const struct mylite_select_group_bind_callbacks *callbacks);
static int bind_group_expression(mylite_db *database, const struct mylite_sql_ast_node *expression,
                                 struct mylite_select_plan *plan,
                                 const struct mylite_select_group_bind_callbacks *callbacks);

int mylite_select_bind_group_by_clause(mylite_db *database,
                                       const struct mylite_sql_ast_node *group_by_clause,
                                       struct mylite_select_plan *plan,
                                       const struct mylite_select_group_bind_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *items = mylite_ast_child_at(group_by_clause, 0U);

    if (!group_bind_callbacks_are_valid(callbacks)) {
        return MYLITE_MISUSE;
    }
    if (group_by_clause == NULL || group_by_clause->kind != MYLITE_SQL_AST_GROUP_BY_CLAUSE ||
        items == NULL || items->kind != MYLITE_SQL_AST_GROUP_ITEM_LIST) {
        return mylite_select_set_unknown_group_column_error(database, "");
    }

    plan->has_group_by = true;
    for (const struct mylite_sql_ast_node *item = items->first_child; item != NULL;
         item = item->next_sibling) {
        int status = bind_group_item(database, item, plan, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_select_bind_having_clause(mylite_db *database,
                                     const struct mylite_sql_ast_node *having_clause,
                                     struct mylite_select_plan *plan,
                                     const struct mylite_select_group_bind_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *expression = mylite_ast_child_at(having_clause, 0U);

    if (!group_bind_callbacks_are_valid(callbacks)) {
        return MYLITE_MISUSE;
    }
    if (having_clause == NULL || having_clause->kind != MYLITE_SQL_AST_HAVING_CLAUSE ||
        expression == NULL) {
        return callbacks->set_unsupported_where_error(database);
    }

    plan->has_having = true;
    plan->having_expression = expression;
    return mylite_select_bind_aggregate_aware_expression(
        database, expression, plan, "having clause", callbacks->aggregate_callbacks);
}

static bool
group_bind_callbacks_are_valid(const struct mylite_select_group_bind_callbacks *callbacks)
{
    if (callbacks == NULL || callbacks->aggregate_callbacks == NULL ||
        callbacks->predicate_callbacks == NULL ||
        callbacks->set_invalid_group_function_error == NULL ||
        callbacks->set_unsupported_where_error == NULL) {
        return false;
    }
    return true;
}

static int bind_group_item(mylite_db *database, const struct mylite_sql_ast_node *group_item,
                           struct mylite_select_plan *plan,
                           const struct mylite_select_group_bind_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *expression = mylite_ast_child_at(group_item, 0U);
    struct mylite_select_group_key group_key = {
        .kind = MYLITE_SELECT_GROUP_KEY_EXPRESSION,
        .direction = MYLITE_SQL_AST_KEY_PART_ORDER_ASC,
        .expression = expression,
    };

    if (group_item == NULL || group_item->kind != MYLITE_SQL_AST_GROUP_ITEM || expression == NULL) {
        return mylite_select_set_unknown_group_column_error(database, "");
    }
    if (group_item->key_part_order == MYLITE_SQL_AST_KEY_PART_ORDER_DESC) {
        group_key.direction = MYLITE_SQL_AST_KEY_PART_ORDER_DESC;
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
            status = mylite_select_set_unknown_group_column_error(database, reference);
            free(reference);
            return status;
        }
        if (mylite_select_output_contains_aggregate(plan, (size_t)(ordinal - 1U))) {
            return callbacks->set_invalid_group_function_error(database);
        }
        group_key.kind = MYLITE_SELECT_GROUP_KEY_OUTPUT;
        group_key.output_index = (size_t)(ordinal - 1U);
        group_key.expression = NULL;
        return mylite_select_plan_add_group_key(plan, &group_key);
    }

    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER) {
        enum mylite_select_group_key_kind kind = MYLITE_SELECT_GROUP_KEY_EXPRESSION;
        size_t index = 0U;
        int status =
            mylite_select_resolve_group_reference(database, plan, expression, &kind, &index);

        if (status != MYLITE_OK) {
            return status;
        }
        if (kind == MYLITE_SELECT_GROUP_KEY_OUTPUT) {
            if (mylite_select_output_contains_aggregate(plan, index)) {
                return callbacks->set_invalid_group_function_error(database);
            }
            group_key.kind = kind;
            group_key.output_index = index;
            group_key.expression = NULL;
            return mylite_select_plan_add_group_key(plan, &group_key);
        }
    }

    {
        int status = bind_group_expression(database, expression, plan, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return mylite_select_plan_add_group_key(plan, &group_key);
}

static int bind_group_expression(mylite_db *database, const struct mylite_sql_ast_node *expression,
                                 struct mylite_select_plan *plan,
                                 const struct mylite_select_group_bind_callbacks *callbacks)
{
    int status = mylite_select_bind_predicate_expression(database, expression, plan,
                                                         callbacks->predicate_callbacks);

    if (status == MYLITE_UNSUPPORTED) {
        return mylite_select_set_unknown_group_column_error(database, "");
    }
    return status;
}
