#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_dml_update_delete_expression_bind.h"
#include "mylite_select.h"
#include "mylite_select_types.h"
#include "mylite_span.h"

#include <stdlib.h>

static const struct mylite_dml_mutation_expression_bind_diagnostics
    update_expression_bind_diagnostics = {
        .set_unknown_column_error = mylite_dml_set_update_unknown_column_error,
        .set_unsupported_clause_error = mylite_dml_set_update_unsupported_clause_error,
        .set_unsupported_expression_error = mylite_dml_set_update_unsupported_expression_error,
};

static int reject_deferred_update_clauses(
    mylite_db *database,
    const struct mylite_update_plan *plan
);

static int bind_update_assignment_values(
    mylite_db *database,
    const struct mylite_select_table *table,
    struct mylite_update_bound_assignment *assignments,
    size_t assignment_count
);

static int bind_update_assignment_expression(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_sql_ast_node *expression
);

static int bind_update_where_clause(
    mylite_db *database,
    const struct mylite_update_plan *plan,
    const struct mylite_select_table *table
);

static int bind_update_order_expression(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_sql_ast_node *expression
);

int mylite_dml_bind_update_subset(
    mylite_db *database,
    const struct mylite_update_plan *plan,
    const struct mylite_select_table *table,
    struct mylite_update_bound_assignment **out_assignments
) {
    struct mylite_update_bound_assignment *assignments = NULL;
    size_t assignment_count = 0U;
    int status = MYLITE_OK;

    if (database == NULL || plan == NULL || table == NULL || out_assignments == NULL) {
        return MYLITE_MISUSE;
    }

    status = reject_deferred_update_clauses(database, plan);
    *out_assignments = NULL;
    if (status != MYLITE_OK) {
        return status;
    }

    assignment_count = plan->assignment_count;
    if (assignment_count == 0U) {
        return mylite_dml_set_update_unsupported_assignment_error(database);
    }

    assignments = calloc(assignment_count, sizeof(*assignments));
    if (assignments == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_dml_bind_update_assignment_targets(
        database,
        plan,
        table,
        assignments,
        assignment_count
    );
    if (status == MYLITE_OK) {
        status = bind_update_assignment_values(database, table, assignments, assignment_count);
    }
    if (status == MYLITE_OK) {
        status = bind_update_where_clause(database, plan, table);
    }
    if (status != MYLITE_OK) {
        free(assignments);
        return status;
    }

    *out_assignments = assignments;
    return MYLITE_OK;
}

int mylite_dml_bind_update_order_by_clause(
    mylite_db *database,
    const struct mylite_update_plan *plan,
    const struct mylite_select_table *table,
    struct mylite_update_order_plan *order_plan
) {
    const struct mylite_sql_ast_node *items = NULL;

    if (database == NULL || plan == NULL || table == NULL || order_plan == NULL) {
        return MYLITE_MISUSE;
    }

    if (plan->order_by_clause == NULL) {
        return MYLITE_OK;
    }
    items = mylite_ast_child_at(plan->order_by_clause, 0U);
    if (plan->order_by_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE || items == NULL ||
        items->kind != MYLITE_SQL_AST_ORDER_ITEM_LIST) {
        return mylite_dml_set_update_unsupported_clause_error(database);
    }

    for (const struct mylite_sql_ast_node *item = items->first_child; item != NULL;
         item = item->next_sibling) {
        const struct mylite_sql_ast_node *expression = mylite_ast_child_at(item, 0U);
        struct mylite_select_order_key order_key = {
            .kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION,
            .direction = MYLITE_SQL_AST_KEY_PART_ORDER_ASC,
            .expression = expression,
        };
        int status = MYLITE_OK;

        if (item->kind != MYLITE_SQL_AST_ORDER_ITEM || expression == NULL) {
            return mylite_dml_set_update_unsupported_clause_error(database);
        }
        if (item->key_part_order == MYLITE_SQL_AST_KEY_PART_ORDER_DESC) {
            order_key.direction = MYLITE_SQL_AST_KEY_PART_ORDER_DESC;
        }
        status = bind_update_order_expression(database, table, expression);
        if (status == MYLITE_OK) {
            status = mylite_dml_add_update_order_key(order_plan, &order_key);
            if (status == MYLITE_NOMEM) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
            }
        }
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return order_plan->order_key_count == 0U
               ? mylite_dml_set_update_unsupported_clause_error(database)
               : MYLITE_OK;
}

static int reject_deferred_update_clauses(
    mylite_db *database,
    const struct mylite_update_plan *plan
) {
    const struct mylite_sql_ast_node *limit = plan->limit_clause;

    if (limit == NULL) {
        return MYLITE_OK;
    }
    if (limit->kind != MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE ||
        mylite_ast_child_at(limit, 0U) == NULL ||
        mylite_ast_child_at(limit, 0U)->kind != MYLITE_SQL_AST_LIMIT_BOUND ||
        !mylite_ast_child_at(limit, 0U)->has_limit_bound_value) {
        return mylite_dml_set_update_unsupported_clause_error(database);
    }
    return MYLITE_OK;
}

static int bind_update_assignment_values(
    mylite_db *database,
    const struct mylite_select_table *table,
    struct mylite_update_bound_assignment *assignments,
    size_t assignment_count
) {
    for (size_t index = 0U; index < assignment_count; ++index) {
        int status = bind_update_assignment_expression(database, table, assignments[index].value);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int bind_update_assignment_expression(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_sql_ast_node *expression
) {
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_DEFAULT) {
        return MYLITE_OK;
    }
    return mylite_dml_bind_mutation_expression(
        database,
        table,
        expression,
        "field list",
        &update_expression_bind_diagnostics
    );
}

static int bind_update_where_clause(
    mylite_db *database,
    const struct mylite_update_plan *plan,
    const struct mylite_select_table *table
) {
    const struct mylite_sql_ast_node *predicate = mylite_ast_child_at(plan->where_clause, 0U);

    if (plan->where_clause == NULL) {
        return MYLITE_OK;
    }
    if (plan->where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE || predicate == NULL) {
        return mylite_dml_set_update_unsupported_clause_error(database);
    }
    return mylite_dml_bind_mutation_expression(
        database,
        table,
        predicate,
        "where clause",
        &update_expression_bind_diagnostics
    );
}

static int bind_update_order_expression(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_sql_ast_node *expression
) {
    return mylite_dml_bind_mutation_expression(
        database,
        table,
        expression,
        "order clause",
        &update_expression_bind_diagnostics
    );
}
