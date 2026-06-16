static int plan_row_scalar_string_reverse_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_REVERSE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "REVERSE");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE;
    out_expression->argument_count = 1U;

    rc = plan_row_scalar_string_reverse_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    return rc;
}

static int plan_row_scalar_string_reverse_argument(
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
            "REVERSE() supports only string, integer, boolean, NULL, session scalar, "
            "system variable, and descriptor column arguments"
        );
        return MYLITE_ERROR;
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
        return plan_row_scalar_string_reverse_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (has_source && (expression->kind == MYLITE_SQL_AST_RAND_FUNCTION ||
                       expression->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION)) {
        set_unsupported_error(
            database,
            "REVERSE() does not support RAND() arguments in table-backed SELECT"
        );
        return MYLITE_ERROR;
    }
    if (row_scalar_expression_is_nested_value_argument(expression)) {
        return plan_row_scalar_nested_value_argument(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (!mylite_execution_string_reverse_scalar_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "REVERSE() supports only string, integer, boolean, NULL, session scalar, "
            "system variable, and descriptor column arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_literal_value(database, expression, out_expression);
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
    }
    return plan_row_scalar_session_value(database, expression, out_expression);
}

static int plan_row_scalar_string_reverse_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT REVERSE() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!string_reverse_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool string_reverse_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }
    if (column_descriptor_is_decimal(column) || column_descriptor_is_string_family(column) ||
        column_descriptor_is_date(column) || column_descriptor_is_time(column) ||
        column_descriptor_is_datetime(column) || column_descriptor_is_timestamp(column) ||
        column_descriptor_is_year(column)) {
        return true;
    }
    if (column_descriptor_is_binary_string_family(column) || column_descriptor_is_bit(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT REVERSE() does not support binary string or BIT columns"
        );
        return false;
    }
    if (column_descriptor_is_approximate(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT REVERSE() does not support approximate numeric columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "row-scalar SELECT REVERSE() supports only integer, DECIMAL, nonbinary string, YEAR, "
        "and temporal columns"
    );
    return false;
}

static int plan_row_scalar_soundex_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SOUNDEX_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "SOUNDEX");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX;
    out_expression->argument_count = 1U;

    rc = plan_row_scalar_soundex_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    return rc;
}

static int plan_row_scalar_soundex_argument(
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
            "SOUNDEX() supports only string, integer, boolean, NULL, session scalar, "
            "system variable, and descriptor column arguments"
        );
        return MYLITE_ERROR;
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
        return plan_row_scalar_soundex_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (has_source && (expression->kind == MYLITE_SQL_AST_RAND_FUNCTION ||
                       expression->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION)) {
        set_unsupported_error(
            database,
            "SOUNDEX() does not support RAND() arguments in table-backed SELECT"
        );
        return MYLITE_ERROR;
    }
    if (row_scalar_expression_is_nested_value_argument(expression)) {
        return plan_row_scalar_nested_value_argument(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (!mylite_execution_soundex_scalar_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "SOUNDEX() supports only string, integer, boolean, NULL, session scalar, "
            "system variable, and descriptor column arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_literal_value(database, expression, out_expression);
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
    }
    return plan_row_scalar_session_value(database, expression, out_expression);
}

static int plan_row_scalar_soundex_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT SOUNDEX() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!soundex_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool soundex_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }
    if (column_descriptor_is_decimal(column) || column_descriptor_is_string_family(column) ||
        column_descriptor_is_date(column) || column_descriptor_is_time(column) ||
        column_descriptor_is_datetime(column) || column_descriptor_is_timestamp(column) ||
        column_descriptor_is_year(column)) {
        return true;
    }
    if (column_descriptor_is_binary_string_family(column) || column_descriptor_is_bit(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT SOUNDEX() does not support binary string or BIT columns"
        );
        return false;
    }
    if (column_descriptor_is_approximate(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT SOUNDEX() does not support approximate numeric columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "row-scalar SELECT SOUNDEX() supports only integer, DECIMAL, nonbinary string, YEAR, "
        "and temporal columns"
    );
    return false;
}

static int plan_row_scalar_string_quote_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_QUOTE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "QUOTE");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE;
    out_expression->argument_count = 1U;

    rc = plan_row_scalar_string_quote_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    return rc;
}

static int plan_row_scalar_string_quote_argument(
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
            "QUOTE() supports only string, integer, DECIMAL, boolean, NULL, session scalar, "
            "system variable, and descriptor column arguments"
        );
        return MYLITE_ERROR;
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
        return plan_row_scalar_string_quote_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (has_source && (expression->kind == MYLITE_SQL_AST_RAND_FUNCTION ||
                       expression->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION)) {
        set_unsupported_error(
            database,
            "QUOTE() does not support RAND() arguments in table-backed SELECT"
        );
        return MYLITE_ERROR;
    }
    if (row_scalar_expression_is_nested_value_argument(expression)) {
        return plan_row_scalar_nested_value_argument(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (!mylite_execution_quote_scalar_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "QUOTE() supports only string, integer, DECIMAL, boolean, NULL, session scalar, "
            "system variable, and descriptor column arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_string_quote_literal(database, expression, out_expression);
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
    }
    return plan_row_scalar_session_value(database, expression, out_expression);
}

static int plan_row_scalar_string_quote_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(database, "QUOTE() supports only scalar literals");
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_STRING:
        rc = decode_sql_string_literal_with_policy(
            database,
            literal,
            "QUOTE() supports only string literals",
            "QUOTE() string literal decoding failed",
            true,
            &text,
            &text_length
        );
        if (rc == MYLITE_OK) {
            rc = assign_text_value(database, text, text_length, &out_expression->value);
            if (rc == MYLITE_OK) {
                text = NULL;
            }
        }
        free(text);
        return rc;
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
        text = (char *)malloc(literal->span.length + 1U);
        if (text == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        memcpy(text, literal->span.text, literal->span.length);
        text[literal->span.length] = '\0';
        rc = assign_text_value(database, text, literal->span.length, &out_expression->value);
        if (rc == MYLITE_OK) {
            text = NULL;
        }
        free(text);
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
    default:
        break;
    }

    set_unsupported_error(
        database,
        "QUOTE() supports only string, integer, DECIMAL, boolean, and NULL literals"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_string_quote_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT QUOTE() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!string_quote_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool string_quote_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }
    if (column_descriptor_is_decimal(column) || column_descriptor_is_string_family(column) ||
        column_descriptor_is_date(column) || column_descriptor_is_time(column) ||
        column_descriptor_is_datetime(column) || column_descriptor_is_timestamp(column) ||
        column_descriptor_is_year(column)) {
        return true;
    }
    if (column_descriptor_is_binary_string_family(column) || column_descriptor_is_bit(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT QUOTE() does not support binary string or BIT columns"
        );
        return false;
    }
    if (column_descriptor_is_approximate(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT QUOTE() does not support approximate numeric columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "row-scalar SELECT QUOTE() supports only integer, DECIMAL, nonbinary string, YEAR, "
        "and temporal columns"
    );
    return false;
}
