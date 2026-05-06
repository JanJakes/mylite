#include "mylite_select_aggregate_bind_registration.h"

#include "mylite_diagnostics.h"
#include "mylite_select.h"
#include "mylite_select_aggregate_count_distinct_bind.h"
#include "mylite_select_order_resolve.h"
#include "mylite_select_predicate_bind.h"
#include "mylite_select_predicate_expression_bind.h"
#include "mylite_span.h"

#include <stdlib.h>

static int
collect_aggregate_bindings(mylite_db *database, const struct mylite_sql_ast_node *expression,
                           struct mylite_select_plan *plan,
                           const struct mylite_select_aggregate_bind_callbacks *callbacks);
static int
bind_group_concat_aggregate(mylite_db *database, const struct mylite_sql_ast_node *expression,
                            struct mylite_select_plan *plan,
                            const struct mylite_select_aggregate_bind_callbacks *callbacks);
static int
bind_group_concat_order_by(mylite_db *database, const struct mylite_sql_ast_node *expression,
                           struct mylite_select_plan *plan,
                           const struct mylite_select_aggregate_bind_callbacks *callbacks);
static int
bind_group_concat_order_item(mylite_db *database, const struct mylite_sql_ast_node *order_item,
                             size_t argument_count, struct mylite_select_plan *plan,
                             const struct mylite_select_aggregate_bind_callbacks *callbacks);
static int
bind_aggregate_argument_list(mylite_db *database, const struct mylite_sql_ast_node *arguments,
                             struct mylite_select_plan *plan,
                             const struct mylite_select_aggregate_bind_callbacks *callbacks);
static bool aggregate_argument_list_is_valid(const struct mylite_sql_ast_node *arguments);
static bool aggregate_binding_needs_distinct_argument_descriptors(
    const struct mylite_select_aggregate_binding *binding);

bool mylite_select_aggregate_bind_callbacks_are_valid(
    const struct mylite_select_aggregate_bind_callbacks *callbacks)
{
    if (callbacks == NULL || callbacks->predicate_callbacks == NULL ||
        callbacks->subquery_callbacks == NULL ||
        callbacks->infer_aggregate_expression_descriptor == NULL ||
        callbacks->infer_expression_descriptor == NULL ||
        callbacks->set_invalid_group_function_error == NULL ||
        callbacks->set_unsupported_projection_error == NULL) {
        return false;
    }
    return true;
}

int mylite_select_bind_aggregate_call(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan, const struct mylite_select_aggregate_bind_callbacks *callbacks)
{
    struct mylite_select_aggregate_binding binding = {
        .call = expression,
        .argument = mylite_ast_child_at(expression, 1U),
        .kind = expression->aggregate_kind,
        .argument_kind = expression->aggregate_argument,
    };
    int status = MYLITE_OK;

    if (binding.kind == MYLITE_SQL_AST_AGGREGATE_NONE ||
        binding.argument_kind == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_NONE) {
        return callbacks->set_invalid_group_function_error(database);
    }
    if (binding.kind == MYLITE_SQL_AST_AGGREGATE_GROUP_CONCAT) {
        status = bind_group_concat_aggregate(database, expression, plan, callbacks);
        if (status != MYLITE_OK) {
            return status;
        }
    } else if (binding.argument_kind == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION) {
        status = mylite_select_bind_predicate_expression(database, binding.argument, plan,
                                                         callbacks->predicate_callbacks);
        if (status != MYLITE_OK) {
            return status;
        }
    } else if (binding.argument_kind ==
               MYLITE_SQL_AST_AGGREGATE_ARGUMENT_DISTINCT_EXPRESSION_LIST) {
        status = mylite_select_bind_count_distinct_arguments(database, binding.argument, plan,
                                                             callbacks);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    status = callbacks->infer_aggregate_expression_descriptor(database, plan, expression,
                                                              &binding.descriptor);
    if (status == MYLITE_OK && aggregate_binding_needs_distinct_argument_descriptors(&binding)) {
        status = mylite_select_infer_count_distinct_argument_descriptors(
            database, plan, binding.argument, &binding, callbacks);
    }
    if (status != MYLITE_OK) {
        mylite_select_aggregate_binding_deinit(&binding);
        return status;
    }
    plan->has_aggregate = true;
    status = mylite_select_plan_add_aggregate_binding(plan, &binding);
    if (status != MYLITE_OK) {
        mylite_select_aggregate_binding_deinit(&binding);
    }
    return status;
}

int mylite_select_collect_aggregate_bindings(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan, const struct mylite_select_aggregate_bind_callbacks *callbacks)
{
    if (!mylite_select_aggregate_bind_callbacks_are_valid(callbacks)) {
        return MYLITE_MISUSE;
    }
    return collect_aggregate_bindings(database, expression, plan, callbacks);
}

static int collect_aggregate_bindings( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan, const struct mylite_select_aggregate_bind_callbacks *callbacks)
{
    if (expression == NULL) {
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_AGGREGATE_CALL) {
        struct mylite_select_aggregate_binding binding = {
            .call = expression,
            .argument = mylite_ast_child_at(expression, 1U),
            .kind = expression->aggregate_kind,
            .argument_kind = expression->aggregate_argument,
        };
        int status = callbacks->infer_aggregate_expression_descriptor(database, plan, expression,
                                                                      &binding.descriptor);

        if (status == MYLITE_OK &&
            aggregate_binding_needs_distinct_argument_descriptors(&binding)) {
            status = mylite_select_infer_count_distinct_argument_descriptors(
                database, plan, binding.argument, &binding, callbacks);
        }
        if (status == MYLITE_OK) {
            status = mylite_select_plan_add_aggregate_binding(plan, &binding);
        }
        if (status != MYLITE_OK) {
            mylite_select_aggregate_binding_deinit(&binding);
            return status;
        }
        plan->has_aggregate = true;
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_QUANTIFIED_COMPARISON) {
        return collect_aggregate_bindings(database, mylite_ast_child_at(expression, 0U), plan,
                                          callbacks);
    }

    for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
         child = child->next_sibling) {
        int status = collect_aggregate_bindings(database, child, plan, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int
bind_group_concat_aggregate(mylite_db *database, const struct mylite_sql_ast_node *expression,
                            struct mylite_select_plan *plan,
                            const struct mylite_select_aggregate_bind_callbacks *callbacks)
{
    struct mylite_select_aggregate_binding binding = {
        .argument = mylite_ast_child_at(expression, 1U),
        .kind = expression->aggregate_kind,
        .argument_kind = expression->aggregate_argument,
    };
    int status = MYLITE_OK;

    if (binding.argument_kind != MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION_LIST &&
        binding.argument_kind != MYLITE_SQL_AST_AGGREGATE_ARGUMENT_DISTINCT_EXPRESSION_LIST) {
        return callbacks->set_invalid_group_function_error(database);
    }

    status = bind_aggregate_argument_list(database, binding.argument, plan, callbacks);
    if (status == MYLITE_OK) {
        status = bind_group_concat_order_by(database, expression, plan, callbacks);
    }
    return status;
}

static int
bind_group_concat_order_by(mylite_db *database, const struct mylite_sql_ast_node *expression,
                           struct mylite_select_plan *plan,
                           const struct mylite_select_aggregate_bind_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *order_by =
        mylite_ast_find_child_kind(expression, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    const struct mylite_sql_ast_node *items =
        order_by == NULL ? NULL : mylite_ast_child_at(order_by, 0U);
    size_t argument_count = mylite_sql_ast_node_child_count(arguments);

    for (const struct mylite_sql_ast_node *item = items == NULL ? NULL : items->first_child;
         item != NULL; item = item->next_sibling) {
        int status = bind_group_concat_order_item(database, item, argument_count, plan, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int
bind_group_concat_order_item(mylite_db *database, const struct mylite_sql_ast_node *order_item,
                             size_t argument_count, struct mylite_select_plan *plan,
                             const struct mylite_select_aggregate_bind_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *expression = mylite_ast_child_at(order_item, 0U);

    if (expression == NULL) {
        return callbacks->set_invalid_group_function_error(database);
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        expression->literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        uint64_t ordinal = 0U;

        if (!mylite_select_parse_uint64_span(expression->span, &ordinal) || ordinal == 0U ||
            ordinal > argument_count) {
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
    return mylite_select_bind_predicate_expression_in_clause(
        database, expression, plan, "order clause", 0U, mylite_select_plan_table_count(plan),
        callbacks->predicate_callbacks);
}

static int
bind_aggregate_argument_list(mylite_db *database, const struct mylite_sql_ast_node *arguments,
                             struct mylite_select_plan *plan,
                             const struct mylite_select_aggregate_bind_callbacks *callbacks)
{
    if (!aggregate_argument_list_is_valid(arguments)) {
        return callbacks->set_invalid_group_function_error(database);
    }

    for (const struct mylite_sql_ast_node *argument = arguments->first_child; argument != NULL;
         argument = argument->next_sibling) {
        int status = mylite_select_bind_predicate_expression(database, argument, plan,
                                                             callbacks->predicate_callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static bool aggregate_argument_list_is_valid(const struct mylite_sql_ast_node *arguments)
{
    if (arguments == NULL || arguments->first_child == NULL) {
        return false;
    }
    return arguments->kind == MYLITE_SQL_AST_EXPRESSION_LIST ||
           arguments->kind == MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST;
}

static bool aggregate_binding_needs_distinct_argument_descriptors(
    const struct mylite_select_aggregate_binding *binding)
{
    return binding != NULL && binding->kind == MYLITE_SQL_AST_AGGREGATE_COUNT &&
           binding->argument_kind == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_DISTINCT_EXPRESSION_LIST;
}
