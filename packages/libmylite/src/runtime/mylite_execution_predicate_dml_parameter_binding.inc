static int bind_select_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate *predicate,
    int *parameter_index
) {
    struct predicate_sql_work_item *items = NULL;
    size_t item_count = 0U;
    int rc = MYLITE_OK;

    if (!planned_select_predicate_has_expression(predicate)) {
        return MYLITE_OK;
    }

    rc = append_predicate_sql_work_node(&items, &item_count, predicate->root_index);
    while (rc == MYLITE_OK && item_count > 0U) {
        struct predicate_sql_work_item item = items[--item_count];
        const struct planned_select_predicate_node *node = NULL;

        if (item.node_index >= predicate->node_count) {
            rc = MYLITE_ERROR;
            continue;
        }

        node = &predicate->nodes[item.node_index];
        if (node->kind == PLANNED_SELECT_PREDICATE_AND ||
            node->kind == PLANNED_SELECT_PREDICATE_OR ||
            node->kind == PLANNED_SELECT_PREDICATE_XOR) {
            rc = append_predicate_sql_work_node(&items, &item_count, node->right_index);
            if (rc == MYLITE_OK) {
                rc = append_predicate_sql_work_node(&items, &item_count, node->left_index);
            }
            continue;
        }
        if (node->kind == PLANNED_SELECT_PREDICATE_NOT) {
            rc = append_predicate_sql_work_node(&items, &item_count, node->left_index);
            continue;
        }
        if (node->kind == PLANNED_SELECT_PREDICATE_EXISTS) {
            rc = bind_select_exists_predicate_parameters(
                statement,
                node->exists_subquery,
                parameter_index
            );
            continue;
        }
        rc = bind_select_predicate_node_parameters(statement, node, parameter_index);
    }

    free(items);
    return rc;
}

static int bind_select_exists_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_exists_subquery *subquery,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (subquery == NULL) {
        return MYLITE_ERROR;
    }
    if (subquery->limit.has_limit && subquery->limit.row_count == 0) {
        return MYLITE_OK;
    }

    rc = bind_select_predicate_parameters_without_exists(
        statement,
        &subquery->predicate,
        parameter_index
    );
    if (rc == MYLITE_OK && subquery->limit.has_limit) {
        rc = bind_int64_parameter(statement, *parameter_index, subquery->limit.row_count);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }

    return rc;
}

static int bind_select_predicate_parameters_without_exists(
    sqlite3_stmt *statement,
    const struct planned_select_predicate *predicate,
    int *parameter_index
) {
    struct predicate_sql_work_item *items = NULL;
    size_t item_count = 0U;
    int rc = MYLITE_OK;

    if (!planned_select_predicate_has_expression(predicate)) {
        return MYLITE_OK;
    }

    rc = append_predicate_sql_work_node(&items, &item_count, predicate->root_index);
    while (rc == MYLITE_OK && item_count > 0U) {
        struct predicate_sql_work_item item = items[--item_count];
        const struct planned_select_predicate_node *node = NULL;

        if (item.node_index >= predicate->node_count) {
            rc = MYLITE_ERROR;
            continue;
        }

        node = &predicate->nodes[item.node_index];
        if (node->kind == PLANNED_SELECT_PREDICATE_AND ||
            node->kind == PLANNED_SELECT_PREDICATE_OR ||
            node->kind == PLANNED_SELECT_PREDICATE_XOR) {
            rc = append_predicate_sql_work_node(&items, &item_count, node->right_index);
            if (rc == MYLITE_OK) {
                rc = append_predicate_sql_work_node(&items, &item_count, node->left_index);
            }
            continue;
        }
        if (node->kind == PLANNED_SELECT_PREDICATE_NOT) {
            rc = append_predicate_sql_work_node(&items, &item_count, node->left_index);
            continue;
        }
        if (node->kind == PLANNED_SELECT_PREDICATE_EXISTS) {
            rc = MYLITE_ERROR;
            continue;
        }
        rc = bind_select_predicate_node_parameters_without_subqueries(
            statement,
            node,
            parameter_index
        );
    }

    free(items);
    return rc;
}

static int bind_select_predicate_node_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
) {
    if (node->kind == PLANNED_SELECT_PREDICATE_SCALAR_COUNT_SUBQUERY_COMPARISON) {
        int rc = bind_select_exists_predicate_parameters(
            statement,
            node->exists_subquery,
            parameter_index
        );
        if (rc == MYLITE_OK) {
            rc = bind_planned_value_parameter(statement, *parameter_index, &node->value);
        }
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
        return rc;
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_SCALAR_SUBQUERY_IN) {
        return bind_select_scalar_subquery_in_predicate_parameters(
            statement,
            node,
            parameter_index
        );
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_SCALAR_AGGREGATE_SUBQUERY_COMPARISON) {
        return bind_select_scalar_aggregate_subquery_comparison_predicate_parameters(
            statement,
            node,
            parameter_index
        );
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_SCALAR_AGGREGATE_SUBQUERY_BETWEEN) {
        return bind_select_scalar_aggregate_subquery_between_predicate_parameters(
            statement,
            node,
            parameter_index
        );
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_IN) {
        return bind_select_row_scalar_in_predicate_parameters(statement, node, parameter_index);
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_IN) {
        return bind_select_in_predicate_parameters(statement, node, parameter_index);
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_QUANTIFIED_SUBQUERY) {
        return bind_select_quantified_subquery_predicate_parameters(
            statement,
            node,
            parameter_index
        );
    }

    return bind_select_non_in_predicate_node_parameters(statement, node, parameter_index);
}

static int bind_select_non_in_predicate_node_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH ||
        node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON ||
        node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL ||
        node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_BOOLEAN ||
        node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_BETWEEN) {
        bool row_scalar_decimal_like =
            node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON &&
            node->row_scalar_compare_as_real && comparison_operator_is_like(node->operator_kind);

        if (node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON &&
            comparison_operator_is_regexp(node->operator_kind)) {
            rc = bind_planned_value_parameter(statement, *parameter_index, &node->value);
            if (rc == MYLITE_OK) {
                ++(*parameter_index);
            }
            if (rc == MYLITE_OK) {
                rc = bind_row_scalar_expression_parameters(
                    statement,
                    node->row_scalar_expression,
                    parameter_index
                );
            }
            return rc;
        }
        rc = bind_row_scalar_expression_parameters(
            statement,
            node->row_scalar_expression,
            parameter_index
        );
        if (rc == MYLITE_OK && row_scalar_decimal_like) {
            rc = bind_row_scalar_expression_parameters(
                statement,
                node->row_scalar_expression,
                parameter_index
            );
        }
        if (rc == MYLITE_OK && node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON) {
            if (node->row_scalar_value_expression != NULL) {
                rc = bind_row_scalar_expression_parameters(
                    statement,
                    node->row_scalar_value_expression,
                    parameter_index
                );
            } else {
                rc = bind_planned_value_parameter(statement, *parameter_index, &node->value);
            }
            if (rc == MYLITE_OK && node->row_scalar_value_expression == NULL) {
                ++(*parameter_index);
            }
        }
        if (rc == MYLITE_OK && node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_BETWEEN) {
            if (node->row_scalar_value_expression != NULL) {
                rc = bind_row_scalar_expression_parameters(
                    statement,
                    node->row_scalar_value_expression,
                    parameter_index
                );
            } else {
                rc = bind_planned_value_parameter(statement, *parameter_index, &node->value);
            }
            if (rc == MYLITE_OK && node->row_scalar_value_expression == NULL) {
                ++(*parameter_index);
            }
            if (rc == MYLITE_OK) {
                if (node->row_scalar_upper_value_expression != NULL) {
                    rc = bind_row_scalar_expression_parameters(
                        statement,
                        node->row_scalar_upper_value_expression,
                        parameter_index
                    );
                } else {
                    rc = bind_planned_value_parameter(
                        statement,
                        *parameter_index,
                        &node->upper_value
                    );
                }
                if (rc == MYLITE_OK && node->row_scalar_upper_value_expression == NULL) {
                    ++(*parameter_index);
                }
            }
        }
        return rc;
    }
    if (node->kind != PLANNED_SELECT_PREDICATE_COMPARISON &&
        node->kind != PLANNED_SELECT_PREDICATE_BETWEEN) {
        return MYLITE_OK;
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_COMPARISON && node->value_is_column_reference) {
        return MYLITE_OK;
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_COMPARISON &&
        node->row_scalar_value_expression != NULL) {
        return bind_row_scalar_expression_parameters(
            statement,
            node->row_scalar_value_expression,
            parameter_index
        );
    }

    if (node->row_scalar_value_expression != NULL) {
        rc = bind_row_scalar_expression_parameters(
            statement,
            node->row_scalar_value_expression,
            parameter_index
        );
    } else {
        rc = bind_planned_value_parameter(statement, *parameter_index, &node->value);
    }
    if (rc == MYLITE_OK && node->row_scalar_value_expression == NULL) {
        ++(*parameter_index);
    }
    if (rc == MYLITE_OK && node->kind == PLANNED_SELECT_PREDICATE_BETWEEN) {
        if (node->row_scalar_upper_value_expression != NULL) {
            rc = bind_row_scalar_expression_parameters(
                statement,
                node->row_scalar_upper_value_expression,
                parameter_index
            );
        } else {
            rc = bind_planned_value_parameter(statement, *parameter_index, &node->upper_value);
        }
        if (rc == MYLITE_OK && node->row_scalar_upper_value_expression == NULL) {
            ++(*parameter_index);
        }
    }

    return rc;
}

static int bind_select_predicate_node_parameters_without_subqueries(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
) {
    if (node->kind == PLANNED_SELECT_PREDICATE_SCALAR_COUNT_SUBQUERY_COMPARISON) {
        return MYLITE_ERROR;
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_SCALAR_SUBQUERY_IN) {
        return MYLITE_ERROR;
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_SCALAR_AGGREGATE_SUBQUERY_COMPARISON ||
        node->kind == PLANNED_SELECT_PREDICATE_SCALAR_AGGREGATE_SUBQUERY_BETWEEN) {
        return MYLITE_ERROR;
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_IN) {
        return bind_select_row_scalar_in_predicate_parameters(statement, node, parameter_index);
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_IN) {
        return bind_select_in_literal_predicate_parameters(statement, node, parameter_index);
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_QUANTIFIED_SUBQUERY) {
        return MYLITE_ERROR;
    }

    return bind_select_non_in_predicate_node_parameters(statement, node, parameter_index);
}

static int bind_select_in_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
) {
    if (node->in_subquery != NULL) {
        return bind_select_in_subquery_predicate_parameters(
            statement,
            node->in_subquery,
            parameter_index
        );
    }

    return bind_select_in_literal_predicate_parameters(statement, node, parameter_index);
}

static int bind_select_in_literal_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
) {
    if (node->in_subquery != NULL) {
        return MYLITE_ERROR;
    }

    return bind_select_in_value_list_parameters(statement, node, parameter_index);
}

static int bind_select_row_scalar_in_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
) {
    int rc = bind_row_scalar_expression_parameters(
        statement,
        node->row_scalar_expression,
        parameter_index
    );

    if (rc == MYLITE_OK) {
        rc = bind_select_in_value_list_parameters(statement, node, parameter_index);
    }
    return rc;
}

static int bind_select_scalar_subquery_in_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (node == NULL || node->in_subquery == NULL) {
        return MYLITE_ERROR;
    }

    rc =
        bind_select_in_subquery_predicate_parameters(statement, node->in_subquery, parameter_index);
    if (rc == MYLITE_OK) {
        rc = bind_select_in_value_list_parameters(statement, node, parameter_index);
    }

    return rc;
}

static int bind_select_scalar_aggregate_subquery_comparison_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
) {
    int rc = bind_scalar_aggregate_subquery_parameters(
        statement,
        node->subject_scalar_subquery,
        parameter_index
    );

    if (rc == MYLITE_OK) {
        rc = bind_scalar_aggregate_subquery_or_value_parameters(
            statement,
            node->value_scalar_subquery,
            &node->value,
            parameter_index
        );
    }
    return rc;
}

static int bind_select_scalar_aggregate_subquery_between_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
) {
    int rc = bind_scalar_aggregate_subquery_parameters(
        statement,
        node->subject_scalar_subquery,
        parameter_index
    );

    if (rc == MYLITE_OK) {
        rc = bind_scalar_aggregate_subquery_or_value_parameters(
            statement,
            node->value_scalar_subquery,
            &node->value,
            parameter_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_scalar_aggregate_subquery_or_value_parameters(
            statement,
            node->upper_scalar_subquery,
            &node->upper_value,
            parameter_index
        );
    }
    return rc;
}

static int bind_scalar_aggregate_subquery_parameters(
    sqlite3_stmt *statement,
    const struct planned_scalar_aggregate_subquery *subquery,
    int *parameter_index
) {
    if (subquery == NULL) {
        return MYLITE_OK;
    }

    return bind_select_predicate_parameters_without_exists(
        statement,
        &subquery->subquery.predicate,
        parameter_index
    );
}

static int bind_scalar_aggregate_subquery_or_value_parameters(
    sqlite3_stmt *statement,
    const struct planned_scalar_aggregate_subquery *subquery,
    const struct planned_value *value,
    int *parameter_index
) {
    int rc = bind_scalar_aggregate_subquery_parameters(statement, subquery, parameter_index);

    if (rc == MYLITE_OK && subquery == NULL) {
        rc = bind_planned_value_parameter(statement, *parameter_index, value);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }
    return rc;
}

static int bind_select_in_value_list_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    for (size_t value_index = 0U; rc == MYLITE_OK && value_index < node->value_count;
         ++value_index) {
        if (node->row_scalar_value_expressions != NULL &&
            node->row_scalar_value_expressions[value_index].kind !=
                PLANNED_ROW_SCALAR_EXPRESSION_NONE) {
            rc = bind_row_scalar_expression_parameters(
                statement,
                &node->row_scalar_value_expressions[value_index],
                parameter_index
            );
        } else {
            rc = bind_planned_value_parameter(
                statement,
                *parameter_index,
                &node->values[value_index]
            );
        }
        if (rc == MYLITE_OK && (node->row_scalar_value_expressions == NULL ||
                                node->row_scalar_value_expressions[value_index].kind ==
                                    PLANNED_ROW_SCALAR_EXPRESSION_NONE)) {
            ++(*parameter_index);
        }
    }

    return rc;
}

static int bind_select_in_subquery_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_in_subquery *subquery,
    int *parameter_index
) {
    if (subquery == NULL) {
        return MYLITE_ERROR;
    }
    return bind_select_predicate_parameters_without_exists(
        statement,
        &subquery->predicate,
        parameter_index
    );
}

static int bind_select_quantified_subquery_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (node == NULL || node->in_subquery == NULL) {
        return MYLITE_ERROR;
    }

    rc =
        bind_select_in_subquery_predicate_parameters(statement, node->in_subquery, parameter_index);
    if (rc == MYLITE_OK) {
        rc = bind_select_in_subquery_predicate_parameters(
            statement,
            node->in_subquery,
            parameter_index
        );
    }
    return rc;
}

static int bind_count_parameters(sqlite3_stmt *statement, const struct planned_count *plan) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    if (plan->function == PLANNED_COUNT_LITERAL) {
        rc = bind_planned_value_parameter(statement, parameter_index, &plan->count_literal);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_count_derived_source_parameters(statement, plan, &parameter_index);
    }

    return rc;
}

static int bind_count_derived_source_parameters(
    sqlite3_stmt *statement,
    const struct planned_count *plan,
    int *parameter_index
) {
    switch (plan->source_kind) {
    case PLANNED_COUNT_SOURCE_DERIVED_SELECT:
        return bind_select_parameters_at(statement, &plan->derived_select, parameter_index);
    case PLANNED_COUNT_SOURCE_DERIVED_ROW_SCALAR:
        return bind_row_scalar_select_parameters_at(
            statement,
            &plan->derived_row_scalar_select,
            parameter_index
        );
    case PLANNED_COUNT_SOURCE_DERIVED_GROUPED_AGGREGATE:
        return bind_grouped_aggregate_parameters_at(
            statement,
            plan->derived_grouped_aggregate,
            true,
            parameter_index
        );
    case PLANNED_COUNT_SOURCE_DERIVED_COMPOUND:
        return bind_count_derived_compound_parameters(
            statement,
            &plan->derived_compound,
            parameter_index
        );
    case PLANNED_COUNT_SOURCE_NONE:
    case PLANNED_COUNT_SOURCE_TABLE:
        return MYLITE_OK;
    }
    return MYLITE_ERROR;
}

static int bind_count_derived_compound_parameters(
    sqlite3_stmt *statement,
    const struct planned_count_derived_compound *compound_plan,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (compound_plan == NULL || compound_plan->branches == NULL || parameter_index == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t branch_index = 0U; rc == MYLITE_OK && branch_index < compound_plan->branch_count;
         ++branch_index) {
        rc = bind_count_derived_compound_branch_parameters(
            statement,
            &compound_plan->branches[branch_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_count_derived_compound_branch_parameters(
    sqlite3_stmt *statement,
    const struct planned_count_derived_compound_branch *branch,
    int *parameter_index
) {
    if (branch == NULL || parameter_index == NULL) {
        return MYLITE_MISUSE;
    }
    switch (branch->kind) {
    case PLANNED_COUNT_SOURCE_DERIVED_SELECT:
        return bind_select_parameters_at(statement, &branch->select, parameter_index);
    case PLANNED_COUNT_SOURCE_DERIVED_ROW_SCALAR:
        return bind_row_scalar_select_parameters_at(
            statement,
            &branch->row_scalar_select,
            parameter_index
        );
    case PLANNED_COUNT_SOURCE_DERIVED_GROUPED_AGGREGATE:
        return bind_grouped_aggregate_parameters_at(
            statement,
            branch->grouped_aggregate,
            true,
            parameter_index
        );
    case PLANNED_COUNT_SOURCE_NONE:
    case PLANNED_COUNT_SOURCE_TABLE:
    case PLANNED_COUNT_SOURCE_DERIVED_COMPOUND:
        break;
    }
    return MYLITE_ERROR;
}

static int bind_column_aggregate_parameters(
    sqlite3_stmt *statement,
    const struct planned_column_aggregate_select *plan
) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < plan->item_count; ++item_index) {
        rc = bind_column_aggregate_item_parameters(
            statement,
            &plan->items[item_index],
            &parameter_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);
    }

    return rc;
}

static int bind_column_aggregate_item_parameters(
    sqlite3_stmt *statement,
    const struct planned_column_aggregate *item,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (item->function == PLANNED_COLUMN_AGGREGATE_COUNT_LITERAL) {
        rc = bind_planned_value_parameter(statement, *parameter_index, &item->count_literal);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }
    if (rc == MYLITE_OK && item->function == PLANNED_COLUMN_AGGREGATE_JSON_OBJECTAGG) {
        rc = bind_row_scalar_expression_parameters(
            statement,
            &item->aggregate_key_row_scalar_expression,
            parameter_index
        );
    }
    if (rc == MYLITE_OK && item->aggregate_uses_row_scalar_expression) {
        rc = bind_row_scalar_expression_parameters(
            statement,
            &item->aggregate_row_scalar_expression,
            parameter_index
        );
        if (rc == MYLITE_OK && item->function == PLANNED_COLUMN_AGGREGATE_AVG) {
            rc = bind_row_scalar_expression_parameters(
                statement,
                &item->aggregate_row_scalar_expression,
                parameter_index
            );
        }
    }
    if (rc == MYLITE_OK && item->has_separator) {
        rc = bind_planned_value_parameter(statement, *parameter_index, &item->separator);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }
    if (rc == MYLITE_OK && item->aggregate_has_projection_rhs) {
        rc = bind_row_scalar_expression_parameters(
            statement,
            &item->aggregate_projection_rhs,
            parameter_index
        );
    }

    return rc;
}

static int bind_grouped_aggregate_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan
) {
    return bind_grouped_aggregate_parameters_with_limit(statement, plan, true);
}

static int bind_grouped_aggregate_count_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan
) {
    return bind_grouped_aggregate_parameters_with_limit(statement, plan, false);
}

static int bind_grouped_aggregate_rollup_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan
) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    for (size_t aggregate_index = 0U; rc == MYLITE_OK && aggregate_index < plan->aggregate_count;
         ++aggregate_index) {
        const struct planned_grouped_aggregate_item *item = &plan->aggregates[aggregate_index];

        if (item->is_hidden_order_key) {
            continue;
        }
        rc = bind_grouped_aggregate_row_scalar_expression_parameters(
            statement,
            item,
            &parameter_index
        );
        if (rc == MYLITE_OK && item->has_separator) {
            rc = bind_planned_value_parameter(statement, parameter_index, &item->separator);
            if (rc == MYLITE_OK) {
                ++parameter_index;
            }
        }
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_source_parameters(
            statement,
            plan->sources,
            plan->source_count,
            &parameter_index
        );
    }
    for (size_t join_index = 0U; rc == MYLITE_OK && join_index < plan->join_count; ++join_index) {
        rc = bind_select_join_condition_parameters(
            statement,
            &plan->join_conditions[join_index],
            &parameter_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);
    }

    return rc;
}

static int bind_grouped_aggregate_parameters_with_limit(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    bool include_limit
) {
    int parameter_index = 1;

    return bind_grouped_aggregate_parameters_at(statement, plan, include_limit, &parameter_index);
}

static int bind_grouped_aggregate_parameters_at(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    bool include_limit,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    rc = bind_grouped_projection_parameters(statement, plan, parameter_index);
    for (size_t aggregate_index = 0U; rc == MYLITE_OK && aggregate_index < plan->aggregate_count;
         ++aggregate_index) {
        const struct planned_grouped_aggregate_item *item = &plan->aggregates[aggregate_index];

        if (item->is_hidden_order_key) {
            continue;
        }
        rc = bind_grouped_aggregate_row_scalar_expression_parameters(
            statement,
            item,
            parameter_index
        );
        if (rc == MYLITE_OK && item->has_separator) {
            rc = bind_planned_value_parameter(statement, *parameter_index, &item->separator);
            if (rc == MYLITE_OK) {
                ++(*parameter_index);
            }
        }
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_source_parameters(
            statement,
            plan->sources,
            plan->source_count,
            parameter_index
        );
    }
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
        rc = bind_grouped_key_parameters(statement, plan, parameter_index);
    }
    if (rc == MYLITE_OK && plan->having.kind != PLANNED_GROUPED_HAVING_NONE &&
        plan->having.operand == PLANNED_GROUPED_HAVING_OPERAND_AGGREGATE) {
        if (plan->having.aggregate_index >= plan->aggregate_count) {
            return MYLITE_ERROR;
        }
        rc = bind_grouped_having_aggregate_parameters(
            statement,
            &plan->aggregates[plan->having.aggregate_index],
            parameter_index
        );
    }
    if (rc == MYLITE_OK && plan->having.kind != PLANNED_GROUPED_HAVING_NONE &&
        plan->having.operand == PLANNED_GROUPED_HAVING_OPERAND_ROW_SCALAR) {
        rc = bind_row_scalar_expression_parameters(
            statement,
            plan->having.row_scalar_expression,
            parameter_index
        );
    }
    if (rc == MYLITE_OK && plan->having.kind == PLANNED_GROUPED_HAVING_COMPARISON) {
        if (plan->having.value_uses_uint64_order_key) {
            rc = bind_text_parameter(statement, *parameter_index, plan->having.uint64_order_key);
        } else {
            rc = bind_planned_value_parameter(statement, *parameter_index, &plan->having.value);
        }
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }
    for (size_t value_index = 0U; rc == MYLITE_OK &&
         plan->having.kind == PLANNED_GROUPED_HAVING_IN &&
         value_index < plan->having.value_count;
         ++value_index) {
        rc = bind_planned_value_parameter(
            statement,
            *parameter_index,
            &plan->having.values[value_index]
        );
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }
    if (rc == MYLITE_OK && include_limit) {
        rc = bind_grouped_order_parameters(statement, plan, parameter_index);
    }
    if (rc == MYLITE_OK && include_limit && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }
    if (rc == MYLITE_OK && include_limit && plan->limit.has_offset) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.offset);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }

    return rc;
}

static int bind_grouped_order_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (plan == NULL || !plan->order.has_order) {
        return MYLITE_OK;
    }
    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < plan->order.item_count;
         ++item_index) {
        const struct planned_select_order_item *item = &plan->order.items[item_index];

        if (plan->order_item_uses_aggregate[item_index]) {
            size_t aggregate_index = plan->order_item_aggregate_indexes[item_index];

            if (aggregate_index >= plan->aggregate_count) {
                return MYLITE_ERROR;
            }
            if (plan->aggregates[aggregate_index].function ==
                PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT) {
                continue;
            }
            rc = bind_grouped_aggregate_row_scalar_expression_parameters(
                statement,
                &plan->aggregates[aggregate_index],
                parameter_index
            );
        } else if (item->kind == PLANNED_SELECT_ORDER_ITEM_ROW_SCALAR) {
            rc =
                bind_row_scalar_expression_parameters(statement, item->expression, parameter_index);
        }
    }

    return rc;
}

static int bind_grouped_projection_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    for (size_t projection_index = 0U; rc == MYLITE_OK && projection_index < plan->projection_count;
         ++projection_index) {
        const struct planned_grouped_projection *projection = &plan->projections[projection_index];

        if (!projection->uses_row_scalar_expression) {
            continue;
        }
        rc = bind_row_scalar_expression_parameters(
            statement,
            &projection->row_scalar_expression,
            parameter_index
        );
    }
    return rc;
}

static int bind_grouped_having_aggregate_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate_item *item,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (item == NULL) {
        return MYLITE_OK;
    }
    if (item->function == PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT) {
        rc = bind_grouped_aggregate_row_scalar_expression_parameters(
            statement,
            item,
            parameter_index
        );
        if (rc == MYLITE_OK && item->has_separator) {
            rc = bind_planned_value_parameter(statement, *parameter_index, &item->separator);
            if (rc == MYLITE_OK) {
                ++(*parameter_index);
            }
        }
        return rc;
    }
    if (!item->aggregate_uses_row_scalar_expression) {
        return MYLITE_OK;
    }
    if (item->function == PLANNED_GROUPED_AGGREGATE_AVG) {
        return bind_row_scalar_expression_parameters(
            statement,
            &item->aggregate_row_scalar_expression,
            parameter_index
        );
    }
    return bind_grouped_aggregate_row_scalar_expression_parameters(statement, item, parameter_index);
}

static int bind_grouped_aggregate_row_scalar_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate_item *item,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (item == NULL || !item->aggregate_uses_row_scalar_expression) {
        return MYLITE_OK;
    }
    if (item->function == PLANNED_GROUPED_AGGREGATE_JSON_OBJECTAGG) {
        rc = bind_row_scalar_expression_parameters(
            statement,
            &item->aggregate_key_row_scalar_expression,
            parameter_index
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    rc = bind_row_scalar_expression_parameters(
        statement,
        &item->aggregate_row_scalar_expression,
        parameter_index
    );

    if (rc == MYLITE_OK && item->function == PLANNED_GROUPED_AGGREGATE_AVG) {
        rc = bind_row_scalar_expression_parameters(
            statement,
            &item->aggregate_row_scalar_expression,
            parameter_index
        );
    }
    return rc;
}

static int bind_grouped_key_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    for (size_t group_index = 0U; rc == MYLITE_OK && group_index < plan->group_count;
         ++group_index) {
        const struct planned_grouped_key *group = &plan->groups[group_index];

        if (!group->uses_row_scalar_expression) {
            continue;
        }
        rc = bind_row_scalar_expression_parameters(
            statement,
            &group->row_scalar_expression,
            parameter_index
        );
    }
    return rc;
}

static int bind_delete_parameters(sqlite3_stmt *statement, const struct planned_delete *plan) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    if (plan->is_joined) {
        for (size_t index = 0U; rc == MYLITE_OK && index < plan->target_source_count; ++index) {
            rc = bind_select_source_parameters(
                statement,
                plan->sources,
                plan->source_count,
                &parameter_index
            );
            if (rc == MYLITE_OK) {
                rc = bind_select_join_condition_parameters(
                    statement,
                    &plan->join_condition,
                    &parameter_index
                );
            }
            if (rc == MYLITE_OK) {
                rc =
                    bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);
            }
        }
    } else {
        rc = bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);
    }
    if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, parameter_index, plan->limit.row_count);
    }

    return rc;
}

static int bind_update_parameters(sqlite3_stmt *statement, const struct planned_update *plan) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    if (planned_update_has_multiple_assignments(plan)) {
        rc = bind_update_multiple_assignment_parameters(statement, &parameter_index, plan);
    } else if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL) {
        rc = bind_update_date_interval_parameters(statement, &parameter_index, plan);
    } else if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_COLUMN_COPY) {
        rc = MYLITE_OK;
    } else if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_ROW_SCALAR) {
        rc = bind_row_scalar_expression_parameters(
            statement,
            &plan->assignment_row_scalar_expression,
            &parameter_index
        );
    } else {
        rc = bind_update_assignment_parameter(statement, parameter_index, plan);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK) {
        rc = bind_update_auto_update_parameters(statement, &parameter_index, plan);
    }
    if (rc == MYLITE_OK && plan->is_joined) {
        rc = bind_joined_update_source_parameters(statement, &parameter_index, plan);
    }
    if (rc == MYLITE_OK && plan->is_joined) {
        rc = bind_select_join_condition_parameters(
            statement,
            &plan->join_condition,
            &parameter_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);
    }
    if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, parameter_index, plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK && planned_update_has_multiple_assignments(plan)) {
        rc = bind_update_multiple_changed_condition_parameters(statement, &parameter_index, plan);
    } else if (
        rc == MYLITE_OK && plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL
    ) {
        rc = bind_update_date_interval_parameters(statement, &parameter_index, plan);
    } else if (
        rc == MYLITE_OK && plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_COLUMN_COPY
    ) {
        rc = MYLITE_OK;
    } else if (
        rc == MYLITE_OK && plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_ROW_SCALAR
    ) {
        rc = bind_update_row_scalar_changed_condition_parameters(
            statement,
            &parameter_index,
            &plan->assignment_row_scalar_expression
        );
    } else if (
        rc == MYLITE_OK &&
        (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC ||
         !plan->assignment_value.is_null)
    ) {
        rc = bind_update_changed_condition_parameter(statement, parameter_index, plan);
    }

    return rc;
}

static int bind_update_multiple_assignment_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->assignment_count; ++index) {
        if (planned_update_assignment_is_noop(&plan->assignments[index])) {
            continue;
        }
        if (plan->assignments[index].value_kind == PLANNED_UPDATE_ASSIGNMENT_ROW_SCALAR) {
            rc = bind_row_scalar_expression_parameters(
                statement,
                &plan->assignments[index].row_scalar_expression,
                parameter_index
            );
        } else {
            rc = bind_planned_value_parameter(
                statement,
                *parameter_index,
                &plan->assignments[index].value
            );
            if (rc == MYLITE_OK) {
                ++(*parameter_index);
            }
        }
    }

    return rc;
}

static int bind_update_date_interval_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
) {
    const char *input_kind_name =
        mylite_date_interval_second_input_kind_name(plan->date_interval_input_kind);
    const char *unit_name = mylite_date_interval_unit_name(plan->date_interval_unit);
    int64_t operation_value = 0;
    int rc = bind_text_parameter(statement, *parameter_index, input_kind_name);

    if (plan->date_interval_subtract) {
        operation_value = 1;
    }

    if (rc == MYLITE_OK) {
        ++(*parameter_index);
        if (plan->date_interval_is_null) {
            rc = mylite_sqlite_status_to_mylite(sqlite3_bind_null(statement, *parameter_index));
        } else {
            rc = bind_int64_parameter(statement, *parameter_index, plan->date_interval_value);
        }
    }
    if (rc == MYLITE_OK) {
        ++(*parameter_index);
        rc = bind_text_parameter(statement, *parameter_index, unit_name);
    }
    if (rc == MYLITE_OK) {
        ++(*parameter_index);
        rc = bind_int64_parameter(statement, *parameter_index, operation_value);
    }
    if (rc == MYLITE_OK) {
        ++(*parameter_index);
    }
    return rc;
}

static int bind_update_auto_update_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (!planned_update_column_has_auto_update(plan, &plan->columns[column_index])) {
            continue;
        }
        rc = bind_planned_value_parameter(statement, *parameter_index, &plan->auto_update_value);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }

    return rc;
}

static int bind_joined_update_source_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
) {
    if (plan == NULL) {
        return MYLITE_ERROR;
    }
    return bind_select_source_parameters(
        statement,
        plan->sources,
        plan->source_count,
        parameter_index
    );
}

static int bind_update_multiple_changed_condition_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->assignment_count; ++index) {
        if (planned_update_assignment_is_noop(&plan->assignments[index])) {
            continue;
        }
        if (plan->assignments[index].value_kind == PLANNED_UPDATE_ASSIGNMENT_ROW_SCALAR) {
            rc = bind_update_row_scalar_changed_condition_parameters(
                statement,
                parameter_index,
                &plan->assignments[index].row_scalar_expression
            );
            continue;
        }
        if (plan->assignments[index].value.is_null) {
            continue;
        }
        rc = bind_planned_value_parameter(
            statement,
            *parameter_index,
            &plan->assignments[index].value
        );
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }

    return rc;
}

static int bind_update_row_scalar_changed_condition_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_row_scalar_expression *expression
) {
    int rc = MYLITE_OK;

    for (size_t occurrence = 0U; rc == MYLITE_OK && occurrence < 3U; ++occurrence) {
        rc = bind_row_scalar_expression_parameters(statement, expression, parameter_index);
    }

    return rc;
}

static int bind_update_assignment_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct planned_update *plan
) {
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC) {
        return bind_int64_parameter(statement, parameter_index, plan->arithmetic_delta);
    }
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_COLUMN_COPY) {
        return MYLITE_OK;
    }
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_ROW_SCALAR) {
        return MYLITE_OK;
    }

    return bind_planned_value_parameter(statement, parameter_index, &plan->assignment_value);
}

static int bind_update_changed_condition_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct planned_update *plan
) {
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC) {
        return bind_int64_parameter(statement, parameter_index, plan->arithmetic_delta);
    }
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_COLUMN_COPY) {
        return MYLITE_OK;
    }
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_ROW_SCALAR) {
        return MYLITE_OK;
    }

    return bind_planned_value_parameter(statement, parameter_index, &plan->assignment_value);
}

static int bind_update_matched_parameters(
    sqlite3_stmt *statement,
    const struct planned_update *plan
) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    if (plan->is_joined) {
        rc = bind_joined_update_source_parameters(statement, &parameter_index, plan);
    }
    if (rc == MYLITE_OK && plan->is_joined) {
        rc = bind_select_join_condition_parameters(
            statement,
            &plan->join_condition,
            &parameter_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);
    }
    return rc;
}

static int bind_update_matched_count_parameters(
    sqlite3_stmt *statement,
    const struct planned_update *plan
) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    if (plan->is_joined) {
        rc = bind_joined_update_source_parameters(statement, &parameter_index, plan);
    }
    if (rc == MYLITE_OK && plan->is_joined) {
        rc = bind_select_join_condition_parameters(
            statement,
            &plan->join_condition,
            &parameter_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);
    }

    if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, parameter_index, plan->limit.row_count);
    }

    return rc;
}

static int bind_planned_value_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct planned_value *value
) {
    int sqlite_rc = SQLITE_OK;

    if (parameter_index <= 0 || value == NULL) {
        return MYLITE_ERROR;
    }
    if (value->is_null) {
        sqlite_rc = sqlite3_bind_null(statement, parameter_index);
    } else if (value->is_text) {
        if (value->text_length > (size_t)INT_MAX) {
            return MYLITE_ERROR;
        }
        sqlite_rc = sqlite3_bind_text(
            statement,
            parameter_index,
            value->text,
            (int)value->text_length,
            SQLITE_TRANSIENT
        );
    } else if (value->is_blob) {
        if (value->text_length > (size_t)INT_MAX) {
            return MYLITE_ERROR;
        }
        sqlite_rc = sqlite3_bind_blob(
            statement,
            parameter_index,
            value->text,
            (int)value->text_length,
            SQLITE_TRANSIENT
        );
    } else if (value->is_real) {
        sqlite_rc = sqlite3_bind_double(statement, parameter_index, value->real);
    } else {
        sqlite_rc = sqlite3_bind_int64(statement, parameter_index, (sqlite3_int64)value->integer);
    }
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int bind_int64_parameter(sqlite3_stmt *statement, int parameter_index, int64_t value) {
    int sqlite_rc = SQLITE_OK;

    if (parameter_index <= 0) {
        return MYLITE_ERROR;
    }

    sqlite_rc = sqlite3_bind_int64(statement, parameter_index, (sqlite3_int64)value);
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int bind_text_parameter(sqlite3_stmt *statement, int parameter_index, const char *value) {
    int sqlite_rc = SQLITE_OK;

    if (parameter_index <= 0 || value == NULL) {
        return MYLITE_ERROR;
    }

    sqlite_rc = sqlite3_bind_text(statement, parameter_index, value, -1, SQLITE_TRANSIENT);
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}
