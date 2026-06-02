static bool dml_allows_missing_default_adjustment(
    const struct mylite_db *database,
    bool ignore_errors
) {
    return (ignore_errors || !session_sql_mode_is_strict(database)) != 0;
}

static bool dml_allows_string_truncation_adjustment(
    const struct mylite_db *database,
    bool ignore_errors
) {
    return dml_allows_missing_default_adjustment(database, ignore_errors);
}

static int convert_insert_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
) {
    bool handled = false;
    int rc = MYLITE_OK;

    if (value_node == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
    if (insert_value_is_unix_timestamp_arithmetic(value_node)) {
        return convert_insert_unix_timestamp_arithmetic_value(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            allow_string_truncation_adjustment,
            out_value
        );
    }
    if (column_descriptor_is_auto_increment(column)) {
        return convert_auto_increment_insert_value(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (value_node->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE) {
        return materialize_dml_default_value(
            database,
            column,
            dml_allows_missing_default_adjustment(database, ignore_errors),
            out_value
        );
    }
    rc = convert_statement_time_value_for_column(database, value_node, column, out_value, &handled);
    if (handled) {
        return rc;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        return convert_null_insert_value(database, column, ignore_errors, out_value);
    }

    return convert_insert_value_by_descriptor(
        database,
        value_node,
        column,
        row_number,
        ignore_errors,
        allow_string_truncation_adjustment,
        out_value
    );
}

static bool insert_value_is_unix_timestamp_arithmetic(
    const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *left = NULL;
    const struct mylite_sql_ast_node *right = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;

    value_node = unwrap_parenthesized_expression(value_node);
    if (insert_unix_timestamp_now_node_is_admitted(value_node)) {
        return true;
    }
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        mylite_sql_ast_node_child_count(value_node) != 2U) {
        return false;
    }

    operator_kind = mylite_sql_ast_node_operator(value_node);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_ADD &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_SUBTRACT) {
        return false;
    }

    left = child_at(value_node, 0U);
    right = child_at(value_node, 1U);
    return (insert_unix_timestamp_now_node_is_admitted(left) &&
            insert_unix_timestamp_delta_node_is_admitted(right)) != 0;
}

static int convert_insert_unix_timestamp_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
) {
    static const struct unix_timestamp_arithmetic_messages messages = {
        .unsupported =
            "INSERT UNIX_TIMESTAMP arithmetic supports only UNIX_TIMESTAMP(), "
            "UNIX_TIMESTAMP() +/- signed integer literals, and UNIX_TIMESTAMP() +/- NULL",
        .delta_range =
            "INSERT UNIX_TIMESTAMP arithmetic supports only signed 64-bit integer deltas",
    };
    struct integer_column_range range = {0};
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    int rc = MYLITE_OK;

    if (column_descriptor_is_auto_increment(column)) {
        set_unsupported_error(
            database,
            "INSERT UNIX_TIMESTAMP arithmetic does not yet support AUTO_INCREMENT targets"
        );
        return MYLITE_ERROR;
    }

    if (!column_descriptor_is_string_family(column)) {
        rc = integer_range_for_column(
            database,
            column,
            "INSERT UNIX_TIMESTAMP arithmetic supports only integer and nonbinary string targets",
            &range
        );
        (void)range;
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    rc = evaluate_insert_unix_timestamp_arithmetic_value(database, value_node, &messages, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value.is_null) {
        return convert_null_insert_value(database, column, ignore_errors, out_value);
    }

    if (column_descriptor_is_string_family(column)) {
        return finish_insert_unix_timestamp_text_value(
            database,
            value.integer,
            column,
            row_number,
            allow_string_truncation_adjustment,
            out_value
        );
    }
    return finish_insert_unix_timestamp_integer_value(
        database,
        value.integer,
        column,
        row_number,
        ignore_errors,
        out_value
    );
}

static int evaluate_insert_unix_timestamp_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct unix_timestamp_arithmetic_messages *messages,
    struct scalar_arithmetic_value *out_value
) {
    const struct mylite_sql_ast_node *right = NULL;
    struct scalar_arithmetic_value delta = {.is_null = false, .integer = 0};
    int64_t result = 0;
    bool overflow = false;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    int rc = MYLITE_OK;

    if (messages == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){
        .is_null = false,
        .integer = current_timestamp_epoch(database),
    };

    value_node = unwrap_parenthesized_expression(value_node);
    if (insert_unix_timestamp_now_node_is_admitted(value_node)) {
        return MYLITE_OK;
    }
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        mylite_sql_ast_node_child_count(value_node) != 2U ||
        !insert_unix_timestamp_now_node_is_admitted(child_at(value_node, 0U))) {
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }

    operator_kind = mylite_sql_ast_node_operator(value_node);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_ADD &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_SUBTRACT) {
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }

    right = child_at(value_node, 1U);
    rc = evaluate_insert_unix_timestamp_delta(database, right, messages, &delta);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (delta.is_null) {
        out_value->is_null = true;
        out_value->integer = 0;
        return MYLITE_OK;
    }

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_ADD) {
        overflow = checked_int64_add(out_value->integer, delta.integer, &result);
    } else {
        overflow = checked_int64_subtract(out_value->integer, delta.integer, &result);
    }
    if (overflow) {
        set_scalar_arithmetic_overflow_error(database);
        return MYLITE_ERROR;
    }

    out_value->integer = result;
    return MYLITE_OK;
}

static int evaluate_insert_unix_timestamp_delta(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct unix_timestamp_arithmetic_messages *messages,
    struct scalar_arithmetic_value *out_value
) {
    const struct mylite_sql_ast_node *operand = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;

    if (messages == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){.is_null = false, .integer = 0};

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL) {
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL) {
        if (mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
            out_value->is_null = true;
            return MYLITE_OK;
        }
        return parse_insert_unix_timestamp_delta_literal(
            database,
            value_node,
            false,
            messages->delta_range,
            out_value
        );
    }
    if (value_node->kind != MYLITE_SQL_AST_UNARY_EXPRESSION) {
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }

    operator_kind = mylite_sql_ast_node_operator(value_node);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }

    operand = unwrap_parenthesized_expression(child_at(value_node, 0U));
    return parse_insert_unix_timestamp_delta_literal(
        database,
        operand,
        operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        messages->delta_range,
        out_value
    );
}

static int parse_insert_unix_timestamp_delta_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    const char *delta_range_message,
    struct scalar_arithmetic_value *out_value
) {
    static const uint64_t int64_min_magnitude = 9223372036854775808ULL;
    uint64_t limit = (uint64_t)INT64_MAX;
    uint64_t magnitude = 0U;

    if (is_negative) {
        limit = int64_min_magnitude;
    }

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER ||
        parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        magnitude > limit) {
        set_unsupported_error(database, delta_range_message);
        return MYLITE_ERROR;
    }

    if (is_negative && magnitude == int64_min_magnitude) {
        out_value->integer = INT64_MIN;
    } else if (is_negative) {
        out_value->integer = -(int64_t)magnitude;
    } else {
        out_value->integer = (int64_t)magnitude;
    }
    return MYLITE_OK;
}

static int finish_insert_unix_timestamp_integer_value(
    struct mylite_db *database,
    int64_t integer,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    static const uint64_t int64_min_magnitude = 9223372036854775808ULL;
    bool is_negative = integer < 0;
    uint64_t magnitude = 0U;

    if (integer == INT64_MIN) {
        magnitude = int64_min_magnitude;
    } else if (is_negative) {
        magnitude = (uint64_t)-integer;
    } else {
        magnitude = (uint64_t)integer;
    }

    out_value->is_null = false;
    return convert_integer_for_column_with_policy(
        database,
        magnitude,
        is_negative,
        column,
        row_number,
        ignore_errors,
        &out_value->integer
    );
}

static int finish_insert_unix_timestamp_text_value(
    struct mylite_db *database,
    int64_t integer,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    struct char_text_conversion conversion = {
        .text = NULL,
        .text_length = 0U,
        .row_number = row_number,
    };
    int rc = format_insert_unix_timestamp_integer_text(database, integer, &text, &text_length);

    if (rc != MYLITE_OK) {
        return rc;
    }
    conversion.text = text;
    conversion.text_length = text_length;

    if (column_descriptor_is_char(column)) {
        rc = convert_char_text(database, column, &conversion, allow_string_truncation_adjustment);
    } else if (column_descriptor_is_varchar(column)) {
        rc =
            convert_varchar_text(database, column, &conversion, allow_string_truncation_adjustment);
    } else if (column_descriptor_is_text_family(column)) {
        rc = convert_text_family_text(
            database,
            column,
            &conversion,
            allow_string_truncation_adjustment,
            false
        );
    } else {
        rc = MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        free(text);
        if (rc == MYLITE_ERROR && !diagnostics_has_error_condition(&database->diagnostics)) {
            set_unsupported_error(
                database,
                "INSERT UNIX_TIMESTAMP arithmetic supports only integer and nonbinary string "
                "targets"
            );
        }
        return rc;
    }

    return assign_text_value(database, text, conversion.text_length, out_value);
}

static int format_insert_unix_timestamp_integer_text(
    struct mylite_db *database,
    int64_t integer,
    char **out_text,
    size_t *out_text_length
) {
    char buffer[integer_text_capacity];
    char *text = NULL;
    int written = 0;

    if (out_text == NULL || out_text_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    written = snprintf(buffer, sizeof(buffer), "%" PRId64, integer);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        set_runtime_error(database, "failed to format INSERT UNIX_TIMESTAMP arithmetic value");
        return MYLITE_ERROR;
    }

    text = malloc((size_t)written + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(text, buffer, (size_t)written + 1U);
    *out_text = text;
    *out_text_length = (size_t)written;
    return MYLITE_OK;
}

static bool insert_unix_timestamp_now_node_is_admitted(
    const struct mylite_sql_ast_node *value_node
) {
    value_node = unwrap_parenthesized_expression(value_node);
    return (value_node != NULL && value_node->kind == MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION &&
            mylite_sql_ast_node_child_count(value_node) == 0U) != 0;
}

static bool insert_unix_timestamp_delta_node_is_admitted(
    const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *operand = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL) {
        return false;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(value_node);
        return (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
                literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
    }
    if (value_node->kind != MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return false;
    }

    operator_kind = mylite_sql_ast_node_operator(value_node);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        return false;
    }

    operand = unwrap_parenthesized_expression(child_at(value_node, 0U));
    return (operand != NULL && operand->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(operand) == MYLITE_SQL_AST_LITERAL_INTEGER) != 0;
}

static int convert_insert_value_for_plan(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
) {
    struct select_source_context source_context = {0};

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL || plan == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (column->is_generated) {
        if (value_node->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE) {
            planned_value_deinit(out_value);
            *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
            return MYLITE_OK;
        }
        set_generated_column_value_error(database, column->name, plan->table.name);
        return MYLITE_ERROR;
    }
    if (value_node->kind != MYLITE_SQL_AST_DEFAULT_FUNCTION) {
        return convert_insert_value(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            allow_string_truncation_adjustment,
            out_value
        );
    }

    (void)row_number;
    (void)ignore_errors;
    (void)allow_string_truncation_adjustment;
    source_context.source = &plan->target;
    return convert_default_function_value_for_target(
        database,
        value_node,
        &source_context,
        plan->columns,
        plan->column_count,
        column,
        ignore_errors,
        out_value
    );
}

static int convert_insert_value_by_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
) {
    if (column_descriptor_is_char(column)) {
        return convert_char_literal(
            database,
            value_node,
            column,
            row_number,
            allow_string_truncation_adjustment,
            out_value
        );
    }
    if (column_descriptor_is_varchar(column)) {
        return convert_varchar_literal(
            database,
            value_node,
            column,
            row_number,
            allow_string_truncation_adjustment,
            out_value
        );
    }
    if (column_descriptor_is_text_family(column)) {
        return convert_text_family_literal(
            database,
            value_node,
            column,
            row_number,
            allow_string_truncation_adjustment,
            false,
            out_value
        );
    }
    if (column_descriptor_is_json(column)) {
        (void)row_number;
        (void)ignore_errors;
        return convert_json_literal(database, value_node, column, out_value);
    }
    if (column_descriptor_is_enum(column)) {
        (void)ignore_errors;
        return convert_enum_literal(database, value_node, column, row_number, out_value);
    }
    if (column_descriptor_is_set(column)) {
        (void)ignore_errors;
        return convert_set_literal(database, value_node, column, row_number, out_value);
    }
    if (column_descriptor_is_binary_string_family(column)) {
        return convert_binary_string_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_bit(column)) {
        return convert_bit_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_year(column)) {
        return convert_year_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_decimal(column)) {
        return convert_decimal_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_approximate(column)) {
        return convert_approximate_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_date(column)) {
        return convert_date_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_time(column)) {
        return convert_time_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_datetime(column)) {
        return convert_datetime_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_timestamp(column)) {
        return convert_timestamp_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_spatial(column)) {
        (void)row_number;
        (void)ignore_errors;
        set_unsupported_error(database, "Spatial DML supports only NULL or DEFAULT values");
        return MYLITE_ERROR;
    }

    return convert_integer_literal(
        database,
        value_node,
        column,
        row_number,
        ignore_errors,
        out_value
    );
}

static int convert_auto_increment_insert_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    if (value_node == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE ||
        (value_node->kind == MYLITE_SQL_AST_LITERAL &&
         mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL)) {
        *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        return MYLITE_OK;
    }

    return convert_integer_literal(
        database,
        value_node,
        column,
        row_number,
        ignore_errors,
        out_value
    );
}

static int convert_null_insert_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool ignore_errors,
    struct planned_value *out_value
) {
    int rc = MYLITE_OK;

    if (column->is_nullable) {
        return MYLITE_OK;
    }
    if (!ignore_errors) {
        if (column_descriptor_is_spatial(column)) {
            set_spatial_bad_null_error(database, column->name);
            return MYLITE_ERROR;
        }
        set_bad_null_error(database, column->name);
        return MYLITE_ERROR;
    }

    rc = append_bad_null_warning(database, column->name);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_string_family(column)) {
        return make_empty_text_value(database, out_value);
    }
    if (column_descriptor_is_json(column)) {
        return make_json_null_value(database, out_value);
    }
    if (column_descriptor_is_enum(column)) {
        return make_empty_text_value(database, out_value);
    }
    if (column_descriptor_is_set(column)) {
        return make_empty_text_value(database, out_value);
    }
    if (column_descriptor_is_binary_blob_family(column)) {
        return make_empty_blob_value(database, out_value);
    }
    if (column_descriptor_is_binary_string_family(column)) {
        return make_implicit_binary_value(database, column, out_value);
    }
    if (column_descriptor_is_bit(column)) {
        return make_implicit_bit_value(database, column, out_value);
    }
    if (column_descriptor_is_year(column)) {
        return make_zero_year_value(database, out_value);
    }
    if (column_descriptor_is_decimal(column)) {
        struct decimal_type_info info = {0};

        rc = decimal_type_info_for_logical_type(column->logical_type, &info);
        if (rc == MYLITE_OK) {
            rc = build_decimal_zero_value(database, &info, out_value);
        }
        return rc;
    }
    if (column_descriptor_is_approximate(column)) {
        return make_zero_approximate_value(out_value);
    }
    if (column_descriptor_is_date(column)) {
        return make_zero_date_value(database, out_value);
    }
    if (column_descriptor_is_time(column)) {
        return make_zero_time_value(database, out_value);
    }
    if (column_descriptor_is_datetime(column)) {
        return make_zero_datetime_value(database, out_value);
    }
    if (column_descriptor_is_timestamp(column)) {
        return make_zero_timestamp_value(database, out_value);
    }

    out_value->is_null = false;
    out_value->is_text = false;
    out_value->integer = 0;
    return MYLITE_OK;
}
