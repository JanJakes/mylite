static int plan_regexp_like_truth_predicate(
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
    int rc = plan_regexp_like_predicate_expression(
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

static int plan_regexp_like_comparison_predicate(
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
    int rc = plan_regexp_like_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = plan_regexp_like_predicate_integer_value(
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

static int plan_regexp_like_is_null_predicate(
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
    int rc = plan_regexp_like_predicate_expression(
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

static int plan_regexp_like_predicate_expression(
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
            "REGEXP_LIKE() predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_regexp_like_expression(
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

static int plan_regexp_like_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = mylite_execution_string_slice_signed_integer_value(
        database,
        expression,
        "REGEXP_LIKE() predicates support only integer and boolean comparison literals",
        "REGEXP_LIKE() predicate comparison literals must fit the signed 64-bit range",
        &value,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = is_null,
        .is_text = false,
        .integer = value,
    };
    return MYLITE_OK;
}

static int plan_json_valid_truth_predicate(
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
    int rc = plan_json_valid_predicate_expression(
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

static int plan_json_valid_comparison_predicate(
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
    int rc = plan_json_valid_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = plan_json_valid_predicate_integer_value(
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

static int plan_json_valid_is_null_predicate(
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
    int rc = plan_json_valid_predicate_expression(
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

static int plan_json_valid_predicate_expression(
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
            "JSON_VALID() predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_json_valid_expression(
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

static int plan_json_valid_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = mylite_execution_string_slice_signed_integer_value(
        database,
        expression,
        "JSON_VALID() predicates support only integer and boolean comparison literals",
        "JSON_VALID() predicate comparison literals must fit the signed 64-bit range",
        &value,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = is_null,
        .is_text = false,
        .integer = value,
    };
    return MYLITE_OK;
}

static int plan_json_contains_truth_predicate(
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
    int rc = plan_json_contains_predicate_expression(
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

static int plan_json_contains_comparison_predicate(
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
    int rc = plan_json_contains_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = plan_json_contains_predicate_integer_value(
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

static int plan_json_contains_is_null_predicate(
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
    int rc = plan_json_contains_predicate_expression(
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

static int plan_json_contains_predicate_expression(
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
            "JSON_CONTAINS() predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_json_expression(
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

static int plan_json_contains_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = mylite_execution_string_slice_signed_integer_value(
        database,
        expression,
        "JSON_CONTAINS() predicates support only integer and boolean comparison literals",
        "JSON_CONTAINS() predicate comparison literals must fit the signed 64-bit range",
        &value,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = is_null,
        .is_text = false,
        .integer = value,
    };
    return MYLITE_OK;
}
