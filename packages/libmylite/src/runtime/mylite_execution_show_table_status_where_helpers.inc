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
    const char *left = NULL;
    size_t column_index = 0U;
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate);
    int rc =
        show_table_status_where_column_value(database, left_node, values, &left, &column_index);
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
        (void)column_index;
        return evaluate_show_metadata_where_regexp_predicate(
            database,
            left,
            right_node,
            &regexp_messages,
            show_table_status_where_column_is_case_sensitive(column_index),
            out_truth
        );
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
        char *pattern = NULL;

        if (left == NULL) {
            *out_truth = SHOW_VARIABLES_WHERE_UNKNOWN;
            return MYLITE_OK;
        }
        rc = decode_show_table_status_where_string_literal(database, right_node, &pattern);
        if (rc == MYLITE_OK) {
            bool matches = show_like_pattern_matches(
                pattern,
                strlen(pattern),
                left,
                strlen(left),
                show_table_status_where_column_is_case_sensitive(column_index)
            );

            if (matches) {
                *out_truth = SHOW_VARIABLES_WHERE_TRUE;
            } else {
                *out_truth = SHOW_VARIABLES_WHERE_FALSE;
            }
        }
        free(pattern);
        return rc;
    }

    return compare_show_table_status_where_literal(
        database,
        operator_kind,
        left,
        column_index,
        right_node,
        out_truth
    );
}

static int evaluate_show_table_status_where_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_table_status_result_column_count],
    enum show_variables_where_truth *out_truth
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate, 0U);
    const struct mylite_sql_ast_node *value_list = child_at(predicate, 1U);
    const char *left = NULL;
    size_t column_index = 0U;
    size_t value_count = 0U;
    bool saw_null = false;
    int rc =
        show_table_status_where_column_value(database, left_node, values, &left, &column_index);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (left == NULL) {
        *out_truth = SHOW_VARIABLES_WHERE_UNKNOWN;
        return MYLITE_OK;
    }
    if (value_list == NULL || value_list->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST) {
        set_runtime_error(database, "invalid SHOW TABLE STATUS WHERE IN list");
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
                "SHOW TABLE STATUS WHERE IN supports only string and NULL literals"
            );
            return MYLITE_ERROR;
        }

        literal_kind = mylite_sql_ast_node_literal_kind(value_node);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            saw_null = true;
            continue;
        }
        rc = decode_show_table_status_where_string_literal(database, value_node, &right);
        if (rc == MYLITE_OK) {
            bool equal = false;

            rc = show_table_status_where_texts_are_equal(
                database,
                left,
                right,
                column_index,
                &equal
            );
            if (rc == MYLITE_OK && equal) {
                *out_truth = SHOW_VARIABLES_WHERE_TRUE;
                free(right);
                return MYLITE_OK;
            }
        }
        free(right);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    if (saw_null) {
        *out_truth = SHOW_VARIABLES_WHERE_UNKNOWN;
    } else {
        *out_truth = SHOW_VARIABLES_WHERE_FALSE;
    }
    return MYLITE_OK;
}

static int evaluate_show_table_status_where_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_table_status_result_column_count],
    enum show_variables_where_truth *out_truth
) {
    const char *value = NULL;
    size_t column_index = 0U;
    int rc = show_table_status_where_column_value(
        database,
        child_at(predicate, 0U),
        values,
        &value,
        &column_index
    );

    (void)column_index;
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_sql_ast_node_operator(predicate) == MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL) {
        *out_truth = value == NULL ? SHOW_VARIABLES_WHERE_FALSE : SHOW_VARIABLES_WHERE_TRUE;
    } else {
        *out_truth = value == NULL ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
    }
    return MYLITE_OK;
}

static int show_table_status_where_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const char *const values[show_table_status_result_column_count],
    const char **out_value,
    size_t *out_column_index
) {
    size_t column_index = 0U;
    int rc = resolve_show_table_status_where_column(database, column_node, &column_index);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (values == NULL || out_value == NULL || out_column_index == NULL) {
        set_runtime_error(database, "invalid SHOW TABLE STATUS WHERE row");
        return MYLITE_ERROR;
    }

    *out_value = values[column_index];
    *out_column_index = column_index;
    return MYLITE_OK;
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
    const char *left,
    size_t column_index,
    const struct mylite_sql_ast_node *right,
    enum show_variables_where_truth *out_truth
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *right_text = NULL;
    int comparison = 0;
    int rc = MYLITE_OK;

    if (right == NULL || out_truth == NULL) {
        set_runtime_error(database, "invalid SHOW TABLE STATUS WHERE comparison");
        return MYLITE_ERROR;
    }
    if (right->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "SHOW TABLE STATUS WHERE comparisons support only string and NULL literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(right);
    if (compare_show_table_status_where_null_operand(
            operator_kind,
            left,
            literal_kind,
            out_truth
        )) {
        return MYLITE_OK;
    }

    rc = decode_show_table_status_where_string_literal(database, right, &right_text);
    if (rc != MYLITE_OK) {
        free(right_text);
        return rc;
    }

    rc =
        show_table_status_where_compare_text(database, left, right_text, column_index, &comparison);
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

static int show_table_status_where_compare_text(
    struct mylite_db *database,
    const char *left,
    const char *right,
    size_t column_index,
    int *out_comparison
) {
    if (out_comparison == NULL) {
        return MYLITE_MISUSE;
    }
    *out_comparison = 0;
    if (show_table_status_where_column_is_numeric(column_index)) {
        return show_table_status_where_compare_numeric_text(database, left, right, out_comparison);
    }

    *out_comparison = compare_show_table_status_where_text(left, right, column_index);
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
    const char *left,
    const char *right,
    size_t column_index,
    bool *out_equal
) {
    int comparison = 0;
    int rc = MYLITE_OK;

    if (out_equal == NULL) {
        return MYLITE_MISUSE;
    }
    *out_equal = false;
    rc = show_table_status_where_compare_text(database, left, right, column_index, &comparison);
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
    size_t column_index
) {
    if (show_table_status_where_column_is_case_sensitive(column_index)) {
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
