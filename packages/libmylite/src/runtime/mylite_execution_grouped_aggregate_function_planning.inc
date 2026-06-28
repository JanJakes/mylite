static int plan_grouped_aggregate_functions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    size_t first_aggregate_index,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    size_t select_item_count = mylite_sql_ast_node_child_count(select_list);
    size_t aggregate_count = select_item_count - first_aggregate_index;
    int rc = MYLITE_OK;

    if (aggregate_count > grouped_aggregate_max_results) {
        set_unsupported_error(database, "GROUP BY supports at most sixteen aggregate results");
        return MYLITE_ERROR;
    }
    if (aggregate_count > 0U) {
        out_plan->aggregates = calloc(aggregate_count, sizeof(*out_plan->aggregates));
        if (out_plan->aggregates == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
    }
    out_plan->aggregate_count = aggregate_count;

    for (size_t aggregate_index = 0U; aggregate_index < aggregate_count; ++aggregate_index) {
        rc = plan_grouped_aggregate_item(
            database,
            child_at(select_list, first_aggregate_index + aggregate_index),
            source_context,
            table_columns,
            table_column_count,
            out_plan,
            &out_plan->aggregates[aggregate_index]
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int plan_grouped_aggregate_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *aggregate_item,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    struct planned_grouped_aggregate_item *out_item
) {
    const struct mylite_sql_ast_node *aggregate_expression = child_at(aggregate_item, 0U);
    struct mylite_catalog_column_descriptor selected_column = {0};
    size_t selected_source_index = 0U;
    int rc = plan_grouped_aggregate_expression(
        database,
        aggregate_expression,
        source_context,
        table_columns,
        table_column_count,
        out_plan,
        child_at(aggregate_item, 1U),
        out_item
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_item->function == PLANNED_GROUPED_AGGREGATE_NONE) {
        const struct mylite_sql_ast_node *selected_column_node = NULL;

        rc = select_item_column_reference(aggregate_item, &selected_column_node);
        if (rc != MYLITE_OK) {
            set_unsupported_error(
                database,
                "GROUP BY supports selected descriptor group columns followed by aggregate results"
            );
            return MYLITE_ERROR;
        }
        rc = resolve_descriptor_column_reference_with_source_index(
            database,
            selected_column_node,
            source_context,
            COLUMN_REFERENCE_FIELD,
            "GROUP BY supports selected descriptor group columns followed by aggregate results",
            table_columns,
            table_column_count,
            &selected_column,
            &selected_source_index
        );
        if (rc == MYLITE_OK) {
            set_unsupported_error(
                database,
                "GROUP BY supports selected descriptor group columns followed by aggregate results"
            );
            return MYLITE_ERROR;
        }
        return rc;
    }
    return MYLITE_OK;
}

static int plan_grouped_aggregate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *aggregate_expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    const struct mylite_sql_ast_node *alias,
    struct planned_grouped_aggregate_item *out_item
) {
    const struct mylite_sql_ast_node *unwrapped_expression =
        unwrap_parenthesized_expression(aggregate_expression);
    const struct mylite_sql_ast_node *aggregate_function_expression = unwrapped_expression;
    const struct mylite_sql_ast_node *collect_expression = NULL;
    int rc = MYLITE_OK;

    *out_item = (struct planned_grouped_aggregate_item){0};
    out_item->expression = aggregate_expression;
    out_item->alias = alias;
    out_item->function = grouped_aggregate_function_from_expression(unwrapped_expression);
    if (out_item->function == PLANNED_GROUPED_AGGREGATE_NONE) {
        return MYLITE_OK;
    }
    if (spatial_collect_wrapper_from_expression(
            unwrapped_expression,
            &out_item->spatial_wrapper_kind,
            &collect_expression
        )) {
        aggregate_function_expression = collect_expression;
        out_item->has_spatial_wrapper = true;
    }
    out_item->aggregate_is_distinct =
        (out_item->function == PLANNED_GROUPED_AGGREGATE_COUNT_DISTINCT_COLUMN) ||
        aggregate_function_is_distinct(aggregate_function_expression);
    if (!grouped_aggregate_function_has_column(out_item->function)) {
        return MYLITE_OK;
    }
    if (out_item->function == PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT) {
        if (out_plan->aggregate_count != 1U) {
            set_unsupported_error(
                database,
                "GROUP_CONCAT(column) does not yet support multiple grouped aggregate results"
            );
            return MYLITE_ERROR;
        }
        if (out_plan->source_count > 0U) {
            set_unsupported_error(
                database,
                "GROUP_CONCAT(column) does not yet support joined GROUP BY sources"
            );
            return MYLITE_ERROR;
        }
        return plan_grouped_group_concat_options(
            database,
            aggregate_function_expression,
            source_context,
            table_columns,
            table_column_count,
            out_item
        );
    }
    if (out_item->function == PLANNED_GROUPED_AGGREGATE_COUNT_COLUMN ||
        out_item->function == PLANNED_GROUPED_AGGREGATE_COUNT_DISTINCT_COLUMN) {
        if (column_aggregate_argument_is_row_scalar_expression(unwrapped_expression)) {
            return plan_grouped_aggregate_row_scalar_expression(
                database,
                unwrapped_expression,
                source_context,
                table_columns,
                table_column_count,
                out_item
            );
        }
        rc = plan_grouped_count_column(
            database,
            aggregate_function_expression,
            source_context,
            table_columns,
            table_column_count,
            &out_item->aggregate_column,
            &out_item->aggregate_column_source_index
        );
        if (rc == MYLITE_OK &&
            out_item->function == PLANNED_GROUPED_AGGREGATE_COUNT_DISTINCT_COLUMN) {
            rc = validate_count_distinct_column_descriptor(database, &out_item->aggregate_column);
        }
        return rc;
    }
    if (out_item->function == PLANNED_GROUPED_AGGREGATE_ANY_VALUE) {
        return plan_grouped_any_value_column(
            database,
            aggregate_function_expression,
            source_context,
            table_columns,
            table_column_count,
            &out_item->aggregate_column,
            &out_item->aggregate_column_source_index
        );
    }
    if (out_item->function != PLANNED_GROUPED_AGGREGATE_ST_COLLECT &&
        column_aggregate_argument_is_row_scalar_expression(aggregate_function_expression)) {
        return plan_grouped_aggregate_row_scalar_expression(
            database,
            aggregate_function_expression,
            source_context,
            table_columns,
            table_column_count,
            out_item
        );
    }

    return plan_grouped_column_aggregate_column(
        database,
        aggregate_function_expression,
        grouped_column_aggregate_function(out_item->function),
        source_context,
        table_columns,
        table_column_count,
        &out_item->aggregate_column,
        &out_item->aggregate_column_source_index
    );
}

static int plan_grouped_aggregate_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate_item *out_item
) {
    const struct mylite_sql_ast_node *argument = child_at(function, 0U);
    int rc = MYLITE_OK;

    if (row_scalar_expression_contains_fractional_division_attempt(argument)) {
        set_unsupported_error(
            database,
            "aggregate row-scalar arguments do not support / division"
        );
        return MYLITE_ERROR;
    }
    rc = plan_row_scalar_expression(
        database,
        argument,
        true,
        source_context,
        NULL,
        table_columns,
        table_column_count,
        &out_item->aggregate_row_scalar_expression
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_item->aggregate_uses_row_scalar_expression = true;
    return MYLITE_OK;
}

static int plan_grouped_any_value_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
) {
    const struct mylite_sql_ast_node *column_node = child_at(function, 0U);

    return resolve_descriptor_column_reference_with_source_index(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "ANY_VALUE(column) supports only descriptor columns",
        table_columns,
        table_column_count,
        out_column,
        out_source_index
    );
}

static int plan_grouped_count_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
) {
    const struct mylite_sql_ast_node *column_node = child_at(function, 0U);

    return resolve_descriptor_column_reference_with_source_index(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "COUNT(column) supports only descriptor columns",
        table_columns,
        table_column_count,
        out_column,
        out_source_index
    );
}

static int plan_grouped_column_aggregate_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    enum planned_column_aggregate_function aggregate_function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
) {
    const struct mylite_sql_ast_node *column_node = child_at(function, 0U);
    int rc = resolve_descriptor_column_reference_with_source_index(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_FIELD,
        column_aggregate_column_error(aggregate_function),
        table_columns,
        table_column_count,
        out_column,
        out_source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    return validate_column_aggregate_descriptor_column(
        database,
        aggregate_function,
        out_column
    );
}

static int plan_grouped_group_concat_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate_item *out_item
) {
    int rc = MYLITE_OK;

    function = unwrap_parenthesized_expression(function);
    if (function == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    out_item->aggregate_is_distinct = group_concat_is_distinct(function);
    if (column_aggregate_argument_is_row_scalar_expression(function)) {
        rc = plan_grouped_aggregate_row_scalar_expression(
            database,
            function,
            source_context,
            table_columns,
            table_column_count,
            out_item
        );
    } else {
        rc = plan_group_concat_value_column(
            database,
            function,
            source_context,
            table_columns,
            table_column_count,
            &out_item->aggregate_column,
            &out_item->aggregate_column_source_index
        );
    }

    if (rc == MYLITE_OK) {
        rc = plan_group_concat_order_column(
            database,
            function,
            source_context,
            table_columns,
            table_column_count,
            false,
            &out_item->aggregate_order
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_group_concat_separator(
            database,
            function,
            &out_item->has_separator,
            &out_item->separator
        );
    }

    return rc;
}

static int plan_group_concat_value_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
) {
    const struct mylite_sql_ast_node *column_node = group_concat_value_node(function);
    struct integer_column_range range = {0};
    int rc = resolve_descriptor_column_reference_with_source_index(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "GROUP_CONCAT(column) supports only descriptor columns",
        table_columns,
        table_column_count,
        out_column,
        out_source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_string_family(out_column)) {
        return MYLITE_OK;
    }
    return integer_range_for_column(
        database,
        out_column,
        "GROUP_CONCAT(column) supports only integer and nonbinary string descriptor columns",
        &range
    );
}

static int plan_group_concat_order_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool qualify_column_reference,
    struct planned_select_order *out_order
) {
    const struct mylite_sql_ast_node *order_clause = group_concat_order_node(function);
    const struct mylite_sql_ast_node *direction = NULL;
    struct integer_column_range range = {0};
    int rc = MYLITE_OK;

    *out_order = (struct planned_select_order){
        .has_order = false,
        .direction = PLANNED_SELECT_ORDER_DEFAULT,
        .column_source_index = 0U,
        .qualify_column_reference = qualify_column_reference,
    };
    if (order_clause == NULL) {
        return MYLITE_OK;
    }
    if (order_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
        set_unsupported_error(
            database,
            "GROUP_CONCAT ORDER BY supports only one descriptor column"
        );
        return MYLITE_ERROR;
    }

    rc = resolve_descriptor_column_reference_with_source_index(
        database,
        child_at(order_clause, 0U),
        source_context,
        COLUMN_REFERENCE_ORDER,
        "GROUP_CONCAT ORDER BY supports only one descriptor column",
        table_columns,
        table_column_count,
        &out_order->column,
        &out_order->column_source_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_order->column.is_nullable) {
        set_unsupported_error(
            database,
            "GROUP_CONCAT ORDER BY supports only NOT NULL descriptor columns"
        );
        return MYLITE_ERROR;
    }
    if (!column_descriptor_is_bit(&out_order->column) &&
        !column_descriptor_is_year(&out_order->column) &&
        !column_descriptor_is_date(&out_order->column) &&
        !column_descriptor_is_time(&out_order->column) &&
        !column_descriptor_is_datetime(&out_order->column) &&
        !column_descriptor_is_timestamp(&out_order->column)) {
        rc = integer_range_for_column(
            database,
            &out_order->column,
            "GROUP_CONCAT ORDER BY supports only NOT NULL integer, BIT, YEAR, DATE, TIME, "
            "DATETIME, or TIMESTAMP descriptor columns",
            &range
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    out_order->has_order = true;
    out_order->direction = PLANNED_SELECT_ORDER_ASC;
    direction = child_at(order_clause, 1U);
    if (mylite_sql_ast_node_order_direction(direction) == MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
        out_order->direction = PLANNED_SELECT_ORDER_DESC;
    }
    return MYLITE_OK;
}

static int plan_group_concat_separator(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    bool *out_has_separator,
    struct planned_value *out_separator
) {
    const struct mylite_sql_ast_node *separator = group_concat_separator_node(function);
    char *text = NULL;
    size_t text_length = 0U;
    size_t character_count = 0U;
    int rc = MYLITE_OK;

    *out_has_separator = false;
    *out_separator = (struct planned_value){.is_null = false};
    if (separator == NULL) {
        return MYLITE_OK;
    }

    rc = decode_sql_string_literal(
        database,
        separator,
        "GROUP_CONCAT SEPARATOR supports only string literals",
        "GROUP_CONCAT SEPARATOR does not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && validate_utf8_text(text, text_length, &character_count) != MYLITE_OK) {
        set_unsupported_error(database, "GROUP_CONCAT SEPARATOR supports only valid UTF-8 text");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = assign_text_value(database, text, text_length, out_separator);
        text = NULL;
        *out_has_separator = true;
    }

    (void)character_count;
    free(text);
    return rc;
}

static const struct mylite_sql_ast_node *group_concat_value_node(
    const struct mylite_sql_ast_node *function
) {
    return child_at(function, 0U);
}

static const struct mylite_sql_ast_node *group_concat_order_node(
    const struct mylite_sql_ast_node *function
) {
    const struct mylite_sql_ast_node *child = child_at(function, 1U);

    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
            return child;
        }
        child = child->next_sibling;
    }
    return NULL;
}

static const struct mylite_sql_ast_node *group_concat_separator_node(
    const struct mylite_sql_ast_node *function
) {
    const struct mylite_sql_ast_node *child = child_at(function, 1U);

    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(child) == MYLITE_SQL_AST_LITERAL_STRING) {
            return child;
        }
        child = child->next_sibling;
    }
    return NULL;
}

static bool group_concat_is_distinct(const struct mylite_sql_ast_node *function) {
    const struct mylite_sql_ast_node *child = child_at(function, 1U);

    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_GROUP_CONCAT_DISTINCT_MODIFIER) {
            return true;
        }
        child = child->next_sibling;
    }
    return false;
}

static enum planned_grouped_aggregate_function grouped_aggregate_function_from_expression(
    const struct mylite_sql_ast_node *expression
) {
    enum planned_count_function count_function = count_function_from_expression(expression);
    enum planned_column_aggregate_function column_function =
        column_aggregate_function_from_expression(expression);

    if (spatial_collect_wrapper_from_expression(expression, NULL, NULL)) {
        return PLANNED_GROUPED_AGGREGATE_ST_COLLECT;
    }
    if (count_function == PLANNED_COUNT_STAR) {
        return PLANNED_GROUPED_AGGREGATE_COUNT_STAR;
    }
    if (count_function == PLANNED_COUNT_COLUMN) {
        return PLANNED_GROUPED_AGGREGATE_COUNT_COLUMN;
    }
    if (count_function == PLANNED_COUNT_DISTINCT_COLUMN) {
        return PLANNED_GROUPED_AGGREGATE_COUNT_DISTINCT_COLUMN;
    }
    if (count_function == PLANNED_COUNT_LITERAL) {
        return PLANNED_GROUPED_AGGREGATE_COUNT_COLUMN;
    }
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_COUNT_EXPRESSION_FUNCTION) {
        return PLANNED_GROUPED_AGGREGATE_COUNT_COLUMN;
    }

    switch (column_function) {
    case PLANNED_COLUMN_AGGREGATE_MIN:
        return PLANNED_GROUPED_AGGREGATE_MIN;
    case PLANNED_COLUMN_AGGREGATE_MAX:
        return PLANNED_GROUPED_AGGREGATE_MAX;
    case PLANNED_COLUMN_AGGREGATE_SUM:
        return PLANNED_GROUPED_AGGREGATE_SUM;
    case PLANNED_COLUMN_AGGREGATE_AVG:
        return PLANNED_GROUPED_AGGREGATE_AVG;
    case PLANNED_COLUMN_AGGREGATE_BIT_AND:
        return PLANNED_GROUPED_AGGREGATE_BIT_AND;
    case PLANNED_COLUMN_AGGREGATE_BIT_OR:
        return PLANNED_GROUPED_AGGREGATE_BIT_OR;
    case PLANNED_COLUMN_AGGREGATE_BIT_XOR:
        return PLANNED_GROUPED_AGGREGATE_BIT_XOR;
    case PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT:
        return PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT;
    case PLANNED_COLUMN_AGGREGATE_STDDEV_POP:
        return PLANNED_GROUPED_AGGREGATE_STDDEV_POP;
    case PLANNED_COLUMN_AGGREGATE_STDDEV_SAMP:
        return PLANNED_GROUPED_AGGREGATE_STDDEV_SAMP;
    case PLANNED_COLUMN_AGGREGATE_VAR_POP:
        return PLANNED_GROUPED_AGGREGATE_VAR_POP;
    case PLANNED_COLUMN_AGGREGATE_VAR_SAMP:
        return PLANNED_GROUPED_AGGREGATE_VAR_SAMP;
    case PLANNED_COLUMN_AGGREGATE_ST_COLLECT:
        return PLANNED_GROUPED_AGGREGATE_ST_COLLECT;
    case PLANNED_COLUMN_AGGREGATE_NONE:
    case PLANNED_COLUMN_AGGREGATE_COUNT_STAR:
    case PLANNED_COLUMN_AGGREGATE_COUNT_COLUMN:
    case PLANNED_COLUMN_AGGREGATE_COUNT_LITERAL:
    case PLANNED_COLUMN_AGGREGATE_COUNT_DISTINCT_COLUMN:
    case PLANNED_COLUMN_AGGREGATE_ANY_VALUE:
        break;
    }
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_ANY_VALUE_FUNCTION) {
        return PLANNED_GROUPED_AGGREGATE_ANY_VALUE;
    }

    return PLANNED_GROUPED_AGGREGATE_NONE;
}

static enum planned_column_aggregate_function grouped_column_aggregate_function(
    enum planned_grouped_aggregate_function function
) {
    switch (function) {
    case PLANNED_GROUPED_AGGREGATE_MIN:
        return PLANNED_COLUMN_AGGREGATE_MIN;
    case PLANNED_GROUPED_AGGREGATE_MAX:
        return PLANNED_COLUMN_AGGREGATE_MAX;
    case PLANNED_GROUPED_AGGREGATE_SUM:
        return PLANNED_COLUMN_AGGREGATE_SUM;
    case PLANNED_GROUPED_AGGREGATE_AVG:
        return PLANNED_COLUMN_AGGREGATE_AVG;
    case PLANNED_GROUPED_AGGREGATE_BIT_AND:
        return PLANNED_COLUMN_AGGREGATE_BIT_AND;
    case PLANNED_GROUPED_AGGREGATE_BIT_OR:
        return PLANNED_COLUMN_AGGREGATE_BIT_OR;
    case PLANNED_GROUPED_AGGREGATE_BIT_XOR:
        return PLANNED_COLUMN_AGGREGATE_BIT_XOR;
    case PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT:
        return PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT;
    case PLANNED_GROUPED_AGGREGATE_STDDEV_POP:
        return PLANNED_COLUMN_AGGREGATE_STDDEV_POP;
    case PLANNED_GROUPED_AGGREGATE_STDDEV_SAMP:
        return PLANNED_COLUMN_AGGREGATE_STDDEV_SAMP;
    case PLANNED_GROUPED_AGGREGATE_VAR_POP:
        return PLANNED_COLUMN_AGGREGATE_VAR_POP;
    case PLANNED_GROUPED_AGGREGATE_VAR_SAMP:
        return PLANNED_COLUMN_AGGREGATE_VAR_SAMP;
    case PLANNED_GROUPED_AGGREGATE_ST_COLLECT:
        return PLANNED_COLUMN_AGGREGATE_ST_COLLECT;
    case PLANNED_GROUPED_AGGREGATE_NONE:
    case PLANNED_GROUPED_AGGREGATE_COUNT_STAR:
    case PLANNED_GROUPED_AGGREGATE_COUNT_COLUMN:
    case PLANNED_GROUPED_AGGREGATE_COUNT_DISTINCT_COLUMN:
    case PLANNED_GROUPED_AGGREGATE_ANY_VALUE:
        return PLANNED_COLUMN_AGGREGATE_NONE;
    }

    return PLANNED_COLUMN_AGGREGATE_NONE;
}

static bool grouped_aggregate_function_has_column(
    enum planned_grouped_aggregate_function function
) {
    if (function == PLANNED_GROUPED_AGGREGATE_NONE) {
        return false;
    }
    if (function == PLANNED_GROUPED_AGGREGATE_COUNT_STAR) {
        return false;
    }

    return true;
}

static bool grouped_aggregate_function_is_bitwise(
    enum planned_grouped_aggregate_function function
) {
    if (function == PLANNED_GROUPED_AGGREGATE_BIT_AND) {
        return true;
    }
    if (function == PLANNED_GROUPED_AGGREGATE_BIT_OR) {
        return true;
    }
    if (function == PLANNED_GROUPED_AGGREGATE_BIT_XOR) {
        return true;
    }

    return false;
}

static bool grouped_aggregate_function_is_statistical(
    enum planned_grouped_aggregate_function function
) {
    if (function == PLANNED_GROUPED_AGGREGATE_STDDEV_POP) {
        return true;
    }
    if (function == PLANNED_GROUPED_AGGREGATE_STDDEV_SAMP) {
        return true;
    }
    if (function == PLANNED_GROUPED_AGGREGATE_VAR_POP) {
        return true;
    }
    if (function == PLANNED_GROUPED_AGGREGATE_VAR_SAMP) {
        return true;
    }

    return false;
}

static bool planned_grouped_aggregate_columns_match(
    const struct mylite_catalog_column_descriptor *left_column,
    size_t left_source_index,
    const struct mylite_catalog_column_descriptor *right_column,
    size_t right_source_index
) {
    return (left_column->column_id == right_column->column_id &&
            left_source_index == right_source_index) != 0;
}

static bool find_matching_grouped_key(
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    size_t *out_group_index
) {
    for (size_t group_index = 0U; group_index < plan->group_count; ++group_index) {
        if (planned_grouped_aggregate_columns_match(
                column,
                source_index,
                &plan->groups[group_index].column,
                plan->groups[group_index].column_source_index
            )) {
            *out_group_index = group_index;
            return true;
        }
    }

    return false;
}

static bool find_grouped_key_by_unqualified_name(
    const struct planned_grouped_aggregate *plan,
    const char *name,
    size_t *out_group_index,
    bool *out_ambiguous
) {
    bool matched = false;

    *out_ambiguous = false;
    for (size_t group_index = 0U; group_index < plan->group_count; ++group_index) {
        if (text_equals_ascii_case_insensitive(name, plan->groups[group_index].column.name)) {
            if (matched) {
                *out_ambiguous = true;
                return false;
            }
            *out_group_index = group_index;
            matched = true;
        }
    }

    return matched;
}

static bool find_grouped_projection_by_unqualified_name(
    const struct planned_grouped_aggregate *plan,
    const char *name,
    size_t *out_projection_index,
    bool *out_ambiguous
) {
    bool matched = false;

    *out_ambiguous = false;
    for (size_t projection_index = 0U; projection_index < plan->projection_count;
         ++projection_index) {
        if (text_equals_ascii_case_insensitive(
                name,
                plan->projections[projection_index].column.name
            )) {
            if (matched) {
                *out_ambiguous = true;
                return false;
            }
            *out_projection_index = projection_index;
            matched = true;
        }
    }

    return matched;
}

static const struct table_name_resolution *planned_grouped_aggregate_source_resolution(
    const struct planned_grouped_aggregate *plan,
    size_t source_index
) {
    if (plan->source_count == 0U) {
        return &plan->source;
    }
    if (source_index >= plan->source_count) {
        return &plan->sources[0].source;
    }
    return &plan->sources[source_index].source;
}
