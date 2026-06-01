static int evaluate_bit_count_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
) {
    struct scalar_arithmetic_value arithmetic = {.is_null = false, .integer = 0};
    bool handled = false;
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_bitwise_value){.is_null = false, .integer = 0U};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_bit_count_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_bit_count_direct_literal_operand(database, expression, out_value, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (is_scalar_bitwise_projection_expression(expression)) {
        return evaluate_scalar_bitwise_expression(database, expression, out_value);
    }
    if (!is_scalar_arithmetic_projection_expression(expression)) {
        set_bit_count_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_scalar_arithmetic_expression(database, expression, &arithmetic);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_value->is_null = arithmetic.is_null;
    out_value->integer = (uint64_t)arithmetic.integer;
    out_value->division_by_zero_warning_count = arithmetic.division_by_zero_warning_count;
    return MYLITE_OK;
}

static int evaluate_bit_count_direct_literal_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value,
    bool *out_handled
) {
    static const uint64_t int64_min_magnitude = 9223372036854775808ULL;
    const struct mylite_sql_ast_node *literal = expression;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;
    bool has_sign = false;
    uint64_t magnitude = 0U;

    if (out_value == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (expression == NULL) {
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return MYLITE_OK;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            return MYLITE_OK;
        }
        has_sign = true;
        is_negative = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_OK;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_value->is_null = true;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        out_value->integer = 1U;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_value->integer = 0U;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return MYLITE_OK;
    }
    *out_handled = true;

    if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        set_bit_count_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (!is_negative) {
        out_value->integer = magnitude;
        return MYLITE_OK;
    }
    if (magnitude > int64_min_magnitude) {
        set_bit_count_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (magnitude == int64_min_magnitude) {
        out_value->integer = (uint64_t)INT64_MIN;
    } else if (magnitude != 0U) {
        out_value->integer = (uint64_t)(-(int64_t)magnitude);
    }
    return MYLITE_OK;
}

static uint64_t count_uint64_bits(uint64_t value) {
    uint64_t count = 0U;

    while (value != 0U) {
        count += value & 1U;
        value >>= 1U;
    }
    return count;
}

static int evaluate_scalar_bitwise_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
) {
    struct scalar_bitwise_eval_stack expression_stack = {0};
    struct scalar_bitwise_value_stack value_stack = {0};
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_bitwise_value){.is_null = false, .integer = 0U};
    if (expression == NULL) {
        set_scalar_bitwise_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = scalar_bitwise_eval_stack_push(
        database,
        &expression_stack,
        SCALAR_BITWISE_EVAL_ENTER,
        expression,
        MYLITE_SQL_AST_OPERATOR_NONE
    );
    while (rc == MYLITE_OK && expression_stack.count != 0U) {
        struct scalar_bitwise_eval_frame frame = expression_stack.items[--expression_stack.count];

        rc = evaluate_scalar_bitwise_frame(database, &expression_stack, &value_stack, &frame);
    }
    if (rc == MYLITE_OK && !scalar_bitwise_value_stack_pop(&value_stack, out_value)) {
        set_runtime_error(database, "missing scalar bitwise result");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && value_stack.count != 0U) {
        set_runtime_error(database, "invalid scalar bitwise result stack");
        rc = MYLITE_ERROR;
    }

    scalar_bitwise_eval_stack_deinit(&expression_stack);
    scalar_bitwise_value_stack_deinit(&value_stack);
    return rc;
}

static int evaluate_scalar_bitwise_frame(
    struct mylite_db *database,
    struct scalar_bitwise_eval_stack *expression_stack,
    struct scalar_bitwise_value_stack *value_stack,
    const struct scalar_bitwise_eval_frame *frame
) {
    if (frame == NULL) {
        return MYLITE_MISUSE;
    }
    switch (frame->kind) {
    case SCALAR_BITWISE_EVAL_APPLY:
        return evaluate_scalar_bitwise_apply_frame(database, value_stack, frame->operator_kind);
    case SCALAR_BITWISE_EVAL_APPLY_UNARY:
        return evaluate_scalar_bitwise_apply_unary_frame(database, value_stack);
    case SCALAR_BITWISE_EVAL_SHORT_CIRCUIT_OR_ENTER_RIGHT:
        return evaluate_scalar_bitwise_short_circuit_or_enter_right_frame(
            database,
            expression_stack,
            value_stack,
            frame
        );
    case SCALAR_BITWISE_EVAL_ENTER:
        return evaluate_scalar_bitwise_enter_frame(
            database,
            expression_stack,
            value_stack,
            frame->expression
        );
    }

    set_runtime_error(database, "invalid scalar bitwise evaluation frame");
    return MYLITE_ERROR;
}

static int evaluate_scalar_bitwise_apply_frame(
    struct mylite_db *database,
    struct scalar_bitwise_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
) {
    struct scalar_bitwise_value left = {.is_null = false, .integer = 0U};
    struct scalar_bitwise_value right = {.is_null = false, .integer = 0U};
    struct scalar_bitwise_value result = {.is_null = false, .integer = 0U};
    int rc = MYLITE_OK;

    if (!scalar_bitwise_value_stack_pop(value_stack, &right) ||
        !scalar_bitwise_value_stack_pop(value_stack, &left)) {
        set_runtime_error(database, "invalid scalar bitwise evaluation stack");
        return MYLITE_ERROR;
    }
    if (left.division_by_zero_warning_count > SIZE_MAX - right.division_by_zero_warning_count) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    result.division_by_zero_warning_count =
        left.division_by_zero_warning_count + right.division_by_zero_warning_count;

    rc = apply_scalar_bitwise_operator(&left, &right, operator_kind, &result);
    if (rc == MYLITE_OK) {
        rc = scalar_bitwise_value_stack_push(database, value_stack, result);
    } else {
        set_scalar_bitwise_unsupported_error(database);
    }
    return rc;
}

static int evaluate_scalar_bitwise_apply_unary_frame(
    struct mylite_db *database,
    struct scalar_bitwise_value_stack *value_stack
) {
    struct scalar_bitwise_value value = {.is_null = false, .integer = 0U};

    if (!scalar_bitwise_value_stack_pop(value_stack, &value)) {
        set_runtime_error(database, "invalid scalar bitwise evaluation stack");
        return MYLITE_ERROR;
    }
    if (!value.is_null) {
        value.integer = ~value.integer;
    }
    return scalar_bitwise_value_stack_push(database, value_stack, value);
}

static int evaluate_scalar_bitwise_short_circuit_or_enter_right_frame(
    struct mylite_db *database,
    struct scalar_bitwise_eval_stack *expression_stack,
    const struct scalar_bitwise_value_stack *value_stack,
    const struct scalar_bitwise_eval_frame *frame
) {
    const struct scalar_bitwise_value *left = NULL;
    int rc = MYLITE_OK;

    if (value_stack == NULL || frame == NULL || value_stack->count == 0U) {
        set_runtime_error(database, "invalid scalar bitwise evaluation stack");
        return MYLITE_ERROR;
    }

    left = &value_stack->items[value_stack->count - 1U];
    if (left->is_null) {
        return MYLITE_OK;
    }

    rc = scalar_bitwise_eval_stack_push(
        database,
        expression_stack,
        SCALAR_BITWISE_EVAL_APPLY,
        NULL,
        frame->operator_kind
    );
    if (rc == MYLITE_OK) {
        rc = scalar_bitwise_eval_stack_push(
            database,
            expression_stack,
            SCALAR_BITWISE_EVAL_ENTER,
            frame->expression,
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    return rc;
}

static int evaluate_scalar_bitwise_enter_frame(
    struct mylite_db *database,
    struct scalar_bitwise_eval_stack *expression_stack,
    struct scalar_bitwise_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    struct scalar_bitwise_value value = {.is_null = false, .integer = 0U};
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_scalar_bitwise_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        operator_kind = mylite_sql_ast_node_operator(expression);
        if (!is_scalar_bitwise_unary_operator(operator_kind)) {
            rc = evaluate_scalar_bitwise_operand(database, expression, &value);
            if (rc == MYLITE_OK) {
                rc = scalar_bitwise_value_stack_push(database, value_stack, value);
            }
            return rc;
        }
        rc = scalar_bitwise_eval_stack_push(
            database,
            expression_stack,
            SCALAR_BITWISE_EVAL_APPLY_UNARY,
            NULL,
            operator_kind
        );
        if (rc == MYLITE_OK) {
            rc = scalar_bitwise_eval_stack_push(
                database,
                expression_stack,
                SCALAR_BITWISE_EVAL_ENTER,
                child_at(expression, 0U),
                MYLITE_SQL_AST_OPERATOR_NONE
            );
        }
        return rc;
    }
    if (expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION) {
        operator_kind = mylite_sql_ast_node_operator(expression);
        if (is_scalar_bitwise_operator(operator_kind)) {
            rc = scalar_bitwise_eval_stack_push(
                database,
                expression_stack,
                SCALAR_BITWISE_EVAL_SHORT_CIRCUIT_OR_ENTER_RIGHT,
                child_at(expression, 1U),
                operator_kind
            );
            if (rc == MYLITE_OK) {
                rc = scalar_bitwise_eval_stack_push(
                    database,
                    expression_stack,
                    SCALAR_BITWISE_EVAL_ENTER,
                    child_at(expression, 0U),
                    MYLITE_SQL_AST_OPERATOR_NONE
                );
            }
            return rc;
        }
    }

    rc = evaluate_scalar_bitwise_operand(database, expression, &value);
    if (rc == MYLITE_OK) {
        rc = scalar_bitwise_value_stack_push(database, value_stack, value);
    }
    return rc;
}

static int evaluate_scalar_bitwise_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
) {
    struct scalar_arithmetic_value arithmetic = {.is_null = false, .integer = 0};
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_bitwise_value){.is_null = false, .integer = 0U};
    rc = evaluate_scalar_arithmetic_expression(database, expression, &arithmetic);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_value->is_null = arithmetic.is_null;
    out_value->integer = (uint64_t)arithmetic.integer;
    out_value->division_by_zero_warning_count = arithmetic.division_by_zero_warning_count;
    return MYLITE_OK;
}

static int apply_scalar_bitwise_operator(
    const struct scalar_bitwise_value *left,
    const struct scalar_bitwise_value *right,
    enum mylite_sql_ast_operator operator_kind,
    struct scalar_bitwise_value *out_result
) {
    if (left == NULL || right == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    if (left->is_null || right->is_null) {
        out_result->is_null = true;
        return MYLITE_OK;
    }

    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
        out_result->integer = left->integer ^ right->integer;
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_LEFT_SHIFT:
        out_result->integer =
            right->integer >= scalar_bitwise_integer_bits ? 0U : left->integer << right->integer;
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_RIGHT_SHIFT:
        out_result->integer =
            right->integer >= scalar_bitwise_integer_bits ? 0U : left->integer >> right->integer;
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
        out_result->integer = left->integer & right->integer;
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
        out_result->integer = left->integer | right->integer;
        return MYLITE_OK;
    default:
        return MYLITE_ERROR;
    }
}

static int scalar_bitwise_eval_stack_push(
    struct mylite_db *database,
    struct scalar_bitwise_eval_stack *stack,
    enum scalar_bitwise_eval_frame_kind kind,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
) {
    struct scalar_bitwise_eval_frame *items = NULL;
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
        items = (struct scalar_bitwise_eval_frame *)
            realloc((void *)stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        stack->items = items;
        stack->capacity = capacity;
    }

    stack->items[stack->count] = (struct scalar_bitwise_eval_frame){
        .kind = kind,
        .expression = expression,
        .operator_kind = operator_kind,
    };
    ++stack->count;
    return MYLITE_OK;
}

static void scalar_bitwise_eval_stack_deinit(struct scalar_bitwise_eval_stack *stack) {
    if (stack == NULL) {
        return;
    }

    free((void *)stack->items);
    *stack = (struct scalar_bitwise_eval_stack){0};
}

static int scalar_bitwise_value_stack_push(
    struct mylite_db *database,
    struct scalar_bitwise_value_stack *stack,
    struct scalar_bitwise_value value
) {
    struct scalar_bitwise_value *items = NULL;
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
        items =
            (struct scalar_bitwise_value *)realloc((void *)stack->items, capacity * sizeof(*items));
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

static bool scalar_bitwise_value_stack_pop(
    struct scalar_bitwise_value_stack *stack,
    struct scalar_bitwise_value *out_value
) {
    if (stack == NULL || out_value == NULL || stack->count == 0U) {
        return false;
    }

    --stack->count;
    *out_value = stack->items[stack->count];
    return true;
}

static void scalar_bitwise_value_stack_deinit(struct scalar_bitwise_value_stack *stack) {
    if (stack == NULL) {
        return;
    }

    free((void *)stack->items);
    *stack = (struct scalar_bitwise_value_stack){0};
}

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

static int scalar_arithmetic_value(
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
    rc = evaluate_scalar_arithmetic_expression(database, expression, &value);
    if (rc != MYLITE_OK || value.is_null) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    written =
        snprintf(out_cell->integer_text, sizeof(out_cell->integer_text), "%" PRId64, value.integer);
    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        set_runtime_error(database, "failed to format scalar arithmetic value");
        return MYLITE_ERROR;
    }

    out_cell->value = out_cell->integer_text;
    out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
    return MYLITE_OK;
}

static int evaluate_scalar_arithmetic_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
) {
    struct scalar_arithmetic_eval_stack expression_stack = {0};
    struct scalar_arithmetic_value_stack value_stack = {0};
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){.is_null = false, .integer = 0};
    if (expression == NULL) {
        set_scalar_arithmetic_unsupported_error(database);
        return MYLITE_ERROR;
    }
    rc = scalar_arithmetic_eval_stack_push(
        database,
        &expression_stack,
        SCALAR_ARITHMETIC_EVAL_ENTER,
        expression,
        MYLITE_SQL_AST_OPERATOR_NONE
    );
    while (rc == MYLITE_OK && expression_stack.count != 0U) {
        struct scalar_arithmetic_eval_frame frame =
            expression_stack.items[--expression_stack.count];

        rc = evaluate_scalar_arithmetic_frame(database, &expression_stack, &value_stack, &frame);
    }
    if (rc == MYLITE_OK) {
        rc = finish_scalar_arithmetic_result(database, &value_stack, out_value);
    }
    scalar_arithmetic_eval_stack_deinit(&expression_stack);
    scalar_arithmetic_value_stack_deinit(&value_stack);
    return rc;
}

static int evaluate_scalar_arithmetic_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_arithmetic_eval_frame *frame
) {
    if (frame == NULL) {
        return MYLITE_MISUSE;
    }
    if (frame->kind == SCALAR_ARITHMETIC_EVAL_APPLY) {
        return evaluate_scalar_arithmetic_apply_frame(database, value_stack, frame->operator_kind);
    }
    if (frame->kind == SCALAR_ARITHMETIC_EVAL_APPLY_UNARY) {
        return evaluate_scalar_arithmetic_apply_unary_frame(
            database,
            value_stack,
            frame->operator_kind
        );
    }
    return evaluate_scalar_arithmetic_enter_frame(
        database,
        expression_stack,
        value_stack,
        frame->expression
    );
}

static int evaluate_scalar_arithmetic_apply_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
) {
    struct scalar_arithmetic_value left = {.is_null = false, .integer = 0};
    struct scalar_arithmetic_value right = {.is_null = false, .integer = 0};
    struct scalar_arithmetic_value result = {.is_null = false, .integer = 0};
    struct scalar_arithmetic_operation operation = {
        .operator_kind = operator_kind,
        .left = 0,
        .right = 0,
    };
    int rc = MYLITE_OK;

    if (!scalar_arithmetic_value_stack_pop(value_stack, &right) ||
        !scalar_arithmetic_value_stack_pop(value_stack, &left)) {
        set_runtime_error(database, "invalid scalar arithmetic evaluation stack");
        return MYLITE_ERROR;
    }
    if (left.division_by_zero_warning_count > SIZE_MAX - right.division_by_zero_warning_count) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    result.division_by_zero_warning_count =
        left.division_by_zero_warning_count + right.division_by_zero_warning_count;
    if (left.is_null || right.is_null) {
        result.is_null = true;
        return scalar_arithmetic_value_stack_push(database, value_stack, result);
    }
    if ((operator_kind == MYLITE_SQL_AST_OPERATOR_MODULO ||
         operator_kind == MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE) &&
        right.integer == 0) {
        result.is_null = true;
        if (result.division_by_zero_warning_count == SIZE_MAX) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        ++result.division_by_zero_warning_count;
        return scalar_arithmetic_value_stack_push(database, value_stack, result);
    }

    operation.left = left.integer;
    operation.right = right.integer;
    rc = apply_scalar_arithmetic_operator(database, &operation, &result.integer);
    if (rc == MYLITE_OK) {
        rc = scalar_arithmetic_value_stack_push(database, value_stack, result);
    }
    return rc;
}

static int evaluate_scalar_arithmetic_apply_unary_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
) {
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    int64_t result = 0;

    if (!scalar_arithmetic_value_stack_pop(value_stack, &value)) {
        set_runtime_error(database, "invalid scalar arithmetic evaluation stack");
        return MYLITE_ERROR;
    }
    if (value.is_null || operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
        return scalar_arithmetic_value_stack_push(database, value_stack, value);
    }
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        set_scalar_arithmetic_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (checked_int64_negate(value.integer, &result)) {
        set_scalar_arithmetic_overflow_error(database);
        return MYLITE_ERROR;
    }

    value.integer = result;
    return scalar_arithmetic_value_stack_push(database, value_stack, value);
}

static int evaluate_scalar_arithmetic_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
) {
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_scalar_arithmetic_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        int rc = MYLITE_OK;

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            set_scalar_arithmetic_unsupported_error(database);
            return MYLITE_ERROR;
        }
        rc = scalar_arithmetic_eval_stack_push(
            database,
            expression_stack,
            SCALAR_ARITHMETIC_EVAL_APPLY_UNARY,
            NULL,
            operator_kind
        );
        if (rc == MYLITE_OK) {
            rc = scalar_arithmetic_eval_stack_push(
                database,
                expression_stack,
                SCALAR_ARITHMETIC_EVAL_ENTER,
                child_at(expression, 0U),
                MYLITE_SQL_AST_OPERATOR_NONE
            );
        }
        return rc;
    }
    if (expression->kind == MYLITE_SQL_AST_MOD_FUNCTION) {
        int rc = MYLITE_OK;

        if (mylite_sql_ast_node_child_count(expression) != 2U) {
            set_scalar_arithmetic_unsupported_error(database);
            return MYLITE_ERROR;
        }
        rc = scalar_arithmetic_eval_stack_push(
            database,
            expression_stack,
            SCALAR_ARITHMETIC_EVAL_APPLY,
            NULL,
            MYLITE_SQL_AST_OPERATOR_MODULO
        );
        if (rc == MYLITE_OK) {
            rc = scalar_arithmetic_eval_stack_push(
                database,
                expression_stack,
                SCALAR_ARITHMETIC_EVAL_ENTER,
                child_at(expression, 1U),
                MYLITE_SQL_AST_OPERATOR_NONE
            );
        }
        if (rc == MYLITE_OK) {
            rc = scalar_arithmetic_eval_stack_push(
                database,
                expression_stack,
                SCALAR_ARITHMETIC_EVAL_ENTER,
                child_at(expression, 0U),
                MYLITE_SQL_AST_OPERATOR_NONE
            );
        }
        return rc;
    }
    if (expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return evaluate_scalar_arithmetic_operand_frame(database, value_stack, expression);
    }

    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

    if (!is_scalar_arithmetic_operator(operator_kind)) {
        return evaluate_scalar_arithmetic_non_arithmetic_binary_frame(
            database,
            value_stack,
            expression
        );
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE) {
        bool left_operand_short_circuits = false;

        int rc = scalar_arithmetic_div_left_operand_short_circuits(
            database,
            child_at(expression, 0U),
            &left_operand_short_circuits
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (left_operand_short_circuits) {
            value.is_null = true;
            return scalar_arithmetic_value_stack_push(database, value_stack, value);
        }
    }

    int rc = scalar_arithmetic_eval_stack_push(
        database,
        expression_stack,
        SCALAR_ARITHMETIC_EVAL_APPLY,
        NULL,
        operator_kind
    );

    if (rc == MYLITE_OK) {
        rc = scalar_arithmetic_eval_stack_push(
            database,
            expression_stack,
            SCALAR_ARITHMETIC_EVAL_ENTER,
            child_at(expression, 1U),
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    if (rc == MYLITE_OK) {
        rc = scalar_arithmetic_eval_stack_push(
            database,
            expression_stack,
            SCALAR_ARITHMETIC_EVAL_ENTER,
            child_at(expression, 0U),
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    return rc;
}

static int evaluate_scalar_arithmetic_operand_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
) {
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    int rc = evaluate_scalar_arithmetic_operand(database, expression, &value);

    if (rc == MYLITE_OK) {
        rc = scalar_arithmetic_value_stack_push(database, value_stack, value);
    }
    return rc;
}

static int evaluate_scalar_arithmetic_non_arithmetic_binary_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
) {
    if (!row_scalar_expression_is_concat_operator(expression)) {
        set_scalar_arithmetic_unsupported_error(database);
        return MYLITE_ERROR;
    }
    return evaluate_scalar_arithmetic_operand_frame(database, value_stack, expression);
}

static int scalar_arithmetic_div_left_operand_short_circuits(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_short_circuits
) {
    struct scalar_arithmetic_node_stack stack = {0};
    bool short_circuits = true;
    int rc = MYLITE_OK;

    if (out_short_circuits == NULL) {
        return MYLITE_MISUSE;
    }
    *out_short_circuits = false;
    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    while (stack.count != 0U && rc == MYLITE_OK && short_circuits) {
        const struct mylite_sql_ast_node *current =
            unwrap_parenthesized_expression(stack.items[--stack.count]);

        rc = scalar_arithmetic_div_short_circuit_visit_node(
            database,
            current,
            &stack,
            &short_circuits
        );
    }
    if (rc == MYLITE_OK) {
        *out_short_circuits = short_circuits;
    }
    scalar_arithmetic_node_stack_deinit(&stack);
    return rc;
}

static int scalar_arithmetic_div_short_circuit_visit_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack,
    bool *out_short_circuits
) {
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;

    if (expression == NULL || out_short_circuits == NULL) {
        if (out_short_circuits != NULL) {
            *out_short_circuits = false;
        }
        return MYLITE_OK;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        *out_short_circuits =
            mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL;
        return MYLITE_OK;
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        operator_kind = mylite_sql_ast_node_operator(expression);
        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            *out_short_circuits = false;
            return MYLITE_OK;
        }
        return scalar_arithmetic_div_short_circuit_push_child(
            database,
            stack,
            child_at(expression, 0U)
        );
    case MYLITE_SQL_AST_IFNULL_FUNCTION:
        return scalar_arithmetic_div_short_circuit_push_function_arguments(
            database,
            stack,
            expression,
            out_short_circuits
        );
    case MYLITE_SQL_AST_COALESCE_FUNCTION:
    case MYLITE_SQL_AST_NULLIF_FUNCTION:
        return scalar_arithmetic_div_short_circuit_push_child(
            database,
            stack,
            child_at(expression, 0U)
        );
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
        return scalar_arithmetic_div_short_circuit_push_function_arguments(
            database,
            stack,
            expression,
            out_short_circuits
        );
    default:
        *out_short_circuits = false;
        return MYLITE_OK;
    }
}

static int scalar_arithmetic_div_short_circuit_push_child(
    struct mylite_db *database,
    struct scalar_arithmetic_node_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    if (!scalar_arithmetic_node_stack_push(stack, expression)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int scalar_arithmetic_div_short_circuit_push_function_arguments(
    struct mylite_db *database,
    struct scalar_arithmetic_node_stack *stack,
    const struct mylite_sql_ast_node *arguments,
    bool *out_short_circuits
) {
    size_t child_count = mylite_sql_ast_node_child_count(arguments);

    if (child_count == 0U) {
        *out_short_circuits = false;
        return MYLITE_OK;
    }
    for (size_t child_index = 0U; child_index < child_count; ++child_index) {
        int rc = scalar_arithmetic_div_short_circuit_push_child(
            database,
            stack,
            child_at(arguments, child_index)
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int finish_scalar_arithmetic_result(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    struct scalar_arithmetic_value *out_value
) {
    if (!scalar_arithmetic_value_stack_pop(value_stack, out_value)) {
        set_runtime_error(database, "missing scalar arithmetic result");
        return MYLITE_ERROR;
    }
    if (value_stack->count != 0U) {
        set_runtime_error(database, "invalid scalar arithmetic result stack");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int evaluate_scalar_arithmetic_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
) {
    struct session_scalar_cell cell = {0};
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (!is_scalar_value_projection_expression(expression)) {
        if (!row_scalar_expression_is_concat_operator(expression)) {
            set_scalar_arithmetic_unsupported_error(database);
            return MYLITE_ERROR;
        }
        rc = scalar_concat_operator_value(database, expression, &cell);
        if (rc == MYLITE_OK) {
            rc = parse_scalar_arithmetic_operand(database, &cell, out_value);
        }
        session_scalar_cell_deinit(&cell);
        return rc;
    }
    if ((expression->kind == MYLITE_SQL_AST_LITERAL ||
         expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) &&
        !is_if_non_function_value_expression(expression)) {
        set_scalar_arithmetic_operand_out_of_range_error(database);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_USER_VARIABLE) {
        rc = session_user_variable_value(database, expression, &cell);
        if (rc == MYLITE_OK) {
            rc = parse_scalar_arithmetic_operand(database, &cell, out_value);
        }
        return rc;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        rc = literal_projection_value(database, expression, &cell);
        break;
    case MYLITE_SQL_AST_IF_FUNCTION:
        rc = if_function_value(database, expression, &cell);
        break;
    case MYLITE_SQL_AST_IFNULL_FUNCTION:
        rc = ifnull_function_value(database, expression, &cell);
        break;
    case MYLITE_SQL_AST_COALESCE_FUNCTION:
        rc = coalesce_function_value(database, expression, &cell);
        break;
    case MYLITE_SQL_AST_NULLIF_FUNCTION:
        rc = nullif_function_value(database, expression, &cell);
        break;
    case MYLITE_SQL_AST_ISNULL_FUNCTION:
        rc = isnull_function_value(database, expression, &cell);
        break;
    default:
        set_scalar_arithmetic_unsupported_error(database);
        rc = MYLITE_ERROR;
        break;
    }
    if (rc == MYLITE_OK) {
        rc = parse_scalar_arithmetic_operand(database, &cell, out_value);
    }
    return rc;
}

static int parse_scalar_arithmetic_operand(
    struct mylite_db *database,
    const struct session_scalar_cell *cell,
    struct scalar_arithmetic_value *out_value
) {
    static const uint64_t int64_min_magnitude = 9223372036854775808ULL;
    const char *text = cell == NULL ? NULL : cell->value;
    uint64_t limit = (uint64_t)INT64_MAX;
    uint64_t magnitude = 0U;
    bool is_negative = false;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){.is_null = false, .integer = 0};
    out_value->division_by_zero_warning_count =
        cell == NULL ? 0U : cell->staged_division_by_zero_warning_count;
    if (text == NULL) {
        out_value->is_null = true;
        return MYLITE_OK;
    }
    if (text[0] == '-') {
        is_negative = true;
        limit = int64_min_magnitude;
        ++text;
    }
    if (text[0] == '\0') {
        set_scalar_arithmetic_unsupported_error(database);
        return MYLITE_ERROR;
    }

    for (size_t index = 0U; text[index] != '\0'; ++index) {
        unsigned char byte = (unsigned char)text[index];
        uint64_t digit = 0U;

        if (byte < '0' || byte > '9') {
            set_scalar_arithmetic_unsupported_error(database);
            return MYLITE_ERROR;
        }
        digit = (uint64_t)(byte - '0');
        if (magnitude > (limit - digit) / decimal_base) {
            set_scalar_arithmetic_operand_out_of_range_error(database);
            return MYLITE_ERROR;
        }
        magnitude = (magnitude * decimal_base) + digit;
    }

    if (is_negative && magnitude == int64_min_magnitude) {
        out_value->integer = INT64_MIN;
    } else if (is_negative && magnitude != 0U) {
        out_value->integer = -(int64_t)magnitude;
    } else {
        out_value->integer = (int64_t)magnitude;
    }
    return MYLITE_OK;
}

static int apply_scalar_arithmetic_operator(
    struct mylite_db *database,
    const struct scalar_arithmetic_operation *operation,
    int64_t *out_result
) {
    bool overflow = false;

    if (operation == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    switch (operation->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
        overflow = checked_int64_add(operation->left, operation->right, out_result);
        break;
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        overflow = checked_int64_subtract(operation->left, operation->right, out_result);
        break;
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        overflow = checked_int64_multiply(operation->left, operation->right, out_result);
        break;
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        overflow = checked_int64_modulo(operation->left, operation->right, out_result);
        break;
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
        overflow = checked_int64_divide(operation->left, operation->right, out_result);
        break;
    default:
        set_scalar_arithmetic_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (overflow) {
        set_scalar_arithmetic_overflow_error(database);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int scalar_arithmetic_eval_stack_push(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *stack,
    enum scalar_arithmetic_eval_frame_kind kind,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
) {
    struct scalar_arithmetic_eval_frame *items = NULL;
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
        items = (struct scalar_arithmetic_eval_frame *)
            realloc((void *)stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        stack->items = items;
        stack->capacity = capacity;
    }

    stack->items[stack->count] = (struct scalar_arithmetic_eval_frame){
        .kind = kind,
        .expression = expression,
        .operator_kind = operator_kind,
    };
    ++stack->count;
    return MYLITE_OK;
}

static void scalar_arithmetic_eval_stack_deinit(struct scalar_arithmetic_eval_stack *stack) {
    if (stack == NULL) {
        return;
    }

    free((void *)stack->items);
    *stack = (struct scalar_arithmetic_eval_stack){0};
}

static int scalar_arithmetic_value_stack_push(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *stack,
    struct scalar_arithmetic_value value
) {
    struct scalar_arithmetic_value *items = NULL;
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
        items = (struct scalar_arithmetic_value *)
            realloc((void *)stack->items, capacity * sizeof(*items));
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

static bool scalar_arithmetic_value_stack_pop(
    struct scalar_arithmetic_value_stack *stack,
    struct scalar_arithmetic_value *out_value
) {
    if (stack == NULL || out_value == NULL || stack->count == 0U) {
        return false;
    }

    --stack->count;
    *out_value = stack->items[stack->count];
    return true;
}

static void scalar_arithmetic_value_stack_deinit(struct scalar_arithmetic_value_stack *stack) {
    if (stack == NULL) {
        return;
    }

    free((void *)stack->items);
    *stack = (struct scalar_arithmetic_value_stack){0};
}

static bool scalar_arithmetic_node_stack_push(
    struct scalar_arithmetic_node_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node **items = NULL;
    size_t capacity = 0U;

    if (stack == NULL) {
        return false;
    }
    if (stack->count == stack->capacity) {
        capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;
        if (capacity < stack->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            return false;
        }
        items = (const struct mylite_sql_ast_node **)
            realloc((void *)stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            return false;
        }
        stack->items = items;
        stack->capacity = capacity;
    }

    stack->items[stack->count] = expression;
    ++stack->count;
    return true;
}

static void scalar_arithmetic_node_stack_deinit(struct scalar_arithmetic_node_stack *stack) {
    if (stack == NULL) {
        return;
    }

    free((void *)stack->items);
    *stack = (struct scalar_arithmetic_node_stack){0};
}

static bool checked_int64_add(int64_t left, int64_t right, int64_t *out_result) {
    if ((right > 0 && left > INT64_MAX - right) || (right < 0 && left < INT64_MIN - right)) {
        return true;
    }

    *out_result = left + right;
    return false;
}

static bool checked_int64_subtract(int64_t left, int64_t right, int64_t *out_result) {
    if ((right < 0 && left > INT64_MAX + right) || (right > 0 && left < INT64_MIN + right)) {
        return true;
    }

    *out_result = left - right;
    return false;
}

static bool checked_int64_multiply(int64_t left, int64_t right, int64_t *out_result) {
    if (left == 0 || right == 0) {
        *out_result = 0;
        return false;
    }
    if ((left == INT64_MIN && right == -1) || (right == INT64_MIN && left == -1)) {
        return true;
    }
    if (left > 0) {
        if (right > 0) {
            if (left > INT64_MAX / right) {
                return true;
            }
        } else if (right < INT64_MIN / left) {
            return true;
        }
    } else if (right > 0) {
        if (left < INT64_MIN / right) {
            return true;
        }
    } else if (left < INT64_MAX / right) {
        return true;
    }

    *out_result = left * right;
    return false;
}

static bool checked_int64_modulo(int64_t left, int64_t right, int64_t *out_result) {
    if (left == INT64_MIN && right == -1) {
        *out_result = 0;
        return false;
    }

    *out_result = left % right;
    return false;
}

static bool checked_int64_divide(int64_t left, int64_t right, int64_t *out_result) {
    if (left == INT64_MIN && (right == 1 || right == -1)) {
        return true;
    }

    *out_result = left / right;
    return false;
}

static bool checked_int64_negate(int64_t value, int64_t *out_result) {
    if (value == INT64_MIN) {
        return true;
    }

    *out_result = -value;
    return false;
}

static void set_scalar_arithmetic_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SELECT scalar arithmetic projection supports only signed 64-bit integer, boolean, "
        "NULL, and nested IF()/IFNULL()/COALESCE()/NULLIF()/ISNULL() operands with +, "
        "binary -, and * arithmetic plus unary +, unary -, %, infix MOD, MOD(), and infix DIV"
    );
}

static void set_scalar_division_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SELECT scalar division projection supports only top-level / over signed 64-bit scalar "
        "arithmetic operands without nested / division"
    );
}

static void set_scalar_arithmetic_operand_out_of_range_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SELECT scalar arithmetic projection supports only signed 64-bit integer operands"
    );
}

static void set_scalar_arithmetic_overflow_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_bigint_out_of_range,
        "22003",
        "BIGINT value is out of range in scalar arithmetic expression"
    );
}

static void set_scalar_bitwise_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SELECT scalar bitwise projection supports only top-level numeric bitwise ~, &, |, ^, "
        "<<, and >> over signed 64-bit integer, boolean, NULL, and nested "
        "IF()/IFNULL()/COALESCE()/NULLIF()/ISNULL() scalar arithmetic operands"
    );
}

static void set_abs_signed_minimum_overflow_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_bigint_out_of_range,
        "22003",
        "BIGINT value is out of range in 'abs(-(9223372036854775808))'"
    );
}

static void set_abs_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "ABS() supports only top-level no-source SELECT, FROM DUAL SELECT, and DO numeric "
        "ABS() over integer, boolean, NULL, signed 64-bit scalar arithmetic, and limited "
        "numeric bitwise operands"
    );
}

static void set_sign_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SIGN() supports only top-level no-source SELECT, FROM DUAL SELECT, and DO numeric "
        "SIGN() over integer, boolean, NULL, direct decimal integer literals, signed 64-bit "
        "scalar arithmetic, and limited numeric bitwise operands"
    );
}

static void set_rounding_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "CEIL()/CEILING()/FLOOR()/ROUND() support only top-level integer-domain scalar "
        "use; ROUND(value, places) supports only signed 64-bit negative-place rounding"
    );
}

static int set_rounding_signed_overflow_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_bigint_out_of_range,
        "22003",
        "BIGINT value is out of range in 'round()'"
    );
    return MYLITE_ERROR;
}

static void set_sqrt_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SQRT() supports only top-level no-source SELECT, FROM DUAL SELECT, and DO numeric "
        "SQRT() over integer, boolean, NULL, signed 64-bit scalar arithmetic, and limited "
        "numeric bitwise operands"
    );
}

static void set_angle_conversion_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "DEGREES()/RADIANS() support only top-level no-source SELECT, FROM DUAL SELECT, "
        "and DO numeric conversion over integer, boolean, NULL, signed 64-bit scalar "
        "arithmetic, and limited numeric bitwise operands"
    );
}

static void set_inverse_trig_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "ACOS()/ASIN() support only top-level no-source SELECT, FROM DUAL SELECT, and DO "
        "inverse trigonometry over integer, boolean, NULL, signed 64-bit scalar arithmetic, "
        "and limited numeric bitwise operands"
    );
}

static void set_direct_trig_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SIN()/COS()/TAN()/COT() support only top-level no-source SELECT, FROM DUAL SELECT, "
        "and DO trigonometry over integer, boolean, NULL, signed 64-bit scalar arithmetic, "
        "and limited numeric bitwise operands"
    );
}

static void set_atan_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "ATAN()/ATAN2() support only top-level no-source SELECT, FROM DUAL SELECT, and DO "
        "arc tangent over integer, boolean, NULL, signed 64-bit scalar arithmetic, and limited "
        "numeric bitwise operands"
    );
}

static void set_exp_log_power_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "EXP()/LN()/LOG()/LOG10()/LOG2()/POW()/POWER() support only top-level no-source "
        "SELECT, FROM DUAL SELECT, and DO use over integer, boolean, NULL, signed 64-bit "
        "scalar arithmetic, and limited numeric bitwise operands"
    );
}

static void set_base_conversion_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "BIN()/OCT() support only top-level no-source SELECT, FROM DUAL SELECT, and DO "
        "numeric conversion over integer, boolean, NULL, signed 64-bit scalar arithmetic, "
        "and limited numeric bitwise operands; CONV() support is limited to the same "
        "top-level scalar contexts, with signed 64-bit scalar arithmetic base arguments"
    );
}

static void set_bit_count_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "BIT_COUNT() supports only top-level no-source SELECT, FROM DUAL SELECT, and DO "
        "numeric BIT_COUNT() over integer, boolean, NULL, signed 64-bit scalar "
        "arithmetic, and limited numeric bitwise operands"
    );
}

static void set_crc32_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "CRC32() supports only top-level no-source SELECT, FROM DUAL SELECT, and DO "
        "CRC32() over string, hex, integer, boolean, and NULL literals"
    );
}

static void set_hex_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "HEX() supports only top-level no-source SELECT, FROM DUAL SELECT, DO, and "
        "single-table row-scalar SELECT over string, hex, integer, boolean, NULL, supported "
        "session scalar, binary cast/convert, and supported descriptor column arguments"
    );
}

static void set_format_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "FORMAT() supports only top-level no-source SELECT, FROM DUAL SELECT, and DO "
        "FORMAT(value, decimals) over exact integer, fixed decimal, boolean, and NULL "
        "literal operands"
    );
}

static void set_truncate_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "TRUNCATE() supports only top-level no-source SELECT, FROM DUAL SELECT, and DO "
        "TRUNCATE(value, decimals) over exact integer, fixed decimal, boolean, and NULL "
        "literal operands"
    );
}

static void set_scalar_logical_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SELECT scalar logical projection supports only signed 64-bit integer, boolean, "
        "NULL, and nested IF()/IFNULL()/COALESCE()/NULLIF()/ISNULL() operands with signed "
        "64-bit +, binary -, *, %, infix MOD, MOD(), infix DIV, comparison operators "
        "=, <=>, <>, !=, <, <=, >, >=, keyword logical NOT, AND, XOR, and OR, and "
        "scalar IS NULL, IS TRUE, IS FALSE, and IS UNKNOWN"
    );
}

static void set_scalar_comparison_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SELECT scalar comparison projection supports only signed 64-bit integer, boolean, "
        "NULL, and nested IF()/IFNULL()/COALESCE()/NULLIF()/ISNULL() operands with signed "
        "64-bit +, binary -, *, %, infix MOD, MOD(), infix DIV, and comparison operators "
        "=, <=>, <>, !=, <, <=, >, and >="
    );
}
