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

static int append_show_table_status(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct show_table_status_context *context = user_data;
    struct table_status_values status = {0};
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

    if (table->kind == MYLITE_CATALOG_TABLE_KIND_BASE) {
        rc = load_table_status_values(context->database, table, &status);
        if (rc != MYLITE_OK) {
            return rc;
        }
    } else {
        rc = format_table_status_timestamp(
            context->database,
            table->created_time_utc_epoch,
            status.create_time_text,
            sizeof(status.create_time_text),
            &status.create_time
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    const char *values[show_table_status_result_column_count] = {
        table->name,
        table->kind == MYLITE_CATALOG_TABLE_KIND_VIEW ? NULL : "InnoDB",
        table->kind == MYLITE_CATALOG_TABLE_KIND_VIEW ? NULL : "10",
        status.row_format,
        table->kind == MYLITE_CATALOG_TABLE_KIND_VIEW ? NULL : status.row_count_text,
        table->kind == MYLITE_CATALOG_TABLE_KIND_VIEW ? NULL : status.average_row_length_text,
        table->kind == MYLITE_CATALOG_TABLE_KIND_VIEW ? NULL : "16384",
        table->kind == MYLITE_CATALOG_TABLE_KIND_VIEW ? NULL : "0",
        status.index_length,
        table->kind == MYLITE_CATALOG_TABLE_KIND_VIEW ? NULL : "0",
        status.auto_increment,
        status.create_time,
        status.update_time,
        NULL,
        table->kind == MYLITE_CATALOG_TABLE_KIND_VIEW ? NULL : table->default_collation,
        NULL,
        status.create_options,
        table->kind == MYLITE_CATALOG_TABLE_KIND_VIEW ? "VIEW" : table->comment,
    };

    if (context->where_clause != NULL) {
        rc = show_table_status_where_clause_matches(
            context->database,
            context->where_clause,
            values,
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

static int append_show_builtin_schema_table_status(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    struct show_table_status_context *context
) {
    int rc = MYLITE_OK;

    if (directory == NULL || context == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < directory->table_count; ++index) {
        rc = append_show_builtin_table_status(directory, directory->table_names[index], context);
    }
    return rc;
}

static int append_show_builtin_table_status(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name,
    struct show_table_status_context *context
) {
    struct table_status_values status = {0};
    bool where_matches = true;
    int rc = MYLITE_OK;

    if (directory == NULL || table_name == NULL || context == NULL || context->database == NULL ||
        context->result == NULL) {
        return MYLITE_MISUSE;
    }
    if (!show_like_filter_matches(context->filter, table_name, true)) {
        return MYLITE_OK;
    }

    rc = load_builtin_table_status_values(context->database, directory, table_name, &status);
    const char *table_type = builtin_schema_table_type(directory, table_name);
    const char *values[show_table_status_result_column_count] = {
        table_name,
        builtin_schema_table_engine(directory, table_name),
        builtin_schema_table_version(directory, table_name),
        builtin_schema_table_row_format(directory, table_name),
        builtin_schema_table_rows(directory, table_name),
        builtin_schema_table_average_row_length(directory, table_name),
        builtin_schema_table_data_length(directory, table_name),
        table_type == NULL || strcmp(table_type, "VIEW") == 0 ? NULL : "0",
        builtin_schema_table_index_length(directory, table_name),
        builtin_schema_table_data_free(directory, table_name),
        builtin_schema_table_auto_increment(directory, table_name),
        status.create_time,
        status.update_time,
        NULL,
        builtin_schema_table_collation(directory, table_name),
        NULL,
        builtin_schema_table_create_options(directory, table_name),
        builtin_schema_table_comment(directory, table_name),
    };

    if (rc == MYLITE_OK && context->where_clause != NULL) {
        rc = show_table_status_where_clause_matches(
            context->database,
            context->where_clause,
            values,
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

static int format_builtin_table_status_timestamp(
    struct mylite_db *database,
    struct table_status_values *status
) {
    if (status == NULL) {
        return MYLITE_MISUSE;
    }
    return format_table_status_timestamp(
        database,
        current_timestamp_epoch(database),
        status->create_time_text,
        sizeof(status->create_time_text),
        &status->create_time
    );
}

static int validate_show_table_status_where_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause
) {
    static const char *const validation_values[show_table_status_result_column_count] = {
        "table_name",
        "InnoDB",
        "10",
        "Dynamic",
        "0",
        "0",
        "16384",
        "0",
        "0",
        "0",
        "0",
        "2000-01-01 00:00:00",
        "2000-01-01 00:00:00",
        "2000-01-01 00:00:00",
        "utf8mb4_0900_ai_ci",
        "0",
        "",
        "",
    };
    bool matches = false;

    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    return show_table_status_where_clause_matches(
        database,
        where_clause,
        validation_values,
        &matches
    );
}

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

static int append_show_column(
    const struct mylite_catalog_column_descriptor *column,
    void *user_data
) {
    struct show_columns_context *context = user_data;
    const char *type_text = NULL;
    char type_text_storage[MYLITE_CATALOG_TYPE_NAME_CAPACITY];
    char default_text[generated_default_display_text_capacity];
    const char *values[show_full_columns_result_column_count] = {0};
    int rc = MYLITE_OK;

    if (column == NULL || context == NULL || context->result == NULL) {
        return MYLITE_MISUSE;
    }

    if (!show_like_filter_matches(context->filter, column->name, false)) {
        return MYLITE_OK;
    }

    rc = show_column_type_text(
        context->database,
        column->logical_type,
        type_text_storage,
        sizeof(type_text_storage),
        &type_text
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    values[0] = column->name;
    values[1] = type_text;
    if (context->full) {
        bool where_matches = true;

        values[2] = show_column_collation_text(context->table, column);
        values[3] = "NO";
        if (column->is_nullable) {
            values[3] = "YES";
        }
        values[4] = column_key_text(
            (struct loaded_index_info_span){
                .indexes = context->indexes,
                .count = context->index_count,
            },
            context->primary_key,
            column
        );
        rc = column_default_display_text(
            context->database,
            column,
            default_text,
            sizeof(default_text),
            &values[show_full_columns_default_column]
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        values[show_full_columns_extra_column] = column_extra_text(column);
        values[show_full_columns_privileges_column] = "select,insert,update,references";
        values[show_full_columns_comment_column] = column->comment;

        if (context->where_clause != NULL) {
            rc = show_columns_where_clause_matches(
                context->database,
                context->where_clause,
                values,
                context->full,
                &where_matches
            );
        }
        if (rc == MYLITE_OK && !where_matches) {
            return MYLITE_OK;
        }
        return mylite_result_append_text_row(context->result, values);
    }

    values[2] = "NO";
    if (column->is_nullable) {
        values[2] = "YES";
    }
    values[3] = column_key_text(
        (struct loaded_index_info_span){
            .indexes = context->indexes,
            .count = context->index_count,
        },
        context->primary_key,
        column
    );
    rc = column_default_display_text(
        context->database,
        column,
        default_text,
        sizeof(default_text),
        &values[4]
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    values[show_columns_extra_column] = column_extra_text(column);

    if (context->where_clause != NULL) {
        bool where_matches = true;

        rc = show_columns_where_clause_matches(
            context->database,
            context->where_clause,
            values,
            context->full,
            &where_matches
        );
        if (rc == MYLITE_OK && !where_matches) {
            return MYLITE_OK;
        }
    }

    return mylite_result_append_text_row(context->result, values);
}

static int evaluate_show_columns_where_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const *values,
    bool full,
    enum show_variables_where_truth *out_truth
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate, 0U);
    const struct mylite_sql_ast_node *right_node = child_at(predicate, 1U);
    const char *left = NULL;
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate);
    int rc = show_columns_where_column_value(database, left_node, values, full, &left);
    static const struct show_metadata_regexp_messages regexp_messages = {
        .literal = "SHOW COLUMNS WHERE REGEXP predicates support only string pattern literals",
        .nul = "SHOW COLUMNS WHERE REGEXP pattern literals do not support NUL bytes",
        .ascii = "SHOW COLUMNS WHERE REGEXP pattern literals support only ASCII text",
        .unsupported =
            "SHOW COLUMNS WHERE REGEXP patterns support only MyLite's baseline ASCII regular "
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

        rc = decode_show_columns_where_string_literal(database, right_node, &pattern);
        if (rc == MYLITE_OK) {
            if (left == NULL) {
                *out_truth = SHOW_VARIABLES_WHERE_UNKNOWN;
            } else if (
                show_like_pattern_matches(pattern, strlen(pattern), left, strlen(left), false)
            ) {
                *out_truth = SHOW_VARIABLES_WHERE_TRUE;
            } else {
                *out_truth = SHOW_VARIABLES_WHERE_FALSE;
            }
        }
        free(pattern);
        return rc;
    }

    return compare_show_columns_where_literal(database, operator_kind, left, right_node, out_truth);
}

static int evaluate_show_columns_where_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const *values,
    bool full,
    enum show_variables_where_truth *out_truth
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate, 0U);
    const struct mylite_sql_ast_node *value_list = child_at(predicate, 1U);
    const char *left = NULL;
    size_t value_count = 0U;
    bool matched = false;
    bool saw_null = false;
    int rc = show_columns_where_column_value(database, left_node, values, full, &left);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value_list == NULL || value_list->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST) {
        set_runtime_error(database, "invalid SHOW COLUMNS WHERE IN list");
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
                "SHOW COLUMNS WHERE IN supports only string and NULL literals"
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
                "SHOW COLUMNS WHERE IN supports only string and NULL literals"
            );
            return MYLITE_ERROR;
        }
        rc = decode_show_columns_where_string_literal(database, value_node, &right);
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

static int evaluate_show_columns_where_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const *values,
    bool full,
    enum show_variables_where_truth *out_truth
) {
    const char *value = NULL;
    int rc =
        show_columns_where_column_value(database, child_at(predicate, 0U), values, full, &value);

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

static int show_columns_where_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const char *const *values,
    bool full,
    const char **out_value
) {
    size_t column_index = 0U;
    int rc = resolve_show_columns_where_column(database, column_node, full, &column_index);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (values == NULL || out_value == NULL) {
        set_runtime_error(database, "invalid SHOW COLUMNS WHERE row");
        return MYLITE_ERROR;
    }

    *out_value = values[column_index];
    return MYLITE_OK;
}

static int resolve_show_columns_where_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    bool full,
    size_t *out_column_index
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *display_text = NULL;
    const char *const *columns = show_columns_result_columns;
    size_t column_count = show_columns_result_column_count;
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (out_column_index == NULL) {
        return MYLITE_MISUSE;
    }
    *out_column_index = 0U;
    if (column_node == NULL || (column_node->kind != MYLITE_SQL_AST_IDENTIFIER &&
                                column_node->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        set_unsupported_error(database, "SHOW COLUMNS WHERE supports only output columns");
        return MYLITE_ERROR;
    }

    if (full) {
        columns = show_full_columns_result_columns;
        column_count = show_full_columns_result_column_count;
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
        for (size_t column_index = 0U; column_index < column_count; ++column_index) {
            if (text_equals_ascii_case_insensitive(parts[0], columns[column_index])) {
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

static int compare_show_columns_where_literal(
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
        set_runtime_error(database, "invalid SHOW COLUMNS WHERE comparison");
        return MYLITE_ERROR;
    }
    if (right->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "SHOW COLUMNS WHERE comparisons support only string and NULL literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(right);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL &&
        compare_show_index_where_null_operand(operator_kind, left, literal_kind, out_truth)) {
        return MYLITE_OK;
    }

    rc = decode_show_columns_where_string_literal(database, right, &right_text);
    if (rc != MYLITE_OK) {
        free(right_text);
        return rc;
    }
    if (left == NULL &&
        compare_show_index_where_null_operand(operator_kind, left, literal_kind, out_truth)) {
        free(right_text);
        return MYLITE_OK;
    }

    comparison = compare_ascii_case_insensitive_text(left, right_text);
    rc = show_index_where_truth_from_comparison(
        database,
        operator_kind,
        (struct show_table_status_where_comparison){.value = comparison},
        out_truth
    );

    free(right_text);
    return rc;
}

static int decode_show_columns_where_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_text
) {
    size_t text_length = 0U;

    return decode_sql_string_literal(
        database,
        literal_node,
        "SHOW COLUMNS WHERE supports only string literal predicates",
        "SHOW COLUMNS WHERE string literals do not support NUL bytes",
        out_text,
        &text_length
    );
}

static int append_show_index(
    struct mylite_db *database,
    mylite_result *result,
    const struct table_name_resolution *target,
    const struct loaded_index_info *index,
    const struct mylite_sql_ast_node *where_clause
) {
    static const char cardinality[] = "0";
    static const char non_unique[] = "1";
    static const char unique[] = "0";
    char sequence[integer_text_capacity];
    const char *non_unique_text = non_unique;
    int rc = MYLITE_OK;

    if (database == NULL || result == NULL || target == NULL || index == NULL) {
        return MYLITE_MISUSE;
    }
    if (index->index.is_unique) {
        non_unique_text = unique;
    }

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        const struct loaded_index_part *part = &index->parts[part_index];
        char prefix_text[integer_text_capacity];
        const char *nullable = show_index_part_nullable_text(part);
        const char *collation = show_index_part_collation_text(index, part);
        const char *prefix_value = NULL;
        const char *index_type = show_index_index_type_text(index);
        bool where_matches = true;

        rc = show_index_part_prefix_value(
            database,
            index,
            part,
            prefix_text,
            sizeof(prefix_text),
            &prefix_value
        );
        if (rc == MYLITE_OK) {
            rc = information_schema_format_i64(
                database,
                part->index_column.ordinal_position,
                sequence,
                sizeof(sequence)
            );
        }
        if (rc == MYLITE_OK) {
            const char *values[show_index_result_column_count] = {
                target->table_name,
                non_unique_text,
                index->index.name,
                sequence,
                part->column.name,
                collation,
                cardinality,
                prefix_value,
                NULL,
                nullable,
                index_type,
                "",
                index->index.comment,
                index_visibility_text(index->index.is_visible),
                NULL,
            };

            if (where_clause != NULL) {
                rc =
                    show_index_where_clause_matches(database, where_clause, values, &where_matches);
            }
            if (rc == MYLITE_OK && where_matches) {
                rc = mylite_result_append_text_row(result, values);
            }
        }
    }

    return rc;
}

static const char *show_index_part_nullable_text(const struct loaded_index_part *part) {
    if (part->column.is_nullable) {
        return "YES";
    }

    return "";
}

static const char *show_index_part_collation_text(
    const struct loaded_index_info *index,
    const struct loaded_index_part *part
) {
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        return NULL;
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        return "A";
    }
    if (part->index_column.sort_direction == MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC) {
        return "D";
    }

    return "A";
}

static int show_index_part_prefix_value(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    const struct loaded_index_part *part,
    char *prefix_text,
    size_t prefix_text_size,
    const char **out_prefix_value
) {
    *out_prefix_value = NULL;
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        *out_prefix_value = "32";
        return MYLITE_OK;
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT ||
        !part->index_column.has_prefix_length) {
        return MYLITE_OK;
    }

    int rc = information_schema_format_i64(
        database,
        part->index_column.prefix_length,
        prefix_text,
        prefix_text_size
    );

    if (rc == MYLITE_OK) {
        *out_prefix_value = prefix_text;
    }
    return rc;
}

static const char *show_index_index_type_text(const struct loaded_index_info *index) {
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        return "FULLTEXT";
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        return "SPATIAL";
    }

    return "BTREE";
}

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

static int show_column_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char **out_type_text
) {
    return show_descriptor_type_text(
        database,
        logical_type,
        buffer,
        buffer_size,
        "SHOW COLUMNS supports only integer, BIT, string, decimal, approximate numeric, DATE, "
        "TIME, DATETIME, and TIMESTAMP column descriptors",
        out_type_text
    );
}

static const char *show_column_collation_text(
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column_descriptor_is_string_family(column) || column_descriptor_is_enum(column) ||
        column_descriptor_is_set(column)) {
        return column_effective_collation_name(table, column);
    }

    return NULL;
}

static int collect_show_database(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct show_databases_context *context = user_data;

    if (schema == NULL || context == NULL) {
        return MYLITE_MISUSE;
    }

    return show_database_list_append(context->database, context, schema->name);
}

static int append_builtin_show_database_names(struct show_databases_context *context) {
    if (context == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < mylite_execution_catalog_builtin_schema_count(); ++index) {
        const struct mylite_execution_catalog_builtin_schema *schema =
            mylite_execution_catalog_builtin_schema_at(index);
        int rc = show_database_list_append(context->database, context, schema->name);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int show_database_list_append(
    struct mylite_db *database,
    struct show_databases_context *context,
    const char *name
) {
    struct show_database_name *new_names = NULL;
    int written = 0;

    if (context == NULL || name == NULL) {
        return MYLITE_MISUSE;
    }
    if (context->name_count == context->name_capacity) {
        size_t new_capacity = context->name_capacity == 0U ? show_databases_initial_name_capacity
                                                           : context->name_capacity * 2U;

        if (new_capacity < context->name_capacity) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        new_names = realloc(context->names, new_capacity * sizeof(context->names[0]));
        if (new_names == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        context->names = new_names;
        context->name_capacity = new_capacity;
    }

    written = snprintf(
        context->names[context->name_count].name,
        sizeof(context->names[context->name_count].name),
        "%s",
        name
    );
    if (written < 0 || (size_t)written >= sizeof(context->names[context->name_count].name)) {
        set_identifier_too_long_error(database, "database");
        return MYLITE_ERROR;
    }
    ++context->name_count;
    return MYLITE_OK;
}

static int append_sorted_show_database_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct show_like_filter *filter,
    const struct mylite_sql_ast_node *where_clause,
    struct show_databases_context *context
) {
    const char *previous_name = NULL;
    int rc = MYLITE_OK;

    if (result == NULL || context == NULL) {
        return MYLITE_MISUSE;
    }
    qsort(
        context->names,
        context->name_count,
        sizeof(context->names[0]),
        compare_show_database_names
    );

    for (size_t index = 0U; rc == MYLITE_OK && index < context->name_count; ++index) {
        const char *name = context->names[index].name;
        const char *values[1] = {name};
        bool where_matches = true;

        if (previous_name != NULL &&
            compare_ascii_case_insensitive_text(previous_name, name) == 0) {
            continue;
        }
        previous_name = name;
        if (!show_like_filter_matches(filter, name, true)) {
            continue;
        }
        if (where_clause != NULL) {
            rc = show_databases_where_clause_matches(database, where_clause, name, &where_matches);
        }
        if (rc == MYLITE_OK && !where_matches) {
            continue;
        }

        if (rc == MYLITE_OK) {
            rc = mylite_result_append_text_row(result, values);
        }
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }

    return rc;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): qsort fixes this callback shape.
static int compare_show_database_names(const void *left, const void *right) {
    const struct show_database_name *left_name = left;
    const struct show_database_name *right_name = right;

    return compare_ascii_case_insensitive_text(left_name->name, right_name->name);
}

static int validate_show_databases_where_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause
) {
    bool matches = false;

    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    return show_databases_where_clause_matches(database, where_clause, "mysql", &matches);
}

static int show_databases_where_clause_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const char *name,
    bool *out_matches
) {
    enum show_variables_where_truth truth = SHOW_VARIABLES_WHERE_FALSE;
    int rc = MYLITE_OK;

    if (where_clause == NULL || where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE || name == NULL ||
        out_matches == NULL) {
        set_runtime_error(database, "invalid SHOW DATABASES WHERE predicate");
        return MYLITE_ERROR;
    }

    *out_matches = false;
    rc =
        evaluate_show_databases_where_predicate(database, child_at(where_clause, 0U), name, &truth);
    if (rc == MYLITE_OK) {
        *out_matches = truth == SHOW_VARIABLES_WHERE_TRUE;
    }
    return rc;
}

static int evaluate_show_databases_where_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *name,
    enum show_variables_where_truth *out_truth
) {
    struct show_variables_where_frame_stack frame_stack = {0};
    struct show_variables_where_truth_stack truth_stack = {0};
    struct show_variables_where_eval_frame frame = {
        .node = predicate,
        .action = SHOW_VARIABLES_WHERE_VISIT,
    };
    int rc = MYLITE_OK;

    if (predicate == NULL || name == NULL || out_truth == NULL) {
        set_runtime_error(database, "invalid SHOW DATABASES WHERE predicate");
        return MYLITE_ERROR;
    }

    rc = show_variables_where_frame_stack_push(database, &frame_stack, frame);
    while (rc == MYLITE_OK && show_variables_where_frame_stack_pop(&frame_stack, &frame)) {
        const struct mylite_sql_ast_node *current = unwrap_parenthesized_predicate(frame.node);

        if (current == NULL) {
            set_runtime_error(database, "invalid SHOW DATABASES WHERE predicate");
            rc = MYLITE_ERROR;
        } else if (frame.action == SHOW_VARIABLES_WHERE_VISIT) {
            rc = visit_show_databases_where_predicate(database, &frame_stack, current);
        } else {
            rc = evaluate_show_databases_where_frame(database, current, name, &truth_stack);
        }
    }

    if (rc == MYLITE_OK) {
        if (truth_stack.count != 1U ||
            !show_variables_where_truth_stack_pop(&truth_stack, out_truth)) {
            set_runtime_error(database, "invalid SHOW DATABASES WHERE predicate state");
            rc = MYLITE_ERROR;
        }
    }

    show_variables_where_truth_stack_deinit(&truth_stack);
    show_variables_where_frame_stack_deinit(&frame_stack);
    return rc;
}

static int visit_show_databases_where_predicate(
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

static int evaluate_show_databases_where_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *name,
    struct show_variables_where_truth_stack *truth_stack
) {
    enum show_variables_where_truth truth = SHOW_VARIABLES_WHERE_FALSE;
    int rc = MYLITE_OK;

    if (predicate->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        rc = evaluate_show_databases_where_comparison_predicate(database, predicate, name, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_IN_PREDICATE) {
        rc = evaluate_show_databases_where_in_predicate(database, predicate, name, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        rc = evaluate_show_databases_where_is_null_predicate(database, predicate, name, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        enum show_variables_where_truth child_truth = SHOW_VARIABLES_WHERE_FALSE;

        if (!show_variables_where_truth_stack_pop(truth_stack, &child_truth)) {
            set_runtime_error(database, "invalid SHOW DATABASES WHERE predicate stack");
            return MYLITE_ERROR;
        }
        truth = negate_show_variables_where_truth(child_truth);
    } else if (
        predicate->kind == MYLITE_SQL_AST_AND_PREDICATE ||
        predicate->kind == MYLITE_SQL_AST_OR_PREDICATE
    ) {
        rc = evaluate_show_variables_where_logical_frame(database, predicate, truth_stack, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_XOR_PREDICATE) {
        set_unsupported_error(database, "SHOW DATABASES WHERE does not support XOR predicates");
        return MYLITE_ERROR;
    } else {
        set_unsupported_error(database, "SHOW DATABASES WHERE supports output-column predicates");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = show_variables_where_truth_stack_push(database, truth_stack, truth);
    }
    return rc;
}

static int evaluate_show_databases_where_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *name,
    enum show_variables_where_truth *out_truth
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate, 0U);
    const struct mylite_sql_ast_node *right_node = child_at(predicate, 1U);
    const char *left = NULL;
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate);
    int rc = show_databases_where_column_value(database, left_node, name, &left);
    static const struct show_metadata_regexp_messages regexp_messages = {
        .literal = "SHOW DATABASES WHERE REGEXP predicates support only string pattern literals",
        .nul = "SHOW DATABASES WHERE REGEXP pattern literals do not support NUL bytes",
        .ascii = "SHOW DATABASES WHERE REGEXP pattern literals support only ASCII text",
        .unsupported =
            "SHOW DATABASES WHERE REGEXP patterns support only MyLite's baseline ASCII regular "
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
            true,
            out_truth
        );
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
        char *pattern = NULL;

        if (left == NULL) {
            *out_truth = SHOW_VARIABLES_WHERE_UNKNOWN;
            return MYLITE_OK;
        }
        rc = decode_show_databases_where_string_literal(database, right_node, &pattern);
        if (rc == MYLITE_OK) {
            bool matches =
                show_like_pattern_matches(pattern, strlen(pattern), left, strlen(left), true);

            *out_truth = matches ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        }
        free(pattern);
        return rc;
    }

    return compare_show_databases_where_literal(
        database,
        operator_kind,
        left,
        right_node,
        out_truth
    );
}

static int evaluate_show_databases_where_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *name,
    enum show_variables_where_truth *out_truth
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate, 0U);
    const struct mylite_sql_ast_node *value_list = child_at(predicate, 1U);
    const char *left = NULL;
    size_t value_count = 0U;
    bool matched = false;
    bool saw_null = false;
    int rc = show_databases_where_column_value(database, left_node, name, &left);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value_list == NULL || value_list->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST) {
        set_runtime_error(database, "invalid SHOW DATABASES WHERE IN list");
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
                "SHOW DATABASES WHERE IN supports only string and NULL literals"
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
                "SHOW DATABASES WHERE IN supports only string and NULL literals"
            );
            return MYLITE_ERROR;
        }
        rc = decode_show_databases_where_string_literal(database, value_node, &right);
        if (rc == MYLITE_OK && left != NULL && strcmp(left, right) == 0) {
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

static int evaluate_show_databases_where_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *name,
    enum show_variables_where_truth *out_truth
) {
    const char *value = NULL;
    int rc = show_databases_where_column_value(database, child_at(predicate, 0U), name, &value);

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

static int show_databases_where_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const char *name,
    const char **out_value
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *display_text = NULL;
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (name == NULL || out_value == NULL) {
        set_runtime_error(database, "invalid SHOW DATABASES WHERE row");
        return MYLITE_ERROR;
    }
    *out_value = NULL;
    if (column_node == NULL || (column_node->kind != MYLITE_SQL_AST_IDENTIFIER &&
                                column_node->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        set_unsupported_error(database, "SHOW DATABASES WHERE supports only output columns");
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
    if (part_count == 1U && text_equals_ascii_case_insensitive(parts[0], "Database")) {
        *out_value = name;
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

static int compare_show_databases_where_literal(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const char *left,
    const struct mylite_sql_ast_node *right,
    enum show_variables_where_truth *out_truth
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *right_text = NULL;
    int rc = MYLITE_OK;

    if (right == NULL || out_truth == NULL) {
        set_runtime_error(database, "invalid SHOW DATABASES WHERE comparison");
        return MYLITE_ERROR;
    }
    if (right->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "SHOW DATABASES WHERE comparisons support only string and NULL literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(right);
    if (compare_show_index_where_null_operand(operator_kind, left, literal_kind, out_truth)) {
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "SHOW DATABASES WHERE supports only string literal predicates"
        );
        return MYLITE_ERROR;
    }

    rc = decode_show_databases_where_string_literal(database, right, &right_text);
    if (rc == MYLITE_OK) {
        rc = show_databases_where_truth_from_comparison(
            database,
            operator_kind,
            (struct show_databases_where_comparison){.value = strcmp(left, right_text)},
            out_truth
        );
    }

    free(right_text);
    return rc;
}

static int show_databases_where_truth_from_comparison(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    struct show_databases_where_comparison comparison,
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
            "SHOW DATABASES WHERE comparison operator is not supported"
        );
        return MYLITE_ERROR;
    }
}

static int decode_show_databases_where_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_text
) {
    size_t text_length = 0U;

    return decode_sql_string_literal(
        database,
        literal_node,
        "SHOW DATABASES WHERE supports only string literal predicates",
        "SHOW DATABASES WHERE string literals do not support NUL bytes",
        out_text,
        &text_length
    );
}

static int make_show_like_filter(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *pattern_node,
    struct show_like_filter *out_filter
) {
    char *pattern = NULL;
    int rc = MYLITE_OK;

    if (out_filter == NULL) {
        set_runtime_error(database, "invalid SHOW LIKE filter");
        return MYLITE_ERROR;
    }
    *out_filter = (struct show_like_filter){
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    if (pattern_node == NULL) {
        return MYLITE_OK;
    }

    rc = decode_show_like_pattern(database, pattern_node, &pattern);
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_filter->has_pattern = true;
    out_filter->pattern = pattern;
    out_filter->pattern_length = strlen(pattern);
    return MYLITE_OK;
}

static void show_like_filter_deinit(struct show_like_filter *filter) {
    if (filter == NULL) {
        return;
    }

    free(filter->pattern);
    *filter = (struct show_like_filter){
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
}

static bool show_like_filter_matches(
    const struct show_like_filter *filter,
    const char *value,
    bool case_sensitive
) {
    if (filter == NULL || !filter->has_pattern) {
        return true;
    }
    if (value == NULL) {
        return false;
    }

    return show_like_pattern_matches(
        filter->pattern,
        filter->pattern_length,
        value,
        strlen(value),
        case_sensitive
    );
}

static int build_show_databases_column_name(
    const struct show_like_filter *filter,
    char **out_name
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (out_name == NULL) {
        return MYLITE_MISUSE;
    }
    *out_name = NULL;

    mylite_dynamic_string_init(&string);
    rc = mylite_dynamic_string_append(&string, "Database");
    if (rc == MYLITE_OK && filter != NULL && filter->has_pattern) {
        rc = mylite_dynamic_string_append(&string, " (");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, filter->pattern);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(&string, ')');
        }
    }
    if (rc == MYLITE_OK) {
        *out_name = mylite_dynamic_string_take(&string);
        if (*out_name == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int build_show_tables_column_name(
    const char *schema_name,
    const struct show_like_filter *filter,
    char **out_name
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (schema_name == NULL || out_name == NULL) {
        return MYLITE_MISUSE;
    }
    *out_name = NULL;

    mylite_dynamic_string_init(&string);
    rc = mylite_dynamic_string_append(&string, "Tables_in_");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, schema_name);
    }
    if (rc == MYLITE_OK && filter != NULL && filter->has_pattern) {
        rc = mylite_dynamic_string_append(&string, " (");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, filter->pattern);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(&string, ')');
        }
    }
    if (rc == MYLITE_OK) {
        *out_name = mylite_dynamic_string_take(&string);
        if (*out_name == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int read_show_table_status_row_count(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t *out_count
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int rc = build_show_table_status_count_sql(table, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = step_count_statement(statement, out_count);
    }

    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int build_show_table_status_count_sql(
    const struct mylite_catalog_table_descriptor *table,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (table == NULL || out_sql == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT COUNT(*) FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, table->physical_name);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);

    return rc;
}

static int format_show_table_status_integer(
    struct mylite_db *database,
    int64_t value,
    char *buffer,
    size_t buffer_size
) {
    int written = snprintf(buffer, buffer_size, "%" PRId64, value);

    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format SHOW TABLE STATUS value");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int decode_show_like_pattern(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *pattern_node,
    char **out_pattern
) {
    struct mylite_dynamic_string string;
    const char *text = NULL;
    size_t length = 0U;
    char quote = '\0';
    int rc = MYLITE_OK;

    if (out_pattern == NULL) {
        set_runtime_error(database, "invalid SHOW LIKE pattern");
        return MYLITE_ERROR;
    }
    *out_pattern = NULL;
    if (pattern_node == NULL || pattern_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(pattern_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "SHOW LIKE supports only string literal patterns");
        return MYLITE_ERROR;
    }

    text = pattern_node->span.text;
    length = pattern_node->span.length;
    if (text == NULL || length < 2U) {
        set_runtime_error(database, "invalid SHOW LIKE pattern");
        return MYLITE_ERROR;
    }

    quote = text[0];
    mylite_dynamic_string_init(&string);
    for (size_t index = 1U; rc == MYLITE_OK && index + 1U < length; ++index) {
        char byte = text[index];

        if (byte == quote && index + 2U < length && text[index + 1U] == quote) {
            rc = mylite_dynamic_string_append_char(&string, quote);
            ++index;
        } else if (byte == '\\' && index + 2U < length) {
            ++index;
            rc = append_decoded_string_escape(database, &string, text[index]);
        } else {
            rc = mylite_dynamic_string_append_char(&string, byte);
        }
    }
    if (rc == MYLITE_OK && string.text == NULL) {
        rc = mylite_dynamic_string_append(&string, "");
    }
    if (rc == MYLITE_OK) {
        *out_pattern = mylite_dynamic_string_take(&string);
        if (*out_pattern == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_decoded_string_escape(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    char escaped_byte
) {
    switch (escaped_byte) {
    case '0':
        set_unsupported_error(database, "SHOW LIKE does not support NUL bytes in patterns");
        return MYLITE_ERROR;
    case 'n':
        return mylite_dynamic_string_append_char(string, '\n');
    case 'r':
        return mylite_dynamic_string_append_char(string, '\r');
    case 't':
        return mylite_dynamic_string_append_char(string, '\t');
    case 'b':
        return mylite_dynamic_string_append_char(string, '\b');
    case 'Z':
        return mylite_dynamic_string_append_char(string, '\032');
    case '\\':
        return mylite_dynamic_string_append_char(string, '\\');
    case '\'':
        return mylite_dynamic_string_append_char(string, '\'');
    case '"':
        return mylite_dynamic_string_append_char(string, '"');
    case '%':
        if (mylite_dynamic_string_append_char(string, '\\') != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        return mylite_dynamic_string_append_char(string, '%');
    case '_':
        if (mylite_dynamic_string_append_char(string, '\\') != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        return mylite_dynamic_string_append_char(string, '_');
    default:
        return mylite_dynamic_string_append_char(string, escaped_byte);
    }
}

static bool show_like_pattern_matches(
    const char *pattern,
    size_t pattern_length,
    const char *value,
    size_t value_length,
    bool case_sensitive
) {
    return show_like_pattern_matches_with_escape(
        pattern,
        pattern_length,
        value,
        value_length,
        case_sensitive,
        true
    );
}

static bool show_like_pattern_matches_with_escape(
    const char *pattern,
    size_t pattern_length,
    const char *value,
    size_t value_length,
    bool case_sensitive,
    bool backslash_escapes
) {
    const size_t no_retry_pattern = SIZE_MAX;
    size_t pattern_index = 0U;
    size_t value_index = 0U;
    size_t retry_pattern_index = no_retry_pattern;
    size_t retry_value_index = 0U;

    while (value_index < value_length) {
        size_t next_pattern_index = pattern_index;

        if (pattern_index < pattern_length && pattern[pattern_index] == '%') {
            pattern_index = show_like_skip_percent_run(pattern, pattern_length, pattern_index);
            if (pattern_index == pattern_length) {
                return true;
            }
            retry_pattern_index = pattern_index;
            retry_value_index = value_index;
            continue;
        }
        if (show_like_pattern_item_matches(
                (struct show_like_pattern_item_request){
                    .pattern = pattern,
                    .pattern_length = pattern_length,
                    .pattern_index = pattern_index,
                    .value_byte = value[value_index],
                    .case_sensitive = case_sensitive,
                    .backslash_escapes = backslash_escapes,
                },
                &next_pattern_index
            )) {
            pattern_index = next_pattern_index;
            ++value_index;
            continue;
        }
        if (retry_pattern_index == no_retry_pattern || retry_value_index >= value_length) {
            return false;
        }
        ++retry_value_index;
        value_index = retry_value_index;
        pattern_index = retry_pattern_index;
    }

    pattern_index = show_like_skip_percent_run(pattern, pattern_length, pattern_index);

    return pattern_index == pattern_length;
}

static size_t show_like_skip_percent_run(
    const char *pattern,
    size_t pattern_length,
    size_t pattern_index
) {
    while (pattern_index < pattern_length && pattern[pattern_index] == '%') {
        ++pattern_index;
    }
    return pattern_index;
}

static bool show_like_pattern_item_matches(
    struct show_like_pattern_item_request request,
    size_t *out_next_pattern_index
) {
    char pattern_byte = '\0';
    size_t next_pattern_index = request.pattern_index;

    if (request.pattern_index >= request.pattern_length || out_next_pattern_index == NULL) {
        return false;
    }

    pattern_byte = request.pattern[request.pattern_index];
    if (pattern_byte == '_') {
        *out_next_pattern_index = request.pattern_index + 1U;
        return true;
    }
    if (request.backslash_escapes && pattern_byte == '\\' &&
        request.pattern_index + 1U < request.pattern_length) {
        ++next_pattern_index;
        pattern_byte = request.pattern[next_pattern_index];
    }
    ++next_pattern_index;
    if (!show_like_bytes_equal(pattern_byte, request.value_byte, request.case_sensitive)) {
        return false;
    }

    *out_next_pattern_index = next_pattern_index;
    return true;
}

static bool show_like_bytes_equal(char left, char right, bool case_sensitive) {
    if (case_sensitive) {
        return left == right;
    }

    return show_like_ascii_lower(left) == show_like_ascii_lower(right);
}

static char show_like_ascii_lower(char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte - 'A' + 'a');
    }
    return byte;
}
