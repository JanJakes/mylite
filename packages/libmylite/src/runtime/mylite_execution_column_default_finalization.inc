static int validate_column_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *default_node,
    const struct planned_column *column
) {
    if (default_node == NULL) {
        return MYLITE_OK;
    }
    if (default_node->kind == MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE) {
        return validate_column_default_value(database, default_node, column);
    }
    if (default_node->kind != MYLITE_SQL_AST_COLUMN_DEFAULT_NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (!column->is_nullable) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_column_default_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *default_node,
    const struct planned_column *column
) {
    const struct mylite_sql_ast_node *value_node = child_at(default_node, 0U);

    if (column_default_value_is_current_timestamp_expression(value_node)) {
        if (planned_column_is_datetime(column) || planned_column_is_timestamp(column)) {
            return MYLITE_OK;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (column_default_value_is_current_date_expression(value_node)) {
        if (planned_column_is_date(column)) {
            return MYLITE_OK;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (column_default_value_is_current_time_expression(value_node)) {
        if (planned_column_is_time(column)) {
            return MYLITE_OK;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (planned_column_is_text_family(column)) {
        if (column_default_value_is_parenthesized_text_expression(default_node)) {
            return MYLITE_OK;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (planned_column_is_binary_string_family(column)) {
        return validate_binary_column_default_value(database, default_node, column);
    }
    if (planned_column_is_spatial(column)) {
        set_json_cant_have_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (planned_column_is_json(column)) {
        if (column_default_value_is_parenthesized_expression(default_node)) {
            set_unsupported_error(database, "JSON expression defaults are not yet supported");
            return MYLITE_ERROR;
        }
        set_json_cant_have_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int validate_binary_column_default_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *default_node,
    const struct planned_column *column
) {
    const struct mylite_sql_ast_node *value_node = child_at(default_node, 0U);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (planned_column_is_binary_blob_family(column)) {
        if (column_default_value_is_parenthesized_binary_blob_expression(default_node)) {
            return MYLITE_OK;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(value_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
        literal_kind == MYLITE_SQL_AST_LITERAL_HEX) {
        return MYLITE_OK;
    }
    set_invalid_default_error(database, column->name);
    return MYLITE_ERROR;
}

static int finalize_planned_column_defaults(
    struct mylite_db *database,
    struct planned_column *columns,
    size_t column_count
) {
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        int rc = finalize_planned_column_default(database, &columns[column_index]);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int finalize_planned_column_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    const struct mylite_sql_ast_node *value_node = NULL;

    column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NONE;
    column->default_integer = 0;
    column->default_text[0] = '\0';
    if (column->default_node == NULL ||
        column->default_node->kind == MYLITE_SQL_AST_COLUMN_DEFAULT_NULL) {
        return MYLITE_OK;
    }
    if (column->default_node->kind != MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    value_node = child_at(column->default_node, 0U);
    if (column_default_value_is_current_timestamp_expression(value_node)) {
        if (planned_column_is_datetime(column) || planned_column_is_timestamp(column)) {
            column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP;
            return MYLITE_OK;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (column_default_value_is_current_date_expression(value_node)) {
        if (planned_column_is_date(column)) {
            column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE;
            return MYLITE_OK;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (column_default_value_is_current_time_expression(value_node)) {
        if (planned_column_is_time(column)) {
            column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME;
            return MYLITE_OK;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (value_node != NULL &&
        value_node->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "CURRENT_TIMESTAMP");
        return MYLITE_ERROR;
    }
    if (column_default_value_is_parenthesized_expression(column->default_node)) {
        return finalize_planned_column_parenthesized_default(database, column, value_node);
    }

    return finalize_planned_column_type_default(database, column);
}

static int finalize_planned_column_parenthesized_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
) {
    if (planned_column_is_char_or_varchar(column)) {
        return finalize_planned_column_character_expression_default(database, column, value_node);
    }
    if (planned_column_is_text_family(column)) {
        return finalize_planned_column_text_expression_default(database, column, value_node);
    }
    if (planned_column_is_binary_blob_family(column)) {
        return finalize_planned_column_binary_blob_expression_default(database, column, value_node);
    }
    if (planned_column_is_bit(column)) {
        return finalize_planned_column_bit_expression_default(database, column, value_node);
    }
    if (planned_column_is_year(column)) {
        return finalize_planned_column_year_expression_default(database, column, value_node);
    }
    if (planned_column_is_enum(column) || planned_column_is_set(column) ||
        planned_column_is_json(column) || planned_column_is_spatial(column)) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    return finalize_planned_column_integer_expression_default(database, column, value_node);
}

static int finalize_planned_column_type_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    int64_t default_integer = 0;
    int rc = MYLITE_OK;

    if (planned_column_is_char(column) || planned_column_is_varchar(column)) {
        return finalize_planned_column_string_default(database, column);
    }
    if (planned_column_is_decimal(column)) {
        return finalize_planned_column_decimal_default(database, column);
    }
    if (planned_column_is_approximate(column)) {
        return finalize_planned_column_approximate_default(database, column);
    }
    if (planned_column_is_bit(column)) {
        return finalize_planned_column_bit_default(database, column);
    }
    if (planned_column_is_binary_string_family(column)) {
        return finalize_planned_column_binary_default(database, column);
    }
    if (planned_column_is_year(column)) {
        return finalize_planned_column_year_default(database, column);
    }
    if (planned_column_is_date(column)) {
        return finalize_planned_column_date_default(database, column);
    }
    if (planned_column_is_time(column)) {
        return finalize_planned_column_time_default(database, column);
    }
    if (planned_column_is_datetime(column)) {
        return finalize_planned_column_datetime_default(database, column);
    }
    if (planned_column_is_timestamp(column)) {
        return finalize_planned_column_timestamp_default(database, column);
    }
    if (planned_column_is_enum(column)) {
        return finalize_planned_column_enum_default(database, column);
    }
    if (planned_column_is_set(column)) {
        return finalize_planned_column_set_default(database, column);
    }

    rc = convert_column_default_value(
        database,
        child_at(column->default_node, 0U),
        column,
        &default_integer
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER;
    column->default_integer = default_integer;

    return MYLITE_OK;
}

static int finalize_planned_column_integer_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
) {
    struct scalar_arithmetic_value expression_value = {.is_null = false, .integer = 0};
    int64_t default_integer = 0;
    int rc = evaluate_integer_default_expression(database, value_node, &expression_value);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            return rc;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (expression_value.is_null) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION;
        snprintf(column->default_text, sizeof(column->default_text), "NULL");
        return MYLITE_OK;
    }

    rc = convert_integer_expression_default_result(
        database,
        column,
        expression_value.integer,
        &default_integer
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    rc = copy_planned_integer_expression_default_text(database, column, value_node);
    if (rc != MYLITE_OK) {
        return rc;
    }
    column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION;
    column->default_integer = default_integer;

    return MYLITE_OK;
}

static int finalize_planned_column_bit_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
) {
    struct scalar_arithmetic_value expression_value = {.is_null = false, .integer = 0};
    int64_t default_integer = 0;
    int rc = evaluate_integer_default_expression(database, value_node, &expression_value);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            return rc;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (expression_value.is_null) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION;
        snprintf(column->default_text, sizeof(column->default_text), "NULL");
        return MYLITE_OK;
    }

    rc = convert_bit_expression_default_result(
        database,
        column,
        expression_value.integer,
        &default_integer
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    rc = copy_planned_integer_expression_default_text(database, column, value_node);
    if (rc != MYLITE_OK) {
        return rc;
    }
    column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION;
    column->default_integer = default_integer;

    return MYLITE_OK;
}

static int finalize_planned_column_year_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
) {
    struct scalar_arithmetic_value expression_value = {.is_null = false, .integer = 0};
    int64_t default_integer = 0;
    int rc = evaluate_integer_default_expression(database, value_node, &expression_value);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            return rc;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (expression_value.is_null) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION;
        snprintf(column->default_text, sizeof(column->default_text), "NULL");
        return MYLITE_OK;
    }

    rc = convert_year_expression_default_result(expression_value.integer, &default_integer);
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    rc = copy_planned_integer_expression_default_text(database, column, value_node);
    if (rc != MYLITE_OK) {
        return rc;
    }
    column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION;
    column->default_integer = default_integer;

    return MYLITE_OK;
}

static int finalize_planned_column_text_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *expression = unwrap_parenthesized_expression(value_node);
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION;
        snprintf(column->default_text, sizeof(column->default_text), "NULL");
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_text_family_literal(database, expression, &descriptor, 1U, false, false, &value);
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}

static int finalize_planned_column_character_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *expression = unwrap_parenthesized_expression(value_node);
    char *text = NULL;
    size_t text_length = 0U;
    size_t character_count = 0U;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION;
        snprintf(column->default_text, sizeof(column->default_text), "NULL");
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        expression,
        "CHAR/VARCHAR generated defaults support only string literals",
        "CHAR/VARCHAR generated defaults do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK) {
        rc = validate_utf8_text(text, text_length, &character_count);
        if (rc != MYLITE_OK) {
            set_invalid_default_error(database, column->name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_character_expression_default_text(database, column, text, text_length);
    }
    free(text);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION;
    }

    return rc;
}

static int finalize_planned_column_string_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    return finalize_planned_column_string_default_value(
        database,
        column,
        child_at(column->default_node, 0U)
    );
}

static int finalize_planned_column_parenthesized_string_literal_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    const struct mylite_sql_ast_node *expression =
        unwrap_parenthesized_expression(child_at(column->default_node, 0U));

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NONE;
        column->default_text[0] = '\0';
        return MYLITE_OK;
    }

    return finalize_planned_column_string_default_value(database, column, expression);
}

static int finalize_planned_column_string_default_value(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(column, &descriptor);
    if (planned_column_is_char(column)) {
        rc = convert_char_literal(database, value_node, &descriptor, 1U, false, &value);
    } else {
        rc = convert_varchar_literal(database, value_node, &descriptor, 1U, false, &value);
    }
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}

static int finalize_planned_column_decimal_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    const struct mylite_sql_ast_node *value_node = child_at(column->default_node, 0U);
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(column, &descriptor);
    if (value_node != NULL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    rc = convert_decimal_literal(database, value_node, &descriptor, 1U, false, &value);
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL;
    }

    return rc;
}

static int finalize_planned_column_approximate_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    const struct mylite_sql_ast_node *value_node = child_at(column->default_node, 0U);
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_real = false,
        .integer = 0,
        .real = 0.0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(column, &descriptor);
    if (value_node != NULL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    rc = convert_approximate_literal(database, value_node, &descriptor, 1U, false, &value);
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_approximate_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}

static int finalize_planned_column_bit_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    const struct mylite_sql_ast_node *value_node = child_at(column->default_node, 0U);
    uint64_t width = 0U;
    uint64_t magnitude = 0U;
    bool is_negative = false;
    bool parse_overflow = false;
    int rc = bit_width_for_logical_type(column->logical_type, &width);

    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (value_node == NULL) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        (mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING ||
         mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_HEX)) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    rc = parse_bit_value_magnitude(value_node, &magnitude, &is_negative, &parse_overflow);
    if (rc != MYLITE_OK || parse_overflow || (is_negative && magnitude != 0U) ||
        magnitude > bit_width_max_value(width)) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    rc = format_bit_default_text(
        database,
        column->name,
        magnitude,
        column->default_text,
        sizeof(column->default_text)
    );
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }
    return rc;
}

static int finalize_planned_column_binary_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_blob = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    if (planned_column_is_binary_blob_family(column)) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_binary_string_literal(
        database,
        child_at(column->default_node, 0U),
        &descriptor,
        1U,
        false,
        &value
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_binary_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_BINARY;
    }

    return rc;
}

static int finalize_planned_column_binary_blob_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *expression = unwrap_parenthesized_expression(value_node);
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_blob = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION;
        snprintf(column->default_text, sizeof(column->default_text), "NULL");
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_HEX) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_binary_string_literal(database, expression, &descriptor, 1U, false, &value);
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_binary_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_BINARY;
    }

    return rc;
}

static int finalize_planned_column_year_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_year_literal(
        database,
        child_at(column->default_node, 0U),
        &descriptor,
        1U,
        false,
        &value
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}

static int finalize_planned_column_date_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_date_literal(
        database,
        child_at(column->default_node, 0U),
        &descriptor,
        1U,
        false,
        &value
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}

static int finalize_planned_column_time_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_time_literal(
        database,
        child_at(column->default_node, 0U),
        &descriptor,
        1U,
        false,
        &value
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}

static int finalize_planned_column_datetime_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_datetime_literal(
        database,
        child_at(column->default_node, 0U),
        &descriptor,
        1U,
        false,
        &value
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}

static int finalize_planned_column_timestamp_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_timestamp_literal(
        database,
        child_at(column->default_node, 0U),
        &descriptor,
        1U,
        false,
        &value
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}

static int finalize_planned_column_enum_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    if (column->default_node == NULL ||
        column->default_node->kind != MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_enum_string_literal(
        database,
        child_at(column->default_node, 0U),
        &descriptor,
        1U,
        false,
        true,
        ENUM_STRING_TRIM_TRAILING_SPACES,
        &value
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}

static int finalize_planned_column_set_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    if (column->default_node == NULL ||
        column->default_node->kind != MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_set_string_literal(
        database,
        child_at(column->default_node, 0U),
        &descriptor,
        1U,
        false,
        true,
        true,
        &value
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}
