
static int scalar_logical_value(
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
    rc = evaluate_scalar_logical_expression(database, expression, &value);
    if (rc != MYLITE_OK || value.is_null) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    written =
        snprintf(out_cell->integer_text, sizeof(out_cell->integer_text), "%" PRId64, value.integer);
    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        set_runtime_error(database, "failed to format scalar logical value");
        return MYLITE_ERROR;
    }

    out_cell->value = out_cell->integer_text;
    out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
    return MYLITE_OK;
}

static int evaluate_scalar_logical_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
) {
    struct scalar_logical_eval_stack expression_stack = {0};
    struct scalar_arithmetic_value_stack value_stack = {0};
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){.is_null = false, .integer = 0};
    if (!is_scalar_logical_projection_expression(expression)) {
        set_scalar_logical_unsupported_error(database);
        return MYLITE_ERROR;
    }
    rc = scalar_logical_eval_stack_push(
        database,
        &expression_stack,
        SCALAR_LOGICAL_EVAL_ENTER,
        expression,
        MYLITE_SQL_AST_OPERATOR_NONE
    );
    while (rc == MYLITE_OK && expression_stack.count != 0U) {
        struct scalar_logical_eval_frame frame = expression_stack.items[--expression_stack.count];

        rc = evaluate_scalar_logical_frame(database, &expression_stack, &value_stack, &frame);
    }
    if (rc == MYLITE_OK) {
        rc = finish_scalar_arithmetic_result(database, &value_stack, out_value);
    }
    scalar_logical_eval_stack_deinit(&expression_stack);
    scalar_arithmetic_value_stack_deinit(&value_stack);
    return rc;
}

static int evaluate_scalar_logical_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_logical_eval_frame *frame
) {
    if (frame == NULL) {
        return MYLITE_MISUSE;
    }
    switch (frame->kind) {
    case SCALAR_LOGICAL_EVAL_APPLY_NOT:
        return evaluate_scalar_logical_apply_not_frame(database, value_stack);
    case SCALAR_LOGICAL_EVAL_APPLY_COMPARISON:
        return evaluate_scalar_logical_apply_comparison_frame(
            database,
            value_stack,
            frame->operator_kind
        );
    case SCALAR_LOGICAL_EVAL_APPLY_IS:
        return evaluate_scalar_logical_apply_is_frame(database, value_stack, frame->operator_kind);
    case SCALAR_LOGICAL_EVAL_COMPARISON_SHORT_CIRCUIT_OR_ENTER_RIGHT:
        return evaluate_scalar_logical_comparison_short_circuit_or_enter_right_frame(
            database,
            expression_stack,
            value_stack,
            frame
        );
    case SCALAR_LOGICAL_EVAL_APPLY_LOGICAL:
        return evaluate_scalar_logical_apply_logical_frame(
            database,
            value_stack,
            frame->operator_kind
        );
    case SCALAR_LOGICAL_EVAL_LOGICAL_SHORT_CIRCUIT_OR_ENTER_RIGHT:
        return evaluate_scalar_logical_short_circuit_or_enter_right_frame(
            database,
            expression_stack,
            value_stack,
            frame
        );
    case SCALAR_LOGICAL_EVAL_ENTER:
    default:
        return evaluate_scalar_logical_enter_frame(
            database,
            expression_stack,
            value_stack,
            frame->expression
        );
    }
}

static int evaluate_scalar_logical_apply_not_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack
) {
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};

    if (!scalar_arithmetic_value_stack_pop(value_stack, &value)) {
        set_runtime_error(database, "invalid scalar logical evaluation stack");
        return MYLITE_ERROR;
    }
    if (value.is_null) {
        return scalar_arithmetic_value_stack_push(database, value_stack, value);
    }
    value.integer = value.integer == 0 ? 1 : 0;
    return scalar_arithmetic_value_stack_push(database, value_stack, value);
}

static int evaluate_scalar_logical_apply_comparison_frame(
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
        set_runtime_error(database, "invalid scalar logical comparison stack");
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
        set_scalar_logical_unsupported_error(database);
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

static int evaluate_scalar_logical_apply_is_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
) {
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    struct scalar_arithmetic_value result = {.is_null = false, .integer = 0};

    if (!scalar_arithmetic_value_stack_pop(value_stack, &value)) {
        set_runtime_error(database, "invalid scalar IS evaluation stack");
        return MYLITE_ERROR;
    }
    result.division_by_zero_warning_count = value.division_by_zero_warning_count;

    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
        if (value.is_null) {
            result.integer = 1;
        }
        break;
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
        if (!value.is_null) {
            result.integer = 1;
        }
        break;
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
        if (scalar_arithmetic_truth_value(&value)) {
            result.integer = 1;
        }
        break;
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
        if (!scalar_arithmetic_truth_value(&value)) {
            result.integer = 1;
        }
        break;
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
        if (!value.is_null && value.integer == 0) {
            result.integer = 1;
        }
        break;
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
        if (value.is_null || value.integer != 0) {
            result.integer = 1;
        }
        break;
    default:
        set_scalar_logical_unsupported_error(database);
        return MYLITE_ERROR;
    }

    return scalar_arithmetic_value_stack_push(database, value_stack, result);
}

static int evaluate_scalar_logical_comparison_short_circuit_or_enter_right_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    const struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_logical_eval_frame *frame
) {
    const struct scalar_arithmetic_value *left = NULL;
    int rc = MYLITE_OK;

    if (value_stack == NULL || frame == NULL || value_stack->count == 0U) {
        set_runtime_error(database, "invalid scalar logical comparison stack");
        return MYLITE_ERROR;
    }

    left = &value_stack->items[value_stack->count - 1U];
    if (left->is_null) {
        return MYLITE_OK;
    }

    rc = scalar_logical_eval_stack_push(
        database,
        expression_stack,
        SCALAR_LOGICAL_EVAL_APPLY_COMPARISON,
        NULL,
        frame->operator_kind
    );
    if (rc == MYLITE_OK) {
        rc = scalar_logical_eval_stack_push(
            database,
            expression_stack,
            SCALAR_LOGICAL_EVAL_ENTER,
            frame->expression,
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    return rc;
}

static int evaluate_scalar_logical_apply_logical_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
) {
    struct scalar_arithmetic_value left = {.is_null = false, .integer = 0};
    struct scalar_arithmetic_value right = {.is_null = false, .integer = 0};
    struct scalar_arithmetic_value result = {.is_null = false, .integer = 0};

    if (!scalar_arithmetic_value_stack_pop(value_stack, &right) ||
        !scalar_arithmetic_value_stack_pop(value_stack, &left)) {
        set_runtime_error(database, "invalid scalar logical evaluation stack");
        return MYLITE_ERROR;
    }
    if (left.division_by_zero_warning_count > SIZE_MAX - right.division_by_zero_warning_count) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    result.division_by_zero_warning_count =
        left.division_by_zero_warning_count + right.division_by_zero_warning_count;

    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
        evaluate_scalar_logical_and_result(&left, &right, &result);
        break;
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
        evaluate_scalar_logical_or_result(&left, &right, &result);
        break;
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
        evaluate_scalar_logical_xor_result(&left, &right, &result);
        break;
    default:
        set_scalar_logical_unsupported_error(database);
        return MYLITE_ERROR;
    }

    return scalar_arithmetic_value_stack_push(database, value_stack, result);
}

static void evaluate_scalar_logical_and_result(
    const struct scalar_arithmetic_value *left,
    const struct scalar_arithmetic_value *right,
    struct scalar_arithmetic_value *result
) {
    if ((!left->is_null && left->integer == 0) || (!right->is_null && right->integer == 0)) {
        result->integer = 0;
        return;
    }
    if (left->is_null || right->is_null) {
        result->is_null = true;
        return;
    }
    result->integer = 1;
}

static void evaluate_scalar_logical_or_result(
    const struct scalar_arithmetic_value *left,
    const struct scalar_arithmetic_value *right,
    struct scalar_arithmetic_value *result
) {
    if (scalar_arithmetic_truth_value(left) || scalar_arithmetic_truth_value(right)) {
        result->integer = 1;
        return;
    }
    if (left->is_null || right->is_null) {
        result->is_null = true;
        return;
    }
    result->integer = 0;
}

static void evaluate_scalar_logical_xor_result(
    const struct scalar_arithmetic_value *left,
    const struct scalar_arithmetic_value *right,
    struct scalar_arithmetic_value *result
) {
    bool left_true = false;
    bool right_true = false;

    if (left->is_null || right->is_null) {
        result->is_null = true;
        return;
    }

    left_true = scalar_arithmetic_truth_value(left);
    right_true = scalar_arithmetic_truth_value(right);
    if (left_true != right_true) {
        result->integer = 1;
    } else {
        result->integer = 0;
    }
}

static bool scalar_arithmetic_truth_value(const struct scalar_arithmetic_value *value) {
    if (value == NULL || value->is_null) {
        return false;
    }
    if (value->integer == 0) {
        return false;
    }
    return true;
}

static int evaluate_scalar_logical_short_circuit_or_enter_right_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_logical_eval_frame *frame
) {
    struct scalar_arithmetic_value *left = NULL;
    int rc = MYLITE_OK;

    if (value_stack == NULL || frame == NULL || value_stack->count == 0U) {
        set_runtime_error(database, "invalid scalar logical evaluation stack");
        return MYLITE_ERROR;
    }

    left = &value_stack->items[value_stack->count - 1U];
    if (frame->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_AND && !left->is_null &&
        left->integer == 0) {
        left->integer = 0;
        return MYLITE_OK;
    }
    if (frame->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_OR && !left->is_null &&
        left->integer != 0) {
        left->integer = 1;
        return MYLITE_OK;
    }
    if (frame->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR && left->is_null) {
        return MYLITE_OK;
    }

    rc = scalar_logical_eval_stack_push(
        database,
        expression_stack,
        SCALAR_LOGICAL_EVAL_APPLY_LOGICAL,
        NULL,
        frame->operator_kind
    );
    if (rc == MYLITE_OK) {
        rc = scalar_logical_eval_stack_push(
            database,
            expression_stack,
            SCALAR_LOGICAL_EVAL_ENTER,
            frame->expression,
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    return rc;
}

static int evaluate_scalar_logical_enter_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_scalar_logical_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return evaluate_scalar_logical_enter_unary_frame(
            database,
            expression_stack,
            value_stack,
            expression
        );
    }
    if (expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return evaluate_scalar_logical_enter_arithmetic_frame(database, value_stack, expression);
    }

    operator_kind = mylite_sql_ast_node_operator(expression);
    if (is_scalar_logical_operator(operator_kind)) {
        return evaluate_scalar_logical_enter_logical_frame(
            database,
            expression_stack,
            expression,
            operator_kind
        );
    }
    if (is_scalar_comparison_operator(operator_kind)) {
        return evaluate_scalar_logical_enter_comparison_frame(
            database,
            expression_stack,
            expression,
            operator_kind
        );
    }
    if (is_scalar_is_operator(operator_kind)) {
        return evaluate_scalar_logical_enter_is_frame(
            database,
            expression_stack,
            expression,
            operator_kind
        );
    }

    return evaluate_scalar_logical_enter_arithmetic_frame(database, value_stack, expression);
}

static int evaluate_scalar_logical_enter_unary_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
    int rc = MYLITE_OK;

    if (operator_kind != MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT) {
        return evaluate_scalar_logical_enter_arithmetic_frame(database, value_stack, expression);
    }

    rc = scalar_logical_eval_stack_push(
        database,
        expression_stack,
        SCALAR_LOGICAL_EVAL_APPLY_NOT,
        NULL,
        operator_kind
    );
    if (rc == MYLITE_OK) {
        rc = scalar_logical_eval_stack_push(
            database,
            expression_stack,
            SCALAR_LOGICAL_EVAL_ENTER,
            child_at(expression, 0U),
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    return rc;
}

static int evaluate_scalar_logical_enter_logical_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
) {
    int rc = scalar_logical_eval_stack_push(
        database,
        expression_stack,
        SCALAR_LOGICAL_EVAL_LOGICAL_SHORT_CIRCUIT_OR_ENTER_RIGHT,
        child_at(expression, 1U),
        operator_kind
    );

    if (rc == MYLITE_OK) {
        rc = scalar_logical_eval_stack_push(
            database,
            expression_stack,
            SCALAR_LOGICAL_EVAL_ENTER,
            child_at(expression, 0U),
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    return rc;
}

static int evaluate_scalar_logical_enter_comparison_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
) {
    int rc = MYLITE_OK;

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        return evaluate_scalar_logical_enter_null_safe_comparison_frame(
            database,
            expression_stack,
            expression,
            operator_kind
        );
    }

    rc = scalar_logical_eval_stack_push(
        database,
        expression_stack,
        SCALAR_LOGICAL_EVAL_COMPARISON_SHORT_CIRCUIT_OR_ENTER_RIGHT,
        child_at(expression, 1U),
        operator_kind
    );
    if (rc == MYLITE_OK) {
        rc = scalar_logical_eval_stack_push(
            database,
            expression_stack,
            SCALAR_LOGICAL_EVAL_ENTER,
            child_at(expression, 0U),
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    return rc;
}

static int evaluate_scalar_logical_enter_is_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
) {
    int rc = scalar_logical_eval_stack_push(
        database,
        expression_stack,
        SCALAR_LOGICAL_EVAL_APPLY_IS,
        NULL,
        operator_kind
    );

    if (rc == MYLITE_OK) {
        rc = scalar_logical_eval_stack_push(
            database,
            expression_stack,
            SCALAR_LOGICAL_EVAL_ENTER,
            child_at(expression, 0U),
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    return rc;
}

static int evaluate_scalar_logical_enter_null_safe_comparison_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
) {
    int rc = scalar_logical_eval_stack_push(
        database,
        expression_stack,
        SCALAR_LOGICAL_EVAL_APPLY_COMPARISON,
        NULL,
        operator_kind
    );

    if (rc == MYLITE_OK) {
        rc = scalar_logical_eval_stack_push(
            database,
            expression_stack,
            SCALAR_LOGICAL_EVAL_ENTER,
            child_at(expression, 1U),
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    if (rc == MYLITE_OK) {
        rc = scalar_logical_eval_stack_push(
            database,
            expression_stack,
            SCALAR_LOGICAL_EVAL_ENTER,
            child_at(expression, 0U),
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    return rc;
}

static int evaluate_scalar_logical_enter_arithmetic_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
) {
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    int rc = evaluate_scalar_arithmetic_expression(database, expression, &value);

    if (rc == MYLITE_OK) {
        rc = scalar_arithmetic_value_stack_push(database, value_stack, value);
    }
    return rc;
}

static int scalar_logical_eval_stack_push(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *stack,
    enum scalar_logical_eval_frame_kind kind,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
) {
    struct scalar_logical_eval_frame *items = NULL;
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
        items = (struct scalar_logical_eval_frame *)
            realloc((void *)stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        stack->items = items;
        stack->capacity = capacity;
    }

    stack->items[stack->count] = (struct scalar_logical_eval_frame){
        .kind = kind,
        .expression = expression,
        .operator_kind = operator_kind,
    };
    ++stack->count;
    return MYLITE_OK;
}

static void scalar_logical_eval_stack_deinit(struct scalar_logical_eval_stack *stack) {
    if (stack == NULL) {
        return;
    }

    free((void *)stack->items);
    *stack = (struct scalar_logical_eval_stack){0};
}
