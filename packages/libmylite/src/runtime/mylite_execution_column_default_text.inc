static bool column_default_value_is_current_date_expression(
    const struct mylite_sql_ast_node *value_node
) {
    value_node = unwrap_parenthesized_expression(value_node);
    return (value_node != NULL && value_node->kind == MYLITE_SQL_AST_CURRENT_DATE_VALUE) != 0;
}

static bool column_default_value_is_current_time_expression(
    const struct mylite_sql_ast_node *value_node
) {
    value_node = unwrap_parenthesized_expression(value_node);
    return (value_node != NULL && value_node->kind == MYLITE_SQL_AST_CURRENT_TIME_VALUE) != 0;
}

static bool column_default_value_is_current_timestamp_expression(
    const struct mylite_sql_ast_node *value_node
) {
    value_node = unwrap_parenthesized_expression(value_node);
    return (value_node != NULL && value_node->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE) != 0;
}

static void planned_column_descriptor_for_default(
    const struct planned_column *column,
    struct mylite_catalog_column_descriptor *out_descriptor
) {
    memset(out_descriptor, 0, sizeof(*out_descriptor));
    snprintf(out_descriptor->name, sizeof(out_descriptor->name), "%s", column->name);
    snprintf(
        out_descriptor->logical_type,
        sizeof(out_descriptor->logical_type),
        "%s",
        column->logical_type
    );
    snprintf(
        out_descriptor->physical_type,
        sizeof(out_descriptor->physical_type),
        "%s",
        column->physical_type
    );
    out_descriptor->is_nullable = column->is_nullable;
}

static int copy_planned_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    struct planned_value *value
) {
    if (value->text == NULL || value->text_length >= sizeof(column->default_text)) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    memcpy(column->default_text, value->text, value->text_length);
    column->default_text[value->text_length] = '\0';

    return MYLITE_OK;
}

static int copy_planned_character_expression_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    const char *text,
    size_t text_length
) {
    if (text == NULL || text_length >= sizeof(column->default_text)) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    memcpy(column->default_text, text, text_length);
    column->default_text[text_length] = '\0';
    return MYLITE_OK;
}

static int copy_planned_binary_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    const struct planned_value *value
) {
    char *hex_text = NULL;
    size_t hex_length = 0U;
    int rc = MYLITE_OK;

    if (value == NULL || !value->is_blob || (value->text == NULL && value->text_length != 0U)) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    rc = mylite_execution_format_hex_bytes(
        database,
        (const unsigned char *)value->text,
        value->text_length,
        &hex_text
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    hex_length = strlen(hex_text);
    if (hex_length >= sizeof(column->default_text)) {
        free(hex_text);
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    memcpy(column->default_text, hex_text, hex_length + 1U);
    free(hex_text);
    return MYLITE_OK;
}

static int copy_planned_default_approximate_text(
    struct mylite_db *database,
    struct planned_column *column,
    const struct planned_value *value
) {
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    int rc = approximate_type_info_for_logical_type(column->logical_type, &info);

    if (rc == MYLITE_OK && (value == NULL || !value->is_real)) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = format_approximate_value_text(
            database,
            value->real,
            &info,
            "default approximate",
            column->default_text,
            sizeof(column->default_text)
        );
    }
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }

    return rc;
}

static int copy_planned_integer_expression_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *expression
) {
    struct mylite_dynamic_string text = {0};
    int rc = MYLITE_OK;

    mylite_dynamic_string_init(&text);
    rc = append_integer_expression_default_text(&text, expression);
    if (rc == MYLITE_OK && (text.length == 0U || text.length >= sizeof(column->default_text))) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        memcpy(column->default_text, text.text, text.length);
        column->default_text[text.length] = '\0';
    } else if (rc != MYLITE_NOMEM) {
        set_invalid_default_error(database, column->name);
        rc = MYLITE_ERROR;
    }

    mylite_dynamic_string_deinit(&text);
    return rc;
}

static int append_integer_expression_default_text(
    struct mylite_dynamic_string *string,
    const struct mylite_sql_ast_node *expression
) {
    struct integer_expression_default_text_stack stack = {
        .items = NULL,
        .count = 0U,
        .capacity = 0U,
    };
    int rc = MYLITE_OK;

    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    rc = push_integer_expression_default_text_item(
        &stack,
        INTEGER_EXPRESSION_DEFAULT_TEXT_NODE,
        expression,
        NULL
    );
    while (rc == MYLITE_OK && stack.count > 0U) {
        struct integer_expression_default_text_work_item item = stack.items[--stack.count];

        rc = append_integer_expression_default_text_item(string, &stack, &item);
    }

    free(stack.items);
    return rc;
}

static int append_integer_expression_default_text_item(
    struct mylite_dynamic_string *string,
    struct integer_expression_default_text_stack *stack,
    const struct integer_expression_default_text_work_item *item
) {
    if (item->kind == INTEGER_EXPRESSION_DEFAULT_TEXT_BYTES) {
        return mylite_dynamic_string_append(string, item->text);
    }
    if (item->kind == INTEGER_EXPRESSION_DEFAULT_TEXT_NODE) {
        return append_integer_expression_default_node_text(string, stack, item->expression);
    }
    return MYLITE_ERROR;
}

static int append_integer_expression_default_node_text(
    struct mylite_dynamic_string *string,
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return append_integer_expression_default_literal_text(string, expression);
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
        return append_integer_expression_default_parenthesized_text(stack, expression);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return append_integer_expression_default_unary_text(stack, expression);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return append_integer_expression_default_binary_text(stack, expression);
    case MYLITE_SQL_AST_MOD_FUNCTION:
        return append_integer_expression_default_mod_text(stack, expression);
    default:
        return MYLITE_ERROR;
    }
}

static int append_integer_expression_default_parenthesized_text(
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    int rc = push_integer_expression_default_text_item(
        stack,
        INTEGER_EXPRESSION_DEFAULT_TEXT_BYTES,
        NULL,
        ")"
    );

    if (rc == MYLITE_OK) {
        rc = push_integer_expression_default_text_item(
            stack,
            INTEGER_EXPRESSION_DEFAULT_TEXT_NODE,
            child_at(expression, 0U),
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = push_integer_expression_default_text_item(
            stack,
            INTEGER_EXPRESSION_DEFAULT_TEXT_BYTES,
            NULL,
            "("
        );
    }
    return rc;
}

static int append_integer_expression_default_unary_text(
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
    const char *operator_text = NULL;
    int rc = MYLITE_OK;

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
        operator_text = "+";
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        operator_text = "-";
    } else {
        return MYLITE_ERROR;
    }

    rc = push_integer_expression_default_text_item(
        stack,
        INTEGER_EXPRESSION_DEFAULT_TEXT_NODE,
        child_at(expression, 0U),
        NULL
    );
    if (rc == MYLITE_OK) {
        rc = push_integer_expression_default_text_item(
            stack,
            INTEGER_EXPRESSION_DEFAULT_TEXT_BYTES,
            NULL,
            operator_text
        );
    }
    return rc;
}

static int append_integer_expression_default_binary_text(
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    const char *operator_text =
        integer_expression_default_binary_operator_text(mylite_sql_ast_node_operator(expression));
    int rc = MYLITE_OK;

    if (operator_text == NULL) {
        return MYLITE_ERROR;
    }

    rc = push_integer_expression_default_text_item(
        stack,
        INTEGER_EXPRESSION_DEFAULT_TEXT_NODE,
        child_at(expression, 1U),
        NULL
    );
    if (rc == MYLITE_OK) {
        rc = push_integer_expression_default_text_item(
            stack,
            INTEGER_EXPRESSION_DEFAULT_TEXT_BYTES,
            NULL,
            operator_text
        );
    }
    if (rc == MYLITE_OK) {
        rc = push_integer_expression_default_text_item(
            stack,
            INTEGER_EXPRESSION_DEFAULT_TEXT_NODE,
            child_at(expression, 0U),
            NULL
        );
    }
    return rc;
}

static int append_integer_expression_default_mod_text(
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_child_count(expression) != 2U) {
        return MYLITE_ERROR;
    }

    rc = push_integer_expression_default_text_item(
        stack,
        INTEGER_EXPRESSION_DEFAULT_TEXT_NODE,
        child_at(expression, 1U),
        NULL
    );
    if (rc == MYLITE_OK) {
        rc = push_integer_expression_default_text_item(
            stack,
            INTEGER_EXPRESSION_DEFAULT_TEXT_BYTES,
            NULL,
            " % "
        );
    }
    if (rc == MYLITE_OK) {
        rc = push_integer_expression_default_text_item(
            stack,
            INTEGER_EXPRESSION_DEFAULT_TEXT_NODE,
            child_at(expression, 0U),
            NULL
        );
    }
    return rc;
}

static const char *integer_expression_default_binary_operator_text(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
        return " + ";
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        return " - ";
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        return " * ";
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
        return " DIV ";
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        return " % ";
    default:
        return NULL;
    }
}

static int push_integer_expression_default_text_item(
    struct integer_expression_default_text_stack *stack,
    enum integer_expression_default_text_work_item_kind kind,
    const struct mylite_sql_ast_node *expression,
    const char *text
) {
    struct integer_expression_default_text_work_item *grown_items = NULL;
    size_t new_capacity = 0U;

    if (stack->count == stack->capacity) {
        new_capacity = stack->capacity == 0U
                           ? integer_expression_default_text_initial_capacity
                           : stack->capacity * integer_expression_default_text_growth_factor;
        if (new_capacity < stack->capacity || new_capacity > SIZE_MAX / sizeof(*stack->items)) {
            return MYLITE_NOMEM;
        }
        grown_items = realloc(stack->items, new_capacity * sizeof(*grown_items));
        if (grown_items == NULL) {
            return MYLITE_NOMEM;
        }
        stack->items = grown_items;
        stack->capacity = new_capacity;
    }

    stack->items[stack->count] = (struct integer_expression_default_text_work_item){
        .kind = kind,
        .expression = expression,
        .text = text,
    };
    ++stack->count;
    return MYLITE_OK;
}

static int append_integer_expression_default_literal_text(
    struct mylite_dynamic_string *string,
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL || expression->span.text == NULL || expression->span.length == 0U) {
        return MYLITE_ERROR;
    }
    return mylite_dynamic_string_append_bytes(
        string,
        expression->span.text,
        expression->span.length
    );
}

static int convert_integer_expression_default_result(
    struct mylite_db *database,
    const struct planned_column *column,
    int64_t value,
    int64_t *out_value
) {
    const uint64_t bigint_signed_negative_abs_max = 9223372036854775808ULL;
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct integer_column_range range = {0};
    uint64_t magnitude = 0U;
    bool is_negative = value < 0;
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(column, &descriptor);
    rc = integer_range_for_column(
        database,
        &descriptor,
        "DEFAULT expression supports only baseline integer columns",
        &range
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (is_negative) {
        magnitude = value == INT64_MIN ? bigint_signed_negative_abs_max : (uint64_t)(-value);
        if (range.negative_abs_max == 0U || magnitude > range.negative_abs_max) {
            return MYLITE_ERROR;
        }
    } else {
        magnitude = (uint64_t)value;
        if (magnitude > range.positive_max) {
            return MYLITE_ERROR;
        }
    }

    *out_value = value;
    return MYLITE_OK;
}

static int convert_bit_expression_default_result(
    struct mylite_db *database,
    const struct planned_column *column,
    int64_t value,
    int64_t *out_value
) {
    uint64_t width = 0U;
    uint64_t magnitude = 0U;
    int rc = bit_width_for_logical_type(column == NULL ? NULL : column->logical_type, &width);

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (rc != MYLITE_OK || value < 0) {
        return MYLITE_ERROR;
    }

    magnitude = (uint64_t)value;
    if (magnitude > bit_width_max_value(width)) {
        return MYLITE_ERROR;
    }

    *out_value = value;
    (void)database;
    return MYLITE_OK;
}

static int convert_year_expression_default_result(int64_t value, int64_t *out_value) {
    const uint64_t bigint_signed_negative_abs_max = 9223372036854775808ULL;
    uint64_t magnitude = 0U;
    uint32_t year = 0U;
    bool is_negative = value < 0;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    magnitude = value == INT64_MIN ? bigint_signed_negative_abs_max : (uint64_t)llabs(value);
    if (!convert_year_numeric_magnitude(magnitude, is_negative, &year)) {
        return MYLITE_ERROR;
    }

    *out_value = (int64_t)year;
    return MYLITE_OK;
}
