static int convert_column_default_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct planned_column *column,
    int64_t *out_value
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct integer_column_range range = {0};
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = parse_column_default_integer_magnitude(
        database,
        value_node,
        column,
        &magnitude,
        &is_negative
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    planned_column_descriptor_for_default(column, &descriptor);
    rc = integer_range_for_column(
        database,
        &descriptor,
        "DEFAULT supports only baseline integer columns",
        &range
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return finish_column_default_integer_value(
        database,
        column,
        &range,
        magnitude,
        is_negative,
        out_value
    );
}

static int parse_column_default_integer_magnitude(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct planned_column *column,
    uint64_t *out_magnitude,
    bool *out_is_negative
) {
    const struct mylite_sql_ast_node *literal = value_node;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool has_unary = false;
    int rc = MYLITE_OK;

    if (value_node == NULL || column == NULL || out_magnitude == NULL || out_is_negative == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    *out_magnitude = 0U;
    *out_is_negative = false;
    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        has_unary = true;
        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            *out_is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_invalid_default_error(database, column->name);
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (boolean_literal_magnitude(literal, out_magnitude)) {
        return MYLITE_OK;
    }

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        rc = parse_unsigned_integer_literal(&literal->span, out_magnitude);
    } else if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING && !has_unary) {
        rc = parse_column_default_integer_string(database, literal, out_magnitude, out_is_negative);
    } else {
        rc = MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        if (rc != MYLITE_NOMEM) {
            set_invalid_default_error(database, column->name);
        }
        return rc;
    }

    return MYLITE_OK;
}

static int finish_column_default_integer_value(
    struct mylite_db *database,
    const struct planned_column *column,
    const struct integer_column_range *range,
    uint64_t magnitude,
    bool is_negative,
    int64_t *out_value
) {
    const uint64_t bigint_signed_negative_abs_max = 9223372036854775808ULL;

    if (column == NULL || range == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (is_negative) {
        if ((range->negative_abs_max == 0U && magnitude != 0U) ||
            magnitude > range->negative_abs_max) {
            set_invalid_default_error(database, column->name);
            return MYLITE_ERROR;
        }
        *out_value = magnitude == bigint_signed_negative_abs_max ? INT64_MIN : -(int64_t)magnitude;
        return MYLITE_OK;
    }
    if (magnitude > range->positive_max) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    *out_value = (int64_t)magnitude;
    return MYLITE_OK;
}

static int parse_column_default_integer_string(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_magnitude,
    bool *out_is_negative
) {
    char *text = NULL;
    size_t text_length = 0U;
    enum mylite_execution_dml_numeric_parse_result parse_result =
        MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
    int rc = decode_sql_string_literal(
        database,
        literal,
        "DEFAULT supports only baseline integer literals and exact quoted integer strings",
        "invalid integer default string literal",
        &text,
        &text_length
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    parse_result = mylite_execution_parse_signed_integer_text(
        text,
        text_length,
        out_magnitude,
        out_is_negative
    );
    if (parse_result != MYLITE_EXECUTION_DML_NUMERIC_PARSE_OK) {
        rc = MYLITE_ERROR;
    }

    free(text);
    return rc;
}

static int evaluate_integer_default_expression(
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

        rc = evaluate_integer_default_frame(database, &expression_stack, &value_stack, &frame);
    }
    if (rc == MYLITE_OK) {
        rc = finish_integer_default_result(&value_stack, out_value);
    }
    scalar_arithmetic_eval_stack_deinit(&expression_stack);
    scalar_arithmetic_value_stack_deinit(&value_stack);
    return rc;
}

static int evaluate_integer_default_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_arithmetic_eval_frame *frame
) {
    if (frame == NULL) {
        return MYLITE_MISUSE;
    }
    if (frame->kind == SCALAR_ARITHMETIC_EVAL_APPLY) {
        return evaluate_integer_default_apply_frame(database, value_stack, frame->operator_kind);
    }
    if (frame->kind == SCALAR_ARITHMETIC_EVAL_APPLY_UNARY) {
        return evaluate_integer_default_apply_unary_frame(
            database,
            value_stack,
            frame->operator_kind
        );
    }
    return evaluate_integer_default_enter_frame(
        database,
        expression_stack,
        value_stack,
        frame->expression
    );
}

static int evaluate_integer_default_apply_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
) {
    struct scalar_arithmetic_value left = {.is_null = false, .integer = 0};
    struct scalar_arithmetic_value right = {.is_null = false, .integer = 0};
    struct scalar_arithmetic_value result = {.is_null = false, .integer = 0};
    int64_t integer_result = 0;
    bool overflow = false;

    if (!scalar_arithmetic_value_stack_pop(value_stack, &right) ||
        !scalar_arithmetic_value_stack_pop(value_stack, &left)) {
        return MYLITE_ERROR;
    }
    if (left.is_null || right.is_null) {
        result.is_null = true;
        return scalar_arithmetic_value_stack_push(database, value_stack, result);
    }

    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
        overflow = checked_int64_add(left.integer, right.integer, &integer_result);
        break;
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        overflow = checked_int64_subtract(left.integer, right.integer, &integer_result);
        break;
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        overflow = checked_int64_multiply(left.integer, right.integer, &integer_result);
        break;
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        if (right.integer == 0 || (left.integer == INT64_MIN && right.integer == -1)) {
            return MYLITE_ERROR;
        }
        integer_result = left.integer % right.integer;
        break;
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
        if (right.integer == 0 || (left.integer == INT64_MIN && right.integer == -1)) {
            return MYLITE_ERROR;
        }
        integer_result = left.integer / right.integer;
        break;
    default:
        return MYLITE_ERROR;
    }
    if (overflow) {
        return MYLITE_ERROR;
    }

    result.integer = integer_result;
    return scalar_arithmetic_value_stack_push(database, value_stack, result);
}

static int evaluate_integer_default_apply_unary_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
) {
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    int64_t result = 0;

    if (!scalar_arithmetic_value_stack_pop(value_stack, &value)) {
        return MYLITE_ERROR;
    }
    if (value.is_null || operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
        return scalar_arithmetic_value_stack_push(database, value_stack, value);
    }
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE ||
        checked_int64_negate(value.integer, &result)) {
        return MYLITE_ERROR;
    }

    value.integer = result;
    return scalar_arithmetic_value_stack_push(database, value_stack, value);
}

static int evaluate_integer_default_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return evaluate_integer_default_literal_enter_frame(database, value_stack, expression);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return evaluate_integer_default_unary_enter_frame(
            database,
            expression_stack,
            value_stack,
            expression
        );
    case MYLITE_SQL_AST_MOD_FUNCTION:
        return evaluate_integer_default_mod_enter_frame(database, expression_stack, expression);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return evaluate_integer_default_binary_enter_frame(database, expression_stack, expression);
    default:
        return MYLITE_ERROR;
    }
}

static int evaluate_integer_default_literal_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
) {
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    int rc = parse_integer_default_literal_value(expression, false, &value);

    if (rc == MYLITE_OK) {
        rc = scalar_arithmetic_value_stack_push(database, value_stack, value);
    }
    return rc;
}

static int evaluate_integer_default_unary_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
    const struct mylite_sql_ast_node *operand =
        unwrap_parenthesized_expression(child_at(expression, 0U));
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    int rc = MYLITE_OK;

    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        return MYLITE_ERROR;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE && operand != NULL &&
        operand->kind == MYLITE_SQL_AST_LITERAL) {
        rc = parse_integer_default_literal_value(operand, true, &value);
        if (rc == MYLITE_OK) {
            rc = scalar_arithmetic_value_stack_push(database, value_stack, value);
        }
        return rc;
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
            operand,
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    return rc;
}

static int evaluate_integer_default_mod_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    const struct mylite_sql_ast_node *expression
) {
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_child_count(expression) != 2U) {
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

static int evaluate_integer_default_binary_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
    int rc = MYLITE_OK;

    if (!integer_default_binary_operator_is_supported(operator_kind)) {
        return MYLITE_ERROR;
    }
    rc = scalar_arithmetic_eval_stack_push(
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

static bool integer_default_binary_operator_is_supported(
    enum mylite_sql_ast_operator operator_kind
) {
    return (operator_kind == MYLITE_SQL_AST_OPERATOR_ADD ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_SUBTRACT ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_MULTIPLY ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_MODULO ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE) != 0;
}

static int finish_integer_default_result(
    struct scalar_arithmetic_value_stack *value_stack,
    struct scalar_arithmetic_value *out_value
) {
    if (!scalar_arithmetic_value_stack_pop(value_stack, out_value)) {
        return MYLITE_ERROR;
    }
    if (value_stack->count != 0U) {
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int parse_integer_default_literal_value(
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    struct scalar_arithmetic_value *out_value
) {
    const uint64_t int64_min_magnitude = 9223372036854775808ULL;
    uint64_t limit = (uint64_t)INT64_MAX;
    uint64_t magnitude = 0U;

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_value->is_null = true;
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return MYLITE_ERROR;
    }
    if (is_negative) {
        limit = int64_min_magnitude;
    }
    if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        magnitude > limit) {
        return MYLITE_ERROR;
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

static bool column_default_value_is_parenthesized_expression(
    const struct mylite_sql_ast_node *default_node
) {
    const struct mylite_sql_ast_node *value_node = child_at(default_node, 0U);

    return (value_node != NULL && value_node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) != 0;
}

static bool column_default_value_is_parenthesized_text_expression(
    const struct mylite_sql_ast_node *default_node
) {
    const struct mylite_sql_ast_node *value_node = child_at(default_node, 0U);
    const struct mylite_sql_ast_node *expression = unwrap_parenthesized_expression(value_node);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_INTEGER;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION ||
        expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    return (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
}

static bool column_default_value_is_parenthesized_binary_blob_expression(
    const struct mylite_sql_ast_node *default_node
) {
    const struct mylite_sql_ast_node *value_node = child_at(default_node, 0U);
    const struct mylite_sql_ast_node *expression = unwrap_parenthesized_expression(value_node);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_INTEGER;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION ||
        expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    return (literal_kind == MYLITE_SQL_AST_LITERAL_HEX ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
}
