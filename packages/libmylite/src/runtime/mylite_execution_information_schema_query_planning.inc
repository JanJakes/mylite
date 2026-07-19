
static int information_schema_plan_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    struct information_schema_query *out_query
) {
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (select_list_is_wildcard(select_list)) {
        return information_schema_plan_wildcard_projection(database, out_query);
    }

    const struct mylite_sql_ast_node *item = child_at(select_list, 0U);
    bool select_list_has_one_item = mylite_sql_ast_node_child_count(select_list) == 1U;

    while (item != NULL) {
        int rc = information_schema_plan_select_item_projection(
            database,
            select_list_has_one_item,
            item,
            out_query
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        item = item->next_sibling;
    }

    return MYLITE_OK;
}

static int information_schema_plan_wildcard_projection(
    struct mylite_db *database,
    struct information_schema_query *out_query
) {
    for (size_t column_index = 0U; column_index < out_query->definition->column_count;
         ++column_index) {
        int rc = information_schema_append_projection(database, out_query, column_index, NULL);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int information_schema_plan_select_item_projection(
    struct mylite_db *database,
    bool select_list_has_one_item,
    const struct mylite_sql_ast_node *item,
    struct information_schema_query *out_query
) {
    const struct mylite_sql_ast_node *expression =
        mylite_execution_unwrap_parenthesized_expression(child_at(item, 0U));
    enum planned_count_function count_function = count_function_from_expression(expression);
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    if (count_function == PLANNED_COUNT_STAR) {
        const struct mylite_sql_ast_node *alias = child_at(item, 1U);

        if (!select_list_has_one_item) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA SELECT supports either COUNT(*) or metadata columns"
            );
            return MYLITE_ERROR;
        }
        if (alias != NULL) {
            rc = mylite_execution_information_schema_copy_select_item_alias(
                database,
                alias,
                &out_query->count_name
            );
        } else {
            rc = mylite_execution_information_schema_copy_aggregate_label(
                database,
                &expression->span,
                &out_query->count_name
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_query->is_count_star = true;
        return MYLITE_OK;
    }
    if (count_function != PLANNED_COUNT_NONE || expression == NULL) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT supports only metadata columns and COUNT(*)"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
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
        return rc;
    }
    if (expression->kind == MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION) {
        return information_schema_append_expression_projection(
            database,
            out_query,
            expression,
            child_at(item, 1U)
        );
    }
    if (out_query->has_group &&
        expression->kind == MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION) {
        return information_schema_append_group_concat_ordered_column_projection(
            database,
            out_query,
            expression,
            child_at(item, 1U)
        );
    }
    if (out_query->has_group && expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        mylite_sql_ast_node_operator(expression) == MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT) {
        return information_schema_append_logical_not_column_projection(
            database,
            out_query,
            expression,
            child_at(item, 1U)
        );
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_INTEGER) {
        return information_schema_append_integer_literal_projection(
            database,
            out_query,
            expression,
            child_at(item, 1U)
        );
    }

    set_unsupported_error(
        database,
        "INFORMATION_SCHEMA SELECT supports only metadata columns, COUNT(*), integer literals, "
        "CAST(numeric metadata expression AS UNSIGNED), and limited grouped STATISTICS "
        "expressions"
    );
    return MYLITE_ERROR;
}

static int information_schema_append_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    size_t column_index,
    const struct mylite_sql_ast_node *alias
) {
    return information_schema_append_projection_slot(
        database,
        query,
        &(const struct information_schema_projection_slot_request){
            .column_index = column_index,
            .kind = INFORMATION_SCHEMA_PROJECTION_COLUMN,
            .expression = NULL,
            .alias = alias,
        }
    );
}

static int information_schema_append_expression_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
) {
    return information_schema_append_projection_slot(
        database,
        query,
        &(const struct information_schema_projection_slot_request){
            .column_index = 0U,
            .kind = INFORMATION_SCHEMA_PROJECTION_UNSIGNED_INTEGER_EXPRESSION,
            .expression = expression,
            .alias = alias,
        }
    );
}

static int information_schema_append_integer_literal_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
) {
    return information_schema_append_projection_slot(
        database,
        query,
        &(const struct information_schema_projection_slot_request){
            .column_index = 0U,
            .kind = INFORMATION_SCHEMA_PROJECTION_INTEGER_LITERAL,
            .expression = expression,
            .alias = alias,
        }
    );
}

static int information_schema_append_group_concat_ordered_column_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
) {
    size_t column_index = 0U;
    int rc = information_schema_validate_group_concat_ordered_column_expression(
        database,
        query,
        expression,
        &column_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    return information_schema_append_projection_slot(
        database,
        query,
        &(const struct information_schema_projection_slot_request){
            .column_index = column_index,
            .kind = INFORMATION_SCHEMA_PROJECTION_GROUP_CONCAT_ORDERED_COLUMN,
            .expression = expression,
            .alias = alias,
        }
    );
}

static int information_schema_append_logical_not_column_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
) {
    size_t column_index = 0U;
    int rc = information_schema_validate_logical_not_column_expression(
        database,
        query,
        expression,
        &column_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    return information_schema_append_projection_slot(
        database,
        query,
        &(const struct information_schema_projection_slot_request){
            .column_index = column_index,
            .kind = INFORMATION_SCHEMA_PROJECTION_LOGICAL_NOT_COLUMN,
            .expression = expression,
            .alias = alias,
        }
    );
}

static int information_schema_append_projection_slot(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct information_schema_projection_slot_request *request
) {
    struct information_schema_projection_expression planned_expression = {0};
    size_t required_count = 0U;
    char *column_name = NULL;
    int rc = MYLITE_OK;

    if (query->projection_count == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    required_count = query->projection_count + 1U;

    if (request->alias != NULL) {
        rc = mylite_execution_information_schema_copy_select_item_alias(
            database,
            request->alias,
            &column_name
        );
    } else if (request->expression != NULL) {
        rc = copy_source_span_text(database, &request->expression->span, &column_name);
    } else {
        rc = duplicate_text(
            database,
            query->definition->columns[request->column_index].name,
            &column_name
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = information_schema_plan_projection_expression(
        database,
        query,
        request,
        column_name,
        &planned_expression
    );
    if (rc == MYLITE_OK) {
        rc = information_schema_reserve_projections(database, query, required_count);
    }
    if (rc != MYLITE_OK) {
        mylite_execution_information_schema_projection_expression_deinit(&planned_expression);
        free(column_name);
        return rc;
    }

    query->projection_indexes[query->projection_count] = request->column_index;
    query->projection_kinds[query->projection_count] = request->kind;
    query->projection_expressions[query->projection_count] = planned_expression;
    query->projection_names[query->projection_count] = column_name;
    query->projection_count = required_count;
    return MYLITE_OK;
}

static int information_schema_reserve_projections(
    struct mylite_db *database,
    struct information_schema_query *query,
    size_t required_count
) {
    size_t *indexes = NULL;
    enum information_schema_projection_kind *kinds = NULL;
    struct information_schema_projection_expression *expressions = NULL;
    char **names = NULL;
    size_t capacity = query->projection_capacity;

    if (required_count <= capacity) {
        return MYLITE_OK;
    }
    capacity = capacity == 0U ? information_schema_projection_stack_capacity : capacity;
    while (capacity < required_count) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*indexes) || capacity > SIZE_MAX / sizeof(*kinds) ||
        capacity > SIZE_MAX / sizeof(*expressions) || capacity > SIZE_MAX / sizeof(*names)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    indexes = realloc(query->projection_indexes, capacity * sizeof(*indexes));
    if (indexes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    query->projection_indexes = indexes;
    kinds = realloc(query->projection_kinds, capacity * sizeof(*kinds));
    if (kinds == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    query->projection_kinds = kinds;
    expressions = realloc(query->projection_expressions, capacity * sizeof(*expressions));
    if (expressions == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    query->projection_expressions = expressions;
    names = (char **)realloc((void *)query->projection_names, capacity * sizeof(*names));
    if (names == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    query->projection_names = names;
    memset(
        &query->projection_expressions[query->projection_capacity],
        0,
        (capacity - query->projection_capacity) * sizeof(*query->projection_expressions)
    );
    query->projection_capacity = capacity;
    return MYLITE_OK;
}

static int information_schema_plan_projection_expression(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_projection_slot_request *request,
    const char *column_name,
    struct information_schema_projection_expression *out_expression
) {
    int rc = MYLITE_OK;

    *out_expression = (struct information_schema_projection_expression){0};
    if (request->kind == INFORMATION_SCHEMA_PROJECTION_UNSIGNED_INTEGER_EXPRESSION) {
        return information_schema_compile_unsigned_projection_expression(
            database,
            query,
            request->expression,
            out_expression
        );
    }
    if (request->kind != INFORMATION_SCHEMA_PROJECTION_INTEGER_LITERAL) {
        return MYLITE_OK;
    }
    if (request->expression == NULL || request->expression->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(request->expression) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA integer literal projection");
        return MYLITE_ERROR;
    }
    rc = copy_source_span_text(
        database,
        &request->expression->span,
        &out_expression->integer_literal_text
    );
    if (rc == MYLITE_OK) {
        rc = mylite_execution_information_schema_make_scalar_result_descriptor(
            database,
            request->expression,
            column_name,
            &out_expression->integer_literal_descriptor
        );
    }
    if (rc != MYLITE_OK) {
        mylite_execution_information_schema_projection_expression_deinit(out_expression);
    }
    return rc;
}

static int information_schema_plan_group(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *group_clause,
    struct information_schema_query *out_query
) {
    static const size_t expected_group_columns[] = {
        information_schema_statistics_index_name_column,
        information_schema_statistics_index_type_column,
        information_schema_statistics_non_unique_column,
    };
    const struct mylite_sql_ast_node *group_key = NULL;
    size_t group_key_count = 0U;
    int rc = MYLITE_OK;

    if (group_clause == NULL) {
        return MYLITE_OK;
    }
    if (out_query->definition->kind != MYLITE_EXECUTION_CATALOG_TABLE_STATISTICS) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA GROUP BY is supported only for STATISTICS index metadata"
        );
        return MYLITE_ERROR;
    }
    if (out_query->is_count_star) {
        set_unsupported_error(database, "INFORMATION_SCHEMA GROUP BY does not support COUNT(*)");
        return MYLITE_ERROR;
    }
    group_key_count = mylite_sql_ast_node_child_count(group_clause);
    if (group_key_count != sizeof(expected_group_columns) / sizeof(expected_group_columns[0])) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA STATISTICS GROUP BY supports index_name, index_type, non_unique"
        );
        return MYLITE_ERROR;
    }

    group_key = child_at(group_clause, 0U);
    for (size_t index = 0U; rc == MYLITE_OK && index < group_key_count; ++index) {
        size_t column_index = 0U;

        if (group_key == NULL || group_key->kind == MYLITE_SQL_AST_GROUP_BY_ROLLUP_MODIFIER) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA GROUP BY WITH ROLLUP is unsupported"
            );
            return MYLITE_ERROR;
        }
        rc = information_schema_resolve_column_reference(
            database,
            out_query,
            group_key,
            COLUMN_REFERENCE_FIELD,
            &column_index
        );
        if (rc == MYLITE_OK && column_index != expected_group_columns[index]) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA STATISTICS GROUP BY supports index_name, index_type, "
                "non_unique"
            );
            return MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            out_query->group_indexes[index] = column_index;
        }
        group_key = group_key->next_sibling;
    }
    if (rc == MYLITE_OK) {
        out_query->group_index_count = group_key_count;
    }
    for (size_t projection_index = 0U;
         rc == MYLITE_OK && projection_index < out_query->projection_count;
         ++projection_index) {
        if (!information_schema_projection_is_grouped_statistics_compatible(
                out_query,
                projection_index
            )) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA STATISTICS GROUP BY supports grouped columns, "
                "GROUP_CONCAT(column_name ORDER BY seq_in_index), and NOT non_unique"
            );
            rc = MYLITE_ERROR;
        }
    }
    return rc;
}

static bool information_schema_projection_is_grouped_statistics_compatible(
    const struct information_schema_query *query,
    size_t projection_index
) {
    size_t column_index = query->projection_indexes[projection_index];

    switch (query->projection_kinds[projection_index]) {
    case INFORMATION_SCHEMA_PROJECTION_COLUMN:
        return information_schema_group_contains_column(query, column_index);
    case INFORMATION_SCHEMA_PROJECTION_GROUP_CONCAT_ORDERED_COLUMN:
        return column_index == information_schema_statistics_column_name_column;
    case INFORMATION_SCHEMA_PROJECTION_LOGICAL_NOT_COLUMN:
        return information_schema_group_contains_column(query, column_index);
    case INFORMATION_SCHEMA_PROJECTION_UNSIGNED_INTEGER_EXPRESSION:
    case INFORMATION_SCHEMA_PROJECTION_INTEGER_LITERAL:
        return false;
    }
    return false;
}

static bool information_schema_group_contains_column(
    const struct information_schema_query *query,
    size_t column_index
) {
    for (size_t index = 0U; index < query->group_index_count; ++index) {
        if (query->group_indexes[index] == column_index) {
            return true;
        }
    }
    return false;
}

static int information_schema_validate_group_concat_ordered_column_expression(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    size_t *out_column_index
) {
    const struct mylite_sql_ast_node *value = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *separator = NULL;
    size_t order_index = 0U;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA grouped STATISTICS supports GROUP_CONCAT metadata expressions"
        );
        return MYLITE_ERROR;
    }
    value = mylite_execution_unwrap_parenthesized_expression(child_at(expression, 0U));
    order_clause = child_at(expression, 1U);
    separator = child_at(expression, 2U);
    if (separator != NULL) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA grouped STATISTICS GROUP_CONCAT does not support SEPARATOR"
        );
        return MYLITE_ERROR;
    }
    if (order_clause == NULL || order_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE ||
        mylite_sql_ast_node_order_direction(child_at(order_clause, 1U)) ==
            MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA grouped STATISTICS GROUP_CONCAT supports ORDER BY seq_in_index"
        );
        return MYLITE_ERROR;
    }
    rc = information_schema_resolve_column_reference(
        database,
        query,
        value,
        COLUMN_REFERENCE_FIELD,
        out_column_index
    );
    if (rc == MYLITE_OK && *out_column_index != information_schema_statistics_column_name_column) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA grouped STATISTICS GROUP_CONCAT supports column_name"
        );
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_resolve_column_reference(
            database,
            query,
            child_at(order_clause, 0U),
            COLUMN_REFERENCE_ORDER,
            &order_index
        );
    }
    if (rc == MYLITE_OK && order_index != information_schema_statistics_seq_in_index_column) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA grouped STATISTICS GROUP_CONCAT supports ORDER BY seq_in_index"
        );
        return MYLITE_ERROR;
    }
    return rc;
}

static int information_schema_validate_logical_not_column_expression(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    size_t *out_column_index
) {
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_UNARY_EXPRESSION ||
        mylite_sql_ast_node_operator(expression) != MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA grouped STATISTICS supports NOT metadata expressions"
        );
        return MYLITE_ERROR;
    }

    int rc = information_schema_resolve_column_reference(
        database,
        query,
        mylite_execution_unwrap_parenthesized_expression(child_at(expression, 0U)),
        COLUMN_REFERENCE_FIELD,
        out_column_index
    );

    if (rc == MYLITE_OK && *out_column_index != information_schema_statistics_non_unique_column) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA grouped STATISTICS NOT expression supports non_unique"
        );
        return MYLITE_ERROR;
    }
    return rc;
}

static int information_schema_compile_unsigned_projection_expression(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    struct information_schema_projection_expression *out_expression
) {
    struct information_schema_numeric_plan_stack stack = {0};
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT supports only CAST(... AS UNSIGNED) metadata expressions"
        );
        return MYLITE_ERROR;
    }
    rc = information_schema_numeric_plan_stack_push(
        database,
        &stack,
        (struct information_schema_numeric_plan_frame){
            .action = INFORMATION_SCHEMA_NUMERIC_PLAN_ENTER,
            .expression = child_at(expression, 0U),
        }
    );
    while (rc == MYLITE_OK && stack.count != 0U) {
        struct information_schema_numeric_plan_frame frame = stack.items[--stack.count];

        rc = information_schema_compile_numeric_projection_node(
            database,
            query,
            &frame,
            &stack,
            out_expression
        );
    }
    information_schema_numeric_plan_stack_deinit(&stack);
    if (rc != MYLITE_OK) {
        mylite_execution_information_schema_projection_expression_deinit(out_expression);
    }
    return rc;
}

static int information_schema_compile_numeric_projection_node(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_numeric_plan_frame *frame,
    struct information_schema_numeric_plan_stack *stack,
    struct information_schema_projection_expression *out_expression
) {
    const struct mylite_sql_ast_node *unwrapped = NULL;

    if (frame->action == INFORMATION_SCHEMA_NUMERIC_PLAN_APPEND_DIVIDE) {
        return information_schema_append_numeric_projection_instruction(
            database,
            out_expression,
            (struct information_schema_numeric_projection_instruction){
                .kind = INFORMATION_SCHEMA_NUMERIC_PROJECTION_DIVIDE,
            }
        );
    }
    unwrapped = mylite_execution_unwrap_parenthesized_expression(frame->expression);
    if (unwrapped == NULL) {
        set_unsupported_error(database, "INFORMATION_SCHEMA SELECT numeric expression is invalid");
        return MYLITE_ERROR;
    }
    if (unwrapped->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(unwrapped) == MYLITE_SQL_AST_LITERAL_INTEGER) {
        double literal = 0.0;
        int rc = information_schema_parse_numeric_projection_literal(database, unwrapped, &literal);

        if (rc != MYLITE_OK) {
            return rc;
        }
        return information_schema_append_numeric_projection_instruction(
            database,
            out_expression,
            (struct information_schema_numeric_projection_instruction){
                .kind = INFORMATION_SCHEMA_NUMERIC_PROJECTION_LITERAL,
                .literal = literal,
            }
        );
    }
    if (unwrapped->kind == MYLITE_SQL_AST_IDENTIFIER ||
        unwrapped->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        size_t column_index = 0U;
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
        return information_schema_append_numeric_projection_instruction(
            database,
            out_expression,
            (struct information_schema_numeric_projection_instruction){
                .kind = INFORMATION_SCHEMA_NUMERIC_PROJECTION_COLUMN,
                .column_index = column_index,
            }
        );
    }
    if (unwrapped->kind == MYLITE_SQL_AST_BINARY_EXPRESSION &&
        mylite_sql_ast_node_operator(unwrapped) == MYLITE_SQL_AST_OPERATOR_DIVIDE) {
        int rc = information_schema_numeric_plan_stack_push(
            database,
            stack,
            (struct information_schema_numeric_plan_frame){
                .action = INFORMATION_SCHEMA_NUMERIC_PLAN_APPEND_DIVIDE,
            }
        );

        if (rc == MYLITE_OK) {
            rc = information_schema_numeric_plan_stack_push(
                database,
                stack,
                (struct information_schema_numeric_plan_frame){
                    .action = INFORMATION_SCHEMA_NUMERIC_PLAN_ENTER,
                    .expression = child_at(unwrapped, 1U),
                }
            );
        }
        if (rc == MYLITE_OK) {
            rc = information_schema_numeric_plan_stack_push(
                database,
                stack,
                (struct information_schema_numeric_plan_frame){
                    .action = INFORMATION_SCHEMA_NUMERIC_PLAN_ENTER,
                    .expression = child_at(unwrapped, 0U),
                }
            );
        }
        return rc;
    }

    set_unsupported_error(
        database,
        "INFORMATION_SCHEMA SELECT supports numeric metadata expressions with columns, integer "
        "literals, and /"
    );
    return MYLITE_ERROR;
}

static int information_schema_append_numeric_projection_instruction(
    struct mylite_db *database,
    struct information_schema_projection_expression *expression,
    struct information_schema_numeric_projection_instruction instruction
) {
    struct information_schema_numeric_projection_instruction *instructions = NULL;
    size_t capacity = expression->instruction_capacity;

    if (expression->instruction_count == capacity) {
        capacity = capacity == 0U ? if_stack_initial_capacity : capacity * 2U;
        if (capacity < expression->instruction_capacity ||
            capacity > SIZE_MAX / sizeof(*instructions)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        instructions = realloc(expression->instructions, capacity * sizeof(*instructions));
        if (instructions == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        expression->instructions = instructions;
        expression->instruction_capacity = capacity;
    }
    expression->instructions[expression->instruction_count++] = instruction;
    return MYLITE_OK;
}

static int information_schema_parse_numeric_projection_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    double *out_value
) {
    char *text = NULL;
    char *end = NULL;
    int rc = copy_source_span_text(database, &literal->span, &text);

    if (rc != MYLITE_OK) {
        return rc;
    }
    errno = 0;
    *out_value = mylite_numeric_parse_double(text, &end);
    if (errno == ERANGE || end == text || (end != NULL && *end != '\0')) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT numeric projection supports only numeric metadata values"
        );
        rc = MYLITE_ERROR;
    }
    free(text);
    return rc;
}

static int information_schema_numeric_plan_stack_push(
    struct mylite_db *database,
    struct information_schema_numeric_plan_stack *stack,
    struct information_schema_numeric_plan_frame frame
) {
    struct information_schema_numeric_plan_frame *items = NULL;
    size_t capacity = stack->capacity;

    if (stack->count == capacity) {
        capacity = capacity == 0U ? if_stack_initial_capacity : capacity * 2U;
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
    stack->items[stack->count++] = frame;
    return MYLITE_OK;
}

static void information_schema_numeric_plan_stack_deinit(
    struct information_schema_numeric_plan_stack *stack
) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    *stack = (struct information_schema_numeric_plan_stack){0};
}

static int information_schema_plan_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    struct information_schema_query *out_query
) {
    const struct mylite_sql_ast_node *order_items = NULL;
    int rc = MYLITE_OK;

    out_query->has_order = false;
    out_query->order_direction = PLANNED_SELECT_ORDER_DEFAULT;
    out_query->order_count = 0U;
    if (order_clause == NULL) {
        return MYLITE_OK;
    }
    if (out_query->has_group) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA grouped SELECT does not support ORDER BY"
        );
        return MYLITE_ERROR;
    }
    if (order_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE ||
        child_at(order_clause, 0U) == NULL) {
        set_unsupported_error(database, "INFORMATION_SCHEMA SELECT has an invalid ORDER BY");
        return MYLITE_ERROR;
    }
    order_items = child_at(order_clause, 0U);
    if (order_items->kind == MYLITE_SQL_AST_ORDER_BY_ITEM_LIST) {
        const struct mylite_sql_ast_node *item = child_at(order_items, 0U);

        while (rc == MYLITE_OK && item != NULL) {
            const struct mylite_sql_ast_node *direction = NULL;

            if (out_query->order_count == information_schema_order_key_capacity) {
                set_unsupported_error(
                    database,
                    "INFORMATION_SCHEMA SELECT ORDER BY has too many columns"
                );
                return MYLITE_ERROR;
            }
            if (item->kind != MYLITE_SQL_AST_ORDER_BY_ITEM || child_at(item, 0U) == NULL) {
                set_unsupported_error(
                    database,
                    "INFORMATION_SCHEMA SELECT has an invalid ORDER BY"
                );
                return MYLITE_ERROR;
            }
            rc = information_schema_resolve_order_reference(
                database,
                out_query,
                child_at(item, 0U),
                &out_query->order_indexes[out_query->order_count]
            );
            direction = child_at(item, 1U);
            out_query->order_directions[out_query->order_count] = PLANNED_SELECT_ORDER_ASC;
            if (mylite_sql_ast_node_order_direction(direction) ==
                MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
                out_query->order_directions[out_query->order_count] = PLANNED_SELECT_ORDER_DESC;
            }
            if (rc == MYLITE_OK) {
                ++out_query->order_count;
            }
            item = item->next_sibling;
        }
    } else {
        const struct mylite_sql_ast_node *direction = child_at(order_clause, 1U);

        rc = information_schema_resolve_order_reference(
            database,
            out_query,
            child_at(order_clause, 0U),
            &out_query->order_indexes[0]
        );
        out_query->order_directions[0] = PLANNED_SELECT_ORDER_ASC;
        if (mylite_sql_ast_node_order_direction(direction) == MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
            out_query->order_directions[0] = PLANNED_SELECT_ORDER_DESC;
        }
        if (rc == MYLITE_OK) {
            out_query->order_count = 1U;
        }
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_query->has_order = out_query->order_count > 0U;
    out_query->order_index = out_query->order_indexes[0];
    out_query->order_direction = out_query->order_directions[0];
    return MYLITE_OK;
}

static int information_schema_resolve_order_reference(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *order_reference,
    size_t *out_order_index
) {
    char reference_text[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (order_reference != NULL && order_reference->kind == MYLITE_SQL_AST_IDENTIFIER) {
        rc =
            copy_identifier_text(order_reference, reference_text, sizeof(reference_text), database);
        if (rc != MYLITE_OK) {
            return rc;
        }
        for (size_t projection_index = 0U; projection_index < query->projection_count;
             ++projection_index) {
            if (!mylite_execution_text_equals_ascii_case_insensitive(
                    query->projection_names[projection_index],
                    reference_text
                )) {
                continue;
            }
            if (query->projection_kinds[projection_index] != INFORMATION_SCHEMA_PROJECTION_COLUMN) {
                set_unsupported_error(
                    database,
                    "INFORMATION_SCHEMA SELECT ORDER BY expression aliases are not supported"
                );
                return MYLITE_ERROR;
            }
            *out_order_index = query->projection_indexes[projection_index];
            return MYLITE_OK;
        }
    }
    return information_schema_resolve_column_reference(
        database,
        query,
        order_reference,
        COLUMN_REFERENCE_ORDER,
        out_order_index
    );
}

static int information_schema_plan_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct information_schema_query *out_query
) {
    return mylite_execution_plan_select_limit(database, limit_clause, &out_query->limit);
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
    out_query->source_schema_name = "information_schema";
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
        if (mylite_execution_text_equals_ascii_case_insensitive(
                definition->columns[column_index].name,
                column_name
            )) {
            *out_index = column_index;
            return MYLITE_OK;
        }
    }
    return MYLITE_ERROR;
}
