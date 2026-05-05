#include "mylite_select_aggregate_bind_registration.h"

#include "mylite_select.h"
#include "mylite_select_aggregate_count_distinct_bind.h"
#include "mylite_select_predicate_bind.h"
#include "mylite_span.h"

static int
collect_aggregate_bindings(mylite_db *database, const struct mylite_sql_ast_node *expression,
                           struct mylite_select_plan *plan,
                           const struct mylite_select_aggregate_bind_callbacks *callbacks);

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
    if (binding.argument_kind == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION) {
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
    if (status == MYLITE_OK &&
        binding.argument_kind == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_DISTINCT_EXPRESSION_LIST) {
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
            binding.argument_kind == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_DISTINCT_EXPRESSION_LIST) {
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
