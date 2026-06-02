static int bind_select_parameters(sqlite3_stmt *statement, const struct planned_select *plan) {
    int parameter_index = 1;

    return bind_select_parameters_at(statement, plan, &parameter_index);
}

static int bind_select_parameters_at(
    sqlite3_stmt *statement,
    const struct planned_select *plan,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    rc = bind_select_predicate_parameters(statement, &plan->predicate, parameter_index);
    if (rc == MYLITE_OK) {
        rc = bind_select_order_parameters(statement, &plan->order, parameter_index);
    }
    if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }
    if (rc == MYLITE_OK && plan->limit.has_offset) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.offset);
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
        rc = bind_select_predicate_parameters(statement, &plan->predicate, parameter_index);
    }
    if (rc == MYLITE_OK && plan->has_source) {
        rc = bind_select_order_parameters(statement, &plan->order, parameter_index);
    }
    if (rc == MYLITE_OK && plan->has_source && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }
    if (rc == MYLITE_OK && plan->has_source && plan->limit.has_offset) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.offset);
    }
    if (rc == MYLITE_OK && !plan->has_source && plan->tableless_filter.has_filter &&
        plan->tableless_filter.has_scalar_predicate) {
        rc = bind_select_predicate_parameters_without_exists(
            statement,
            &plan->tableless_filter.predicate,
            parameter_index
        );
    }
    if (rc == MYLITE_OK && !plan->has_source && plan->tableless_filter.has_filter &&
        !plan->tableless_filter.has_scalar_predicate) {
        rc = bind_select_exists_predicate_parameters(
            statement,
            &plan->tableless_filter.subquery,
            parameter_index
        );
    }
    if (rc == MYLITE_OK && !plan->has_source && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }
    if (rc == MYLITE_OK && !plan->has_source && plan->limit.has_offset) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.offset);
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
            item->kind == PLANNED_SELECT_ORDER_ITEM_RAND) {
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
