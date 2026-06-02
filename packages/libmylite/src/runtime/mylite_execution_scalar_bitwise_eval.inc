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
