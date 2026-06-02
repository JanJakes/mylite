static int validate_show_index_where_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause
) {
    static const char *const validation_values[show_index_result_column_count] = {
        "table_name",
        "1",
        "key_name",
        "1",
        "column_name",
        "A",
        "0",
        "3",
        NULL,
        "YES",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    bool matches = false;

    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    return show_index_where_clause_matches(database, where_clause, validation_values, &matches);
}

static int show_index_where_clause_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const char *const values[show_index_result_column_count],
    bool *out_matches
) {
    enum show_variables_where_truth truth = SHOW_VARIABLES_WHERE_FALSE;
    int rc = MYLITE_OK;

    if (where_clause == NULL || where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE ||
        values == NULL || out_matches == NULL) {
        set_runtime_error(database, "invalid SHOW INDEX WHERE predicate");
        return MYLITE_ERROR;
    }

    *out_matches = false;
    rc = evaluate_show_index_where_predicate(database, child_at(where_clause, 0U), values, &truth);
    if (rc == MYLITE_OK) {
        *out_matches = truth == SHOW_VARIABLES_WHERE_TRUE;
    }
    return rc;
}

static int evaluate_show_index_where_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_index_result_column_count],
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
        set_runtime_error(database, "invalid SHOW INDEX WHERE predicate");
        return MYLITE_ERROR;
    }

    rc = show_variables_where_frame_stack_push(database, &frame_stack, frame);
    while (rc == MYLITE_OK && show_variables_where_frame_stack_pop(&frame_stack, &frame)) {
        const struct mylite_sql_ast_node *current = unwrap_parenthesized_predicate(frame.node);

        if (current == NULL) {
            set_runtime_error(database, "invalid SHOW INDEX WHERE predicate");
            rc = MYLITE_ERROR;
        } else if (frame.action == SHOW_VARIABLES_WHERE_VISIT) {
            rc = visit_show_index_where_predicate(database, &frame_stack, current);
        } else {
            rc = evaluate_show_index_where_frame(database, current, values, &truth_stack);
        }
    }

    if (rc == MYLITE_OK) {
        if (truth_stack.count != 1U ||
            !show_variables_where_truth_stack_pop(&truth_stack, out_truth)) {
            set_runtime_error(database, "invalid SHOW INDEX WHERE predicate state");
            rc = MYLITE_ERROR;
        }
    }

    show_variables_where_truth_stack_deinit(&truth_stack);
    show_variables_where_frame_stack_deinit(&frame_stack);
    return rc;
}

static int visit_show_index_where_predicate(
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

static int evaluate_show_index_where_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_index_result_column_count],
    struct show_variables_where_truth_stack *truth_stack
) {
    enum show_variables_where_truth truth = SHOW_VARIABLES_WHERE_FALSE;
    int rc = MYLITE_OK;

    if (predicate->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        rc = evaluate_show_index_where_comparison_predicate(database, predicate, values, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_IN_PREDICATE) {
        rc = evaluate_show_index_where_in_predicate(database, predicate, values, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        rc = evaluate_show_index_where_is_null_predicate(database, predicate, values, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        enum show_variables_where_truth child_truth = SHOW_VARIABLES_WHERE_FALSE;

        if (!show_variables_where_truth_stack_pop(truth_stack, &child_truth)) {
            set_runtime_error(database, "invalid SHOW INDEX WHERE predicate stack");
            return MYLITE_ERROR;
        }
        truth = negate_show_variables_where_truth(child_truth);
    } else if (
        predicate->kind == MYLITE_SQL_AST_AND_PREDICATE ||
        predicate->kind == MYLITE_SQL_AST_OR_PREDICATE
    ) {
        rc = evaluate_show_variables_where_logical_frame(database, predicate, truth_stack, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_XOR_PREDICATE) {
        set_unsupported_error(database, "SHOW INDEX WHERE does not support XOR predicates");
        return MYLITE_ERROR;
    } else {
        set_unsupported_error(database, "SHOW INDEX WHERE supports output-column predicates");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = show_variables_where_truth_stack_push(database, truth_stack, truth);
    }
    return rc;
}

static int evaluate_show_index_where_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_index_result_column_count],
    enum show_variables_where_truth *out_truth
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate, 0U);
    const struct mylite_sql_ast_node *right_node = child_at(predicate, 1U);
    const char *left = NULL;
    size_t column_index = 0U;
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate);
    int rc = show_index_where_column_value(database, left_node, values, &left, &column_index);
    static const struct show_metadata_regexp_messages regexp_messages = {
        .literal = "SHOW INDEX WHERE REGEXP predicates support only string pattern literals",
        .nul = "SHOW INDEX WHERE REGEXP pattern literals do not support NUL bytes",
        .ascii = "SHOW INDEX WHERE REGEXP pattern literals support only ASCII text",
        .unsupported =
            "SHOW INDEX WHERE REGEXP patterns support only MyLite's baseline ASCII regular "
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
            false,
            out_truth
        );
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
        char *pattern = NULL;

        rc = decode_show_index_where_string_literal(database, right_node, &pattern);
        if (rc == MYLITE_OK) {
            if (left == NULL) {
                *out_truth = SHOW_VARIABLES_WHERE_UNKNOWN;
            } else {
                bool matches =
                    show_like_pattern_matches(pattern, strlen(pattern), left, strlen(left), false);

                if (matches) {
                    *out_truth = SHOW_VARIABLES_WHERE_TRUE;
                } else {
                    *out_truth = SHOW_VARIABLES_WHERE_FALSE;
                }
            }
        }
        free(pattern);
        return rc;
    }

    return compare_show_index_where_literal(
        database,
        operator_kind,
        left,
        column_index,
        right_node,
        out_truth
    );
}

static int evaluate_show_index_where_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_index_result_column_count],
    enum show_variables_where_truth *out_truth
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate, 0U);
    const struct mylite_sql_ast_node *value_list = child_at(predicate, 1U);
    const char *left = NULL;
    size_t column_index = 0U;
    size_t value_count = 0U;
    bool saw_null = false;
    int rc = show_index_where_column_value(database, left_node, values, &left, &column_index);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value_list == NULL || value_list->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST) {
        set_runtime_error(database, "invalid SHOW INDEX WHERE IN list");
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
                "SHOW INDEX WHERE IN supports only string and NULL literals"
            );
            return MYLITE_ERROR;
        }

        literal_kind = mylite_sql_ast_node_literal_kind(value_node);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            saw_null = true;
            continue;
        }
        rc = decode_show_index_where_string_literal(database, value_node, &right);
        if (rc == MYLITE_OK) {
            bool equal = false;

            if (left != NULL) {
                rc = show_index_where_texts_are_equal(database, left, right, column_index, &equal);
            }
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

    if (left == NULL || saw_null) {
        *out_truth = SHOW_VARIABLES_WHERE_UNKNOWN;
    } else {
        *out_truth = SHOW_VARIABLES_WHERE_FALSE;
    }
    return MYLITE_OK;
}

static int evaluate_show_index_where_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_index_result_column_count],
    enum show_variables_where_truth *out_truth
) {
    const char *value = NULL;
    size_t column_index = 0U;
    int rc = show_index_where_column_value(
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

static int show_index_where_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const char *const values[show_index_result_column_count],
    const char **out_value,
    size_t *out_column_index
) {
    size_t column_index = 0U;
    int rc = resolve_show_index_where_column(database, column_node, &column_index);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (values == NULL || out_value == NULL || out_column_index == NULL) {
        set_runtime_error(database, "invalid SHOW INDEX WHERE row");
        return MYLITE_ERROR;
    }

    *out_value = values[column_index];
    *out_column_index = column_index;
    return MYLITE_OK;
}

static int resolve_show_index_where_column(
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
        set_unsupported_error(database, "SHOW INDEX WHERE supports only output columns");
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
        for (size_t column_index = 0U; column_index < show_index_result_column_count;
             ++column_index) {
            if (text_equals_ascii_case_insensitive(
                    parts[0],
                    show_index_result_columns[column_index]
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

static int compare_show_index_where_literal(
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
        set_runtime_error(database, "invalid SHOW INDEX WHERE comparison");
        return MYLITE_ERROR;
    }
    if (right->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "SHOW INDEX WHERE comparisons support only string and NULL literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(right);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL &&
        compare_show_index_where_null_operand(operator_kind, left, literal_kind, out_truth)) {
        return MYLITE_OK;
    }

    rc = decode_show_index_where_string_literal(database, right, &right_text);
    if (rc != MYLITE_OK) {
        free(right_text);
        return rc;
    }
    if (left == NULL &&
        compare_show_index_where_null_operand(operator_kind, left, literal_kind, out_truth)) {
        free(right_text);
        return MYLITE_OK;
    }

    rc = show_index_where_compare_text(database, left, right_text, column_index, &comparison);
    if (rc == MYLITE_OK) {
        rc = show_index_where_truth_from_comparison(
            database,
            operator_kind,
            (struct show_table_status_where_comparison){.value = comparison},
            out_truth
        );
    }

    free(right_text);
    return rc;
}

static int show_index_where_compare_text(
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
    if (show_index_where_column_is_numeric(column_index)) {
        return show_index_where_compare_numeric_text(database, left, right, out_comparison);
    }

    *out_comparison = compare_show_index_where_text(left, right);
    return MYLITE_OK;
}

static int show_index_where_compare_numeric_text(
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
            "SHOW INDEX WHERE numeric columns support only unsigned decimal string predicates"
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

static int show_index_where_texts_are_equal(
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
    rc = show_index_where_compare_text(database, left, right, column_index, &comparison);
    if (rc == MYLITE_OK) {
        *out_equal = comparison == 0;
    }
    return rc;
}

static int show_index_where_truth_from_comparison(
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
        set_unsupported_error(database, "SHOW INDEX WHERE comparison operator is not supported");
        return MYLITE_ERROR;
    }
}

static bool compare_show_index_where_null_operand(
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

static int decode_show_index_where_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_text
) {
    size_t text_length = 0U;

    return decode_sql_string_literal(
        database,
        literal_node,
        "SHOW INDEX WHERE supports only string literal predicates",
        "SHOW INDEX WHERE string literals do not support NUL bytes",
        out_text,
        &text_length
    );
}

static int evaluate_show_metadata_where_regexp_predicate(
    struct mylite_db *database,
    const char *left,
    const struct mylite_sql_ast_node *right,
    const struct show_metadata_regexp_messages *messages,
    bool case_sensitive,
    enum show_variables_where_truth *out_truth
) {
    struct mylite_regexp_program *program = NULL;
    char *pattern = NULL;
    size_t pattern_length = 0U;
    bool matches = false;
    int rc = MYLITE_OK;

    if (messages == NULL || out_truth == NULL) {
        return MYLITE_MISUSE;
    }
    if (right == NULL || right->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(right) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, messages->literal);
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        right,
        messages->literal,
        messages->nul,
        &pattern,
        &pattern_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_value_is_supported_string_key(pattern, pattern_length)) {
        free(pattern);
        set_unsupported_error(database, messages->ascii);
        return MYLITE_ERROR;
    }

    enum mylite_regexp_compile_status compile_status = MYLITE_REGEXP_COMPILE_OK;

    if (case_sensitive) {
        compile_status = mylite_regexp_compile_ascii_cs(pattern, pattern_length, &program);
    } else {
        compile_status = mylite_regexp_compile_ascii_ci(pattern, pattern_length, &program);
    }
    free(pattern);
    if (compile_status != MYLITE_REGEXP_COMPILE_OK) {
        mylite_regexp_program_free(program);
        if (compile_status == MYLITE_REGEXP_COMPILE_NOMEM) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        if (compile_status == MYLITE_REGEXP_COMPILE_UNCLOSED_BRACKET) {
            set_regexp_error(
                database,
                "The regular expression contains an unclosed bracket expression."
            );
            return MYLITE_ERROR;
        }
        if (compile_status == MYLITE_REGEXP_COMPILE_INVALID_RANGE) {
            set_regexp_character_range_error(
                database,
                "The regular expression contains an invalid character range."
            );
            return MYLITE_ERROR;
        }
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }

    if (left == NULL) {
        mylite_regexp_program_free(program);
        *out_truth = SHOW_VARIABLES_WHERE_UNKNOWN;
        return MYLITE_OK;
    }

    enum mylite_regexp_match_status match_status = MYLITE_REGEXP_MATCH_OK;

    if (case_sensitive) {
        match_status = mylite_regexp_program_match_ascii_cs(program, left, strlen(left), &matches);
    } else {
        match_status = mylite_regexp_program_match_ascii_ci(program, left, strlen(left), &matches);
    }
    mylite_regexp_program_free(program);
    if (match_status == MYLITE_REGEXP_MATCH_NOMEM) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (match_status == MYLITE_REGEXP_MATCH_UNSUPPORTED_VALUE) {
        set_unsupported_error(
            database,
            "SHOW metadata WHERE REGEXP input supports only ASCII text"
        );
        return MYLITE_ERROR;
    }
    if (match_status != MYLITE_REGEXP_MATCH_OK) {
        set_unsupported_error(
            database,
            "SHOW metadata WHERE REGEXP input is too large for MyLite's baseline subset"
        );
        return MYLITE_ERROR;
    }

    if (matches) {
        *out_truth = SHOW_VARIABLES_WHERE_TRUE;
    } else {
        *out_truth = SHOW_VARIABLES_WHERE_FALSE;
    }
    return MYLITE_OK;
}

static int compare_show_index_where_text(const char *left, const char *right) {
    return compare_ascii_case_insensitive_text(left, right);
}

static bool show_index_where_column_is_numeric(size_t column_index) {
    return (column_index == show_index_non_unique_column ||
            column_index == show_index_seq_in_index_column ||
            column_index == show_index_cardinality_column ||
            column_index == show_index_sub_part_column) != 0;
}

static const char *index_visibility_text(bool is_visible) {
    if (!is_visible) {
        return "NO";
    }

    return "YES";
}
