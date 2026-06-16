static int plan_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    bool allow_scalar_subquery = true;
    bool handled = false;
    int rc = MYLITE_OK;

    planned_row_scalar_expression_deinit(out_expression);
    rc = normalize_row_scalar_expression(database, expression, &expression);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = plan_row_scalar_special_expression(
        database,
        expression,
        has_source,
        source_context,
        table,
        table_columns,
        table_column_count,
        out_expression,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }

    if (has_source && row_scalar_expression_contains_integer_arithmetic_attempt(expression)) {
        return plan_row_scalar_integer_arithmetic_expression(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }

    if (has_source && expression->kind != MYLITE_SQL_AST_SCALAR_SUBQUERY) {
        allow_scalar_subquery = false;
    }
    return plan_row_scalar_non_concat_expression(
        database,
        expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        allow_scalar_subquery,
        out_expression
    );
}

static int plan_row_scalar_special_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    bool *out_handled
) {
    bool handled = false;
    int rc = MYLITE_OK;

    *out_handled = true;
    if (expression->kind == MYLITE_SQL_AST_COLLATE_EXPRESSION) {
        return plan_row_scalar_collate_expression(
            database,
            expression,
            has_source,
            source_context,
            table,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (row_scalar_expression_is_concat_operator(expression)) {
        return plan_row_scalar_concat_operator_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_CONCAT_FUNCTION) {
        return plan_row_scalar_concat_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_COUNT_STAR_FUNCTION) {
        return plan_row_scalar_count_star_expression(
            database,
            expression,
            has_source,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION) {
        return plan_row_scalar_sum_column_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_CONCAT_WS_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "CONCAT_WS");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_CONCAT_WS_FUNCTION) {
        return plan_row_scalar_concat_ws_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_FIELD_FUNCTION) {
        return plan_row_scalar_field_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_GREATEST_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "GREATEST");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LEAST_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "LEAST");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_GREATEST_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_LEAST_FUNCTION) {
        return plan_row_scalar_greatest_least_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_INTERVAL_FUNCTION) {
        return plan_row_scalar_interval_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (row_scalar_numeric_function_kind_is_supported(expression->kind)) {
        return plan_row_scalar_numeric_function_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    rc = plan_row_scalar_temporal_function_expression(
        database,
        expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        out_expression,
        &handled
    );
    if (handled) {
        return rc;
    }
    if (mylite_execution_scalar_is_date_format_numeric_comparison_expression(expression)) {
        return plan_row_scalar_date_format_numeric_comparison_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (row_scalar_date_format_comparison_attempt(expression)) {
        set_unsupported_error(
            database,
            "DATE_FORMAT() numeric comparison supports only DATE_FORMAT(value, format) "
            "[=, <>, <, <=, >, >=] numeric_literal"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_QUOTE_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "QUOTE");
        return MYLITE_ERROR;
    }
    if (row_scalar_expression_is_string_function(expression->kind)) {
        return plan_row_scalar_string_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (is_row_scalar_json_expression(expression)) {
        return plan_row_scalar_json_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            COLUMN_REFERENCE_FIELD,
            out_expression
        );
    }
    rc = plan_row_scalar_binary_string_expression(
        database,
        expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        out_expression,
        &handled
    );
    if (handled) {
        return rc;
    }
    if (expression->kind == MYLITE_SQL_AST_DEFAULT_FUNCTION) {
        return plan_row_scalar_default_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (mylite_execution_is_charset_collation_function_kind(expression->kind)) {
        return plan_row_scalar_charset_collation_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            table,
            out_expression
        );
    }
    if (row_scalar_control_flow_condition_operator(expression) ==
        MYLITE_SQL_AST_OPERATOR_SOUNDS_LIKE) {
        return plan_row_scalar_sounds_like_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (row_scalar_control_flow_condition_operator(expression) == MYLITE_SQL_AST_OPERATOR_LIKE) {
        return plan_row_scalar_like_predicate_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (is_row_scalar_control_flow_expression(expression)) {
        return plan_row_scalar_control_flow_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (row_scalar_expression_is_window_function(expression)) {
        return plan_row_scalar_window_function_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (row_scalar_expression_is_rand_function(expression)) {
        return plan_row_scalar_rand_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    *out_handled = false;
    return MYLITE_OK;
}

static int plan_row_scalar_collate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *base = NULL;
    const char *collation_name = NULL;
    int rc = MYLITE_OK;

    (void)table;
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_COLLATE_EXPRESSION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_unsupported_error(database, "COLLATE supports only postfix row-scalar expressions");
        return MYLITE_ERROR;
    }

    base = child_at(expression, 0U);
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLLATE;
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->argument_count = 1U;

    rc = plan_row_scalar_expression(
        database,
        base,
        has_source,
        source_context,
        table,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = validate_row_scalar_collate_expression(
            database,
            expression,
            &out_expression->arguments[0],
            &collation_name
        );
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(out_expression);
        return rc;
    }

    out_expression->collation_name = collation_name;
    return MYLITE_OK;
}

static int validate_row_scalar_collate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct planned_row_scalar_expression *base_expression,
    const char **out_collation_name
) {
    const struct mylite_sql_ast_node *collation_node = NULL;
    const struct mylite_execution_catalog_scalar_collation *collation_info = NULL;
    const char *charset = NULL;
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (out_collation_name == NULL) {
        return MYLITE_MISUSE;
    }
    *out_collation_name = NULL;
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_COLLATE_EXPRESSION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_unsupported_error(database, "COLLATE supports only postfix row-scalar expressions");
        return MYLITE_ERROR;
    }

    collation_node = child_at(expression, 1U);
    rc = copy_table_option_name_text(
        database,
        collation_node,
        collation_name,
        sizeof(collation_name),
        (struct table_option_name_policy){
            .identifier_kind = "collation",
            .nul_message = "COLLATE names do not support NUL bytes",
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    collation_info = mylite_execution_scalar_collation_info_by_name(collation_name);
    if (collation_info == NULL) {
        set_unknown_collation_error(database, collation_name);
        return MYLITE_ERROR;
    }

    rc = row_scalar_collate_base_charset(
        database,
        child_at(expression, 0U),
        base_expression,
        &charset
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_equals_ascii_case_insensitive(charset, collation_info->charset)) {
        set_collation_not_valid_for_charset_error(
            database,
            collation_info->collation,
            charset
        );
        return MYLITE_ERROR;
    }

    *out_collation_name = collation_info->collation;
    return MYLITE_OK;
}

static int row_scalar_collate_base_charset(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *base_node,
    const struct planned_row_scalar_expression *base_expression,
    const char **out_charset
) {
    if (out_charset == NULL) {
        return MYLITE_MISUSE;
    }
    *out_charset = "binary";
    base_node = unwrap_parenthesized_expression(base_node);
    if (base_node == NULL || base_expression == NULL) {
        set_unsupported_error(
            database,
            "COLLATE supports only row-scalar values with known character set metadata"
        );
        return MYLITE_ERROR;
    }

    switch (base_expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        *out_charset = base_expression->value.is_text ? database->session.character_set_connection
                                                      : "binary";
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        if (column_descriptor_is_string_family(&base_expression->column) &&
            base_expression->column_effective_charset[0] != '\0') {
            *out_charset = base_expression->column_effective_charset;
            return MYLITE_OK;
        }
        if (column_descriptor_is_string_family(&base_expression->column) &&
            base_expression->column.character_set_name[0] != '\0') {
            *out_charset = base_expression->column.character_set_name;
            return MYLITE_OK;
        }
        *out_charset = "binary";
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
        *out_charset = database->session.character_set_connection;
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION: {
        const char *charset = row_scalar_conversion_result_charset(base_expression);

        if (charset == NULL) {
            set_unsupported_error(
                database,
                "COLLATE supports only character or binary CAST/CONVERT results"
            );
            return MYLITE_ERROR;
        }
        *out_charset = charset;
        return MYLITE_OK;
    }
    case PLANNED_ROW_SCALAR_EXPRESSION_COLLATE: {
        const struct mylite_execution_catalog_scalar_collation *collation_info =
            mylite_execution_scalar_collation_info_by_name(base_expression->collation_name);

        if (collation_info == NULL) {
            set_unknown_collation_error(database, base_expression->collation_name);
            return MYLITE_ERROR;
        }
        *out_charset = collation_info->charset;
        return MYLITE_OK;
    }
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_DIGEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
        *out_charset = database->session.character_set_connection;
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_COMPARISON:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_COMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESSED_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_RANDOM_BYTES:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_FUNCTION:
        break;
    }

    set_unsupported_error(
        database,
        "COLLATE supports only row-scalar values with known character set metadata"
    );
    return MYLITE_ERROR;
}

static const char *row_scalar_conversion_result_charset(
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION ||
        expression->conversion_step_count == 0U) {
        return NULL;
    }
    switch (expression->conversion_steps[0U].kind) {
    case PLANNED_ROW_SCALAR_CONVERSION_CHAR:
    case PLANNED_ROW_SCALAR_CONVERSION_USING_CHARSET:
    case PLANNED_ROW_SCALAR_CONVERSION_BINARY:
    case PLANNED_ROW_SCALAR_CONVERSION_USING_BINARY:
        return expression->conversion_steps[0U].charset;
    case PLANNED_ROW_SCALAR_CONVERSION_SIGNED:
    case PLANNED_ROW_SCALAR_CONVERSION_UNSIGNED:
    case PLANNED_ROW_SCALAR_CONVERSION_NONE:
        break;
    }
    return NULL;
}

static int plan_row_scalar_count_star_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    struct planned_row_scalar_expression *out_expression
) {
    if (!has_source) {
        set_no_tables_used_error(database);
        return MYLITE_ERROR;
    }
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_COUNT_STAR_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 0U) {
        set_unsupported_error(database, "row-scalar SELECT supports only COUNT(*) aggregates");
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR;
    return MYLITE_OK;
}

static int plan_row_scalar_sum_column_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *column_node = NULL;
    struct integer_column_range range;
    struct mylite_catalog_column_descriptor column = {0};
    size_t source_index = 0U;
    int rc = MYLITE_OK;

    if (!has_source) {
        set_no_tables_used_error(database);
        return MYLITE_ERROR;
    }
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(database, "row-scalar SELECT supports only SUM(column) aggregates");
        return MYLITE_ERROR;
    }

    column_node = child_at(expression, 0U);
    rc = resolve_descriptor_column_reference_with_source_index(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "SUM(column) supports only descriptor columns",
        table_columns,
        table_column_count,
        &column,
        &source_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = integer_range_for_column(
        database,
        &column,
        "SUM(column) supports only integer descriptor columns",
        &range
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN;
    out_expression->column = column;
    out_expression->column_has_source_index = select_source_context_is_joined(source_context);
    out_expression->column_source_index = source_index;
    return MYLITE_OK;
}

static int normalize_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_expression,
    const struct mylite_sql_ast_node **out_expression
) {
    const struct mylite_sql_ast_node *expression = source_expression;

    if (out_expression == NULL) {
        return MYLITE_MISUSE;
    }
    *out_expression = NULL;
    if (expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (row_scalar_expression_contains_any_value_function(expression) &&
        row_scalar_expression_contains_rand_function(expression)) {
        set_unsupported_error(database, "RAND() inside ANY_VALUE() is not supported");
        return MYLITE_ERROR;
    }
    expression = unwrap_any_value_function_expression(expression);
    if (expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_ANY_VALUE_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "ANY_VALUE");
        return MYLITE_ERROR;
    }
    *out_expression = expression;
    return MYLITE_OK;
}

static bool row_scalar_expression_is_rand_function(const struct mylite_sql_ast_node *expression) {
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_RAND_FUNCTION) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR) {
        return true;
    }
    return false;
}

static int plan_row_scalar_rand_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    size_t child_count = 0U;
    uint32_t seed = 0U;
    int rc = MYLITE_OK;

    if (out_expression == NULL) {
        return MYLITE_MISUSE;
    }
    planned_row_scalar_expression_deinit(out_expression);
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "RAND");
        return MYLITE_ERROR;
    }

    child_count = mylite_sql_ast_node_child_count(expression);
    if (expression->kind == MYLITE_SQL_AST_RAND_FUNCTION && child_count == 0U) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_RAND;
        out_expression->has_rand_seed = false;
        out_expression->rand_seed_is_expression = false;
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION && child_count == 1U) {
        const struct mylite_sql_ast_node *seed_expression = child_at(expression, 0U);

        if (row_scalar_rand_seed_is_descriptor_expression(seed_expression)) {
            return plan_row_scalar_rand_seed_expression(
                database,
                seed_expression,
                has_source,
                source_context,
                table_columns,
                table_column_count,
                out_expression
            );
        }

        rc = rand_seed_value(database, seed_expression, &seed);
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_RAND;
        out_expression->has_rand_seed = true;
        out_expression->rand_seed_is_expression = false;
        out_expression->rand_seed = seed;
        return MYLITE_OK;
    }

    set_unsupported_error(database, "RAND() supports only RAND() and RAND(seed)");
    return MYLITE_ERROR;
}

static int plan_row_scalar_rand_seed_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *seed_expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    if (!has_source) {
        set_unsupported_error(
            database,
            "RAND(seed) table seed expressions require a descriptor source"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_RAND;
    out_expression->has_rand_seed = true;
    out_expression->rand_seed_is_expression = true;
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->argument_count = 1U;

    rc = plan_row_scalar_conversion_expression(
        database,
        seed_expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(out_expression);
    }
    return rc;
}

static bool row_scalar_rand_seed_is_descriptor_expression(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (row_scalar_conversion_kind_from_expression(expression) ==
        PLANNED_ROW_SCALAR_CONVERSION_NONE) {
        return false;
    }
    return ast_subtree_has_column_reference(expression);
}

static bool ast_subtree_has_column_reference(const struct mylite_sql_ast_node *node) {
    struct scalar_arithmetic_node_stack stack = {0};
    bool has_reference = false;

    if (node != NULL && !scalar_arithmetic_node_stack_push(&stack, node)) {
        return true;
    }
    while (stack.count != 0U && !has_reference) {
        const struct mylite_sql_ast_node *current = stack.items[--stack.count];

        if (current->kind == MYLITE_SQL_AST_IDENTIFIER ||
            current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
            has_reference = true;
            break;
        }
        for (const struct mylite_sql_ast_node *child = current->first_child; child != NULL;
             child = child->next_sibling) {
            if (!scalar_arithmetic_node_stack_push(&stack, child)) {
                has_reference = true;
                break;
            }
        }
    }
    scalar_arithmetic_node_stack_deinit(&stack);
    return has_reference;
}

static bool row_scalar_expression_is_window_function(const struct mylite_sql_ast_node *expression) {
    return planned_window_function_kind_from_ast(expression) != PLANNED_WINDOW_FUNCTION_NONE;
}

static int plan_row_scalar_window_function_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum planned_window_function_kind kind = planned_window_function_kind_from_ast(expression);
    const struct mylite_sql_ast_node *arguments = window_function_argument_list_node(expression);
    const struct mylite_sql_ast_node *window_spec = window_function_spec_node(expression);
    const struct mylite_sql_ast_node *null_treatment =
        window_function_null_treatment_node(expression);
    const struct mylite_sql_ast_node *clause = NULL;
    const char *function_name = window_function_display_name(kind);
    int rc = MYLITE_OK;

    if (out_expression == NULL) {
        return MYLITE_MISUSE;
    }
    planned_row_scalar_expression_deinit(out_expression);
    if (kind == PLANNED_WINDOW_FUNCTION_NONE || function_name == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (null_treatment != NULL && null_treatment->kind == MYLITE_SQL_AST_WINDOW_IGNORE_NULLS) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            mysql_error_not_supported_yet,
            "42000",
            "This version of MySQL doesn't yet support 'IGNORE NULLS'"
        );
        return MYLITE_ERROR;
    }
    if (null_treatment != NULL && null_treatment->kind != MYLITE_SQL_AST_WINDOW_RESPECT_NULLS) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = plan_row_scalar_window_function_arguments(
        database,
        kind,
        arguments,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        out_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(out_expression);
        return rc;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION;
    out_expression->window_function_kind = kind;
    out_expression->window_order_direction = PLANNED_SELECT_ORDER_ASC;
    if (window_spec == NULL) {
        return MYLITE_OK;
    }
    if (window_spec->kind != MYLITE_SQL_AST_WINDOW_SPEC) {
        set_window_function_unsupported_error(
            database,
            function_name,
            "supports only a baseline window spec"
        );
        return MYLITE_ERROR;
    }
    if (!has_source && mylite_sql_ast_node_child_count(window_spec) != 0U) {
        set_window_function_unsupported_error(
            database,
            function_name,
            "without a table source supports only OVER ()"
        );
        return MYLITE_ERROR;
    }

    clause = child_at(window_spec, 0U);
    while (rc == MYLITE_OK && clause != NULL) {
        if (clause->kind == MYLITE_SQL_AST_WINDOW_PARTITION_CLAUSE) {
            rc = plan_row_scalar_window_partition_clause(
                database,
                function_name,
                clause,
                source_context,
                table_columns,
                table_column_count,
                out_expression
            );
        } else if (clause->kind == MYLITE_SQL_AST_WINDOW_ORDER_CLAUSE) {
            rc = plan_row_scalar_window_order_clause(
                database,
                function_name,
                clause,
                source_context,
                table_columns,
                table_column_count,
                out_expression
            );
        } else {
            set_window_function_unsupported_error(
                database,
                function_name,
                "supports only PARTITION BY and ORDER BY"
            );
            rc = MYLITE_ERROR;
        }
        clause = clause->next_sibling;
    }

    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(out_expression);
    }
    return rc;
}

static enum planned_window_function_kind planned_window_function_kind_from_ast(
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL) {
        return PLANNED_WINDOW_FUNCTION_NONE;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_ROW_NUMBER_FUNCTION:
        return PLANNED_WINDOW_FUNCTION_ROW_NUMBER;
    case MYLITE_SQL_AST_RANK_FUNCTION:
        return PLANNED_WINDOW_FUNCTION_RANK;
    case MYLITE_SQL_AST_DENSE_RANK_FUNCTION:
        return PLANNED_WINDOW_FUNCTION_DENSE_RANK;
    case MYLITE_SQL_AST_PERCENT_RANK_FUNCTION:
        return PLANNED_WINDOW_FUNCTION_PERCENT_RANK;
    case MYLITE_SQL_AST_CUME_DIST_FUNCTION:
        return PLANNED_WINDOW_FUNCTION_CUME_DIST;
    case MYLITE_SQL_AST_NTILE_FUNCTION:
        return PLANNED_WINDOW_FUNCTION_NTILE;
    case MYLITE_SQL_AST_LAG_FUNCTION:
        return PLANNED_WINDOW_FUNCTION_LAG;
    case MYLITE_SQL_AST_LEAD_FUNCTION:
        return PLANNED_WINDOW_FUNCTION_LEAD;
    case MYLITE_SQL_AST_FIRST_VALUE_FUNCTION:
        return PLANNED_WINDOW_FUNCTION_FIRST_VALUE;
    case MYLITE_SQL_AST_LAST_VALUE_FUNCTION:
        return PLANNED_WINDOW_FUNCTION_LAST_VALUE;
    case MYLITE_SQL_AST_NTH_VALUE_FUNCTION:
        return PLANNED_WINDOW_FUNCTION_NTH_VALUE;
    default:
        break;
    }
    return PLANNED_WINDOW_FUNCTION_NONE;
}

static const struct mylite_sql_ast_node *window_function_argument_list_node(
    const struct mylite_sql_ast_node *expression
) {
    enum planned_window_function_kind kind = planned_window_function_kind_from_ast(expression);

    if (!window_function_has_arguments(kind)) {
        return NULL;
    }
    return child_at(expression, 0U);
}

static const struct mylite_sql_ast_node *window_function_spec_node(
    const struct mylite_sql_ast_node *expression
) {
    enum planned_window_function_kind kind = planned_window_function_kind_from_ast(expression);

    if (!window_function_has_arguments(kind)) {
        return child_at(expression, 0U);
    }
    return child_at(expression, 1U);
}

static const struct mylite_sql_ast_node *window_function_null_treatment_node(
    const struct mylite_sql_ast_node *expression
) {
    enum planned_window_function_kind kind = planned_window_function_kind_from_ast(expression);

    if (!window_function_has_arguments(kind)) {
        return NULL;
    }
    return child_at(expression, 2U);
}

static bool window_function_has_arguments(enum planned_window_function_kind kind) {
    switch (kind) {
    case PLANNED_WINDOW_FUNCTION_NTILE:
    case PLANNED_WINDOW_FUNCTION_LAG:
    case PLANNED_WINDOW_FUNCTION_LEAD:
    case PLANNED_WINDOW_FUNCTION_FIRST_VALUE:
    case PLANNED_WINDOW_FUNCTION_LAST_VALUE:
    case PLANNED_WINDOW_FUNCTION_NTH_VALUE:
        return true;
    case PLANNED_WINDOW_FUNCTION_NONE:
    case PLANNED_WINDOW_FUNCTION_ROW_NUMBER:
    case PLANNED_WINDOW_FUNCTION_RANK:
    case PLANNED_WINDOW_FUNCTION_DENSE_RANK:
    case PLANNED_WINDOW_FUNCTION_PERCENT_RANK:
    case PLANNED_WINDOW_FUNCTION_CUME_DIST:
        break;
    }
    return false;
}

static int plan_row_scalar_window_function_arguments(
    struct mylite_db *database,
    enum planned_window_function_kind kind,
    const struct mylite_sql_ast_node *arguments,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    size_t argument_count = arguments == NULL ? 0U : mylite_sql_ast_node_child_count(arguments);
    int rc = validate_window_function_argument_count(
        database,
        (struct window_function_argument_count_request){
            .kind = kind,
            .argument_count = argument_count,
        }
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument_count == 0U) {
        return MYLITE_OK;
    }
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->argument_count = argument_count;

    const struct mylite_sql_ast_node *argument = child_at(arguments, 0U);
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < argument_count;
         ++argument_index) {
        rc = plan_row_scalar_window_function_argument(
            database,
            kind,
            argument_index,
            argument,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_index]
        );
        argument = argument == NULL ? NULL : argument->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = validate_window_function_integer_argument(database, kind, out_expression);
    }
    return rc;
}

static int validate_window_function_argument_count(
    struct mylite_db *database,
    struct window_function_argument_count_request request
) {
    const char *function_name = window_function_display_name(request.kind);
    size_t min_count = 0U;
    size_t max_count = 0U;

    switch (request.kind) {
    case PLANNED_WINDOW_FUNCTION_NTILE:
    case PLANNED_WINDOW_FUNCTION_FIRST_VALUE:
    case PLANNED_WINDOW_FUNCTION_LAST_VALUE:
        min_count = 1U;
        max_count = 1U;
        break;
    case PLANNED_WINDOW_FUNCTION_NTH_VALUE:
        min_count = 2U;
        max_count = 2U;
        break;
    case PLANNED_WINDOW_FUNCTION_LAG:
    case PLANNED_WINDOW_FUNCTION_LEAD:
        min_count = 1U;
        max_count = 3U;
        break;
    case PLANNED_WINDOW_FUNCTION_ROW_NUMBER:
    case PLANNED_WINDOW_FUNCTION_RANK:
    case PLANNED_WINDOW_FUNCTION_DENSE_RANK:
    case PLANNED_WINDOW_FUNCTION_PERCENT_RANK:
    case PLANNED_WINDOW_FUNCTION_CUME_DIST:
        min_count = 0U;
        max_count = 0U;
        break;
    case PLANNED_WINDOW_FUNCTION_NONE:
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (request.argument_count < min_count || request.argument_count > max_count ||
        function_name == NULL) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int plan_row_scalar_window_function_argument(
    struct mylite_db *database,
    enum planned_window_function_kind kind,
    size_t argument_index,
    const struct mylite_sql_ast_node *argument,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_argument
) {
    const struct mylite_sql_ast_node *unwrapped = unwrap_parenthesized_expression(argument);
    const char *function_name = window_function_display_name(kind);

    if (unwrapped == NULL || function_name == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (unwrapped->kind == MYLITE_SQL_AST_IDENTIFIER ||
        unwrapped->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (!has_source) {
            return set_unknown_window_argument_column_error(database, unwrapped);
        }
        return plan_row_scalar_window_function_column_argument(
            database,
            function_name,
            argument_index,
            unwrapped,
            source_context,
            table_columns,
            table_column_count,
            out_argument
        );
    }
    if (unwrapped->kind == MYLITE_SQL_AST_LITERAL ||
        (unwrapped->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
         window_function_signed_integer_argument_is_supported(kind, argument_index))) {
        if (unwrapped->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
            return plan_row_scalar_integer_value(database, unwrapped, out_argument);
        }
        return plan_row_scalar_literal_value(database, unwrapped, out_argument);
    }

    set_window_function_unsupported_error(
        database,
        function_name,
        "supports only descriptor-column and scalar-literal arguments"
    );
    return MYLITE_ERROR;
}

static bool window_function_signed_integer_argument_is_supported(
    enum planned_window_function_kind kind,
    size_t argument_index
) {
    return kind == PLANNED_WINDOW_FUNCTION_NTH_VALUE && argument_index == 1U;
}

static int set_unknown_window_argument_column_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = collect_column_reference_parts(database, argument, parts, &part_count);

    if (rc == MYLITE_OK) {
        rc = format_column_reference_name(
            database,
            parts,
            part_count,
            column_name,
            sizeof(column_name)
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    set_unknown_column_error(database, column_name);
    return MYLITE_ERROR;
}

static int plan_row_scalar_window_function_column_argument(
    struct mylite_db *database,
    const char *function_name,
    size_t argument_index,
    const struct mylite_sql_ast_node *argument,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_argument
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        argument,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "window functions support only descriptor-column arguments",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!window_function_argument_column_descriptor_is_supported(
            database,
            function_name,
            argument_index,
            &column
        )) {
        return MYLITE_ERROR;
    }

    out_argument->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_argument->column = column;
    return MYLITE_OK;
}

static bool window_function_argument_column_descriptor_is_supported(
    struct mylite_db *database,
    const char *function_name,
    size_t argument_index,
    const struct mylite_catalog_column_descriptor *column
) {
    if (argument_index != 0U) {
        set_window_function_unsupported_error(
            database,
            function_name,
            "offset and default arguments support only scalar literals"
        );
        return false;
    }
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }
    if (column_descriptor_is_decimal(column) || column_descriptor_is_string_family(column) ||
        column_descriptor_is_binary_string_family(column) || column_descriptor_is_bit(column) ||
        column_descriptor_is_date(column) || column_descriptor_is_time(column) ||
        column_descriptor_is_datetime(column) || column_descriptor_is_timestamp(column) ||
        column_descriptor_is_year(column)) {
        return true;
    }
    if (column_descriptor_is_approximate(column)) {
        set_window_function_unsupported_error(
            database,
            function_name,
            "arguments do not support approximate columns"
        );
        return false;
    }

    set_window_function_unsupported_error(
        database,
        function_name,
        "arguments support only integer, DECIMAL, string, binary string, BIT, YEAR, and temporal "
        "columns"
    );
    return false;
}

static int validate_window_function_integer_argument(
    struct mylite_db *database,
    enum planned_window_function_kind kind,
    const struct planned_row_scalar_expression *expression
) {
    size_t argument_index = window_function_integer_argument_index(kind);
    bool positive = false;

    if (argument_index == SIZE_MAX) {
        return MYLITE_OK;
    }
    positive = kind == PLANNED_WINDOW_FUNCTION_NTILE || kind == PLANNED_WINDOW_FUNCTION_NTH_VALUE;
    if (expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (argument_index >= expression->argument_count) {
        return MYLITE_OK;
    }

    const struct planned_row_scalar_expression *argument = &expression->arguments[argument_index];
    if (argument->kind == PLANNED_ROW_SCALAR_EXPRESSION_VALUE && argument->value.is_null &&
        window_function_null_integer_argument_is_syntax_error(kind)) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (argument->kind != PLANNED_ROW_SCALAR_EXPRESSION_VALUE || argument->value.is_null ||
        argument->value.is_text || argument->value.is_blob || argument->value.is_real ||
        argument->value.integer < (positive ? 1 : 0)) {
        set_incorrect_window_function_argument_error(database, kind);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static bool window_function_null_integer_argument_is_syntax_error(
    enum planned_window_function_kind kind
) {
    switch (kind) {
    case PLANNED_WINDOW_FUNCTION_NTILE:
    case PLANNED_WINDOW_FUNCTION_LAG:
    case PLANNED_WINDOW_FUNCTION_LEAD:
        return true;
    case PLANNED_WINDOW_FUNCTION_NONE:
    case PLANNED_WINDOW_FUNCTION_ROW_NUMBER:
    case PLANNED_WINDOW_FUNCTION_RANK:
    case PLANNED_WINDOW_FUNCTION_DENSE_RANK:
    case PLANNED_WINDOW_FUNCTION_PERCENT_RANK:
    case PLANNED_WINDOW_FUNCTION_CUME_DIST:
    case PLANNED_WINDOW_FUNCTION_FIRST_VALUE:
    case PLANNED_WINDOW_FUNCTION_LAST_VALUE:
    case PLANNED_WINDOW_FUNCTION_NTH_VALUE:
        break;
    }
    return false;
}

static size_t window_function_integer_argument_index(enum planned_window_function_kind kind) {
    switch (kind) {
    case PLANNED_WINDOW_FUNCTION_NTILE:
        return 0U;
    case PLANNED_WINDOW_FUNCTION_LAG:
    case PLANNED_WINDOW_FUNCTION_LEAD:
    case PLANNED_WINDOW_FUNCTION_NTH_VALUE:
        return 1U;
    case PLANNED_WINDOW_FUNCTION_NONE:
    case PLANNED_WINDOW_FUNCTION_ROW_NUMBER:
    case PLANNED_WINDOW_FUNCTION_RANK:
    case PLANNED_WINDOW_FUNCTION_DENSE_RANK:
    case PLANNED_WINDOW_FUNCTION_PERCENT_RANK:
    case PLANNED_WINDOW_FUNCTION_CUME_DIST:
    case PLANNED_WINDOW_FUNCTION_FIRST_VALUE:
    case PLANNED_WINDOW_FUNCTION_LAST_VALUE:
        break;
    }
    return SIZE_MAX;
}

static void set_incorrect_window_function_argument_error(
    struct mylite_db *database,
    enum planned_window_function_kind kind
) {
    const char *function_name = window_function_display_name(kind);
    char lowered[window_function_name_lower_capacity];
    char message[window_function_unsupported_message_capacity];
    int written = 0;

    if (copy_lowercase_function_name(function_name, lowered, sizeof(lowered)) != MYLITE_OK) {
        set_runtime_error(database, "failed to format window function argument diagnostic");
        return;
    }
    written = snprintf(message, sizeof(message), "Incorrect arguments to %s", lowered);
    if (written < 0 || (size_t)written >= sizeof(message)) {
        set_runtime_error(database, "failed to format window function argument diagnostic");
        return;
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_arguments,
        "HY000",
        message
    );
}

static int plan_row_scalar_window_partition_clause(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *partition_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = MYLITE_OK;

    if (out_expression->window_has_partition) {
        set_window_function_unsupported_error(
            database,
            function_name,
            "supports only one PARTITION BY key"
        );
        return MYLITE_ERROR;
    }

    rc = resolve_descriptor_column_reference(
        database,
        child_at(partition_clause, 0U),
        source_context,
        COLUMN_REFERENCE_FIELD,
        "window functions support only descriptor columns in PARTITION BY",
        table_columns,
        table_column_count,
        &column
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!window_key_column_descriptor_is_supported(database, function_name, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->window_has_partition = true;
    out_expression->window_partition_column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_window_order_clause(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *direction = child_at(order_clause, 1U);
    struct mylite_catalog_column_descriptor column = {0};
    int rc = MYLITE_OK;

    if (out_expression->window_has_order) {
        set_window_function_unsupported_error(
            database,
            function_name,
            "supports only one ORDER BY key"
        );
        return MYLITE_ERROR;
    }

    rc = resolve_descriptor_column_reference(
        database,
        child_at(order_clause, 0U),
        source_context,
        COLUMN_REFERENCE_ORDER,
        "window functions support only descriptor columns in ORDER BY",
        table_columns,
        table_column_count,
        &column
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!window_key_column_descriptor_is_supported(database, function_name, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->window_has_order = true;
    out_expression->window_order_column = column;
    out_expression->window_order_direction = PLANNED_SELECT_ORDER_ASC;
    if (mylite_sql_ast_node_order_direction(direction) == MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
        out_expression->window_order_direction = PLANNED_SELECT_ORDER_DESC;
    }
    return MYLITE_OK;
}

static bool window_key_column_descriptor_is_supported(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }
    if (column_descriptor_is_decimal(column) || column_descriptor_is_string_family(column) ||
        column_descriptor_is_binary_string_family(column) || column_descriptor_is_bit(column) ||
        column_descriptor_is_date(column) || column_descriptor_is_time(column) ||
        column_descriptor_is_datetime(column) || column_descriptor_is_timestamp(column) ||
        column_descriptor_is_year(column)) {
        return true;
    }
    if (column_descriptor_is_approximate(column)) {
        set_window_function_unsupported_error(
            database,
            function_name,
            "window keys do not support approximate columns"
        );
        return false;
    }

    set_window_function_unsupported_error(
        database,
        function_name,
        "window keys support only integer, DECIMAL, string, binary string, BIT, "
        "YEAR, and temporal columns"
    );
    return false;
}

static const char *window_function_display_name(enum planned_window_function_kind kind) {
    switch (kind) {
    case PLANNED_WINDOW_FUNCTION_ROW_NUMBER:
        return "ROW_NUMBER";
    case PLANNED_WINDOW_FUNCTION_RANK:
        return "RANK";
    case PLANNED_WINDOW_FUNCTION_DENSE_RANK:
        return "DENSE_RANK";
    case PLANNED_WINDOW_FUNCTION_PERCENT_RANK:
        return "PERCENT_RANK";
    case PLANNED_WINDOW_FUNCTION_CUME_DIST:
        return "CUME_DIST";
    case PLANNED_WINDOW_FUNCTION_NTILE:
        return "NTILE";
    case PLANNED_WINDOW_FUNCTION_LAG:
        return "LAG";
    case PLANNED_WINDOW_FUNCTION_LEAD:
        return "LEAD";
    case PLANNED_WINDOW_FUNCTION_FIRST_VALUE:
        return "FIRST_VALUE";
    case PLANNED_WINDOW_FUNCTION_LAST_VALUE:
        return "LAST_VALUE";
    case PLANNED_WINDOW_FUNCTION_NTH_VALUE:
        return "NTH_VALUE";
    case PLANNED_WINDOW_FUNCTION_NONE:
        break;
    }
    return NULL;
}

static int set_window_function_unsupported_error(
    struct mylite_db *database,
    const char *function_name,
    const char *detail
) {
    char message[window_function_unsupported_message_capacity];
    int written = snprintf(
        message,
        sizeof(message),
        "%s() %s",
        function_name == NULL ? "Window function" : function_name,
        detail == NULL ? "is not supported" : detail
    );

    if (written < 0 || (size_t)written >= sizeof(message)) {
        set_runtime_error(database, "failed to format window function diagnostic");
        return MYLITE_ERROR;
    }
    set_unsupported_error(database, message);
    return MYLITE_ERROR;
}

static int copy_lowercase_function_name(const char *source, char *destination, size_t size) {
    size_t length = source == NULL ? 0U : strlen(source);

    if (source == NULL || destination == NULL || size == 0U || length >= size) {
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; index < length; ++index) {
        destination[index] = (char)tolower((unsigned char)source[index]);
    }
    destination[length] = '\0';
    return MYLITE_OK;
}

static int plan_row_scalar_integer_arithmetic_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    struct row_scalar_integer_arithmetic_plan_stack stack = {0};
    int rc = MYLITE_OK;

    if (out_expression == NULL) {
        return MYLITE_MISUSE;
    }

    rc = row_scalar_integer_arithmetic_plan_stack_push(
        database,
        &stack,
        (struct row_scalar_integer_arithmetic_plan_frame){
            .expression = expression,
            .out_expression = out_expression,
        }
    );
    while (rc == MYLITE_OK && stack.count != 0U) {
        struct row_scalar_integer_arithmetic_plan_frame frame = stack.items[--stack.count];

        rc = plan_row_scalar_integer_arithmetic_frame(
            database,
            &frame,
            &stack,
            source_context,
            table_columns,
            table_column_count
        );
    }

    row_scalar_integer_arithmetic_plan_stack_deinit(&stack);
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(out_expression);
    }
    return rc;
}

static int plan_row_scalar_integer_arithmetic_frame(
    struct mylite_db *database,
    const struct row_scalar_integer_arithmetic_plan_frame *frame,
    struct row_scalar_integer_arithmetic_plan_stack *stack,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    const struct mylite_sql_ast_node *current = NULL;

    if (frame == NULL || frame->out_expression == NULL) {
        return MYLITE_MISUSE;
    }
    current = unwrap_parenthesized_expression(frame->expression);
    if (current == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    if (current->kind == MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return plan_row_scalar_integer_arithmetic_binary(
            database,
            current,
            frame->out_expression,
            stack
        );
    }
    if (current->kind == MYLITE_SQL_AST_MOD_FUNCTION) {
        return plan_row_scalar_integer_arithmetic_operator(
            database,
            current,
            MYLITE_SQL_AST_OPERATOR_MODULO,
            frame->out_expression,
            stack
        );
    }
    if (current->kind == MYLITE_SQL_AST_LITERAL ||
        current->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_arithmetic_literal(database, current, frame->out_expression);
    }
    if (current->kind == MYLITE_SQL_AST_IDENTIFIER ||
        current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return plan_row_scalar_integer_arithmetic_column(
            database,
            current,
            source_context,
            table_columns,
            table_column_count,
            frame->out_expression
        );
    }
    if (current->kind == MYLITE_SQL_AST_COUNT_STAR_FUNCTION) {
        return plan_row_scalar_count_star_expression(
            database,
            current,
            true,
            frame->out_expression
        );
    }
    if (row_scalar_numeric_function_kind_is_supported(current->kind)) {
        return plan_row_scalar_numeric_function_expression(
            database,
            current,
            true,
            source_context,
            table_columns,
            table_column_count,
            frame->out_expression
        );
    }
    if (current->kind == MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION) {
        return plan_row_scalar_unix_timestamp_expression(
            database,
            current,
            true,
            source_context,
            table_columns,
            table_column_count,
            frame->out_expression
        );
    }
    if (mylite_execution_string_length_function_kind(current->kind) !=
        PLANNED_STRING_LENGTH_FUNCTION_NONE) {
        return plan_row_scalar_string_length_expression(
            database,
            current,
            true,
            source_context,
            table_columns,
            table_column_count,
            COLUMN_REFERENCE_FIELD,
            frame->out_expression
        );
    }
    if (predicate_node_is_temporal_extract_expression(current)) {
        enum mylite_temporal_extract_kind extract_kind = MYLITE_TEMPORAL_EXTRACT_DATE;
        const struct mylite_sql_ast_node *argument = NULL;
        int mode = 0;
        int rc = mylite_execution_resolve_temporal_extract_call(
            database,
            current,
            &extract_kind,
            &argument,
            &mode
        );

        (void)argument;
        (void)mode;
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (!temporal_extract_predicate_kind_is_numeric(extract_kind)) {
            set_unsupported_error(
                database,
                "table-backed arithmetic expression supports only numeric temporal extractor "
                "functions"
            );
            return MYLITE_ERROR;
        }
        return plan_row_scalar_temporal_extract_expression(
            database,
            current,
            true,
            source_context,
            table_columns,
            table_column_count,
            frame->out_expression
        );
    }

    set_unsupported_error(
        database,
        "table-backed arithmetic expression supports only descriptor columns, numeric and string "
        "literals, booleans, NULL, numeric temporal extractor functions, parentheses, and "
        "+, binary -, *, /, DIV, and MOD operators"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_integer_arithmetic_binary(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression,
    struct row_scalar_integer_arithmetic_plan_stack *stack
) {
    return plan_row_scalar_integer_arithmetic_operator(
        database,
        expression,
        mylite_sql_ast_node_operator(expression),
        out_expression,
        stack
    );
}

static int plan_row_scalar_integer_arithmetic_operator(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind,
    struct planned_row_scalar_expression *out_expression,
    struct row_scalar_integer_arithmetic_plan_stack *stack
) {
    int rc = MYLITE_OK;

    if (!integer_arithmetic_operator_is_supported(operator_kind)) {
        set_unsupported_error(
            database,
            "table-backed arithmetic expression supports only +, binary -, *, /, DIV, and MOD "
            "operators"
        );
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC;
    out_expression->arithmetic_operator = operator_kind;
    out_expression->argument_count = 2U;

    rc = row_scalar_integer_arithmetic_plan_stack_push(
        database,
        stack,
        (struct row_scalar_integer_arithmetic_plan_frame){
            .expression = child_at(expression, 1U),
            .out_expression = &out_expression->arguments[1],
        }
    );
    if (rc == MYLITE_OK) {
        rc = row_scalar_integer_arithmetic_plan_stack_push(
            database,
            stack,
            (struct row_scalar_integer_arithmetic_plan_frame){
                .expression = child_at(expression, 0U),
                .out_expression = &out_expression->arguments[0],
            }
        );
    }
    return rc;
}

static void row_scalar_integer_arithmetic_plan_stack_deinit(
    struct row_scalar_integer_arithmetic_plan_stack *stack
) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    *stack = (struct row_scalar_integer_arithmetic_plan_stack){0};
}

static int row_scalar_integer_arithmetic_plan_stack_push(
    struct mylite_db *database,
    struct row_scalar_integer_arithmetic_plan_stack *stack,
    struct row_scalar_integer_arithmetic_plan_frame frame
) {
    struct row_scalar_integer_arithmetic_plan_frame *items = NULL;
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
        items = realloc(stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        stack->items = items;
        stack->capacity = capacity;
    }

    stack->items[stack->count] = frame;
    ++stack->count;
    return MYLITE_OK;
}

static int plan_row_scalar_integer_arithmetic_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *unwrapped = unwrap_parenthesized_expression(literal);

    if (unwrapped == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (unwrapped->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        const struct mylite_sql_ast_node *child =
            unwrap_parenthesized_expression(child_at(unwrapped, 0U));
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(unwrapped);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            set_unsupported_error(
                database,
                "table-backed arithmetic expression supports only signed numeric literal operands"
            );
            return MYLITE_ERROR;
        }
        if (child == NULL || child->kind != MYLITE_SQL_AST_LITERAL) {
            set_unsupported_error(
                database,
                "table-backed arithmetic expression supports unary signs only on numeric literals"
            );
            return MYLITE_ERROR;
        }
        if (mylite_sql_ast_node_literal_kind(child) == MYLITE_SQL_AST_LITERAL_INTEGER) {
            return plan_row_scalar_integer_value(database, unwrapped, out_expression);
        }
        if (mylite_sql_ast_node_literal_kind(child) == MYLITE_SQL_AST_LITERAL_DECIMAL ||
            mylite_sql_ast_node_literal_kind(child) == MYLITE_SQL_AST_LITERAL_FLOAT) {
            return plan_row_scalar_greatest_least_decimal_value(database, unwrapped, out_expression);
        }
        set_unsupported_error(
            database,
            "table-backed arithmetic expression supports unary signs only on numeric literals"
        );
        return MYLITE_ERROR;
    }
    if (unwrapped->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "table-backed arithmetic expression supports only numeric and string literals, "
            "booleans, and NULL operands"
        );
        return MYLITE_ERROR;
    }

    switch (mylite_sql_ast_node_literal_kind(unwrapped)) {
    case MYLITE_SQL_AST_LITERAL_STRING:
        return plan_row_scalar_literal_value(database, unwrapped, out_expression);
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return plan_row_scalar_integer_value(database, unwrapped, out_expression);
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        return plan_row_scalar_greatest_least_decimal_value(database, unwrapped, out_expression);
    case MYLITE_SQL_AST_LITERAL_TRUE:
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->value = (struct planned_value){.is_null = false, .integer = 1};
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_FALSE:
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->value = (struct planned_value){.is_null = false, .integer = 0};
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_NULL:
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        return MYLITE_OK;
    default:
        break;
    }

    set_unsupported_error(
        database,
        "table-backed arithmetic expression supports only numeric and string literals, booleans, "
        "and NULL operands"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_integer_arithmetic_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    size_t source_index = 0U;
    bool cast_as_integer = false;
    int rc = resolve_descriptor_column_reference_with_source_index(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "table-backed arithmetic expression supports only descriptor columns",
        table_columns,
        table_column_count,
        &column,
        &source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!signed_integer_arithmetic_column_descriptor_is_supported(
            database,
            &column,
            &cast_as_integer
        )) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    out_expression->column_cast_as_integer = cast_as_integer;
    out_expression->column_has_source_index = select_source_context_is_joined(source_context);
    out_expression->column_source_index = source_index;
    return MYLITE_OK;
}

static bool signed_integer_arithmetic_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool *out_cast_as_integer
) {
    if (out_cast_as_integer == NULL) {
        return false;
    }
    *out_cast_as_integer = false;
    if (!row_scalar_numeric_function_column_descriptor_is_supported(column)) {
        set_unsupported_error(
            database,
            "table-backed arithmetic expression supports only numeric and nonbinary string "
            "descriptor columns"
        );
        return false;
    }
    return true;
}

static int plan_row_scalar_temporal_function_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    bool *out_handled
) {
    const char *calendar_date_argument_count_error = NULL;
    int rc = MYLITE_OK;

    if (out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    rc = plan_row_scalar_date_function_expression(
        database,
        expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        out_expression,
        out_handled
    );
    if (*out_handled) {
        return rc;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_PERIOD_ADD_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "PERIOD_ADD");
        *out_handled = true;
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_PERIOD_DIFF_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "PERIOD_DIFF");
        *out_handled = true;
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_CONVERT_TZ_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "CONVERT_TZ");
        *out_handled = true;
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_PERIOD_ADD_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_DIFF_FUNCTION:
        *out_handled = true;
        return plan_row_scalar_period_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_CONVERT_TZ_FUNCTION:
        *out_handled = true;
        return plan_row_scalar_convert_tz_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "DATEDIFF");
        *out_handled = true;
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_TIMEDIFF_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "TIMEDIFF");
        *out_handled = true;
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_DATEDIFF_FUNCTION:
        *out_handled = true;
        return plan_row_scalar_datediff_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_TIMEDIFF_FUNCTION:
        *out_handled = true;
        return plan_row_scalar_timediff_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_TIMESTAMPDIFF_FUNCTION:
        *out_handled = true;
        return plan_row_scalar_timestampdiff_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_TIMESTAMP_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "TIMESTAMP");
        *out_handled = true;
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_TIMESTAMP_FUNCTION:
        *out_handled = true;
        return plan_row_scalar_timestamp_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION:
        *out_handled = true;
        return plan_row_scalar_unix_timestamp_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_SEC_TO_TIME_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "SEC_TO_TIME");
        *out_handled = true;
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION:
        *out_handled = true;
        return plan_row_scalar_sec_to_time_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_FROM_UNIXTIME_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "FROM_UNIXTIME");
        *out_handled = true;
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION:
        *out_handled = true;
        return plan_row_scalar_from_unixtime_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_FROM_DAYS_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "FROM_DAYS");
        *out_handled = true;
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_MAKEDATE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "MAKEDATE");
        *out_handled = true;
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_MAKETIME_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "MAKETIME");
        *out_handled = true;
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_FROM_DAYS_FUNCTION:
    case MYLITE_SQL_AST_MAKEDATE_FUNCTION:
    case MYLITE_SQL_AST_MAKETIME_FUNCTION:
        *out_handled = true;
        return plan_row_scalar_temporal_constructor_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_TIME_TO_SEC_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "TIME_TO_SEC");
        *out_handled = true;
        return MYLITE_ERROR;
    default:
        break;
    }

    calendar_date_argument_count_error =
        calendar_date_argument_count_error_function_name(expression->kind);
    if (calendar_date_argument_count_error != NULL) {
        set_native_function_parameter_count_error(database, calendar_date_argument_count_error);
        *out_handled = true;
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_EXTRACT_FUNCTION ||
        mylite_execution_is_temporal_extract_function_kind(expression->kind)) {
        *out_handled = true;
        return plan_row_scalar_temporal_extract_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }

    return MYLITE_OK;
}

static int plan_row_scalar_date_function_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    bool *out_handled
) {
    if (out_handled == NULL || expression == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = true;
    if (expression->kind == MYLITE_SQL_AST_DATE_FORMAT_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "DATE_FORMAT");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_TIME_FORMAT_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "TIME_FORMAT");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_STR_TO_DATE_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "STR_TO_DATE");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_DATE_FORMAT_FUNCTION) {
        return plan_row_scalar_date_format_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_GET_FORMAT_FUNCTION) {
        return plan_row_scalar_get_format_expression(database, expression, out_expression);
    }
    if (expression->kind == MYLITE_SQL_AST_TIME_FORMAT_FUNCTION) {
        return plan_row_scalar_time_format_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_STR_TO_DATE_FUNCTION) {
        return plan_row_scalar_str_to_date_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (mylite_execution_is_date_interval_second_function_kind(expression->kind)) {
        return plan_row_scalar_date_interval_second_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    *out_handled = false;
    return MYLITE_OK;
}

static const char *calendar_date_argument_count_error_function_name(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_DAYNAME_ARGUMENT_COUNT_ERROR:
        return "DAYNAME";
    case MYLITE_SQL_AST_DAYOFWEEK_ARGUMENT_COUNT_ERROR:
        return "DAYOFWEEK";
    case MYLITE_SQL_AST_DAYOFYEAR_ARGUMENT_COUNT_ERROR:
        return "DAYOFYEAR";
    case MYLITE_SQL_AST_LAST_DAY_ARGUMENT_COUNT_ERROR:
        return "LAST_DAY";
    case MYLITE_SQL_AST_TO_DAYS_ARGUMENT_COUNT_ERROR:
        return "TO_DAYS";
    case MYLITE_SQL_AST_TO_SECONDS_ARGUMENT_COUNT_ERROR:
        return "TO_SECONDS";
    case MYLITE_SQL_AST_MONTHNAME_ARGUMENT_COUNT_ERROR:
        return "MONTHNAME";
    case MYLITE_SQL_AST_WEEKDAY_ARGUMENT_COUNT_ERROR:
        return "WEEKDAY";
    case MYLITE_SQL_AST_WEEKOFYEAR_ARGUMENT_COUNT_ERROR:
        return "WEEKOFYEAR";
    case MYLITE_SQL_AST_YEARWEEK_ARGUMENT_COUNT_ERROR:
        return "YEARWEEK";
    default:
        return NULL;
    }
}

static bool row_scalar_expression_is_string_function(enum mylite_sql_ast_node_kind kind) {
    if (mylite_execution_is_string_length_function_kind(kind) ||
        mylite_execution_is_string_codepoint_function_kind(kind) ||
        mylite_execution_is_string_case_function_kind(kind) ||
        mylite_execution_is_string_trim_function_kind(kind) ||
        mylite_execution_is_string_slice_function_kind(kind) ||
        mylite_execution_is_string_padding_function_kind(kind) ||
        mylite_execution_is_string_bitmask_function_kind(kind) ||
        mylite_execution_is_string_search_function_kind(kind)) {
        return true;
    }

    switch (kind) {
    case MYLITE_SQL_AST_REVERSE_FUNCTION:
    case MYLITE_SQL_AST_SOUNDEX_FUNCTION:
    case MYLITE_SQL_AST_SOUNDEX_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_QUOTE_FUNCTION:
    case MYLITE_SQL_AST_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_INSERT_STRING_FUNCTION:
    case MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION:
    case MYLITE_SQL_AST_FIND_IN_SET_FUNCTION:
    case MYLITE_SQL_AST_STRCMP_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_INSTR_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_INSTR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REGEXP_SUBSTR_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_SUBSTR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REGEXP_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_REPLACE_ARGUMENT_COUNT_ERROR:
        return true;
    default:
        return false;
    }
}

static int plan_row_scalar_string_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    if (mylite_execution_is_string_length_function_kind(expression->kind)) {
        return plan_row_scalar_string_length_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            COLUMN_REFERENCE_FIELD,
            out_expression
        );
    }
    if (mylite_execution_is_string_codepoint_function_kind(expression->kind)) {
        return plan_row_scalar_string_codepoint_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (mylite_execution_is_string_case_function_kind(expression->kind)) {
        return plan_row_scalar_string_case_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (mylite_execution_is_string_trim_function_kind(expression->kind)) {
        return plan_row_scalar_string_trim_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (mylite_execution_is_string_slice_function_kind(expression->kind)) {
        return plan_row_scalar_string_slice_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            COLUMN_REFERENCE_FIELD,
            out_expression
        );
    }
    if (mylite_execution_is_string_padding_function_kind(expression->kind)) {
        return plan_row_scalar_string_padding_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (mylite_execution_is_string_bitmask_function_kind(expression->kind)) {
        return plan_row_scalar_string_bitmask_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (mylite_execution_is_string_search_function_kind(expression->kind)) {
        return plan_row_scalar_string_search_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_REPLACE_FUNCTION) {
        return plan_row_scalar_string_replace_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_INSERT_STRING_FUNCTION) {
        return plan_row_scalar_string_insert_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_REVERSE_FUNCTION) {
        return plan_row_scalar_string_reverse_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_SOUNDEX_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "SOUNDEX");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_SOUNDEX_FUNCTION) {
        return plan_row_scalar_soundex_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_QUOTE_FUNCTION) {
        return plan_row_scalar_string_quote_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION) {
        return plan_row_scalar_substring_index_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_FIND_IN_SET_FUNCTION) {
        return plan_row_scalar_find_in_set_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            COLUMN_REFERENCE_FIELD,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_STRCMP_FUNCTION) {
        return plan_row_scalar_strcmp_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (mylite_execution_regexp_string_function_argument_count_error_name(expression->kind) !=
        NULL) {
        set_native_function_parameter_count_error(
            database,
            mylite_execution_regexp_string_function_argument_count_error_name(expression->kind)
        );
        return MYLITE_ERROR;
    }
    if (mylite_execution_regexp_string_function_kind(expression->kind) !=
        PLANNED_REGEXP_STRING_FUNCTION_NONE) {
        return plan_row_scalar_regexp_string_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    return plan_row_scalar_regexp_like_expression(
        database,
        expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_FIELD,
        out_expression
    );
}

static int plan_row_scalar_default_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor source_column = {0};
    int rc = MYLITE_OK;

    if (!has_source) {
        return resolve_default_function_source_column(
            database,
            expression,
            NULL,
            NULL,
            0U,
            &source_column
        );
    }

    rc = resolve_default_function_source_column(
        database,
        expression,
        source_context,
        table_columns,
        table_column_count,
        &source_column
    );
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        rc = materialize_default_function_source_value(
            database,
            &source_column,
            &out_expression->value
        );
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(out_expression);
    }
    return rc;
}

static bool is_row_scalar_json_expression(const struct mylite_sql_ast_node *expression) {
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_JSON_VALID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_VALID_FUNCTION:
    case MYLITE_SQL_AST_JSON_CONTAINS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_CONTAINS_FUNCTION:
    case MYLITE_SQL_AST_JSON_CONTAINS_PATH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_CONTAINS_PATH_FUNCTION:
    case MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION:
    case MYLITE_SQL_AST_JSON_VALUE_FUNCTION:
    case MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_JSON_KEYS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_KEYS_FUNCTION:
    case MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_TYPE_FUNCTION:
    case MYLITE_SQL_AST_JSON_SET_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_SET_FUNCTION:
    case MYLITE_SQL_AST_JSON_INSERT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_INSERT_FUNCTION:
    case MYLITE_SQL_AST_JSON_REPLACE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_JSON_REMOVE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_REMOVE_FUNCTION:
    case MYLITE_SQL_AST_JSON_QUOTE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_QUOTE_FUNCTION:
    case MYLITE_SQL_AST_JSON_UNQUOTE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION:
    case MYLITE_SQL_AST_JSON_ARRAY_FUNCTION:
    case MYLITE_SQL_AST_JSON_OBJECT_FUNCTION:
        return true;
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        operator_kind = mylite_sql_ast_node_operator(expression);
        return (operator_kind == MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT ||
                operator_kind == MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT) != 0;
    default:
        break;
    }
    return false;
}

static int plan_row_scalar_json_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_JSON_VALID_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_VALID");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_VALID_FUNCTION:
        return plan_row_scalar_json_valid_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    case MYLITE_SQL_AST_JSON_CONTAINS_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_CONTAINS");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_CONTAINS_FUNCTION:
        return plan_row_scalar_json_contains_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    case MYLITE_SQL_AST_JSON_CONTAINS_PATH_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_CONTAINS_PATH");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_CONTAINS_PATH_FUNCTION:
        return plan_row_scalar_json_contains_path_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    case MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_EXTRACT");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION:
        return plan_row_scalar_json_extract_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    case MYLITE_SQL_AST_JSON_VALUE_FUNCTION:
        return plan_row_scalar_json_value_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    case MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_LENGTH");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_LENGTH_FUNCTION:
        return plan_row_scalar_json_length_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    case MYLITE_SQL_AST_JSON_KEYS_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_KEYS");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_KEYS_FUNCTION:
        return plan_row_scalar_json_keys_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    case MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_TYPE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_TYPE_FUNCTION:
        return plan_row_scalar_json_type_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    case MYLITE_SQL_AST_JSON_SET_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_SET");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_SET_FUNCTION:
    case MYLITE_SQL_AST_JSON_INSERT_FUNCTION:
    case MYLITE_SQL_AST_JSON_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_JSON_REMOVE_FUNCTION:
        return plan_row_scalar_json_set_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    case MYLITE_SQL_AST_JSON_INSERT_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_INSERT");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_REPLACE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_REPLACE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_REMOVE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_REMOVE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_QUOTE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_QUOTE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_QUOTE_FUNCTION:
        return plan_row_scalar_json_quote_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    case MYLITE_SQL_AST_JSON_UNQUOTE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_UNQUOTE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION:
        return plan_row_scalar_json_unquote_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    case MYLITE_SQL_AST_JSON_ARRAY_FUNCTION:
        return plan_row_scalar_json_array_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    case MYLITE_SQL_AST_JSON_OBJECT_FUNCTION:
        return plan_row_scalar_json_object_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return plan_row_scalar_json_extract_operator_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    default:
        break;
    }

    set_unsupported_error(database, "unsupported JSON row scalar expression");
    return MYLITE_ERROR;
}
