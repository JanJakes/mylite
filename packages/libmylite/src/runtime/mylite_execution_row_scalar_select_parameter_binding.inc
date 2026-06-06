static int bind_select_parameters(sqlite3_stmt *statement, const struct planned_select *plan) {
    int parameter_index = 1;

    return bind_select_parameters_at(statement, plan, &parameter_index);
}

static int bind_compound_select_parameters_at(
    sqlite3_stmt *statement,
    const struct planned_compound_select *plan,
    int *parameter_index
);

static int bind_select_parameters_at(
    sqlite3_stmt *statement,
    const struct planned_select *plan,
    int *parameter_index
) {
    struct select_parameter_bind_stack stack = {0};
    int rc = select_parameter_bind_stack_push(
        &stack,
        (struct select_parameter_bind_item){
            .kind = SELECT_PARAMETER_BIND_PLAN,
            .plan = plan,
        }
    );

    if (rc == MYLITE_OK) {
        rc = bind_select_parameter_work_stack(statement, &stack, parameter_index);
    }
    select_parameter_bind_stack_deinit(&stack);
    return rc;
}

static int bind_select_parameter_work_stack(
    sqlite3_stmt *statement,
    struct select_parameter_bind_stack *stack,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && stack->count != 0U) {
        struct select_parameter_bind_item item = stack->items[--stack->count];

        rc = bind_select_parameter_work_item(statement, stack, &item, parameter_index);
    }
    return rc;
}

static int bind_select_parameter_work_item(
    sqlite3_stmt *statement,
    struct select_parameter_bind_stack *stack,
    const struct select_parameter_bind_item *item,
    int *parameter_index
) {
    if (item == NULL) {
        return MYLITE_ERROR;
    }

    switch (item->kind) {
    case SELECT_PARAMETER_BIND_PLAN:
        return push_select_plan_parameter_work(stack, item->plan);
    case SELECT_PARAMETER_BIND_GROUPED_AGGREGATE:
        return bind_grouped_aggregate_parameters_at(
            statement,
            item->grouped_plan,
            true,
            parameter_index
        );
    case SELECT_PARAMETER_BIND_COMPOUND_SELECT:
        return bind_compound_select_parameters_at(statement, item->compound_plan, parameter_index);
    case SELECT_PARAMETER_BIND_SOURCE_LIST:
        return push_select_source_list_parameter_work(stack, item->sources, item->source_count);
    case SELECT_PARAMETER_BIND_SOURCE:
        if (item->source == NULL || !item->source->is_derived) {
            return MYLITE_OK;
        }
        if (item->source->derived_select == NULL &&
            item->source->derived_grouped_aggregate == NULL &&
            item->source->derived_compound_select == NULL) {
            return MYLITE_ERROR;
        }
        return select_parameter_bind_stack_push(
            stack,
            (struct select_parameter_bind_item){
                .kind = item->source->derived_compound_select != NULL
                            ? SELECT_PARAMETER_BIND_COMPOUND_SELECT
                            : (item->source->derived_select == NULL
                                   ? SELECT_PARAMETER_BIND_GROUPED_AGGREGATE
                                   : SELECT_PARAMETER_BIND_PLAN),
                .plan = item->source->derived_select,
                .grouped_plan = item->source->derived_grouped_aggregate,
                .compound_plan = item->source->derived_compound_select,
            }
        );
    case SELECT_PARAMETER_BIND_JOIN_CONDITION:
        return bind_select_join_condition_parameters(
            statement,
            item->join_condition,
            parameter_index
        );
    case SELECT_PARAMETER_BIND_PREDICATE:
        return bind_select_predicate_parameters(statement, item->predicate, parameter_index);
    case SELECT_PARAMETER_BIND_ORDER:
        return bind_select_order_parameters(statement, item->order, parameter_index);
    case SELECT_PARAMETER_BIND_LIMIT:
        return bind_select_limit_parameters(statement, item->limit, parameter_index);
    }

    return MYLITE_ERROR;
}

static int push_select_plan_parameter_work(
    struct select_parameter_bind_stack *stack,
    const struct planned_select *plan
) {
    int rc = MYLITE_OK;

    if (plan == NULL) {
        return MYLITE_ERROR;
    }
    rc = select_parameter_bind_stack_push(
        stack,
        (struct select_parameter_bind_item){
            .kind = SELECT_PARAMETER_BIND_LIMIT,
            .limit = &plan->limit,
        }
    );
    if (rc == MYLITE_OK) {
        rc = select_parameter_bind_stack_push(
            stack,
            (struct select_parameter_bind_item){
                .kind = SELECT_PARAMETER_BIND_ORDER,
                .order = &plan->order,
            }
        );
    }
    if (rc == MYLITE_OK) {
        rc = select_parameter_bind_stack_push(
            stack,
            (struct select_parameter_bind_item){
                .kind = SELECT_PARAMETER_BIND_PREDICATE,
                .predicate = &plan->predicate,
            }
        );
    }
    for (size_t remaining = plan->join_count; rc == MYLITE_OK && remaining != 0U; --remaining) {
        size_t join_index = remaining - 1U;

        rc = select_parameter_bind_stack_push(
            stack,
            (struct select_parameter_bind_item){
                .kind = SELECT_PARAMETER_BIND_JOIN_CONDITION,
                .join_condition = &plan->join_conditions[join_index],
            }
        );
    }
    if (rc == MYLITE_OK) {
        rc = select_parameter_bind_stack_push(
            stack,
            (struct select_parameter_bind_item){
                .kind = SELECT_PARAMETER_BIND_SOURCE_LIST,
                .sources = plan->sources,
                .source_count = plan->source_count,
            }
        );
    }
    return rc;
}

static int bind_compound_select_parameters_at(
    sqlite3_stmt *statement,
    const struct planned_compound_select *plan,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (plan == NULL || plan->branches == NULL || parameter_index == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t branch_index = 0U; rc == MYLITE_OK && branch_index < plan->branch_count;
         ++branch_index) {
        rc = bind_select_parameters_at(statement, &plan->branches[branch_index], parameter_index);
    }
    return rc;
}

static int push_select_source_list_parameter_work(
    struct select_parameter_bind_stack *stack,
    const struct planned_select_source *sources,
    size_t source_count
) {
    if (source_count != 0U && sources == NULL) {
        return MYLITE_ERROR;
    }
    for (size_t remaining = source_count; remaining != 0U; --remaining) {
        int rc = select_parameter_bind_stack_push(
            stack,
            (struct select_parameter_bind_item){
                .kind = SELECT_PARAMETER_BIND_SOURCE,
                .source = &sources[remaining - 1U],
            }
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int bind_select_limit_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_limit *limit,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (limit == NULL || parameter_index == NULL) {
        return MYLITE_MISUSE;
    }
    if (limit->has_limit) {
        rc = bind_int64_parameter(statement, *parameter_index, limit->row_count);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }
    if (rc == MYLITE_OK && limit->has_offset) {
        rc = bind_int64_parameter(statement, *parameter_index, limit->offset);
    }
    return rc;
}

static int select_parameter_bind_stack_push(
    struct select_parameter_bind_stack *stack,
    struct select_parameter_bind_item item
) {
    struct select_parameter_bind_item *items = NULL;
    size_t capacity = 0U;

    if (stack == NULL) {
        return MYLITE_ERROR;
    }
    if (stack->count == stack->capacity) {
        capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;
        if (capacity < stack->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            return MYLITE_NOMEM;
        }
        items = realloc(stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            return MYLITE_NOMEM;
        }
        stack->items = items;
        stack->capacity = capacity;
    }

    stack->items[stack->count] = item;
    ++stack->count;
    return MYLITE_OK;
}

static void select_parameter_bind_stack_deinit(struct select_parameter_bind_stack *stack) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    *stack = (struct select_parameter_bind_stack){0};
}

static int bind_select_source_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_source *sources,
    size_t source_count,
    int *parameter_index
) {
    struct select_parameter_bind_stack stack = {0};
    int rc = select_parameter_bind_stack_push(
        &stack,
        (struct select_parameter_bind_item){
            .kind = SELECT_PARAMETER_BIND_SOURCE_LIST,
            .sources = sources,
            .source_count = source_count,
        }
    );

    if (rc == MYLITE_OK) {
        rc = bind_select_parameter_work_stack(statement, &stack, parameter_index);
    }
    select_parameter_bind_stack_deinit(&stack);
    return rc;
}

static int bind_select_join_condition_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_join_condition *condition,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (condition == NULL || !condition->has_condition) {
        return MYLITE_OK;
    }
    if (condition->right_value_is_row_scalar_expression) {
        rc = bind_row_scalar_expression_parameters(
            statement,
            condition->right_row_scalar_expression,
            parameter_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(
            statement,
            &condition->extra_predicate,
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_select_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_select *plan
) {
    int parameter_index = 1;

    return bind_row_scalar_select_parameters_at(statement, plan, &parameter_index);
}

static int bind_row_scalar_select_parameters_at(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_select *plan,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < plan->item_count; ++item_index) {
        rc = bind_row_scalar_expression_parameters(
            statement,
            &plan->items[item_index].expression,
            parameter_index
        );
    }
    if (rc == MYLITE_OK && plan->has_source) {
        rc = bind_row_scalar_select_source_parameters(statement, plan, parameter_index);
    }
    if (rc == MYLITE_OK && !plan->has_source) {
        rc = bind_row_scalar_tableless_filter_parameters(statement, plan, parameter_index);
    }

    return rc;
}

static int bind_row_scalar_select_source_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_select *plan,
    int *parameter_index
) {
    int rc = bind_select_source_parameters(
        statement,
        plan->sources,
        plan->source_count,
        parameter_index
    );

    for (size_t join_index = 0U; rc == MYLITE_OK && join_index < plan->join_count; ++join_index) {
        rc = bind_select_join_condition_parameters(
            statement,
            &plan->join_conditions[join_index],
            parameter_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(statement, &plan->predicate, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_order_parameters(statement, &plan->order, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_limit_parameters(statement, &plan->limit, parameter_index);
    }
    return rc;
}

static int bind_row_scalar_tableless_filter_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_select *plan,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (plan->tableless_filter.has_filter && plan->tableless_filter.has_scalar_predicate) {
        rc = bind_select_predicate_parameters_without_exists(
            statement,
            &plan->tableless_filter.predicate,
            parameter_index
        );
    }
    if (rc == MYLITE_OK && plan->tableless_filter.has_filter &&
        !plan->tableless_filter.has_scalar_predicate) {
        rc = bind_select_exists_predicate_parameters(
            statement,
            &plan->tableless_filter.subquery,
            parameter_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_limit_parameters(statement, &plan->limit, parameter_index);
    }
    return rc;
}

static int bind_select_order_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_order *order,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (order == NULL || !order->has_order) {
        return MYLITE_OK;
    }
    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < order->item_count; ++item_index) {
        const struct planned_select_order_item *item = &order->items[item_index];

        if (item->kind == PLANNED_SELECT_ORDER_ITEM_FIELD ||
            item->kind == PLANNED_SELECT_ORDER_ITEM_RAND ||
            item->kind == PLANNED_SELECT_ORDER_ITEM_ROW_SCALAR) {
            rc =
                bind_row_scalar_expression_parameters(statement, item->expression, parameter_index);
        }
    }

    return rc;
}

static int bind_insert_select_materialize_parameters(
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan
) {
    if (plan->source_kind == PLANNED_INSERT_SELECT_SOURCE_COMPOUND) {
        return bind_insert_select_compound_parameters(statement, plan);
    }

    return bind_select_parameters(statement, &plan->source);
}

static int bind_insert_select_compound_parameters(
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan
) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    for (size_t branch_index = 0U;
         rc == MYLITE_OK && branch_index < plan->compound_source.branch_count;
         ++branch_index) {
        const struct planned_insert_select_compound_branch *branch =
            &plan->compound_source.branches[branch_index];

        if (branch->kind == PLANNED_INSERT_SELECT_COMPOUND_BRANCH_ROW_SCALAR) {
            rc = bind_row_scalar_select_parameters_at(
                statement,
                &branch->row_source,
                &parameter_index
            );
        } else {
            rc = bind_select_parameters_at(statement, &branch->source, &parameter_index);
        }
    }

    return rc;
}
