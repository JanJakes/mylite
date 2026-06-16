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
                                       : mylite_execution_string_trim_function_kind(expression->kind);
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
    if (!mylite_execution_string_trim_scalar_argument_is_admitted(expression)) {
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
        !mylite_execution_string_trim_scalar_argument_is_admitted(expression)) {
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
    size_t source_index = 0U;
    int rc = resolve_descriptor_column_reference_with_source_index(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT trim functions support only descriptor columns",
        table_columns,
        table_column_count,
        &column,
        &source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!string_trim_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    out_expression->column_has_source_index = select_source_context_is_joined(source_context);
    out_expression->column_source_index = source_index;
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
                                       : mylite_execution_string_slice_function_kind(expression->kind);
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
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[1]
        );
    } else if (rc == MYLITE_OK) {
        rc = plan_row_scalar_string_slice_length_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[1]
        );
    }
    if (rc == MYLITE_OK && function_kind == PLANNED_STRING_SLICE_FUNCTION_SUBSTRING &&
        argument_count == 3U) {
        rc = plan_row_scalar_string_slice_length_argument(
            database,
            child_at(expression, 2U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
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
    if (!mylite_execution_string_slice_scalar_text_argument_is_admitted(expression)) {
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
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    return plan_row_scalar_integer_like_argument(
        database,
        expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        "string slice functions support only integer, boolean, NULL, integer descriptor, and "
        "supported integer expression length arguments",
        "string slice function length literals must fit the signed 64-bit range",
        out_expression
    );
}

static int plan_row_scalar_string_slice_position_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    expression = unwrap_parenthesized_expression(expression);
    return plan_row_scalar_integer_like_argument(
        database,
        expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        "string slice functions support only integer, boolean, NULL, integer descriptor, and "
        "supported integer expression position arguments",
        "string slice function position literals must fit the signed 64-bit range",
        out_expression
    );
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
    size_t source_index = 0U;
    int rc = resolve_descriptor_column_reference_with_source_index(
        database,
        expression,
        source_context,
        column_diagnostic_context,
        "row-scalar SELECT string slice functions support only descriptor columns",
        table_columns,
        table_column_count,
        &column,
        &source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!string_slice_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    out_expression->column_has_source_index = select_source_context_is_joined(source_context);
    out_expression->column_source_index = source_index;
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
                                       : mylite_execution_string_padding_function_kind(expression->kind);
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
            has_source,
            source_context,
            table_columns,
            table_column_count,
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
            has_source,
            source_context,
            table_columns,
            table_column_count,
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
    if (!mylite_execution_string_replace_scalar_argument_is_admitted(expression)) {
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
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    return plan_row_scalar_integer_like_argument(
        database,
        expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        "string padding functions support only integer, boolean, NULL, integer descriptor, and "
        "supported integer expression length/count arguments",
        "string padding function length/count literals must fit the signed 64-bit range",
        out_expression
    );
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
