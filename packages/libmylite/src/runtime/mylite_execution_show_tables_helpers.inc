static int append_show_table(const struct mylite_catalog_table_descriptor *table, void *user_data) {
    struct show_tables_context *context = user_data;
    const char *values[show_tables_result_column_count] = {NULL, NULL};
    bool where_matches = true;
    int rc = MYLITE_OK;

    if (table == NULL || context == NULL || context->database == NULL || context->result == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE &&
        table->kind != MYLITE_CATALOG_TABLE_KIND_VIEW) {
        return MYLITE_OK;
    }

    if (!show_like_filter_matches(context->filter, table->name, true)) {
        return MYLITE_OK;
    }

    values[0] = table->name;
    if (context->is_full) {
        values[1] = table->kind == MYLITE_CATALOG_TABLE_KIND_VIEW ? "VIEW" : "BASE TABLE";
    }

    if (context->where_clause != NULL) {
        rc = show_tables_where_clause_matches(
            context->database,
            context->where_clause,
            values,
            context->table_name_column,
            context->is_full,
            &where_matches
        );
    }
    if (rc == MYLITE_OK && !where_matches) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(context->result, values);
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(context->database);
    }
    return rc;
}

static int append_show_builtin_schema_tables(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    struct show_tables_context *context
) {
    int rc = MYLITE_OK;

    if (directory == NULL || context == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < directory->table_count; ++index) {
        rc = append_show_builtin_table(directory, directory->table_names[index], context);
    }
    return rc;
}

static int append_show_builtin_table(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name,
    struct show_tables_context *context
) {
    const char *values[show_tables_result_column_count] = {NULL, NULL};
    bool where_matches = true;
    int rc = MYLITE_OK;

    if (directory == NULL || table_name == NULL || context == NULL || context->database == NULL ||
        context->result == NULL) {
        return MYLITE_MISUSE;
    }
    if (!show_like_filter_matches(context->filter, table_name, true)) {
        return MYLITE_OK;
    }

    values[0] = table_name;
    if (context->is_full) {
        values[1] = builtin_schema_table_type(directory, table_name);
    }

    if (context->where_clause != NULL) {
        rc = show_tables_where_clause_matches(
            context->database,
            context->where_clause,
            values,
            context->table_name_column,
            context->is_full,
            &where_matches
        );
    }
    if (rc == MYLITE_OK && !where_matches) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(context->result, values);
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(context->database);
    }
    return rc;
}

static int validate_show_tables_where_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const char *table_name_column,
    bool is_full
) {
    const char *validation_values[show_tables_result_column_count] = {
        "table_name",
        "BASE TABLE",
    };
    bool matches = false;

    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    return show_tables_where_clause_matches(
        database,
        where_clause,
        validation_values,
        table_name_column,
        is_full,
        &matches
    );
}

static int show_tables_where_clause_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const char *const values[show_tables_result_column_count],
    const char *table_name_column,
    bool is_full,
    bool *out_matches
) {
    enum show_variables_where_truth truth = SHOW_VARIABLES_WHERE_FALSE;
    int rc = MYLITE_OK;

    if (where_clause == NULL || where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE ||
        values == NULL || table_name_column == NULL || out_matches == NULL) {
        set_runtime_error(database, "invalid SHOW TABLES WHERE predicate");
        return MYLITE_ERROR;
    }

    *out_matches = false;
    rc = evaluate_show_tables_where_predicate(
        database,
        child_at(where_clause, 0U),
        values,
        table_name_column,
        is_full,
        &truth
    );
    if (rc == MYLITE_OK) {
        *out_matches = truth == SHOW_VARIABLES_WHERE_TRUE;
    }
    return rc;
}

static int evaluate_show_tables_where_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_tables_result_column_count],
    const char *table_name_column,
    bool is_full,
    enum show_variables_where_truth *out_truth
) {
    struct show_variables_where_frame_stack frame_stack = {0};
    struct show_variables_where_truth_stack truth_stack = {0};
    struct show_variables_where_eval_frame frame = {
        .node = predicate,
        .action = SHOW_VARIABLES_WHERE_VISIT,
    };
    int rc = MYLITE_OK;

    if (predicate == NULL || values == NULL || table_name_column == NULL || out_truth == NULL) {
        set_runtime_error(database, "invalid SHOW TABLES WHERE predicate");
        return MYLITE_ERROR;
    }

    rc = show_variables_where_frame_stack_push(database, &frame_stack, frame);
    while (rc == MYLITE_OK && show_variables_where_frame_stack_pop(&frame_stack, &frame)) {
        const struct mylite_sql_ast_node *current = unwrap_parenthesized_predicate(frame.node);

        if (current == NULL) {
            set_runtime_error(database, "invalid SHOW TABLES WHERE predicate");
            rc = MYLITE_ERROR;
        } else if (frame.action == SHOW_VARIABLES_WHERE_VISIT) {
            rc = visit_show_tables_where_predicate(database, &frame_stack, current);
        } else {
            rc = evaluate_show_tables_where_frame(
                database,
                current,
                values,
                table_name_column,
                is_full,
                &truth_stack
            );
        }
    }

    if (rc == MYLITE_OK) {
        if (truth_stack.count != 1U ||
            !show_variables_where_truth_stack_pop(&truth_stack, out_truth)) {
            set_runtime_error(database, "invalid SHOW TABLES WHERE predicate state");
            rc = MYLITE_ERROR;
        }
    }

    show_variables_where_truth_stack_deinit(&truth_stack);
    show_variables_where_frame_stack_deinit(&frame_stack);
    return rc;
}

static int visit_show_tables_where_predicate(
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

static int evaluate_show_tables_where_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_tables_result_column_count],
    const char *table_name_column,
    bool is_full,
    struct show_variables_where_truth_stack *truth_stack
) {
    enum show_variables_where_truth truth = SHOW_VARIABLES_WHERE_FALSE;
    int rc = MYLITE_OK;

    if (predicate->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        rc = evaluate_show_tables_where_comparison_predicate(
            database,
            predicate,
            values,
            table_name_column,
            is_full,
            &truth
        );
    } else if (predicate->kind == MYLITE_SQL_AST_IN_PREDICATE) {
        rc = evaluate_show_tables_where_in_predicate(
            database,
            predicate,
            values,
            table_name_column,
            is_full,
            &truth
        );
    } else if (predicate->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        rc = evaluate_show_tables_where_is_null_predicate(
            database,
            predicate,
            values,
            table_name_column,
            is_full,
            &truth
        );
    } else if (predicate->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        enum show_variables_where_truth child_truth = SHOW_VARIABLES_WHERE_FALSE;

        if (!show_variables_where_truth_stack_pop(truth_stack, &child_truth)) {
            set_runtime_error(database, "invalid SHOW TABLES WHERE predicate stack");
            return MYLITE_ERROR;
        }
        truth = negate_show_variables_where_truth(child_truth);
    } else if (
        predicate->kind == MYLITE_SQL_AST_AND_PREDICATE ||
        predicate->kind == MYLITE_SQL_AST_OR_PREDICATE
    ) {
        rc = evaluate_show_variables_where_logical_frame(database, predicate, truth_stack, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_XOR_PREDICATE) {
        set_unsupported_error(database, "SHOW TABLES WHERE does not support XOR predicates");
        return MYLITE_ERROR;
    } else {
        set_unsupported_error(database, "SHOW TABLES WHERE supports output-column predicates");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = show_variables_where_truth_stack_push(database, truth_stack, truth);
    }
    return rc;
}

static int evaluate_show_tables_where_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_tables_result_column_count],
    const char *table_name_column,
    bool is_full,
    enum show_variables_where_truth *out_truth
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate, 0U);
    const struct mylite_sql_ast_node *right_node = child_at(predicate, 1U);
    const char *left = NULL;
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate);
    int rc = show_tables_where_column_value(
        database,
        left_node,
        values,
        table_name_column,
        is_full,
        &left
    );
    static const struct show_metadata_regexp_messages regexp_messages = {
        .literal = "SHOW TABLES WHERE REGEXP predicates support only string pattern literals",
        .nul = "SHOW TABLES WHERE REGEXP pattern literals do not support NUL bytes",
        .ascii = "SHOW TABLES WHERE REGEXP pattern literals support only ASCII text",
        .unsupported =
            "SHOW TABLES WHERE REGEXP patterns support only MyLite's baseline ASCII regular "
            "expression subset",
    };

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_REGEXP ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_RLIKE) {
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

        if (left == NULL) {
            *out_truth = SHOW_VARIABLES_WHERE_UNKNOWN;
            return MYLITE_OK;
        }
        rc = decode_show_tables_where_string_literal(database, right_node, &pattern);
        if (rc == MYLITE_OK) {
            bool matches =
                show_like_pattern_matches(pattern, strlen(pattern), left, strlen(left), false);

            if (matches) {
                *out_truth = SHOW_VARIABLES_WHERE_TRUE;
            } else {
                *out_truth = SHOW_VARIABLES_WHERE_FALSE;
            }
        }
        free(pattern);
        return rc;
    }

    return compare_show_tables_where_literal(database, operator_kind, left, right_node, out_truth);
}

static int evaluate_show_tables_where_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_tables_result_column_count],
    const char *table_name_column,
    bool is_full,
    enum show_variables_where_truth *out_truth
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate, 0U);
    const struct mylite_sql_ast_node *value_list = child_at(predicate, 1U);
    const char *left = NULL;
    size_t value_count = 0U;
    bool matched = false;
    bool saw_null = false;
    int rc = show_tables_where_column_value(
        database,
        left_node,
        values,
        table_name_column,
        is_full,
        &left
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value_list == NULL || value_list->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST) {
        set_runtime_error(database, "invalid SHOW TABLES WHERE IN list");
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
                "SHOW TABLES WHERE IN supports only string and NULL literals"
            );
            return MYLITE_ERROR;
        }

        literal_kind = mylite_sql_ast_node_literal_kind(value_node);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            saw_null = true;
            continue;
        }
        if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
            set_unsupported_error(
                database,
                "SHOW TABLES WHERE IN supports only string and NULL literals"
            );
            return MYLITE_ERROR;
        }
        rc = decode_show_tables_where_string_literal(database, value_node, &right);
        if (rc == MYLITE_OK && left != NULL &&
            compare_ascii_case_insensitive_text(left, right) == 0) {
            matched = true;
        }
        free(right);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    if (matched) {
        *out_truth = SHOW_VARIABLES_WHERE_TRUE;
    } else if (left == NULL || saw_null) {
        *out_truth = SHOW_VARIABLES_WHERE_UNKNOWN;
    } else {
        *out_truth = SHOW_VARIABLES_WHERE_FALSE;
    }
    return MYLITE_OK;
}

static int evaluate_show_tables_where_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const values[show_tables_result_column_count],
    const char *table_name_column,
    bool is_full,
    enum show_variables_where_truth *out_truth
) {
    const char *value = NULL;
    int rc = show_tables_where_column_value(
        database,
        child_at(predicate, 0U),
        values,
        table_name_column,
        is_full,
        &value
    );

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

static int show_tables_where_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const char *const values[show_tables_result_column_count],
    const char *table_name_column,
    bool is_full,
    const char **out_value
) {
    size_t column_index = 0U;
    int rc = resolve_show_tables_where_column(
        database,
        column_node,
        table_name_column,
        is_full,
        &column_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (values == NULL || out_value == NULL) {
        set_runtime_error(database, "invalid SHOW TABLES WHERE row");
        return MYLITE_ERROR;
    }

    *out_value = values[column_index];
    return MYLITE_OK;
}

static int resolve_show_tables_where_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const char *table_name_column,
    bool is_full,
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
        set_unsupported_error(database, "SHOW TABLES WHERE supports only output columns");
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
    if (part_count == 1U && text_equals_ascii_case_insensitive(parts[0], table_name_column)) {
        *out_column_index = show_tables_name_column;
        return MYLITE_OK;
    }
    if (part_count == 1U && is_full && text_equals_ascii_case_insensitive(parts[0], "Table_type")) {
        *out_column_index = show_tables_type_column;
        return MYLITE_OK;
    }

    rc = show_variables_column_reference_text(database, column_node, &display_text);
    if (rc == MYLITE_OK) {
        set_unknown_where_column_error(database, display_text);
        rc = MYLITE_ERROR;
    }
    free(display_text);
    return rc;
}

static int compare_show_tables_where_literal(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const char *left,
    const struct mylite_sql_ast_node *right,
    enum show_variables_where_truth *out_truth
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *right_text = NULL;
    int comparison = 0;
    int rc = MYLITE_OK;

    if (right == NULL || out_truth == NULL) {
        set_runtime_error(database, "invalid SHOW TABLES WHERE comparison");
        return MYLITE_ERROR;
    }
    if (right->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "SHOW TABLES WHERE comparisons support only string and NULL literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(right);
    if (compare_show_index_where_null_operand(operator_kind, left, literal_kind, out_truth)) {
        return MYLITE_OK;
    }

    rc = decode_show_tables_where_string_literal(database, right, &right_text);
    if (rc != MYLITE_OK) {
        free(right_text);
        return rc;
    }

    comparison = compare_ascii_case_insensitive_text(left, right_text);
    rc = show_tables_where_truth_from_comparison(
        database,
        operator_kind,
        (struct show_table_status_where_comparison){.value = comparison},
        out_truth
    );

    free(right_text);
    return rc;
}

static int show_tables_where_truth_from_comparison(
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
        set_unsupported_error(database, "SHOW TABLES WHERE comparison operator is not supported");
        return MYLITE_ERROR;
    }
}

static int decode_show_tables_where_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_text
) {
    size_t text_length = 0U;

    return decode_sql_string_literal(
        database,
        literal_node,
        "SHOW TABLES WHERE supports only string literal predicates",
        "SHOW TABLES WHERE string literals do not support NUL bytes",
        out_text,
        &text_length
    );
}
