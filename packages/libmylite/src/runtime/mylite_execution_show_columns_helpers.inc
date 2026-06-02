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
        values[4] = mylite_execution_column_key_text(
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
    values[3] = mylite_execution_column_key_text(
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
