static int information_schema_append_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_query *query
) {
    int rc = MYLITE_OK;

    if (query->is_count_star) {
        char *column_name = NULL;

        if (query->count_alias != NULL) {
            rc = copy_select_item_alias_text(database, query->count_alias, &column_name);
        } else {
            rc = copy_aggregate_result_column_name(
                database,
                &query->count_expression->span,
                &column_name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_result_append_column(result, column_name);
            if (rc != MYLITE_OK) {
                set_nomem_error(database);
            }
        }
        free(column_name);
        return rc;
    }

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < query->projection_count;
         ++column_index) {
        rc = mylite_result_append_column(result, query->projection_names[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static int information_schema_append_result_rows(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
) {
    size_t *indexes = NULL;
    size_t index_count = 0U;
    size_t visible_count = 0U;
    const char **values = NULL;
    int rc = information_schema_matching_row_indexes(
        database,
        statement,
        query,
        rows,
        &indexes,
        &index_count
    );

    if (out_read_row_count != NULL) {
        *out_read_row_count = 0U;
    }
    if (rc == MYLITE_OK && index_count == 0U) {
        free(indexes);
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK && rows->rows == NULL) {
        free(indexes);
        set_runtime_error(database, "information_schema row set is inconsistent");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_sort_row_indexes(query, rows, indexes, index_count);
    }
    if (rc == MYLITE_OK && query->projection_count != 0U) {
        values = (const char **)calloc(query->projection_count, sizeof(*values));
        if (values == NULL) {
            free(indexes);
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
    }
    for (size_t output_index = 0U; rc == MYLITE_OK && output_index < index_count; ++output_index) {
        size_t row_index = indexes[output_index];

        if (query->limit.has_limit && visible_count >= (size_t)query->limit.row_count) {
            break;
        }
        for (size_t column_index = 0U; column_index < query->projection_count; ++column_index) {
            values[column_index] = rows->rows[row_index][query->projection_indexes[column_index]];
        }
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
        ++visible_count;
    }
    free((void *)values);
    if (rc == MYLITE_OK && out_read_row_count != NULL) {
        *out_read_row_count = visible_count;
    }
    free(indexes);
    return rc;
}

static int information_schema_append_count_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
) {
    char count_text[integer_text_capacity];
    const char *values[1] = {count_text};
    size_t *indexes = NULL;
    size_t index_count = 0U;
    bool emits_count_row = true;
    int rc = information_schema_matching_row_indexes(
        database,
        statement,
        query,
        rows,
        &indexes,
        &index_count
    );

    if (out_read_row_count != NULL) {
        *out_read_row_count = 0U;
    }
    if (query->limit.has_limit && query->limit.row_count == 0) {
        emits_count_row = false;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            (int64_t)index_count,
            count_text,
            sizeof(count_text)
        );
    }
    if (rc == MYLITE_OK && emits_count_row) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK && emits_count_row && out_read_row_count != NULL) {
        *out_read_row_count = index_count;
    }
    free(indexes);
    return rc;
}

static int information_schema_matching_row_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t **out_indexes,
    size_t *out_index_count
) {
    const struct mylite_sql_ast_node *optional_clause = child_at(statement, 2U);
    const struct mylite_sql_ast_node *where_clause = NULL;
    size_t *indexes = NULL;
    size_t index_count = 0U;
    int rc = MYLITE_OK;

    *out_indexes = NULL;
    *out_index_count = 0U;
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
            break;
        }
        optional_clause = optional_clause->next_sibling;
    }

    if (rows->row_count == 0U) {
        return MYLITE_OK;
    }
    if (rows->row_count > SIZE_MAX / sizeof(*indexes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    indexes = malloc(rows->row_count * sizeof(*indexes));
    if (indexes == NULL && rows->row_count != 0U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < rows->row_count; ++row_index) {
        bool matches = true;

        if (where_clause != NULL) {
            rc = information_schema_predicate_matches(
                database,
                query,
                child_at(where_clause, 0U),
                rows->rows[row_index],
                &matches
            );
        }
        if (rc == MYLITE_OK && matches) {
            indexes[index_count] = row_index;
            ++index_count;
        }
    }
    if (rc != MYLITE_OK) {
        free(indexes);
        return rc;
    }

    *out_indexes = indexes;
    *out_index_count = index_count;
    return MYLITE_OK;
}

static int information_schema_sort_row_indexes(
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t *indexes,
    size_t index_count
) {
    if (!query->has_order) {
        return MYLITE_OK;
    }
    for (size_t index = 1U; index < index_count; ++index) {
        size_t row_index = indexes[index];
        size_t scan = index;

        while (scan > 0U && information_schema_compare_rows(
                                query,
                                rows,
                                (struct information_schema_row_order_pair){
                                    .left_row = row_index,
                                    .right_row = indexes[scan - 1U],
                                }
                            ) < 0) {
            indexes[scan] = indexes[scan - 1U];
            --scan;
        }
        indexes[scan] = row_index;
    }
    return MYLITE_OK;
}

static int information_schema_compare_rows(
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    struct information_schema_row_order_pair pair
) {
    const char *left = rows->rows[pair.left_row][query->order_index];
    const char *right = rows->rows[pair.right_row][query->order_index];
    int comparison = 0;

    if (left == NULL && right == NULL) {
        return 0;
    }
    if (left == NULL) {
        comparison = -1;
    } else if (right == NULL) {
        comparison = 1;
    } else if (information_schema_column_is_numeric(query->definition, query->order_index)) {
        int64_t left_value = 0;
        int64_t right_value = 0;

        if (information_schema_text_to_i64(left, &left_value) == MYLITE_OK &&
            information_schema_text_to_i64(right, &right_value) == MYLITE_OK) {
            comparison = (left_value > right_value) - (left_value < right_value);
        } else {
            comparison =
                information_schema_compare_text(query->definition, query->order_index, left, right);
        }
    } else {
        comparison =
            information_schema_compare_text(query->definition, query->order_index, left, right);
    }
    if (query->order_direction == PLANNED_SELECT_ORDER_DESC) {
        comparison = -comparison;
    }
    return comparison;
}

static int information_schema_plan_where(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct information_schema_query *query
) {
    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    if (where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        set_unsupported_error(database, "INFORMATION_SCHEMA SELECT supports metadata predicates");
        return MYLITE_ERROR;
    }
    return information_schema_validate_predicate(database, query, child_at(where_clause, 0U));
}

static int information_schema_validate_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    struct information_schema_predicate_frame_stack frame_stack = {0};
    struct information_schema_predicate_eval_frame frame = {
        .node = predicate_node,
        .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
    };
    int rc = information_schema_predicate_frame_stack_push(database, &frame_stack, frame);

    while (rc == MYLITE_OK && information_schema_predicate_frame_stack_pop(&frame_stack, &frame)) {
        const struct mylite_sql_ast_node *current = unwrap_parenthesized_predicate(frame.node);

        if (current == NULL) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA WHERE supports metadata predicates"
            );
            rc = MYLITE_ERROR;
            continue;
        }

        switch (current->kind) {
        case MYLITE_SQL_AST_NOT_PREDICATE:
            rc = information_schema_predicate_frame_stack_push(
                database,
                &frame_stack,
                (struct information_schema_predicate_eval_frame){
                    .node = child_at(current, 0U),
                    .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
                }
            );
            break;
        case MYLITE_SQL_AST_AND_PREDICATE:
        case MYLITE_SQL_AST_OR_PREDICATE:
        case MYLITE_SQL_AST_XOR_PREDICATE:
            rc = information_schema_predicate_frame_stack_push(
                database,
                &frame_stack,
                (struct information_schema_predicate_eval_frame){
                    .node = child_at(current, 1U),
                    .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
                }
            );
            if (rc == MYLITE_OK) {
                rc = information_schema_predicate_frame_stack_push(
                    database,
                    &frame_stack,
                    (struct information_schema_predicate_eval_frame){
                        .node = child_at(current, 0U),
                        .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
                    }
                );
            }
            break;
        case MYLITE_SQL_AST_COMPARISON_PREDICATE:
            rc = information_schema_validate_comparison_predicate(database, query, current);
            break;
        case MYLITE_SQL_AST_IS_NULL_PREDICATE:
            rc = information_schema_validate_is_null_predicate(database, query, current);
            break;
        case MYLITE_SQL_AST_BETWEEN_PREDICATE:
            rc = information_schema_validate_between_predicate(database, query, current);
            break;
        case MYLITE_SQL_AST_IN_PREDICATE:
            rc = information_schema_validate_in_predicate(database, query, current);
            break;
        default:
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA WHERE supports metadata predicates"
            );
            rc = MYLITE_ERROR;
            break;
        }
    }

    information_schema_predicate_frame_stack_deinit(&frame_stack);
    return rc;
}

static int information_schema_validate_comparison_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    struct information_schema_predicate_value right = {
        .text = NULL,
        .is_null = false,
        .is_numeric = false,
    };
    size_t column_index = 0U;
    int rc = information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );

    (void)column_index;
    if (rc == MYLITE_OK) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
            rc = information_schema_predicate_like_pattern_text(
                database,
                child_at(predicate_node, 1U),
                &right
            );
        } else {
            rc = information_schema_predicate_value_text(
                database,
                child_at(predicate_node, 1U),
                &right
            );
        }
    }
    free(right.text);
    return rc;
}

static int information_schema_validate_is_null_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    size_t column_index = 0U;

    return information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );
}

static int information_schema_validate_between_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    struct information_schema_predicate_value lower = {0};
    struct information_schema_predicate_value upper = {0};
    size_t column_index = 0U;
    int rc = information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );

    (void)column_index;
    if (rc == MYLITE_OK) {
        rc = information_schema_predicate_literal_value_text(
            database,
            child_at(predicate_node, 1U),
            &lower
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_predicate_literal_value_text(
            database,
            child_at(predicate_node, 2U),
            &upper
        );
    }
    free(lower.text);
    free(upper.text);
    return rc;
}

static int information_schema_validate_in_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    const struct mylite_sql_ast_node *values = child_at(predicate_node, 1U);
    const struct mylite_sql_ast_node *value = NULL;
    size_t column_index = 0U;
    int rc = information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );

    (void)column_index;
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (values == NULL || values->kind == MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "INFORMATION_SCHEMA WHERE does not support IN subqueries");
        return MYLITE_ERROR;
    }
    if (values->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST) {
        set_unsupported_error(database, "INFORMATION_SCHEMA WHERE supports literal IN lists");
        return MYLITE_ERROR;
    }

    value = child_at(values, 0U);
    while (rc == MYLITE_OK && value != NULL) {
        struct information_schema_predicate_value item = {0};

        rc = information_schema_predicate_literal_value_text(database, value, &item);
        free(item.text);
        value = value->next_sibling;
    }
    return rc;
}

static int information_schema_predicate_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    char **row,
    bool *out_matches
) {
    struct information_schema_predicate_frame_stack frame_stack = {0};
    struct information_schema_truth_stack truth_stack = {0};
    struct information_schema_predicate_eval_frame frame = {
        .node = predicate_node,
        .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
    };
    int rc = information_schema_predicate_frame_stack_push(database, &frame_stack, frame);

    *out_matches = false;
    while (rc == MYLITE_OK && information_schema_predicate_frame_stack_pop(&frame_stack, &frame)) {
        const struct mylite_sql_ast_node *current = unwrap_parenthesized_predicate(frame.node);

        if (current == NULL) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA WHERE supports metadata predicates"
            );
            rc = MYLITE_ERROR;
        } else if (frame.action == INFORMATION_SCHEMA_PREDICATE_VISIT) {
            rc = information_schema_visit_predicate(database, &frame_stack, current);
        } else {
            rc = information_schema_evaluate_predicate(database, query, current, row, &truth_stack);
        }
    }
    if (rc == MYLITE_OK) {
        enum information_schema_truth_value truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;

        if (truth_stack.count != 1U || !information_schema_truth_stack_pop(&truth_stack, &truth)) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA predicate state");
            rc = MYLITE_ERROR;
        } else {
            *out_matches = information_schema_truth_is_true(truth);
        }
    }

    information_schema_truth_stack_deinit(&truth_stack);
    information_schema_predicate_frame_stack_deinit(&frame_stack);
    return rc;
}

static void information_schema_predicate_frame_stack_deinit(
    struct information_schema_predicate_frame_stack *stack
) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    *stack = (struct information_schema_predicate_frame_stack){0};
}

static int information_schema_predicate_frame_stack_push(
    struct mylite_db *database,
    struct information_schema_predicate_frame_stack *stack,
    struct information_schema_predicate_eval_frame frame
) {
    struct information_schema_predicate_eval_frame *grown_items = NULL;
    size_t new_capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;

    if (stack->count < stack->capacity) {
        stack->items[stack->count] = frame;
        ++stack->count;
        return MYLITE_OK;
    }
    if (new_capacity < stack->capacity || new_capacity > SIZE_MAX / sizeof(*grown_items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    grown_items = realloc(stack->items, new_capacity * sizeof(*grown_items));
    if (grown_items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    stack->items = grown_items;
    stack->capacity = new_capacity;
    stack->items[stack->count] = frame;
    ++stack->count;
    return MYLITE_OK;
}

static bool information_schema_predicate_frame_stack_pop(
    struct information_schema_predicate_frame_stack *stack,
    struct information_schema_predicate_eval_frame *out_frame
) {
    if (stack == NULL || stack->count == 0U) {
        return false;
    }
    --stack->count;
    *out_frame = stack->items[stack->count];
    return true;
}

static void information_schema_truth_stack_deinit(struct information_schema_truth_stack *stack) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    *stack = (struct information_schema_truth_stack){0};
}

static int information_schema_truth_stack_push(
    struct mylite_db *database,
    struct information_schema_truth_stack *stack,
    enum information_schema_truth_value value
) {
    enum information_schema_truth_value *grown_items = NULL;
    size_t new_capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;

    if (stack->count < stack->capacity) {
        stack->items[stack->count] = value;
        ++stack->count;
        return MYLITE_OK;
    }
    if (new_capacity < stack->capacity || new_capacity > SIZE_MAX / sizeof(*grown_items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    grown_items = realloc(stack->items, new_capacity * sizeof(*grown_items));
    if (grown_items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    stack->items = grown_items;
    stack->capacity = new_capacity;
    stack->items[stack->count] = value;
    ++stack->count;
    return MYLITE_OK;
}

static bool information_schema_truth_stack_pop(
    struct information_schema_truth_stack *stack,
    enum information_schema_truth_value *out_value
) {
    if (stack == NULL || stack->count == 0U) {
        return false;
    }
    --stack->count;
    *out_value = stack->items[stack->count];
    return true;
}

static int information_schema_visit_predicate(
    struct mylite_db *database,
    struct information_schema_predicate_frame_stack *frame_stack,
    const struct mylite_sql_ast_node *predicate_node
) {
    int rc = information_schema_predicate_frame_stack_push(
        database,
        frame_stack,
        (struct information_schema_predicate_eval_frame){
            .node = predicate_node,
            .action = INFORMATION_SCHEMA_PREDICATE_EVALUATE,
        }
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (predicate_node->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        return information_schema_predicate_frame_stack_push(
            database,
            frame_stack,
            (struct information_schema_predicate_eval_frame){
                .node = child_at(predicate_node, 0U),
                .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
            }
        );
    }
    if (predicate_node->kind == MYLITE_SQL_AST_AND_PREDICATE ||
        predicate_node->kind == MYLITE_SQL_AST_OR_PREDICATE ||
        predicate_node->kind == MYLITE_SQL_AST_XOR_PREDICATE) {
        rc = information_schema_predicate_frame_stack_push(
            database,
            frame_stack,
            (struct information_schema_predicate_eval_frame){
                .node = child_at(predicate_node, 1U),
                .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
            }
        );
        if (rc == MYLITE_OK) {
            rc = information_schema_predicate_frame_stack_push(
                database,
                frame_stack,
                (struct information_schema_predicate_eval_frame){
                    .node = child_at(predicate_node, 0U),
                    .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
                }
            );
        }
    }
    return rc;
}

static int information_schema_evaluate_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    char **row,
    struct information_schema_truth_stack *truth_stack
) {
    enum information_schema_truth_value truth = INFORMATION_SCHEMA_TRUTH_FALSE;
    int rc = MYLITE_OK;

    if (predicate_node->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        rc = information_schema_comparison_matches(database, query, predicate_node, row, &truth);
    } else if (predicate_node->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        rc = information_schema_is_null_predicate_matches(
            database,
            query,
            predicate_node,
            row,
            &truth
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_BETWEEN_PREDICATE) {
        rc = information_schema_between_matches(database, query, predicate_node, row, &truth);
    } else if (predicate_node->kind == MYLITE_SQL_AST_IN_PREDICATE) {
        rc = information_schema_in_matches(database, query, predicate_node, row, &truth);
    } else if (predicate_node->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        enum information_schema_truth_value child_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;

        if (!information_schema_truth_stack_pop(truth_stack, &child_truth)) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA predicate stack");
            return MYLITE_ERROR;
        }
        if (child_truth == INFORMATION_SCHEMA_TRUTH_TRUE) {
            truth = INFORMATION_SCHEMA_TRUTH_FALSE;
        } else if (child_truth == INFORMATION_SCHEMA_TRUTH_FALSE) {
            truth = INFORMATION_SCHEMA_TRUTH_TRUE;
        } else {
            truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
        }
    } else if (
        predicate_node->kind == MYLITE_SQL_AST_AND_PREDICATE ||
        predicate_node->kind == MYLITE_SQL_AST_OR_PREDICATE ||
        predicate_node->kind == MYLITE_SQL_AST_XOR_PREDICATE
    ) {
        rc = information_schema_compound_predicate_matches(
            database,
            predicate_node->kind,
            truth_stack,
            &truth
        );
    } else {
        set_unsupported_error(database, "INFORMATION_SCHEMA WHERE supports metadata predicates");
        return MYLITE_ERROR;
    }

    if (rc == MYLITE_OK) {
        rc = information_schema_truth_stack_push(database, truth_stack, truth);
    }
    return rc;
}

static int information_schema_is_null_predicate_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    char **row,
    enum information_schema_truth_value *out_truth
) {
    size_t column_index = 0U;
    int rc = information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );

    if (rc == MYLITE_OK) {
        bool is_null = row[column_index] == NULL;
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate_node);

        *out_truth = information_schema_truth_from_bool(is_null);
        if (operator_kind != MYLITE_SQL_AST_OPERATOR_IS_NULL) {
            if (is_null) {
                *out_truth = INFORMATION_SCHEMA_TRUTH_FALSE;
            } else {
                *out_truth = INFORMATION_SCHEMA_TRUTH_TRUE;
            }
        }
    }
    return rc;
}

static int information_schema_compound_predicate_matches(
    struct mylite_db *database,
    enum mylite_sql_ast_node_kind predicate_kind,
    struct information_schema_truth_stack *truth_stack,
    enum information_schema_truth_value *out_truth
) {
    enum information_schema_truth_value right_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
    enum information_schema_truth_value left_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;

    if (!information_schema_truth_stack_pop(truth_stack, &right_truth) ||
        !information_schema_truth_stack_pop(truth_stack, &left_truth)) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA predicate stack");
        return MYLITE_ERROR;
    }
    if (predicate_kind == MYLITE_SQL_AST_AND_PREDICATE) {
        if (left_truth == INFORMATION_SCHEMA_TRUTH_FALSE ||
            right_truth == INFORMATION_SCHEMA_TRUTH_FALSE) {
            *out_truth = INFORMATION_SCHEMA_TRUTH_FALSE;
        } else if (
            left_truth == INFORMATION_SCHEMA_TRUTH_TRUE &&
            right_truth == INFORMATION_SCHEMA_TRUTH_TRUE
        ) {
            *out_truth = INFORMATION_SCHEMA_TRUTH_TRUE;
        } else {
            *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
        }
    } else if (predicate_kind == MYLITE_SQL_AST_OR_PREDICATE) {
        if (left_truth == INFORMATION_SCHEMA_TRUTH_TRUE ||
            right_truth == INFORMATION_SCHEMA_TRUTH_TRUE) {
            *out_truth = INFORMATION_SCHEMA_TRUTH_TRUE;
        } else if (
            left_truth == INFORMATION_SCHEMA_TRUTH_FALSE &&
            right_truth == INFORMATION_SCHEMA_TRUTH_FALSE
        ) {
            *out_truth = INFORMATION_SCHEMA_TRUTH_FALSE;
        } else {
            *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
        }
    } else if (
        left_truth == INFORMATION_SCHEMA_TRUTH_UNKNOWN ||
        right_truth == INFORMATION_SCHEMA_TRUTH_UNKNOWN
    ) {
        *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
    } else {
        *out_truth = information_schema_truth_from_bool(left_truth != right_truth);
    }
    return MYLITE_OK;
}

static enum information_schema_truth_value information_schema_truth_from_bool(bool condition) {
    if (condition) {
        return INFORMATION_SCHEMA_TRUTH_TRUE;
    }
    return INFORMATION_SCHEMA_TRUTH_FALSE;
}

static bool information_schema_truth_is_true(enum information_schema_truth_value truth) {
    return truth == INFORMATION_SCHEMA_TRUTH_TRUE;
}

static int information_schema_comparison_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    char **row,
    enum information_schema_truth_value *out_truth
) {
    size_t column_index = 0U;
    struct information_schema_predicate_value right = {
        .text = NULL,
        .is_null = false,
        .is_numeric = false,
    };
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate_node);
    const char *left_text = NULL;
    int rc = information_schema_comparison_predicate_value(
        database,
        query,
        predicate_node,
        operator_kind,
        &column_index,
        &right
    );

    if (rc != MYLITE_OK) {
        free(right.text);
        return rc;
    }

    left_text = row[column_index];
    rc = information_schema_comparison_truth(
        database,
        query,
        column_index,
        left_text,
        &right,
        operator_kind,
        out_truth
    );

    free(right.text);
    return rc;
}

static int information_schema_comparison_predicate_value(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    enum mylite_sql_ast_operator operator_kind,
    size_t *out_column_index,
    struct information_schema_predicate_value *out_right
) {
    int rc = information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        out_column_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
        return information_schema_predicate_like_pattern_text(
            database,
            child_at(predicate_node, 1U),
            out_right
        );
    }
    return information_schema_predicate_value_text(
        database,
        child_at(predicate_node, 1U),
        out_right
    );
}

static int information_schema_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
) {
    *out_truth = INFORMATION_SCHEMA_TRUTH_FALSE;
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
        return information_schema_like_comparison_truth(
            database,
            query,
            column_index,
            left_text,
            right,
            out_truth
        );
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        return information_schema_null_safe_comparison_truth(
            database,
            query,
            column_index,
            left_text,
            right,
            operator_kind,
            out_truth
        );
    }
    return information_schema_regular_comparison_truth(
        database,
        query,
        column_index,
        left_text,
        right,
        operator_kind,
        out_truth
    );
}

static int information_schema_like_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum information_schema_truth_value *out_truth
) {
    bool like_matches = false;
    int rc = MYLITE_OK;

    if (left_text == NULL || right->is_null) {
        *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
        return MYLITE_OK;
    }

    rc = information_schema_like_comparison_matches(
        database,
        &query->definition->columns[column_index],
        left_text,
        right->text,
        &like_matches
    );
    if (rc == MYLITE_OK) {
        *out_truth = information_schema_truth_from_bool(like_matches);
    }
    return rc;
}

static int information_schema_null_safe_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
) {
    if (left_text == NULL || right->is_null) {
        bool both_null = ((left_text == NULL) && right->is_null) != 0;

        *out_truth = information_schema_truth_from_bool(both_null);
        return MYLITE_OK;
    }

    return information_schema_nonnull_comparison_truth(
        database,
        query,
        column_index,
        left_text,
        right,
        operator_kind,
        out_truth
    );
}

static int information_schema_regular_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
) {
    if (left_text == NULL || right->is_null) {
        *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
        return MYLITE_OK;
    }

    return information_schema_nonnull_comparison_truth(
        database,
        query,
        column_index,
        left_text,
        right,
        operator_kind,
        out_truth
    );
}

static int information_schema_nonnull_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
) {
    bool comparison_matches = false;
    int rc = information_schema_compare_nonnull_value_matches(
        database,
        query,
        column_index,
        left_text,
        right,
        operator_kind,
        &comparison_matches
    );

    if (rc == MYLITE_OK) {
        *out_truth = information_schema_truth_from_bool(comparison_matches);
    }
    return rc;
}

static int information_schema_like_comparison_matches(
    struct mylite_db *database,
    const struct mylite_execution_catalog_column_definition *column,
    const char *left_text,
    const char *right_text,
    bool *out_matches
) {
    bool case_sensitive = true;
    bool backslash_escapes = true;

    if (information_schema_column_uses_case_insensitive_collation(column)) {
        case_sensitive = false;
    }
    if (session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES)) {
        backslash_escapes = false;
    }

    *out_matches = show_like_pattern_matches_with_escape(
        right_text,
        strlen(right_text),
        left_text,
        strlen(left_text),
        case_sensitive,
        backslash_escapes
    );
    return MYLITE_OK;
}

static int information_schema_compare_nonnull_value_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    bool *out_matches
) {
    if (information_schema_column_is_numeric(query->definition, column_index)) {
        return information_schema_numeric_comparison_matches(
            database,
            operator_kind,
            left_text,
            right->text,
            out_matches
        );
    }

    return information_schema_text_comparison_matches(
        database,
        &query->definition->columns[column_index],
        operator_kind,
        left_text,
        right->text,
        right->is_numeric,
        out_matches
    );
}

static int information_schema_between_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    char **row,
    enum information_schema_truth_value *out_truth
) {
    struct information_schema_predicate_value lower = {0};
    struct information_schema_predicate_value upper = {0};
    size_t column_index = 0U;
    const char *left_text = NULL;
    int rc = information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );

    *out_truth = INFORMATION_SCHEMA_TRUTH_FALSE;
    if (rc == MYLITE_OK) {
        rc = information_schema_predicate_literal_value_text(
            database,
            child_at(predicate_node, 1U),
            &lower
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_predicate_literal_value_text(
            database,
            child_at(predicate_node, 2U),
            &upper
        );
    }
    if (rc != MYLITE_OK) {
        free(lower.text);
        free(upper.text);
        return rc;
    }

    left_text = row[column_index];
    if (left_text == NULL || lower.is_null || upper.is_null) {
        *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
    } else {
        bool lower_matches = false;
        bool upper_matches = false;

        rc = information_schema_between_bound_matches(
            database,
            query,
            column_index,
            left_text,
            &lower,
            MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL,
            &lower_matches
        );
        if (rc == MYLITE_OK) {
            rc = information_schema_between_bound_matches(
                database,
                query,
                column_index,
                left_text,
                &upper,
                MYLITE_SQL_AST_OPERATOR_LESS_EQUAL,
                &upper_matches
            );
        }
        if (rc == MYLITE_OK) {
            *out_truth = information_schema_truth_from_bool((lower_matches && upper_matches) != 0);
        }
    }

    free(lower.text);
    free(upper.text);
    return rc;
}

static int information_schema_between_bound_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    bool *out_matches
) {
    int comparison = 0;

    if (information_schema_column_is_numeric(query->definition, column_index)) {
        return information_schema_numeric_comparison_matches(
            database,
            operator_kind,
            left_text,
            right->text,
            out_matches
        );
    }

    comparison = information_schema_compare_column_text(
        &query->definition->columns[column_index],
        left_text,
        right->text
    );
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LESS_EQUAL) {
        *out_matches = ((comparison <= 0) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL) {
        *out_matches = ((comparison >= 0) != 0);
    } else {
        set_unsupported_error(database, "INFORMATION_SCHEMA WHERE BETWEEN supports bound ordering");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int information_schema_in_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    char **row,
    enum information_schema_truth_value *out_truth
) {
    const struct mylite_sql_ast_node *values = child_at(predicate_node, 1U);
    const struct mylite_sql_ast_node *value = NULL;
    size_t column_index = 0U;
    const char *left_text = NULL;
    bool saw_null = false;
    int rc = information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );

    *out_truth = INFORMATION_SCHEMA_TRUTH_FALSE;
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (values == NULL || values->kind == MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "INFORMATION_SCHEMA WHERE does not support IN subqueries");
        return MYLITE_ERROR;
    }
    if (values->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST) {
        set_unsupported_error(database, "INFORMATION_SCHEMA WHERE supports literal IN lists");
        return MYLITE_ERROR;
    }

    left_text = row[column_index];
    if (left_text == NULL) {
        *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
        return MYLITE_OK;
    }

    value = child_at(values, 0U);
    while (rc == MYLITE_OK && value != NULL) {
        struct information_schema_predicate_value item = {0};

        rc = information_schema_predicate_literal_value_text(database, value, &item);
        if (rc == MYLITE_OK) {
            if (item.is_null) {
                saw_null = true;
            } else {
                bool item_matches = false;

                rc = information_schema_compare_nonnull_value_matches(
                    database,
                    query,
                    column_index,
                    left_text,
                    &item,
                    MYLITE_SQL_AST_OPERATOR_EQUAL,
                    &item_matches
                );
                if (rc == MYLITE_OK && item_matches) {
                    *out_truth = INFORMATION_SCHEMA_TRUTH_TRUE;
                    free(item.text);
                    return MYLITE_OK;
                }
            }
        }
        free(item.text);
        value = value->next_sibling;
    }
    if (rc == MYLITE_OK && saw_null) {
        *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
    }
    return rc;
}

static int information_schema_numeric_comparison_matches(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const char *left_text,
    const char *right_text,
    bool *out_matches
) {
    int64_t left_value = 0;
    int64_t right_value = 0;

    if (information_schema_text_to_i64(left_text, &left_value) != MYLITE_OK ||
        information_schema_text_to_i64(right_text, &right_value) != MYLITE_OK) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE comparison requires integer metadata values"
        );
        return MYLITE_ERROR;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_EQUAL ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        *out_matches = ((left_value == right_value) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_EQUAL) {
        *out_matches = ((left_value != right_value) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_LESS) {
        *out_matches = ((left_value < right_value) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_LESS_EQUAL) {
        *out_matches = ((left_value <= right_value) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER) {
        *out_matches = ((left_value > right_value) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL) {
        *out_matches = ((left_value >= right_value) != 0);
    } else {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE comparison supports equality and ordering operators"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int information_schema_text_comparison_matches(
    struct mylite_db *database,
    const struct mylite_execution_catalog_column_definition *column,
    enum mylite_sql_ast_operator operator_kind,
    const char *left_text,
    const char *right_text,
    bool right_is_numeric,
    bool *out_matches
) {
    int comparison = 0;

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_EQUAL ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        *out_matches = information_schema_compare_column_text(column, left_text, right_text) == 0;
        return MYLITE_OK;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_EQUAL) {
        *out_matches = information_schema_compare_column_text(column, left_text, right_text) != 0;
        return MYLITE_OK;
    }
    if (!right_is_numeric) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE string predicates support only equality comparisons"
        );
        return MYLITE_ERROR;
    }

    comparison = information_schema_compare_column_text(column, left_text, right_text);
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LESS) {
        *out_matches = ((comparison < 0) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_LESS_EQUAL) {
        *out_matches = ((comparison <= 0) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER) {
        *out_matches = ((comparison > 0) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL) {
        *out_matches = ((comparison >= 0) != 0);
    } else {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE comparison supports equality and ordering operators"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int information_schema_predicate_value_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
) {
    *out_value = (struct information_schema_predicate_value){
        .text = NULL,
        .is_null = false,
        .is_numeric = false,
    };
    if (value_node == NULL) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE supports string, integer, DATABASE(), and SCHEMA() values"
        );
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_DATABASE_FUNCTION ||
        value_node->kind == MYLITE_SQL_AST_SCHEMA_FUNCTION) {
        return information_schema_predicate_function_value(database, value_node, out_value);
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_value->is_null = true;
        return MYLITE_OK;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING) {
        size_t text_length = 0U;

        return decode_sql_string_literal(
            database,
            value_node,
            "INFORMATION_SCHEMA WHERE supports only string literals",
            "INFORMATION_SCHEMA WHERE string literals do not support NUL bytes",
            &out_value->text,
            &text_length
        );
    }

    return information_schema_predicate_integer_value_text(database, value_node, out_value);
}

static int information_schema_predicate_literal_value_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
) {
    if (value_node != NULL && (value_node->kind == MYLITE_SQL_AST_DATABASE_FUNCTION ||
                               value_node->kind == MYLITE_SQL_AST_SCHEMA_FUNCTION)) {
        *out_value = (struct information_schema_predicate_value){0};
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE IN and BETWEEN support only literal values"
        );
        return MYLITE_ERROR;
    }
    return information_schema_predicate_value_text(database, value_node, out_value);
}

static int information_schema_predicate_like_pattern_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
) {
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    *out_value = (struct information_schema_predicate_value){
        .text = NULL,
        .is_null = false,
        .is_numeric = false,
    };
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE LIKE supports only string pattern literals"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "INFORMATION_SCHEMA WHERE LIKE supports only string pattern literals",
        "INFORMATION_SCHEMA WHERE LIKE pattern literals do not support NUL bytes",
        &out_value->text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_value_is_supported_string_key(out_value->text, text_length)) {
        free(out_value->text);
        *out_value = (struct information_schema_predicate_value){0};
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE LIKE pattern literals support only ASCII text"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int information_schema_predicate_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
) {
    struct session_scalar_cell cell = {0};
    int rc = session_scalar_value(database, value_node, &cell);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (cell.value == NULL) {
        out_value->is_null = true;
        return MYLITE_OK;
    }
    return duplicate_text(database, cell.value, &out_value->text);
}

static int information_schema_predicate_integer_value_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    char buffer[integer_text_capacity];
    int rc = MYLITE_OK;

    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA WHERE supports only decimal integer predicate values"
            );
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (!boolean_literal_magnitude(literal, &magnitude)) {
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA WHERE supports string, integer, DATABASE(), and SCHEMA() values"
            );
            return MYLITE_ERROR;
        }
        rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
        if (rc != MYLITE_OK) {
            set_limit_out_of_range_error(database);
            return MYLITE_ERROR;
        }
    }

    rc = information_schema_format_signed_magnitude(
        database,
        is_negative,
        magnitude,
        buffer,
        sizeof(buffer)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_value->is_numeric = true;
    return duplicate_text(database, buffer, &out_value->text);
}

static int information_schema_format_signed_magnitude(
    struct mylite_db *database,
    bool is_negative,
    uint64_t magnitude,
    char *buffer,
    size_t buffer_size
) {
    const uint64_t int64_positive_max = (uint64_t)INT64_MAX;
    const uint64_t int64_negative_abs_max = ((uint64_t)INT64_MAX) + 1U;
    int written = 0;

    if (is_negative) {
        if (magnitude > int64_negative_abs_max) {
            set_limit_out_of_range_error(database);
            return MYLITE_ERROR;
        }
        if (magnitude == int64_negative_abs_max) {
            written = snprintf(buffer, buffer_size, "%" PRId64, INT64_MIN);
        } else {
            written = snprintf(buffer, buffer_size, "-%" PRIu64, magnitude);
        }
    } else {
        if (magnitude > int64_positive_max) {
            set_limit_out_of_range_error(database);
            return MYLITE_ERROR;
        }
        written = snprintf(buffer, buffer_size, "%" PRIu64, magnitude);
    }
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format INFORMATION_SCHEMA predicate value");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int information_schema_resolve_column_reference(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *column_node,
    enum column_reference_diagnostic_context diagnostic_context,
    size_t *out_column_index
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *display_text = NULL;
    size_t part_count = 0U;
    const char *column_name = NULL;
    bool qualifier_matches = false;
    int rc = collect_identifier_parts(
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
        column_name = parts[0];
    } else {
        if (part_count == 2U) {
            if (query->has_alias) {
                qualifier_matches = text_equals_ascii_case_insensitive(parts[0], query->alias);
            } else {
                qualifier_matches =
                    text_equals_ascii_case_insensitive(parts[0], query->definition->name);
            }
        }
        if (qualifier_matches) {
            column_name = parts[1];
        } else {
            rc = information_schema_column_reference_text(database, column_node, &display_text);
            if (rc == MYLITE_OK) {
                set_unknown_column_reference_error(database, diagnostic_context, display_text);
                rc = MYLITE_ERROR;
            }
            free(display_text);
            return rc;
        }
    }

    rc =
        information_schema_table_definition_index(query->definition, column_name, out_column_index);
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    rc = information_schema_column_reference_text(database, column_node, &display_text);
    if (rc == MYLITE_OK) {
        set_unknown_column_reference_error(database, diagnostic_context, display_text);
        rc = MYLITE_ERROR;
    }
    free(display_text);
    return rc;
}

static int information_schema_column_reference_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char **out_text
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_dynamic_string string;
    size_t part_count = 0U;
    int rc = collect_identifier_parts(
        column_node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    mylite_dynamic_string_init(&string);
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append_char(&string, '.');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, parts[part_index]);
        }
    }
    if (rc == MYLITE_OK) {
        *out_text = mylite_dynamic_string_take(&string);
        if (*out_text == NULL) {
            rc = MYLITE_NOMEM;
            set_nomem_error(database);
        }
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int information_schema_plan_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    struct information_schema_query *out_query
) {
    const struct mylite_sql_ast_node *item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (select_list_is_wildcard(select_list)) {
        for (size_t column_index = 0U; column_index < out_query->definition->column_count;
             ++column_index) {
            int rc = information_schema_append_projection(database, out_query, column_index, NULL);

            if (rc != MYLITE_OK) {
                return rc;
            }
        }
        return MYLITE_OK;
    }

    item = child_at(select_list, 0U);
    while (item != NULL) {
        const struct mylite_sql_ast_node *expression =
            unwrap_parenthesized_expression(child_at(item, 0U));
        enum planned_count_function count_function = count_function_from_expression(expression);
        size_t column_index = 0U;
        int rc = MYLITE_OK;

        if (count_function == PLANNED_COUNT_STAR) {
            if (mylite_sql_ast_node_child_count(select_list) != 1U) {
                set_unsupported_error(
                    database,
                    "INFORMATION_SCHEMA SELECT supports either COUNT(*) or metadata columns"
                );
                return MYLITE_ERROR;
            }
            out_query->is_count_star = true;
            out_query->count_expression = expression;
            out_query->count_alias = child_at(item, 1U);
            return MYLITE_OK;
        }
        if (count_function != PLANNED_COUNT_NONE ||
            (expression == NULL || (expression->kind != MYLITE_SQL_AST_IDENTIFIER &&
                                    expression->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER))) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA SELECT supports only metadata columns and COUNT(*)"
            );
            return MYLITE_ERROR;
        }

        rc = information_schema_resolve_column_reference(
            database,
            out_query,
            expression,
            COLUMN_REFERENCE_FIELD,
            &column_index
        );
        if (rc == MYLITE_OK) {
            rc = information_schema_append_projection(
                database,
                out_query,
                column_index,
                child_at(item, 1U)
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        item = item->next_sibling;
    }

    return MYLITE_OK;
}

static int information_schema_append_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    size_t column_index,
    const struct mylite_sql_ast_node *alias
) {
    size_t required_count = query->projection_count + 1U;
    size_t *indexes = NULL;
    char **names = NULL;
    char *column_name = NULL;
    int rc = MYLITE_OK;

    if (required_count > SIZE_MAX / sizeof(*indexes) ||
        required_count > SIZE_MAX / sizeof(*names)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    indexes = realloc(query->projection_indexes, required_count * sizeof(*indexes));
    if (indexes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    query->projection_indexes = indexes;

    names = (char **)realloc((void *)query->projection_names, required_count * sizeof(*names));
    if (names == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    query->projection_names = names;

    if (alias != NULL) {
        rc = copy_select_item_alias_text(database, alias, &column_name);
    } else {
        rc = duplicate_text(database, query->definition->columns[column_index].name, &column_name);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    query->projection_indexes[query->projection_count] = column_index;
    query->projection_names[query->projection_count] = column_name;
    query->projection_count = required_count;
    return MYLITE_OK;
}

static int information_schema_plan_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    struct information_schema_query *out_query
) {
    const struct mylite_sql_ast_node *direction = NULL;
    int rc = MYLITE_OK;

    out_query->has_order = false;
    out_query->order_direction = PLANNED_SELECT_ORDER_DEFAULT;
    if (order_clause == NULL) {
        return MYLITE_OK;
    }
    if (order_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
        set_unsupported_error(database, "INFORMATION_SCHEMA SELECT supports one ORDER BY column");
        return MYLITE_ERROR;
    }
    rc = information_schema_resolve_column_reference(
        database,
        out_query,
        child_at(order_clause, 0U),
        COLUMN_REFERENCE_ORDER,
        &out_query->order_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_query->has_order = true;
    out_query->order_direction = PLANNED_SELECT_ORDER_ASC;
    direction = child_at(order_clause, 1U);
    if (mylite_sql_ast_node_order_direction(direction) == MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
        out_query->order_direction = PLANNED_SELECT_ORDER_DESC;
    }
    return MYLITE_OK;
}

static int information_schema_plan_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct information_schema_query *out_query
) {
    out_query->limit = (struct planned_select_limit){
        .has_limit = false,
        .row_count = 0,
        .has_offset = false,
        .offset = 0,
    };
    if (limit_clause == NULL) {
        return MYLITE_OK;
    }
    if (limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE || child_at(limit_clause, 1U) != NULL) {
        set_unsupported_error(database, "INFORMATION_SCHEMA SELECT supports only LIMIT row_count");
        return MYLITE_ERROR;
    }

    return plan_delete_limit(database, limit_clause, &out_query->limit);
}

static int information_schema_resolve_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct information_schema_query *out_query
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const char *table_name = NULL;
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT requires a schema-qualified metadata table"
        );
        return MYLITE_ERROR;
    }

    rc = collect_identifier_parts(
        child_at(from_clause, 0U),
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 2U && schema_name_is_information_schema(parts[0])) {
        table_name = parts[1];
    } else if (part_count == 1U && selected_schema_is_information_schema(database)) {
        table_name = parts[0];
    } else {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT requires a schema-qualified metadata table"
        );
        return MYLITE_ERROR;
    }

    out_query->definition = find_information_schema_table_definition(table_name);
    if (out_query->definition == NULL) {
        set_unknown_information_schema_table_error(database, table_name);
        return MYLITE_ERROR;
    }
    if (from_table_index_hint_list_node(from_clause) != NULL) {
        set_unsupported_error(database, "INFORMATION_SCHEMA SELECT does not support index hints");
        return MYLITE_ERROR;
    }
    if (from_table_alias_node(from_clause) != NULL) {
        rc = copy_identifier_text(
            from_table_alias_node(from_clause),
            out_query->alias,
            sizeof(out_query->alias),
            database
        );
        if (rc == MYLITE_OK) {
            out_query->has_alias = true;
        }
    }
    return rc;
}

static const struct mylite_execution_catalog_table_definition *find_information_schema_table_definition(
    const char *table_name
) {
    return mylite_execution_catalog_information_schema_table_definition_by_name(table_name);
}

static int information_schema_table_definition_index(
    const struct mylite_execution_catalog_table_definition *definition,
    const char *column_name,
    size_t *out_index
) {
    for (size_t column_index = 0U; column_index < definition->column_count; ++column_index) {
        if (text_equals_ascii_case_insensitive(
                definition->columns[column_index].name,
                column_name
            )) {
            *out_index = column_index;
            return MYLITE_OK;
        }
    }
    return MYLITE_ERROR;
}

static int information_schema_compare_text(
    const struct mylite_execution_catalog_table_definition *definition,
    size_t column_index,
    const char *left,
    const char *right
) {
    return information_schema_compare_column_text(&definition->columns[column_index], left, right);
}

static int information_schema_compare_column_text(
    const struct mylite_execution_catalog_column_definition *column,
    const char *left,
    const char *right
) {
    if (information_schema_column_uses_case_insensitive_collation(column)) {
        return compare_ascii_case_insensitive_text(left, right);
    }
    return strcmp(left, right);
}

static int compare_ascii_case_insensitive_text(const char *left, const char *right) {
    size_t index = 0U;

    while (left[index] != '\0' && right[index] != '\0') {
        unsigned char left_byte = (unsigned char)ascii_lower((unsigned char)left[index]);
        unsigned char right_byte = (unsigned char)ascii_lower((unsigned char)right[index]);

        if (left_byte < right_byte) {
            return -1;
        }
        if (left_byte > right_byte) {
            return 1;
        }
        ++index;
    }
    if (left[index] == '\0' && right[index] == '\0') {
        return 0;
    }
    if (left[index] == '\0') {
        return -1;
    }
    return 1;
}

static bool information_schema_column_uses_case_insensitive_collation(
    const struct mylite_execution_catalog_column_definition *column
) {
    const char *collation = column->collation_name;

    if (collation == NULL) {
        return false;
    }
    if (text_equals_ascii_case_insensitive(collation, "utf8mb3_general_ci") ||
        text_equals_ascii_case_insensitive(collation, "utf8mb3_tolower_ci") ||
        text_equals_ascii_case_insensitive(collation, "utf8mb4_0900_ai_ci") ||
        text_equals_ascii_case_insensitive(collation, "utf8mb4_general_ci") ||
        text_equals_ascii_case_insensitive(collation, "utf8mb4_unicode_ci") ||
        text_equals_ascii_case_insensitive(collation, "utf8mb4_unicode_520_ci")) {
        return true;
    }
    return false;
}

static bool information_schema_column_is_numeric(
    const struct mylite_execution_catalog_table_definition *definition,
    size_t column_index
) {
    const char *data_type = definition->columns[column_index].data_type;

    if (data_type == NULL) {
        return false;
    }
    if (strcmp(data_type, "int") == 0 || strcmp(data_type, "bigint") == 0) {
        return true;
    }
    return false;
}

static int information_schema_text_to_i64(const char *text, int64_t *out_value) {
    char *end = NULL;
    long long value = 0;

    if (text == NULL || text[0] == '\0') {
        return MYLITE_ERROR;
    }
    value = strtoll(text, &end, decimal_base);
    if (end == NULL || *end != '\0') {
        return MYLITE_ERROR;
    }
    *out_value = (int64_t)value;
    return MYLITE_OK;
}

static int information_schema_format_i64(
    struct mylite_db *database,
    int64_t value,
    char *buffer,
    size_t buffer_size
) {
    int written = snprintf(buffer, buffer_size, "%" PRId64, value);

    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format INFORMATION_SCHEMA value");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int information_schema_character_metadata_for_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    char *character_length_text,
    size_t character_length_text_size,
    char *character_octet_text,
    size_t character_octet_text_size,
    struct information_schema_character_metadata *out_metadata
) {
    uint64_t character_length = 0U;
    uint64_t character_octet_length = 0U;
    bool has_character_metadata = false;
    bool has_binary_metadata = false;
    int rc = MYLITE_OK;

    *out_metadata = (struct information_schema_character_metadata){0};
    if (column_descriptor_is_char(column)) {
        size_t length = 0U;

        rc = parse_char_descriptor_length(
            database,
            column->logical_type,
            "INFORMATION_SCHEMA.COLUMNS supports only baseline CHAR descriptors",
            &length
        );
        character_length = (uint64_t)length;
        character_octet_length =
            character_length * column_effective_max_bytes_per_character(table, column);
        has_character_metadata = true;
    } else if (column_descriptor_is_varchar(column)) {
        size_t length = 0U;

        rc = parse_varchar_descriptor_length(
            database,
            column->logical_type,
            "INFORMATION_SCHEMA.COLUMNS supports only baseline VARCHAR descriptors",
            &length
        );
        character_length = (uint64_t)length;
        character_octet_length =
            character_length * column_effective_max_bytes_per_character(table, column);
        has_character_metadata = true;
    } else if (column_descriptor_is_text_family(column)) {
        const struct text_family_type_info *info =
            text_family_type_info_for_logical_type(column->logical_type);

        if (info == NULL) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA.COLUMNS supports only baseline TEXT descriptors"
            );
            return MYLITE_ERROR;
        }
        character_length = info->maximum_length;
        character_octet_length = info->maximum_length;
        has_character_metadata = true;
    } else if (column_descriptor_is_enum(column)) {
        struct enum_type_info info = {0};

        rc = enum_type_info_for_logical_type(
            database,
            column->logical_type,
            "INFORMATION_SCHEMA.COLUMNS supports only baseline ENUM descriptors",
            &info
        );
        character_length = (uint64_t)info.max_label_character_length;
        character_octet_length =
            character_length * column_effective_max_bytes_per_character(table, column);
        has_character_metadata = true;
    } else if (column_descriptor_is_set(column)) {
        struct set_type_info info = {0};

        rc = set_type_info_for_logical_type(
            database,
            column->logical_type,
            "INFORMATION_SCHEMA.COLUMNS supports only baseline SET descriptors",
            &info
        );
        character_length = (uint64_t)info.max_display_character_length;
        character_octet_length =
            character_length * column_effective_max_bytes_per_character(table, column);
        has_character_metadata = true;
    } else if (column_descriptor_is_binary_string_family(column)) {
        struct binary_string_type_info storage = {0};
        const struct binary_string_type_info *info =
            binary_string_type_info_for_logical_type(column->logical_type, &storage);

        if (info == NULL) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA.COLUMNS supports only baseline binary descriptors"
            );
            return MYLITE_ERROR;
        }
        character_length = info->maximum_length;
        character_octet_length = info->maximum_length;
        has_character_metadata = true;
        has_binary_metadata = true;
    }
    if (rc != MYLITE_OK || !has_character_metadata) {
        return rc;
    }

    rc = information_schema_format_i64(
        database,
        (int64_t)character_length,
        character_length_text,
        character_length_text_size
    );
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            (int64_t)character_octet_length,
            character_octet_text,
            character_octet_text_size
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    const char *character_set_name = column_effective_character_set_name(table, column);
    const char *collation_name = column_effective_collation_name(table, column);

    *out_metadata = (struct information_schema_character_metadata){
        .character_maximum_length = character_length_text,
        .character_octet_length = character_octet_text,
        .character_set_name = character_set_name,
        .collation_name = collation_name,
    };
    if (has_binary_metadata) {
        out_metadata->character_set_name = NULL;
        out_metadata->collation_name = NULL;
    }
    return MYLITE_OK;
}

static const char *information_schema_data_type_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
) {
    const char *logical_type = column->logical_type;
    const char *spatial_type = NULL;

    if (column_descriptor_is_char(column)) {
        return "char";
    }
    if (column_descriptor_is_varchar(column)) {
        return "varchar";
    }
    if (column_descriptor_is_text_family(column)) {
        return information_schema_text_data_type_for_descriptor(column);
    }
    if (column_descriptor_is_enum(column)) {
        return "enum";
    }
    if (column_descriptor_is_set(column)) {
        return "set";
    }
    if (column_descriptor_is_json(column)) {
        return "json";
    }
    if (spatial_logical_type_display_text(logical_type, &spatial_type)) {
        return spatial_type;
    }
    if (column_descriptor_is_bit(column)) {
        return "bit";
    }
    if (column_descriptor_is_binary_string_family(column)) {
        return information_schema_binary_data_type_for_descriptor(column);
    }
    if (column_descriptor_is_decimal(column)) {
        return "decimal";
    }
    if (column_descriptor_is_approximate(column)) {
        return information_schema_approximate_data_type_for_descriptor(column);
    }
    if (column_descriptor_is_date(column)) {
        return "date";
    }
    if (column_descriptor_is_time(column)) {
        return "time";
    }
    if (column_descriptor_is_datetime(column)) {
        return "datetime";
    }
    if (column_descriptor_is_timestamp(column)) {
        return "timestamp";
    }
    if (column_descriptor_is_year(column)) {
        return "year";
    }
    if (logical_type == NULL) {
        return "";
    }
    return information_schema_integer_data_type_for_descriptor(logical_type);
}

static const char *information_schema_text_data_type_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
) {
    const struct text_family_type_info *info =
        text_family_type_info_for_logical_type(column->logical_type);

    return info == NULL ? "" : info->display_type;
}

static const char *information_schema_binary_data_type_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info =
        binary_string_type_info_for_logical_type(column->logical_type, &storage);
    const char *logical_type = column->logical_type;

    if (info == NULL || logical_type == NULL) {
        return "";
    }
    if (strncmp(logical_type, "BINARY(", binary_logical_prefix_length) == 0) {
        return "binary";
    }
    if (strncmp(logical_type, "VARBINARY(", varbinary_logical_prefix_length) == 0) {
        return "varbinary";
    }
    if (info->display_type == NULL) {
        return "";
    }
    return info->display_type;
}

static const char *information_schema_approximate_data_type_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
) {
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };

    if (approximate_type_info_for_logical_type(column->logical_type, &info) == MYLITE_OK &&
        info.type_class == APPROXIMATE_TYPE_FLOAT) {
        return "float";
    }
    return "double";
}

static const char *information_schema_integer_data_type_for_descriptor(const char *logical_type) {
    if (strcmp(logical_type, "TINYINT") == 0 || strcmp(logical_type, "TINYINT(1)") == 0 ||
        strcmp(logical_type, "TINYINT UNSIGNED") == 0) {
        return "tinyint";
    }
    if (strcmp(logical_type, "SMALLINT") == 0 || strcmp(logical_type, "SMALLINT UNSIGNED") == 0) {
        return "smallint";
    }
    if (strcmp(logical_type, "MEDIUMINT") == 0 || strcmp(logical_type, "MEDIUMINT UNSIGNED") == 0) {
        return "mediumint";
    }
    if (strcmp(logical_type, "BIGINT") == 0 || strcmp(logical_type, "BIGINT UNSIGNED") == 0) {
        return "bigint";
    }
    return "int";
}

static const char *information_schema_numeric_precision_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
) {
    const char *logical_type = column->logical_type;

    if (column_descriptor_is_bit(column) || column_descriptor_is_string_family(column) ||
        column_descriptor_is_enum(column) || column_descriptor_is_set(column) ||
        column_descriptor_is_json(column) || column_descriptor_is_binary_string_family(column) ||
        column_descriptor_is_spatial(column) || column_descriptor_is_decimal(column) ||
        column_descriptor_is_date(column) || column_descriptor_is_time(column) ||
        column_descriptor_is_datetime(column) || column_descriptor_is_timestamp(column) ||
        column_descriptor_is_year(column)) {
        return NULL;
    }
    if (column_descriptor_is_approximate(column)) {
        struct approximate_type_info info = {
            .type_class = APPROXIMATE_TYPE_DOUBLE,
            .is_unsigned = false,
        };

        if (approximate_type_info_for_logical_type(column->logical_type, &info) == MYLITE_OK &&
            info.type_class == APPROXIMATE_TYPE_FLOAT) {
            return "12";
        }
        return "22";
    }
    if (logical_type == NULL) {
        return "10";
    }
    if (strcmp(logical_type, "TINYINT") == 0 || strcmp(logical_type, "TINYINT(1)") == 0 ||
        strcmp(logical_type, "TINYINT UNSIGNED") == 0) {
        return "3";
    }
    if (strcmp(logical_type, "SMALLINT") == 0 || strcmp(logical_type, "SMALLINT UNSIGNED") == 0) {
        return "5";
    }
    if (strcmp(logical_type, "MEDIUMINT") == 0 || strcmp(logical_type, "MEDIUMINT UNSIGNED") == 0) {
        return "7";
    }
    if (strcmp(logical_type, "BIGINT UNSIGNED") == 0) {
        return "20";
    }
    if (strcmp(logical_type, "BIGINT") == 0) {
        return "19";
    }
    return "10";
}

static const char *information_schema_numeric_scale_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column_descriptor_is_bit(column) || column_descriptor_is_string_family(column) ||
        column_descriptor_is_enum(column) || column_descriptor_is_set(column) ||
        column_descriptor_is_json(column) || column_descriptor_is_binary_string_family(column) ||
        column_descriptor_is_spatial(column) || column_descriptor_is_decimal(column) ||
        column_descriptor_is_approximate(column) || column_descriptor_is_date(column) ||
        column_descriptor_is_time(column) || column_descriptor_is_datetime(column) ||
        column_descriptor_is_timestamp(column) || column_descriptor_is_year(column)) {
        return NULL;
    }
    return "0";
}
