static int plan_row_scalar_date_interval_second_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_date_interval_second_input_kind input_kind =
        MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING;
    enum mylite_date_interval_unit unit = MYLITE_DATE_INTERVAL_UNIT_SECOND;
    const char *function_name = "DATE_ADD";
    const char *input_kind_name = NULL;
    const char *unit_name = NULL;
    int subtract_flag = 0;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || !mylite_execution_is_date_interval_second_function_kind(expression->kind)) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }
    function_name = mylite_execution_date_interval_second_function_name(expression->kind);
    rc = mylite_execution_validate_date_interval_second_function_shape(database, expression, function_name);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (expression->kind != MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION) {
        rc = mylite_execution_date_interval_unit_from_ast(
            database,
            mylite_execution_date_interval_unit_node(expression),
            function_name,
            &unit
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    unit_name = mylite_date_interval_unit_name(unit);
    if (unit_name == NULL) {
        return set_row_scalar_date_interval_second_unsupported_error(
            database,
            function_name,
            "supports only YEAR, QUARTER, MONTH, WEEK, DAY, HOUR, MINUTE, and SECOND interval "
            "units"
        );
    }
    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(date_interval_planned_argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND;
    out_expression->argument_count = date_interval_planned_argument_count;

    rc = plan_row_scalar_date_interval_second_temporal_argument(
        database,
        mylite_execution_date_interval_second_temporal_node(expression),
        function_name,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0],
        &input_kind
    );
    input_kind_name = mylite_date_interval_second_input_kind_name(input_kind);
    if (rc == MYLITE_OK && input_kind_name != NULL) {
        rc = copy_text_value(database, input_kind_name, &out_expression->arguments[1].value);
        out_expression->arguments[1].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_date_interval_second_interval_argument(
            database,
            mylite_execution_date_interval_second_interval_node(expression),
            function_name,
            unit,
            &out_expression->arguments[2]
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_text_value(database, unit_name, &out_expression->arguments[3].value);
        out_expression->arguments[3].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    if (rc == MYLITE_OK) {
        if (mylite_execution_date_interval_second_function_subtracts(expression->kind)) {
            subtract_flag = 1;
        }
        out_expression->arguments[4].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[4].value =
            (struct planned_value){.is_null = false, .integer = subtract_flag};
    }
    return rc;
}

static int plan_row_scalar_date_interval_second_temporal_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_interval_second_input_kind *out_input_kind
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_input_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING;
    if (expression == NULL) {
        return set_row_scalar_date_interval_second_unsupported_error(
            database,
            function_name,
            "supports only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor "
            "columns, string descriptor columns, and NULL"
        );
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_date_interval_second_literal(
            database,
            expression,
            function_name,
            out_expression
        );
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
        return plan_row_scalar_date_interval_second_column(
            database,
            expression,
            function_name,
            source_context,
            table_columns,
            table_column_count,
            out_expression,
            out_input_kind
        );
    }

    return set_row_scalar_date_interval_second_unsupported_error(
        database,
        function_name,
        "supports only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor columns, "
        "string descriptor columns, and NULL"
    );
}

static int plan_row_scalar_date_interval_second_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_interval_second_input_kind *out_input_kind
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT temporal interval functions support only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_date(&column)) {
        *out_input_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_DATE;
    } else if (column_descriptor_is_datetime(&column)) {
        *out_input_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_DATETIME;
    } else if (column_descriptor_is_timestamp(&column)) {
        *out_input_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_TIMESTAMP;
    } else if (column_descriptor_is_time(&column)) {
        return set_row_scalar_date_interval_second_unsupported_error(
            database,
            function_name,
            "does not yet support TIME values"
        );
    } else if (column_descriptor_is_string_family(&column)) {
        *out_input_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING;
    } else {
        return set_row_scalar_date_interval_second_unsupported_error(
            database,
            function_name,
            "supports only DATE, DATETIME, TIMESTAMP, string descriptor columns, string temporal "
            "literals, and NULL"
        );
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_date_interval_second_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    struct planned_row_scalar_expression *out_expression
) {
    char unsupported_message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    char nul_message[date_interval_nul_diagnostic_capacity];
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        return set_row_scalar_date_interval_second_unsupported_error(
            database,
            function_name,
            "supports only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor "
            "columns, string descriptor columns, and NULL"
        );
    }
    if (!mylite_execution_date_interval_second_message(
            unsupported_message,
            sizeof(unsupported_message),
            function_name,
            "supports only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor "
            "columns, string descriptor columns, and NULL"
        ) ||
        !mylite_execution_date_interval_second_message(
            nul_message,
            sizeof(nul_message),
            function_name,
            "date literals do not support NUL bytes"
        )) {
        set_runtime_error(database, "failed to format temporal function diagnostic");
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        expression,
        unsupported_message,
        nul_message,
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, nul_message);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        rc = assign_text_value(database, text, text_length, &out_expression->value);
        text = NULL;
    }
    free(text);
    return rc;
}

static int plan_row_scalar_date_interval_second_interval_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    enum mylite_date_interval_unit unit,
    struct planned_row_scalar_expression *out_expression
) {
    int64_t interval_seconds = 0;
    bool interval_is_null = false;
    int rc = mylite_execution_date_interval_second_interval_argument(
        database,
        function_name,
        expression,
        unit,
        &interval_seconds,
        &interval_is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    if (interval_is_null) {
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
    } else {
        out_expression->value =
            (struct planned_value){.is_null = false, .integer = interval_seconds};
    }
    return MYLITE_OK;
}

static int set_row_scalar_date_interval_second_unsupported_error(
    struct mylite_db *database,
    const char *function_name,
    const char *suffix
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

    if (!mylite_execution_date_interval_second_message(message, sizeof(message), function_name, suffix)) {
        set_runtime_error(database, "failed to format temporal function diagnostic");
        return MYLITE_ERROR;
    }
    set_unsupported_error(database, message);
    return MYLITE_ERROR;
}

static int plan_row_scalar_temporal_extract_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_temporal_extract_kind extract_kind = MYLITE_TEMPORAL_EXTRACT_DATE;
    enum mylite_temporal_extract_input_kind input_kind = MYLITE_TEMPORAL_EXTRACT_INPUT_STRING;
    const struct mylite_sql_ast_node *argument = NULL;
    const char *extract_kind_name = NULL;
    const char *input_kind_name = NULL;
    int mode = 0;
    int rc = MYLITE_OK;

    rc = mylite_execution_resolve_temporal_extract_call(
        database,
        expression,
        &extract_kind,
        &argument,
        &mode
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(4U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT;
    out_expression->temporal_extract_kind = extract_kind;
    out_expression->argument_count = 4U;

    rc = plan_row_scalar_temporal_extract_argument(
        database,
        argument,
        extract_kind,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0],
        &input_kind
    );
    extract_kind_name = mylite_temporal_extract_kind_name(extract_kind);
    input_kind_name = mylite_temporal_extract_input_kind_name(input_kind);
    if (rc == MYLITE_OK && extract_kind_name != NULL) {
        rc = copy_text_value(database, extract_kind_name, &out_expression->arguments[1].value);
        out_expression->arguments[1].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    if (rc == MYLITE_OK && input_kind_name != NULL) {
        rc = copy_text_value(database, input_kind_name, &out_expression->arguments[2].value);
        out_expression->arguments[2].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    if (rc == MYLITE_OK) {
        out_expression->arguments[3].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[3].value =
            (struct planned_value){.is_null = false, .integer = mode};
    }
    return rc;
}

static int plan_row_scalar_temporal_extract_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_temporal_extract_kind extract_kind,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_temporal_extract_input_kind *out_input_kind
) {
    const int is_calendar_name = (int)mylite_temporal_extract_kind_is_calendar_name(extract_kind);
    const int is_calendar_date = (int)mylite_temporal_extract_kind_is_calendar_date(extract_kind);
    const int is_week_temporal = (int)mylite_temporal_extract_kind_is_week_temporal(extract_kind);
    const char *unsupported_argument_message =
        "temporal extract functions support only string temporal literals, descriptor columns, and "
        "NULL";

    if (is_calendar_name != 0) {
        unsupported_argument_message =
            "calendar name functions support only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, string descriptor columns, and NULL";
    } else if (is_week_temporal != 0) {
        unsupported_argument_message =
            "week temporal functions support only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, string descriptor columns, and NULL";
    } else if (is_calendar_date != 0) {
        unsupported_argument_message =
            "calendar date functions support only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, string descriptor columns, and NULL";
    }

    expression = unwrap_parenthesized_expression(expression);
    if (out_input_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input_kind = MYLITE_TEMPORAL_EXTRACT_INPUT_STRING;
    if (expression == NULL) {
        set_unsupported_error(database, unsupported_argument_message);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_temporal_extract_literal_value(
                database,
                expression,
                extract_kind,
                out_expression
            );
        }
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
        return plan_row_scalar_temporal_extract_column(
            database,
            expression,
            extract_kind,
            source_context,
            table_columns,
            table_column_count,
            out_expression,
            out_input_kind
        );
    }

    set_unsupported_error(database, unsupported_argument_message);
    return MYLITE_ERROR;
}

static int plan_row_scalar_temporal_extract_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_temporal_extract_kind extract_kind,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    const int is_calendar_name = (int)mylite_temporal_extract_kind_is_calendar_name(extract_kind);
    const int is_calendar_date = (int)mylite_temporal_extract_kind_is_calendar_date(extract_kind);
    const int is_week_temporal = (int)mylite_temporal_extract_kind_is_week_temporal(extract_kind);
    const char *unsupported_message =
        "temporal extract functions support only string temporal literals, descriptor columns, and "
        "NULL";
    const char *nul_message = "temporal extract literals do not support NUL bytes";
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (is_calendar_name != 0) {
        unsupported_message =
            "calendar name functions support only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, string descriptor columns, and NULL";
        nul_message = "calendar name function literals do not support NUL bytes";
    } else if (is_week_temporal != 0) {
        unsupported_message =
            "week temporal functions support only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, string descriptor columns, and NULL";
        nul_message = "week temporal function literals do not support NUL bytes";
    } else if (is_calendar_date != 0) {
        unsupported_message =
            "calendar date functions support only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, string descriptor columns, and NULL";
        nul_message = "calendar date function literals do not support NUL bytes";
    }

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL ||
        out_expression == NULL) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
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
        nul_message,
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, nul_message);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = assign_text_value(database, text, text_length, &out_expression->value);
    }
    return rc;
}

static int plan_row_scalar_temporal_extract_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_temporal_extract_kind extract_kind,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_temporal_extract_input_kind *out_input_kind
) {
    struct mylite_catalog_column_descriptor column = {0};
    size_t source_index = 0U;
    int rc = resolve_descriptor_column_reference_with_source_index(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT temporal extract functions support only descriptor columns",
        table_columns,
        table_column_count,
        &column,
        &source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_date(&column)) {
        if (mylite_temporal_extract_kind_is_time_part(extract_kind) &&
            extract_kind != MYLITE_TEMPORAL_EXTRACT_MICROSECOND &&
            extract_kind != MYLITE_TEMPORAL_EXTRACT_SIGNED_MICROSECOND) {
            return reject_date_column_temporal_extract_time_part(database, extract_kind);
        }
        *out_input_kind = MYLITE_TEMPORAL_EXTRACT_INPUT_DATE;
    } else if (column_descriptor_is_time(&column)) {
        if (mylite_temporal_extract_kind_is_date_part(extract_kind)) {
            return reject_time_column_temporal_extract_date_part(database, extract_kind);
        }
        *out_input_kind = MYLITE_TEMPORAL_EXTRACT_INPUT_TIME;
    } else if (column_descriptor_is_datetime(&column)) {
        *out_input_kind = MYLITE_TEMPORAL_EXTRACT_INPUT_DATETIME;
    } else if (column_descriptor_is_timestamp(&column)) {
        *out_input_kind = MYLITE_TEMPORAL_EXTRACT_INPUT_TIMESTAMP;
    } else if (column_descriptor_is_string_family(&column)) {
        if (extract_kind == MYLITE_TEMPORAL_EXTRACT_TIME) {
            set_unsupported_error(
                database,
                "TIME() does not yet support string descriptor columns"
            );
            return MYLITE_ERROR;
        }
        *out_input_kind = MYLITE_TEMPORAL_EXTRACT_INPUT_STRING;
    } else {
        return reject_unsupported_temporal_extract_column(database, extract_kind);
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    out_expression->column_has_source_index = select_source_context_is_joined(source_context);
    out_expression->column_source_index = source_index;
    return MYLITE_OK;
}

static int reject_date_column_temporal_extract_time_part(
    struct mylite_db *database,
    enum mylite_temporal_extract_kind extract_kind
) {
    if (extract_kind == MYLITE_TEMPORAL_EXTRACT_HOUR ||
        extract_kind == MYLITE_TEMPORAL_EXTRACT_MINUTE ||
        extract_kind == MYLITE_TEMPORAL_EXTRACT_SECOND) {
        set_unsupported_error(database, "HOUR()/MINUTE()/SECOND() do not yet support DATE values");
    } else {
        set_unsupported_error(database, "EXTRACT() time units do not yet support DATE values");
    }
    return MYLITE_ERROR;
}

static int reject_time_column_temporal_extract_date_part(
    struct mylite_db *database,
    enum mylite_temporal_extract_kind extract_kind
) {
    if (mylite_temporal_extract_kind_is_calendar_name(extract_kind)) {
        set_unsupported_error(database, "calendar name functions do not yet support TIME values");
    } else if (mylite_temporal_extract_kind_is_week_temporal(extract_kind)) {
        set_unsupported_error(database, "week temporal functions do not yet support TIME values");
    } else if (mylite_temporal_extract_kind_is_calendar_date(extract_kind)) {
        set_unsupported_error(database, "calendar date functions do not yet support TIME values");
    } else if (extract_kind == MYLITE_TEMPORAL_EXTRACT_QUARTER) {
        set_unsupported_error(
            database,
            "DATE()/YEAR()/QUARTER()/MONTH()/DAY()/DAYOFMONTH() do not support TIME values"
        );
    } else if (extract_kind == MYLITE_TEMPORAL_EXTRACT_YEAR_MONTH) {
        set_unsupported_error(database, "EXTRACT() date units do not support TIME values");
    } else {
        set_unsupported_error(
            database,
            "DATE()/YEAR()/MONTH()/DAY()/DAYOFMONTH() do not support TIME values"
        );
    }
    return MYLITE_ERROR;
}

static int reject_unsupported_temporal_extract_column(
    struct mylite_db *database,
    enum mylite_temporal_extract_kind extract_kind
) {
    if (mylite_temporal_extract_kind_is_calendar_name(extract_kind)) {
        set_unsupported_error(
            database,
            "calendar name functions support only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, string descriptor columns, and NULL"
        );
    } else if (mylite_temporal_extract_kind_is_week_temporal(extract_kind)) {
        set_unsupported_error(
            database,
            "week temporal functions support only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, string descriptor columns, and NULL"
        );
    } else if (mylite_temporal_extract_kind_is_calendar_date(extract_kind)) {
        set_unsupported_error(
            database,
            "calendar date functions support only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, string descriptor columns, and NULL"
        );
    } else {
        set_unsupported_error(
            database,
            "temporal extract functions support only DATE, TIME, DATETIME, TIMESTAMP, string, "
            "and NULL values"
        );
    }
    return MYLITE_ERROR;
}
