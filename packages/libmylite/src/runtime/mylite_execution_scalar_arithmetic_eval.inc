
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
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT) {
        value.integer = scalar_arithmetic_truth_value(&value) ? 0 : 1;
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
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT) {
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
    enum mylite_sys_function_kind sys_function_kind = MYLITE_SYS_FUNCTION_NONE;
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
    if (expression->kind == MYLITE_SQL_AST_SYSTEM_VARIABLE) {
        rc = system_variable_value(database, expression, &cell);
        if (rc == MYLITE_OK) {
            rc = parse_scalar_arithmetic_operand(database, &cell, out_value);
        }
        return rc;
    }
    if (generic_sys_function_kind(expression, &sys_function_kind)) {
        rc = generic_sys_function_value(database, expression, sys_function_kind, &cell);
        if (rc == MYLITE_OK) {
            rc = parse_scalar_arithmetic_operand(database, &cell, out_value);
        }
        session_scalar_cell_deinit(&cell);
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
