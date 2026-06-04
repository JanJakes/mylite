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
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
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
    if (expression->kind == MYLITE_SQL_AST_GREATEST_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_GREATEST_ARGUMENT_COUNT_ERROR ||
        expression->kind == MYLITE_SQL_AST_LEAST_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_LEAST_ARGUMENT_COUNT_ERROR) {
        set_unsupported_error(
            database,
            "row-scalar SELECT GREATEST() and LEAST() are supported only as top-level "
            "projection expressions"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_INTERVAL_FUNCTION) {
        set_unsupported_error(
            database,
            "row-scalar SELECT INTERVAL() is supported only as a top-level projection expression"
        );
        return MYLITE_ERROR;
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
        "replacement, control-flow functions, HEX(), JSON_VALID(), CHARSET(), COLLATION(), and "
        "COERCIBILITY() functions, "
        "literals, DATABASE(), and system variables"
    );
    return MYLITE_ERROR;
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

        rc = resolve_descriptor_column_reference(
            database,
            expression,
            source_context,
            COLUMN_REFERENCE_FIELD,
            "row-scalar SELECT CAST/CONVERT supports only descriptor columns",
            table_columns,
            table_column_count,
            &column
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (!row_scalar_conversion_column_descriptor_is_supported(database, &column)) {
            return MYLITE_ERROR;
        }

        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
        out_expression->column = column;
        return MYLITE_OK;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_literal_value(database, expression, out_expression);
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
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
    int rc = session_scalar_value(database, expression, &cell);

    if (rc != MYLITE_OK) {
        return rc;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    if (cell.value == NULL) {
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
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
    return MYLITE_OK;
}
