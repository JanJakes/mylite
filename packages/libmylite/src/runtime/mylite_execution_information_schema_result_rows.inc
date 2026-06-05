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
    struct information_schema_result_projection_buffers buffers = {0};
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
        rc = information_schema_sort_row_indexes(
            database,
            statement,
            query,
            rows,
            indexes,
            index_count
        );
    }
    if (rc == MYLITE_OK && query->projection_count != 0U) {
        rc = information_schema_result_projection_buffers_init(database, query, &buffers);
    }
    for (size_t output_index = 0U; rc == MYLITE_OK && output_index < index_count; ++output_index) {
        size_t row_index = indexes[output_index];

        if (query->limit.has_limit && visible_count >= (size_t)query->limit.row_count) {
            break;
        }
        rc = information_schema_append_projected_result_row(
            database,
            query,
            (const char *const *)rows->rows[row_index],
            &buffers,
            result
        );
        ++visible_count;
    }
    information_schema_result_projection_buffers_deinit(&buffers);
    if (rc == MYLITE_OK && out_read_row_count != NULL) {
        *out_read_row_count = visible_count;
    }
    free(indexes);
    return rc;
}

static int information_schema_result_projection_buffers_init(
    struct mylite_db *database,
    const struct information_schema_query *query,
    struct information_schema_result_projection_buffers *buffers
) {
    if (query->projection_count == 0U) {
        return MYLITE_OK;
    }
    buffers->values = (const char **)calloc(query->projection_count, sizeof(*buffers->values));
    if (buffers->values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (query->projection_count > SIZE_MAX / integer_text_capacity) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    buffers->computed_values = calloc(query->projection_count, integer_text_capacity);
    if (buffers->computed_values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static void information_schema_result_projection_buffers_deinit(
    struct information_schema_result_projection_buffers *buffers
) {
    if (buffers == NULL) {
        return;
    }
    free(buffers->computed_values);
    free((void *)buffers->values);
    *buffers = (struct information_schema_result_projection_buffers){0};
}

static int information_schema_append_projected_result_row(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    const struct information_schema_result_projection_buffers *buffers,
    mylite_result *result
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < query->projection_count;
         ++column_index) {
        rc = information_schema_projection_value(
            database,
            query,
            row,
            column_index,
            buffers->computed_values + (column_index * integer_text_capacity),
            integer_text_capacity,
            &buffers->values[column_index]
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, buffers->values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static int information_schema_projection_value(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    size_t projection_index,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    *out_value = NULL;
    if (query->projection_kinds[projection_index] == INFORMATION_SCHEMA_PROJECTION_COLUMN) {
        *out_value = row[query->projection_indexes[projection_index]];
        return MYLITE_OK;
    }
    if (query->projection_kinds[projection_index] ==
        INFORMATION_SCHEMA_PROJECTION_UNSIGNED_INTEGER_EXPRESSION) {
        return information_schema_unsigned_projection_expression_value(
            database,
            query,
            row,
            query->projection_expressions[projection_index],
            buffer,
            buffer_size,
            out_value
        );
    }

    set_runtime_error(database, "invalid INFORMATION_SCHEMA projection descriptor");
    return MYLITE_ERROR;
}

static int information_schema_unsigned_projection_expression_value(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    const struct mylite_sql_ast_node *expression,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    double value = 0.0;
    bool is_null = false;
    unsigned long long rounded = 0ULL;
    int written = 0;
    int rc = MYLITE_OK;
    static const double nearest_integer_rounding_offset = 0.5;

    *out_value = NULL;
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA numeric projection expression");
        return MYLITE_ERROR;
    }
    rc = information_schema_numeric_projection_expression_value(
        database,
        query,
        row,
        child_at(expression, 0U),
        &value,
        &is_null
    );
    if (rc != MYLITE_OK || is_null) {
        return rc;
    }
    if (value < 0.0 || value > (double)ULLONG_MAX) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT unsigned numeric projection value is out of range"
        );
        return MYLITE_ERROR;
    }

    rounded = (unsigned long long)floor(value + nearest_integer_rounding_offset);
    written = snprintf(buffer, buffer_size, "%llu", rounded);
    if (written < 0 || (size_t)written >= buffer_size) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    *out_value = buffer;
    return MYLITE_OK;
}

static int information_schema_numeric_projection_expression_value(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    const struct mylite_sql_ast_node *expression,
    double *out_value,
    bool *out_is_null
) {
    struct information_schema_numeric_eval_frame_stack frames = {0};
    struct information_schema_numeric_eval_value_stack values = {0};
    int rc = MYLITE_OK;

    *out_value = 0.0;
    *out_is_null = false;
    rc = information_schema_numeric_eval_frame_stack_push(
        database,
        &frames,
        (struct information_schema_numeric_eval_frame){
            .action = INFORMATION_SCHEMA_NUMERIC_EVAL_ENTER,
            .expression = expression,
        }
    );
    while (rc == MYLITE_OK && frames.count != 0U) {
        struct information_schema_numeric_eval_frame frame = frames.items[--frames.count];

        if (frame.action == INFORMATION_SCHEMA_NUMERIC_EVAL_APPLY_DIVIDE) {
            rc = information_schema_numeric_projection_apply_divide_frame(database, &values);
        } else {
            rc = information_schema_numeric_projection_enter_frame(
                database,
                query,
                row,
                frame.expression,
                &frames,
                &values
            );
        }
    }
    if (rc == MYLITE_OK && values.count != 1U) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA numeric projection expression");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        *out_value = values.items[0].value;
        *out_is_null = values.items[0].is_null;
    }
    information_schema_numeric_eval_value_stack_deinit(&values);
    information_schema_numeric_eval_frame_stack_deinit(&frames);
    return rc;
}

static int information_schema_numeric_projection_enter_frame(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    const struct mylite_sql_ast_node *expression,
    struct information_schema_numeric_eval_frame_stack *frames,
    struct information_schema_numeric_eval_value_stack *values
) {
    const struct mylite_sql_ast_node *unwrapped = unwrap_parenthesized_expression(expression);

    if (unwrapped == NULL) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA numeric projection expression");
        return MYLITE_ERROR;
    }
    if (unwrapped->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(unwrapped) == MYLITE_SQL_AST_LITERAL_INTEGER) {
        double value = 0.0;
        int rc = information_schema_numeric_literal_value(database, unwrapped, &value);

        if (rc != MYLITE_OK) {
            return rc;
        }
        return information_schema_numeric_eval_value_stack_push(
            database,
            values,
            (struct information_schema_numeric_eval_value){.value = value, .is_null = false}
        );
    }
    if (unwrapped->kind == MYLITE_SQL_AST_IDENTIFIER ||
        unwrapped->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        size_t column_index = 0U;
        double value = 0.0;
        bool is_null = false;
        int rc = information_schema_resolve_column_reference(
            database,
            query,
            unwrapped,
            COLUMN_REFERENCE_FIELD,
            &column_index
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = information_schema_numeric_text_value(database, row[column_index], &value, &is_null);
        if (rc != MYLITE_OK) {
            return rc;
        }
        return information_schema_numeric_eval_value_stack_push(
            database,
            values,
            (struct information_schema_numeric_eval_value){.value = value, .is_null = is_null}
        );
    }
    if (unwrapped->kind == MYLITE_SQL_AST_BINARY_EXPRESSION &&
        mylite_sql_ast_node_operator(unwrapped) == MYLITE_SQL_AST_OPERATOR_DIVIDE) {
        int rc = information_schema_numeric_eval_frame_stack_push(
            database,
            frames,
            (struct information_schema_numeric_eval_frame){
                .action = INFORMATION_SCHEMA_NUMERIC_EVAL_APPLY_DIVIDE,
                .expression = NULL,
            }
        );

        if (rc == MYLITE_OK) {
            rc = information_schema_numeric_eval_frame_stack_push(
                database,
                frames,
                (struct information_schema_numeric_eval_frame){
                    .action = INFORMATION_SCHEMA_NUMERIC_EVAL_ENTER,
                    .expression = child_at(unwrapped, 1U),
                }
            );
        }
        if (rc == MYLITE_OK) {
            rc = information_schema_numeric_eval_frame_stack_push(
                database,
                frames,
                (struct information_schema_numeric_eval_frame){
                    .action = INFORMATION_SCHEMA_NUMERIC_EVAL_ENTER,
                    .expression = child_at(unwrapped, 0U),
                }
            );
        }
        return rc;
    }

    set_runtime_error(database, "invalid INFORMATION_SCHEMA numeric projection expression");
    return MYLITE_ERROR;
}

static int information_schema_numeric_projection_apply_divide_frame(
    struct mylite_db *database,
    struct information_schema_numeric_eval_value_stack *values
) {
    struct information_schema_numeric_eval_value left = {0};
    struct information_schema_numeric_eval_value right = {0};

    if (values->count < 2U) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA numeric projection expression");
        return MYLITE_ERROR;
    }
    right = values->items[--values->count];
    left = values->items[--values->count];
    if (left.is_null || right.is_null) {
        return information_schema_numeric_eval_value_stack_push(
            database,
            values,
            (struct information_schema_numeric_eval_value){.value = 0.0, .is_null = true}
        );
    }
    if (right.value == 0.0) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT numeric projection does not support division by zero"
        );
        return MYLITE_ERROR;
    }
    return information_schema_numeric_eval_value_stack_push(
        database,
        values,
        (struct information_schema_numeric_eval_value){
            .value = left.value / right.value,
            .is_null = false,
        }
    );
}

static int information_schema_numeric_eval_frame_stack_push(
    struct mylite_db *database,
    struct information_schema_numeric_eval_frame_stack *stack,
    struct information_schema_numeric_eval_frame frame
) {
    struct information_schema_numeric_eval_frame *items = NULL;
    size_t capacity = 0U;

    if (stack->count == stack->capacity) {
        capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;
        if (capacity < stack->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        items = realloc(stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        stack->items = items;
        stack->capacity = capacity;
    }
    stack->items[stack->count] = frame;
    ++stack->count;
    return MYLITE_OK;
}

static void information_schema_numeric_eval_frame_stack_deinit(
    struct information_schema_numeric_eval_frame_stack *stack
) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    *stack = (struct information_schema_numeric_eval_frame_stack){0};
}

static int information_schema_numeric_eval_value_stack_push(
    struct mylite_db *database,
    struct information_schema_numeric_eval_value_stack *stack,
    struct information_schema_numeric_eval_value value
) {
    struct information_schema_numeric_eval_value *items = NULL;
    size_t capacity = 0U;

    if (stack->count == stack->capacity) {
        capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;
        if (capacity < stack->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        items = realloc(stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        stack->items = items;
        stack->capacity = capacity;
    }
    stack->items[stack->count] = value;
    ++stack->count;
    return MYLITE_OK;
}

static void information_schema_numeric_eval_value_stack_deinit(
    struct information_schema_numeric_eval_value_stack *stack
) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    *stack = (struct information_schema_numeric_eval_value_stack){0};
}

static int information_schema_numeric_text_value(
    struct mylite_db *database,
    const char *text,
    double *out_value,
    bool *out_is_null
) {
    char *end = NULL;

    *out_value = 0.0;
    *out_is_null = text == NULL;
    if (text == NULL) {
        return MYLITE_OK;
    }

    errno = 0;
    *out_value = strtod(text, &end);
    if (errno == ERANGE || end == text || (end != NULL && *end != '\0')) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT numeric projection supports only numeric metadata values"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int information_schema_numeric_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    double *out_value
) {
    char *text = NULL;
    bool is_null = false;
    int rc = copy_source_span_text(database, &literal->span, &text);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = information_schema_numeric_text_value(database, text, out_value, &is_null);
    free(text);
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
    bool includes_connection_control_failed_login_attempts = false;
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

    rc = information_schema_includes_connection_control_failed_login_attempts(
        database,
        statement,
        query,
        &includes_connection_control_failed_login_attempts
    );
    if (rc != MYLITE_OK) {
        return rc;
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

        if (query->definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_TABLES &&
            information_schema_row_is_connection_control_failed_login_attempts(
                rows->rows[row_index]
            ) &&
            !includes_connection_control_failed_login_attempts) {
            continue;
        }
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

static int information_schema_includes_connection_control_failed_login_attempts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    bool *out_includes_table
) {
    const struct mylite_sql_ast_node *optional_clause = NULL;

    if (out_includes_table == NULL) {
        return MYLITE_MISUSE;
    }
    *out_includes_table = false;
    if (statement == NULL || query == NULL ||
        query->definition->kind != MYLITE_EXECUTION_CATALOG_TABLE_TABLES) {
        return MYLITE_OK;
    }

    optional_clause = child_at(statement, 2U);
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            return information_schema_predicate_includes_connection_control_failed_login_attempts(
                database,
                query,
                child_at(optional_clause, 0U),
                out_includes_table
            );
        }
        optional_clause = optional_clause->next_sibling;
    }
    return MYLITE_OK;
}

static int information_schema_predicate_includes_connection_control_failed_login_attempts(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    bool *out_includes_table
) {
    struct scalar_arithmetic_node_stack stack = {0};
    int rc = MYLITE_OK;

    if (out_includes_table == NULL) {
        return MYLITE_MISUSE;
    }
    *out_includes_table = false;
    if (!scalar_arithmetic_node_stack_push(&stack, predicate_node)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    while (rc == MYLITE_OK && stack.count != 0U && !*out_includes_table) {
        const struct mylite_sql_ast_node *predicate =
            unwrap_parenthesized_predicate(stack.items[--stack.count]);

        if (predicate == NULL) {
            continue;
        }
        if (predicate->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
            rc = information_schema_predicate_marks_connection_control_failed_login_attempts(
                database,
                query,
                predicate,
                out_includes_table
            );
        } else if (
            predicate->kind == MYLITE_SQL_AST_AND_PREDICATE ||
            predicate->kind == MYLITE_SQL_AST_OR_PREDICATE ||
            predicate->kind == MYLITE_SQL_AST_XOR_PREDICATE
        ) {
            rc = information_schema_predicate_stack_push_binary_children(
                database,
                &stack,
                predicate
            );
        }
    }
    scalar_arithmetic_node_stack_deinit(&stack);
    return rc;
}

static int information_schema_predicate_marks_connection_control_failed_login_attempts(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate,
    bool *out_includes_table
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate);
    size_t column_index = 0U;
    struct information_schema_predicate_value right = {0};
    int rc = MYLITE_OK;

    if (operator_kind != MYLITE_SQL_AST_OPERATOR_EQUAL &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        return MYLITE_OK;
    }
    rc = information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );
    if (rc == MYLITE_OK && column_index == information_schema_tables_table_name_column) {
        rc = information_schema_predicate_value_text(database, child_at(predicate, 1U), &right);
        if (rc == MYLITE_OK && !right.is_null && right.text != NULL &&
            strcmp(right.text, "CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS") == 0) {
            *out_includes_table = true;
        }
    }
    free(right.text);
    return rc;
}

static int information_schema_predicate_stack_push_binary_children(
    struct mylite_db *database,
    struct scalar_arithmetic_node_stack *stack,
    const struct mylite_sql_ast_node *predicate
) {
    if (!scalar_arithmetic_node_stack_push(stack, child_at(predicate, 1U)) ||
        !scalar_arithmetic_node_stack_push(stack, child_at(predicate, 0U))) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static bool information_schema_row_is_connection_control_failed_login_attempts(char **row) {
    return row != NULL &&
           strcmp(row[information_schema_tables_table_schema_column], "information_schema") == 0 &&
           strcmp(
               row[information_schema_tables_table_name_column],
               "CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS"
           ) == 0;
}

static int information_schema_sort_row_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t *indexes,
    size_t index_count
) {
    int rc = MYLITE_OK;

    if (!query->has_order) {
        (void)database;
        (void)statement;
        if (query->definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_TABLES) {
            information_schema_sort_tables_default_row_indexes(rows, indexes, index_count);
        }
        return rc;
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

static void information_schema_sort_tables_default_row_indexes(
    const struct information_schema_row_set *rows,
    size_t *indexes,
    size_t index_count
) {
    for (size_t index = 1U; index < index_count; ++index) {
        size_t row_index = indexes[index];
        size_t scan = index;

        while (scan > 0U &&
               information_schema_compare_tables_default_rows(
                   rows,
                   row_index,
                   indexes[scan - 1U]
               ) < 0) {
            indexes[scan] = indexes[scan - 1U];
            --scan;
        }
        indexes[scan] = row_index;
    }
}

static int information_schema_compare_tables_default_rows(
    const struct information_schema_row_set *rows,
    size_t left_row,
    size_t right_row
) {
    const char *left_schema = rows->rows[left_row][information_schema_tables_table_schema_column];
    const char *right_schema = rows->rows[right_row][information_schema_tables_table_schema_column];
    const int left_priority = information_schema_tables_default_schema_priority(left_schema);
    const int right_priority =
        information_schema_tables_default_schema_priority(right_schema);

    if (left_priority != right_priority) {
        return left_priority - right_priority;
    }
    const int left_table_priority = information_schema_tables_default_table_priority(
        left_schema,
        rows->rows[left_row][information_schema_tables_table_name_column]
    );
    const int right_table_priority = information_schema_tables_default_table_priority(
        right_schema,
        rows->rows[right_row][information_schema_tables_table_name_column]
    );

    if (left_table_priority != right_table_priority) {
        return left_table_priority - right_table_priority;
    }
    return information_schema_compare_text(
        rows->definition,
        information_schema_tables_table_name_column,
        rows->rows[left_row][information_schema_tables_table_name_column],
        rows->rows[right_row][information_schema_tables_table_name_column]
    );
}

static int information_schema_tables_default_schema_priority(const char *schema_name) {
    enum { information_schema_unknown_schema_priority = 5 };

    if (schema_name == NULL) {
        return information_schema_unknown_schema_priority;
    }
    if (strcmp(schema_name, "mysql") == 0) {
        return 0;
    }
    if (strcmp(schema_name, "information_schema") == 0) {
        return 1;
    }
    if (strcmp(schema_name, "performance_schema") == 0) {
        return 2;
    }
    if (strcmp(schema_name, "sys") == 0) {
        return 3;
    }
    return information_schema_unknown_schema_priority - 1;
}

static int information_schema_tables_default_table_priority(
    const char *schema_name,
    const char *table_name
) {
    if (schema_name != NULL && table_name != NULL && strcmp(schema_name, "mysql") == 0 &&
        strcmp(table_name, "innodb_table_stats") == 0) {
        return 0;
    }
    return 1;
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
    } else if (query->definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_COLLATIONS &&
               query->order_index == information_schema_collations_name_column) {
        size_t left_index = 0U;
        size_t right_index = 0U;
        bool left_known =
            mylite_execution_catalog_mysql_collation_index_by_name(left, &left_index);
        bool right_known =
            mylite_execution_catalog_mysql_collation_index_by_name(right, &right_index);

        if (left_known && right_known) {
            comparison = (left_index > right_index) - (left_index < right_index);
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
