static int plan_select_predicate_leaf_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    size_t node_index = 0U;
    int rc = MYLITE_OK;

    if (predicate_node_is_find_in_set_expression(predicate_node)) {
        rc = plan_find_in_set_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_regexp_like_expression(predicate_node)) {
        rc = plan_regexp_like_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_json_valid_expression(predicate_node)) {
        rc = plan_json_valid_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_json_contains_expression(predicate_node)) {
        rc = plan_json_contains_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_string_length_expression(predicate_node)) {
        rc = plan_string_length_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_date_format_numeric_predicate_expression(predicate_node)) {
        rc = plan_date_format_numeric_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_temporal_extract_expression(predicate_node)) {
        rc = plan_temporal_extract_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_scalar_literal_expression(predicate_node)) {
        rc = plan_scalar_literal_truth_predicate(
            database,
            predicate_node,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        rc = plan_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        rc = plan_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE) {
        rc = plan_is_boolean_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_BETWEEN_PREDICATE) {
        rc = plan_between_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_IN_PREDICATE) {
        rc = plan_in_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate,
            &node_index
        );
    } else {
        rc = plan_exists_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate,
            &node_index
        );
    }
    if (rc == MYLITE_OK) {
        rc =
            append_predicate_result_index(database, result_indexes, result_index_count, node_index);
    }

    return rc;
}

static int plan_select_predicate_leaf_node_without_exists(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    size_t node_index = 0U;
    int rc = MYLITE_OK;

    if (predicate_node_is_find_in_set_expression(predicate_node)) {
        rc = plan_find_in_set_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_regexp_like_expression(predicate_node)) {
        rc = plan_regexp_like_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_json_valid_expression(predicate_node)) {
        rc = plan_json_valid_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_json_contains_expression(predicate_node)) {
        rc = plan_json_contains_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_string_length_expression(predicate_node)) {
        rc = plan_string_length_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_date_format_numeric_predicate_expression(predicate_node)) {
        rc = plan_date_format_numeric_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_temporal_extract_expression(predicate_node)) {
        rc = plan_temporal_extract_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_scalar_literal_expression(predicate_node)) {
        rc = plan_scalar_literal_truth_predicate(
            database,
            predicate_node,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        rc = plan_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        rc = plan_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE) {
        rc = plan_is_boolean_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_BETWEEN_PREDICATE) {
        rc = plan_between_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else {
        rc = plan_in_literal_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    }
    if (rc == MYLITE_OK) {
        rc =
            append_predicate_result_index(database, result_indexes, result_index_count, node_index);
    }

    return rc;
}

static bool planned_predicate_kind_for_operator(
    enum mylite_sql_ast_operator operator_kind,
    enum planned_select_predicate_kind *out_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND:
        *out_kind = PLANNED_SELECT_PREDICATE_AND;
        return true;
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR:
        *out_kind = PLANNED_SELECT_PREDICATE_OR;
        return true;
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
        *out_kind = PLANNED_SELECT_PREDICATE_XOR;
        return true;
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
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
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
        break;
    }

    return false;
}

static int plan_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    bool row_scalar_function_handled = false;
    int row_scalar_function_rc = plan_row_scalar_function_comparison_predicate(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        predicate,
        out_node_index,
        &row_scalar_function_handled
    );

    if (row_scalar_function_handled || row_scalar_function_rc != MYLITE_OK) {
        return row_scalar_function_rc;
    }

    if (predicate_node_is_scalar_literal_expression(child_at(predicate_node, 0U))) {
        if (predicate_node_is_scalar_literal_expression(child_at(predicate_node, 1U))) {
            return plan_scalar_literal_comparison_predicate(
                database,
                predicate_node,
                predicate,
                out_node_index
            );
        }
        if (predicate_node_is_column_reference(child_at(predicate_node, 1U))) {
            return plan_literal_left_column_comparison_predicate(
                database,
                predicate_node,
                source_context,
                table_columns,
                table_column_count,
                predicate,
                out_node_index
            );
        }
    }

    if (comparison_predicate_rhs_is_column_reference(predicate_node)) {
        if (options != NULL && options->allow_same_scope_column_reference_rhs) {
            return plan_same_scope_column_comparison_predicate(
                database,
                predicate_node,
                source_context,
                table_columns,
                table_column_count,
                predicate,
                out_node_index
            );
        }
        if (options == NULL || !options->allow_column_reference_rhs) {
            set_unsupported_error(
                database,
                "WHERE column-to-column predicates are supported only inside EXISTS"
            );
            return MYLITE_ERROR;
        }
        return plan_column_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            options,
            predicate,
            out_node_index
        );
    }

    int rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );

    if (rc == MYLITE_OK) {
        rc = validate_comparison_predicate_column(database, &node);
    }
    if (rc == MYLITE_OK) {
        if (comparison_operator_is_like(node.operator_kind)) {
            node.like_uses_escape = (bool)!session_sql_mode_has(
                &database->session,
                MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES
            );
            if (child_at(predicate_node, 1U) != NULL &&
                child_at(predicate_node, 1U)->kind == MYLITE_SQL_AST_LITERAL) {
                rc = convert_predicate_string_pattern_literal(
                    database,
                    child_at(predicate_node, 1U),
                    &node.value
                );
            } else {
                rc = plan_predicate_row_scalar_value_expression(
                    database,
                    child_at(predicate_node, 1U),
                    source_context,
                    table_columns,
                    table_column_count,
                    &node
                );
            }
        } else if (comparison_operator_is_regexp(node.operator_kind)) {
            rc = convert_predicate_regexp_pattern_literal(
                database,
                child_at(predicate_node, 1U),
                &node.value
            );
        } else if (
            child_at(predicate_node, 1U) != NULL &&
            child_at(predicate_node, 1U)->kind == MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION
        ) {
            rc = plan_predicate_row_scalar_value_expression(
                database,
                child_at(predicate_node, 1U),
                source_context,
                table_columns,
                table_column_count,
                &node
            );
        } else if (
            child_at(predicate_node, 1U) != NULL &&
            child_at(predicate_node, 1U)->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE &&
            (column_descriptor_is_string_family(&node.column) ||
             column_descriptor_is_datetime(&node.column) ||
             column_descriptor_is_timestamp(&node.column))
        ) {
            rc = plan_predicate_row_scalar_value_expression(
                database,
                child_at(predicate_node, 1U),
                source_context,
                table_columns,
                table_column_count,
                &node
            );
        } else if (predicate_comparison_value_is_null(child_at(predicate_node, 1U))) {
            node.value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        } else if (
            column_descriptor_is_string_family(&node.column) &&
            child_at(predicate_node, 1U) != NULL &&
            child_at(predicate_node, 1U)->kind != MYLITE_SQL_AST_LITERAL
        ) {
            rc = plan_predicate_row_scalar_value_expression(
                database,
                child_at(predicate_node, 1U),
                source_context,
                table_columns,
                table_column_count,
                &node
            );
        } else {
            rc = convert_predicate_value_with_date_comparison_mode(
                database,
                child_at(predicate_node, 1U),
                &node.column,
                &node.value,
                &node.compare_date_as_datetime
            );
        }
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_value_deinit(&node.upper_value);
        planned_row_scalar_expression_deinit(node.row_scalar_value_expression);
        free(node.row_scalar_value_expression);
        return rc;
    }

    rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_value_deinit(&node.upper_value);
        planned_row_scalar_expression_deinit(node.row_scalar_value_expression);
        free(node.row_scalar_value_expression);
    }
    return rc;
}

static int plan_predicate_row_scalar_value_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    node->row_scalar_value_expression = calloc(1U, sizeof(*node->row_scalar_value_expression));
    if (node->row_scalar_value_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_expression(
        database,
        expression,
        true,
        source_context,
        NULL,
        table_columns,
        table_column_count,
        node->row_scalar_value_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_value_expression);
        free(node->row_scalar_value_expression);
        node->row_scalar_value_expression = NULL;
    }
    return rc;
}

static int plan_row_scalar_function_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate_node, 0U);

    *out_handled = true;
    if (predicate_node_is_find_in_set_expression(left_node)) {
        return plan_find_in_set_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_regexp_like_expression(left_node)) {
        return plan_regexp_like_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_json_valid_expression(left_node)) {
        return plan_json_valid_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_json_contains_expression(left_node)) {
        return plan_json_contains_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_string_length_expression(left_node)) {
        return plan_string_length_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_substring_expression(left_node)) {
        return plan_substring_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_temporal_extract_expression(left_node)) {
        return plan_temporal_extract_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }

    *out_handled = false;
    return MYLITE_OK;
}

static int plan_scalar_literal_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_where_scalar_literal_expression(database, predicate_node, &node);

    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_scalar_literal_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_where_scalar_literal_expression(database, child_at(predicate_node, 0U), &node);

    if (rc == MYLITE_OK) {
        rc = plan_where_scalar_literal_value(database, child_at(predicate_node, 1U), &node.value);
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

static int plan_literal_left_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_COMPARISON,
        .operator_kind = comparison_operator_flipped(mylite_sql_ast_node_operator(predicate_node)),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 1U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );

    if (rc == MYLITE_OK) {
        rc = validate_comparison_predicate_column(database, &node);
    }
    if (rc == MYLITE_OK) {
        if (predicate_comparison_value_is_null(child_at(predicate_node, 0U))) {
            node.value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        } else {
            rc = convert_predicate_value_with_date_comparison_mode(
                database,
                child_at(predicate_node, 0U),
                &node.column,
                &node.value,
                &node.compare_date_as_datetime
            );
        }
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_value_deinit(&node.upper_value);
        return rc;
    }

    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static int plan_same_scope_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
        .value_is_column_reference = true,
    };
    int rc = MYLITE_OK;

    if (node.operator_kind != MYLITE_SQL_AST_OPERATOR_EQUAL) {
        set_unsupported_error(database, "WHERE column-to-column predicates support only =");
        return MYLITE_ERROR;
    }

    rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );
    if (rc == MYLITE_OK) {
        rc = resolve_predicate_column_with_source_index(
            database,
            child_at(predicate_node, 1U),
            source_context,
            table_columns,
            table_column_count,
            &node.value_column,
            &node.value_column_source_index
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!same_scope_column_comparison_is_supported(&node)) {
        set_unsupported_error(
            database,
            "WHERE column-to-column predicates support only same-family integer or string "
            "descriptor equality"
        );
        return MYLITE_ERROR;
    }

    predicate->qualify_column_references = true;
    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static bool same_scope_column_comparison_is_supported(
    const struct planned_select_predicate_node *node
) {
    return join_condition_columns_are_compatible(&node->column, &node->value_column);
}

static int plan_scalar_literal_is_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_value value = {.is_null = false, .is_text = false, .integer = 0};
    int rc = plan_where_scalar_literal_value(database, child_at(predicate_node, 0U), &value);
    bool truth = false;

    if (rc == MYLITE_OK) {
        truth = scalar_literal_is_result(&value, mylite_sql_ast_node_operator(predicate_node));
        rc = plan_constant_truth_predicate(database, truth, predicate, out_node_index);
    }
    planned_value_deinit(&value);
    return rc;
}

static int plan_constant_truth_predicate(
    struct mylite_db *database,
    bool truth,
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

    node.row_scalar_expression = calloc(1U, sizeof(*node.row_scalar_expression));
    if (node.row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    node.row_scalar_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    node.row_scalar_expression->value =
        (struct planned_value){.is_null = false, .is_text = false, .integer = 0};
    if (truth) {
        node.row_scalar_expression->value.integer = 1;
    }

    rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_where_scalar_literal_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    node->row_scalar_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    rc = plan_where_scalar_literal_value(database, expression, &node->row_scalar_expression->value);
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_where_scalar_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = mylite_execution_string_slice_signed_integer_value(
        database,
        expression,
        "WHERE scalar literal predicates support only integer, boolean, and NULL literals",
        "WHERE scalar literal integer literals must fit the signed 64-bit range",
        &value,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){.is_null = is_null, .is_text = false, .integer = value};
    if (is_null) {
        out_value->integer = 0;
    }
    return MYLITE_OK;
}

static bool predicate_node_is_scalar_literal_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    const struct mylite_sql_ast_node *node = unwrap_parenthesized_predicate(predicate_node);
    const struct mylite_sql_ast_node *literal = node;

    if (node != NULL && node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(node);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        literal = unwrap_parenthesized_predicate(child_at(node, 0U));
        return (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
                mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) != 0;
    }

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }
    switch (mylite_sql_ast_node_literal_kind(literal)) {
    case MYLITE_SQL_AST_LITERAL_INTEGER:
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
    case MYLITE_SQL_AST_LITERAL_NULL:
        return true;
    default:
        return false;
    }
}

static bool predicate_node_is_column_reference(const struct mylite_sql_ast_node *node) {
    return (node != NULL && (node->kind == MYLITE_SQL_AST_IDENTIFIER ||
                             node->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) != 0;
}

static bool predicate_comparison_value_is_null(const struct mylite_sql_ast_node *value_node) {
    return (value_node != NULL && value_node->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) != 0;
}

static enum mylite_sql_ast_operator comparison_operator_flipped(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return MYLITE_SQL_AST_OPERATOR_GREATER;
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL;
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return MYLITE_SQL_AST_OPERATOR_LESS;
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return MYLITE_SQL_AST_OPERATOR_LESS_EQUAL;
    default:
        return operator_kind;
    }
}

static bool scalar_literal_is_result(
    const struct planned_value *value,
    enum mylite_sql_ast_operator operator_kind
) {
    bool is_true = false;
    bool is_false = false;
    bool is_unknown = false;

    if (value == NULL || value->is_null) {
        is_unknown = true;
    } else if (value->integer == 0) {
        is_false = true;
    } else {
        is_true = true;
    }

    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
        return is_true;
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
        if (is_true) {
            return false;
        }
        return true;
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
        return is_false;
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
        if (is_false) {
            return false;
        }
        return true;
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
        return is_unknown;
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
        if (is_unknown) {
            return false;
        }
        return true;
    default:
        return false;
    }
}

static bool predicate_node_is_find_in_set_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    return (predicate_node != NULL &&
            predicate_node->kind == MYLITE_SQL_AST_FIND_IN_SET_FUNCTION) != 0;
}

static bool predicate_node_is_json_valid_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    return (predicate_node != NULL && predicate_node->kind == MYLITE_SQL_AST_JSON_VALID_FUNCTION) !=
           0;
}

static bool predicate_node_is_json_contains_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    return (predicate_node != NULL &&
            (predicate_node->kind == MYLITE_SQL_AST_JSON_CONTAINS_FUNCTION ||
             predicate_node->kind == MYLITE_SQL_AST_JSON_CONTAINS_PATH_FUNCTION)) != 0;
}

static bool predicate_node_is_regexp_like_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    return (predicate_node != NULL &&
            predicate_node->kind == MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION) != 0;
}

static bool predicate_node_is_string_length_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    return (predicate_node != NULL && mylite_execution_is_string_length_function_kind(predicate_node->kind)) != 0;
}

static bool predicate_node_is_substring_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    if (predicate_node == NULL) {
        return false;
    }

    switch (predicate_node->kind) {
    case MYLITE_SQL_AST_SUBSTRING_FUNCTION:
    case MYLITE_SQL_AST_SUBSTR_FUNCTION:
    case MYLITE_SQL_AST_MID_FUNCTION:
        return true;
    default:
        return false;
    }
}

static bool predicate_node_is_temporal_extract_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    return (predicate_node != NULL &&
            (predicate_node->kind == MYLITE_SQL_AST_EXTRACT_FUNCTION ||
             mylite_execution_is_temporal_extract_function_kind(predicate_node->kind))) != 0;
}

static bool predicate_node_is_date_format_numeric_predicate_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    const struct mylite_sql_ast_node *left = NULL;

    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    if (predicate_node == NULL || predicate_node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return false;
    }

    left = unwrap_parenthesized_expression(child_at(predicate_node, 0U));
    return (left != NULL && left->kind == MYLITE_SQL_AST_DATE_FORMAT_FUNCTION) != 0;
}
