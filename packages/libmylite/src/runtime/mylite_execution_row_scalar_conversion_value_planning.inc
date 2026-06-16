static int plan_row_scalar_non_concat_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_scalar_subquery,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *unwrapped = unwrap_parenthesized_expression(expression);

    if (is_row_scalar_control_flow_expression(unwrapped)) {
        return plan_row_scalar_control_flow_expression(
            database,
            unwrapped,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    return plan_row_scalar_non_control_flow_expression(
        database,
        unwrapped,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        allow_scalar_subquery,
        out_expression
    );
}

static int plan_row_scalar_non_control_flow_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_scalar_subquery,
    struct planned_row_scalar_expression *out_expression
) {
    const char *top_level_only_message = NULL;
    bool handled = false;
    int binary_rc = MYLITE_OK;
    int temporal_rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_literal_value(database, expression, out_expression);
    }
    if (generic_function_name_equals(expression, "GET_LOCK")) {
        return plan_row_scalar_get_lock_function(database, expression, out_expression);
    }
    if (generic_function_name_equals(expression, "RELEASE_LOCK")) {
        return plan_row_scalar_release_lock_function(database, expression, out_expression);
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
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
    if (row_scalar_conversion_kind_from_expression(expression) !=
        PLANNED_ROW_SCALAR_CONVERSION_NONE) {
        return plan_row_scalar_conversion_expression(
            database,
            expression,
            has_source,
            source_context,
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
    if (expression->kind == MYLITE_SQL_AST_ELT_FUNCTION) {
        return plan_row_scalar_elt_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
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
    temporal_rc = plan_row_scalar_temporal_function_expression(
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
        return temporal_rc;
    }
    if (expression->kind == MYLITE_SQL_AST_SCALAR_SUBQUERY) {
        if (!allow_scalar_subquery) {
            set_unsupported_error(
                database,
                "row-scalar SELECT supports scalar subqueries only as CONCAT() arguments"
            );
            return MYLITE_ERROR;
        }
        return plan_row_scalar_session_value(database, expression, out_expression);
    }
    if (expression->kind == MYLITE_SQL_AST_UUID_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR) {
        return plan_row_scalar_uuid_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION) {
        return plan_row_scalar_unix_timestamp_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_UNIX_TIMESTAMP_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "UNIX_TIMESTAMP");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_DATABASE_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_SCHEMA_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE ||
        expression->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR ||
        expression->kind == MYLITE_SQL_AST_SYSDATE_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR ||
        expression->kind == MYLITE_SQL_AST_CURRENT_DATE_VALUE ||
        expression->kind == MYLITE_SQL_AST_CURRENT_TIME_VALUE ||
        expression->kind == MYLITE_SQL_AST_UTC_DATE_VALUE ||
        expression->kind == MYLITE_SQL_AST_UTC_TIME_VALUE ||
        expression->kind == MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE ||
        expression->kind == MYLITE_SQL_AST_SYSTEM_VARIABLE) {
        return plan_row_scalar_session_value(database, expression, out_expression);
    }
    top_level_only_message = row_scalar_top_level_only_expression_message(expression);
    if (top_level_only_message != NULL) {
        set_unsupported_error(database, top_level_only_message);
        return MYLITE_ERROR;
    }
    binary_rc = plan_row_scalar_binary_string_expression(
        database,
        expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        out_expression,
        &handled
    );
    if (binary_rc != MYLITE_OK || handled) {
        return binary_rc;
    }
    if (!ast_subtree_has_column_reference(expression) &&
        is_scalar_projection_expression(expression)) {
        return plan_row_scalar_session_value(database, expression, out_expression);
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (!has_source) {
            char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
            char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
            size_t part_count = 0U;
            int rc = collect_column_reference_parts(database, expression, parts, &part_count);

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
        return plan_row_scalar_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }

    set_unsupported_error(
        database,
        "row-scalar SELECT supports only CONCAT(), CONCAT_WS(), FIELD(), GREATEST(), LEAST(), "
        "INTERVAL(), DATE_FORMAT(), descriptor columns, limited temporal extract, string length, "
        "string case, string trim, string codepoint, string slice, string search, string "
        "replacement, numeric functions, control-flow functions, UNIX_TIMESTAMP(), HEX(), "
        "JSON_VALID(), CHARSET(), COLLATION(), and COERCIBILITY() functions, "
        "literals, DATABASE(), and system variables"
    );
    return MYLITE_ERROR;
}

static bool row_scalar_expression_is_nested_value_argument(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (row_scalar_expression_is_concat_operator(expression) ||
        row_scalar_expression_is_string_function(expression->kind) ||
        is_row_scalar_json_expression(expression) ||
        row_scalar_numeric_function_kind_is_supported(expression->kind) ||
        row_scalar_numeric_function_kind_is_error(expression->kind) ||
        row_scalar_conversion_kind_from_expression(expression) !=
            PLANNED_ROW_SCALAR_CONVERSION_NONE ||
        is_row_scalar_control_flow_expression(expression)) {
        return true;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_CONCAT_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_WS_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_WS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ELT_FUNCTION:
    case MYLITE_SQL_AST_GREATEST_FUNCTION:
    case MYLITE_SQL_AST_GREATEST_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LEAST_FUNCTION:
    case MYLITE_SQL_AST_LEAST_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UUID_FUNCTION:
    case MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_IS_UUID_FUNCTION:
    case MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION:
    case MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION:
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION:
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MD5_FUNCTION:
    case MYLITE_SQL_AST_MD5_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SHA_FUNCTION:
    case MYLITE_SQL_AST_SHA_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SHA1_FUNCTION:
    case MYLITE_SQL_AST_SHA1_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SHA2_FUNCTION:
    case MYLITE_SQL_AST_SHA2_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_HEX_FUNCTION:
    case MYLITE_SQL_AST_TO_BASE64_FUNCTION:
    case MYLITE_SQL_AST_FROM_BASE64_FUNCTION:
    case MYLITE_SQL_AST_COMPRESS_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESS_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_RANDOM_BYTES_FUNCTION:
    case MYLITE_SQL_AST_UNHEX_FUNCTION:
    case MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION:
    case MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION:
    case MYLITE_SQL_AST_CHAR_FUNCTION:
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SYSDATE_FUNCTION:
    case MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CURRENT_DATE_VALUE:
    case MYLITE_SQL_AST_CURRENT_TIME_VALUE:
    case MYLITE_SQL_AST_UTC_DATE_VALUE:
    case MYLITE_SQL_AST_UTC_TIME_VALUE:
    case MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE:
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        return true;
    default:
        break;
    }
    return false;
}

static int plan_row_scalar_nested_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    return plan_row_scalar_non_concat_expression(
        database,
        expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        true,
        out_expression
    );
}

static int plan_row_scalar_integer_like_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    const char *range_message,
    struct planned_row_scalar_expression *out_expression
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (!has_source) {
            char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
            char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
            size_t part_count = 0U;

            rc = collect_column_reference_parts(database, expression, parts, &part_count);
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
        return plan_row_scalar_integer_like_column_argument(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            unsupported_message,
            out_expression
        );
    }
    if (row_scalar_expression_contains_integer_arithmetic_attempt(expression) ||
        (has_source && row_scalar_expression_is_integer_like_runtime_argument(expression))) {
        return plan_row_scalar_integer_arithmetic_expression(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (!has_source &&
        (row_scalar_numeric_function_kind_is_integer_like_runtime_argument(expression->kind) ||
         mylite_execution_string_length_function_kind(expression->kind) !=
             PLANNED_STRING_LENGTH_FUNCTION_NONE)) {
        return plan_row_scalar_non_concat_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            false,
            out_expression
        );
    }
    if (!mylite_execution_string_slice_length_argument_is_admitted(expression)) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    rc = mylite_execution_string_slice_signed_integer_value(
        database,
        expression,
        unsupported_message,
        range_message,
        &value,
        &is_null
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_literal_value(database, expression, out_expression);
    }
    return plan_row_scalar_integer_value(database, expression, out_expression);
}

static int plan_row_scalar_integer_like_column_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    size_t source_index = 0U;
    int rc = resolve_descriptor_column_reference_with_source_index(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT integer-like arguments support only descriptor columns",
        table_columns,
        table_column_count,
        &column,
        &source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (strcmp(column.physical_type, "INTEGER") != 0) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    out_expression->column_has_source_index = select_source_context_is_joined(source_context);
    out_expression->column_source_index = source_index;
    return MYLITE_OK;
}

static bool row_scalar_expression_is_integer_like_runtime_argument(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    return row_scalar_expression_contains_integer_arithmetic_attempt(expression) ||
           row_scalar_numeric_function_kind_is_integer_like_runtime_argument(expression->kind) ||
           mylite_execution_string_length_function_kind(expression->kind) !=
               PLANNED_STRING_LENGTH_FUNCTION_NONE ||
           expression->kind == MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION ||
           predicate_node_is_temporal_extract_expression(expression);
}

static bool row_scalar_numeric_function_kind_is_integer_like_runtime_argument(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (row_scalar_numeric_function_kind_from_ast(ast_kind)) {
    case PLANNED_ROW_SCALAR_NUMERIC_ABS:
    case PLANNED_ROW_SCALAR_NUMERIC_SIGN:
    case PLANNED_ROW_SCALAR_NUMERIC_CEIL:
    case PLANNED_ROW_SCALAR_NUMERIC_CEILING:
    case PLANNED_ROW_SCALAR_NUMERIC_FLOOR:
    case PLANNED_ROW_SCALAR_NUMERIC_ROUND:
    case PLANNED_ROW_SCALAR_NUMERIC_BIT_COUNT:
        return true;
    case PLANNED_ROW_SCALAR_NUMERIC_NONE:
    case PLANNED_ROW_SCALAR_NUMERIC_SQRT:
    case PLANNED_ROW_SCALAR_NUMERIC_DEGREES:
    case PLANNED_ROW_SCALAR_NUMERIC_RADIANS:
    case PLANNED_ROW_SCALAR_NUMERIC_ACOS:
    case PLANNED_ROW_SCALAR_NUMERIC_ASIN:
    case PLANNED_ROW_SCALAR_NUMERIC_ATAN:
    case PLANNED_ROW_SCALAR_NUMERIC_ATAN2:
    case PLANNED_ROW_SCALAR_NUMERIC_SIN:
    case PLANNED_ROW_SCALAR_NUMERIC_COS:
    case PLANNED_ROW_SCALAR_NUMERIC_TAN:
    case PLANNED_ROW_SCALAR_NUMERIC_COT:
    case PLANNED_ROW_SCALAR_NUMERIC_EXP:
    case PLANNED_ROW_SCALAR_NUMERIC_LN:
    case PLANNED_ROW_SCALAR_NUMERIC_LOG:
    case PLANNED_ROW_SCALAR_NUMERIC_LOG10:
    case PLANNED_ROW_SCALAR_NUMERIC_LOG2:
    case PLANNED_ROW_SCALAR_NUMERIC_POW:
    case PLANNED_ROW_SCALAR_NUMERIC_POWER:
        break;
    }
    return row_scalar_numeric_function_kind_is_error(ast_kind);
}

static int plan_row_scalar_get_lock_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    return plan_row_scalar_named_lock_function(
        database,
        expression,
        (struct row_scalar_named_lock_function){
            .function_name = "GET_LOCK",
            .argument_count = 2U,
            .result = 1,
        },
        out_expression
    );
}

static int plan_row_scalar_release_lock_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    return plan_row_scalar_named_lock_function(
        database,
        expression,
        (struct row_scalar_named_lock_function){
            .function_name = "RELEASE_LOCK",
            .argument_count = 1U,
            .result = 1,
        },
        out_expression
    );
}

static int plan_row_scalar_named_lock_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct row_scalar_named_lock_function lock_function,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *arguments =
        expression == NULL ? NULL : expression->first_child;

    while (arguments != NULL && arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        arguments = arguments->next_sibling;
    }

    if (arguments == NULL ||
        mylite_sql_ast_node_child_count(arguments) != lock_function.argument_count) {
        set_native_function_parameter_count_error(database, lock_function.function_name);
        return MYLITE_ERROR;
    }

    *out_expression = (struct planned_row_scalar_expression){
        .kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE,
        .value = (struct planned_value){.is_null = false, .integer = lock_function.result},
    };
    return MYLITE_OK;
}

static bool generic_function_name_equals(
    const struct mylite_sql_ast_node *expression,
    const char *name
) {
    const struct mylite_sql_ast_node *function_name = NULL;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_GENERIC_FUNCTION) {
        return false;
    }
    function_name = expression->first_child;
    while (function_name != NULL) {
        if (function_name->kind == MYLITE_SQL_AST_IDENTIFIER &&
            source_span_text_equals_ascii_case_insensitive(&function_name->span, name)) {
            return true;
        }
        function_name = function_name->next_sibling;
    }
    return false;
}

static const char *row_scalar_top_level_only_expression_message(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return NULL;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_INTERVAL_FUNCTION:
        return "row-scalar SELECT INTERVAL() is supported only as a top-level projection "
               "expression";
    default:
        break;
    }
    return NULL;
}

static int plan_row_scalar_conversion_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *current = unwrap_parenthesized_expression(expression);
    int rc = MYLITE_OK;

    if (row_scalar_conversion_kind_from_expression(current) == PLANNED_ROW_SCALAR_CONVERSION_NONE) {
        set_unsupported_error(database, "row-scalar SELECT CAST/CONVERT expression is unsupported");
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION;
    while (row_scalar_conversion_kind_from_expression(current) !=
           PLANNED_ROW_SCALAR_CONVERSION_NONE) {
        rc = append_row_scalar_conversion_step(database, current, out_expression);
        if (rc != MYLITE_OK) {
            return rc;
        }
        current = child_at(current, 0U);
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->conversion_kind = out_expression->conversion_steps[0].kind;
    out_expression->conversion_charset = out_expression->conversion_steps[0].charset;
    out_expression->conversion_collation = out_expression->conversion_steps[0].collation;
    out_expression->conversion_ascii_only = out_expression->conversion_steps[0].ascii_only;
    out_expression->argument_count = 1U;

    return plan_row_scalar_conversion_argument(
        database,
        current,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
}

static int append_row_scalar_conversion_step(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    struct planned_row_scalar_conversion_step step = {0};
    struct scalar_convert_charset_info charset_info = {0};
    enum planned_row_scalar_conversion_kind conversion_kind =
        row_scalar_conversion_kind_from_expression(expression);

    if (out_expression->conversion_step_count >= planned_row_scalar_conversion_step_capacity) {
        set_unsupported_error(database, "row-scalar SELECT CAST/CONVERT nesting is too deep");
        return MYLITE_ERROR;
    }
    if (conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_NONE) {
        set_unsupported_error(database, "row-scalar SELECT CAST/CONVERT expression is unsupported");
        return MYLITE_ERROR;
    }
    if (out_expression->conversion_step_count != 0U &&
        (row_scalar_conversion_kind_is_numeric(conversion_kind) ||
         row_scalar_conversion_kind_is_numeric(out_expression->conversion_steps[0].kind))) {
        set_unsupported_error(
            database,
            "row-scalar SELECT CAST/CONVERT nested numeric conversions are unsupported"
        );
        return MYLITE_ERROR;
    }
    if (conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_USING_CHARSET) {
        int rc = scalar_convert_charset_info_for_expression(database, expression, &charset_info);
        if (rc != MYLITE_OK) {
            return rc;
        }
    } else if (expression == NULL || mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(database, "row-scalar SELECT CAST/CONVERT expression is unsupported");
        return MYLITE_ERROR;
    }

    step.kind = conversion_kind;
    switch (conversion_kind) {
    case PLANNED_ROW_SCALAR_CONVERSION_CHAR:
        step.charset = database->session.character_set_connection;
        step.collation = database->session.collation_connection;
        break;
    case PLANNED_ROW_SCALAR_CONVERSION_USING_CHARSET:
        step.charset = charset_info.charset;
        step.collation = charset_info.collation;
        step.ascii_only = charset_info.ascii_only_value;
        break;
    case PLANNED_ROW_SCALAR_CONVERSION_BINARY:
    case PLANNED_ROW_SCALAR_CONVERSION_USING_BINARY:
        step.charset = "binary";
        step.collation = "binary";
        break;
    case PLANNED_ROW_SCALAR_CONVERSION_SIGNED:
    case PLANNED_ROW_SCALAR_CONVERSION_UNSIGNED:
    case PLANNED_ROW_SCALAR_CONVERSION_NONE:
        break;
    }

    out_expression->conversion_steps[out_expression->conversion_step_count] = step;
    ++out_expression->conversion_step_count;
    return MYLITE_OK;
}

static enum planned_row_scalar_conversion_kind row_scalar_conversion_kind_from_expression(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return PLANNED_ROW_SCALAR_CONVERSION_NONE;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
        return PLANNED_ROW_SCALAR_CONVERSION_BINARY;
    case MYLITE_SQL_AST_CAST_CHAR_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION:
        return PLANNED_ROW_SCALAR_CONVERSION_CHAR;
    case MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION:
        return PLANNED_ROW_SCALAR_CONVERSION_SIGNED;
    case MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_UNSIGNED_TYPE_EXPRESSION:
        return PLANNED_ROW_SCALAR_CONVERSION_UNSIGNED;
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        return PLANNED_ROW_SCALAR_CONVERSION_USING_BINARY;
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
        return PLANNED_ROW_SCALAR_CONVERSION_USING_CHARSET;
    default:
        break;
    }
    return PLANNED_ROW_SCALAR_CONVERSION_NONE;
}

static bool row_scalar_expression_is_explicit_binary_conversion(
    const struct mylite_sql_ast_node *expression
) {
    enum planned_row_scalar_conversion_kind conversion_kind =
        row_scalar_conversion_kind_from_expression(expression);

    return conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_BINARY ||
           conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_USING_BINARY;
}

static bool row_scalar_expression_contains_explicit_binary_conversion(
    const struct mylite_sql_ast_node *expression
) {
    struct scalar_arithmetic_node_stack stack = {0};
    bool found = false;

    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        return false;
    }
    while (stack.count != 0U && !found) {
        const struct mylite_sql_ast_node *current = stack.items[--stack.count];
        size_t child_count = 0U;

        current = unwrap_parenthesized_expression(current);
        if (current == NULL) {
            continue;
        }
        if (row_scalar_expression_is_explicit_binary_conversion(current)) {
            found = true;
            break;
        }
        if (current->kind == MYLITE_SQL_AST_SCALAR_SUBQUERY) {
            continue;
        }
        child_count = mylite_sql_ast_node_child_count(current);
        for (size_t child_index = 0U; child_index < child_count; ++child_index) {
            if (!scalar_arithmetic_node_stack_push(&stack, child_at(current, child_index))) {
                scalar_arithmetic_node_stack_deinit(&stack);
                return false;
            }
        }
    }
    scalar_arithmetic_node_stack_deinit(&stack);

    return found;
}

static bool row_scalar_conversion_kind_is_numeric(
    enum planned_row_scalar_conversion_kind conversion_kind
) {
    if (conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_SIGNED ||
        conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_UNSIGNED) {
        return true;
    }
    return false;
}

static int plan_row_scalar_conversion_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "row-scalar SELECT CAST/CONVERT supports only descriptor columns, scalar literals, "
            "and supported scalar values"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        struct mylite_catalog_column_descriptor column = {0};
        size_t source_index = 0U;
        int rc = MYLITE_OK;

        if (!has_source) {
            char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
            char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
            size_t part_count = 0U;

            rc = collect_column_reference_parts(database, expression, parts, &part_count);
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

        rc = resolve_descriptor_column_reference_with_source_index(
            database,
            expression,
            source_context,
            COLUMN_REFERENCE_FIELD,
            "row-scalar SELECT CAST/CONVERT supports only descriptor columns",
            table_columns,
            table_column_count,
            &column,
            &source_index
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (!row_scalar_conversion_column_descriptor_is_supported(database, &column)) {
            return MYLITE_ERROR;
        }

        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
        out_expression->column = column;
        out_expression->column_has_source_index = select_source_context_is_joined(source_context);
        out_expression->column_source_index = source_index;
        return MYLITE_OK;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_literal_value(database, expression, out_expression);
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
    }
    if (row_scalar_conversion_kind_from_expression(expression) !=
        PLANNED_ROW_SCALAR_CONVERSION_NONE) {
        return plan_row_scalar_conversion_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (mylite_execution_string_slice_function_kind(expression->kind) !=
        PLANNED_STRING_SLICE_FUNCTION_NONE) {
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
    if (expression->kind == MYLITE_SQL_AST_DATABASE_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_SCHEMA_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE ||
        expression->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR ||
        expression->kind == MYLITE_SQL_AST_SYSDATE_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR ||
        expression->kind == MYLITE_SQL_AST_CURRENT_DATE_VALUE ||
        expression->kind == MYLITE_SQL_AST_CURRENT_TIME_VALUE ||
        expression->kind == MYLITE_SQL_AST_UTC_DATE_VALUE ||
        expression->kind == MYLITE_SQL_AST_UTC_TIME_VALUE ||
        expression->kind == MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE ||
        expression->kind == MYLITE_SQL_AST_SYSTEM_VARIABLE) {
        return plan_row_scalar_session_value(database, expression, out_expression);
    }
    set_unsupported_error(
        database,
        "row-scalar SELECT CAST/CONVERT supports only descriptor columns, scalar literals, "
        "and supported scalar values"
    );
    return MYLITE_ERROR;
}

static bool row_scalar_conversion_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }
    if (column_descriptor_is_string_family(column)) {
        return true;
    }
    if (column_descriptor_is_binary_string_family(column) || column_descriptor_is_bit(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT CAST/CONVERT does not support binary string or BIT columns"
        );
        return false;
    }
    if (column_descriptor_is_decimal(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT CAST/CONVERT does not support DECIMAL columns"
        );
        return false;
    }
    if (column_descriptor_is_approximate(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT CAST/CONVERT does not support approximate numeric columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "row-scalar SELECT CAST/CONVERT supports only integer and nonbinary string columns"
    );
    return false;
}

static int plan_row_scalar_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(database, "row-scalar SELECT supports only scalar literals");
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_STRING:
        rc = decode_sql_string_literal(
            database,
            literal,
            "row-scalar SELECT supports only string literals",
            "row-scalar SELECT string literals do not support NUL bytes",
            &text,
            &text_length
        );
        if (rc == MYLITE_OK) {
            rc = assign_text_value(database, text, text_length, &out_expression->value);
        }
        return rc;
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return plan_row_scalar_integer_value(database, literal, out_expression);
    case MYLITE_SQL_AST_LITERAL_TRUE:
        out_expression->value = (struct planned_value){.is_null = false, .integer = 1};
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_FALSE:
        out_expression->value = (struct planned_value){.is_null = false, .integer = 0};
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_NULL:
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_BIT:
        return plan_row_scalar_binary_literal_value(database, literal, out_expression);
    default:
        break;
    }

    set_unsupported_error(
        database,
        "row-scalar SELECT supports only string, integer, boolean, NULL, hex, and bit literals"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_binary_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = MYLITE_OK;

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(database, "row-scalar SELECT supports only binary literals");
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_HEX) {
        rc = decode_binary_hex_literal(database, literal, &bytes, &byte_count);
    } else if (literal_kind == MYLITE_SQL_AST_LITERAL_BIT) {
        rc = decode_binary_bit_literal(database, &literal->span, &bytes, &byte_count);
    } else {
        set_unsupported_error(database, "row-scalar SELECT supports only binary literals");
        return MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        free(bytes);
        return rc;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    return assign_blob_value(database, bytes, byte_count, &out_expression->value);
}

static int plan_row_scalar_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *literal = expression;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int64_t value = 0;
    int rc = MYLITE_OK;

    if (expression != NULL && expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "row-scalar SELECT supports only signed integer literals"
            );
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(database, "row-scalar SELECT supports only signed integer literals");
        return MYLITE_ERROR;
    }

    rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
    if (rc != MYLITE_OK || (is_negative && magnitude > (uint64_t)INT64_MAX + 1U) ||
        (!is_negative && magnitude > (uint64_t)INT64_MAX)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT integer literals must fit the signed 64-bit range"
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

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    out_expression->value = (struct planned_value){.is_null = false, .integer = value};
    return MYLITE_OK;
}

static int plan_row_scalar_session_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    struct session_scalar_cell cell = {0};
    uint64_t magnitude = 0U;
    bool is_negative = false;
    int rc = session_scalar_value(database, expression, &cell);

    if (rc != MYLITE_OK) {
        return rc;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    if (cell.value == NULL) {
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        return MYLITE_OK;
    }
    if (is_scalar_arithmetic_projection_expression(expression) &&
        mylite_execution_parse_signed_integer_text(
            cell.value,
            strlen(cell.value),
            &magnitude,
            &is_negative
        ) == MYLITE_EXECUTION_DML_NUMERIC_PARSE_OK &&
        ((!is_negative && magnitude <= (uint64_t)INT64_MAX) ||
         (is_negative && magnitude <= (uint64_t)INT64_MAX + 1U))) {
        if (is_negative && magnitude == (uint64_t)INT64_MAX + 1U) {
            out_expression->value = (struct planned_value){.is_null = false, .integer = INT64_MIN};
        } else if (is_negative) {
            out_expression->value =
                (struct planned_value){.is_null = false, .integer = -(int64_t)magnitude};
        } else {
            out_expression->value =
                (struct planned_value){.is_null = false, .integer = (int64_t)magnitude};
        }
        return MYLITE_OK;
    }

    return copy_text_value(database, cell.value, &out_expression->value);
}

static int plan_row_scalar_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    size_t source_index = 0U;
    int rc = resolve_descriptor_column_reference_with_source_index(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT supports only descriptor columns",
        table_columns,
        table_column_count,
        &column,
        &source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!row_scalar_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    out_expression->column_has_source_index = select_source_context_is_joined(source_context);
    out_expression->column_source_index = source_index;
    return populate_row_scalar_column_effective_collation_metadata(
        database,
        source_context,
        &column,
        source_index,
        out_expression
    );
}

static int populate_row_scalar_column_effective_collation_metadata(
    struct mylite_db *database,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_table_descriptor single_source_table = {0};
    const struct mylite_catalog_table_descriptor *table = NULL;
    const char *charset = NULL;
    const char *collation = NULL;
    int rc = MYLITE_OK;

    rc = row_scalar_column_source_table(
        database,
        source_context,
        source_index,
        &single_source_table,
        &table
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    charset = column_effective_character_set_name(table, column);
    collation = column_effective_collation_name(table, column);
    if (charset != NULL) {
        snprintf(
            out_expression->column_effective_charset,
            sizeof(out_expression->column_effective_charset),
            "%s",
            charset
        );
    }
    if (collation != NULL) {
        snprintf(
            out_expression->column_effective_collation,
            sizeof(out_expression->column_effective_collation),
            "%s",
            collation
        );
    }
    return MYLITE_OK;
}

static int row_scalar_column_source_table(
    struct mylite_db *database,
    const struct select_source_context *source_context,
    size_t source_index,
    struct mylite_catalog_table_descriptor *single_source_table,
    const struct mylite_catalog_table_descriptor **out_table
) {
    bool found = false;
    int rc = MYLITE_OK;

    *out_table = NULL;
    if (source_context == NULL) {
        return MYLITE_OK;
    }
    if (select_source_context_is_joined(source_context)) {
        if (source_index < source_context->source_count &&
            !source_context->sources[source_index].is_derived) {
            *out_table = &source_context->sources[source_index].table;
        }
        return MYLITE_OK;
    }
    if (source_context->source == NULL) {
        return MYLITE_OK;
    }
    if (source_context->source->table_name[0] == '\0' ||
        source_context->source->schema.name[0] == '\0') {
        return MYLITE_OK;
    }

    rc = mylite_temporary_catalog_try_read_table_by_name(
        &database->session.temporary_catalog,
        source_context->source->schema.name,
        source_context->source->table_name,
        single_source_table,
        &found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read temporary table descriptor");
        return rc;
    }
    if (found) {
        *out_table = single_source_table;
        return MYLITE_OK;
    }

    rc = mylite_catalog_read_table_by_name(
        database,
        source_context->source->schema.schema_id,
        source_context->source->table_name,
        single_source_table
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read table descriptor");
        return rc;
    }
    *out_table = single_source_table;
    return MYLITE_OK;
}
