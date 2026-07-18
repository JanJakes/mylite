static int convert_predicate_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    *out_value = (struct planned_value){.is_null = false, .integer = 0};
    if (value_node == NULL || column == NULL) {
        set_unsupported_error(database, "WHERE supports only integer predicate literals");
        return MYLITE_ERROR;
    }

    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, "WHERE supports only integer predicate literals");
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (!boolean_literal_magnitude(literal, &magnitude)) {
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            (mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER &&
             mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_STRING &&
             mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_HEX)) {
            set_unsupported_error(
                database,
                "WHERE supports only integer, boolean, and exact quoted integer predicate literals"
            );
            return MYLITE_ERROR;
        }
        if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
            if (is_negative) {
                set_unsupported_error(
                    database,
                    "WHERE supports only integer, boolean, and exact quoted integer predicate "
                    "literals"
                );
                return MYLITE_ERROR;
            }
            return convert_predicate_integer_string_literal(database, literal, column, out_value);
        }
        if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_HEX) {
            return convert_predicate_integer_hex_literal(
                database,
                literal,
                column,
                is_negative,
                out_value
            );
        }

        rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
        if (rc != MYLITE_OK) {
            set_predicate_out_of_range_error(database, column->name);
            return MYLITE_ERROR;
        }
    }

    rc = convert_integer_for_predicate(
        database,
        magnitude,
        is_negative,
        column,
        &out_value->integer
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_value->is_null = false;

    return MYLITE_OK;
}

static int convert_predicate_integer_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    bool is_negative,
    struct planned_value *out_value
) {
    uint64_t magnitude = 0U;
    bool overflow = false;
    int rc = parse_dml_hex_literal_magnitude(database, literal, &magnitude, &overflow);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (overflow) {
        set_predicate_out_of_range_error(database, column->name);
        return MYLITE_ERROR;
    }

    rc = convert_integer_for_predicate(
        database,
        magnitude,
        is_negative,
        column,
        &out_value->integer
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_value->is_null = false;
    return MYLITE_OK;
}

static int convert_predicate_integer_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    size_t value_start = 0U;
    size_t value_end = 0U;
    bool is_negative = false;
    bool has_digits = false;
    bool append_truncated_warning = false;
    uint64_t magnitude = 0U;
    enum mylite_execution_dml_numeric_parse_result parse_result =
        MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
    int rc = decode_sql_string_literal(
        database,
        literal,
        "WHERE supports only integer, boolean, and exact quoted integer predicate literals",
        "invalid integer string literal",
        &text,
        &text_length
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    while (value_start < text_length &&
           predicate_integer_string_ascii_whitespace((unsigned char)text[value_start])) {
        ++value_start;
    }
    value_end = text_length;
    while (value_end > value_start &&
           predicate_integer_string_ascii_whitespace((unsigned char)text[value_end - 1U])) {
        --value_end;
    }

    parse_result = mylite_execution_parse_signed_integer_text(
        text + value_start,
        value_end - value_start,
        &magnitude,
        &is_negative
    );
    if (parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID && value_start == value_end) {
        parse_result = MYLITE_EXECUTION_DML_NUMERIC_PARSE_OK;
        magnitude = 0U;
        is_negative = false;
    }
    if (parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID && value_start != value_end) {
        rc = scan_predicate_integer_string_prefix(
            text + value_start,
            value_end - value_start,
            &magnitude,
            &is_negative,
            &has_digits
        );
        if (rc == MYLITE_OK && !has_digits) {
            magnitude = 0U;
            is_negative = false;
        }
        append_truncated_warning = rc == MYLITE_OK;
        parse_result = rc == MYLITE_OK ? MYLITE_EXECUTION_DML_NUMERIC_PARSE_TRUNCATED
                                       : MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
    }
    if (parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID) {
        set_unsupported_error(
            database,
            "WHERE supports only integer, boolean, and exact quoted integer predicate literals"
        );
        rc = MYLITE_ERROR;
    } else if (parse_result != MYLITE_EXECUTION_DML_NUMERIC_PARSE_OK &&
               parse_result != MYLITE_EXECUTION_DML_NUMERIC_PARSE_TRUNCATED) {
        set_predicate_out_of_range_error(database, column->name);
        rc = MYLITE_ERROR;
    }

    if (rc == MYLITE_OK) {
        rc = convert_integer_for_predicate(
            database,
            magnitude,
            is_negative,
            column,
            &out_value->integer
        );
    }
    if (rc == MYLITE_OK && append_truncated_warning) {
        rc = append_predicate_truncated_double_warning(database, text, text_length);
    }
    free(text);
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_value->is_null = false;
    return MYLITE_OK;
}

static int scan_predicate_integer_string_prefix(
    const char *text,
    size_t text_length,
    uint64_t *out_magnitude,
    bool *out_is_negative,
    bool *out_has_digits
) {
    size_t index = 0U;
    uint64_t magnitude = 0U;

    if (text == NULL || out_magnitude == NULL || out_is_negative == NULL ||
        out_has_digits == NULL || text_length == 0U) {
        return MYLITE_ERROR;
    }

    *out_magnitude = 0U;
    *out_is_negative = false;
    *out_has_digits = false;
    if (text[index] == '+' || text[index] == '-') {
        *out_is_negative = text[index] == '-';
        ++index;
    }
    while (index < text_length && text[index] >= '0' && text[index] <= '9') {
        uint64_t digit = (uint64_t)(text[index] - '0');

        if (magnitude > (UINT64_MAX - digit) / decimal_base) {
            return MYLITE_ERROR;
        }
        magnitude = (magnitude * decimal_base) + digit;
        *out_has_digits = true;
        ++index;
    }
    if (*out_has_digits && index < text_length &&
        (text[index] == '.' || text[index] == 'e' || text[index] == 'E')) {
        return MYLITE_ERROR;
    }

    *out_magnitude = magnitude;
    return MYLITE_OK;
}

static int append_predicate_truncated_double_warning(
    struct mylite_db *database,
    const char *value,
    size_t value_length
) {
    static const char prefix[] = "Truncated incorrect DOUBLE value: '";
    static const char suffix[] = "'";
    char *message = NULL;
    size_t message_length = 0U;
    int rc = MYLITE_OK;

    if (database == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (value_length > SIZE_MAX - sizeof(prefix) - sizeof(suffix)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    message_length = (sizeof(prefix) - 1U) + value_length + (sizeof(suffix) - 1U);
    message = (char *)malloc(message_length + 1U);
    if (message == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    memcpy(message, prefix, sizeof(prefix) - 1U);
    memcpy(message + sizeof(prefix) - 1U, value, value_length);
    memcpy(message + sizeof(prefix) - 1U + value_length, suffix, sizeof(suffix));

    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_incorrect_double,
        "22007",
        message
    );
    free(message);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int convert_predicate_string_numeric_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    const uint64_t signed_positive_max = 9223372036854775807ULL;
    const uint64_t signed_negative_abs_max = 9223372036854775808ULL;
    const struct mylite_sql_ast_node *literal = value_node;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    *out_value = (struct planned_value){.is_null = false, .is_text = false, .integer = 0};
    if (value_node == NULL) {
        set_unsupported_error(
            database,
            "WHERE string numeric predicates support only signed integer literals"
        );
        return MYLITE_ERROR;
    }

    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "WHERE string numeric predicates support only signed integer literals"
            );
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }

    if (!boolean_literal_magnitude(literal, &magnitude)) {
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            set_unsupported_error(
                database,
                "WHERE string numeric predicates support only signed integer literals"
            );
            return MYLITE_ERROR;
        }
        rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
        if (rc != MYLITE_OK) {
            set_predicate_out_of_range_error(database, column->name);
            return MYLITE_ERROR;
        }
    }

    if (is_negative) {
        if (magnitude > signed_negative_abs_max) {
            set_predicate_out_of_range_error(database, column->name);
            return MYLITE_ERROR;
        }
        out_value->integer = magnitude == signed_negative_abs_max ? INT64_MIN : -(int64_t)magnitude;
        return MYLITE_OK;
    }
    if (magnitude > signed_positive_max) {
        set_predicate_out_of_range_error(database, column->name);
        return MYLITE_ERROR;
    }

    out_value->integer = (int64_t)magnitude;
    return MYLITE_OK;
}

static bool predicate_integer_string_ascii_whitespace(unsigned char byte) {
    return (byte == ' ' || (byte >= '\t' && byte <= '\r')) != 0;
}

static int convert_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_PARAMETER) {
        return copy_active_stmt_parameter_value(database, value_node, out_value);
    }
    if (column_descriptor_is_string_family(column)) {
        return convert_predicate_string_literal(database, value_node, out_value);
    }
    if (column_descriptor_is_enum(column)) {
        return convert_predicate_enum_literal(database, value_node, column, out_value);
    }
    if (column_descriptor_is_set(column)) {
        return convert_predicate_set_literal(database, value_node, column, out_value);
    }
    if (column_descriptor_is_binary_string_family(column)) {
        return convert_binary_string_literal(database, value_node, column, 1U, false, out_value);
    }
    if (column_descriptor_is_bit(column)) {
        return convert_bit_literal(database, value_node, column, 1U, false, out_value);
    }
    if (column_descriptor_is_decimal(column)) {
        return convert_decimal_literal(database, value_node, column, 1U, false, out_value);
    }
    if (column_descriptor_is_approximate(column)) {
        return convert_approximate_literal(database, value_node, column, 1U, false, out_value);
    }
    if (column_descriptor_is_year(column)) {
        return convert_predicate_year_literal(database, value_node, column, out_value);
    }
    if (column_descriptor_is_date(column)) {
        return convert_predicate_date_literal(database, value_node, column, false, out_value, NULL);
    }
    if (column_descriptor_is_time(column)) {
        return convert_predicate_time_literal(database, value_node, column, out_value);
    }
    if (column_descriptor_is_datetime(column)) {
        return convert_predicate_datetime_literal(database, value_node, column, out_value);
    }
    if (column_descriptor_is_timestamp(column)) {
        return convert_predicate_timestamp_literal(database, value_node, column, out_value);
    }
    return convert_predicate_integer_literal(database, value_node, column, out_value);
}

static int convert_predicate_value_with_date_comparison_mode(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value,
    bool *out_compare_date_as_datetime
) {
    if (out_compare_date_as_datetime != NULL) {
        *out_compare_date_as_datetime = false;
    }
    if (column_descriptor_is_date(column)) {
        return convert_predicate_date_literal(
            database,
            value_node,
            column,
            false,
            out_value,
            out_compare_date_as_datetime
        );
    }
    return convert_predicate_value(database, value_node, column, out_value);
}

static int convert_predicate_date_between_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *lower_node,
    const struct mylite_sql_ast_node *upper_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_lower_value,
    struct planned_value *out_upper_value,
    bool *out_compare_date_as_datetime
) {
    bool lower_is_datetime = false;
    bool upper_is_datetime = false;
    int rc = MYLITE_OK;

    if (out_compare_date_as_datetime != NULL) {
        *out_compare_date_as_datetime = false;
    }
    rc = convert_predicate_date_literal(
        database,
        lower_node,
        column,
        false,
        out_lower_value,
        &lower_is_datetime
    );
    if (rc == MYLITE_OK) {
        rc = convert_predicate_date_literal(
            database,
            upper_node,
            column,
            false,
            out_upper_value,
            &upper_is_datetime
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!lower_is_datetime && !upper_is_datetime) {
        return MYLITE_OK;
    }
    if (out_compare_date_as_datetime != NULL) {
        *out_compare_date_as_datetime = true;
    }
    if (!lower_is_datetime) {
        planned_value_deinit(out_lower_value);
        rc = convert_predicate_date_literal(
            database,
            lower_node,
            column,
            true,
            out_lower_value,
            NULL
        );
    }
    if (rc == MYLITE_OK && !upper_is_datetime) {
        planned_value_deinit(out_upper_value);
        rc = convert_predicate_date_literal(
            database,
            upper_node,
            column,
            true,
            out_upper_value,
            NULL
        );
    }

    return rc;
}

static int convert_predicate_date_in_value_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value **out_values,
    size_t *out_value_count,
    bool *out_compare_date_as_datetime
) {
    struct planned_value *values = NULL;
    size_t value_count = 0U;
    bool uses_datetime = false;
    int rc = MYLITE_OK;

    *out_values = NULL;
    *out_value_count = 0U;
    if (out_compare_date_as_datetime != NULL) {
        *out_compare_date_as_datetime = false;
    }
    if (value_list == NULL || value_list->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST ||
        mylite_sql_ast_node_child_count(value_list) == 0U) {
        set_unsupported_error(database, "WHERE supports only nonempty IN predicate lists");
        return MYLITE_ERROR;
    }
    value_count = mylite_sql_ast_node_child_count(value_list);
    if (value_count > SIZE_MAX / sizeof(*values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = calloc(value_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = convert_predicate_date_in_values(
        database,
        value_list,
        column,
        values,
        value_count,
        &uses_datetime
    );
    if (rc == MYLITE_OK && uses_datetime) {
        rc = convert_predicate_date_in_values_to_datetime(
            database,
            value_list,
            column,
            values,
            value_count
        );
    }
    if (rc != MYLITE_OK) {
        planned_value_array_deinit(values, value_count);
        free(values);
        return rc;
    }

    if (out_compare_date_as_datetime != NULL) {
        *out_compare_date_as_datetime = uses_datetime;
    }
    *out_values = values;
    *out_value_count = value_count;
    return MYLITE_OK;
}

static int convert_predicate_date_in_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *values,
    size_t value_count,
    bool *out_uses_datetime
) {
    const struct mylite_sql_ast_node *value_node = value_list->first_child;

    *out_uses_datetime = false;
    for (size_t value_index = 0U; value_index < value_count;
         ++value_index, value_node = value_node->next_sibling) {
        bool value_is_datetime = false;
        int rc = MYLITE_OK;

        if (predicate_comparison_value_is_null(value_node)) {
            values[value_index] = (struct planned_value){.is_null = true, .integer = 0};
            continue;
        }
        rc = convert_predicate_date_literal(
            database,
            value_node,
            column,
            false,
            &values[value_index],
            &value_is_datetime
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (value_is_datetime) {
            *out_uses_datetime = true;
        }
    }
    return MYLITE_OK;
}

static int convert_predicate_date_in_values_to_datetime(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *values,
    size_t value_count
) {
    const struct mylite_sql_ast_node *value_node = value_list->first_child;

    for (size_t value_index = 0U; value_index < value_count;
         ++value_index, value_node = value_node->next_sibling) {

        if (date_in_value_already_compares_as_datetime(value_node, &values[value_index])) {
            continue;
        }
        planned_value_deinit(&values[value_index]);
        int rc = convert_predicate_date_literal(
            database,
            value_node,
            column,
            true,
            &values[value_index],
            NULL
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static bool date_in_value_already_compares_as_datetime(
    const struct mylite_sql_ast_node *value_node,
    const struct planned_value *value
) {
    if (predicate_comparison_value_is_null(value_node)) {
        return true;
    }
    if (!value->is_text) {
        return false;
    }
    if (value->text_length != datetime_text_length) {
        return false;
    }
    return true;
}

static void planned_value_array_deinit(struct planned_value *values, size_t value_count) {
    for (size_t value_index = 0U; value_index < value_count; ++value_index) {
        planned_value_deinit(&values[value_index]);
    }
}

static int convert_predicate_enum_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;

    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        return MYLITE_OK;
    }
    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        literal = child_at(value_node, 0U);
    }
    if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
        return convert_enum_string_literal(
            database,
            value_node,
            column,
            1U,
            false,
            false,
            ENUM_STRING_PRESERVE_TRAILING_SPACES,
            out_value
        );
    }
    return convert_enum_integer_literal(database, value_node, column, 1U, false, out_value);
}

static int convert_predicate_set_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;

    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        return MYLITE_OK;
    }
    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        literal = child_at(value_node, 0U);
    }
    if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
        return convert_set_string_literal(
            database,
            literal,
            column,
            1U,
            false,
            false,
            false,
            out_value
        );
    }
    return convert_set_integer_literal(database, value_node, column, 1U, false, out_value);
}

static int convert_predicate_year_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    return convert_year_literal(database, value_node, column, 1U, false, out_value);
}

static int convert_predicate_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "WHERE string predicates support only string literals");
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal_with_policy(
        database,
        value_node,
        "WHERE string predicates support only string literals",
        "WHERE string predicate literals do not support NUL bytes",
        true,
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return assign_text_value(database, text, text_length, out_value);
}

static int convert_predicate_string_pattern_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        return MYLITE_OK;
    }
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "WHERE LIKE predicates support only string pattern literals"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal_with_policy(
        database,
        value_node,
        "WHERE LIKE predicates support only string pattern literals",
        "WHERE LIKE pattern literals do not support NUL bytes",
        true,
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return assign_text_value(database, text, text_length, out_value);
}

static int convert_predicate_regexp_pattern_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    bool case_sensitive,
    struct planned_value *out_value
) {
    struct mylite_regexp_program *program = NULL;
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "WHERE REGEXP predicates support only string pattern literals"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "WHERE REGEXP predicates support only string pattern literals",
        "WHERE REGEXP pattern literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_value_is_supported_string_key(text, text_length)) {
        free(text);
        set_unsupported_error(database, "WHERE REGEXP pattern literals support only ASCII text");
        return MYLITE_ERROR;
    }

    enum mylite_regexp_compile_status status =
        case_sensitive ? mylite_regexp_compile_ascii_cs(text, text_length, &program)
                       : mylite_regexp_compile_ascii_ci(text, text_length, &program);
    mylite_regexp_program_free(program);
    if (status != MYLITE_REGEXP_COMPILE_OK) {
        free(text);
        if (status == MYLITE_REGEXP_COMPILE_NOMEM) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        if (status == MYLITE_REGEXP_COMPILE_UNCLOSED_BRACKET) {
            set_regexp_error(
                database,
                "The regular expression contains an unclosed bracket expression."
            );
            return MYLITE_ERROR;
        }
        if (status == MYLITE_REGEXP_COMPILE_INVALID_RANGE) {
            set_regexp_character_range_error(
                database,
                "The regular expression contains an invalid character range."
            );
            return MYLITE_ERROR;
        }
        set_unsupported_error(
            database,
            "WHERE REGEXP patterns support only MyLite's baseline ASCII regular expression subset"
        );
        return MYLITE_ERROR;
    }

    return assign_text_value(database, text, text_length, out_value);
}
