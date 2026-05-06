#include "mylite_select_predicate_bind.h"

#include "mylite_select.h"
#include "mylite_select_predicate_expression_bind.h"
#include "mylite_span.h"

static bool predicate_bind_callbacks_are_valid(
    const struct mylite_select_predicate_bind_callbacks *callbacks
);

int mylite_select_bind_where_clause(
    mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct mylite_select_plan *plan,
    const struct mylite_select_predicate_bind_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *predicate = mylite_ast_child_at(where_clause, 0U);

    if (!predicate_bind_callbacks_are_valid(callbacks)) {
        return MYLITE_MISUSE;
    }
    if (where_clause == NULL || where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE ||
        predicate == NULL) {
        return callbacks->set_unsupported_where_error(database);
    }
    return mylite_select_bind_predicate_expression(database, predicate, plan, callbacks);
}

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_select_bind_join_predicates(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_select_predicate_bind_callbacks *callbacks
) {
    if (!predicate_bind_callbacks_are_valid(callbacks)) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; index < plan->join_predicate_count; ++index) {
        const struct mylite_select_join_predicate *predicate = &plan->join_predicates[index];
        int status = mylite_select_bind_predicate_expression_in_clause(
            database,
            predicate->expression,
            plan,
            "on clause",
            predicate->first_table,
            predicate->table_count,
            callbacks
        );

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_select_bind_predicate_expression(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_select_plan *plan,
    const struct mylite_select_predicate_bind_callbacks *callbacks
) {
    if (!predicate_bind_callbacks_are_valid(callbacks)) {
        return MYLITE_MISUSE;
    }
    return mylite_select_bind_predicate_expression_in_clause(
        database,
        expression,
        plan,
        "where clause",
        0U,
        mylite_select_plan_table_count(plan),
        callbacks
    );
}

static bool predicate_bind_callbacks_are_valid(
    const struct mylite_select_predicate_bind_callbacks *callbacks
) {
    if (callbacks == NULL || callbacks->subquery_callbacks == NULL ||
        callbacks->set_invalid_group_function_error == NULL ||
        callbacks->set_unsupported_where_error == NULL) {
        return false;
    }
    return true;
}
