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
    } else if (predicate_node_is_sounds_like_expression(predicate_node)) {
        rc = plan_sounds_like_truth_predicate(
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
        bool scalar_aggregate_handled = false;

        rc = plan_scalar_aggregate_subquery_between_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate,
            &node_index,
            &scalar_aggregate_handled
        );
        if (rc != MYLITE_OK || scalar_aggregate_handled) {
            /* handled above */
        } else if (predicate_node_is_temporal_extract_expression(child_at(predicate_node, 0U))) {
            rc = plan_temporal_extract_between_predicate(
                database,
                predicate_node,
                source_context,
                table_columns,
                table_column_count,
                out_predicate,
                &node_index
            );
        } else if (row_scalar_conversion_kind_from_expression(child_at(predicate_node, 0U)) !=
                       PLANNED_ROW_SCALAR_CONVERSION_NONE ||
                   predicate_node_is_supported_row_scalar_expression(
                       child_at(predicate_node, 0U))) {
            rc = plan_row_scalar_expression_between_predicate(
                database,
                predicate_node,
                source_context,
                table_columns,
                table_column_count,
                out_predicate,
                &node_index
            );
        } else {
            rc = plan_between_predicate(
                database,
                predicate_node,
                source_context,
                table_columns,
                table_column_count,
                out_predicate,
                &node_index
            );
        }
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
    } else if (predicate_node_is_sounds_like_expression(predicate_node)) {
        rc = plan_sounds_like_truth_predicate(
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
        bool scalar_aggregate_handled = false;

        rc = plan_scalar_aggregate_subquery_between_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate,
            &node_index,
            &scalar_aggregate_handled
        );
        if (rc != MYLITE_OK || scalar_aggregate_handled) {
            /* handled above */
        } else if (predicate_node_is_temporal_extract_expression(child_at(predicate_node, 0U))) {
            rc = plan_temporal_extract_between_predicate(
                database,
                predicate_node,
                source_context,
                table_columns,
                table_column_count,
                out_predicate,
                &node_index
            );
        } else if (row_scalar_conversion_kind_from_expression(child_at(predicate_node, 0U)) !=
                       PLANNED_ROW_SCALAR_CONVERSION_NONE ||
                   predicate_node_is_supported_row_scalar_expression(
                       child_at(predicate_node, 0U))) {
            rc = plan_row_scalar_expression_between_predicate(
                database,
                predicate_node,
                source_context,
                table_columns,
                table_column_count,
                out_predicate,
                &node_index
            );
        } else {
            rc = plan_between_predicate(
                database,
                predicate_node,
                source_context,
                table_columns,
                table_column_count,
                out_predicate,
                &node_index
            );
        }
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
    case MYLITE_SQL_AST_OPERATOR_SOUNDS_LIKE:
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

    bool scalar_aggregate_subquery_handled = false;
    int scalar_aggregate_subquery_rc = plan_scalar_aggregate_subquery_comparison_predicate(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        options,
        predicate,
        out_node_index,
        &scalar_aggregate_subquery_handled
    );

    if (scalar_aggregate_subquery_handled || scalar_aggregate_subquery_rc != MYLITE_OK) {
        return scalar_aggregate_subquery_rc;
    }

    bool scalar_count_subquery_handled = false;
    int scalar_count_subquery_rc = plan_scalar_count_subquery_comparison_predicate(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        options,
        predicate,
        out_node_index,
        &scalar_count_subquery_handled
    );

    if (scalar_count_subquery_handled || scalar_count_subquery_rc != MYLITE_OK) {
        return scalar_count_subquery_rc;
    }

    bool literal_left_handled = false;
    int literal_left_rc = plan_scalar_literal_or_literal_column_comparison_predicate(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        predicate,
        out_node_index,
        &literal_left_handled
    );

    if (literal_left_handled || literal_left_rc != MYLITE_OK) {
        return literal_left_rc;
    }

    bool rhs_column_handled = false;
    int rhs_column_rc = plan_rhs_column_comparison_predicate(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        options,
        predicate,
        out_node_index,
        &rhs_column_handled
    );

    if (rhs_column_handled || rhs_column_rc != MYLITE_OK) {
        return rhs_column_rc;
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
    if (rc == MYLITE_OK && comparison_operator_is_regexp(node.operator_kind) &&
        !column_descriptor_is_string_family(&node.column)) {
        node.regexp_cast_subject_as_text = true;
    }
    if (rc == MYLITE_OK) {
        rc = plan_comparison_predicate_value(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            &node
        );
    }
    if (rc != MYLITE_OK) {
        planned_select_predicate_node_deinit_runtime_values(&node);
        return rc;
    }

    rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    if (rc != MYLITE_OK) {
        planned_select_predicate_node_deinit_runtime_values(&node);
    }
    return rc;
}

static int plan_scalar_count_subquery_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate_node, 0U);
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_SCALAR_COUNT_SUBQUERY_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    size_t outer_source_count = 1U;
    int rc = MYLITE_OK;

    *out_handled = left_node != NULL && left_node->kind == MYLITE_SQL_AST_SCALAR_SUBQUERY;
    if (!*out_handled) {
        return MYLITE_OK;
    }
    if (options == NULL ||
        (!options->allow_exists && !options->allow_scalar_count_subquery_comparison)) {
        set_unsupported_error(
            database,
            "scalar COUNT subquery comparisons are supported only in SELECT WHERE"
        );
        return MYLITE_ERROR;
    }
    if (source_context == NULL) {
        set_unsupported_error(
            database,
            "scalar COUNT subquery comparisons support only an outer table source"
        );
        return MYLITE_ERROR;
    }
    if (select_source_context_is_joined(source_context)) {
        outer_source_count = source_context->source_count;
    }

    node.exists_subquery = calloc(1U, sizeof(*node.exists_subquery));
    if (node.exists_subquery == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_scalar_count_subquery(
        database,
        left_node,
        source_context,
        table_columns,
        table_column_count,
        outer_source_count,
        node.exists_subquery
    );
    if (rc == MYLITE_OK) {
        rc = plan_where_scalar_literal_value(database, child_at(predicate_node, 1U), &node.value);
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_exists_subquery_deinit(node.exists_subquery);
        free(node.exists_subquery);
        return rc;
    }

    predicate->qualify_column_references = true;
    return MYLITE_OK;
}

static int plan_scalar_aggregate_subquery_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate_node, 0U);
    const struct mylite_sql_ast_node *right_node = child_at(predicate_node, 1U);
    bool left_is_scalar_aggregate = scalar_subquery_is_column_aggregate(left_node);
    bool right_is_scalar_aggregate = scalar_subquery_is_column_aggregate(right_node);
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_SCALAR_AGGREGATE_SUBQUERY_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    size_t outer_source_count = 1U;
    int rc = MYLITE_OK;

    *out_handled = left_is_scalar_aggregate || right_is_scalar_aggregate;
    if (!*out_handled) {
        return MYLITE_OK;
    }
    if (options == NULL ||
        (!options->allow_exists && !options->allow_scalar_count_subquery_comparison)) {
        set_unsupported_error(
            database,
            "scalar aggregate subquery comparisons are supported only in SELECT WHERE"
        );
        return MYLITE_ERROR;
    }
    if (source_context == NULL) {
        set_unsupported_error(
            database,
            "scalar aggregate subquery comparisons support only an outer table source"
        );
        return MYLITE_ERROR;
    }
    if (select_source_context_is_joined(source_context)) {
        outer_source_count = source_context->source_count;
    }

    if (left_is_scalar_aggregate) {
        rc = allocate_scalar_aggregate_subquery(database, &node.subject_scalar_subquery);
        if (rc == MYLITE_OK) {
            rc = plan_scalar_aggregate_subquery_operand(
                database,
                left_node,
                source_context,
                table_columns,
                table_column_count,
                node.subject_scalar_subquery,
                outer_source_count
            );
        }
    } else {
        rc = resolve_predicate_column_with_source_index(
            database,
            left_node,
            source_context,
            table_columns,
            table_column_count,
            &node.column,
            &node.column_source_index
        );
        if (rc == MYLITE_OK) {
            rc = validate_comparison_predicate_column(database, &node);
        }
    }
    if (rc == MYLITE_OK && right_is_scalar_aggregate) {
        rc = allocate_scalar_aggregate_subquery(database, &node.value_scalar_subquery);
        if (rc == MYLITE_OK) {
            rc = plan_scalar_aggregate_subquery_operand(
                database,
                right_node,
                source_context,
                table_columns,
                table_column_count,
                node.value_scalar_subquery,
                outer_source_count
            );
        }
    } else if (rc == MYLITE_OK) {
        rc = plan_where_scalar_literal_value(database, right_node, &node.value);
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_scalar_aggregate_subquery_deinit(node.subject_scalar_subquery);
        free(node.subject_scalar_subquery);
        planned_scalar_aggregate_subquery_deinit(node.value_scalar_subquery);
        free(node.value_scalar_subquery);
        return rc;
    }

    predicate->qualify_column_references = true;
    return MYLITE_OK;
}

static int plan_scalar_aggregate_subquery_between_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
) {
    const struct mylite_sql_ast_node *subject_node = child_at(predicate_node, 0U);
    const struct mylite_sql_ast_node *lower_node = child_at(predicate_node, 1U);
    const struct mylite_sql_ast_node *upper_node = child_at(predicate_node, 2U);
    bool subject_is_scalar_aggregate = scalar_subquery_is_column_aggregate(subject_node);
    bool lower_is_scalar_aggregate = scalar_subquery_is_column_aggregate(lower_node);
    bool upper_is_scalar_aggregate = scalar_subquery_is_column_aggregate(upper_node);
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_SCALAR_AGGREGATE_SUBQUERY_BETWEEN,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    size_t outer_source_count = 1U;
    int rc = MYLITE_OK;

    *out_handled =
        subject_is_scalar_aggregate || lower_is_scalar_aggregate || upper_is_scalar_aggregate;
    if (!*out_handled) {
        return MYLITE_OK;
    }
    if (options == NULL ||
        (!options->allow_exists && !options->allow_scalar_count_subquery_comparison)) {
        set_unsupported_error(
            database,
            "scalar aggregate subquery BETWEEN predicates are supported only in SELECT WHERE"
        );
        return MYLITE_ERROR;
    }
    if (source_context == NULL) {
        set_unsupported_error(
            database,
            "scalar aggregate subquery BETWEEN predicates support only an outer table source"
        );
        return MYLITE_ERROR;
    }
    if (select_source_context_is_joined(source_context)) {
        outer_source_count = source_context->source_count;
    }

    if (subject_is_scalar_aggregate) {
        rc = allocate_scalar_aggregate_subquery(database, &node.subject_scalar_subquery);
        if (rc == MYLITE_OK) {
            rc = plan_scalar_aggregate_subquery_operand(
                database,
                subject_node,
                source_context,
                table_columns,
                table_column_count,
                node.subject_scalar_subquery,
                outer_source_count
            );
        }
    } else {
        rc = resolve_predicate_column_with_source_index(
            database,
            subject_node,
            source_context,
            table_columns,
            table_column_count,
            &node.column,
            &node.column_source_index
        );
    }
    if (rc == MYLITE_OK && lower_is_scalar_aggregate) {
        rc = allocate_scalar_aggregate_subquery(database, &node.value_scalar_subquery);
        if (rc == MYLITE_OK) {
            rc = plan_scalar_aggregate_subquery_operand(
                database,
                lower_node,
                source_context,
                table_columns,
                table_column_count,
                node.value_scalar_subquery,
                outer_source_count
            );
        }
    } else if (rc == MYLITE_OK) {
        rc = plan_where_scalar_literal_value(database, lower_node, &node.value);
    }
    if (rc == MYLITE_OK && upper_is_scalar_aggregate) {
        rc = allocate_scalar_aggregate_subquery(database, &node.upper_scalar_subquery);
        if (rc == MYLITE_OK) {
            rc = plan_scalar_aggregate_subquery_operand(
                database,
                upper_node,
                source_context,
                table_columns,
                table_column_count,
                node.upper_scalar_subquery,
                outer_source_count
            );
        }
    } else if (rc == MYLITE_OK) {
        rc = plan_where_scalar_literal_value(database, upper_node, &node.upper_value);
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_value_deinit(&node.upper_value);
        planned_scalar_aggregate_subquery_deinit(node.subject_scalar_subquery);
        free(node.subject_scalar_subquery);
        planned_scalar_aggregate_subquery_deinit(node.value_scalar_subquery);
        free(node.value_scalar_subquery);
        planned_scalar_aggregate_subquery_deinit(node.upper_scalar_subquery);
        free(node.upper_scalar_subquery);
        return rc;
    }

    predicate->qualify_column_references = true;
    return MYLITE_OK;
}

static int plan_scalar_aggregate_subquery_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *scalar_subquery,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_scalar_aggregate_subquery *out_subquery,
    size_t outer_source_count
) {
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *from_clause = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    int rc = MYLITE_OK;

    *out_subquery = (struct planned_scalar_aggregate_subquery){0};
    out_subquery->subquery.inner_source_index = outer_source_count;
    if (scalar_subquery == NULL || scalar_subquery->kind != MYLITE_SQL_AST_SCALAR_SUBQUERY ||
        mylite_sql_ast_node_child_count(scalar_subquery) != 1U) {
        set_unsupported_error(
            database,
            "scalar aggregate subquery supports only parenthesized SELECT"
        );
        return MYLITE_ERROR;
    }
    statement = child_at(scalar_subquery, 0U);
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "scalar aggregate subquery supports only SELECT");
        return MYLITE_ERROR;
    }
    select_list = child_at(statement, 0U);
    from_clause = child_at(statement, 1U);
    optional_clause = child_at(statement, 2U);
    if (mylite_sql_ast_node_select_modifier(statement) != MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT ||
        mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        set_unsupported_error(
            database,
            "scalar aggregate subqueries do not support SELECT modifiers"
        );
        return MYLITE_ERROR;
    }
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else {
            set_unsupported_error(database, "scalar aggregate subqueries support only WHERE");
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }

    rc = scalar_aggregate_subquery_select_expression(
        database,
        select_list,
        &expression,
        &out_subquery->function
    );
    if (rc == MYLITE_OK) {
        rc = plan_exists_table_source(database, from_clause, &out_subquery->subquery);
    }
    if (rc == MYLITE_OK) {
        rc = plan_scalar_aggregate_subquery_column(
            database,
            expression,
            out_subquery->function,
            &out_subquery->subquery,
            &out_subquery->column,
            &out_subquery->column_source_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_exists_inner_predicate(
            database,
            where_clause,
            source_context,
            table_columns,
            table_column_count,
            &out_subquery->subquery
        );
    }
    if (rc != MYLITE_OK) {
        planned_scalar_aggregate_subquery_deinit(out_subquery);
    }
    return rc;
}

static bool scalar_subquery_is_column_aggregate(const struct mylite_sql_ast_node *scalar_subquery) {
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *item = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    enum planned_column_aggregate_function function = PLANNED_COLUMN_AGGREGATE_NONE;

    if (scalar_subquery == NULL || scalar_subquery->kind != MYLITE_SQL_AST_SCALAR_SUBQUERY ||
        mylite_sql_ast_node_child_count(scalar_subquery) != 1U) {
        return false;
    }
    statement = child_at(scalar_subquery, 0U);
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return false;
    }
    select_list = child_at(statement, 0U);
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        mylite_sql_ast_node_child_count(select_list) != 1U) {
        return false;
    }
    item = child_at(select_list, 0U);
    if (item == NULL || item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        return false;
    }
    expression = unwrap_parenthesized_expression(child_at(item, 0U));
    function = column_aggregate_function_from_expression(expression);
    return function == PLANNED_COLUMN_AGGREGATE_MIN || function == PLANNED_COLUMN_AGGREGATE_MAX ||
           function == PLANNED_COLUMN_AGGREGATE_SUM || function == PLANNED_COLUMN_AGGREGATE_AVG;
}

static int scalar_aggregate_subquery_select_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct mylite_sql_ast_node **out_expression,
    enum planned_column_aggregate_function *out_function
) {
    const struct mylite_sql_ast_node *item = NULL;
    const struct mylite_sql_ast_node *expression = NULL;

    *out_expression = NULL;
    *out_function = PLANNED_COLUMN_AGGREGATE_NONE;
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(select_list) != 1U) {
        set_scalar_subquery_column_count_error(database);
        return MYLITE_ERROR;
    }
    item = child_at(select_list, 0U);
    if (item == NULL || item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    expression = unwrap_parenthesized_expression(child_at(item, 0U));
    *out_function = column_aggregate_function_from_expression(expression);
    if (*out_function != PLANNED_COLUMN_AGGREGATE_MIN &&
        *out_function != PLANNED_COLUMN_AGGREGATE_MAX &&
        *out_function != PLANNED_COLUMN_AGGREGATE_SUM &&
        *out_function != PLANNED_COLUMN_AGGREGATE_AVG) {
        set_unsupported_error(
            database,
            "scalar aggregate subqueries support only MIN(), MAX(), SUM(), and AVG()"
        );
        return MYLITE_ERROR;
    }

    *out_expression = expression;
    return MYLITE_OK;
}

static int plan_scalar_aggregate_subquery_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum planned_column_aggregate_function function,
    const struct planned_exists_subquery *subquery,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
) {
    struct select_source_context source_context = {0};
    const struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    size_t source_index = 0U;
    struct integer_column_range range;
    int rc = scalar_aggregate_subquery_source_context(
        subquery,
        &source_context,
        &columns,
        &column_count
    );

    if (rc == MYLITE_OK) {
        rc = resolve_descriptor_column_reference_with_source_index(
            database,
            child_at(expression, 0U),
            &source_context,
            COLUMN_REFERENCE_FIELD,
            column_aggregate_column_error(function),
            columns,
            column_count,
            out_column,
            &source_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = integer_range_for_column(
            database,
            out_column,
            column_aggregate_integer_error(function),
            &range
        );
    }
    if (rc == MYLITE_OK) {
        *out_source_index = subquery->inner_source_index + source_index;
    }
    return rc;
}

static int scalar_aggregate_subquery_source_context(
    const struct planned_exists_subquery *subquery,
    struct select_source_context *out_source_context,
    const struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count
) {
    *out_source_context = (struct select_source_context){0};
    *out_columns = NULL;
    *out_column_count = 0U;
    if (subquery == NULL || !subquery->has_table_source) {
        return MYLITE_ERROR;
    }
    if (subquery->source_count > 0U) {
        return init_join_select_source_context(
            subquery->sources,
            subquery->source_count,
            out_source_context
        );
    }

    *out_source_context = (struct select_source_context){
        .source = &subquery->source.source,
        .has_alias = subquery->source.has_alias,
    };
    memcpy(out_source_context->alias, subquery->source.alias, sizeof(out_source_context->alias));
    *out_columns = subquery->source.columns;
    *out_column_count = subquery->source.column_count;
    return MYLITE_OK;
}

static int allocate_scalar_aggregate_subquery(
    struct mylite_db *database,
    struct planned_scalar_aggregate_subquery **out_subquery
) {
    *out_subquery = calloc(1U, sizeof(**out_subquery));
    if (*out_subquery == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters): table columns and outer sources are distinct
// count domains.
static int plan_scalar_count_subquery(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *scalar_subquery,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    size_t outer_source_count,
    struct planned_exists_subquery *out_subquery
) {
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *from_clause = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    int rc = MYLITE_OK;

    *out_subquery = (struct planned_exists_subquery){
        .has_table_source = false,
        .inner_source_index = outer_source_count,
    };
    if (scalar_subquery == NULL || scalar_subquery->kind != MYLITE_SQL_AST_SCALAR_SUBQUERY ||
        mylite_sql_ast_node_child_count(scalar_subquery) != 1U) {
        set_unsupported_error(database, "scalar COUNT subquery supports only parenthesized SELECT");
        return MYLITE_ERROR;
    }
    statement = child_at(scalar_subquery, 0U);
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "scalar COUNT subquery supports only SELECT subqueries");
        return MYLITE_ERROR;
    }
    select_list = child_at(statement, 0U);
    from_clause = child_at(statement, 1U);
    optional_clause = child_at(statement, 2U);
    if (mylite_sql_ast_node_select_modifier(statement) != MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT ||
        mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        set_unsupported_error(database, "scalar COUNT subqueries do not support SELECT modifiers");
        return MYLITE_ERROR;
    }
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else {
            set_unsupported_error(database, "scalar COUNT subqueries support only WHERE");
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }

    rc = plan_exists_table_source(database, from_clause, out_subquery);
    if (rc == MYLITE_OK) {
        rc = validate_scalar_count_subquery_select_list(database, select_list);
    }
    if (rc == MYLITE_OK) {
        rc = plan_exists_inner_predicate(
            database,
            where_clause,
            source_context,
            table_columns,
            table_column_count,
            out_subquery
        );
    }
    return rc;
}

// NOLINTEND(bugprone-easily-swappable-parameters)

static int validate_scalar_count_subquery_select_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list
) {
    const struct mylite_sql_ast_node *item = NULL;
    const struct mylite_sql_ast_node *expression = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(select_list) != 1U) {
        set_scalar_subquery_column_count_error(database);
        return MYLITE_ERROR;
    }
    item = child_at(select_list, 0U);
    if (item == NULL || item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    expression = child_at(item, 0U);
    if (!scalar_count_subquery_select_expression_is_supported(expression)) {
        set_unsupported_error(
            database,
            "scalar COUNT subquery comparisons support only COUNT(*) or COUNT(literal)"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static bool scalar_count_subquery_select_expression_is_supported(
    const struct mylite_sql_ast_node *expression
) {
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    return expression != NULL && (expression->kind == MYLITE_SQL_AST_COUNT_STAR_FUNCTION ||
                                  expression->kind == MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION);
}

static int plan_scalar_literal_or_literal_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
) {
    *out_handled = false;
    if (!predicate_node_is_scalar_literal_expression(child_at(predicate_node, 0U))) {
        return MYLITE_OK;
    }

    *out_handled = true;
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

    *out_handled = false;
    return MYLITE_OK;
}

static int plan_rhs_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
) {
    *out_handled = comparison_predicate_rhs_is_column_reference(predicate_node);
    if (!*out_handled) {
        return MYLITE_OK;
    }
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

static int plan_comparison_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    if (comparison_operator_is_like(node->operator_kind)) {
        return plan_comparison_predicate_like_value(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            node
        );
    }
    if (comparison_operator_is_regexp(node->operator_kind)) {
        return convert_predicate_regexp_pattern_literal(
            database,
            child_at(predicate_node, 1U),
            false,
            &node->value
        );
    }
    if (comparison_predicate_rhs_requires_row_scalar_value(predicate_node, node)) {
        return plan_predicate_row_scalar_value_expression(
            database,
            child_at(predicate_node, 1U),
            source_context,
            table_columns,
            table_column_count,
            node
        );
    }
    if (predicate_comparison_value_is_null(child_at(predicate_node, 1U))) {
        node->value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        return MYLITE_OK;
    }
    if (column_descriptor_is_string_family(&node->column) &&
        predicate_value_is_integer_literal(child_at(predicate_node, 1U))) {
        node->compare_string_as_integer = true;
        return convert_predicate_string_numeric_literal(
            database,
            child_at(predicate_node, 1U),
            &node->column,
            &node->value
        );
    }
    return convert_predicate_value_with_date_comparison_mode(
        database,
        child_at(predicate_node, 1U),
        &node->column,
        &node->value,
        &node->compare_date_as_datetime
    );
}

static int plan_comparison_predicate_like_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    int rc = like_escape_character_from_predicate(
        database,
        predicate_node,
        &node->like_escape_character
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (child_at(predicate_node, 1U) != NULL &&
        child_at(predicate_node, 1U)->kind == MYLITE_SQL_AST_LITERAL) {
        return convert_predicate_string_pattern_literal(
            database,
            child_at(predicate_node, 1U),
            &node->value
        );
    }
    return plan_predicate_row_scalar_value_expression(
        database,
        child_at(predicate_node, 1U),
        source_context,
        table_columns,
        table_column_count,
        node
    );
}

static int like_escape_character_from_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    char *out_escape_character
) {
    const struct mylite_sql_ast_node *escape_node = child_at(predicate_node, 2U);
    char *escape_text = NULL;
    size_t escape_length = 0U;
    int rc = MYLITE_OK;

    if (out_escape_character == NULL) {
        return MYLITE_MISUSE;
    }
    *out_escape_character =
        session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES)
            ? '\0'
            : '\\';
    if (escape_node == NULL) {
        return MYLITE_OK;
    }
    if (escape_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(escape_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "LIKE ESCAPE supports only string literals");
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        escape_node,
        "LIKE ESCAPE supports only string literals",
        "LIKE ESCAPE does not support NUL bytes",
        &escape_text,
        &escape_length
    );
    if (rc == MYLITE_OK && escape_length != 1U) {
        set_unsupported_error(database, "LIKE ESCAPE supports one-character string literals");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        *out_escape_character = escape_text[0];
    }
    free(escape_text);
    return rc;
}

static bool comparison_predicate_rhs_requires_row_scalar_value(
    const struct mylite_sql_ast_node *predicate_node,
    const struct planned_select_predicate_node *node
) {
    const struct mylite_sql_ast_node *rhs = child_at(predicate_node, 1U);

    if (rhs == NULL) {
        return false;
    }
    if (rhs->kind == MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION) {
        return true;
    }
    if (rhs->kind == MYLITE_SQL_AST_COLLATE_EXPRESSION) {
        return true;
    }
    if (predicate_node_is_supported_row_scalar_expression(rhs)) {
        return true;
    }
    if (rhs->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE) {
        return column_descriptor_is_string_family(&node->column) ||
               column_descriptor_is_datetime(&node->column) ||
               column_descriptor_is_timestamp(&node->column);
    }
    if (column_descriptor_is_string_family(&node->column) &&
        predicate_value_is_integer_literal(rhs)) {
        return false;
    }
    return column_descriptor_is_string_family(&node->column) && rhs->kind != MYLITE_SQL_AST_LITERAL;
}

static bool predicate_value_is_integer_literal(const struct mylite_sql_ast_node *value_node) {
    const struct mylite_sql_ast_node *literal = value_node;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NULL;

    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            return false;
        }
        literal = child_at(value_node, 0U);
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    return literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
           literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
           literal_kind == MYLITE_SQL_AST_LITERAL_FALSE;
}

static void planned_select_predicate_node_deinit_runtime_values(
    struct planned_select_predicate_node *node
) {
    planned_value_deinit(&node->value);
    planned_value_deinit(&node->upper_value);
    planned_row_scalar_expression_deinit(node->row_scalar_value_expression);
    free(node->row_scalar_value_expression);
    planned_row_scalar_expression_deinit(node->row_scalar_upper_value_expression);
    free(node->row_scalar_upper_value_expression);
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

static int plan_predicate_row_scalar_upper_value_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    node->row_scalar_upper_value_expression =
        calloc(1U, sizeof(*node->row_scalar_upper_value_expression));
    if (node->row_scalar_upper_value_expression == NULL) {
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
        node->row_scalar_upper_value_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_upper_value_expression);
        free(node->row_scalar_upper_value_expression);
        node->row_scalar_upper_value_expression = NULL;
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
    if (row_scalar_expression_contains_integer_arithmetic_attempt(left_node)) {
        return plan_row_scalar_expression_comparison_predicate(
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
    if (row_scalar_conversion_kind_from_expression(left_node) !=
        PLANNED_ROW_SCALAR_CONVERSION_NONE) {
        return plan_row_scalar_expression_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    left_node = unwrap_parenthesized_expression(left_node);
    if (left_node != NULL && left_node->kind == MYLITE_SQL_AST_COLLATE_EXPRESSION) {
        return plan_row_scalar_expression_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (row_scalar_expression_is_rand_function(left_node)) {
        return plan_row_scalar_expression_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_supported_row_scalar_expression(left_node)) {
        return plan_row_scalar_expression_comparison_predicate(
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

static bool predicate_node_is_sounds_like_expression(const struct mylite_sql_ast_node *node) {
    node = unwrap_parenthesized_predicate(node);
    return node != NULL && node->kind == MYLITE_SQL_AST_BINARY_EXPRESSION &&
           mylite_sql_ast_node_operator(node) == MYLITE_SQL_AST_OPERATOR_SOUNDS_LIKE &&
           mylite_sql_ast_node_child_count(node) == 2U;
}

static int plan_sounds_like_truth_predicate(
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

    node.row_scalar_expression = calloc(1U, sizeof(*node.row_scalar_expression));
    if (node.row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_expression(
        database,
        predicate_node,
        true,
        source_context,
        NULL,
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

static int plan_row_scalar_expression_comparison_predicate(
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
    int rc = MYLITE_OK;

    node.row_scalar_compare_as_real = predicate_row_scalar_cast_expression_decimal_scale(
        child_at(predicate_node, 0U),
        &node.row_scalar_decimal_scale
    );
    node.row_scalar_regexp_case_sensitive =
        predicate_row_scalar_cast_expression_is_binary(child_at(predicate_node, 0U));
    node.row_scalar_expression = calloc(1U, sizeof(*node.row_scalar_expression));
    if (node.row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_expression(
        database,
        child_at(predicate_node, 0U),
        true,
        source_context,
        NULL,
        table_columns,
        table_column_count,
        node.row_scalar_expression
    );
    if (rc == MYLITE_OK) {
        if (comparison_operator_is_like(node.operator_kind)) {
            rc = plan_comparison_predicate_like_value(
                database,
                predicate_node,
                source_context,
                table_columns,
                table_column_count,
                &node
            );
        } else if (comparison_operator_is_regexp(node.operator_kind)) {
            rc = convert_predicate_regexp_pattern_literal(
                database,
                child_at(predicate_node, 1U),
                node.row_scalar_regexp_case_sensitive,
                &node.value
            );
        } else if (node.row_scalar_compare_as_real) {
            rc = plan_where_scalar_decimal_literal_value(
                database,
                child_at(predicate_node, 1U),
                &node.value
            );
        } else if (
            predicate_node_is_supported_row_scalar_predicate_value_expression(
                child_at(predicate_node, 1U)
            ) ||
            (child_at(predicate_node, 1U) != NULL &&
             node.row_scalar_expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_COLLATE &&
             child_at(predicate_node, 1U)->kind == MYLITE_SQL_AST_LITERAL)) {
            rc = plan_predicate_row_scalar_value_expression(
                database,
                child_at(predicate_node, 1U),
                source_context,
                table_columns,
                table_column_count,
                &node
            );
        } else {
            rc = plan_where_row_scalar_predicate_literal_value(
                database,
                node.row_scalar_expression,
                child_at(predicate_node, 1U),
                &node.value
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_value_expression);
        free(node.row_scalar_value_expression);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_row_scalar_expression_between_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_BETWEEN,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = MYLITE_OK;

    node.row_scalar_compare_as_real = predicate_row_scalar_cast_expression_decimal_scale(
        child_at(predicate_node, 0U),
        &node.row_scalar_decimal_scale
    );
    node.row_scalar_expression = calloc(1U, sizeof(*node.row_scalar_expression));
    if (node.row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_expression(
        database,
        child_at(predicate_node, 0U),
        true,
        source_context,
        NULL,
        table_columns,
        table_column_count,
        node.row_scalar_expression
    );
    if (rc == MYLITE_OK && node.row_scalar_compare_as_real) {
        rc = plan_where_scalar_decimal_literal_value(
            database,
            child_at(predicate_node, 1U),
            &node.value
        );
    } else if (
        rc == MYLITE_OK &&
        predicate_node_is_supported_row_scalar_predicate_value_expression(
            child_at(predicate_node, 1U)
        )) {
        rc = plan_predicate_row_scalar_value_expression(
            database,
            child_at(predicate_node, 1U),
            source_context,
            table_columns,
            table_column_count,
            &node
        );
    } else if (rc == MYLITE_OK) {
        rc = plan_where_row_scalar_predicate_literal_value(
            database,
            node.row_scalar_expression,
            child_at(predicate_node, 1U),
            &node.value
        );
    }
    if (rc == MYLITE_OK && node.row_scalar_compare_as_real) {
        rc = plan_where_scalar_decimal_literal_value(
            database,
            child_at(predicate_node, 2U),
            &node.upper_value
        );
    } else if (
        rc == MYLITE_OK &&
        predicate_node_is_supported_row_scalar_predicate_value_expression(
            child_at(predicate_node, 2U)
        )) {
        rc = plan_predicate_row_scalar_upper_value_expression(
            database,
            child_at(predicate_node, 2U),
            source_context,
            table_columns,
            table_column_count,
            &node
        );
    } else if (rc == MYLITE_OK) {
        rc = plan_where_row_scalar_predicate_literal_value(
            database,
            node.row_scalar_expression,
            child_at(predicate_node, 2U),
            &node.upper_value
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_value_deinit(&node.upper_value);
        planned_row_scalar_expression_deinit(node.row_scalar_value_expression);
        free(node.row_scalar_value_expression);
        planned_row_scalar_expression_deinit(node.row_scalar_upper_value_expression);
        free(node.row_scalar_upper_value_expression);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
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

    if (node.operator_kind != MYLITE_SQL_AST_OPERATOR_EQUAL &&
        node.operator_kind != MYLITE_SQL_AST_OPERATOR_NOT_EQUAL) {
        set_unsupported_error(database, "WHERE column-to-column predicates support only = and <>");
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
            "WHERE column-to-column predicates support only same-family numeric or string "
            "descriptor comparisons"
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

static int plan_where_row_scalar_predicate_literal_value(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *row_scalar_expression,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal =
        mylite_execution_unwrap_parenthesized_expression(expression);
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (row_scalar_predicate_literal_prefers_text(row_scalar_expression) &&
        literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = decode_sql_string_literal(
            database,
            literal,
            "WHERE row-scalar predicates support only string literals without NUL bytes",
            "WHERE row-scalar predicate string literals do not support NUL bytes",
            &text,
            &text_length
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        return assign_text_value(database, text, text_length, out_value);
    }

    return plan_where_scalar_literal_value(database, expression, out_value);
}

static bool row_scalar_predicate_literal_prefers_text(
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL) {
        return false;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        return expression->value.is_text;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return column_descriptor_uses_string_key_collation(&expression->column, true);
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_DIGEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
        return true;
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
        return expression->field_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
        return (expression->conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_CHAR ||
                expression->conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_USING_CHARSET) != 0;
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_ELT:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_COMPARISON:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_COMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESSED_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_RANDOM_BYTES:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLLATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_FUNCTION:
        break;
    }

    return false;
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
    const struct mylite_sql_ast_node *literal =
        mylite_execution_unwrap_parenthesized_expression(expression);
    int64_t value = 0;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
        return plan_where_scalar_exact_integer_string_literal_value(database, literal, out_value);
    }
    if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_HEX) {
        return plan_where_scalar_hex_literal_value(database, literal, out_value);
    }

    rc = mylite_execution_string_slice_signed_integer_value(
        database,
        expression,
        "WHERE scalar literal predicates support only integer, boolean, NULL, and exact quoted "
        "integer literals",
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

static int plan_where_scalar_hex_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct planned_value *out_value
) {
    const uint64_t signed_positive_max = 9223372036854775807ULL;
    uint64_t magnitude = 0U;
    bool overflow = false;
    int rc = parse_dml_hex_literal_magnitude(database, literal, &magnitude, &overflow);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (overflow || magnitude > signed_positive_max) {
        set_unsupported_error(
            database,
            "WHERE scalar literal integer literals must fit the signed 64-bit range"
        );
        return MYLITE_ERROR;
    }

    planned_value_deinit(out_value);
    *out_value =
        (struct planned_value){.is_null = false, .is_text = false, .integer = (int64_t)magnitude};
    return MYLITE_OK;
}

static int plan_where_scalar_exact_integer_string_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct planned_value *out_value
) {
    const uint64_t signed_positive_max = 9223372036854775807ULL;
    const uint64_t signed_negative_abs_max = 9223372036854775808ULL;
    char *text = NULL;
    size_t text_length = 0U;
    size_t value_start = 0U;
    size_t value_end = 0U;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    enum mylite_execution_dml_numeric_parse_result parse_result =
        MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
    int rc = decode_sql_string_literal(
        database,
        literal,
        "WHERE scalar literal predicates support only integer, boolean, NULL, and exact quoted "
        "integer literals",
        "invalid integer string literal",
        &text,
        &text_length
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    while (value_start < text_length &&
           predicate_integer_string_ascii_whitespace((unsigned char)text[value_start])) {
        ++value_start;
    }
    value_end = text_length;
    while (value_end > value_start &&
           predicate_integer_string_ascii_whitespace((unsigned char)text[value_end - 1U])) {
        --value_end;
    }

    parse_result = mylite_execution_parse_signed_integer_text(
        text + value_start,
        value_end - value_start,
        &magnitude,
        &is_negative
    );
    if (parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID && value_start == value_end) {
        parse_result = MYLITE_EXECUTION_DML_NUMERIC_PARSE_OK;
        magnitude = 0U;
        is_negative = false;
    }
    free(text);

    if (parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID) {
        set_unsupported_error(
            database,
            "WHERE scalar literal predicates support only integer, boolean, NULL, and exact "
            "quoted integer literals"
        );
        return MYLITE_ERROR;
    }
    if (parse_result != MYLITE_EXECUTION_DML_NUMERIC_PARSE_OK ||
        (!is_negative && magnitude > signed_positive_max) ||
        (is_negative && magnitude > signed_negative_abs_max)) {
        set_unsupported_error(
            database,
            "WHERE scalar literal integer literals must fit the signed 64-bit range"
        );
        return MYLITE_ERROR;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){.is_null = false, .is_text = false, .integer = 0};
    if (is_negative && magnitude == signed_negative_abs_max) {
        out_value->integer = INT64_MIN;
    } else if (is_negative) {
        out_value->integer = -(int64_t)magnitude;
    } else {
        out_value->integer = (int64_t)magnitude;
    }
    return MYLITE_OK;
}

static int plan_where_scalar_decimal_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal =
        mylite_execution_unwrap_parenthesized_expression(expression);
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = decode_sql_string_literal(
            database,
            literal,
            "WHERE decimal cast predicates support only decimal string literals",
            "WHERE decimal cast predicate literals do not support NUL bytes",
            &text,
            &text_length
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (!where_decimal_string_literal_is_supported(text, text_length)) {
            free(text);
            set_unsupported_error(
                database,
                "WHERE decimal cast predicates support only finite decimal string literals"
            );
            return MYLITE_ERROR;
        }
        return assign_text_value(database, text, text_length, out_value);
    }
    if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_DECIMAL) {
        if (!where_decimal_span_is_supported(&literal->span)) {
            set_unsupported_error(
                database,
                "WHERE decimal cast predicates support only finite decimal literals"
            );
            return MYLITE_ERROR;
        }
        text = (char *)malloc(literal->span.length + 1U);
        if (text == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        memcpy(text, literal->span.text, literal->span.length);
        text[literal->span.length] = '\0';
        return assign_text_value(database, text, literal->span.length, out_value);
    }

    return plan_where_scalar_literal_value(database, expression, out_value);
}

static bool where_decimal_string_literal_is_supported(const char *text, size_t text_length) {
    struct mylite_sql_source_span span = {
        .text = text,
        .length = text_length,
    };

    return where_decimal_span_is_supported(&span);
}

static bool where_decimal_span_is_supported(const struct mylite_sql_source_span *span) {
    size_t index = 0U;
    size_t end = 0U;
    bool saw_digit = false;
    bool saw_decimal_point = false;

    if (span == NULL || span->text == NULL) {
        return false;
    }

    end = span->length;
    while (index < end &&
           predicate_integer_string_ascii_whitespace((unsigned char)span->text[index])) {
        ++index;
    }
    while (end > index &&
           predicate_integer_string_ascii_whitespace((unsigned char)span->text[end - 1U])) {
        --end;
    }
    if (index < end && (span->text[index] == '+' || span->text[index] == '-')) {
        ++index;
    }
    while (index < end) {
        unsigned char byte = (unsigned char)span->text[index];

        if (byte >= '0' && byte <= '9') {
            saw_digit = true;
        } else if (byte == '.' && !saw_decimal_point) {
            saw_decimal_point = true;
        } else {
            return false;
        }
        ++index;
    }
    return saw_digit;
}

static bool predicate_row_scalar_cast_expression_decimal_scale(
    const struct mylite_sql_ast_node *expression,
    size_t *out_scale
) {
    struct scalar_placeholder_cast_info info = {0};
    bool convert_syntax = false;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || (expression->kind != MYLITE_SQL_AST_CAST_CHAR_EXPRESSION &&
                               expression->kind != MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION)) {
        return false;
    }

    convert_syntax = expression->kind == MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION;
    if (parse_scalar_placeholder_cast_info(
            expression,
            SCALAR_PLACEHOLDER_CAST_TEXT,
            convert_syntax,
            &info
        ) != MYLITE_OK) {
        return false;
    }
    if (info.target != SCALAR_PLACEHOLDER_CAST_DECIMAL) {
        return false;
    }
    if (out_scale != NULL) {
        *out_scale = info.has_decimal_scale ? info.decimal_scale : decimal_default_scale;
    }
    return true;
}

static bool predicate_row_scalar_cast_expression_is_binary(
    const struct mylite_sql_ast_node *expression
) {
    enum planned_row_scalar_conversion_kind kind =
        row_scalar_conversion_kind_from_expression(expression);

    return kind == PLANNED_ROW_SCALAR_CONVERSION_BINARY ||
           kind == PLANNED_ROW_SCALAR_CONVERSION_USING_BINARY;
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
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
    case MYLITE_SQL_AST_LITERAL_NULL:
        return true;
    default:
        return false;
    }
}

static bool predicate_node_is_supported_row_scalar_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    const struct mylite_sql_ast_node *node = unwrap_parenthesized_predicate(predicate_node);

    if (node == NULL) {
        return false;
    }
    if (is_row_scalar_control_flow_expression(node) ||
        row_scalar_expression_is_string_function(node->kind) ||
        is_row_scalar_json_expression(node) || row_scalar_expression_is_rand_function(node) ||
        row_scalar_numeric_function_kind_is_supported(node->kind) ||
        row_scalar_conversion_kind_from_expression(node) != PLANNED_ROW_SCALAR_CONVERSION_NONE ||
        node->kind == MYLITE_SQL_AST_COLLATE_EXPRESSION) {
        return true;
    }

    switch (node->kind) {
    case MYLITE_SQL_AST_CONCAT_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_WS_FUNCTION:
    case MYLITE_SQL_AST_FIELD_FUNCTION:
    case MYLITE_SQL_AST_GREATEST_FUNCTION:
    case MYLITE_SQL_AST_LEAST_FUNCTION:
    case MYLITE_SQL_AST_INTERVAL_FUNCTION:
    case MYLITE_SQL_AST_HEX_FUNCTION:
    case MYLITE_SQL_AST_UNHEX_FUNCTION:
    case MYLITE_SQL_AST_TO_BASE64_FUNCTION:
    case MYLITE_SQL_AST_FROM_BASE64_FUNCTION:
    case MYLITE_SQL_AST_UUID_FUNCTION:
    case MYLITE_SQL_AST_IS_UUID_FUNCTION:
    case MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION:
    case MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION:
    case MYLITE_SQL_AST_CHAR_FUNCTION:
    case MYLITE_SQL_AST_DATE_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_TIME_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_STR_TO_DATE_FUNCTION:
    case MYLITE_SQL_AST_DATEDIFF_FUNCTION:
    case MYLITE_SQL_AST_TIMEDIFF_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMPDIFF_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMP_FUNCTION:
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION:
    case MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION:
    case MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION:
    case MYLITE_SQL_AST_FROM_DAYS_FUNCTION:
    case MYLITE_SQL_AST_MAKEDATE_FUNCTION:
    case MYLITE_SQL_AST_MAKETIME_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_ADD_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_DIFF_FUNCTION:
    case MYLITE_SQL_AST_CONVERT_TZ_FUNCTION:
        return true;
    default:
        break;
    }
    if (mylite_execution_is_date_interval_second_function_kind(node->kind) ||
        mylite_execution_is_temporal_extract_function_kind(node->kind) ||
        mylite_execution_is_charset_collation_function_kind(node->kind)) {
        return true;
    }

    return false;
}

static bool predicate_node_is_supported_row_scalar_predicate_value_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    const struct mylite_sql_ast_node *node = unwrap_parenthesized_predicate(predicate_node);

    if (node == NULL) {
        return false;
    }
    if (predicate_node_is_column_reference(node)) {
        return true;
    }
    return predicate_node_is_supported_row_scalar_expression(node);
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
    return (predicate_node != NULL && predicate_node->kind == MYLITE_SQL_AST_FIND_IN_SET_FUNCTION
           ) != 0;
}

static bool predicate_node_is_json_valid_expression(const struct mylite_sql_ast_node *predicate_node
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
    return (predicate_node != NULL && predicate_node->kind == MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION
           ) != 0;
}

static bool predicate_node_is_string_length_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    return (predicate_node != NULL &&
            mylite_execution_is_string_length_function_kind(predicate_node->kind)) != 0;
}

static bool predicate_node_is_substring_expression(const struct mylite_sql_ast_node *predicate_node
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
