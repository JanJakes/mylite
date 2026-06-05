static int build_select_found_rows_sql(const struct planned_select *plan, char **out_sql) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT COUNT(*) FROM ");
    if (rc == MYLITE_OK) {
        rc = append_select_from_sql(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);

    return rc;
}

static int build_count_sql(const struct planned_count *plan, char **out_sql) {
    struct mylite_dynamic_string string;
    size_t next_parameter = plan->function == PLANNED_COUNT_LITERAL ? 2U : 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT COUNT(");
    if (rc == MYLITE_OK && plan->function == PLANNED_COUNT_STAR) {
        rc = mylite_dynamic_string_append(&string, "*");
    }
    if (rc == MYLITE_OK && plan->function == PLANNED_COUNT_COLUMN) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->count_column.name);
    }
    if (rc == MYLITE_OK && plan->function == PLANNED_COUNT_DISTINCT_COLUMN) {
        rc = mylite_dynamic_string_append(&string, "DISTINCT ");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->count_column.name);
        }
    }
    if (rc == MYLITE_OK && plan->function == PLANNED_COUNT_LITERAL) {
        rc = append_numbered_parameter(&string, 1U);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);

    return rc;
}

static int build_column_aggregate_sql(const struct planned_column_aggregate *plan, char **out_sql) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_column_aggregate_select_list_sql(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);

    return rc;
}

static int append_column_aggregate_select_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_column_aggregate *plan,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (plan->function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
        return append_group_concat_call_sql(
            string,
            &plan->aggregate_column,
            0U,
            &plan->aggregate_order,
            false,
            plan->has_separator,
            next_parameter
        );
    }
    if (plan->aggregate_uses_row_scalar_expression) {
        rc = mylite_dynamic_string_append(string, column_aggregate_sql_function(plan->function));
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, '(');
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_expression_sql(
                string,
                &plan->aggregate_row_scalar_expression,
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ')');
        }
        return rc;
    }
    if (plan->function == PLANNED_COLUMN_AGGREGATE_AVG) {
        rc = mylite_dynamic_string_append(string, "SUM(");
        if (rc == MYLITE_OK) {
            rc =
                mylite_dynamic_string_append_quoted_identifier(string, plan->aggregate_column.name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, "), COUNT(");
        }
        if (rc == MYLITE_OK) {
            rc =
                mylite_dynamic_string_append_quoted_identifier(string, plan->aggregate_column.name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ')');
        }
        return rc;
    }

    rc = mylite_dynamic_string_append(string, column_aggregate_sql_function(plan->function));
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->aggregate_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_group_concat_call_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *value_column,
    size_t value_source_index,
    const struct planned_select_order *order,
    bool qualify_column_references,
    bool has_separator,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, "_mylite_group_concat(");

    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(
            string,
            value_column,
            value_source_index,
            qualify_column_references
        );
    }
    if (rc == MYLITE_OK && has_separator) {
        rc = mylite_dynamic_string_append(string, ", ");
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, *next_parameter);
        }
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
    }
    if (rc == MYLITE_OK && order != NULL && order->has_order) {
        rc = mylite_dynamic_string_append(string, " ORDER BY ");
        if (rc == MYLITE_OK) {
            rc = append_descriptor_value_sql_for_source(
                string,
                &order->column,
                order->column_source_index,
                order->qualify_column_reference
            );
        }
        if (rc == MYLITE_OK) {
            if (order->direction == PLANNED_SELECT_ORDER_DESC) {
                rc = mylite_dynamic_string_append(string, " DESC");
            } else {
                rc = mylite_dynamic_string_append(string, " ASC");
            }
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static const char *column_aggregate_sql_function(enum planned_column_aggregate_function function) {
    switch (function) {
    case PLANNED_COLUMN_AGGREGATE_NONE:
        return "";
    case PLANNED_COLUMN_AGGREGATE_MIN:
        return "MIN";
    case PLANNED_COLUMN_AGGREGATE_MAX:
        return "MAX";
    case PLANNED_COLUMN_AGGREGATE_SUM:
        return "SUM";
    case PLANNED_COLUMN_AGGREGATE_AVG:
        return "";
    case PLANNED_COLUMN_AGGREGATE_BIT_AND:
        return "_mylite_bit_and";
    case PLANNED_COLUMN_AGGREGATE_BIT_OR:
        return "_mylite_bit_or";
    case PLANNED_COLUMN_AGGREGATE_BIT_XOR:
        return "_mylite_bit_xor";
    case PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT:
        return "group_concat";
    }

    return "";
}

static int build_grouped_aggregate_sql(
    const struct planned_grouped_aggregate *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = append_grouped_aggregate_query_sql(&string, plan, &next_parameter, true, true);
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);

    return rc;
}

static int build_grouped_aggregate_found_rows_sql(
    const struct planned_grouped_aggregate *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT COUNT(*) FROM (");
    if (rc == MYLITE_OK) {
        rc = append_grouped_aggregate_query_sql(&string, plan, &next_parameter, false, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, "_mylite_grouped_count");
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);

    return rc;
}

static int append_grouped_aggregate_query_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t *next_parameter,
    bool include_order,
    bool include_limit
) {
    int rc = mylite_dynamic_string_append(string, "SELECT ");

    if (rc == MYLITE_OK) {
        rc = append_grouped_aggregate_select_list_sql(string, plan, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_grouped_aggregate_from_sql(string, plan, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(string, &plan->predicate, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " GROUP BY ");
    }
    for (size_t group_index = 0U; rc == MYLITE_OK && group_index < plan->group_count;
         ++group_index) {
        if (group_index > 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_grouped_group_key_sql(string, plan, group_index, next_parameter);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_grouped_having_sql(string, plan, next_parameter);
    }
    if (rc == MYLITE_OK && include_order) {
        rc = append_grouped_order_sql(string, plan, next_parameter);
    }
    if (rc == MYLITE_OK && include_limit) {
        rc = append_select_limit_sql(string, &plan->limit, next_parameter);
    }

    return rc;
}

static int append_grouped_group_key_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t group_index,
    size_t *next_parameter
) {
    const struct planned_grouped_key *group = NULL;

    if (group_index >= plan->group_count) {
        return MYLITE_ERROR;
    }
    group = &plan->groups[group_index];
    if (group->uses_row_scalar_expression) {
        if (next_parameter == NULL) {
            return MYLITE_ERROR;
        }
        return append_row_scalar_expression_sql(
            string,
            &group->row_scalar_expression,
            next_parameter
        );
    }
    return append_descriptor_value_sql_for_source(
        string,
        &group->column,
        group->column_source_index,
        plan->source_count > 0U
    );
}

static int append_grouped_aggregate_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t *next_parameter
) {
    struct planned_select source_plan = {0};

    if (plan->source_count == 0U) {
        int rc = mylite_dynamic_string_append_quoted_identifier(string, plan->table.physical_name);

        if (rc == MYLITE_OK && plan->predicate.qualify_column_references) {
            rc = mylite_dynamic_string_append(string, " AS ");
        }
        if (rc == MYLITE_OK && plan->predicate.qualify_column_references) {
            rc = append_select_source_alias(string, 0U);
        }
        return rc;
    }
    source_plan.sources = plan->sources;
    source_plan.source_count = plan->source_count;
    source_plan.join_kinds = plan->join_kinds;
    source_plan.join_conditions = plan->join_conditions;
    source_plan.join_count = plan->join_count;
    return append_select_from_sql(string, &source_plan, next_parameter);
}

static int append_grouped_aggregate_select_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t *next_parameter
) {
    bool qualify = plan->source_count > 0U;
    int rc = MYLITE_OK;

    for (size_t projection_index = 0U; rc == MYLITE_OK && projection_index < plan->projection_count;
         ++projection_index) {
        if (projection_index > 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_grouped_projection_sql(
                string,
                &plan->projections[projection_index],
                qualify,
                next_parameter
            );
        }
    }

    for (size_t aggregate_index = 0U; rc == MYLITE_OK && aggregate_index < plan->aggregate_count;
         ++aggregate_index) {
        if (aggregate_index > 0U || plan->projection_count > 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_grouped_aggregate_select_item_sql(
                string,
                plan,
                &plan->aggregates[aggregate_index],
                next_parameter
            );
        }
    }

    return rc;
}

static int append_grouped_projection_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_projection *projection,
    bool qualify,
    size_t *next_parameter
) {
    if (projection->uses_row_scalar_expression) {
        if (next_parameter == NULL) {
            return MYLITE_ERROR;
        }
        return append_row_scalar_expression_sql(
            string,
            &projection->row_scalar_expression,
            next_parameter
        );
    }
    return append_descriptor_column_name_sql_for_source(
        string,
        &projection->column,
        projection->column_source_index,
        qualify
    );
}

static int append_grouped_aggregate_select_item_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    const struct planned_grouped_aggregate_item *item,
    size_t *next_parameter
) {
    bool qualify = plan->source_count > 0U;
    int rc = MYLITE_OK;

    if (item->function == PLANNED_GROUPED_AGGREGATE_AVG) {
        rc = mylite_dynamic_string_append(string, "SUM(");
        if (rc == MYLITE_OK) {
            rc = append_descriptor_column_name_sql_for_source(
                string,
                &item->aggregate_column,
                item->aggregate_column_source_index,
                qualify
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, "), COUNT(");
        }
        if (rc == MYLITE_OK) {
            rc = append_descriptor_column_name_sql_for_source(
                string,
                &item->aggregate_column,
                item->aggregate_column_source_index,
                qualify
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ')');
        }
        return rc;
    }
    if (item->function == PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT) {
        return append_group_concat_call_sql(
            string,
            &item->aggregate_column,
            item->aggregate_column_source_index,
            &item->aggregate_order,
            qualify,
            item->has_separator,
            next_parameter
        );
    }

    return append_grouped_aggregate_expression_sql(string, plan, item, next_parameter);
}

static int append_grouped_aggregate_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    const struct planned_grouped_aggregate_item *item,
    size_t *next_parameter
) {
    bool qualify = plan->source_count > 0U;
    int rc = MYLITE_OK;

    if (item->function == PLANNED_GROUPED_AGGREGATE_COUNT_STAR) {
        return mylite_dynamic_string_append(string, "COUNT(*)");
    }
    if (item->function == PLANNED_GROUPED_AGGREGATE_ANY_VALUE) {
        return append_descriptor_column_name_sql_for_source(
            string,
            &item->aggregate_column,
            item->aggregate_column_source_index,
            qualify
        );
    }
    if (item->aggregate_uses_row_scalar_expression) {
        if (next_parameter == NULL) {
            return MYLITE_ERROR;
        }
        rc = mylite_dynamic_string_append(string, grouped_aggregate_sql_function(item->function));
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, '(');
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_expression_sql(
                string,
                &item->aggregate_row_scalar_expression,
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ')');
        }
        return rc;
    }
    if (item->function == PLANNED_GROUPED_AGGREGATE_AVG) {
        rc = mylite_dynamic_string_append(string, "AVG(");
    } else {
        rc = mylite_dynamic_string_append(string, grouped_aggregate_sql_function(item->function));
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, '(');
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(
            string,
            &item->aggregate_column,
            item->aggregate_column_source_index,
            qualify
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static const char *grouped_aggregate_sql_function(
    enum planned_grouped_aggregate_function function
) {
    switch (function) {
    case PLANNED_GROUPED_AGGREGATE_COUNT_COLUMN:
        return "COUNT";
    case PLANNED_GROUPED_AGGREGATE_MIN:
        return "MIN";
    case PLANNED_GROUPED_AGGREGATE_MAX:
        return "MAX";
    case PLANNED_GROUPED_AGGREGATE_SUM:
        return "SUM";
    case PLANNED_GROUPED_AGGREGATE_BIT_AND:
        return "_mylite_bit_and";
    case PLANNED_GROUPED_AGGREGATE_BIT_OR:
        return "_mylite_bit_or";
    case PLANNED_GROUPED_AGGREGATE_BIT_XOR:
        return "_mylite_bit_xor";
    case PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT:
        return "group_concat";
    case PLANNED_GROUPED_AGGREGATE_NONE:
    case PLANNED_GROUPED_AGGREGATE_COUNT_STAR:
    case PLANNED_GROUPED_AGGREGATE_AVG:
    case PLANNED_GROUPED_AGGREGATE_ANY_VALUE:
        return "";
    }

    return "";
}

static int append_grouped_having_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (plan->having.kind == PLANNED_GROUPED_HAVING_NONE) {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " HAVING ");
    if (rc == MYLITE_OK) {
        rc = append_grouped_having_operand_sql(string, plan, next_parameter);
    }
    if (plan->having.kind == PLANNED_GROUPED_HAVING_COMPARISON) {
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ' ');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(
                string,
                comparison_operator_sql(plan->having.operator_kind)
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ' ');
        }
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, *next_parameter);
        }
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
    } else if (plan->having.kind == PLANNED_GROUPED_HAVING_IS_NULL) {
        if (plan->having.operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL) {
            if (rc == MYLITE_OK) {
                rc = mylite_dynamic_string_append(string, " IS NOT NULL");
            }
        } else if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NULL");
        }
    }

    return rc;
}

static int append_grouped_having_operand_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t *next_parameter
) {
    if (plan->having.operand == PLANNED_GROUPED_HAVING_OPERAND_GROUP_COLUMN) {
        if (plan->having.group_index >= plan->group_count) {
            return MYLITE_ERROR;
        }
        return append_descriptor_value_sql_for_source(
            string,
            &plan->groups[plan->having.group_index].column,
            plan->groups[plan->having.group_index].column_source_index,
            plan->source_count > 0U
        );
    }
    if (plan->having.operand == PLANNED_GROUPED_HAVING_OPERAND_AGGREGATE) {
        return append_grouped_having_aggregate_sql(string, plan, next_parameter);
    }

    return MYLITE_OK;
}

static int append_grouped_having_aggregate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t *next_parameter
) {
    if (plan->having.aggregate_index >= plan->aggregate_count) {
        return MYLITE_ERROR;
    }
    return append_grouped_aggregate_expression_sql(
        string,
        plan,
        &plan->aggregates[plan->having.aggregate_index],
        next_parameter
    );
}

static int append_grouped_order_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (!plan->order.has_order) {
        return MYLITE_OK;
    }
    rc = mylite_dynamic_string_append(string, " ORDER BY ");
    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < plan->order.item_count;
         ++item_index) {
        const struct planned_select_order_item *item = &plan->order.items[item_index];

        if (item_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK && plan->order_item_uses_aggregate[item_index]) {
            size_t aggregate_index = plan->order_item_aggregate_indexes[item_index];

            if (aggregate_index >= plan->aggregate_count) {
                return MYLITE_ERROR;
            }
            rc = append_grouped_aggregate_expression_sql(
                string,
                plan,
                &plan->aggregates[aggregate_index],
                next_parameter
            );
        } else if (rc == MYLITE_OK && item->kind == PLANNED_SELECT_ORDER_ITEM_ROW_SCALAR) {
            if (item->expression == NULL) {
                return MYLITE_ERROR;
            }
            rc = append_row_scalar_expression_sql(string, item->expression, next_parameter);
        } else if (rc == MYLITE_OK && item->kind == PLANNED_SELECT_ORDER_ITEM_COLUMN) {
            rc = append_descriptor_value_sql_for_source(
                string,
                &item->column,
                item->column_source_index,
                plan->order.qualify_column_reference
            );
        } else if (rc == MYLITE_OK) {
            return MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = append_select_order_direction_sql(string, item->direction);
        }
    }

    return rc;
}

static int append_select_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (!planned_select_predicate_has_expression(predicate)) {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " WHERE ");
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_expression_sql(string, predicate, next_parameter);
    }

    return rc;
}

static int append_select_predicate_sql_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (!planned_select_predicate_has_expression(predicate)) {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " WHERE ");
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_expression_sql_without_exists(
            string,
            predicate,
            next_parameter
        );
    }

    return rc;
}

static int append_select_predicate_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
) {
    struct predicate_sql_work_item *items = NULL;
    size_t item_count = 0U;
    int rc = append_predicate_sql_work_node(&items, &item_count, predicate->root_index);

    while (rc == MYLITE_OK && item_count > 0U) {
        struct predicate_sql_work_item item = items[--item_count];
        rc = append_select_predicate_expression_work_item(
            string,
            predicate,
            item,
            &items,
            &item_count,
            next_parameter
        );
    }

    free(items);
    return rc;
}

static int append_select_predicate_expression_sql_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
) {
    struct predicate_sql_work_item *items = NULL;
    size_t item_count = 0U;
    int rc = append_predicate_sql_work_node(&items, &item_count, predicate->root_index);

    while (rc == MYLITE_OK && item_count > 0U) {
        struct predicate_sql_work_item item = items[--item_count];
        rc = append_select_predicate_expression_work_item_without_exists(
            string,
            predicate,
            item,
            &items,
            &item_count,
            next_parameter
        );
    }

    free(items);
    return rc;
}

static int append_select_predicate_expression_work_item(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    struct predicate_sql_work_item item,
    struct predicate_sql_work_item **items,
    size_t *item_count,
    size_t *next_parameter
) {
    switch (item.kind) {
    case PREDICATE_SQL_WORK_CLOSE:
        return mylite_dynamic_string_append_char(string, ')');
    case PREDICATE_SQL_WORK_OPERATOR:
        return append_select_predicate_logical_operator_sql(string, item.operator_kind);
    case PREDICATE_SQL_WORK_TEXT:
        return mylite_dynamic_string_append(string, item.text);
    case PREDICATE_SQL_WORK_NODE:
        return append_select_predicate_expression_node_sql(
            string,
            predicate,
            item.node_index,
            next_parameter,
            items,
            item_count
        );
    }

    return MYLITE_ERROR;
}

static int append_select_predicate_expression_work_item_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    struct predicate_sql_work_item item,
    struct predicate_sql_work_item **items,
    size_t *item_count,
    size_t *next_parameter
) {
    switch (item.kind) {
    case PREDICATE_SQL_WORK_CLOSE:
        return mylite_dynamic_string_append_char(string, ')');
    case PREDICATE_SQL_WORK_OPERATOR:
        return append_select_predicate_logical_operator_sql(string, item.operator_kind);
    case PREDICATE_SQL_WORK_TEXT:
        return mylite_dynamic_string_append(string, item.text);
    case PREDICATE_SQL_WORK_NODE:
        return append_select_predicate_expression_node_sql_without_exists(
            string,
            predicate,
            item.node_index,
            next_parameter,
            items,
            item_count
        );
    }

    return MYLITE_ERROR;
}

static int append_select_predicate_logical_operator_sql(
    struct mylite_dynamic_string *string,
    enum mylite_sql_ast_operator operator_kind
) {
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_OR ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR) {
        return mylite_dynamic_string_append(string, " OR ");
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_AND ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND) {
        return mylite_dynamic_string_append(string, " AND ");
    }
    return MYLITE_ERROR;
}

static int append_select_predicate_expression_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t node_index,
    size_t *next_parameter,
    struct predicate_sql_work_item **items,
    size_t *item_count
) {
    const struct planned_select_predicate_node *node = NULL;

    if (node_index >= predicate->node_count) {
        return MYLITE_ERROR;
    }

    node = &predicate->nodes[node_index];
    if (node->kind == PLANNED_SELECT_PREDICATE_AND || node->kind == PLANNED_SELECT_PREDICATE_OR ||
        node->kind == PLANNED_SELECT_PREDICATE_XOR) {
        return append_select_predicate_logical_node_sql(string, node, items, item_count);
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_NOT) {
        return append_select_predicate_not_node_sql(string, node, items, item_count);
    }

    return append_select_predicate_node_sql(string, predicate, node, next_parameter);
}

static int append_select_predicate_expression_node_sql_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t node_index,
    size_t *next_parameter,
    struct predicate_sql_work_item **items,
    size_t *item_count
) {
    const struct planned_select_predicate_node *node = NULL;

    if (node_index >= predicate->node_count) {
        return MYLITE_ERROR;
    }

    node = &predicate->nodes[node_index];
    if (node->kind == PLANNED_SELECT_PREDICATE_AND || node->kind == PLANNED_SELECT_PREDICATE_OR ||
        node->kind == PLANNED_SELECT_PREDICATE_XOR) {
        return append_select_predicate_logical_node_sql(string, node, items, item_count);
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_NOT) {
        return append_select_predicate_not_node_sql(string, node, items, item_count);
    }

    return append_select_predicate_node_sql_without_exists(string, predicate, node, next_parameter);
}

static int append_select_predicate_logical_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    struct predicate_sql_work_item **items,
    size_t *item_count
) {
    int rc = MYLITE_OK;

    if (node->kind == PLANNED_SELECT_PREDICATE_XOR) {
        rc = mylite_dynamic_string_append(string, "((NOT NOT ");
        if (rc == MYLITE_OK) {
            rc = append_predicate_sql_work_close(items, item_count);
        }
        if (rc == MYLITE_OK) {
            rc = append_predicate_sql_work_close(items, item_count);
        }
        if (rc == MYLITE_OK) {
            rc = append_predicate_sql_work_node(items, item_count, node->right_index);
        }
        if (rc == MYLITE_OK) {
            rc = append_predicate_sql_work_text(items, item_count, ") <> (NOT NOT ");
        }
        if (rc == MYLITE_OK) {
            rc = append_predicate_sql_work_node(items, item_count, node->left_index);
        }
        return rc;
    }

    rc = mylite_dynamic_string_append_char(string, '(');

    if (rc == MYLITE_OK) {
        rc = append_predicate_sql_work_close(items, item_count);
    }
    if (rc == MYLITE_OK) {
        rc = append_predicate_sql_work_node(items, item_count, node->right_index);
    }
    if (rc == MYLITE_OK) {
        rc = append_predicate_sql_work_operator(items, item_count, node->operator_kind);
    }
    if (rc == MYLITE_OK) {
        rc = append_predicate_sql_work_node(items, item_count, node->left_index);
    }

    return rc;
}

static int append_select_predicate_not_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    struct predicate_sql_work_item **items,
    size_t *item_count
) {
    int rc = mylite_dynamic_string_append(string, "(NOT ");

    if (rc == MYLITE_OK) {
        rc = append_predicate_sql_work_close(items, item_count);
    }
    if (rc == MYLITE_OK) {
        rc = append_predicate_sql_work_node(items, item_count, node->left_index);
    }

    return rc;
}

static int append_select_predicate_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append_char(string, '(');

    if (node->kind == PLANNED_SELECT_PREDICATE_EXISTS) {
        if (rc == MYLITE_OK) {
            rc = append_select_exists_predicate_sql(string, node->exists_subquery, next_parameter);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ')');
        }
        return rc;
    }

    if (rc == MYLITE_OK) {
        rc = append_select_predicate_non_exists_node_sql(string, predicate, node, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_select_predicate_node_sql_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append_char(string, '(');

    if (node->kind == PLANNED_SELECT_PREDICATE_EXISTS) {
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_non_subquery_node_sql(string, predicate, node, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_select_predicate_non_exists_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    if (node->kind == PLANNED_SELECT_PREDICATE_COMPARISON &&
        comparison_operator_is_regexp(node->operator_kind)) {
        return append_select_regexp_predicate_sql(
            string,
            predicate->qualify_column_references,
            node,
            next_parameter
        );
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH ||
        node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON ||
        node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL ||
        node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_BETWEEN) {
        return append_select_row_scalar_predicate_sql(string, node, next_parameter);
    }

    return append_select_predicate_column_term_sql(string, predicate, node, next_parameter);
}

static int append_select_predicate_non_subquery_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    if (node->kind == PLANNED_SELECT_PREDICATE_COMPARISON &&
        comparison_operator_is_regexp(node->operator_kind)) {
        return append_select_regexp_predicate_sql(
            string,
            predicate->qualify_column_references,
            node,
            next_parameter
        );
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH ||
        node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON ||
        node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL ||
        node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_BETWEEN) {
        return append_select_row_scalar_predicate_sql(string, node, next_parameter);
    }

    return append_select_predicate_column_term_sql_without_subqueries(
        string,
        predicate,
        node,
        next_parameter
    );
}

static int append_select_predicate_column_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    if (node->kind == PLANNED_SELECT_PREDICATE_COMPARISON &&
        node->operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE_BINARY) {
        return append_select_like_binary_predicate_sql(string, predicate, node, next_parameter);
    }

    int rc =
        append_select_predicate_subject_sql(string, node, predicate->qualify_column_references);

    if (rc == MYLITE_OK) {
        if (node->kind == PLANNED_SELECT_PREDICATE_COMPARISON) {
            rc = append_select_comparison_predicate_term_sql(
                string,
                predicate->qualify_column_references,
                node,
                next_parameter
            );
        } else if (node->kind == PLANNED_SELECT_PREDICATE_IS_NULL) {
            rc = append_select_is_null_predicate_term_sql(string, node);
        } else if (node->kind == PLANNED_SELECT_PREDICATE_IS_BOOLEAN) {
            rc = append_select_is_boolean_predicate_term_sql(
                string,
                predicate->qualify_column_references,
                node
            );
        } else if (node->kind == PLANNED_SELECT_PREDICATE_BETWEEN) {
            rc = append_select_between_predicate_term_sql(string, next_parameter);
        } else if (node->kind == PLANNED_SELECT_PREDICATE_IN) {
            rc = append_select_in_predicate_term_sql(string, node, next_parameter);
        } else {
            rc = MYLITE_ERROR;
        }
    }

    return rc;
}

static int append_select_predicate_column_term_sql_without_subqueries(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    if (node->kind == PLANNED_SELECT_PREDICATE_COMPARISON &&
        node->operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE_BINARY) {
        return append_select_like_binary_predicate_sql(string, predicate, node, next_parameter);
    }

    int rc =
        append_select_predicate_subject_sql(string, node, predicate->qualify_column_references);

    if (rc == MYLITE_OK) {
        if (node->kind == PLANNED_SELECT_PREDICATE_COMPARISON) {
            rc = append_select_comparison_predicate_term_sql(
                string,
                predicate->qualify_column_references,
                node,
                next_parameter
            );
        } else if (node->kind == PLANNED_SELECT_PREDICATE_IS_NULL) {
            rc = append_select_is_null_predicate_term_sql(string, node);
        } else if (node->kind == PLANNED_SELECT_PREDICATE_IS_BOOLEAN) {
            rc = append_select_is_boolean_predicate_term_sql(
                string,
                predicate->qualify_column_references,
                node
            );
        } else if (node->kind == PLANNED_SELECT_PREDICATE_BETWEEN) {
            rc = append_select_between_predicate_term_sql(string, next_parameter);
        } else if (node->kind == PLANNED_SELECT_PREDICATE_IN) {
            rc = append_select_in_literal_predicate_term_sql(string, node, next_parameter);
        } else {
            rc = MYLITE_ERROR;
        }
    }

    return rc;
}

static int append_select_row_scalar_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    int rc = append_row_scalar_expression_sql(string, node->row_scalar_expression, next_parameter);

    if (rc == MYLITE_OK && node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK && node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON) {
        if (row_scalar_expression_uses_string_collation(node->row_scalar_expression)) {
            rc = append_string_key_collation_sql(string);
        }
    }
    if (rc == MYLITE_OK && node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON) {
        rc = mylite_dynamic_string_append_char(string, ' ');
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, comparison_operator_sql(node->operator_kind));
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ' ');
        }
        if (rc == MYLITE_OK && node->row_scalar_value_expression != NULL) {
            rc = append_row_scalar_expression_sql(
                string,
                node->row_scalar_value_expression,
                next_parameter
            );
        } else if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, *next_parameter);
        }
        if (rc == MYLITE_OK && node->row_scalar_value_expression == NULL) {
            ++(*next_parameter);
        }
        return rc;
    }
    if (rc == MYLITE_OK && node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_BETWEEN) {
        return append_select_between_predicate_term_sql(string, next_parameter);
    }
    if (rc == MYLITE_OK && node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL) {
        if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NULL) {
            return mylite_dynamic_string_append(string, " IS NULL");
        }
        if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL) {
            return mylite_dynamic_string_append(string, " IS NOT NULL");
        }
    }

    return MYLITE_ERROR;
}

static int append_select_exists_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_exists_subquery *subquery,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (subquery == NULL) {
        return MYLITE_ERROR;
    }
    if (subquery->limit.has_limit && subquery->limit.row_count == 0) {
        return mylite_dynamic_string_append(string, "0");
    }

    rc = mylite_dynamic_string_append(string, "EXISTS (SELECT 1");
    if (rc == MYLITE_OK && subquery->has_table_source) {
        rc = mylite_dynamic_string_append(string, " FROM ");
    }
    if (rc == MYLITE_OK && subquery->has_table_source) {
        rc = append_exists_subquery_from_sql(string, subquery);
    }
    if (rc == MYLITE_OK && subquery->has_table_source) {
        rc = append_select_predicate_sql_without_exists(
            string,
            &subquery->predicate,
            next_parameter
        );
    }
    if (rc == MYLITE_OK && subquery->limit.has_limit) {
        rc = append_select_limit_sql(string, &subquery->limit, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_exists_subquery_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_exists_subquery *subquery
) {
    if (subquery->source_count > 0U) {
        return append_exists_subquery_join_from_sql(string, subquery, NULL);
    }

    int rc = mylite_dynamic_string_append_quoted_identifier(
        string,
        subquery->source.table.physical_name
    );

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_source_alias(string, subquery->inner_source_index);
    }

    return rc;
}

static int append_exists_subquery_join_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_exists_subquery *subquery,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (subquery == NULL || subquery->source_count == 0U ||
        subquery->join_count != subquery->source_count - 1U || subquery->join_kinds == NULL ||
        subquery->join_conditions == NULL) {
        return MYLITE_ERROR;
    }

    rc = append_select_source_sql(
        string,
        &subquery->sources[0],
        subquery->inner_source_index,
        next_parameter
    );
    for (size_t join_index = 0U; rc == MYLITE_OK && join_index < subquery->join_count;
         ++join_index) {
        size_t right_source_index = join_index + 1U;

        if (subquery->join_kinds[join_index] != MYLITE_SQL_AST_JOIN_KIND_INNER) {
            return MYLITE_ERROR;
        }
        rc = mylite_dynamic_string_append(string, " JOIN ");
        if (rc == MYLITE_OK) {
            rc = append_select_source_sql(
                string,
                &subquery->sources[right_source_index],
                subquery->inner_source_index + right_source_index,
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            rc = append_exists_subquery_join_condition_sql(
                string,
                subquery,
                join_index,
                next_parameter
            );
        }
    }
    return rc;
}

static int append_exists_subquery_join_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_exists_subquery *subquery,
    size_t join_index,
    size_t *next_parameter
) {
    struct planned_select_join_condition condition = {0};

    if (subquery == NULL || join_index >= subquery->join_count) {
        return MYLITE_ERROR;
    }
    condition = subquery->join_conditions[join_index];
    if (condition.right_value_is_row_scalar_expression) {
        return MYLITE_ERROR;
    }
    condition.left_source_index += subquery->inner_source_index;
    condition.right_source_index += subquery->inner_source_index;
    return append_select_join_condition_sql(string, &condition, next_parameter);
}

static int append_descriptor_value_sql_for_source(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify
) {
    if (column_descriptor_is_time(column)) {
        return append_time_seconds_value_sql_for_source(string, column, source_index, qualify);
    }
    if (column_descriptor_is_string_family(column)) {
        int rc =
            append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);

        if (rc == MYLITE_OK) {
            rc = append_string_key_collation_sql(string);
        }
        return rc;
    }

    return append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
}

static int append_select_predicate_subject_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    bool qualify
) {
    if (node->compare_string_as_integer) {
        return append_string_integer_value_sql_for_source(
            string,
            &node->column,
            node->column_source_index,
            qualify
        );
    }
    if (node->compare_date_as_datetime) {
        return append_date_midnight_value_sql_for_source(
            string,
            &node->column,
            node->column_source_index,
            qualify
        );
    }

    return append_descriptor_value_sql_for_source(
        string,
        &node->column,
        node->column_source_index,
        qualify
    );
}

static int append_string_integer_value_sql_for_source(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify
) {
    int rc = mylite_dynamic_string_append(string, "CAST(");

    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS INTEGER)");
    }

    return rc;
}

static int append_select_like_binary_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, "_mylite_like_binary(");

    if (rc == MYLITE_OK) {
        rc =
            append_select_predicate_subject_sql(string, node, predicate->qualify_column_references);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK && node->row_scalar_value_expression != NULL) {
        rc = append_row_scalar_expression_sql(
            string,
            node->row_scalar_value_expression,
            next_parameter
        );
    } else if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK && node->row_scalar_value_expression == NULL) {
        ++(*next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, node->like_uses_escape ? ", 1)" : ", 0)");
    }

    return rc;
}

static int append_date_midnight_value_sql_for_source(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify
) {
    int rc = mylite_dynamic_string_append_char(string, '(');

    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " || ' 00:00:00')");
    }

    return rc;
}

static int append_descriptor_column_name_sql_for_source(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify
) {
    int rc = MYLITE_OK;

    if (qualify) {
        rc = append_select_source_alias(string, source_index);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, '.');
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
    }

    return rc;
}

static int append_time_seconds_value_sql_for_source(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify
) {
    int rc = mylite_dynamic_string_append(string, "((CASE WHEN substr(");

    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(
            string,
            ", 1, 1) = '-' THEN -1 ELSE 1 END) * ((CAST(substr("
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", CASE WHEN substr(");
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(
            string,
            ", 1, 1) = '-' THEN 2 ELSE 1 END, CASE WHEN substr("
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", 1, 1) = '-' THEN length(");
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") - 7 ELSE length(");
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") - 6 END) AS INTEGER) * 3600) + (CAST(substr(");
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", length(");
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") - 4, 2) AS INTEGER) * 60) + CAST(substr(");
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", length(");
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") - 1, 2) AS INTEGER)))");
    }

    return rc;
}

static int append_select_comparison_predicate_term_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append_char(string, ' ');

    if (comparison_operator_is_like(node->operator_kind)) {
        if (rc == MYLITE_OK) {
            rc = append_select_like_predicate_term_sql(string, node, next_parameter);
        }
        return rc;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, comparison_operator_sql(node->operator_kind));
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ' ');
    }
    if (rc == MYLITE_OK) {
        if (node->value_is_column_reference) {
            rc = append_descriptor_value_sql_for_source(
                string,
                &node->value_column,
                node->value_column_source_index,
                qualify_column
            );
        } else if (node->row_scalar_value_expression != NULL) {
            rc = append_row_scalar_expression_sql(
                string,
                node->row_scalar_value_expression,
                next_parameter
            );
        } else {
            rc = append_numbered_parameter(string, *next_parameter);
        }
    }
    if (rc == MYLITE_OK && !node->value_is_column_reference &&
        node->row_scalar_value_expression == NULL) {
        ++(*next_parameter);
    }

    return rc;
}

static int append_select_like_predicate_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, "LIKE ");

    if (rc == MYLITE_OK && node->row_scalar_value_expression != NULL) {
        rc = append_row_scalar_expression_sql(
            string,
            node->row_scalar_value_expression,
            next_parameter
        );
    } else if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK && node->row_scalar_value_expression == NULL) {
        ++(*next_parameter);
    }
    if (rc == MYLITE_OK && node->like_uses_escape) {
        rc = mylite_dynamic_string_append(string, " ESCAPE '\\'");
    }

    return rc;
}

static int append_select_regexp_predicate_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, "_mylite_regexp_ci_ascii(");

    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_value_sql_for_source(
            string,
            &node->column,
            node->column_source_index,
            qualify_column
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_select_is_null_predicate_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node
) {
    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL) {
        return mylite_dynamic_string_append(string, " IS NOT NULL");
    }

    return mylite_dynamic_string_append(string, " IS NULL");
}

static int append_select_is_boolean_predicate_term_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node
) {
    switch (node->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
        return append_is_boolean_rhs_term_sql(string, qualify_column, node, " IS NOT NULL AND ");
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
        return append_is_boolean_rhs_term_sql(string, qualify_column, node, " IS NULL OR ");
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
        return append_is_boolean_rhs_term_sql(string, qualify_column, node, " IS NOT NULL AND ");
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
        return append_is_boolean_rhs_term_sql(string, qualify_column, node, " IS NULL OR ");
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
        return mylite_dynamic_string_append(string, " IS NULL");
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
        return mylite_dynamic_string_append(string, " IS NOT NULL");
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_LEFT_SHIFT:
    case MYLITE_SQL_AST_OPERATOR_RIGHT_SHIFT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_LIKE_BINARY:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_RLIKE:
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_CONCAT:
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
        break;
    }

    return MYLITE_ERROR;
}

static int append_is_boolean_rhs_term_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node,
    const char *suffix
) {
    int rc = mylite_dynamic_string_append(string, suffix);

    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(
            string,
            &node->column,
            node->column_source_index,
            qualify_column
        );
    }
    if (rc == MYLITE_OK && (node->operator_kind == MYLITE_SQL_AST_OPERATOR_IS_FALSE ||
                            node->operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE)) {
        rc = mylite_dynamic_string_append(string, " = 0");
    } else if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " <> 0");
    }

    return rc;
}

static int append_select_between_predicate_term_sql(
    struct mylite_dynamic_string *string,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, " BETWEEN ");

    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AND ");
    }
    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
    }

    return rc;
}

static int append_select_in_predicate_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (node->in_subquery != NULL) {
        rc = mylite_dynamic_string_append(string, " IN (");
        if (rc == MYLITE_OK) {
            rc = append_select_in_subquery_sql(string, node->in_subquery, next_parameter);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ')');
        }
        return rc;
    }

    return append_select_in_literal_predicate_term_sql(string, node, next_parameter);
}

static int append_select_in_literal_predicate_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (node->in_subquery != NULL) {
        return MYLITE_ERROR;
    }
    if (node->value_count == 0U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, " IN (");
    for (size_t value_index = 0U; rc == MYLITE_OK && value_index < node->value_count;
         ++value_index) {
        if (value_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, *next_parameter);
        }
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_select_in_subquery_sql(
    struct mylite_dynamic_string *string,
    const struct planned_in_subquery *subquery,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (subquery == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_descriptor_value_sql_for_source(
            string,
            &subquery->column,
            subquery->column_source_index,
            true
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_in_subquery_from_sql(string, subquery, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql_without_exists(
            string,
            &subquery->predicate,
            next_parameter
        );
    }

    return rc;
}

static int append_in_subquery_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_in_subquery *subquery,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (subquery->source_count > 0U) {
        if (subquery->join_count != subquery->source_count - 1U || subquery->join_kinds == NULL ||
            subquery->join_conditions == NULL) {
            return MYLITE_ERROR;
        }
        rc = append_select_source_sql(
            string,
            &subquery->sources[0],
            subquery->inner_source_index,
            next_parameter
        );
        for (size_t join_index = 0U; rc == MYLITE_OK && join_index < subquery->join_count;
             ++join_index) {
            size_t right_source_index = join_index + 1U;

            if (subquery->join_kinds[join_index] != MYLITE_SQL_AST_JOIN_KIND_INNER) {
                return MYLITE_ERROR;
            }
            rc = mylite_dynamic_string_append(string, " JOIN ");
            if (rc == MYLITE_OK) {
                rc = append_select_source_sql(
                    string,
                    &subquery->sources[right_source_index],
                    subquery->inner_source_index + right_source_index,
                    next_parameter
                );
            }
            if (rc == MYLITE_OK) {
                struct planned_select_join_condition condition =
                    subquery->join_conditions[join_index];

                condition.left_source_index += subquery->inner_source_index;
                condition.right_source_index += subquery->inner_source_index;
                rc = append_select_join_condition_sql(string, &condition, next_parameter);
            }
        }
        return rc;
    }

    rc = mylite_dynamic_string_append_quoted_identifier(
        string,
        subquery->source.table.physical_name
    );

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_source_alias(string, subquery->inner_source_index);
    }

    return rc;
}

static int append_predicate_sql_work_node(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    size_t node_index
) {
    return append_predicate_sql_work_item(
        items,
        item_count,
        (struct predicate_sql_work_item){
            .kind = PREDICATE_SQL_WORK_NODE,
            .node_index = node_index,
        }
    );
}

static int append_predicate_sql_work_operator(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    enum mylite_sql_ast_operator operator_kind
) {
    return append_predicate_sql_work_item(
        items,
        item_count,
        (struct predicate_sql_work_item){
            .kind = PREDICATE_SQL_WORK_OPERATOR,
            .operator_kind = operator_kind,
        }
    );
}

static int append_predicate_sql_work_close(
    struct predicate_sql_work_item **items,
    size_t *item_count
) {
    return append_predicate_sql_work_item(
        items,
        item_count,
        (struct predicate_sql_work_item){.kind = PREDICATE_SQL_WORK_CLOSE}
    );
}

static int append_predicate_sql_work_text(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    const char *text
) {
    return append_predicate_sql_work_item(
        items,
        item_count,
        (struct predicate_sql_work_item){
            .kind = PREDICATE_SQL_WORK_TEXT,
            .text = text,
        }
    );
}

static int append_predicate_sql_work_item(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    struct predicate_sql_work_item item
) {
    struct predicate_sql_work_item *grown_items = NULL;
    size_t required_count = *item_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*grown_items)) {
        return MYLITE_NOMEM;
    }

    grown_items = realloc(*items, required_count * sizeof(*grown_items));
    if (grown_items == NULL) {
        return MYLITE_NOMEM;
    }

    *items = grown_items;
    (*items)[*item_count] = item;
    *item_count = required_count;

    return MYLITE_OK;
}

static int append_select_order_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_order *order,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (!order->has_order) {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " ORDER BY ");
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (order->item_count == 0U) {
        return append_select_order_legacy_column_sql(string, order);
    }

    for (size_t item_index = 0U; item_index < order->item_count; ++item_index) {
        rc = append_select_order_item_sql(string, order, item_index, next_parameter);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int append_select_order_legacy_column_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_order *order
) {
    int rc = append_descriptor_value_sql_for_source(
        string,
        &order->column,
        order->column_source_index,
        order->qualify_column_reference
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    return append_select_order_direction_sql(string, order->direction);
}

static int append_select_order_item_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_order *order,
    size_t item_index,
    size_t *next_parameter
) {
    const struct planned_select_order_item *item = &order->items[item_index];
    int rc = MYLITE_OK;

    if (item_index != 0U) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (item->kind == PLANNED_SELECT_ORDER_ITEM_FIELD ||
        item->kind == PLANNED_SELECT_ORDER_ITEM_RAND ||
        item->kind == PLANNED_SELECT_ORDER_ITEM_ROW_SCALAR) {
        if (next_parameter == NULL || item->expression == NULL) {
            return MYLITE_ERROR;
        }
        rc = append_row_scalar_expression_sql(string, item->expression, next_parameter);
    } else {
        rc = append_descriptor_value_sql_for_source(
            string,
            &item->column,
            item->column_source_index,
            order->qualify_column_reference
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    return append_select_order_direction_sql(string, item->direction);
}

static int append_select_order_direction_sql(
    struct mylite_dynamic_string *string,
    enum planned_select_order_direction direction
) {
    if (direction == PLANNED_SELECT_ORDER_DESC) {
        return mylite_dynamic_string_append(string, " DESC");
    }

    return mylite_dynamic_string_append(string, " ASC");
}

static int append_select_limit_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_limit *limit,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (!limit->has_limit) {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " LIMIT ");
    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
    }
    if (rc == MYLITE_OK && limit->has_offset) {
        rc = mylite_dynamic_string_append(string, " OFFSET ");
    }
    if (rc == MYLITE_OK && limit->has_offset) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK && limit->has_offset) {
        ++(*next_parameter);
    }

    return rc;
}
