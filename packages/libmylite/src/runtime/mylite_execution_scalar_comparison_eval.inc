
static int scalar_comparison_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    int written = 0;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    rc = evaluate_scalar_comparison_expression(database, expression, &value);
    if (rc != MYLITE_OK || value.is_null) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    written =
        snprintf(out_cell->integer_text, sizeof(out_cell->integer_text), "%" PRId64, value.integer);
    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        set_runtime_error(database, "failed to format scalar comparison value");
        return MYLITE_ERROR;
    }

    out_cell->value = out_cell->integer_text;
    out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
    return MYLITE_OK;
}

static int evaluate_scalar_comparison_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
) {
    struct scalar_comparison_eval_stack expression_stack = {0};
    struct scalar_arithmetic_value_stack value_stack = {0};
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){.is_null = false, .integer = 0};
    if (!is_scalar_comparison_projection_expression(expression)) {
        set_scalar_comparison_unsupported_error(database);
        return MYLITE_ERROR;
    }
    rc = scalar_comparison_eval_stack_push(
        database,
        &expression_stack,
        SCALAR_COMPARISON_EVAL_ENTER,
        expression,
        MYLITE_SQL_AST_OPERATOR_NONE
    );
    while (rc == MYLITE_OK && expression_stack.count != 0U) {
        struct scalar_comparison_eval_frame frame =
            expression_stack.items[--expression_stack.count];

        rc = evaluate_scalar_comparison_frame(database, &expression_stack, &value_stack, &frame);
    }
    if (rc == MYLITE_OK) {
        rc = finish_scalar_arithmetic_result(database, &value_stack, out_value);
    }
    scalar_comparison_eval_stack_deinit(&expression_stack);
    scalar_arithmetic_value_stack_deinit(&value_stack);
    return rc;
}

static int evaluate_scalar_comparison_frame(
    struct mylite_db *database,
    struct scalar_comparison_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_comparison_eval_frame *frame
) {
    if (frame == NULL) {
        return MYLITE_MISUSE;
    }
    if (frame->kind == SCALAR_COMPARISON_EVAL_APPLY) {
        return evaluate_scalar_comparison_apply_frame(database, value_stack, frame->operator_kind);
    }
    if (frame->kind == SCALAR_COMPARISON_EVAL_SHORT_CIRCUIT_OR_ENTER_RIGHT) {
        return evaluate_scalar_comparison_short_circuit_or_enter_right_frame(
            database,
            expression_stack,
            value_stack,
            frame
        );
    }
    return evaluate_scalar_comparison_enter_frame(
        database,
        expression_stack,
        value_stack,
        frame->expression
    );
}

static int evaluate_scalar_comparison_apply_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
) {
    struct scalar_arithmetic_value left = {.is_null = false, .integer = 0};
    struct scalar_arithmetic_value right = {.is_null = false, .integer = 0};
    struct scalar_arithmetic_value result = {.is_null = false, .integer = 0};
    struct scalar_comparison_operation operation = {
        .operator_kind = operator_kind,
        .left = 0,
        .right = 0,
    };

    if (!scalar_arithmetic_value_stack_pop(value_stack, &right) ||
        !scalar_arithmetic_value_stack_pop(value_stack, &left)) {
        set_runtime_error(database, "invalid scalar comparison evaluation stack");
        return MYLITE_ERROR;
    }
    if (left.division_by_zero_warning_count > SIZE_MAX - right.division_by_zero_warning_count) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    result.division_by_zero_warning_count =
        left.division_by_zero_warning_count + right.division_by_zero_warning_count;

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        if (left.is_null || right.is_null) {
            result.integer = left.is_null == right.is_null ? 1 : 0;
            return scalar_arithmetic_value_stack_push(database, value_stack, result);
        }
        result.integer = left.integer == right.integer ? 1 : 0;
        return scalar_arithmetic_value_stack_push(database, value_stack, result);
    }
    if (left.is_null || right.is_null) {
        result.is_null = true;
        return scalar_arithmetic_value_stack_push(database, value_stack, result);
    }
    if (!is_scalar_comparison_operator(operator_kind)) {
        set_scalar_comparison_unsupported_error(database);
        return MYLITE_ERROR;
    }

    operation.left = left.integer;
    operation.right = right.integer;
    if (scalar_comparison_result_is_true(&operation)) {
        result.integer = 1;
    } else {
        result.integer = 0;
    }
    return scalar_arithmetic_value_stack_push(database, value_stack, result);
}

static int evaluate_scalar_comparison_short_circuit_or_enter_right_frame(
    struct mylite_db *database,
    struct scalar_comparison_eval_stack *expression_stack,
    const struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_comparison_eval_frame *frame
) {
    const struct scalar_arithmetic_value *left = NULL;
    int rc = MYLITE_OK;

    if (value_stack == NULL || frame == NULL || value_stack->count == 0U) {
        set_runtime_error(database, "invalid scalar comparison evaluation stack");
        return MYLITE_ERROR;
    }

    left = &value_stack->items[value_stack->count - 1U];
    if (left->is_null) {
        return MYLITE_OK;
    }

    rc = scalar_comparison_eval_stack_push(
        database,
        expression_stack,
        SCALAR_COMPARISON_EVAL_APPLY,
        NULL,
        frame->operator_kind
    );
    if (rc == MYLITE_OK) {
        rc = scalar_comparison_eval_stack_push(
            database,
            expression_stack,
            SCALAR_COMPARISON_EVAL_ENTER,
            frame->expression,
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    return rc;
}

static int evaluate_scalar_comparison_enter_frame(
    struct mylite_db *database,
    struct scalar_comparison_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_scalar_comparison_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        rc = evaluate_scalar_arithmetic_expression(database, expression, &value);
        if (rc == MYLITE_OK) {
            rc = scalar_arithmetic_value_stack_push(database, value_stack, value);
        }
        return rc;
    }

    operator_kind = mylite_sql_ast_node_operator(expression);
    if (!is_scalar_comparison_operator(operator_kind)) {
        rc = evaluate_scalar_arithmetic_expression(database, expression, &value);
        if (rc == MYLITE_OK) {
            rc = scalar_arithmetic_value_stack_push(database, value_stack, value);
        }
        return rc;
    }

    if (operator_kind != MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        rc = scalar_comparison_eval_stack_push(
            database,
            expression_stack,
            SCALAR_COMPARISON_EVAL_SHORT_CIRCUIT_OR_ENTER_RIGHT,
            child_at(expression, 1U),
            operator_kind
        );
        if (rc == MYLITE_OK) {
            rc = scalar_comparison_eval_stack_push(
                database,
                expression_stack,
                SCALAR_COMPARISON_EVAL_ENTER,
                child_at(expression, 0U),
                MYLITE_SQL_AST_OPERATOR_NONE
            );
        }
        return rc;
    }

    rc = scalar_comparison_eval_stack_push(
        database,
        expression_stack,
        SCALAR_COMPARISON_EVAL_APPLY,
        NULL,
        operator_kind
    );
    if (rc == MYLITE_OK) {
        rc = scalar_comparison_eval_stack_push(
            database,
            expression_stack,
            SCALAR_COMPARISON_EVAL_ENTER,
            child_at(expression, 1U),
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    if (rc == MYLITE_OK) {
        rc = scalar_comparison_eval_stack_push(
            database,
            expression_stack,
            SCALAR_COMPARISON_EVAL_ENTER,
            child_at(expression, 0U),
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    return rc;
}

static bool scalar_comparison_result_is_true(const struct scalar_comparison_operation *operation) {
    if (operation == NULL) {
        return false;
    }
    switch (operation->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return operation->left == operation->right;
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return operation->left != operation->right;
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return operation->left < operation->right;
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return operation->left <= operation->right;
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return operation->left > operation->right;
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return operation->left >= operation->right;
    default:
        return false;
    }
}

static int scalar_comparison_eval_stack_push(
    struct mylite_db *database,
    struct scalar_comparison_eval_stack *stack,
    enum scalar_comparison_eval_frame_kind kind,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
) {
    struct scalar_comparison_eval_frame *items = NULL;
    size_t capacity = 0U;

    if (stack == NULL) {
        return MYLITE_MISUSE;
    }
    if (stack->count == stack->capacity) {
        capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;
        if (capacity < stack->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        items = (struct scalar_comparison_eval_frame *)
            realloc((void *)stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        stack->items = items;
        stack->capacity = capacity;
    }

    stack->items[stack->count] = (struct scalar_comparison_eval_frame){
        .kind = kind,
        .expression = expression,
        .operator_kind = operator_kind,
    };
    ++stack->count;
    return MYLITE_OK;
}

static void scalar_comparison_eval_stack_deinit(struct scalar_comparison_eval_stack *stack) {
    if (stack == NULL) {
        return;
    }

    free((void *)stack->items);
    *stack = (struct scalar_comparison_eval_stack){0};
}
