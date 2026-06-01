static int unix_timestamp_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    char *text = NULL;
    size_t text_length = 0U;
    bool is_null = false;
    bool result_is_null = false;
    int rc = MYLITE_OK;
    int written = 0;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION) {
        set_native_function_parameter_count_error(database, "UNIX_TIMESTAMP");
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(expression) == 0U) {
        written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRId64,
            current_timestamp_epoch(database)
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            set_runtime_error(database, "failed to format UNIX_TIMESTAMP() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "UNIX_TIMESTAMP");
        return MYLITE_ERROR;
    }

    rc = evaluate_unix_timestamp_scalar_argument(
        database,
        child_at(expression, 0U),
        &text,
        &text_length,
        &is_null
    );
    if (rc == MYLITE_OK && !is_null) {
        rc = mylite_unix_timestamp_value(
            database,
            text,
            text_length,
            MYLITE_UNIX_TIMESTAMP_INPUT_STRING,
            &out_cell->owned_text,
            &result_is_null
        );
    } else {
        result_is_null = true;
    }
    if (rc == MYLITE_OK && !result_is_null) {
        out_cell->value = out_cell->owned_text;
    }
    free(text);
    return rc;
}

static int evaluate_unix_timestamp_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "UNIX_TIMESTAMP() supports only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "UNIX_TIMESTAMP() supports only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "UNIX_TIMESTAMP() supports only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        expression,
        "UNIX_TIMESTAMP() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
        "descriptor columns, and NULL",
        "UNIX_TIMESTAMP() literals do not support NUL bytes",
        out_text,
        out_text_length
    );
    if (rc == MYLITE_OK && memchr(*out_text, '\0', *out_text_length) != NULL) {
        set_unsupported_error(database, "UNIX_TIMESTAMP() literals do not support NUL bytes");
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int timestamp_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    char *temporal_text = NULL;
    char *time_text = NULL;
    size_t temporal_text_length = 0U;
    size_t time_text_length = 0U;
    size_t child_count = 0U;
    bool temporal_is_null = false;
    bool time_is_null = false;
    bool result_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_TIMESTAMP_FUNCTION) {
        set_native_function_parameter_count_error(database, "TIMESTAMP");
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count != 1U && child_count != 2U) {
        set_native_function_parameter_count_error(database, "TIMESTAMP");
        return MYLITE_ERROR;
    }

    rc = evaluate_timestamp_scalar_temporal_argument(
        database,
        child_at(expression, 0U),
        &temporal_text,
        &temporal_text_length,
        &temporal_is_null
    );
    if (rc == MYLITE_OK && child_count == 2U) {
        rc = evaluate_timestamp_scalar_time_argument(
            database,
            child_at(expression, 1U),
            &time_text,
            &time_text_length,
            &time_is_null
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_timestamp_value(
            database,
            temporal_text,
            temporal_text_length,
            MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING,
            temporal_is_null,
            time_text,
            time_text_length,
            time_is_null,
            child_count == 2U,
            &out_cell->owned_text,
            &result_is_null
        );
    }
    if (rc == MYLITE_OK && !result_is_null) {
        out_cell->value = out_cell->owned_text;
    }

    free(temporal_text);
    free(time_text);
    return rc;
}

static int evaluate_timestamp_scalar_temporal_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "TIMESTAMP() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
            "descriptor columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "TIMESTAMP() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
            "descriptor columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "TIMESTAMP() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
            "descriptor columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        expression,
        "TIMESTAMP() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
        "descriptor columns, string descriptor columns, and NULL",
        "TIMESTAMP() literals do not support NUL bytes",
        out_text,
        out_text_length
    );
    if (rc == MYLITE_OK && memchr(*out_text, '\0', *out_text_length) != NULL) {
        set_unsupported_error(database, "TIMESTAMP() literals do not support NUL bytes");
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int evaluate_timestamp_scalar_time_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "TIMESTAMP() time argument supports only string time literals, TIME descriptor "
            "columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "TIMESTAMP() time argument supports only string time literals, TIME descriptor "
            "columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "TIMESTAMP() time argument supports only string time literals, TIME descriptor "
            "columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        expression,
        "TIMESTAMP() time argument supports only string time literals, TIME descriptor columns, "
        "string descriptor columns, and NULL",
        "TIMESTAMP() time literals do not support NUL bytes",
        out_text,
        out_text_length
    );
    if (rc == MYLITE_OK && memchr(*out_text, '\0', *out_text_length) != NULL) {
        set_unsupported_error(database, "TIMESTAMP() time literals do not support NUL bytes");
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int datediff_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    char *left_text = NULL;
    char *right_text = NULL;
    size_t left_text_length = 0U;
    size_t right_text_length = 0U;
    bool left_is_null = false;
    bool right_is_null = false;
    bool result_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_DATEDIFF_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "DATEDIFF");
        return MYLITE_ERROR;
    }

    rc = evaluate_datediff_scalar_argument(
        database,
        child_at(expression, 0U),
        &left_text,
        &left_text_length,
        &left_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_datediff_scalar_argument(
            database,
            child_at(expression, 1U),
            &right_text,
            &right_text_length,
            &right_is_null
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_datediff_value(
            database,
            left_text,
            left_text_length,
            MYLITE_DATEDIFF_INPUT_STRING,
            left_is_null,
            right_text,
            right_text_length,
            MYLITE_DATEDIFF_INPUT_STRING,
            right_is_null,
            &out_cell->owned_text,
            &result_is_null
        );
    }
    if (rc == MYLITE_OK && !result_is_null) {
        out_cell->value = out_cell->owned_text;
    }

    free(left_text);
    free(right_text);
    return rc;
}

static int evaluate_datediff_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "DATEDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
            "descriptor columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "DATEDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
            "descriptor columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "DATEDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
            "descriptor columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        expression,
        "DATEDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
        "descriptor columns, string descriptor columns, and NULL",
        "DATEDIFF() literals do not support NUL bytes",
        out_text,
        out_text_length
    );
    if (rc == MYLITE_OK && memchr(*out_text, '\0', *out_text_length) != NULL) {
        set_unsupported_error(database, "DATEDIFF() literals do not support NUL bytes");
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int timestampdiff_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum mylite_timestampdiff_unit unit = MYLITE_TIMESTAMPDIFF_UNIT_DAY;
    char *left_text = NULL;
    char *right_text = NULL;
    size_t left_text_length = 0U;
    size_t right_text_length = 0U;
    bool left_is_null = false;
    bool right_is_null = false;
    bool result_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_TIMESTAMPDIFF_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 3U) {
        set_runtime_error(database, "invalid TIMESTAMPDIFF() syntax");
        return MYLITE_ERROR;
    }

    rc = timestampdiff_function_unit_from_ast(database, child_at(expression, 0U), &unit);
    if (rc == MYLITE_OK && unit == MYLITE_TIMESTAMPDIFF_UNIT_MICROSECOND) {
        set_unsupported_error(database, "TIMESTAMPDIFF() does not yet support MICROSECOND");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = evaluate_timestampdiff_scalar_argument(
            database,
            child_at(expression, 1U),
            &left_text,
            &left_text_length,
            &left_is_null
        );
    }
    if (rc == MYLITE_OK && !left_is_null) {
        rc = evaluate_timestampdiff_scalar_argument(
            database,
            child_at(expression, 2U),
            &right_text,
            &right_text_length,
            &right_is_null
        );
    }
    if (rc == MYLITE_OK && !left_is_null) {
        rc = mylite_timestampdiff_value(
            database,
            unit,
            left_text,
            left_text_length,
            MYLITE_TIMESTAMPDIFF_INPUT_STRING,
            left_is_null,
            right_text,
            right_text_length,
            MYLITE_TIMESTAMPDIFF_INPUT_STRING,
            right_is_null,
            &out_cell->owned_text,
            &result_is_null
        );
    } else if (rc == MYLITE_OK) {
        result_is_null = true;
    }
    if (rc == MYLITE_OK && !result_is_null) {
        out_cell->value = out_cell->owned_text;
    }

    free(left_text);
    free(right_text);
    return rc;
}

static int timediff_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    char *left_text = NULL;
    char *right_text = NULL;
    size_t left_text_length = 0U;
    size_t right_text_length = 0U;
    bool left_is_null = false;
    bool right_is_null = false;
    bool result_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_TIMEDIFF_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "TIMEDIFF");
        return MYLITE_ERROR;
    }

    rc = evaluate_timediff_scalar_argument(
        database,
        child_at(expression, 0U),
        &left_text,
        &left_text_length,
        &left_is_null
    );
    if (rc == MYLITE_OK && !left_is_null) {
        rc = evaluate_timediff_scalar_argument(
            database,
            child_at(expression, 1U),
            &right_text,
            &right_text_length,
            &right_is_null
        );
    }
    if (rc == MYLITE_OK && !left_is_null) {
        rc = mylite_timediff_value(
            database,
            left_text,
            left_text_length,
            MYLITE_TIMEDIFF_INPUT_STRING,
            left_is_null,
            right_text,
            right_text_length,
            MYLITE_TIMEDIFF_INPUT_STRING,
            right_is_null,
            &out_cell->owned_text,
            &result_is_null
        );
    } else if (rc == MYLITE_OK) {
        result_is_null = true;
    }
    if (rc == MYLITE_OK && !result_is_null) {
        out_cell->value = out_cell->owned_text;
    }

    free(left_text);
    free(right_text);
    return rc;
}

static int timestampdiff_function_unit_from_ast(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *unit,
    enum mylite_timestampdiff_unit *out_unit
) {
    char unit_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int rc = MYLITE_OK;

    if (out_unit == NULL) {
        return MYLITE_MISUSE;
    }
    *out_unit = MYLITE_TIMESTAMPDIFF_UNIT_DAY;
    rc = copy_identifier_text(unit, unit_name, sizeof(unit_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (text_equals_ascii_case_insensitive(unit_name, "YEAR") ||
        text_equals_ascii_case_insensitive(unit_name, "SQL_TSI_YEAR")) {
        *out_unit = MYLITE_TIMESTAMPDIFF_UNIT_YEAR;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "QUARTER") ||
        text_equals_ascii_case_insensitive(unit_name, "SQL_TSI_QUARTER")) {
        *out_unit = MYLITE_TIMESTAMPDIFF_UNIT_QUARTER;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "MONTH") ||
        text_equals_ascii_case_insensitive(unit_name, "SQL_TSI_MONTH")) {
        *out_unit = MYLITE_TIMESTAMPDIFF_UNIT_MONTH;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "WEEK") ||
        text_equals_ascii_case_insensitive(unit_name, "SQL_TSI_WEEK")) {
        *out_unit = MYLITE_TIMESTAMPDIFF_UNIT_WEEK;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "DAY") ||
        text_equals_ascii_case_insensitive(unit_name, "SQL_TSI_DAY")) {
        *out_unit = MYLITE_TIMESTAMPDIFF_UNIT_DAY;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "HOUR") ||
        text_equals_ascii_case_insensitive(unit_name, "SQL_TSI_HOUR")) {
        *out_unit = MYLITE_TIMESTAMPDIFF_UNIT_HOUR;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "MINUTE") ||
        text_equals_ascii_case_insensitive(unit_name, "SQL_TSI_MINUTE")) {
        *out_unit = MYLITE_TIMESTAMPDIFF_UNIT_MINUTE;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "SECOND") ||
        text_equals_ascii_case_insensitive(unit_name, "SQL_TSI_SECOND")) {
        *out_unit = MYLITE_TIMESTAMPDIFF_UNIT_SECOND;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "MICROSECOND")) {
        *out_unit = MYLITE_TIMESTAMPDIFF_UNIT_MICROSECOND;
        return MYLITE_OK;
    }

    if (snprintf(message, sizeof(message), "TIMESTAMPDIFF() unit %s is not supported", unit_name) <
        0) {
        set_runtime_error(database, "failed to format TIMESTAMPDIFF() diagnostic");
        return MYLITE_ERROR;
    }
    set_unsupported_error(database, message);
    return MYLITE_ERROR;
}

static int evaluate_timestampdiff_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "TIMESTAMPDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
            "descriptor columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "TIMESTAMPDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
            "descriptor columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "TIMESTAMPDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
            "descriptor columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        expression,
        "TIMESTAMPDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
        "descriptor columns, string descriptor columns, and NULL",
        "TIMESTAMPDIFF() literals do not support NUL bytes",
        out_text,
        out_text_length
    );
    if (rc == MYLITE_OK && memchr(*out_text, '\0', *out_text_length) != NULL) {
        set_unsupported_error(database, "TIMESTAMPDIFF() literals do not support NUL bytes");
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int evaluate_timediff_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "TIMEDIFF() supports only string temporal literals, DATE, TIME, DATETIME, TIMESTAMP "
            "descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "TIMEDIFF() supports only string temporal literals, DATE, TIME, DATETIME, TIMESTAMP "
            "descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "TIMEDIFF() supports only string temporal literals, DATE, TIME, DATETIME, TIMESTAMP "
            "descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        expression,
        "TIMEDIFF() supports only string temporal literals, DATE, TIME, DATETIME, TIMESTAMP "
        "descriptor columns, and NULL",
        "TIMEDIFF() literals do not support NUL bytes",
        out_text,
        out_text_length
    );
    if (rc == MYLITE_OK && memchr(*out_text, '\0', *out_text_length) != NULL) {
        set_unsupported_error(database, "TIMEDIFF() literals do not support NUL bytes");
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int temporal_extract_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum mylite_temporal_extract_kind extract_kind = MYLITE_TEMPORAL_EXTRACT_DATE;
    const struct mylite_sql_ast_node *argument = NULL;
    int mode = 0;
    char *owned_text = NULL;
    const char *text = NULL;
    size_t text_length = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    rc = resolve_temporal_extract_call(database, expression, &extract_kind, &argument, &mode);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = evaluate_temporal_extract_scalar_argument(
        database,
        argument,
        extract_kind,
        &owned_text,
        &text,
        &text_length,
        &is_null
    );
    if (rc == MYLITE_OK) {
        rc = mylite_temporal_extract_value(
            database,
            text,
            text_length,
            extract_kind,
            MYLITE_TEMPORAL_EXTRACT_INPUT_STRING,
            mode,
            &out_cell->owned_text,
            &is_null
        );
    }
    if (rc == MYLITE_OK && !is_null) {
        out_cell->value = out_cell->owned_text;
    }

    free(owned_text);
    return rc;
}

static int resolve_temporal_extract_call(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_temporal_extract_kind *out_extract_kind,
    const struct mylite_sql_ast_node **out_argument,
    int *out_mode
) {
    size_t child_count = 0U;
    enum mylite_temporal_extract_kind extract_kind = MYLITE_TEMPORAL_EXTRACT_DATE;
    int mode = 0;
    int rc = MYLITE_OK;

    if (out_extract_kind == NULL || out_argument == NULL || out_mode == NULL) {
        return MYLITE_MISUSE;
    }
    *out_extract_kind = MYLITE_TEMPORAL_EXTRACT_DATE;
    *out_argument = NULL;
    *out_mode = 0;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(database, "temporal extract functions support exactly one argument");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_EXTRACT_FUNCTION) {
        if (mylite_sql_ast_node_child_count(expression) != 2U) {
            set_unsupported_error(database, "EXTRACT() supports exactly one unit and one value");
            return MYLITE_ERROR;
        }
        rc = extract_function_kind_from_unit(database, child_at(expression, 0U), &extract_kind);
        if (rc == MYLITE_OK) {
            *out_extract_kind = extract_kind;
            *out_argument = child_at(expression, 1U);
        }
        return rc;
    }
    if (!is_temporal_extract_function_kind(expression->kind)) {
        set_unsupported_error(database, "temporal extract functions support exactly one argument");
        return MYLITE_ERROR;
    }

    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count != 1U && child_count != 2U) {
        set_unsupported_error(database, "temporal extract functions support exactly one argument");
        return MYLITE_ERROR;
    }

    extract_kind = temporal_extract_function_kind(expression->kind);
    if (child_count == 2U) {
        if (extract_kind != MYLITE_TEMPORAL_EXTRACT_WEEK &&
            extract_kind != MYLITE_TEMPORAL_EXTRACT_YEARWEEK) {
            const char *function_name =
                extract_kind == MYLITE_TEMPORAL_EXTRACT_WEEKDAY ? "WEEKDAY" : "WEEKOFYEAR";

            set_native_function_parameter_count_error(database, function_name);
            return MYLITE_ERROR;
        }
        rc = evaluate_week_temporal_mode_argument(database, child_at(expression, 1U), &mode);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    *out_extract_kind = extract_kind;
    *out_argument = child_at(expression, 0U);
    *out_mode = mode;
    return MYLITE_OK;
}

static int extract_function_kind_from_unit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *unit,
    enum mylite_temporal_extract_kind *out_kind
) {
    char unit_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int rc = MYLITE_OK;

    if (out_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_kind = MYLITE_TEMPORAL_EXTRACT_DATE;
    rc = copy_identifier_text(unit, unit_name, sizeof(unit_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (text_equals_ascii_case_insensitive(unit_name, "YEAR")) {
        *out_kind = MYLITE_TEMPORAL_EXTRACT_YEAR;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "QUARTER")) {
        *out_kind = MYLITE_TEMPORAL_EXTRACT_QUARTER;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "MONTH")) {
        *out_kind = MYLITE_TEMPORAL_EXTRACT_MONTH;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "DAY")) {
        *out_kind = MYLITE_TEMPORAL_EXTRACT_DAY;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "HOUR")) {
        *out_kind = MYLITE_TEMPORAL_EXTRACT_SIGNED_HOUR;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "MINUTE")) {
        *out_kind = MYLITE_TEMPORAL_EXTRACT_SIGNED_MINUTE;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "SECOND")) {
        *out_kind = MYLITE_TEMPORAL_EXTRACT_SIGNED_SECOND;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "MICROSECOND")) {
        *out_kind = MYLITE_TEMPORAL_EXTRACT_SIGNED_MICROSECOND;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "YEAR_MONTH")) {
        *out_kind = MYLITE_TEMPORAL_EXTRACT_YEAR_MONTH;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "DAY_HOUR")) {
        *out_kind = MYLITE_TEMPORAL_EXTRACT_DAY_HOUR;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "DAY_MINUTE")) {
        *out_kind = MYLITE_TEMPORAL_EXTRACT_DAY_MINUTE;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "DAY_SECOND")) {
        *out_kind = MYLITE_TEMPORAL_EXTRACT_DAY_SECOND;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "HOUR_MINUTE")) {
        *out_kind = MYLITE_TEMPORAL_EXTRACT_HOUR_MINUTE;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "HOUR_SECOND")) {
        *out_kind = MYLITE_TEMPORAL_EXTRACT_HOUR_SECOND;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(unit_name, "MINUTE_SECOND")) {
        *out_kind = MYLITE_TEMPORAL_EXTRACT_MINUTE_SECOND;
        return MYLITE_OK;
    }

    if (snprintf(message, sizeof(message), "EXTRACT() unit %s is not yet supported", unit_name) <
        0) {
        set_runtime_error(database, "failed to format EXTRACT() diagnostic");
        return MYLITE_ERROR;
    }
    set_unsupported_error(database, message);
    return MYLITE_ERROR;
}

static int sec_to_time_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    int64_t seconds = 0;
    bool is_null = false;
    bool result_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "SEC_TO_TIME");
        return MYLITE_ERROR;
    }

    rc = evaluate_sec_to_time_scalar_argument(
        database,
        child_at(expression, 0U),
        &seconds,
        &is_null
    );
    if (rc == MYLITE_OK) {
        rc = mylite_sec_to_time_value(
            database,
            seconds,
            is_null,
            &out_cell->owned_text,
            &result_is_null
        );
    }
    if (rc == MYLITE_OK && !result_is_null) {
        out_cell->value = out_cell->owned_text;
    }
    return rc;
}

static int evaluate_sec_to_time_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_seconds,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (out_seconds == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_seconds = 0;
    *out_is_null = false;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "SEC_TO_TIME() supports only signed integer, boolean, and NULL literal arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            unwrap_parenthesized_expression(child_at(expression, 0U));

        if ((operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
             operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) ||
            literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            set_unsupported_error(
                database,
                "SEC_TO_TIME() supports only signed integer, boolean, and NULL literal arguments"
            );
            return MYLITE_ERROR;
        }
        return sec_to_time_scalar_integer_literal_value(
            database,
            literal,
            operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE,
            out_seconds
        );
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "SEC_TO_TIME() supports only signed integer, boolean, and NULL literal arguments"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_seconds = 1;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_seconds = 0;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "SEC_TO_TIME() supports only signed integer, boolean, and NULL literal arguments"
        );
        return MYLITE_ERROR;
    }

    return sec_to_time_scalar_integer_literal_value(database, expression, false, out_seconds);
}

static int sec_to_time_scalar_integer_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    int64_t *out_seconds
) {
    uint64_t magnitude = 0U;

    if (literal == NULL || out_seconds == NULL ||
        parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        (is_negative && magnitude > (uint64_t)INT64_MAX + 1U) ||
        (!is_negative && magnitude > (uint64_t)INT64_MAX)) {
        set_unsupported_error(
            database,
            "SEC_TO_TIME() integer literals must fit the signed 64-bit range"
        );
        return MYLITE_ERROR;
    }

    if (is_negative && magnitude == (uint64_t)INT64_MAX + 1U) {
        *out_seconds = INT64_MIN;
    } else if (is_negative) {
        *out_seconds = -(int64_t)magnitude;
    } else {
        *out_seconds = (int64_t)magnitude;
    }
    return MYLITE_OK;
}

static int from_unixtime_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    int64_t seconds = 0;
    bool is_null = false;
    bool result_is_null = false;
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION) {
        set_native_function_parameter_count_error(database, "FROM_UNIXTIME");
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count == 2U) {
        set_unsupported_error(database, "FROM_UNIXTIME() format arguments are not yet supported");
        return MYLITE_ERROR;
    }
    if (child_count != 1U) {
        set_native_function_parameter_count_error(database, "FROM_UNIXTIME");
        return MYLITE_ERROR;
    }

    rc = evaluate_from_unixtime_scalar_argument(
        database,
        child_at(expression, 0U),
        &seconds,
        &is_null
    );
    if (rc == MYLITE_OK) {
        rc = mylite_from_unixtime_value(
            database,
            seconds,
            is_null,
            &out_cell->owned_text,
            &result_is_null
        );
    }
    if (rc == MYLITE_OK && !result_is_null) {
        out_cell->value = out_cell->owned_text;
    }
    return rc;
}

static int temporal_constructor_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const char *function_name = NULL;
    int64_t first_value = 0;
    int64_t second_value = 0;
    int64_t third_value = 0;
    struct mylite_maketime_arguments maketime_arguments = {0};
    bool first_is_null = false;
    bool second_is_null = false;
    bool third_is_null = false;
    bool result_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || !is_temporal_constructor_function_kind(expression->kind)) {
        set_unsupported_error(database, "temporal constructor function is not supported");
        return MYLITE_ERROR;
    }
    function_name = temporal_constructor_function_name(expression->kind);
    if (mylite_sql_ast_node_child_count(expression) !=
        temporal_constructor_function_argument_count(expression->kind)) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }

    rc = evaluate_temporal_constructor_scalar_argument(
        database,
        child_at(expression, 0U),
        function_name,
        &first_value,
        &first_is_null
    );
    if (rc == MYLITE_OK && expression->kind == MYLITE_SQL_AST_FROM_DAYS_FUNCTION) {
        rc = mylite_from_days_value(
            database,
            first_value,
            first_is_null,
            &out_cell->owned_text,
            &result_is_null
        );
    } else if (rc == MYLITE_OK && expression->kind == MYLITE_SQL_AST_MAKEDATE_FUNCTION) {
        rc = evaluate_temporal_constructor_scalar_argument(
            database,
            child_at(expression, 1U),
            function_name,
            &second_value,
            &second_is_null
        );
        if (rc == MYLITE_OK) {
            rc = mylite_makedate_value(
                database,
                first_value,
                first_is_null,
                second_value,
                second_is_null,
                &out_cell->owned_text,
                &result_is_null
            );
        }
    } else if (rc == MYLITE_OK && expression->kind == MYLITE_SQL_AST_MAKETIME_FUNCTION) {
        rc = evaluate_temporal_constructor_scalar_argument(
            database,
            child_at(expression, 1U),
            function_name,
            &second_value,
            &second_is_null
        );
        if (rc == MYLITE_OK) {
            rc = evaluate_temporal_constructor_scalar_argument(
                database,
                child_at(expression, 2U),
                function_name,
                &third_value,
                &third_is_null
            );
        }
        if (rc == MYLITE_OK) {
            maketime_arguments.hour = first_value;
            maketime_arguments.hour_is_null = first_is_null;
            maketime_arguments.minute = second_value;
            maketime_arguments.minute_is_null = second_is_null;
            maketime_arguments.second = third_value;
            maketime_arguments.second_is_null = third_is_null;
            rc = mylite_maketime_value(
                database,
                &maketime_arguments,
                &out_cell->owned_text,
                &result_is_null
            );
        }
    }

    if (rc == MYLITE_OK && !result_is_null) {
        out_cell->value = out_cell->owned_text;
    }
    return rc;
}

static int period_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const char *function_name = NULL;
    int64_t first_value = 0;
    int64_t second_value = 0;
    int64_t result = 0;
    bool first_is_null = false;
    bool second_is_null = false;
    bool result_is_null = false;
    int written = 0;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL ||
        (expression->kind != MYLITE_SQL_AST_PERIOD_ADD_FUNCTION &&
         expression->kind != MYLITE_SQL_AST_PERIOD_DIFF_FUNCTION) ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "PERIOD_ADD");
        return MYLITE_ERROR;
    }

    function_name =
        expression->kind == MYLITE_SQL_AST_PERIOD_ADD_FUNCTION ? "PERIOD_ADD" : "PERIOD_DIFF";
    rc = evaluate_temporal_constructor_scalar_argument(
        database,
        child_at(expression, 0U),
        function_name,
        &first_value,
        &first_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_temporal_constructor_scalar_argument(
            database,
            child_at(expression, 1U),
            function_name,
            &second_value,
            &second_is_null
        );
    }
    if (rc == MYLITE_OK && expression->kind == MYLITE_SQL_AST_PERIOD_ADD_FUNCTION) {
        rc = mylite_period_add_value(
            database,
            first_value,
            first_is_null,
            second_value,
            second_is_null,
            &result,
            &result_is_null
        );
    } else if (rc == MYLITE_OK) {
        rc = mylite_period_diff_value(
            database,
            first_value,
            first_is_null,
            second_value,
            second_is_null,
            &result,
            &result_is_null
        );
    }
    if (rc != MYLITE_OK || result_is_null) {
        return rc;
    }

    written = snprintf(out_cell->integer_text, sizeof(out_cell->integer_text), "%" PRId64, result);
    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        set_runtime_error(database, "failed to format period function value");
        return MYLITE_ERROR;
    }
    out_cell->value = out_cell->integer_text;
    return MYLITE_OK;
}

static int convert_tz_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    char *datetime_text = NULL;
    char *from_tz_text = NULL;
    char *to_tz_text = NULL;
    size_t datetime_length = 0U;
    size_t from_tz_length = 0U;
    size_t to_tz_length = 0U;
    bool datetime_is_null = false;
    bool from_tz_is_null = false;
    bool to_tz_is_null = false;
    bool result_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONVERT_TZ_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 3U) {
        set_native_function_parameter_count_error(database, "CONVERT_TZ");
        return MYLITE_ERROR;
    }

    rc = evaluate_convert_tz_scalar_text_argument(
        database,
        child_at(expression, 0U),
        "CONVERT_TZ",
        &datetime_text,
        &datetime_length,
        &datetime_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_convert_tz_scalar_text_argument(
            database,
            child_at(expression, 1U),
            "CONVERT_TZ",
            &from_tz_text,
            &from_tz_length,
            &from_tz_is_null
        );
    }
    if (rc == MYLITE_OK) {
        rc = evaluate_convert_tz_scalar_text_argument(
            database,
            child_at(expression, 2U),
            "CONVERT_TZ",
            &to_tz_text,
            &to_tz_length,
            &to_tz_is_null
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_convert_tz_value(
            database,
            datetime_text,
            datetime_length,
            datetime_is_null,
            from_tz_text,
            from_tz_length,
            from_tz_is_null,
            to_tz_text,
            to_tz_length,
            to_tz_is_null,
            &out_cell->owned_text,
            &result_is_null
        );
    }
    if (rc == MYLITE_OK && !result_is_null) {
        out_cell->value = out_cell->owned_text;
    }

    free(datetime_text);
    free(from_tz_text);
    free(to_tz_text);
    return rc;
}

static int evaluate_from_unixtime_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_seconds,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (out_seconds == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_seconds = 0;
    *out_is_null = false;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "FROM_UNIXTIME() supports only signed integer, boolean, and NULL literal arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            unwrap_parenthesized_expression(child_at(expression, 0U));

        if ((operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
             operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) ||
            literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            set_unsupported_error(
                database,
                "FROM_UNIXTIME() supports only signed integer, boolean, and NULL literal arguments"
            );
            return MYLITE_ERROR;
        }
        return from_unixtime_scalar_integer_literal_value(
            database,
            literal,
            operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE,
            out_seconds
        );
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "FROM_UNIXTIME() supports only signed integer, boolean, and NULL literal arguments"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_seconds = 1;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_seconds = 0;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "FROM_UNIXTIME() supports only signed integer, boolean, and NULL literal arguments"
        );
        return MYLITE_ERROR;
    }

    return from_unixtime_scalar_integer_literal_value(database, expression, false, out_seconds);
}

static int from_unixtime_scalar_integer_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    int64_t *out_seconds
) {
    uint64_t magnitude = 0U;

    if (literal == NULL || out_seconds == NULL ||
        parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        (is_negative && magnitude > (uint64_t)INT64_MAX + 1U) ||
        (!is_negative && magnitude > (uint64_t)INT64_MAX)) {
        set_unsupported_error(
            database,
            "FROM_UNIXTIME() integer literals must fit the signed 64-bit range"
        );
        return MYLITE_ERROR;
    }

    if (is_negative && magnitude == (uint64_t)INT64_MAX + 1U) {
        *out_seconds = INT64_MIN;
    } else if (is_negative) {
        *out_seconds = -(int64_t)magnitude;
    } else {
        *out_seconds = (int64_t)magnitude;
    }
    return MYLITE_OK;
}

static int evaluate_temporal_constructor_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    int64_t *out_value,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char unsupported_message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

    if (out_value == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0;
    *out_is_null = false;

    if (snprintf(
            unsupported_message,
            sizeof(unsupported_message),
            "%s() supports only signed integer, boolean, and NULL literal arguments",
            function_name == NULL ? "temporal constructor" : function_name
        ) < 0) {
        set_runtime_error(database, "failed to format temporal constructor diagnostic");
        return MYLITE_ERROR;
    }

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            unwrap_parenthesized_expression(child_at(expression, 0U));

        if ((operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
             operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) ||
            literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            set_unsupported_error(database, unsupported_message);
            return MYLITE_ERROR;
        }
        return temporal_constructor_scalar_integer_literal_value(
            database,
            literal,
            operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE,
            function_name,
            out_value
        );
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_value = 1;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_value = 0;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    return temporal_constructor_scalar_integer_literal_value(
        database,
        expression,
        false,
        function_name,
        out_value
    );
}

static int evaluate_convert_tz_scalar_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char unsupported_message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = 0;
    int rc = MYLITE_OK;

    if (function_name == NULL || out_text == NULL || out_text_length == NULL ||
        out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    written = snprintf(
        unsupported_message,
        sizeof(unsupported_message),
        "%s() supports only string and NULL literal arguments",
        function_name
    );
    if (written < 0 || (size_t)written >= sizeof(unsupported_message)) {
        set_runtime_error(database, "failed to format CONVERT_TZ() unsupported message");
        return MYLITE_ERROR;
    }

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        expression,
        unsupported_message,
        "CONVERT_TZ() literals do not support NUL bytes",
        out_text,
        out_text_length
    );
    if (rc == MYLITE_OK && memchr(*out_text, '\0', *out_text_length) != NULL) {
        set_unsupported_error(database, "CONVERT_TZ() literals do not support NUL bytes");
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int temporal_constructor_scalar_integer_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    const char *function_name,
    int64_t *out_value
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    uint64_t magnitude = 0U;

    if (snprintf(
            message,
            sizeof(message),
            "%s() integer literals must fit the signed 64-bit range",
            function_name == NULL ? "temporal constructor" : function_name
        ) < 0) {
        set_runtime_error(database, "failed to format temporal constructor diagnostic");
        return MYLITE_ERROR;
    }

    if (literal == NULL || out_value == NULL ||
        parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        (is_negative && magnitude > (uint64_t)INT64_MAX + 1U) ||
        (!is_negative && magnitude > (uint64_t)INT64_MAX)) {
        set_unsupported_error(database, message);
        return MYLITE_ERROR;
    }

    if (is_negative && magnitude == (uint64_t)INT64_MAX + 1U) {
        *out_value = INT64_MIN;
    } else if (is_negative) {
        *out_value = -(int64_t)magnitude;
    } else {
        *out_value = (int64_t)magnitude;
    }
    return MYLITE_OK;
}

static int evaluate_temporal_extract_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_temporal_extract_kind extract_kind,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    const int is_calendar_name = (int)mylite_temporal_extract_kind_is_calendar_name(extract_kind);
    const int is_calendar_date = (int)mylite_temporal_extract_kind_is_calendar_date(extract_kind);
    const int is_week_temporal = (int)mylite_temporal_extract_kind_is_week_temporal(extract_kind);
    const char *unsupported_message =
        "temporal extract functions support only string temporal literals and NULL";
    const char *nul_message = "temporal extract literals do not support NUL bytes";

    if (is_calendar_name != 0) {
        unsupported_message =
            "calendar name functions support only string temporal literals and NULL";
        nul_message = "calendar name function literals do not support NUL bytes";
    } else if (is_week_temporal != 0) {
        unsupported_message = "week temporal functions support only string temporal literals and "
                              "NULL";
        nul_message = "week temporal function literals do not support NUL bytes";
    } else if (is_calendar_date != 0) {
        unsupported_message =
            "calendar date functions support only string temporal literals and NULL";
        nul_message = "calendar date function literals do not support NUL bytes";
    }

    if (out_owned_text == NULL || out_text == NULL || out_text_length == NULL ||
        out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    {
        int rc = decode_sql_string_literal(
            database,
            expression,
            unsupported_message,
            nul_message,
            out_owned_text,
            out_text_length
        );

        if (rc == MYLITE_OK && memchr(*out_owned_text, '\0', *out_text_length) != NULL) {
            set_unsupported_error(database, nul_message);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            *out_text = *out_owned_text;
        }
        return rc;
    }
}

static int evaluate_week_temporal_mode_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int *out_mode
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (out_mode == NULL) {
        return MYLITE_MISUSE;
    }
    *out_mode = 0;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "WEEK() and YEARWEEK() mode support is limited to integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            unwrap_parenthesized_expression(child_at(expression, 0U));

        if ((operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
             operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) ||
            literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            set_unsupported_error(
                database,
                "WEEK() and YEARWEEK() mode support is limited to integer, boolean, and NULL "
                "literals"
            );
            return MYLITE_ERROR;
        }
        return week_temporal_mode_integer_literal_value(
            database,
            literal,
            operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE,
            out_mode
        );
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "WEEK() and YEARWEEK() mode support is limited to integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_mode = 0;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_mode = 1;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_mode = 0;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "WEEK() and YEARWEEK() mode support is limited to integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }

    return week_temporal_mode_integer_literal_value(database, expression, false, out_mode);
}

static int week_temporal_mode_integer_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    int *out_mode
) {
    uint64_t magnitude = 0U;
    int64_t value = 0;

    if (literal == NULL || out_mode == NULL ||
        parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        (is_negative && magnitude > (uint64_t)INT64_MAX + 1U) ||
        (!is_negative && magnitude > (uint64_t)INT64_MAX)) {
        set_unsupported_error(
            database,
            "WEEK() and YEARWEEK() mode literals must fit the signed "
            "64-bit range"
        );
        return MYLITE_ERROR;
    }

    if (is_negative && magnitude == (uint64_t)INT64_MAX + 1U) {
        value = INT64_MIN;
    } else if (is_negative) {
        value = -(int64_t)magnitude;
    } else {
        value = (int64_t)magnitude;
    }
    *out_mode = (int)(value % week_temporal_mode_count);
    if (*out_mode < 0) {
        *out_mode += week_temporal_mode_count;
    }
    return MYLITE_OK;
}

static enum mylite_temporal_extract_kind temporal_extract_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_DATE_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_DATE;
    case MYLITE_SQL_AST_TIME_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_TIME;
    case MYLITE_SQL_AST_TIME_TO_SEC_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_TIME_TO_SEC;
    case MYLITE_SQL_AST_TO_DAYS_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_TO_DAYS;
    case MYLITE_SQL_AST_TO_SECONDS_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_TO_SECONDS;
    case MYLITE_SQL_AST_QUARTER_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_QUARTER;
    case MYLITE_SQL_AST_WEEK_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_WEEK;
    case MYLITE_SQL_AST_WEEKDAY_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_WEEKDAY;
    case MYLITE_SQL_AST_WEEKOFYEAR_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_WEEKOFYEAR;
    case MYLITE_SQL_AST_YEARWEEK_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_YEARWEEK;
    case MYLITE_SQL_AST_YEAR_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_YEAR;
    case MYLITE_SQL_AST_MONTH_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_MONTH;
    case MYLITE_SQL_AST_MONTHNAME_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_MONTHNAME;
    case MYLITE_SQL_AST_DAY_FUNCTION:
    case MYLITE_SQL_AST_DAYOFMONTH_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_DAY;
    case MYLITE_SQL_AST_DAYNAME_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_DAYNAME;
    case MYLITE_SQL_AST_DAYOFWEEK_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_DAYOFWEEK;
    case MYLITE_SQL_AST_DAYOFYEAR_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_DAYOFYEAR;
    case MYLITE_SQL_AST_LAST_DAY_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_LAST_DAY;
    case MYLITE_SQL_AST_HOUR_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_HOUR;
    case MYLITE_SQL_AST_MINUTE_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_MINUTE;
    case MYLITE_SQL_AST_SECOND_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_SECOND;
    case MYLITE_SQL_AST_MICROSECOND_FUNCTION:
        return MYLITE_TEMPORAL_EXTRACT_MICROSECOND;
    default:
        return MYLITE_TEMPORAL_EXTRACT_DATE;
    }
}

static bool is_temporal_extract_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_DATE_FUNCTION:
    case MYLITE_SQL_AST_TIME_FUNCTION:
    case MYLITE_SQL_AST_TIME_TO_SEC_FUNCTION:
    case MYLITE_SQL_AST_TO_DAYS_FUNCTION:
    case MYLITE_SQL_AST_TO_SECONDS_FUNCTION:
    case MYLITE_SQL_AST_QUARTER_FUNCTION:
    case MYLITE_SQL_AST_WEEK_FUNCTION:
    case MYLITE_SQL_AST_WEEKDAY_FUNCTION:
    case MYLITE_SQL_AST_WEEKOFYEAR_FUNCTION:
    case MYLITE_SQL_AST_YEARWEEK_FUNCTION:
    case MYLITE_SQL_AST_YEAR_FUNCTION:
    case MYLITE_SQL_AST_MONTH_FUNCTION:
    case MYLITE_SQL_AST_MONTHNAME_FUNCTION:
    case MYLITE_SQL_AST_DAY_FUNCTION:
    case MYLITE_SQL_AST_DAYNAME_FUNCTION:
    case MYLITE_SQL_AST_DAYOFMONTH_FUNCTION:
    case MYLITE_SQL_AST_DAYOFWEEK_FUNCTION:
    case MYLITE_SQL_AST_DAYOFYEAR_FUNCTION:
    case MYLITE_SQL_AST_LAST_DAY_FUNCTION:
    case MYLITE_SQL_AST_HOUR_FUNCTION:
    case MYLITE_SQL_AST_MINUTE_FUNCTION:
    case MYLITE_SQL_AST_SECOND_FUNCTION:
    case MYLITE_SQL_AST_MICROSECOND_FUNCTION:
        return true;
    default:
        return false;
    }
}
