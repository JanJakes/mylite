static int plan_date_format_numeric_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "DATE_FORMAT() numeric predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node.row_scalar_expression = calloc(1U, sizeof(*node.row_scalar_expression));
    if (node.row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_date_format_numeric_equal_expression(
        database,
        predicate_node,
        true,
        source_context,
        table_columns,
        table_column_count,
        node.row_scalar_expression
    );
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_temporal_extract_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_temporal_extract_predicate_expression(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_temporal_extract_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_temporal_extract_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = plan_temporal_extract_predicate_value(
            database,
            child_at(predicate_node, 1U),
            &node.value
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_temporal_extract_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_temporal_extract_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_temporal_extract_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    enum mylite_temporal_extract_kind extract_kind = MYLITE_TEMPORAL_EXTRACT_DATE;
    const struct mylite_sql_ast_node *argument = NULL;
    int mode = 0;
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "temporal extract function predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_execution_resolve_temporal_extract_call(
        database,
        expression,
        &extract_kind,
        &argument,
        &mode
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument == NULL) {
        return MYLITE_MISUSE;
    }
    (void)mode;
    if (!temporal_extract_predicate_kind_is_numeric(extract_kind)) {
        set_unsupported_error(
            database,
            "temporal extract predicates support only numeric temporal extractor functions"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_temporal_extract_expression(
        database,
        expression,
        true,
        source_context,
        table_columns,
        table_column_count,
        node->row_scalar_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_temporal_extract_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(expression);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int64_t value = 0;
    bool is_null = false;

    if (literal != NULL && literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(literal);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        }
        literal = unwrap_parenthesized_expression(child_at(literal, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "temporal extract predicates support only integer, boolean, and NULL comparison "
            "literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        is_null = true;
    } else if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        value = 1;
    } else if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        value = 0;
    } else if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "temporal extract predicates support only integer, boolean, and NULL comparison "
            "literals"
        );
        return MYLITE_ERROR;
    } else if (
        parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        (is_negative && magnitude > (uint64_t)INT64_MAX + 1U) ||
        (!is_negative && magnitude > (uint64_t)INT64_MAX)
    ) {
        set_unsupported_error(
            database,
            "temporal extract predicate comparison literals must fit the signed 64-bit range"
        );
        return MYLITE_ERROR;
    } else if (is_negative && magnitude == (uint64_t)INT64_MAX + 1U) {
        value = INT64_MIN;
    } else if (is_negative) {
        value = -(int64_t)magnitude;
    } else {
        value = (int64_t)magnitude;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = is_null,
        .is_text = false,
        .integer = value,
    };
    return MYLITE_OK;
}

static bool temporal_extract_predicate_kind_is_numeric(enum mylite_temporal_extract_kind kind) {
    switch (kind) {
    case MYLITE_TEMPORAL_EXTRACT_YEAR:
    case MYLITE_TEMPORAL_EXTRACT_QUARTER:
    case MYLITE_TEMPORAL_EXTRACT_MONTH:
    case MYLITE_TEMPORAL_EXTRACT_DAY:
    case MYLITE_TEMPORAL_EXTRACT_YEAR_MONTH:
    case MYLITE_TEMPORAL_EXTRACT_DAYOFWEEK:
    case MYLITE_TEMPORAL_EXTRACT_DAYOFYEAR:
    case MYLITE_TEMPORAL_EXTRACT_WEEK:
    case MYLITE_TEMPORAL_EXTRACT_WEEKDAY:
    case MYLITE_TEMPORAL_EXTRACT_WEEKOFYEAR:
    case MYLITE_TEMPORAL_EXTRACT_YEARWEEK:
    case MYLITE_TEMPORAL_EXTRACT_HOUR:
    case MYLITE_TEMPORAL_EXTRACT_SIGNED_HOUR:
    case MYLITE_TEMPORAL_EXTRACT_MINUTE:
    case MYLITE_TEMPORAL_EXTRACT_SIGNED_MINUTE:
    case MYLITE_TEMPORAL_EXTRACT_SECOND:
    case MYLITE_TEMPORAL_EXTRACT_SIGNED_SECOND:
    case MYLITE_TEMPORAL_EXTRACT_MICROSECOND:
    case MYLITE_TEMPORAL_EXTRACT_SIGNED_MICROSECOND:
    case MYLITE_TEMPORAL_EXTRACT_DAY_HOUR:
    case MYLITE_TEMPORAL_EXTRACT_DAY_MINUTE:
    case MYLITE_TEMPORAL_EXTRACT_DAY_SECOND:
    case MYLITE_TEMPORAL_EXTRACT_HOUR_MINUTE:
    case MYLITE_TEMPORAL_EXTRACT_HOUR_SECOND:
    case MYLITE_TEMPORAL_EXTRACT_MINUTE_SECOND:
    case MYLITE_TEMPORAL_EXTRACT_TIME_TO_SEC:
    case MYLITE_TEMPORAL_EXTRACT_TO_DAYS:
    case MYLITE_TEMPORAL_EXTRACT_TO_SECONDS:
        return true;
    case MYLITE_TEMPORAL_EXTRACT_DATE:
    case MYLITE_TEMPORAL_EXTRACT_TIME:
    case MYLITE_TEMPORAL_EXTRACT_LAST_DAY:
    case MYLITE_TEMPORAL_EXTRACT_DAYNAME:
    case MYLITE_TEMPORAL_EXTRACT_MONTHNAME:
        return false;
    }
    return false;
}
