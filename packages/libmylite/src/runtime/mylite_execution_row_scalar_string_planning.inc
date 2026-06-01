static int plan_row_scalar_string_length_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    enum planned_string_length_function_kind function_kind = PLANNED_STRING_LENGTH_FUNCTION_NONE;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_LENGTH_FUNCTION_NONE
                                       : string_length_function_kind(expression->kind);
    if (function_kind == PLANNED_STRING_LENGTH_FUNCTION_NONE ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(database, "string length functions support exactly one argument");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH;
    out_expression->string_length_kind = function_kind;
    out_expression->argument_count = 1U;

    rc = plan_row_scalar_string_length_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        &out_expression->arguments[0]
    );
    return rc;
}

static int plan_row_scalar_string_length_argument(
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
        set_unsupported_error(
            database,
            "string length functions support only string, integer, boolean, NULL, session "
            "scalar, system variable, and descriptor column arguments"
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
        return plan_row_scalar_string_length_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    }
    if (has_source && (expression->kind == MYLITE_SQL_AST_RAND_FUNCTION ||
                       expression->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION)) {
        set_unsupported_error(
            database,
            "string length functions do not support RAND() arguments in table-backed SELECT"
        );
        return MYLITE_ERROR;
    }
    if (!string_length_scalar_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "string length functions support only string, integer, boolean, NULL, session "
            "scalar, system variable, and descriptor column arguments"
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

static int plan_row_scalar_string_length_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        column_diagnostic_context,
        "row-scalar SELECT string length functions support only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!string_length_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool string_length_column_descriptor_is_supported(
    struct mylite_db *database,
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
        set_unsupported_error(
            database,
            "string length functions do not support approximate numeric columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "string length functions support only integer, DECIMAL, string, binary string, BIT, "
        "YEAR, and temporal columns"
    );
    return false;
}

static int plan_row_scalar_string_codepoint_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum planned_string_codepoint_function_kind function_kind =
        PLANNED_STRING_CODEPOINT_FUNCTION_NONE;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_CODEPOINT_FUNCTION_NONE
                                       : string_codepoint_function_kind(expression->kind);
    if (function_kind == PLANNED_STRING_CODEPOINT_FUNCTION_NONE ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(database, "string codepoint functions support exactly one argument");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT;
    out_expression->string_codepoint_kind = function_kind;
    out_expression->argument_count = 1U;

    rc = plan_row_scalar_string_codepoint_argument(
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

static int plan_row_scalar_string_codepoint_argument(
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
            "string codepoint functions support only string, hex, integer, boolean, NULL, "
            "session scalar, system variable, and descriptor column arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return plan_row_scalar_string_codepoint_identifier_argument(
            database,
            expression,
            has_source,
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
            "string codepoint functions do not support RAND() arguments in table-backed SELECT"
        );
        return MYLITE_ERROR;
    }
    if (!string_codepoint_scalar_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "string codepoint functions support only string, hex, integer, boolean, NULL, "
            "session scalar, system variable, and descriptor column arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_string_codepoint_literal_argument(
            database,
            expression,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
    }
    return plan_row_scalar_session_value(database, expression, out_expression);
}

static int plan_row_scalar_string_codepoint_identifier_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
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
    return plan_row_scalar_string_codepoint_column(
        database,
        expression,
        source_context,
        table_columns,
        table_column_count,
        out_expression
    );
}

static int plan_row_scalar_string_codepoint_literal_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = mylite_sql_ast_node_literal_kind(expression);

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        char *text = NULL;
        size_t text_length = 0U;
        int rc = decode_sql_string_literal_with_policy(
            database,
            expression,
            "string codepoint functions support only string literals",
            "string codepoint function literals are invalid",
            true,
            &text,
            &text_length
        );

        if (rc == MYLITE_OK) {
            rc = assign_text_value(database, text, text_length, &out_expression->value);
        }
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_HEX) {
        char *bytes = NULL;
        size_t byte_count = 0U;
        int rc = decode_binary_hex_literal(database, expression, &bytes, &byte_count);

        if (rc == MYLITE_OK) {
            rc = assign_blob_value(database, bytes, byte_count, &out_expression->value);
        }
        return rc;
    }
    return plan_row_scalar_literal_value(database, expression, out_expression);
}

static int plan_row_scalar_string_codepoint_column(
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
        "row-scalar SELECT string codepoint functions support only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!string_codepoint_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool string_codepoint_column_descriptor_is_supported(
    struct mylite_db *database,
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
        set_unsupported_error(
            database,
            "string codepoint functions do not support approximate numeric columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "string codepoint functions support only integer, DECIMAL, string, binary string, BIT, "
        "YEAR, and temporal columns"
    );
    return false;
}

static int plan_row_scalar_string_case_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum planned_string_case_function_kind function_kind = PLANNED_STRING_CASE_FUNCTION_NONE;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_CASE_FUNCTION_NONE
                                       : string_case_function_kind(expression->kind);
    if (function_kind == PLANNED_STRING_CASE_FUNCTION_NONE ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(database, "string case functions support exactly one argument");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE;
    out_expression->string_case_kind = function_kind;
    out_expression->argument_count = 1U;

    rc = plan_row_scalar_string_case_argument(
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

static int plan_row_scalar_string_case_argument(
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
            "string case functions support only string, integer, boolean, NULL, session "
            "scalar, system variable, and descriptor column arguments"
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
        return plan_row_scalar_string_case_column(
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
            "string case functions do not support RAND() arguments in table-backed SELECT"
        );
        return MYLITE_ERROR;
    }
    if (!string_case_scalar_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "string case functions support only string, integer, boolean, NULL, session "
            "scalar, system variable, and descriptor column arguments"
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

static int plan_row_scalar_string_case_column(
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
        "row-scalar SELECT string case functions support only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!string_case_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool string_case_column_descriptor_is_supported(
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
        set_unsupported_error(database, "string case functions do not support binary columns");
        return false;
    }
    if (column_descriptor_is_approximate(column)) {
        set_unsupported_error(
            database,
            "string case functions do not support approximate numeric columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "string case functions support only integer, DECIMAL, nonbinary string, YEAR, and "
        "temporal columns"
    );
    return false;
}

static int plan_row_scalar_string_trim_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum planned_string_trim_function_kind function_kind = PLANNED_STRING_TRIM_FUNCTION_NONE;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_TRIM_FUNCTION_NONE
                                       : string_trim_function_kind(expression->kind);
    argument_count = expression == NULL ? 0U : mylite_sql_ast_node_child_count(expression);
    if (function_kind == PLANNED_STRING_TRIM_FUNCTION_NONE ||
        (argument_count != 1U && argument_count != 2U)) {
        set_unsupported_error(
            database,
            "trim functions support one value and an optional remove string"
        );
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM;
    out_expression->string_trim_kind = function_kind;
    out_expression->argument_count = 2U;

    rc = plan_row_scalar_string_trim_value_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK && argument_count == 2U) {
        rc = plan_row_scalar_string_trim_remove_argument(
            database,
            child_at(expression, 1U),
            &out_expression->arguments[1]
        );
    } else if (rc == MYLITE_OK) {
        rc = copy_text_value(database, " ", &out_expression->arguments[1].value);
        if (rc == MYLITE_OK) {
            out_expression->arguments[1].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        }
    }
    return rc;
}

static int plan_row_scalar_string_trim_value_argument(
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
            "trim functions support only string, integer, boolean, NULL, session scalar, system "
            "variable, and descriptor column value arguments"
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
        return plan_row_scalar_string_trim_column(
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
            "trim functions do not support RAND() value arguments in table-backed SELECT"
        );
        return MYLITE_ERROR;
    }
    if (!string_trim_scalar_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "trim functions support only string, integer, boolean, NULL, session scalar, system "
            "variable, and descriptor column value arguments"
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

static int plan_row_scalar_string_trim_remove_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER ||
        !string_trim_scalar_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "trim functions support only scalar string, integer, boolean, NULL, session scalar, "
            "and system variable remove-string arguments"
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

static int plan_row_scalar_string_trim_column(
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
        "row-scalar SELECT trim functions support only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!string_trim_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool string_trim_column_descriptor_is_supported(
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
        set_unsupported_error(database, "trim functions do not support binary columns");
        return false;
    }
    if (column_descriptor_is_approximate(column)) {
        set_unsupported_error(
            database,
            "trim functions do not support approximate numeric columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "trim functions support only integer, DECIMAL, nonbinary string, YEAR, and temporal "
        "columns"
    );
    return false;
}

static int plan_row_scalar_string_slice_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    enum planned_string_slice_function_kind function_kind = PLANNED_STRING_SLICE_FUNCTION_NONE;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_SLICE_FUNCTION_NONE
                                       : string_slice_function_kind(expression->kind);
    argument_count = expression == NULL ? 0U : mylite_sql_ast_node_child_count(expression);
    if (function_kind == PLANNED_STRING_SLICE_FUNCTION_NONE) {
        set_unsupported_error(database, "string slice functions support exactly two arguments");
        return MYLITE_ERROR;
    }
    if (function_kind == PLANNED_STRING_SLICE_FUNCTION_SUBSTRING) {
        if (argument_count != 2U && argument_count != 3U) {
            set_unsupported_error(database, "SUBSTRING functions support two or three arguments");
            return MYLITE_ERROR;
        }
    } else if (argument_count != 2U) {
        set_unsupported_error(database, "string slice functions support exactly two arguments");
        return MYLITE_ERROR;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE;
    out_expression->string_slice_kind = function_kind;
    out_expression->argument_count = argument_count;

    rc = plan_row_scalar_string_slice_value_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK && function_kind == PLANNED_STRING_SLICE_FUNCTION_SUBSTRING) {
        rc = plan_row_scalar_string_slice_position_argument(
            database,
            child_at(expression, 1U),
            &out_expression->arguments[1]
        );
    } else if (rc == MYLITE_OK) {
        rc = plan_row_scalar_string_slice_length_argument(
            database,
            child_at(expression, 1U),
            &out_expression->arguments[1]
        );
    }
    if (rc == MYLITE_OK && function_kind == PLANNED_STRING_SLICE_FUNCTION_SUBSTRING &&
        argument_count == 3U) {
        rc = plan_row_scalar_string_slice_length_argument(
            database,
            child_at(expression, 2U),
            &out_expression->arguments[2]
        );
    }
    return rc;
}

static int plan_row_scalar_string_slice_value_argument(
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
        set_unsupported_error(
            database,
            "string slice functions support only string, integer, boolean, NULL, session "
            "scalar, system variable, and descriptor column string arguments"
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
        return plan_row_scalar_string_slice_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    }
    if (has_source && (expression->kind == MYLITE_SQL_AST_RAND_FUNCTION ||
                       expression->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION)) {
        set_unsupported_error(
            database,
            "string slice functions do not support RAND() arguments in table-backed SELECT"
        );
        return MYLITE_ERROR;
    }
    if (!string_slice_scalar_text_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "string slice functions support only string, integer, boolean, NULL, session "
            "scalar, system variable, and descriptor column string arguments"
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

static int plan_row_scalar_string_slice_length_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (!string_slice_length_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "string slice functions support only integer, boolean, and NULL length literals"
        );
        return MYLITE_ERROR;
    }
    rc = string_slice_signed_length_value(database, expression, &value, &is_null);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_literal_value(database, expression, out_expression);
    }
    return plan_row_scalar_integer_value(database, expression, out_expression);
}

static int plan_row_scalar_string_slice_position_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (!string_slice_length_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "string slice functions support only integer, boolean, and NULL position literals"
        );
        return MYLITE_ERROR;
    }
    rc = string_slice_signed_position_value(database, expression, &value, &is_null);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_literal_value(database, expression, out_expression);
    }
    return plan_row_scalar_integer_value(database, expression, out_expression);
}

static int plan_row_scalar_string_slice_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        column_diagnostic_context,
        "row-scalar SELECT string slice functions support only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!string_slice_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool string_slice_column_descriptor_is_supported(
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
        set_unsupported_error(database, "string slice functions do not support binary columns");
        return false;
    }
    if (column_descriptor_is_approximate(column)) {
        set_unsupported_error(
            database,
            "string slice functions do not support approximate numeric columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "string slice functions support only integer, DECIMAL, nonbinary string, YEAR, and "
        "temporal columns"
    );
    return false;
}

static int plan_row_scalar_string_padding_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum planned_string_padding_function_kind function_kind = PLANNED_STRING_PADDING_FUNCTION_NONE;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_PADDING_FUNCTION_NONE
                                       : string_padding_function_kind(expression->kind);
    argument_count = expression == NULL ? 0U : mylite_sql_ast_node_child_count(expression);
    if ((function_kind == PLANNED_STRING_PADDING_FUNCTION_LPAD ||
         function_kind == PLANNED_STRING_PADDING_FUNCTION_RPAD) &&
        argument_count != 3U) {
        set_native_function_parameter_count_error(
            database,
            function_kind == PLANNED_STRING_PADDING_FUNCTION_LPAD ? "LPAD" : "RPAD"
        );
        return MYLITE_ERROR;
    }
    if (function_kind == PLANNED_STRING_PADDING_FUNCTION_REPEAT && argument_count != 2U) {
        set_native_function_parameter_count_error(database, "REPEAT");
        return MYLITE_ERROR;
    }
    if (function_kind == PLANNED_STRING_PADDING_FUNCTION_SPACE && argument_count != 1U) {
        set_native_function_parameter_count_error(database, "SPACE");
        return MYLITE_ERROR;
    }
    if (function_kind == PLANNED_STRING_PADDING_FUNCTION_NONE) {
        set_unsupported_error(
            database,
            "string padding functions support LPAD, RPAD, REPEAT, and SPACE"
        );
        return MYLITE_ERROR;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING;
    out_expression->string_padding_kind = function_kind;
    out_expression->argument_count = argument_count;

    if (function_kind == PLANNED_STRING_PADDING_FUNCTION_SPACE) {
        return plan_row_scalar_string_padding_count_argument(
            database,
            child_at(expression, 0U),
            &out_expression->arguments[0]
        );
    }

    rc = plan_row_scalar_string_padding_value_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        "string padding functions support only string, integer, boolean, NULL, session scalar, "
        "system variable, and descriptor column value arguments",
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_string_padding_count_argument(
            database,
            child_at(expression, 1U),
            &out_expression->arguments[1]
        );
    }
    if (rc == MYLITE_OK && (function_kind == PLANNED_STRING_PADDING_FUNCTION_LPAD ||
                            function_kind == PLANNED_STRING_PADDING_FUNCTION_RPAD)) {
        rc = plan_row_scalar_string_padding_value_argument(
            database,
            child_at(expression, 2U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            "string padding functions support only string, integer, boolean, NULL, session "
            "scalar, system variable, and descriptor column pad-string arguments",
            &out_expression->arguments[2]
        );
    }
    return rc;
}

static int plan_row_scalar_string_padding_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
) {
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
        return plan_row_scalar_string_padding_column(
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
            "string padding functions do not support RAND() arguments in table-backed SELECT"
        );
        return MYLITE_ERROR;
    }
    if (!string_replace_scalar_argument_is_admitted(expression)) {
        set_unsupported_error(database, unsupported_message);
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

static int plan_row_scalar_string_padding_count_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (!string_slice_length_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "string padding functions support only integer, boolean, and NULL length/count "
            "literals"
        );
        return MYLITE_ERROR;
    }
    rc = string_slice_signed_integer_value(
        database,
        expression,
        "string padding functions support only integer, boolean, and NULL length/count literals",
        "string padding function length/count literals must fit the signed 64-bit range",
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

static int plan_row_scalar_string_padding_column(
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
        "row-scalar SELECT string padding functions support only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!string_padding_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool string_padding_column_descriptor_is_supported(
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
        set_unsupported_error(database, "string padding functions do not support binary columns");
        return false;
    }
    if (column_descriptor_is_approximate(column)) {
        set_unsupported_error(
            database,
            "string padding functions do not support approximate numeric columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "string padding functions support only integer, DECIMAL, nonbinary string, YEAR, and "
        "temporal columns"
    );
    return false;
}

static int plan_row_scalar_string_bitmask_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    enum planned_string_bitmask_function_kind function_kind = PLANNED_STRING_BITMASK_FUNCTION_NONE;
    const char *function_name = NULL;
    size_t argument_count = 0U;

    expression = unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_BITMASK_FUNCTION_NONE
                                       : string_bitmask_function_kind(expression->kind);
    if (function_kind == PLANNED_STRING_BITMASK_FUNCTION_NONE || expression == NULL ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(database, "string bitmask functions support EXPORT_SET and MAKE_SET");
        return MYLITE_ERROR;
    }
    function_name =
        function_kind == PLANNED_STRING_BITMASK_FUNCTION_EXPORT_SET ? "EXPORT_SET" : "MAKE_SET";
    arguments = child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }
    argument_count = mylite_sql_ast_node_child_count(arguments);
    if ((function_kind == PLANNED_STRING_BITMASK_FUNCTION_EXPORT_SET &&
         (argument_count < string_bitmask_export_set_min_argument_count ||
          argument_count > string_bitmask_export_set_max_argument_count)) ||
        (function_kind == PLANNED_STRING_BITMASK_FUNCTION_MAKE_SET &&
         argument_count < string_bitmask_make_set_min_argument_count)) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK;
    out_expression->string_bitmask_kind = function_kind;
    out_expression->argument_count = argument_count;

    if (function_kind == PLANNED_STRING_BITMASK_FUNCTION_EXPORT_SET) {
        return plan_row_scalar_export_set_bitmask_arguments(
            database,
            arguments,
            argument_count,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }

    return plan_row_scalar_make_set_bitmask_arguments(
        database,
        arguments,
        argument_count,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        out_expression
    );
}

static int plan_row_scalar_export_set_bitmask_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    size_t argument_count,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = plan_row_scalar_string_bitmask_bit_argument(
        database,
        child_at(arguments, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );

    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_string_bitmask_value_argument(
            database,
            child_at(arguments, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            "EXPORT_SET() supports only string, integer, boolean, NULL, session scalar, system "
            "variable, and descriptor column on/off/separator arguments",
            &out_expression->arguments[1]
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_string_bitmask_value_argument(
            database,
            child_at(arguments, 2U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            "EXPORT_SET() supports only string, integer, boolean, NULL, session scalar, system "
            "variable, and descriptor column on/off/separator arguments",
            &out_expression->arguments[2]
        );
    }
    if (rc == MYLITE_OK && argument_count >= 4U) {
        rc = plan_row_scalar_string_bitmask_value_argument(
            database,
            child_at(arguments, 3U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            "EXPORT_SET() supports only string, integer, boolean, NULL, session scalar, system "
            "variable, and descriptor column on/off/separator arguments",
            &out_expression->arguments[3]
        );
    }
    if (rc == MYLITE_OK && argument_count == string_bitmask_export_set_max_argument_count) {
        rc = plan_row_scalar_string_bitmask_count_argument(
            database,
            child_at(arguments, 4U),
            &out_expression->arguments[4]
        );
    }
    return rc;
}

static int plan_row_scalar_make_set_bitmask_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    size_t argument_count,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = plan_row_scalar_string_bitmask_bit_argument(
        database,
        child_at(arguments, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );

    for (size_t argument_index = 1U; rc == MYLITE_OK && argument_index < argument_count;
         ++argument_index) {
        rc = plan_row_scalar_string_bitmask_value_argument(
            database,
            child_at(arguments, argument_index),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            "MAKE_SET() supports only string, integer, boolean, NULL, session scalar, system "
            "variable, and descriptor column value arguments",
            &out_expression->arguments[argument_index]
        );
    }
    return rc;
}

static int plan_row_scalar_string_bitmask_bit_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "string bitmask functions support only signed integer, boolean, NULL, and integer "
            "descriptor bitmask arguments"
        );
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
        return plan_row_scalar_string_bitmask_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (!string_slice_length_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "string bitmask functions support only signed integer, boolean, NULL, and integer "
            "descriptor bitmask arguments"
        );
        return MYLITE_ERROR;
    }
    rc = string_slice_signed_integer_value(
        database,
        expression,
        "string bitmask functions support only signed integer, boolean, NULL, and integer "
        "descriptor bitmask arguments",
        "string bitmask function bitmask literals must fit the signed 64-bit range",
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

static int plan_row_scalar_string_bitmask_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
) {
    return plan_row_scalar_string_padding_value_argument(
        database,
        expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        unsupported_message,
        out_expression
    );
}

static int plan_row_scalar_string_bitmask_count_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (!string_slice_length_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "EXPORT_SET() number_of_bits supports only signed integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    rc = string_slice_signed_integer_value(
        database,
        expression,
        "EXPORT_SET() number_of_bits supports only signed integer, boolean, and NULL literals",
        "EXPORT_SET() number_of_bits literals must fit the signed 64-bit range",
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

static int plan_row_scalar_string_bitmask_column(
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
        "row-scalar SELECT string bitmask functions support only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!string_bitmask_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool string_bitmask_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }

    set_unsupported_error(
        database,
        "string bitmask functions support only integer descriptor bitmask columns"
    );
    return false;
}

static int plan_row_scalar_string_search_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum planned_string_search_function_kind function_kind = PLANNED_STRING_SEARCH_FUNCTION_NONE;
    const struct mylite_sql_ast_node *needle = NULL;
    const struct mylite_sql_ast_node *haystack = NULL;
    const struct mylite_sql_ast_node *position = NULL;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_SEARCH_FUNCTION_NONE
                                       : string_search_function_kind(expression->kind);
    argument_count = expression == NULL ? 0U : mylite_sql_ast_node_child_count(expression);
    if (function_kind == PLANNED_STRING_SEARCH_FUNCTION_NONE ||
        (argument_count != 2U && argument_count != 3U) ||
        (function_kind != PLANNED_STRING_SEARCH_FUNCTION_LOCATE && argument_count != 2U)) {
        set_unsupported_error(
            database,
            "string search functions support LOCATE(substr,str[,pos]), INSTR(str,substr), "
            "and POSITION(substr IN str)"
        );
        return MYLITE_ERROR;
    }

    if (function_kind == PLANNED_STRING_SEARCH_FUNCTION_INSTR) {
        haystack = child_at(expression, 0U);
        needle = child_at(expression, 1U);
    } else {
        needle = child_at(expression, 0U);
        haystack = child_at(expression, 1U);
    }
    if (argument_count == 3U) {
        position = child_at(expression, 2U);
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(3U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH;
    out_expression->string_search_kind = function_kind;
    out_expression->argument_count = 3U;

    rc = plan_row_scalar_string_search_text_argument(
        database,
        needle,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        "string search functions support only string, integer, boolean, NULL, session scalar, "
        "system variable, and descriptor column substring arguments",
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_string_search_text_argument(
            database,
            haystack,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            "string search functions support only string, integer, boolean, NULL, session scalar, "
            "system variable, and descriptor column string arguments",
            &out_expression->arguments[1]
        );
    }
    if (rc == MYLITE_OK && position != NULL) {
        rc = plan_row_scalar_string_search_position_argument(
            database,
            position,
            &out_expression->arguments[2]
        );
    } else if (rc == MYLITE_OK) {
        out_expression->arguments[2].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[2].value = (struct planned_value){.is_null = false, .integer = 1};
    }
    return rc;
}

static int plan_row_scalar_string_search_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
) {
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
        return plan_row_scalar_string_search_column(
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
            "string search functions do not support RAND() arguments in table-backed SELECT"
        );
        return MYLITE_ERROR;
    }
    if (!string_slice_scalar_text_argument_is_admitted(expression)) {
        set_unsupported_error(database, unsupported_message);
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

static int plan_row_scalar_string_search_position_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (!string_slice_length_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "LOCATE() position supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    rc = string_slice_signed_integer_value(
        database,
        expression,
        "LOCATE() position supports only integer, boolean, and NULL literals",
        "LOCATE() position literals must fit the signed 64-bit range",
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

static int plan_row_scalar_string_search_column(
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
        "row-scalar SELECT string search functions support only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!string_search_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool string_search_column_descriptor_is_supported(
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
        set_unsupported_error(database, "string search functions do not support binary columns");
        return false;
    }
    if (column_descriptor_is_approximate(column)) {
        set_unsupported_error(
            database,
            "string search functions do not support approximate numeric columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "string search functions support only integer, DECIMAL, nonbinary string, YEAR, and "
        "temporal columns"
    );
    return false;
}

static int plan_row_scalar_string_replace_expression(
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
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_REPLACE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 3U) {
        set_native_function_parameter_count_error(database, "REPLACE");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(3U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE;
    out_expression->argument_count = 3U;

    rc = plan_row_scalar_string_replace_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        "REPLACE() supports only string, integer, boolean, NULL, session scalar, "
        "system variable, and descriptor column value arguments",
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_string_replace_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            "REPLACE() supports only string, integer, boolean, NULL, session scalar, "
            "system variable, and descriptor column search arguments",
            &out_expression->arguments[1]
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_string_replace_argument(
            database,
            child_at(expression, 2U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            "REPLACE() supports only string, integer, boolean, NULL, session scalar, "
            "system variable, and descriptor column replacement arguments",
            &out_expression->arguments[2]
        );
    }
    return rc;
}

static int plan_row_scalar_string_insert_expression(
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
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_INSERT_STRING_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 4U) {
        set_native_function_parameter_count_error(database, "INSERT");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(4U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT;
    out_expression->argument_count = 4U;

    rc = plan_row_scalar_string_insert_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        "INSERT() supports only string, integer, boolean, NULL, session scalar, "
        "system variable, and descriptor column value arguments",
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_string_insert_position_argument(
            database,
            child_at(expression, 1U),
            &out_expression->arguments[1]
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_string_insert_length_argument(
            database,
            child_at(expression, 2U),
            &out_expression->arguments[2]
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_string_insert_argument(
            database,
            child_at(expression, 3U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            "INSERT() supports only string, integer, boolean, NULL, session scalar, "
            "system variable, and descriptor column replacement arguments",
            &out_expression->arguments[3]
        );
    }
    return rc;
}

static int plan_row_scalar_string_replace_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
) {
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
        return plan_row_scalar_string_replace_column(
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
            "REPLACE() does not support RAND() arguments in table-backed SELECT"
        );
        return MYLITE_ERROR;
    }
    if (!string_replace_scalar_argument_is_admitted(expression)) {
        set_unsupported_error(database, unsupported_message);
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

static int plan_row_scalar_string_insert_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
) {
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
        return plan_row_scalar_string_insert_column(
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
            "INSERT() does not support RAND() arguments in table-backed SELECT"
        );
        return MYLITE_ERROR;
    }
    if (!string_replace_scalar_argument_is_admitted(expression)) {
        set_unsupported_error(database, unsupported_message);
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

static int plan_row_scalar_string_insert_position_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (!string_slice_length_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "INSERT() position supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    rc = string_slice_signed_integer_value(
        database,
        expression,
        "INSERT() position supports only integer, boolean, and NULL literals",
        "INSERT() position literals must fit the signed 64-bit range",
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

static int plan_row_scalar_string_insert_length_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (!string_slice_length_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "INSERT() length supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    rc = string_slice_signed_integer_value(
        database,
        expression,
        "INSERT() length supports only integer, boolean, and NULL literals",
        "INSERT() length literals must fit the signed 64-bit range",
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

static int plan_row_scalar_string_replace_column(
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
        "row-scalar SELECT REPLACE() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!string_replace_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool string_replace_column_descriptor_is_supported(
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
            "row-scalar SELECT REPLACE() does not support binary string or BIT columns"
        );
        return false;
    }
    if (column_descriptor_is_approximate(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT REPLACE() does not support approximate numeric columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "row-scalar SELECT REPLACE() supports only integer, DECIMAL, nonbinary string, YEAR, "
        "and temporal columns"
    );
    return false;
}

static int plan_row_scalar_string_insert_column(
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
        "row-scalar SELECT INSERT() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!string_insert_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool string_insert_column_descriptor_is_supported(
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
            "row-scalar SELECT INSERT() does not support binary string or BIT columns"
        );
        return false;
    }
    if (column_descriptor_is_approximate(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT INSERT() does not support approximate numeric columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "row-scalar SELECT INSERT() supports only integer, DECIMAL, nonbinary string, YEAR, "
        "and temporal columns"
    );
    return false;
}

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
    if (!string_reverse_scalar_argument_is_admitted(expression)) {
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
    if (!soundex_scalar_argument_is_admitted(expression)) {
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
    if (!quote_scalar_argument_is_admitted(expression)) {
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

static int plan_row_scalar_substring_index_expression(
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
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 3U) {
        set_native_function_parameter_count_error(database, "SUBSTRING_INDEX");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(3U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX;
    out_expression->argument_count = 3U;

    rc = plan_row_scalar_substring_index_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        "SUBSTRING_INDEX() supports only string, integer, boolean, NULL, session scalar, "
        "system variable, and descriptor column value arguments",
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_substring_index_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            "SUBSTRING_INDEX() supports only string, integer, boolean, NULL, session scalar, "
            "system variable, and descriptor column delimiter arguments",
            &out_expression->arguments[1]
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_substring_index_count_argument(
            database,
            child_at(expression, 2U),
            &out_expression->arguments[2]
        );
    }
    return rc;
}

static int plan_row_scalar_substring_index_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
) {
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
        return plan_row_scalar_substring_index_column(
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
            "SUBSTRING_INDEX() does not support RAND() arguments in table-backed SELECT"
        );
        return MYLITE_ERROR;
    }
    if (!string_replace_scalar_argument_is_admitted(expression)) {
        set_unsupported_error(database, unsupported_message);
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

static int plan_row_scalar_substring_index_count_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (!string_slice_length_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "SUBSTRING_INDEX() count supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    rc = string_slice_signed_integer_value(
        database,
        expression,
        "SUBSTRING_INDEX() count supports only integer, boolean, and NULL literals",
        "SUBSTRING_INDEX() count literals must fit the signed 64-bit range",
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

static int plan_row_scalar_substring_index_column(
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
        "row-scalar SELECT SUBSTRING_INDEX() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!substring_index_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool substring_index_column_descriptor_is_supported(
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
        set_unsupported_error(database, "SUBSTRING_INDEX() does not support binary columns");
        return false;
    }
    if (column_descriptor_is_approximate(column)) {
        set_unsupported_error(database, "SUBSTRING_INDEX() does not support approximate columns");
        return false;
    }

    set_unsupported_error(
        database,
        "SUBSTRING_INDEX() supports only integer, DECIMAL, nonbinary string, YEAR, and "
        "temporal columns"
    );
    return false;
}

static int plan_row_scalar_find_in_set_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_FIND_IN_SET_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "FIND_IN_SET");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET;
    out_expression->argument_count = 2U;

    rc = plan_row_scalar_find_in_set_text_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        "FIND_IN_SET() supports only string, integer, boolean, NULL, session scalar, "
        "system variable, and descriptor column search arguments",
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_find_in_set_text_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            "FIND_IN_SET() supports only string, integer, boolean, NULL, session scalar, "
            "system variable, and descriptor column list arguments",
            &out_expression->arguments[1]
        );
    }
    return rc;
}

static int plan_row_scalar_find_in_set_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
) {
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
        return plan_row_scalar_find_in_set_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    }
    if (has_source && (expression->kind == MYLITE_SQL_AST_RAND_FUNCTION ||
                       expression->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION)) {
        set_unsupported_error(
            database,
            "FIND_IN_SET() does not support RAND() arguments in table-backed SELECT"
        );
        return MYLITE_ERROR;
    }
    if (!string_slice_scalar_text_argument_is_admitted(expression)) {
        set_unsupported_error(database, unsupported_message);
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

static int plan_row_scalar_find_in_set_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        column_diagnostic_context,
        "row-scalar SELECT FIND_IN_SET() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!find_in_set_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool find_in_set_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }
    if (column_descriptor_is_decimal(column) || column_descriptor_is_string_family(column) ||
        column_descriptor_is_enum(column) || column_descriptor_is_date(column) ||
        column_descriptor_is_time(column) || column_descriptor_is_datetime(column) ||
        column_descriptor_is_timestamp(column) || column_descriptor_is_year(column)) {
        return true;
    }
    if (column_descriptor_is_set(column)) {
        set_unsupported_error(database, "FIND_IN_SET() does not support SET columns");
        return false;
    }
    if (column_descriptor_is_binary_string_family(column) || column_descriptor_is_bit(column)) {
        set_unsupported_error(database, "FIND_IN_SET() does not support binary columns");
        return false;
    }
    if (column_descriptor_is_approximate(column)) {
        set_unsupported_error(database, "FIND_IN_SET() does not support approximate columns");
        return false;
    }

    set_unsupported_error(
        database,
        "FIND_IN_SET() supports only integer, DECIMAL, nonbinary string, ENUM, YEAR, "
        "and temporal columns"
    );
    return false;
}

static int plan_row_scalar_strcmp_expression(
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
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_STRCMP_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "STRCMP");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_STRCMP;
    out_expression->argument_count = 2U;

    rc = plan_row_scalar_strcmp_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        "STRCMP() supports only string, integer, boolean, NULL, session scalar, system variable, "
        "and descriptor column left arguments",
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_strcmp_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            "STRCMP() supports only string, integer, boolean, NULL, session scalar, system "
            "variable, and descriptor column right arguments",
            &out_expression->arguments[1]
        );
    }
    return rc;
}

static int plan_row_scalar_strcmp_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
) {
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
        return plan_row_scalar_strcmp_column(
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
            "STRCMP() does not support RAND() arguments in table-backed SELECT"
        );
        return MYLITE_ERROR;
    }
    if (!string_slice_scalar_text_argument_is_admitted(expression)) {
        set_unsupported_error(database, unsupported_message);
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

static int plan_row_scalar_strcmp_column(
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
        "row-scalar SELECT STRCMP() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!strcmp_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool strcmp_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }
    if (column_descriptor_is_decimal(column) || column_descriptor_is_string_family(column) ||
        column_descriptor_is_date(column) || column_descriptor_is_datetime(column) ||
        column_descriptor_is_timestamp(column) || column_descriptor_is_year(column)) {
        return true;
    }
    if (column_descriptor_is_time(column)) {
        set_unsupported_error(database, "STRCMP() does not support TIME columns");
        return false;
    }
    if (column_descriptor_is_binary_string_family(column) || column_descriptor_is_bit(column)) {
        set_unsupported_error(database, "STRCMP() does not support binary columns");
        return false;
    }
    if (column_descriptor_is_approximate(column)) {
        set_unsupported_error(database, "STRCMP() does not support approximate columns");
        return false;
    }

    set_unsupported_error(
        database,
        "STRCMP() supports only integer, DECIMAL, nonbinary string, YEAR, DATE, DATETIME, "
        "and TIMESTAMP columns"
    );
    return false;
}

static int plan_row_scalar_regexp_like_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    struct planned_row_scalar_expression value_argument = {0};
    struct planned_row_scalar_expression pattern_argument = {0};
    bool match_type_is_null = false;
    bool case_sensitive = false;
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION) {
        set_native_function_parameter_count_error(database, "REGEXP_LIKE");
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count != 2U && child_count != 3U) {
        set_native_function_parameter_count_error(database, "REGEXP_LIKE");
        return MYLITE_ERROR;
    }

    rc = plan_row_scalar_regexp_like_value_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        &value_argument
    );
    if (rc == MYLITE_OK && child_count == 3U) {
        rc = evaluate_regexp_like_match_type_argument(
            database,
            child_at(expression, 2U),
            &match_type_is_null,
            &case_sensitive
        );
    }
    if (rc == MYLITE_OK && match_type_is_null) {
        planned_row_scalar_expression_deinit(&value_argument);
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->value = (struct planned_value){.is_null = true};
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_regexp_like_pattern_argument(
            database,
            child_at(expression, 1U),
            &pattern_argument
        );
    }
    if (rc == MYLITE_OK && !pattern_argument.value.is_null) {
        rc = validate_regexp_like_pattern(
            database,
            pattern_argument.value.text,
            pattern_argument.value.text_length,
            case_sensitive,
            "REGEXP_LIKE() patterns support only MyLite's baseline ASCII regular expression subset"
        );
    }
    if (rc == MYLITE_OK) {
        out_expression->arguments =
            (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
        if (out_expression->arguments == NULL) {
            set_nomem_error(database);
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE;
        out_expression->regexp_case_sensitive = case_sensitive;
        out_expression->argument_count = 2U;
        out_expression->arguments[0] = value_argument;
        out_expression->arguments[1] = pattern_argument;
        value_argument = (struct planned_row_scalar_expression){0};
        pattern_argument = (struct planned_row_scalar_expression){0};
    }

    planned_row_scalar_expression_deinit(&value_argument);
    planned_row_scalar_expression_deinit(&pattern_argument);
    return rc;
}

static int plan_row_scalar_regexp_like_value_argument(
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
        set_unsupported_error(
            database,
            "REGEXP_LIKE() supports only string, integer, boolean, NULL, session scalar, "
            "system variable, and descriptor column value arguments"
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
        return plan_row_scalar_regexp_like_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    }
    if (has_source && (expression->kind == MYLITE_SQL_AST_RAND_FUNCTION ||
                       expression->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION)) {
        set_unsupported_error(
            database,
            "REGEXP_LIKE() does not support RAND() arguments in table-backed SELECT"
        );
        return MYLITE_ERROR;
    }
    if (!string_slice_scalar_text_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "REGEXP_LIKE() supports only string, integer, boolean, NULL, session scalar, "
            "system variable, and descriptor column value arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        int rc = plan_row_scalar_literal_value(database, expression, out_expression);

        if (rc == MYLITE_OK && out_expression->value.is_text &&
            !text_value_is_supported_string_key(
                out_expression->value.text,
                out_expression->value.text_length
            )) {
            set_unsupported_error(
                database,
                "REGEXP_LIKE() value arguments support only ASCII text"
            );
            return MYLITE_ERROR;
        }
        return rc;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
    }
    return plan_row_scalar_session_value(database, expression, out_expression);
}

static int plan_row_scalar_regexp_like_pattern_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    static const struct regexp_like_text_argument_messages pattern_messages = {
        .unsupported =
            "REGEXP_LIKE() supports only string, integer, boolean, and NULL pattern arguments",
        .string_unsupported = "REGEXP_LIKE() supports only string pattern literals",
        .embedded_nul = "REGEXP_LIKE() pattern arguments do not support NUL bytes",
        .non_ascii = "REGEXP_LIKE() arguments support only ASCII text",
    };
    struct session_scalar_cell cell = {0};
    char *owned_text = NULL;
    const char *text = NULL;
    size_t text_length = 0U;
    bool is_null = false;
    int rc = evaluate_regexp_like_text_argument(
        database,
        expression,
        false,
        &pattern_messages,
        &cell,
        &owned_text,
        &text,
        &text_length,
        &is_null
    );

    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        if (is_null) {
            out_expression->value = (struct planned_value){.is_null = true};
        } else {
            rc = copy_text_value(database, text, &out_expression->value);
        }
    }

    free(owned_text);
    session_scalar_cell_deinit(&cell);
    return rc;
}

static int plan_row_scalar_regexp_like_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        column_diagnostic_context,
        "row-scalar SELECT REGEXP_LIKE() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!regexp_like_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool regexp_like_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column_descriptor_is_string_family(column)) {
        return true;
    }
    if (column_descriptor_is_binary_string_family(column) || column_descriptor_is_bit(column)) {
        set_unsupported_error(database, "REGEXP_LIKE() does not support binary columns");
        return false;
    }
    set_unsupported_error(database, "REGEXP_LIKE() supports only nonbinary string columns");
    return false;
}

static int plan_row_scalar_regexp_string_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum planned_regexp_string_function_kind kind = PLANNED_REGEXP_STRING_FUNCTION_NONE;
    size_t expected_argument_count = 0U;
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    kind = regexp_string_function_kind(expression->kind);
    if (kind == PLANNED_REGEXP_STRING_FUNCTION_NONE) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    expected_argument_count = kind == PLANNED_REGEXP_STRING_FUNCTION_REPLACE ? 3U : 2U;
    child_count = regexp_string_function_argument_count(expression);
    if (child_count != expected_argument_count) {
        if (child_count > expected_argument_count) {
            set_unsupported_error(
                database,
                "REGEXP string function optional arguments are not supported"
            );
        } else {
            set_native_function_parameter_count_error(database, regexp_string_function_name(kind));
        }
        return MYLITE_ERROR;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(expected_argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->regexp_string_kind = kind;
    out_expression->argument_count = expected_argument_count;
    if (kind == PLANNED_REGEXP_STRING_FUNCTION_INSTR) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR;
    } else if (kind == PLANNED_REGEXP_STRING_FUNCTION_SUBSTR) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR;
    } else {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE;
    }

    rc = plan_row_scalar_regexp_string_argument(
        database,
        regexp_string_function_argument_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        "REGEXP string functions support only string, integer, boolean, NULL, session scalar, "
        "system variable, and descriptor column value arguments",
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_regexp_like_pattern_argument(
            database,
            regexp_string_function_argument_at(expression, 1U),
            &out_expression->arguments[1]
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_row_scalar_regexp_string_pattern(database, &out_expression->arguments[1]);
    }
    if (rc == MYLITE_OK && kind == PLANNED_REGEXP_STRING_FUNCTION_REPLACE) {
        rc = plan_row_scalar_regexp_string_argument(
            database,
            regexp_string_function_argument_at(expression, 2U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            "REGEXP_REPLACE() supports only string, integer, boolean, NULL, session scalar, "
            "system variable, and descriptor column replacement arguments",
            &out_expression->arguments[2]
        );
    }
    return rc;
}

static int plan_row_scalar_regexp_string_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
) {
    return plan_row_scalar_string_replace_argument(
        database,
        expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        unsupported_message,
        out_expression
    );
}

static int validate_row_scalar_regexp_string_pattern(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *pattern_argument
) {
    if (pattern_argument == NULL || pattern_argument->kind != PLANNED_ROW_SCALAR_EXPRESSION_VALUE) {
        set_unsupported_error(database, "REGEXP string function patterns must be scalar values");
        return MYLITE_ERROR;
    }
    if (pattern_argument->value.is_null) {
        return MYLITE_OK;
    }
    if (pattern_argument->value.text_length == 0U) {
        set_regexp_illegal_argument_error(database);
        return MYLITE_ERROR;
    }
    return validate_regexp_like_pattern(
        database,
        pattern_argument->value.text,
        pattern_argument->value.text_length,
        false,
        "REGEXP string function patterns support only MyLite's baseline ASCII regular expression "
        "subset"
    );
}
