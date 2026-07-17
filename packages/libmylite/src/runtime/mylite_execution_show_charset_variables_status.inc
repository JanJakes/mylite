
static int execute_show_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_character_set_result_column_count] = {
        "Charset",
        "Description",
        "Default collation",
        "Maxlen",
    };
    const struct mylite_sql_ast_node *filter_node = child_at(statement, 0U);
    const struct mylite_sql_ast_node *like_pattern = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
        where_clause = filter_node;
    } else {
        like_pattern = filter_node;
    }

    rc = make_show_like_filter(database, like_pattern, &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_character_set_result_column_count;
         ++column_index) {
        rc = append_show_result_column(database, result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_mysql_character_set_count();
         ++row_index) {
        const struct mylite_execution_catalog_character_set *charset =
            mylite_execution_catalog_mysql_character_set_at(row_index);
        const char *const values[show_character_set_result_column_count] = {
            charset->name,
            charset->description,
            charset->default_collation,
            charset->maxlen,
        };
        bool matches = false;

        rc = show_catalog_filter_matches(
            database,
            where_clause,
            &filter,
            &(struct show_catalog_where_row){
                .columns = result_columns,
                .values = values,
                .column_count = show_character_set_result_column_count,
            },
            &matches
        );
        if (rc == MYLITE_OK && matches) {
            rc = mylite_result_append_text_row(result, values);
            if (rc != MYLITE_OK) {
                set_nomem_error(database);
            }
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int show_catalog_filter_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct show_like_filter *filter,
    const struct show_catalog_where_row *row,
    bool *out_matches
) {
    if (row == NULL || row->values == NULL || row->column_count == 0U || out_matches == NULL) {
        set_runtime_error(database, "invalid SHOW catalog filter row");
        return MYLITE_ERROR;
    }

    *out_matches = false;
    if (!show_like_filter_matches(filter, row->values[0], false)) {
        return MYLITE_OK;
    }
    if (where_clause == NULL) {
        *out_matches = true;
        return MYLITE_OK;
    }
    return show_catalog_where_clause_matches(database, where_clause, row, out_matches);
}

static int show_catalog_where_clause_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct show_catalog_where_row *row,
    bool *out_matches
) {
    enum show_variables_where_truth truth = SHOW_VARIABLES_WHERE_FALSE;
    int rc = MYLITE_OK;

    if (where_clause == NULL || where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE ||
        row == NULL || out_matches == NULL) {
        set_runtime_error(database, "invalid SHOW catalog WHERE predicate");
        return MYLITE_ERROR;
    }

    *out_matches = false;
    rc = evaluate_show_catalog_where_predicate(database, child_at(where_clause, 0U), row, &truth);
    if (rc == MYLITE_OK) {
        *out_matches = truth == SHOW_VARIABLES_WHERE_TRUE;
    }
    return rc;
}

static int evaluate_show_catalog_where_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_catalog_where_row *row,
    enum show_variables_where_truth *out_truth
) {
    struct show_variables_where_frame_stack frame_stack = {0};
    struct show_variables_where_truth_stack truth_stack = {0};
    struct show_variables_where_eval_frame frame = {
        .node = predicate,
        .action = SHOW_VARIABLES_WHERE_VISIT,
    };
    int rc = MYLITE_OK;

    if (predicate == NULL || row == NULL || out_truth == NULL) {
        set_runtime_error(database, "invalid SHOW catalog WHERE predicate");
        return MYLITE_ERROR;
    }

    rc = show_variables_where_frame_stack_push(database, &frame_stack, frame);
    while (rc == MYLITE_OK && show_variables_where_frame_stack_pop(&frame_stack, &frame)) {
        const struct mylite_sql_ast_node *current = unwrap_parenthesized_predicate(frame.node);

        if (current == NULL) {
            set_runtime_error(database, "invalid SHOW catalog WHERE predicate");
            rc = MYLITE_ERROR;
        } else if (frame.action == SHOW_VARIABLES_WHERE_VISIT) {
            rc = visit_show_catalog_where_predicate(database, &frame_stack, current);
        } else {
            rc = evaluate_show_catalog_where_frame(database, current, row, &truth_stack);
        }
    }

    if (rc == MYLITE_OK) {
        if (truth_stack.count != 1U ||
            !show_variables_where_truth_stack_pop(&truth_stack, out_truth)) {
            set_runtime_error(database, "invalid SHOW catalog WHERE predicate state");
            rc = MYLITE_ERROR;
        }
    }

    show_variables_where_truth_stack_deinit(&truth_stack);
    show_variables_where_frame_stack_deinit(&frame_stack);
    return rc;
}

static int visit_show_catalog_where_predicate(
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

static int evaluate_show_catalog_where_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_catalog_where_row *row,
    struct show_variables_where_truth_stack *truth_stack
) {
    enum show_variables_where_truth truth = SHOW_VARIABLES_WHERE_FALSE;
    int rc = MYLITE_OK;

    if (predicate->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        rc = evaluate_show_catalog_where_comparison_predicate(database, predicate, row, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_IN_PREDICATE) {
        rc = evaluate_show_catalog_where_in_predicate(database, predicate, row, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        rc = evaluate_show_catalog_where_is_null_predicate(database, predicate, row, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        enum show_variables_where_truth child_truth = SHOW_VARIABLES_WHERE_FALSE;

        if (!show_variables_where_truth_stack_pop(truth_stack, &child_truth)) {
            set_runtime_error(database, "invalid SHOW catalog WHERE predicate stack");
            return MYLITE_ERROR;
        }
        truth = negate_show_variables_where_truth(child_truth);
    } else if (
        predicate->kind == MYLITE_SQL_AST_AND_PREDICATE ||
        predicate->kind == MYLITE_SQL_AST_OR_PREDICATE
    ) {
        rc = evaluate_show_variables_where_logical_frame(database, predicate, truth_stack, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_XOR_PREDICATE) {
        set_unsupported_error(database, "SHOW catalog WHERE does not support XOR predicates");
        return MYLITE_ERROR;
    } else {
        set_unsupported_error(database, "SHOW catalog WHERE supports output-column predicates");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = show_variables_where_truth_stack_push(database, truth_stack, truth);
    }
    return rc;
}

static int execute_show_collation_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_collation_result_column_count] = {
        "Collation",
        "Charset",
        "Id",
        "Default",
        "Compiled",
        "Sortlen",
        "Pad_attribute",
    };
    const struct mylite_sql_ast_node *filter_node = child_at(statement, 0U);
    const struct mylite_sql_ast_node *like_pattern = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
        where_clause = filter_node;
    } else {
        like_pattern = filter_node;
    }

    rc = make_show_like_filter(database, like_pattern, &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_collation_result_column_count;
         ++column_index) {
        rc = append_show_result_column(database, result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_mysql_collation_count();
         ++row_index) {
        const struct mylite_execution_catalog_collation *collation =
            mylite_execution_catalog_mysql_collation_at(row_index);
        const char *const values[show_collation_result_column_count] = {
            collation->name,
            collation->character_set,
            collation->id,
            collation->is_default,
            collation->compiled,
            collation->sortlen,
            collation->pad_attribute,
        };
        bool matches = false;

        rc = show_catalog_filter_matches(
            database,
            where_clause,
            &filter,
            &(struct show_catalog_where_row){
                .columns = result_columns,
                .values = values,
                .column_count = show_collation_result_column_count,
            },
            &matches
        );
        if (rc == MYLITE_OK && matches) {
            rc = mylite_result_append_text_row(result, values);
            if (rc != MYLITE_OK) {
                set_nomem_error(database);
            }
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int evaluate_show_catalog_where_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_catalog_where_row *row,
    enum show_variables_where_truth *out_truth
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate, 0U);
    const struct mylite_sql_ast_node *right_node = child_at(predicate, 1U);
    const char *left = NULL;
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate);
    int rc = show_catalog_where_column_value(database, left_node, row, &left);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_REGEXP ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_RLIKE) {
        set_unsupported_error(database, "SHOW catalog WHERE does not support REGEXP predicates");
        return MYLITE_ERROR;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
        char *pattern = NULL;

        if (left == NULL) {
            *out_truth = SHOW_VARIABLES_WHERE_UNKNOWN;
            return MYLITE_OK;
        }
        rc = decode_show_catalog_where_string_literal(database, right_node, &pattern);
        if (rc == MYLITE_OK) {
            bool matches =
                show_like_pattern_matches(pattern, strlen(pattern), left, strlen(left), false);

            *out_truth = matches ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        }
        free(pattern);
        return rc;
    }

    return compare_show_catalog_where_literal(
        database,
        operator_kind,
        left,
        right_node,
        out_truth
    );
}

static int evaluate_show_catalog_where_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_catalog_where_row *row,
    enum show_variables_where_truth *out_truth
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate, 0U);
    const struct mylite_sql_ast_node *value_list = child_at(predicate, 1U);
    const struct mylite_sql_ast_node *value_node = NULL;
    const char *left = NULL;
    bool saw_null = false;
    int rc = show_catalog_where_column_value(database, left_node, row, &left);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (left == NULL) {
        *out_truth = SHOW_VARIABLES_WHERE_UNKNOWN;
        return MYLITE_OK;
    }
    if (value_list == NULL || value_list->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST) {
        set_runtime_error(database, "invalid SHOW catalog WHERE IN list");
        return MYLITE_ERROR;
    }

    for (value_node = value_list->first_child; value_node != NULL;
         value_node = value_node->next_sibling) {
        enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
        char *right = NULL;

        if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL) {
            set_unsupported_error(
                database,
                "SHOW catalog WHERE IN supports only string and NULL literals"
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
                "SHOW catalog WHERE supports only string literal predicates"
            );
            return MYLITE_ERROR;
        }

        rc = decode_show_catalog_where_string_literal(database, value_node, &right);
        if (rc == MYLITE_OK && compare_ascii_case_insensitive_text(left, right) == 0) {
            *out_truth = SHOW_VARIABLES_WHERE_TRUE;
            free(right);
            return MYLITE_OK;
        }
        free(right);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    *out_truth = saw_null ? SHOW_VARIABLES_WHERE_UNKNOWN : SHOW_VARIABLES_WHERE_FALSE;
    return MYLITE_OK;
}

static int evaluate_show_catalog_where_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_catalog_where_row *row,
    enum show_variables_where_truth *out_truth
) {
    const char *value = NULL;
    int rc = show_catalog_where_column_value(database, child_at(predicate, 0U), row, &value);

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

static int show_catalog_where_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct show_catalog_where_row *row,
    const char **out_value
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *display_text = NULL;
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (row == NULL || row->columns == NULL || row->values == NULL || out_value == NULL) {
        set_runtime_error(database, "invalid SHOW catalog WHERE row");
        return MYLITE_ERROR;
    }
    *out_value = NULL;
    if (column_node == NULL || (column_node->kind != MYLITE_SQL_AST_IDENTIFIER &&
                                column_node->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        set_unsupported_error(database, "SHOW catalog WHERE supports only output columns");
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
        for (size_t column_index = 0U; column_index < row->column_count; ++column_index) {
            if (text_equals_ascii_case_insensitive(parts[0], row->columns[column_index])) {
                *out_value = row->values[column_index];
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

static int compare_show_catalog_where_literal(
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
        set_runtime_error(database, "invalid SHOW catalog WHERE comparison");
        return MYLITE_ERROR;
    }
    if (right->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "SHOW catalog WHERE comparisons support only string and NULL literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(right);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_truth = show_catalog_null_literal_truth(operator_kind, left);
        return MYLITE_OK;
    }
    if (left == NULL) {
        *out_truth = show_catalog_null_left_truth(operator_kind);
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "SHOW catalog WHERE supports only string literal predicates"
        );
        return MYLITE_ERROR;
    }

    rc = decode_show_catalog_where_string_literal(database, right, &right_text);
    if (rc != MYLITE_OK) {
        free(right_text);
        return rc;
    }

    rc = show_catalog_where_truth_from_comparison(
        database,
        (struct show_catalog_where_comparison){
            .operator_kind = operator_kind,
            .comparison = compare_ascii_case_insensitive_text(left, right_text),
        },
        out_truth
    );

    free(right_text);
    return rc;
}

static enum show_variables_where_truth show_catalog_null_literal_truth(
    enum mylite_sql_ast_operator operator_kind,
    const char *left
) {
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        return SHOW_VARIABLES_WHERE_UNKNOWN;
    }
    return left == NULL ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
}

static enum show_variables_where_truth show_catalog_null_left_truth(
    enum mylite_sql_ast_operator operator_kind
) {
    return operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL
               ? SHOW_VARIABLES_WHERE_FALSE
               : SHOW_VARIABLES_WHERE_UNKNOWN;
}

static int show_catalog_where_truth_from_comparison(
    struct mylite_db *database,
    struct show_catalog_where_comparison comparison,
    enum show_variables_where_truth *out_truth
) {
    switch (comparison.operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        *out_truth = comparison.comparison == 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        break;
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        *out_truth = comparison.comparison != 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        break;
    case MYLITE_SQL_AST_OPERATOR_LESS:
        *out_truth = comparison.comparison < 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        break;
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        *out_truth = comparison.comparison <= 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        break;
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        *out_truth = comparison.comparison > 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        break;
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        *out_truth = comparison.comparison >= 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        break;
    default:
        set_unsupported_error(database, "SHOW catalog WHERE comparison operator is not supported");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int decode_show_catalog_where_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_text
) {
    size_t text_length = 0U;

    return decode_sql_string_literal(
        database,
        literal_node,
        "SHOW catalog WHERE supports only string literal predicates",
        "SHOW catalog WHERE string literals do not support NUL bytes",
        out_text,
        &text_length
    );
}

static int execute_show_variables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[2] = {"Variable_name", "Value"};
    static const struct show_result_column_definition result_column_definitions[2] = {
        {MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING,
         MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
         64U,
         0U,
         false,
         true},
        {MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING, 0U, 1024U, 0U, true, true},
    };
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *scope = NULL;
    const struct mylite_sql_ast_node *like_pattern = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *filter_node = NULL;
    bool global_scope = false;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_IDENTIFIER) {
        scope = first_child;
        filter_node = child_at(statement, 1U);
    } else {
        filter_node = first_child;
    }
    if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
        where_clause = filter_node;
    } else {
        like_pattern = filter_node;
    }
    global_scope = show_variables_scope_is_global(scope);

    rc = make_show_like_filter(database, like_pattern, &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_result_columns(
            database,
            result,
            result_columns,
            result_column_definitions,
            sizeof(result_columns) / sizeof(result_columns[0])
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_system_variable_descriptor_count();
         ++index) {
        rc = append_show_variable(
            database,
            result,
            &filter,
            where_clause,
            global_scope,
            mylite_execution_system_variable_descriptor_at(index)
        );
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_status_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[2] = {"Variable_name", "Value"};
    static const struct show_result_column_definition result_column_definitions[2] = {
        {MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING,
         MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
         64U,
         0U,
         false,
         true},
        {MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING, 0U, 1024U, 0U, true, true},
    };
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *scope = NULL;
    const struct mylite_sql_ast_node *like_pattern = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *filter_node = NULL;
    bool global_scope = false;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_IDENTIFIER) {
        scope = first_child;
        filter_node = child_at(statement, 1U);
    } else {
        filter_node = first_child;
    }
    if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
        where_clause = filter_node;
    } else {
        like_pattern = filter_node;
    }
    global_scope = show_variables_scope_is_global(scope);

    rc = make_show_like_filter(database, like_pattern, &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_result_columns(
            database,
            result,
            result_columns,
            result_column_definitions,
            sizeof(result_columns) / sizeof(result_columns[0])
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_show_status_descriptor_count();
         ++index) {
        rc = append_show_status(
            database,
            result,
            &filter,
            where_clause,
            global_scope,
            mylite_execution_show_status_descriptor_at(index)
        );
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int append_show_status(
    struct mylite_db *database,
    mylite_result *result,
    const struct show_like_filter *filter,
    const struct mylite_sql_ast_node *where_clause,
    bool global_scope,
    const struct mylite_execution_show_status_descriptor *descriptor
) {
    const char *values[2] = {NULL, NULL};
    struct show_variables_where_row row = {0};
    bool where_matches = true;
    int rc = MYLITE_OK;

    if (!show_status_descriptor_visible(descriptor, global_scope)) {
        return MYLITE_OK;
    }
    if (!show_like_filter_matches(filter, descriptor->name, false)) {
        return MYLITE_OK;
    }

    values[0] = descriptor->name;
    values[1] = descriptor->value;
    if (where_clause != NULL) {
        row.variable_name = values[0];
        row.value = values[1];
        rc = show_variables_where_clause_matches(database, where_clause, &row, &where_matches);
    }
    if (rc == MYLITE_OK && !where_matches) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static bool show_status_descriptor_visible(
    const struct mylite_execution_show_status_descriptor *descriptor,
    bool global_scope
) {
    if (descriptor == NULL) {
        return false;
    }
    if (global_scope) {
        return descriptor->show_global;
    }
    return descriptor->show_session;
}

static bool show_variables_scope_is_global(const struct mylite_sql_ast_node *scope) {
    static const char expected[] = "global";

    if (scope == NULL) {
        return false;
    }
    if (scope->span.text == NULL || scope->span.length != sizeof(expected) - 1U) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(expected) - 1U; ++index) {
        if (ascii_lower((unsigned char)scope->span.text[index]) != expected[index]) {
            return false;
        }
    }
    return true;
}

static int append_show_variable(
    struct mylite_db *database,
    mylite_result *result,
    const struct show_like_filter *filter,
    const struct mylite_sql_ast_node *where_clause,
    bool global_scope,
    const struct mylite_execution_system_variable_descriptor *descriptor
) {
    char integer_buffer[integer_text_capacity];
    const char *values[2] = {NULL, NULL};
    struct show_variables_where_row row = {0};
    bool where_matches = true;
    int rc = MYLITE_OK;

    if (!show_variable_descriptor_visible(descriptor, global_scope)) {
        return MYLITE_OK;
    }
    if (!show_like_filter_matches(filter, descriptor->name, false)) {
        return MYLITE_OK;
    }

    values[0] = descriptor->name;
    rc = show_system_variable_value(
        database,
        descriptor->kind,
        global_scope,
        integer_buffer,
        sizeof(integer_buffer),
        &values[1]
    );
    if (rc == MYLITE_OK && where_clause != NULL) {
        row.variable_name = values[0];
        row.value = values[1];
        rc = show_variables_where_clause_matches(database, where_clause, &row, &where_matches);
    }
    if (rc == MYLITE_OK && !where_matches) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static bool show_variable_descriptor_visible(
    const struct mylite_execution_system_variable_descriptor *descriptor,
    bool global_scope
) {
    if (descriptor == NULL) {
        return false;
    }
    if (global_scope) {
        return descriptor->show_global;
    }
    return descriptor->show_session;
}
