
static int evaluate_scalar_comparison_numeric_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
);
static int evaluate_scalar_comparison_like_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
);
static int evaluate_scalar_comparison_numeric_frame(
    struct mylite_db *database,
    struct scalar_comparison_numeric_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_comparison_numeric_eval_frame *frame
);
static int evaluate_scalar_comparison_numeric_apply_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_comparison_numeric_apply_unary_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_comparison_numeric_enter_frame(
    struct mylite_db *database,
    struct scalar_comparison_numeric_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int evaluate_scalar_comparison_numeric_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct scalar_arithmetic_value *out_value
);
static int evaluate_scalar_comparison_numeric_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct scalar_arithmetic_value *out_value
);
static int parse_scalar_comparison_complete_numeric_text(
    struct mylite_db *database,
    const char *text,
    struct scalar_arithmetic_value *out_value
);
static int parse_scalar_comparison_string_numeric_text(
    struct mylite_db *database,
    const char *text,
    struct scalar_arithmetic_value *out_value
);
static int apply_scalar_comparison_numeric_operator(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const struct scalar_arithmetic_value *left,
    const struct scalar_arithmetic_value *right,
    struct scalar_arithmetic_value *out_value
);
static bool scalar_comparison_values_are_equal(
    const struct scalar_arithmetic_value *left,
    const struct scalar_arithmetic_value *right
);
static bool scalar_comparison_numeric_result_is_true(
    enum mylite_sql_ast_operator operator_kind,
    const struct scalar_arithmetic_value *left,
    const struct scalar_arithmetic_value *right
);
static double scalar_comparison_numeric_real(const struct scalar_arithmetic_value *value);
static int scalar_comparison_numeric_eval_stack_push(
    struct mylite_db *database,
    struct scalar_comparison_numeric_eval_stack *stack,
    enum scalar_comparison_numeric_eval_frame_kind kind,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
);
static void scalar_comparison_numeric_eval_stack_deinit(
    struct scalar_comparison_numeric_eval_stack *stack
);

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
        result.integer = scalar_comparison_values_are_equal(&left, &right) ? 1 : 0;
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

    if (scalar_comparison_numeric_result_is_true(operator_kind, &left, &right)) {
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
        rc = evaluate_scalar_comparison_numeric_expression(database, expression, &value);
        if (rc == MYLITE_OK) {
            rc = scalar_arithmetic_value_stack_push(database, value_stack, value);
        }
        return rc;
    }

    operator_kind = mylite_sql_ast_node_operator(expression);
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
        rc = evaluate_scalar_comparison_like_expression(database, expression, &value);
        if (rc == MYLITE_OK) {
            rc = scalar_arithmetic_value_stack_push(database, value_stack, value);
        }
        return rc;
    }
    if (!is_scalar_comparison_operator(operator_kind)) {
        rc = evaluate_scalar_comparison_numeric_expression(database, expression, &value);
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

static int evaluate_scalar_comparison_like_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
) {
    static const struct scalar_text_conversion_messages value_messages = {
        .unsupported =
            "SELECT scalar LIKE supports only string, integer, boolean, and NULL values",
        .signed_value = "SELECT scalar LIKE supports only signed integer values",
        .string_unsupported = "SELECT scalar LIKE supports only string value literals",
        .embedded_nul = "SELECT scalar LIKE value literals do not support NUL bytes",
    };
    static const struct scalar_text_conversion_messages pattern_messages = {
        .unsupported =
            "SELECT scalar LIKE supports only string, integer, boolean, and NULL patterns",
        .signed_value = "SELECT scalar LIKE supports only signed integer patterns",
        .string_unsupported = "SELECT scalar LIKE supports only string pattern literals",
        .embedded_nul = "SELECT scalar LIKE pattern literals do not support NUL bytes",
    };
    struct session_scalar_cell value = {0};
    struct session_scalar_cell pattern = {0};
    bool matches = false;
    int rc = MYLITE_OK;

    if (expression == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){.is_null = false, .integer = 0};
    if (expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        mylite_sql_ast_node_operator(expression) != MYLITE_SQL_AST_OPERATOR_LIKE ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_scalar_comparison_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = scalar_text_conversion_input_value(
        database,
        child_at(expression, 0U),
        &value_messages,
        &value
    );
    if (rc == MYLITE_OK) {
        rc = scalar_text_conversion_input_value(
            database,
            child_at(expression, 1U),
            &pattern_messages,
            &pattern
        );
    }
    if (rc == MYLITE_OK && (value.value == NULL || pattern.value == NULL)) {
        out_value->is_null = true;
    } else if (rc == MYLITE_OK) {
        matches = show_like_pattern_matches_with_escape(
            pattern.value,
            scalar_cell_text_length(&pattern),
            value.value,
            scalar_cell_text_length(&value),
            false,
            !session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES)
        );
        out_value->integer = matches ? 1 : 0;
    }

    session_scalar_cell_deinit(&pattern);
    session_scalar_cell_deinit(&value);
    return rc;
}

static int evaluate_scalar_comparison_numeric_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
) {
    struct scalar_comparison_numeric_eval_stack expression_stack = {0};
    struct scalar_arithmetic_value_stack value_stack = {0};
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){.is_null = false, .integer = 0};
    rc = scalar_comparison_numeric_eval_stack_push(
        database,
        &expression_stack,
        SCALAR_COMPARISON_NUMERIC_EVAL_ENTER,
        expression,
        MYLITE_SQL_AST_OPERATOR_NONE
    );
    while (rc == MYLITE_OK && expression_stack.count != 0U) {
        struct scalar_comparison_numeric_eval_frame frame =
            expression_stack.items[--expression_stack.count];

        rc = evaluate_scalar_comparison_numeric_frame(
            database,
            &expression_stack,
            &value_stack,
            &frame
        );
    }
    if (rc == MYLITE_OK) {
        rc = finish_scalar_arithmetic_result(database, &value_stack, out_value);
    }
    scalar_comparison_numeric_eval_stack_deinit(&expression_stack);
    scalar_arithmetic_value_stack_deinit(&value_stack);
    return rc;
}

static int evaluate_scalar_comparison_numeric_frame(
    struct mylite_db *database,
    struct scalar_comparison_numeric_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_comparison_numeric_eval_frame *frame
) {
    if (frame == NULL) {
        return MYLITE_MISUSE;
    }
    if (frame->kind == SCALAR_COMPARISON_NUMERIC_EVAL_APPLY) {
        return evaluate_scalar_comparison_numeric_apply_frame(
            database,
            value_stack,
            frame->operator_kind
        );
    }
    if (frame->kind == SCALAR_COMPARISON_NUMERIC_EVAL_APPLY_UNARY) {
        return evaluate_scalar_comparison_numeric_apply_unary_frame(
            database,
            value_stack,
            frame->operator_kind
        );
    }
    return evaluate_scalar_comparison_numeric_enter_frame(
        database,
        expression_stack,
        value_stack,
        frame->expression
    );
}

static int evaluate_scalar_comparison_numeric_apply_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
) {
    struct scalar_arithmetic_value left = {.is_null = false, .integer = 0};
    struct scalar_arithmetic_value right = {.is_null = false, .integer = 0};
    struct scalar_arithmetic_value result = {.is_null = false, .integer = 0};
    int rc = MYLITE_OK;

    if (!scalar_arithmetic_value_stack_pop(value_stack, &right) ||
        !scalar_arithmetic_value_stack_pop(value_stack, &left)) {
        set_runtime_error(database, "invalid scalar comparison numeric evaluation stack");
        return MYLITE_ERROR;
    }
    rc = apply_scalar_comparison_numeric_operator(
        database,
        operator_kind,
        &left,
        &right,
        &result
    );
    if (rc == MYLITE_OK) {
        rc = scalar_arithmetic_value_stack_push(database, value_stack, result);
    }
    return rc;
}

static int evaluate_scalar_comparison_numeric_apply_unary_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
) {
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    int64_t integer = 0;

    if (!scalar_arithmetic_value_stack_pop(value_stack, &value)) {
        set_runtime_error(database, "invalid scalar comparison numeric evaluation stack");
        return MYLITE_ERROR;
    }
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        set_scalar_comparison_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (value.is_null || operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
        return scalar_arithmetic_value_stack_push(database, value_stack, value);
    }
    if (value.has_numeric_real) {
        value.numeric_real = -value.numeric_real;
        return scalar_arithmetic_value_stack_push(database, value_stack, value);
    }
    if (checked_int64_negate(value.integer, &integer)) {
        set_scalar_arithmetic_overflow_error(database);
        return MYLITE_ERROR;
    }
    value.integer = integer;
    return scalar_arithmetic_value_stack_push(database, value_stack, value);
}

static int evaluate_scalar_comparison_numeric_enter_frame(
    struct mylite_db *database,
    struct scalar_comparison_numeric_eval_stack *expression_stack,
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
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        operator_kind = mylite_sql_ast_node_operator(expression);
        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            set_scalar_comparison_unsupported_error(database);
            return MYLITE_ERROR;
        }
        rc = scalar_comparison_numeric_eval_stack_push(
            database,
            expression_stack,
            SCALAR_COMPARISON_NUMERIC_EVAL_APPLY_UNARY,
            NULL,
            operator_kind
        );
        if (rc == MYLITE_OK) {
            rc = scalar_comparison_numeric_eval_stack_push(
                database,
                expression_stack,
                SCALAR_COMPARISON_NUMERIC_EVAL_ENTER,
                child_at(expression, 0U),
                MYLITE_SQL_AST_OPERATOR_NONE
            );
        }
        return rc;
    }
    if (expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION) {
        operator_kind = mylite_sql_ast_node_operator(expression);
        if (!is_scalar_arithmetic_operator(operator_kind)) {
            rc = evaluate_scalar_arithmetic_expression(database, expression, &value);
            if (rc == MYLITE_OK) {
                rc = scalar_arithmetic_value_stack_push(database, value_stack, value);
            }
            return rc;
        }
        rc = scalar_comparison_numeric_eval_stack_push(
            database,
            expression_stack,
            SCALAR_COMPARISON_NUMERIC_EVAL_APPLY,
            NULL,
            operator_kind
        );
        if (rc == MYLITE_OK) {
            rc = scalar_comparison_numeric_eval_stack_push(
                database,
                expression_stack,
                SCALAR_COMPARISON_NUMERIC_EVAL_ENTER,
                child_at(expression, 1U),
                MYLITE_SQL_AST_OPERATOR_NONE
            );
        }
        if (rc == MYLITE_OK) {
            rc = scalar_comparison_numeric_eval_stack_push(
                database,
                expression_stack,
                SCALAR_COMPARISON_NUMERIC_EVAL_ENTER,
                child_at(expression, 0U),
                MYLITE_SQL_AST_OPERATOR_NONE
            );
        }
        return rc;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        rc = evaluate_scalar_comparison_numeric_literal(database, expression, &value);
    } else {
        rc = evaluate_scalar_arithmetic_expression(database, expression, &value);
    }
    if (rc == MYLITE_OK) {
        rc = scalar_arithmetic_value_stack_push(database, value_stack, value);
    }
    return rc;
}

static int evaluate_scalar_comparison_numeric_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct scalar_arithmetic_value *out_value
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *text = NULL;
    int rc = MYLITE_OK;

    if (literal == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        return evaluate_scalar_comparison_numeric_string_literal(database, literal, out_value);
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_DECIMAL &&
        literal_kind != MYLITE_SQL_AST_LITERAL_FLOAT) {
        return evaluate_scalar_arithmetic_expression(database, literal, out_value);
    }

    rc = copy_source_span_text(database, &literal->span, &text);
    if (rc == MYLITE_OK) {
        rc = parse_scalar_comparison_complete_numeric_text(database, text, out_value);
    }
    free(text);
    return rc;
}

static int evaluate_scalar_comparison_numeric_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct scalar_arithmetic_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = decode_sql_string_literal_with_policy(
        database,
        literal,
        "SELECT scalar comparison supports only ordinary string literals",
        "SELECT scalar comparison does not support NUL bytes in string literals",
        false,
        &text,
        &text_length
    );

    (void)text_length;
    if (rc == MYLITE_OK) {
        rc = parse_scalar_comparison_string_numeric_text(database, text, out_value);
    }
    free(text);
    return rc;
}

static int parse_scalar_comparison_complete_numeric_text(
    struct mylite_db *database,
    const char *text,
    struct scalar_arithmetic_value *out_value
) {
    char *end = NULL;
    double parsed = 0.0;

    if (text == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    errno = 0;
    if (mylite_execution_parse_c_locale_double(text, &end, &parsed) != MYLITE_OK ||
        errno != 0 || end == text || end == NULL || *end != '\0') {
        set_scalar_comparison_unsupported_error(database);
        return MYLITE_ERROR;
    }
    out_value->has_numeric_real = true;
    out_value->numeric_real = parsed;
    return MYLITE_OK;
}

static int parse_scalar_comparison_string_numeric_text(
    struct mylite_db *database,
    const char *text,
    struct scalar_arithmetic_value *out_value
) {
    char *end = NULL;
    double parsed = 0.0;

    if (text == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    errno = 0;
    if (mylite_execution_parse_c_locale_double(text, &end, &parsed) != MYLITE_OK ||
        errno != 0) {
        set_scalar_comparison_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (end == text || end == NULL) {
        parsed = 0.0;
    }
    out_value->has_numeric_real = true;
    out_value->numeric_real = parsed;
    return MYLITE_OK;
}

static int apply_scalar_comparison_numeric_operator(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const struct scalar_arithmetic_value *left,
    const struct scalar_arithmetic_value *right,
    struct scalar_arithmetic_value *out_value
) {
    struct scalar_arithmetic_operation operation = {
        .operator_kind = operator_kind,
        .left = 0,
        .right = 0,
    };

    if (left == NULL || right == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){.is_null = false, .integer = 0};
    if (left->division_by_zero_warning_count > SIZE_MAX - right->division_by_zero_warning_count) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_value->division_by_zero_warning_count =
        left->division_by_zero_warning_count + right->division_by_zero_warning_count;
    if (left->is_null || right->is_null) {
        out_value->is_null = true;
        return MYLITE_OK;
    }
    if (!left->has_numeric_real && !right->has_numeric_real &&
        (operator_kind == MYLITE_SQL_AST_OPERATOR_MODULO ||
         operator_kind == MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE) &&
        right->integer == 0) {
        out_value->is_null = true;
        if (out_value->division_by_zero_warning_count == SIZE_MAX) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        ++out_value->division_by_zero_warning_count;
        return MYLITE_OK;
    }
    if (left->has_numeric_real || right->has_numeric_real) {
        double left_value = scalar_comparison_numeric_real(left);
        double right_value = scalar_comparison_numeric_real(right);

        switch (operator_kind) {
        case MYLITE_SQL_AST_OPERATOR_ADD:
            out_value->numeric_real = left_value + right_value;
            break;
        case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
            out_value->numeric_real = left_value - right_value;
            break;
        case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
            out_value->numeric_real = left_value * right_value;
            break;
        default:
            set_scalar_comparison_unsupported_error(database);
            return MYLITE_ERROR;
        }
        out_value->has_numeric_real = true;
        return MYLITE_OK;
    }

    operation.left = left->integer;
    operation.right = right->integer;
    return apply_scalar_arithmetic_operator(database, &operation, &out_value->integer);
}

static bool scalar_comparison_values_are_equal(
    const struct scalar_arithmetic_value *left,
    const struct scalar_arithmetic_value *right
) {
    if (left == NULL || right == NULL) {
        return false;
    }
    if (left->has_numeric_real || right->has_numeric_real) {
        return scalar_comparison_numeric_real(left) == scalar_comparison_numeric_real(right);
    }
    return left->integer == right->integer;
}

static bool scalar_comparison_numeric_result_is_true(
    enum mylite_sql_ast_operator operator_kind,
    const struct scalar_arithmetic_value *left,
    const struct scalar_arithmetic_value *right
) {
    double left_value = 0.0;
    double right_value = 0.0;

    if (left == NULL || right == NULL) {
        return false;
    }
    if (!left->has_numeric_real && !right->has_numeric_real) {
        struct scalar_comparison_operation operation = {
            .operator_kind = operator_kind,
            .left = left->integer,
            .right = right->integer,
        };

        return scalar_comparison_result_is_true(&operation);
    }

    left_value = scalar_comparison_numeric_real(left);
    right_value = scalar_comparison_numeric_real(right);
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return left_value == right_value;
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return left_value != right_value;
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return left_value < right_value;
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return left_value <= right_value;
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return left_value > right_value;
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return left_value >= right_value;
    default:
        return false;
    }
}

static double scalar_comparison_numeric_real(const struct scalar_arithmetic_value *value) {
    if (value == NULL) {
        return 0.0;
    }
    return value->has_numeric_real ? value->numeric_real : (double)value->integer;
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

static int scalar_comparison_numeric_eval_stack_push(
    struct mylite_db *database,
    struct scalar_comparison_numeric_eval_stack *stack,
    enum scalar_comparison_numeric_eval_frame_kind kind,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
) {
    struct scalar_comparison_numeric_eval_frame *items = NULL;
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
        items = (struct scalar_comparison_numeric_eval_frame *)
            realloc((void *)stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        stack->items = items;
        stack->capacity = capacity;
    }

    stack->items[stack->count] = (struct scalar_comparison_numeric_eval_frame){
        .kind = kind,
        .expression = expression,
        .operator_kind = operator_kind,
    };
    ++stack->count;
    return MYLITE_OK;
}

static void scalar_comparison_numeric_eval_stack_deinit(
    struct scalar_comparison_numeric_eval_stack *stack
) {
    if (stack == NULL) {
        return;
    }

    free((void *)stack->items);
    *stack = (struct scalar_comparison_numeric_eval_stack){0};
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
