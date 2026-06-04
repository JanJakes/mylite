static int plan_string_length_truth_predicate(
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
    int rc = plan_string_length_predicate_expression(
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

static int plan_string_length_comparison_predicate(
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
    int rc = plan_string_length_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK &&
        predicate_node_is_string_length_expression(child_at(predicate_node, 1U))) {
        rc = plan_predicate_row_scalar_value_expression(
            database,
            child_at(predicate_node, 1U),
            source_context,
            table_columns,
            table_column_count,
            &node
        );
    } else if (rc == MYLITE_OK) {
        rc =
            plan_string_length_predicate_value(database, child_at(predicate_node, 1U), &node.value);
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
        planned_row_scalar_expression_deinit(node.row_scalar_value_expression);
        free(node.row_scalar_value_expression);
    }
    return rc;
}

static int plan_string_length_is_null_predicate(
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
    int rc = plan_string_length_predicate_expression(
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

static int plan_string_length_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "string length function predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_string_length_expression(
        database,
        expression,
        true,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_WHERE,
        node->row_scalar_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_string_length_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(expression);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int64_t value = 0;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (literal != NULL && literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        literal = unwrap_parenthesized_expression(child_at(literal, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "string length predicates support only integer, boolean, and NULL comparison literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER &&
        literal_kind != MYLITE_SQL_AST_LITERAL_TRUE &&
        literal_kind != MYLITE_SQL_AST_LITERAL_FALSE &&
        literal_kind != MYLITE_SQL_AST_LITERAL_NULL) {
        set_unsupported_error(
            database,
            "string length predicates support only integer, boolean, and NULL comparison literals"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_execution_string_slice_signed_integer_value(
        database,
        expression,
        "string length predicates support only integer, boolean, and NULL comparison literals",
        "string length predicate comparison literals must fit the signed 64-bit range",
        &value,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    int64_t planned_integer = value;
    if (is_null) {
        planned_integer = 0;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = is_null,
        .is_text = false,
        .integer = planned_integer,
    };
    return MYLITE_OK;
}

static int plan_substring_comparison_predicate(
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
    int rc = plan_substring_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK && predicate_node_is_substring_expression(child_at(predicate_node, 1U))) {
        rc = plan_predicate_row_scalar_value_expression(
            database,
            child_at(predicate_node, 1U),
            source_context,
            table_columns,
            table_column_count,
            &node
        );
    } else if (rc == MYLITE_OK) {
        rc = plan_substring_predicate_value(database, child_at(predicate_node, 1U), &node.value);
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
        planned_row_scalar_expression_deinit(node.row_scalar_value_expression);
        free(node.row_scalar_value_expression);
    }
    return rc;
}

static int plan_substring_is_null_predicate(
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
    int rc = plan_substring_predicate_expression(
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

static int plan_substring_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "SUBSTRING function predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_string_slice_expression(
        database,
        expression,
        true,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_WHERE,
        node->row_scalar_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_substring_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    if (predicate_comparison_value_is_null(expression)) {
        planned_value_deinit(out_value);
        *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        return MYLITE_OK;
    }

    return convert_predicate_string_literal(database, expression, out_value);
}

static int plan_find_in_set_truth_predicate(
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
    int rc = plan_find_in_set_predicate_expression(
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

static int plan_find_in_set_comparison_predicate(
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
    int rc = plan_find_in_set_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = plan_find_in_set_predicate_integer_value(
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

static int plan_find_in_set_is_null_predicate(
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
    int rc = plan_find_in_set_predicate_expression(
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

static int plan_find_in_set_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "FIND_IN_SET() predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_find_in_set_expression(
        database,
        expression,
        true,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_WHERE,
        node->row_scalar_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_find_in_set_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    int64_t value = 0;
    int64_t planned_integer = 0;
    bool is_null = false;
    int rc = mylite_execution_string_slice_signed_integer_value(
        database,
        expression,
        "FIND_IN_SET() predicates support only integer and boolean comparison literals",
        "FIND_IN_SET() predicate comparison literals must fit the signed 64-bit range",
        &value,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    if (!is_null) {
        planned_integer = value;
    }
    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = is_null,
        .is_text = false,
        .integer = planned_integer,
    };
    return MYLITE_OK;
}
