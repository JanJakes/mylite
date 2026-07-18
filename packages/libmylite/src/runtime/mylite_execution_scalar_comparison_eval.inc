
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
static int evaluate_scalar_comparison_sounds_like_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
);
static int evaluate_scalar_comparison_regexp_expression(
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
static int row_constructor_comparison_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int row_constructor_not_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int row_constructor_collect_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *constructor,
    struct row_constructor_scalar_values *out_values
);
static int row_constructor_collect_item_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct row_constructor_scalar_item *out_item
);
static int row_constructor_compare_truth(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const struct row_constructor_scalar_values *left,
    const struct row_constructor_scalar_values *right,
    enum row_constructor_scalar_truth *out_truth
);
static int row_constructor_equal_truth(
    struct mylite_db *database,
    const struct row_constructor_scalar_values *left,
    const struct row_constructor_scalar_values *right,
    enum row_constructor_scalar_truth *out_truth
);
static int row_constructor_null_safe_equal_truth(
    struct mylite_db *database,
    const struct row_constructor_scalar_values *left,
    const struct row_constructor_scalar_values *right,
    enum row_constructor_scalar_truth *out_truth
);
static int row_constructor_order_truth(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const struct row_constructor_scalar_values *left,
    const struct row_constructor_scalar_values *right,
    enum row_constructor_scalar_truth *out_truth
);
static int row_constructor_compare_items(
    struct mylite_db *database,
    const struct row_constructor_scalar_item *left,
    const struct row_constructor_scalar_item *right,
    bool *out_unknown,
    int *out_comparison
);
static int row_constructor_compare_numeric_items(
    struct mylite_db *database,
    const struct row_constructor_scalar_item *left,
    const struct row_constructor_scalar_item *right,
    bool *out_unknown,
    int *out_comparison
);
static int row_constructor_numeric_item_value(
    struct mylite_db *database,
    const struct row_constructor_scalar_item *item,
    struct scalar_arithmetic_value *out_value
);
static int row_constructor_compare_text_items(
    const struct row_constructor_scalar_item *left,
    const struct row_constructor_scalar_item *right
);
static bool row_constructor_item_is_null(const struct row_constructor_scalar_item *item);
static int row_constructor_set_scalar_result(
    struct mylite_db *database,
    const struct row_constructor_scalar_result *result,
    struct session_scalar_cell *out_cell
);
static bool row_constructor_expression_is_constructor(const struct mylite_sql_ast_node *expression);
static bool row_constructor_expression_uses_text_comparison(
    const struct mylite_sql_ast_node *expression
);
static bool row_constructor_comparison_operator_is_supported(
    enum mylite_sql_ast_operator operator_kind
);
static size_t row_constructor_warning_count(const struct row_constructor_scalar_values *values);
static void row_constructor_values_deinit(struct row_constructor_scalar_values *values);
static void set_row_constructor_operand_count_error(
    struct mylite_db *database,
    size_t expected_column_count
);
static void set_row_constructor_unsupported_error(struct mylite_db *database);
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

static int row_constructor_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL) {
        set_row_constructor_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        set_row_constructor_operand_count_error(database, 1U);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        mylite_sql_ast_node_operator(expression) == MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT) {
        return row_constructor_not_scalar_value(database, expression, out_cell);
    }
    if (expression->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        return row_constructor_not_scalar_value(database, expression, out_cell);
    }
    if (expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return row_constructor_comparison_scalar_value(database, expression, out_cell);
    }
    set_row_constructor_unsupported_error(database);
    return MYLITE_ERROR;
}

static int row_constructor_comparison_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *left_node =
        unwrap_parenthesized_expression(child_at(expression, 0U));
    const struct mylite_sql_ast_node *right_node =
        unwrap_parenthesized_expression(child_at(expression, 1U));
    struct row_constructor_scalar_values left = {0};
    struct row_constructor_scalar_values right = {0};
    struct row_constructor_scalar_result result = {
        .truth = ROW_CONSTRUCTOR_SCALAR_UNKNOWN,
        .warning_count = 0U,
    };
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
    int rc = MYLITE_OK;

    if (!row_constructor_comparison_operator_is_supported(operator_kind) ||
        !row_constructor_expression_is_constructor(left_node) ||
        !row_constructor_expression_is_constructor(right_node)) {
        set_row_constructor_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = row_constructor_collect_values(database, left_node, &left);
    if (rc == MYLITE_OK) {
        rc = row_constructor_collect_values(database, right_node, &right);
    }
    if (rc == MYLITE_OK && left.count != right.count) {
        set_row_constructor_operand_count_error(database, left.count);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        result.warning_count = row_constructor_warning_count(&left);
        if (result.warning_count <= SIZE_MAX - row_constructor_warning_count(&right)) {
            result.warning_count += row_constructor_warning_count(&right);
        } else {
            set_nomem_error(database);
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_OK) {
        rc = row_constructor_compare_truth(database, operator_kind, &left, &right, &result.truth);
    }
    if (rc == MYLITE_OK) {
        rc = row_constructor_set_scalar_result(database, &result, out_cell);
    }

    row_constructor_values_deinit(&right);
    row_constructor_values_deinit(&left);
    return rc;
}

static int row_constructor_not_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell value = {0};
    int written = 0;
    int rc = row_constructor_scalar_value(database, child_at(expression, 0U), &value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    out_cell->staged_division_by_zero_warning_count = value.staged_division_by_zero_warning_count;
    if (value.value == NULL) {
        session_scalar_cell_deinit(&value);
        return MYLITE_OK;
    }

    written = snprintf(
        out_cell->integer_text,
        sizeof(out_cell->integer_text),
        "%d",
        strcmp(value.value, "0") == 0 ? 1 : 0
    );
    session_scalar_cell_deinit(&value);
    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        set_runtime_error(database, "failed to format row constructor NOT value");
        return MYLITE_ERROR;
    }
    out_cell->value = out_cell->integer_text;
    return MYLITE_OK;
}

static int row_constructor_collect_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *constructor,
    struct row_constructor_scalar_values *out_values
) {
    size_t value_index = 0U;

    if (out_values == NULL) {
        return MYLITE_MISUSE;
    }
    *out_values = (struct row_constructor_scalar_values){0};
    if (!row_constructor_expression_is_constructor(constructor)) {
        set_row_constructor_unsupported_error(database);
        return MYLITE_ERROR;
    }

    out_values->count = mylite_sql_ast_node_child_count(constructor);
    out_values->items = calloc(out_values->count, sizeof(*out_values->items));
    if (out_values->items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (const struct mylite_sql_ast_node *element = child_at(constructor, 0U); element != NULL;
         element = element->next_sibling) {
        int rc =
            row_constructor_collect_item_value(database, element, &out_values->items[value_index]);

        if (rc != MYLITE_OK) {
            return rc;
        }
        ++value_index;
    }
    return MYLITE_OK;
}

static int row_constructor_collect_item_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct row_constructor_scalar_item *out_item
) {
    int rc = MYLITE_OK;

    if (out_item == NULL) {
        return MYLITE_MISUSE;
    }
    *out_item = (struct row_constructor_scalar_item){0};
    out_item->expression = unwrap_parenthesized_expression(expression);
    out_item->uses_text_comparison =
        row_constructor_expression_uses_text_comparison(out_item->expression);
    if (out_item->uses_text_comparison) {
        return session_scalar_value(database, out_item->expression, &out_item->text_cell);
    }

    rc = evaluate_scalar_comparison_numeric_expression(
        database,
        out_item->expression,
        &out_item->numeric_value
    );
    if (rc == MYLITE_OK) {
        out_item->has_numeric_value = true;
    }
    return rc;
}

static int row_constructor_compare_truth(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const struct row_constructor_scalar_values *left,
    const struct row_constructor_scalar_values *right,
    enum row_constructor_scalar_truth *out_truth
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return row_constructor_equal_truth(database, left, right, out_truth);
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        return row_constructor_null_safe_equal_truth(database, left, right, out_truth);
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL: {
        enum row_constructor_scalar_truth equality = ROW_CONSTRUCTOR_SCALAR_UNKNOWN;
        int rc = row_constructor_equal_truth(database, left, right, &equality);

        if (rc != MYLITE_OK) {
            return rc;
        }
        if (equality == ROW_CONSTRUCTOR_SCALAR_UNKNOWN) {
            *out_truth = ROW_CONSTRUCTOR_SCALAR_UNKNOWN;
            return MYLITE_OK;
        }
        *out_truth = equality == ROW_CONSTRUCTOR_SCALAR_TRUE ? ROW_CONSTRUCTOR_SCALAR_FALSE
                                                             : ROW_CONSTRUCTOR_SCALAR_TRUE;
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return row_constructor_order_truth(database, operator_kind, left, right, out_truth);
    default:
        *out_truth = ROW_CONSTRUCTOR_SCALAR_UNKNOWN;
        return MYLITE_OK;
    }
}

static int row_constructor_equal_truth(
    struct mylite_db *database,
    const struct row_constructor_scalar_values *left,
    const struct row_constructor_scalar_values *right,
    enum row_constructor_scalar_truth *out_truth
) {
    bool saw_unknown = false;

    for (size_t index = 0U; index < left->count; ++index) {
        bool unknown = false;
        int comparison = 0;
        int rc = row_constructor_compare_items(
            database,
            &left->items[index],
            &right->items[index],
            &unknown,
            &comparison
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        if (unknown) {
            saw_unknown = true;
        } else if (comparison != 0) {
            *out_truth = ROW_CONSTRUCTOR_SCALAR_FALSE;
            return MYLITE_OK;
        }
    }
    *out_truth = saw_unknown ? ROW_CONSTRUCTOR_SCALAR_UNKNOWN : ROW_CONSTRUCTOR_SCALAR_TRUE;
    return MYLITE_OK;
}

static int row_constructor_null_safe_equal_truth(
    struct mylite_db *database,
    const struct row_constructor_scalar_values *left,
    const struct row_constructor_scalar_values *right,
    enum row_constructor_scalar_truth *out_truth
) {
    for (size_t index = 0U; index < left->count; ++index) {
        bool left_is_null = row_constructor_item_is_null(&left->items[index]);
        bool right_is_null = row_constructor_item_is_null(&right->items[index]);
        bool unknown = false;
        int comparison = 0;
        int rc = MYLITE_OK;

        if (left_is_null || right_is_null) {
            if (left_is_null != right_is_null) {
                *out_truth = ROW_CONSTRUCTOR_SCALAR_FALSE;
                return MYLITE_OK;
            }
            continue;
        }

        rc = row_constructor_compare_items(
            database,
            &left->items[index],
            &right->items[index],
            &unknown,
            &comparison
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (unknown || comparison != 0) {
            *out_truth = ROW_CONSTRUCTOR_SCALAR_FALSE;
            return MYLITE_OK;
        }
    }
    *out_truth = ROW_CONSTRUCTOR_SCALAR_TRUE;
    return MYLITE_OK;
}

static int row_constructor_order_truth(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const struct row_constructor_scalar_values *left,
    const struct row_constructor_scalar_values *right,
    enum row_constructor_scalar_truth *out_truth
) {
    for (size_t index = 0U; index < left->count; ++index) {
        bool unknown = false;
        int comparison = 0;
        int rc = row_constructor_compare_items(
            database,
            &left->items[index],
            &right->items[index],
            &unknown,
            &comparison
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        if (unknown) {
            *out_truth = ROW_CONSTRUCTOR_SCALAR_UNKNOWN;
            return MYLITE_OK;
        }
        if (comparison == 0) {
            continue;
        }
        if ((operator_kind == MYLITE_SQL_AST_OPERATOR_LESS && comparison < 0) ||
            (operator_kind == MYLITE_SQL_AST_OPERATOR_LESS_EQUAL && comparison < 0) ||
            (operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER && comparison > 0) ||
            (operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL && comparison > 0)) {
            *out_truth = ROW_CONSTRUCTOR_SCALAR_TRUE;
        } else {
            *out_truth = ROW_CONSTRUCTOR_SCALAR_FALSE;
        }
        return MYLITE_OK;
    }
    *out_truth = (operator_kind == MYLITE_SQL_AST_OPERATOR_LESS_EQUAL ||
                  operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL)
                     ? ROW_CONSTRUCTOR_SCALAR_TRUE
                     : ROW_CONSTRUCTOR_SCALAR_FALSE;
    return MYLITE_OK;
}

static int row_constructor_compare_items(
    struct mylite_db *database,
    const struct row_constructor_scalar_item *left,
    const struct row_constructor_scalar_item *right,
    bool *out_unknown,
    int *out_comparison
) {
    if (out_unknown == NULL || out_comparison == NULL) {
        return MYLITE_MISUSE;
    }
    *out_unknown = false;
    *out_comparison = 0;
    if (row_constructor_item_is_null(left) || row_constructor_item_is_null(right)) {
        *out_unknown = true;
        return MYLITE_OK;
    }
    if (left->uses_text_comparison && right->uses_text_comparison) {
        *out_comparison = row_constructor_compare_text_items(left, right);
        return MYLITE_OK;
    }
    return row_constructor_compare_numeric_items(
        database,
        left,
        right,
        out_unknown,
        out_comparison
    );
}

static int row_constructor_compare_numeric_items(
    struct mylite_db *database,
    const struct row_constructor_scalar_item *left,
    const struct row_constructor_scalar_item *right,
    bool *out_unknown,
    int *out_comparison
) {
    struct scalar_arithmetic_value left_value = {0};
    struct scalar_arithmetic_value right_value = {0};
    int rc = row_constructor_numeric_item_value(database, left, &left_value);

    if (rc == MYLITE_OK) {
        rc = row_constructor_numeric_item_value(database, right, &right_value);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (left_value.is_null || right_value.is_null) {
        *out_unknown = true;
        return MYLITE_OK;
    }
    if (scalar_comparison_values_are_equal(&left_value, &right_value)) {
        *out_comparison = 0;
    } else if (scalar_comparison_numeric_result_is_true(
                   MYLITE_SQL_AST_OPERATOR_LESS,
                   &left_value,
                   &right_value
               )) {
        *out_comparison = -1;
    } else {
        *out_comparison = 1;
    }
    return MYLITE_OK;
}

static int row_constructor_numeric_item_value(
    struct mylite_db *database,
    const struct row_constructor_scalar_item *item,
    struct scalar_arithmetic_value *out_value
) {
    if (item == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (item->has_numeric_value) {
        *out_value = item->numeric_value;
        return MYLITE_OK;
    }
    return evaluate_scalar_comparison_numeric_expression(database, item->expression, out_value);
}

static int row_constructor_compare_text_items(
    const struct row_constructor_scalar_item *left,
    const struct row_constructor_scalar_item *right
) {
    const char *left_text = left->text_cell.value;
    const char *right_text = right->text_cell.value;
    size_t left_length = scalar_cell_text_length(&left->text_cell);
    size_t right_length = scalar_cell_text_length(&right->text_cell);
    size_t common_length = 0U;

    while (left_length > 0U && left_text[left_length - 1U] == ' ') {
        --left_length;
    }
    while (right_length > 0U && right_text[right_length - 1U] == ' ') {
        --right_length;
    }
    common_length = left_length < right_length ? left_length : right_length;
    for (size_t index = 0U; index < common_length; ++index) {
        unsigned char left_byte = (unsigned char)ascii_lower((unsigned char)left_text[index]);
        unsigned char right_byte = (unsigned char)ascii_lower((unsigned char)right_text[index]);

        if (left_byte < right_byte) {
            return -1;
        }
        if (left_byte > right_byte) {
            return 1;
        }
    }
    if (left_length == right_length) {
        return 0;
    }
    return left_length < right_length ? -1 : 1;
}

static bool row_constructor_item_is_null(const struct row_constructor_scalar_item *item) {
    if (item == NULL) {
        return true;
    }
    if (item->uses_text_comparison) {
        return item->text_cell.value == NULL;
    }
    return item->numeric_value.is_null;
}

static int row_constructor_set_scalar_result(
    struct mylite_db *database,
    const struct row_constructor_scalar_result *result,
    struct session_scalar_cell *out_cell
) {
    int written = 0;

    if (result == NULL) {
        return MYLITE_MISUSE;
    }
    out_cell->staged_division_by_zero_warning_count = result->warning_count;
    if (result->truth == ROW_CONSTRUCTOR_SCALAR_UNKNOWN) {
        return MYLITE_OK;
    }

    written = snprintf(
        out_cell->integer_text,
        sizeof(out_cell->integer_text),
        "%d",
        result->truth == ROW_CONSTRUCTOR_SCALAR_TRUE ? 1 : 0
    );
    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        set_runtime_error(database, "failed to format row constructor scalar value");
        return MYLITE_ERROR;
    }
    out_cell->value = out_cell->integer_text;
    return MYLITE_OK;
}

static bool row_constructor_expression_is_constructor(const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    return expression != NULL && expression->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR;
}

static bool row_constructor_expression_uses_text_comparison(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_STRING;
    }
    return (is_scalar_concat_projection_expression(expression) ||
            is_string_scalar_function_projection_expression(expression)) != 0;
}

static bool row_constructor_comparison_operator_is_supported(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return true;
    default:
        return false;
    }
}

static size_t row_constructor_warning_count(const struct row_constructor_scalar_values *values) {
    size_t warning_count = 0U;

    for (size_t index = 0U; values != NULL && index < values->count; ++index) {
        if (values->items[index].uses_text_comparison) {
            warning_count += values->items[index].text_cell.staged_division_by_zero_warning_count;
        } else {
            warning_count += values->items[index].numeric_value.division_by_zero_warning_count;
        }
    }
    return warning_count;
}

static void row_constructor_values_deinit(struct row_constructor_scalar_values *values) {
    if (values == NULL) {
        return;
    }
    for (size_t index = 0U; index < values->count; ++index) {
        session_scalar_cell_deinit(&values->items[index].text_cell);
    }
    free(values->items);
    *values = (struct row_constructor_scalar_values){0};
}

static void set_row_constructor_operand_count_error(
    struct mylite_db *database,
    size_t expected_column_count
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Operand should contain %zu column(s)",
        expected_column_count
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_operand_should_contain_one_column,
        "21000",
        message
    );
}

static void set_row_constructor_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(database, "row constructor scalar predicates are not yet supported");
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
    if (frame->kind == SCALAR_COMPARISON_EVAL_APPLY_NOT) {
        return evaluate_scalar_comparison_apply_not_frame(database, value_stack);
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

static int evaluate_scalar_comparison_apply_not_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack
) {
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};

    if (!scalar_arithmetic_value_stack_pop(value_stack, &value)) {
        set_runtime_error(database, "invalid scalar comparison evaluation stack");
        return MYLITE_ERROR;
    }
    if (value.is_null) {
        return scalar_arithmetic_value_stack_push(database, value_stack, value);
    }
    value.integer = value.integer == 0 ? 1 : 0;
    return scalar_arithmetic_value_stack_push(database, value_stack, value);
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
    if (expression->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        rc = scalar_comparison_eval_stack_push(
            database,
            expression_stack,
            SCALAR_COMPARISON_EVAL_APPLY_NOT,
            NULL,
            MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT
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
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_SOUNDS_LIKE) {
        rc = evaluate_scalar_comparison_sounds_like_expression(database, expression, &value);
        if (rc == MYLITE_OK) {
            rc = scalar_arithmetic_value_stack_push(database, value_stack, value);
        }
        return rc;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_REGEXP ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_RLIKE) {
        rc = evaluate_scalar_comparison_regexp_expression(database, expression, &value);
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
        .unsupported = "SELECT scalar LIKE supports only string, integer, boolean, and NULL values",
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
            (lexer_modes_for_statement(database) & MYLITE_SQL_MODE_NO_BACKSLASH_ESCAPES) != 0U
                ? '\0'
                : '\\'
        );
        out_value->integer = matches ? 1 : 0;
    }

    session_scalar_cell_deinit(&pattern);
    session_scalar_cell_deinit(&value);
    return rc;
}

static int evaluate_scalar_comparison_sounds_like_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
) {
    struct session_scalar_cell left = {0};
    struct session_scalar_cell right = {0};
    size_t left_length = 0U;
    size_t right_length = 0U;
    int rc = MYLITE_OK;

    if (expression == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){.is_null = false, .integer = 0};
    if (expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        mylite_sql_ast_node_operator(expression) != MYLITE_SQL_AST_OPERATOR_SOUNDS_LIKE ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_scalar_comparison_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = mylite_execution_scalar_soundex_argument_result_value(
        database,
        child_at(expression, 0U),
        &left
    );
    if (rc == MYLITE_OK) {
        rc = mylite_execution_scalar_soundex_argument_result_value(
            database,
            child_at(expression, 1U),
            &right
        );
    }
    if (rc == MYLITE_OK && (left.value == NULL || right.value == NULL)) {
        out_value->is_null = true;
    } else if (rc == MYLITE_OK) {
        left_length = scalar_cell_text_length(&left);
        right_length = scalar_cell_text_length(&right);
        out_value->integer =
            left_length == right_length && memcmp(left.value, right.value, left_length) == 0 ? 1
                                                                                             : 0;
    }

    session_scalar_cell_deinit(&right);
    session_scalar_cell_deinit(&left);
    return rc;
}

static int evaluate_scalar_comparison_regexp_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
) {
    static const struct scalar_text_conversion_messages value_messages = {
        .unsupported =
            "SELECT scalar REGEXP supports only string, integer, boolean, and NULL values",
        .signed_value = "SELECT scalar REGEXP supports only signed integer values",
        .string_unsupported = "SELECT scalar REGEXP supports only string value literals",
        .embedded_nul = "SELECT scalar REGEXP value literals do not support NUL bytes",
    };
    static const struct scalar_text_conversion_messages pattern_messages = {
        .unsupported =
            "SELECT scalar REGEXP supports only string, integer, boolean, and NULL patterns",
        .signed_value = "SELECT scalar REGEXP supports only signed integer patterns",
        .string_unsupported = "SELECT scalar REGEXP supports only string pattern literals",
        .embedded_nul = "SELECT scalar REGEXP pattern literals do not support NUL bytes",
    };
    struct session_scalar_cell value = {0};
    struct session_scalar_cell pattern = {0};
    struct mylite_regexp_program *program = NULL;
    enum mylite_regexp_compile_status compile_status = MYLITE_REGEXP_COMPILE_OK;
    enum mylite_regexp_match_status match_status = MYLITE_REGEXP_MATCH_OK;
    bool value_is_binary = false;
    bool pattern_is_binary = false;
    bool case_sensitive = false;
    bool matches = false;
    int rc = MYLITE_OK;

    if (expression == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){.is_null = false, .integer = 0};
    if (expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        (mylite_sql_ast_node_operator(expression) != MYLITE_SQL_AST_OPERATOR_REGEXP &&
         mylite_sql_ast_node_operator(expression) != MYLITE_SQL_AST_OPERATOR_RLIKE) ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_scalar_comparison_unsupported_error(database);
        return MYLITE_ERROR;
    }

    value_is_binary = predicate_row_scalar_cast_expression_is_binary(child_at(expression, 0U));
    pattern_is_binary = predicate_row_scalar_cast_expression_is_binary(child_at(expression, 1U));
    case_sensitive = value_is_binary || pattern_is_binary;
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
    if (rc == MYLITE_OK && value_is_binary != pattern_is_binary &&
        !(value_is_binary ? pattern.value == NULL : value.value == NULL)) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            mysql_error_regexp_character_set_mismatch,
            "HY000",
            value_is_binary
                ? "Character set 'binary' cannot be used in conjunction with "
                  "'latin1_swedish_ci' in call to regexp_like."
                : "Character set 'latin1_swedish_ci' cannot be used in conjunction with "
                  "'binary' in call to regexp_like."
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && (value.value == NULL || pattern.value == NULL)) {
        out_value->is_null = true;
    }
    if (rc == MYLITE_OK && !out_value->is_null &&
        !text_value_is_supported_string_key(value.value, scalar_cell_text_length(&value))) {
        set_unsupported_error(
            database,
            "SELECT scalar REGEXP value literals support only ASCII text"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && !out_value->is_null &&
        !text_value_is_supported_string_key(pattern.value, scalar_cell_text_length(&pattern))) {
        set_unsupported_error(
            database,
            "SELECT scalar REGEXP pattern literals support only ASCII text"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && !out_value->is_null) {
        compile_status = case_sensitive ? mylite_regexp_compile_ascii_cs(
                                              pattern.value,
                                              scalar_cell_text_length(&pattern),
                                              &program
                                          )
                                        : mylite_regexp_compile_ascii_ci(
                                              pattern.value,
                                              scalar_cell_text_length(&pattern),
                                              &program
                                          );
        if (compile_status != MYLITE_REGEXP_COMPILE_OK) {
            if (compile_status == MYLITE_REGEXP_COMPILE_NOMEM) {
                set_nomem_error(database);
                rc = MYLITE_NOMEM;
            } else if (compile_status == MYLITE_REGEXP_COMPILE_UNCLOSED_BRACKET) {
                set_regexp_error(
                    database,
                    "The regular expression contains an unclosed bracket expression."
                );
                rc = MYLITE_ERROR;
            } else if (compile_status == MYLITE_REGEXP_COMPILE_INVALID_RANGE) {
                set_regexp_character_range_error(
                    database,
                    "The regular expression contains an invalid character range."
                );
                rc = MYLITE_ERROR;
            } else {
                set_unsupported_error(
                    database,
                    "SELECT scalar REGEXP patterns support only MyLite's baseline ASCII regular "
                    "expression subset"
                );
                rc = MYLITE_ERROR;
            }
        }
    }
    if (rc == MYLITE_OK && !out_value->is_null) {
        match_status = case_sensitive ? mylite_regexp_program_match_ascii_cs(
                                            program,
                                            value.value,
                                            scalar_cell_text_length(&value),
                                            &matches
                                        )
                                      : mylite_regexp_program_match_ascii_ci(
                                            program,
                                            value.value,
                                            scalar_cell_text_length(&value),
                                            &matches
                                        );
        if (match_status == MYLITE_REGEXP_MATCH_NOMEM) {
            set_nomem_error(database);
            rc = MYLITE_NOMEM;
        } else if (match_status == MYLITE_REGEXP_MATCH_UNSUPPORTED_VALUE) {
            set_unsupported_error(database, "SELECT scalar REGEXP values support only ASCII text");
            rc = MYLITE_ERROR;
        } else if (match_status != MYLITE_REGEXP_MATCH_OK) {
            set_unsupported_error(
                database,
                "SELECT scalar REGEXP input is too large for MyLite's baseline subset"
            );
            rc = MYLITE_ERROR;
        } else {
            out_value->integer = matches ? 1 : 0;
        }
    }

    mylite_regexp_program_free(program);
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
    rc = apply_scalar_comparison_numeric_operator(database, operator_kind, &left, &right, &result);
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
    parsed = mylite_numeric_parse_double(text, &end);
    if (errno != 0 || end == text || end == NULL || *end != '\0') {
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
    parsed = mylite_numeric_parse_double(text, &end);
    if (errno != 0) {
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
