static int show_table_status_where_clause_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const char *const values[show_table_status_result_column_count],
    bool *out_matches
) {
    enum show_variables_where_truth truth = SHOW_VARIABLES_WHERE_FALSE;
    int rc = MYLITE_OK;

    if (where_clause == NULL || where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE ||
        values == NULL || out_matches == NULL) {
        set_runtime_error(database, "invalid SHOW TABLE STATUS WHERE predicate");
        return MYLITE_ERROR;
    }

    *out_matches = false;
    rc = evaluate_show_table_status_where_predicate(
        database,
        child_at(where_clause, 0U),
        values,
        &truth
    );
    if (rc == MYLITE_OK) {
        *out_matches = truth == SHOW_VARIABLES_WHERE_TRUE;
    }
    return rc;
}

static int evaluate_show_table_status_where_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_table_status_result_column_count],
    enum show_variables_where_truth *out_truth
) {
    struct show_variables_where_frame_stack frame_stack = {0};
    struct show_variables_where_truth_stack truth_stack = {0};
    struct show_variables_where_eval_frame frame = {
        .node = predicate,
        .action = SHOW_VARIABLES_WHERE_VISIT,
    };
    int rc = MYLITE_OK;

    if (predicate == NULL || values == NULL || out_truth == NULL) {
        set_runtime_error(database, "invalid SHOW TABLE STATUS WHERE predicate");
        return MYLITE_ERROR;
    }

    rc = show_variables_where_frame_stack_push(database, &frame_stack, frame);
    while (rc == MYLITE_OK && show_variables_where_frame_stack_pop(&frame_stack, &frame)) {
        const struct mylite_sql_ast_node *current = unwrap_parenthesized_predicate(frame.node);

        if (current == NULL) {
            set_runtime_error(database, "invalid SHOW TABLE STATUS WHERE predicate");
            rc = MYLITE_ERROR;
        } else if (frame.action == SHOW_VARIABLES_WHERE_VISIT) {
            rc = visit_show_table_status_where_predicate(database, &frame_stack, current);
        } else {
            rc = evaluate_show_table_status_where_frame(database, current, values, &truth_stack);
        }
    }

    if (rc == MYLITE_OK) {
        if (truth_stack.count != 1U ||
            !show_variables_where_truth_stack_pop(&truth_stack, out_truth)) {
            set_runtime_error(database, "invalid SHOW TABLE STATUS WHERE predicate state");
            rc = MYLITE_ERROR;
        }
    }

    show_variables_where_truth_stack_deinit(&truth_stack);
    show_variables_where_frame_stack_deinit(&frame_stack);
    return rc;
}

static int visit_show_table_status_where_predicate(
    struct mylite_db *database,
    struct show_variables_where_frame_stack *frame_stack,
    const struct mylite_sql_ast_node *predicate
) {
    int rc = show_variables_where_frame_stack_push(
        database,
        frame_stack,
        (struct show_variables_where_eval_frame){
            .node = predicate,
            .action = SHOW_VARIABLES_WHERE_EVALUATE,
        }
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (predicate->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        return show_variables_where_frame_stack_push(
            database,
            frame_stack,
            (struct show_variables_where_eval_frame){
                .node = child_at(predicate, 0U),
                .action = SHOW_VARIABLES_WHERE_VISIT,
            }
        );
    }
    if (predicate->kind == MYLITE_SQL_AST_AND_PREDICATE ||
        predicate->kind == MYLITE_SQL_AST_OR_PREDICATE) {
        rc = show_variables_where_frame_stack_push(
            database,
            frame_stack,
            (struct show_variables_where_eval_frame){
                .node = child_at(predicate, 1U),
                .action = SHOW_VARIABLES_WHERE_VISIT,
            }
        );
        if (rc == MYLITE_OK) {
            rc = show_variables_where_frame_stack_push(
                database,
                frame_stack,
                (struct show_variables_where_eval_frame){
                    .node = child_at(predicate, 0U),
                    .action = SHOW_VARIABLES_WHERE_VISIT,
                }
            );
        }
    }
    return rc;
}

static int evaluate_show_table_status_where_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_table_status_result_column_count],
    struct show_variables_where_truth_stack *truth_stack
) {
    enum show_variables_where_truth truth = SHOW_VARIABLES_WHERE_FALSE;
    int rc = MYLITE_OK;

    if (predicate->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        rc = evaluate_show_table_status_where_comparison_predicate(
            database,
            predicate,
            values,
            &truth
        );
    } else if (predicate->kind == MYLITE_SQL_AST_IN_PREDICATE) {
        rc = evaluate_show_table_status_where_in_predicate(database, predicate, values, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        rc =
            evaluate_show_table_status_where_is_null_predicate(database, predicate, values, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        enum show_variables_where_truth child_truth = SHOW_VARIABLES_WHERE_FALSE;

        if (!show_variables_where_truth_stack_pop(truth_stack, &child_truth)) {
            set_runtime_error(database, "invalid SHOW TABLE STATUS WHERE predicate stack");
            return MYLITE_ERROR;
        }
        truth = negate_show_variables_where_truth(child_truth);
    } else if (
        predicate->kind == MYLITE_SQL_AST_AND_PREDICATE ||
        predicate->kind == MYLITE_SQL_AST_OR_PREDICATE
    ) {
        rc = evaluate_show_variables_where_logical_frame(database, predicate, truth_stack, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_XOR_PREDICATE) {
        set_unsupported_error(database, "SHOW TABLE STATUS WHERE does not support XOR predicates");
        return MYLITE_ERROR;
    } else {
        set_unsupported_error(
            database,
            "SHOW TABLE STATUS WHERE supports output-column predicates"
        );
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = show_variables_where_truth_stack_push(database, truth_stack, truth);
    }
    return rc;
}

static int evaluate_show_table_status_where_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_table_status_result_column_count],
    enum show_variables_where_truth *out_truth
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate, 0U);
    const struct mylite_sql_ast_node *right_node = child_at(predicate, 1U);
    struct show_table_status_where_operand left = {0};
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate);
    int rc = show_table_status_where_operand_value(database, left_node, values, &left);
    static const struct show_metadata_regexp_messages regexp_messages = {
        .literal = "SHOW TABLE STATUS WHERE REGEXP predicates support only string pattern literals",
        .nul = "SHOW TABLE STATUS WHERE REGEXP pattern literals do not support NUL bytes",
        .ascii = "SHOW TABLE STATUS WHERE REGEXP pattern literals support only ASCII text",
        .unsupported =
            "SHOW TABLE STATUS WHERE REGEXP patterns support only MyLite's baseline ASCII regular "
            "expression subset",
    };

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_REGEXP ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_RLIKE) {
        rc = evaluate_show_metadata_where_regexp_predicate(
            database,
            left.value,
            right_node,
            &regexp_messages,
            left.case_sensitive,
            out_truth
        );
        show_table_status_where_operand_deinit(&left);
        return rc;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
        char *pattern = NULL;

        if (left.value == NULL) {
            *out_truth = SHOW_VARIABLES_WHERE_UNKNOWN;
            show_table_status_where_operand_deinit(&left);
            return MYLITE_OK;
        }
        rc = decode_show_table_status_where_string_literal(database, right_node, &pattern);
        if (rc == MYLITE_OK) {
            bool matches = show_like_pattern_matches(
                pattern,
                strlen(pattern),
                left.value,
                strlen(left.value),
                left.case_sensitive
            );

            if (matches) {
                *out_truth = SHOW_VARIABLES_WHERE_TRUE;
            } else {
                *out_truth = SHOW_VARIABLES_WHERE_FALSE;
            }
        }
        free(pattern);
        show_table_status_where_operand_deinit(&left);
        return rc;
    }

    rc = compare_show_table_status_where_literal(
        database,
        operator_kind,
        &left,
        right_node,
        out_truth
    );
    show_table_status_where_operand_deinit(&left);
    return rc;
}

static int evaluate_show_table_status_where_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_table_status_result_column_count],
    enum show_variables_where_truth *out_truth
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate, 0U);
    const struct mylite_sql_ast_node *value_list = child_at(predicate, 1U);
    struct show_table_status_where_operand left = {0};
    size_t value_count = 0U;
    bool saw_null = false;
    int rc = show_table_status_where_operand_value(database, left_node, values, &left);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (left.value == NULL) {
        *out_truth = SHOW_VARIABLES_WHERE_UNKNOWN;
        show_table_status_where_operand_deinit(&left);
        return MYLITE_OK;
    }
    if (value_list == NULL || value_list->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST) {
        set_runtime_error(database, "invalid SHOW TABLE STATUS WHERE IN list");
        show_table_status_where_operand_deinit(&left);
        return MYLITE_ERROR;
    }

    value_count = mylite_sql_ast_node_child_count(value_list);
    for (size_t index = 0U; index < value_count; ++index) {
        const struct mylite_sql_ast_node *value_node = child_at(value_list, index);
        enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
        char *right = NULL;

        if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL) {
            set_unsupported_error(
                database,
                "SHOW TABLE STATUS WHERE IN supports only string, unsigned integer, and NULL "
                "literals"
            );
            show_table_status_where_operand_deinit(&left);
            return MYLITE_ERROR;
        }

        literal_kind = mylite_sql_ast_node_literal_kind(value_node);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            saw_null = true;
            continue;
        }
        rc = decode_show_table_status_where_comparable_literal(
            database,
            value_node,
            left.numeric,
            "SHOW TABLE STATUS WHERE IN supports only string, unsigned integer, and NULL literals",
            &right
        );
        if (rc == MYLITE_OK) {
            bool equal = false;

            rc = show_table_status_where_texts_are_equal(
                database,
                &left,
                right,
                &equal
            );
            if (rc == MYLITE_OK && equal) {
                *out_truth = SHOW_VARIABLES_WHERE_TRUE;
                free(right);
                show_table_status_where_operand_deinit(&left);
                return MYLITE_OK;
            }
        }
        free(right);
        if (rc != MYLITE_OK) {
            show_table_status_where_operand_deinit(&left);
            return rc;
        }
    }

    if (saw_null) {
        *out_truth = SHOW_VARIABLES_WHERE_UNKNOWN;
    } else {
        *out_truth = SHOW_VARIABLES_WHERE_FALSE;
    }
    show_table_status_where_operand_deinit(&left);
    return MYLITE_OK;
}

static int evaluate_show_table_status_where_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_table_status_result_column_count],
    enum show_variables_where_truth *out_truth
) {
    struct show_table_status_where_operand value = {0};
    int rc = show_table_status_where_operand_value(
        database,
        child_at(predicate, 0U),
        values,
        &value
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_sql_ast_node_operator(predicate) == MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL) {
        *out_truth = value.value == NULL ? SHOW_VARIABLES_WHERE_FALSE : SHOW_VARIABLES_WHERE_TRUE;
    } else {
        *out_truth = value.value == NULL ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
    }
    show_table_status_where_operand_deinit(&value);
    return MYLITE_OK;
}

static int show_table_status_where_operand_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const char *const values[show_table_status_result_column_count],
    struct show_table_status_where_operand *out_operand
) {
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    if (out_operand == NULL) {
        return MYLITE_MISUSE;
    }
    *out_operand = (struct show_table_status_where_operand){0};
    if (values == NULL) {
        set_runtime_error(database, "invalid SHOW TABLE STATUS WHERE row");
        return MYLITE_ERROR;
    }

    operand_node = unwrap_parenthesized_expression(operand_node);
    if (operand_node != NULL &&
        mylite_execution_string_slice_function_kind(operand_node->kind) ==
            PLANNED_STRING_SLICE_FUNCTION_SUBSTRING) {
        return show_table_status_where_substring_operand_value(
            database,
            operand_node,
            values,
            out_operand
        );
    }

    rc = resolve_show_table_status_where_column(database, operand_node, &column_index);
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_operand->value = values[column_index];
    out_operand->case_sensitive = show_table_status_where_column_is_case_sensitive(column_index);
    out_operand->numeric = show_table_status_where_column_is_numeric(column_index);
    return MYLITE_OK;
}

static void show_table_status_where_operand_deinit(
    struct show_table_status_where_operand *operand
) {
    if (operand != NULL) {
        mylite_execution_session_scalar_cell_deinit(&operand->cell);
        *operand = (struct show_table_status_where_operand){0};
    }
}

static int show_table_status_where_substring_operand_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const char *const values[show_table_status_result_column_count],
    struct show_table_status_where_operand *out_operand
) {
    const struct mylite_sql_ast_node *source_node = NULL;
    const struct mylite_sql_ast_node *position_node = NULL;
    const struct mylite_sql_ast_node *length_node = NULL;
    const char *source_value = NULL;
    size_t source_column_index = 0U;
    size_t argument_count = 0U;
    int64_t position = 0;
    int64_t requested_length = 0;
    bool position_is_null = false;
    bool length_is_null = false;
    bool has_length = false;
    int rc = MYLITE_OK;

    if (operand_node == NULL || values == NULL || out_operand == NULL) {
        set_runtime_error(database, "invalid SHOW TABLE STATUS WHERE substring operand");
        return MYLITE_ERROR;
    }

    argument_count = mylite_sql_ast_node_child_count(operand_node);
    if (argument_count != 2U && argument_count != 3U) {
        set_unsupported_error(
            database,
            "SHOW TABLE STATUS WHERE SUBSTRING operands need two or three arguments"
        );
        return MYLITE_ERROR;
    }

    source_node = child_at(operand_node, 0U);
    position_node = child_at(operand_node, 1U);
    if (argument_count == 3U) {
        has_length = true;
        length_node = child_at(operand_node, 2U);
    }

    rc = resolve_show_table_status_where_column(database, source_node, &source_column_index);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!mylite_execution_string_slice_length_argument_is_admitted(position_node)) {
        set_unsupported_error(
            database,
            "string slice functions support only integer, boolean, and NULL position literals"
        );
        return MYLITE_ERROR;
    }
    if (has_length && !mylite_execution_string_slice_length_argument_is_admitted(length_node)) {
        set_unsupported_error(
            database,
            "string slice functions support only integer, boolean, and NULL length literals"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_execution_string_slice_signed_position_value(
        database,
        position_node,
        &position,
        &position_is_null
    );
    if (rc == MYLITE_OK && has_length) {
        rc = mylite_execution_string_slice_signed_length_value(
            database,
            length_node,
            &requested_length,
            &length_is_null
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    source_value = values[source_column_index];
    if (source_value == NULL || position_is_null || length_is_null) {
        out_operand->value = NULL;
        out_operand->case_sensitive =
            show_table_status_where_column_is_case_sensitive(source_column_index);
        out_operand->numeric = false;
        return MYLITE_OK;
    }

    rc = mylite_execution_string_slice_substring_text_value(
        database,
        source_value,
        strlen(source_value),
        position,
        has_length,
        requested_length,
        &out_operand->cell
    );
    if (rc == MYLITE_OK) {
        out_operand->value = out_operand->cell.value;
        out_operand->case_sensitive =
            show_table_status_where_column_is_case_sensitive(source_column_index);
        out_operand->numeric = false;
    }
    return rc;
}

static int resolve_show_table_status_where_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    size_t *out_column_index
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *display_text = NULL;
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (out_column_index == NULL) {
        return MYLITE_MISUSE;
    }
    *out_column_index = 0U;
    if (column_node == NULL || (column_node->kind != MYLITE_SQL_AST_IDENTIFIER &&
                                column_node->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        set_unsupported_error(database, "SHOW TABLE STATUS WHERE supports only output columns");
        return MYLITE_ERROR;
    }

    rc = collect_identifier_parts(
        column_node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U) {
        for (size_t column_index = 0U; column_index < show_table_status_result_column_count;
             ++column_index) {
            if (text_equals_ascii_case_insensitive(
                    parts[0],
                    show_table_status_result_columns[column_index]
                )) {
                *out_column_index = column_index;
                return MYLITE_OK;
            }
        }
    }

    rc = show_variables_column_reference_text(database, column_node, &display_text);
    if (rc == MYLITE_OK) {
        set_unknown_where_column_error(database, display_text);
        rc = MYLITE_ERROR;
    }
    free(display_text);
    return rc;
}

static int compare_show_table_status_where_literal(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const struct show_table_status_where_operand *left,
    const struct mylite_sql_ast_node *right,
    enum show_variables_where_truth *out_truth
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *right_text = NULL;
    int comparison = 0;
    int rc = MYLITE_OK;

    if (left == NULL || right == NULL || out_truth == NULL) {
        set_runtime_error(database, "invalid SHOW TABLE STATUS WHERE comparison");
        return MYLITE_ERROR;
    }
    if (right->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "SHOW TABLE STATUS WHERE comparisons support only string, unsigned integer, and NULL "
            "literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(right);
    if (compare_show_table_status_where_null_operand(
            operator_kind,
            left->value,
            literal_kind,
            out_truth
        )) {
        return MYLITE_OK;
    }

    rc = decode_show_table_status_where_comparable_literal(
        database,
        right,
        left->numeric,
        "SHOW TABLE STATUS WHERE comparisons support only string, unsigned integer, and NULL "
        "literals",
        &right_text
    );
    if (rc != MYLITE_OK) {
        free(right_text);
        return rc;
    }

    if (left->value == NULL) {
        *out_truth = SHOW_VARIABLES_WHERE_UNKNOWN;
        free(right_text);
        return MYLITE_OK;
    }
    rc = show_table_status_where_compare_text(database, left, right_text, &comparison);
    if (rc == MYLITE_OK) {
        rc = show_table_status_where_truth_from_comparison(
            database,
            operator_kind,
            (struct show_table_status_where_comparison){.value = comparison},
            out_truth
        );
    }

    free(right_text);
    return rc;
}

static int decode_show_table_status_where_comparable_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    bool left_is_numeric,
    const char *unsupported_message,
    char **out_text
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t value = 0U;

    if (literal_node == NULL || out_text == NULL) {
        set_runtime_error(database, "invalid SHOW TABLE STATUS WHERE literal");
        return MYLITE_ERROR;
    }

    *out_text = NULL;
    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        return decode_show_table_status_where_string_literal(database, literal_node, out_text);
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (!left_is_numeric) {
        set_unsupported_error(
            database,
            "SHOW TABLE STATUS WHERE integer literal predicates support only numeric columns"
        );
        return MYLITE_ERROR;
    }
    if (parse_unsigned_integer_literal(&literal_node->span, &value) != MYLITE_OK) {
        set_unsupported_error(
            database,
            "SHOW TABLE STATUS WHERE numeric columns support only unsigned decimal predicates"
        );
        return MYLITE_ERROR;
    }
    return copy_source_span_text(database, &literal_node->span, out_text);
}

static int show_table_status_where_compare_text(
    struct mylite_db *database,
    const struct show_table_status_where_operand *left,
    const char *right,
    int *out_comparison
) {
    if (out_comparison == NULL) {
        return MYLITE_MISUSE;
    }
    *out_comparison = 0;
    if (left == NULL || left->value == NULL) {
        return MYLITE_MISUSE;
    }
    if (left->numeric) {
        return show_table_status_where_compare_numeric_text(
            database,
            left->value,
            right,
            out_comparison
        );
    }

    *out_comparison =
        compare_show_table_status_where_text(left->value, right, left->case_sensitive);
    return MYLITE_OK;
}

static int show_table_status_where_compare_numeric_text(
    struct mylite_db *database,
    const char *left,
    const char *right,
    int *out_comparison
) {
    uint64_t left_value = 0U;
    uint64_t right_value = 0U;

    if (!parse_show_table_status_where_unsigned_text(left, &left_value) ||
        !parse_show_table_status_where_unsigned_text(right, &right_value)) {
        set_unsupported_error(
            database,
            "SHOW TABLE STATUS WHERE numeric columns support only unsigned decimal string "
            "predicates"
        );
        return MYLITE_ERROR;
    }

    if (left_value < right_value) {
        *out_comparison = -1;
    } else if (left_value > right_value) {
        *out_comparison = 1;
    } else {
        *out_comparison = 0;
    }
    return MYLITE_OK;
}

static int show_table_status_where_texts_are_equal(
    struct mylite_db *database,
    const struct show_table_status_where_operand *left,
    const char *right,
    bool *out_equal
) {
    int comparison = 0;
    int rc = MYLITE_OK;

    if (out_equal == NULL) {
        return MYLITE_MISUSE;
    }
    *out_equal = false;
    rc = show_table_status_where_compare_text(database, left, right, &comparison);
    if (rc == MYLITE_OK) {
        *out_equal = comparison == 0;
    }
    return rc;
}

static int show_table_status_where_truth_from_comparison(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    struct show_table_status_where_comparison comparison,
    enum show_variables_where_truth *out_truth
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        *out_truth = comparison.value == 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        *out_truth = comparison.value != 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_LESS:
        *out_truth = comparison.value < 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        *out_truth = comparison.value <= 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        *out_truth = comparison.value > 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        *out_truth = comparison.value >= 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        return MYLITE_OK;
    default:
        set_unsupported_error(
            database,
            "SHOW TABLE STATUS WHERE comparison operator is not supported"
        );
        return MYLITE_ERROR;
    }
}

static bool compare_show_table_status_where_null_operand(
    enum mylite_sql_ast_operator operator_kind,
    const char *left,
    enum mylite_sql_ast_literal_kind literal_kind,
    enum show_variables_where_truth *out_truth
) {
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
            *out_truth = left == NULL ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        } else {
            *out_truth = SHOW_VARIABLES_WHERE_UNKNOWN;
        }
        return true;
    }
    if (left == NULL) {
        *out_truth = operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL
                         ? SHOW_VARIABLES_WHERE_FALSE
                         : SHOW_VARIABLES_WHERE_UNKNOWN;
        return true;
    }
    return false;
}

static int decode_show_table_status_where_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_text
) {
    size_t text_length = 0U;

    return decode_sql_string_literal(
        database,
        literal_node,
        "SHOW TABLE STATUS WHERE supports only string literal predicates",
        "SHOW TABLE STATUS WHERE string literals do not support NUL bytes",
        out_text,
        &text_length
    );
}

static int compare_show_table_status_where_text(
    const char *left,
    const char *right,
    bool case_sensitive
) {
    if (case_sensitive) {
        return strcmp(left, right);
    }
    return compare_ascii_case_insensitive_text(left, right);
}

static bool parse_show_table_status_where_unsigned_text(const char *text, uint64_t *out_value) {
    uint64_t value = 0U;

    if (text == NULL || text[0] == '\0' || out_value == NULL) {
        return false;
    }
    for (size_t index = 0U; text[index] != '\0'; ++index) {
        uint64_t digit = 0U;

        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
        digit = (uint64_t)(text[index] - '0');
        if (value > (UINT64_MAX - digit) / decimal_base) {
            return false;
        }
        value = (value * decimal_base) + digit;
    }

    *out_value = value;
    return true;
}

static bool show_table_status_where_column_is_case_sensitive(size_t column_index) {
    return column_index == show_table_status_name_column;
}

static bool show_table_status_where_column_is_numeric(size_t column_index) {
    return (column_index == show_table_status_version_column ||
            column_index == show_table_status_rows_column ||
            column_index == show_table_status_average_row_length_column ||
            column_index == show_table_status_data_length_column ||
            column_index == show_table_status_max_data_length_column ||
            column_index == show_table_status_index_length_column ||
            column_index == show_table_status_data_free_column ||
            column_index == show_table_status_auto_increment_column ||
            column_index == show_table_status_checksum_column) != 0;
}
