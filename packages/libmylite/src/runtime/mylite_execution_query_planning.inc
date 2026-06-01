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

    if (has_source) {
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
    if (is_date_format_numeric_equal_expression(expression)) {
        return plan_row_scalar_date_format_numeric_equal_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (row_scalar_date_format_equal_attempt(expression)) {
        set_unsupported_error(database, "DATE_FORMAT() numeric comparison requires a literal");
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
    if (is_charset_collation_function_kind(expression->kind)) {
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
        return plan_row_scalar_rand_expression(database, expression, out_expression);
    }
    *out_handled = false;
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
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION && child_count == 1U) {
        rc = rand_seed_value(database, child_at(expression, 0U), &seed);
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_RAND;
        out_expression->has_rand_seed = true;
        out_expression->rand_seed = seed;
        return MYLITE_OK;
    }

    set_unsupported_error(database, "RAND() supports only RAND() and RAND(seed)");
    return MYLITE_ERROR;
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

    set_unsupported_error(
        database,
        "table-backed signed integer arithmetic projection supports only descriptor columns, "
        "integer literals, booleans, NULL, parentheses, and +, binary -, and * operators"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_integer_arithmetic_binary(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression,
    struct row_scalar_integer_arithmetic_plan_stack *stack
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
    int rc = MYLITE_OK;

    if (!integer_arithmetic_operator_is_supported(operator_kind)) {
        set_unsupported_error(
            database,
            "table-backed signed integer arithmetic projection supports only +, binary -, "
            "and * operators"
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
                "table-backed signed integer arithmetic projection supports only signed "
                "integer literal operands"
            );
            return MYLITE_ERROR;
        }
        if (child == NULL || child->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(child) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            set_unsupported_error(
                database,
                "table-backed signed integer arithmetic projection supports unary signs only "
                "on integer literals"
            );
            return MYLITE_ERROR;
        }
        return plan_row_scalar_integer_value(database, unwrapped, out_expression);
    }
    if (unwrapped->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "table-backed signed integer arithmetic projection supports only integer, boolean, "
            "and NULL literal operands"
        );
        return MYLITE_ERROR;
    }

    switch (mylite_sql_ast_node_literal_kind(unwrapped)) {
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return plan_row_scalar_integer_value(database, unwrapped, out_expression);
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
        "table-backed signed integer arithmetic projection supports only integer, boolean, "
        "and NULL literal operands"
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
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "table-backed signed integer arithmetic projection supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!signed_integer_arithmetic_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool signed_integer_arithmetic_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    const char *logical_type = NULL;

    if (column == NULL || strcmp(column->physical_type, "INTEGER") != 0) {
        set_unsupported_error(
            database,
            "table-backed signed integer arithmetic projection supports only signed integer "
            "descriptor columns"
        );
        return false;
    }

    logical_type = column->logical_type;
    if (strstr(logical_type, "UNSIGNED") != NULL) {
        set_unsupported_error(
            database,
            "table-backed signed integer arithmetic projection does not support unsigned "
            "integer columns"
        );
        return false;
    }
    if (strcmp(logical_type, "TINYINT") == 0 || strcmp(logical_type, "TINYINT(1)") == 0 ||
        strcmp(logical_type, "SMALLINT") == 0 || strcmp(logical_type, "MEDIUMINT") == 0 ||
        strcmp(logical_type, "INT") == 0 || strcmp(logical_type, "INTEGER") == 0 ||
        strcmp(logical_type, "BIGINT") == 0) {
        return true;
    }

    set_unsupported_error(
        database,
        "table-backed signed integer arithmetic projection supports only signed integer "
        "descriptor columns"
    );
    return false;
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
        is_temporal_extract_function_kind(expression->kind)) {
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
    if (is_date_interval_second_function_kind(expression->kind)) {
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
    if (is_string_length_function_kind(kind) || is_string_codepoint_function_kind(kind) ||
        is_string_case_function_kind(kind) || is_string_trim_function_kind(kind) ||
        is_string_slice_function_kind(kind) || is_string_padding_function_kind(kind) ||
        is_string_bitmask_function_kind(kind) || is_string_search_function_kind(kind)) {
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
    if (is_string_length_function_kind(expression->kind)) {
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
    if (is_string_codepoint_function_kind(expression->kind)) {
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
    if (is_string_case_function_kind(expression->kind)) {
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
    if (is_string_trim_function_kind(expression->kind)) {
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
    if (is_string_slice_function_kind(expression->kind)) {
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
    if (is_string_padding_function_kind(expression->kind)) {
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
    if (is_string_bitmask_function_kind(expression->kind)) {
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
    if (is_string_search_function_kind(expression->kind)) {
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
    if (regexp_string_function_argument_count_error_name(expression->kind) != NULL) {
        set_native_function_parameter_count_error(
            database,
            regexp_string_function_argument_count_error_name(expression->kind)
        );
        return MYLITE_ERROR;
    }
    if (regexp_string_function_kind(expression->kind) != PLANNED_REGEXP_STRING_FUNCTION_NONE) {
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

static int plan_row_scalar_json_valid_expression(
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
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_VALID_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "JSON_VALID");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID;
    out_expression->argument_count = 1U;

    rc = plan_row_scalar_json_valid_argument(
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

static int plan_row_scalar_json_valid_argument(
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
            "JSON_VALID() supports only string, integer, boolean, NULL, limited binary cast, and "
            "descriptor column arguments"
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
        return plan_row_scalar_json_valid_column(
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
            "JSON_VALID() does not support RAND() arguments in table-backed SELECT"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_CAST_BINARY_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION) {
        struct session_scalar_cell cell = {0};
        int rc = session_scalar_value(database, expression, &cell);

        if (rc == MYLITE_OK) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){
                .is_null = cell.value == NULL,
                .is_text = false,
                .integer = 0,
            };
        }
        session_scalar_cell_deinit(&cell);
        return rc;
    }
    if (!mylite_execution_scalar_json_valid_scalar_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "JSON_VALID() supports only string, integer, boolean, NULL, limited binary cast, and "
            "descriptor column arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_literal_value(database, expression, out_expression);
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
    }
    set_unsupported_error(
        database,
        "JSON_VALID() supports only string, integer, boolean, NULL, limited binary cast, and "
        "descriptor column arguments"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_json_valid_column(
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
        "row-scalar SELECT JSON_VALID() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!json_valid_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool json_valid_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }
    if (column_descriptor_is_json(column) || column_descriptor_is_string_family(column) ||
        column_descriptor_is_binary_string_family(column) || column_descriptor_is_bit(column)) {
        return true;
    }
    set_unsupported_error(
        database,
        "JSON_VALID() supports only JSON, integer, nonbinary string, binary string, and BIT columns"
    );
    return false;
}

static int plan_row_scalar_json_contains_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_CONTAINS_FUNCTION) {
        set_native_function_parameter_count_error(database, "JSON_CONTAINS");
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count != 2U && child_count != 3U) {
        set_native_function_parameter_count_error(database, "JSON_CONTAINS");
        return MYLITE_ERROR;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(child_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS;
    out_expression->argument_count = child_count;

    rc = plan_row_scalar_json_introspection_document_argument(
        database,
        child_at(expression, 0U),
        "JSON_CONTAINS",
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_json_introspection_document_argument(
            database,
            child_at(expression, 1U),
            "JSON_CONTAINS",
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            &out_expression->arguments[1]
        );
    }
    if (rc == MYLITE_OK && child_count == 3U) {
        rc = plan_row_scalar_json_search_path_argument(
            database,
            child_at(expression, 2U),
            &out_expression->arguments[2]
        );
    }
    return rc;
}

static int plan_row_scalar_json_contains_path_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *argument = NULL;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_CONTAINS_PATH_FUNCTION) {
        set_native_function_parameter_count_error(database, "JSON_CONTAINS_PATH");
        return MYLITE_ERROR;
    }
    rc = mylite_execution_scalar_json_collect_function_arguments(
        database,
        expression,
        "JSON_CONTAINS_PATH",
        &arguments,
        &argument_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument_count < 3U) {
        set_native_function_parameter_count_error(database, "JSON_CONTAINS_PATH");
        return MYLITE_ERROR;
    }
    if (argument_count > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH;
    out_expression->argument_count = argument_count;

    argument = child_at(arguments, 0U);
    rc = plan_row_scalar_json_introspection_document_argument(
        database,
        argument,
        "JSON_CONTAINS_PATH",
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        &out_expression->arguments[0]
    );
    argument = argument == NULL ? NULL : argument->next_sibling;
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_json_contains_path_mode_argument(
            database,
            argument,
            &out_expression->arguments[1]
        );
    }
    argument = argument == NULL ? NULL : argument->next_sibling;
    for (size_t argument_index = 2U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         ++argument_index) {
        rc = plan_row_scalar_json_search_path_argument(
            database,
            argument,
            &out_expression->arguments[argument_index]
        );
        argument = argument->next_sibling;
    }
    if (rc == MYLITE_OK && argument != NULL) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int plan_row_scalar_json_contains_path_mode_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "JSON_CONTAINS_PATH() supports only string and NULL one_or_all arguments"
        );
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        return plan_row_scalar_literal_value(database, expression, out_expression);
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        set_invalid_json_one_or_all_error(database);
        return MYLITE_ERROR;
    }
    return plan_row_scalar_literal_value(database, expression, out_expression);
}

static int plan_row_scalar_json_extract_expression(
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
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION) {
        set_native_function_parameter_count_error(database, "JSON_EXTRACT");
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(expression) != 2U) {
        set_unsupported_error(database, "JSON_EXTRACT() multiple path arguments are not supported");
        return MYLITE_ERROR;
    }
    return plan_row_scalar_json_extract_arguments(
        database,
        child_at(expression, 0U),
        PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT,
        child_at(expression, 1U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        out_expression
    );
}

static int plan_row_scalar_json_value_expression(
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
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_VALUE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE;
    out_expression->argument_count = 2U;

    rc = plan_row_scalar_json_value_document_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_json_value_path_argument(
            database,
            child_at(expression, 1U),
            &out_expression->arguments[1]
        );
    }
    return rc;
}

static int plan_row_scalar_json_length_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_LENGTH_FUNCTION) {
        set_native_function_parameter_count_error(database, "JSON_LENGTH");
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count != 1U && child_count != 2U) {
        set_native_function_parameter_count_error(database, "JSON_LENGTH");
        return MYLITE_ERROR;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(child_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH;
    out_expression->argument_count = child_count;

    rc = plan_row_scalar_json_introspection_document_argument(
        database,
        child_at(expression, 0U),
        "JSON_LENGTH",
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK && child_count == 2U) {
        rc = plan_row_scalar_json_length_path_argument(
            database,
            child_at(expression, 1U),
            &out_expression->arguments[1]
        );
    }
    return rc;
}

static int plan_row_scalar_json_keys_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_KEYS_FUNCTION) {
        set_native_function_parameter_count_error(database, "JSON_KEYS");
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count != 1U && child_count != 2U) {
        set_native_function_parameter_count_error(database, "JSON_KEYS");
        return MYLITE_ERROR;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(child_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS;
    out_expression->argument_count = child_count;

    rc = plan_row_scalar_json_introspection_document_argument(
        database,
        child_at(expression, 0U),
        "JSON_KEYS",
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK && child_count == 2U) {
        rc = plan_row_scalar_json_keys_path_argument(
            database,
            child_at(expression, 1U),
            &out_expression->arguments[1]
        );
    }
    return rc;
}

static int plan_row_scalar_json_type_expression(
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
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_TYPE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "JSON_TYPE");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE;
    out_expression->argument_count = 1U;

    rc = plan_row_scalar_json_introspection_document_argument(
        database,
        child_at(expression, 0U),
        "JSON_TYPE",
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        &out_expression->arguments[0]
    );
    return rc;
}

static int plan_row_scalar_json_quote_expression(
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
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_QUOTE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "JSON_QUOTE");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE;
    out_expression->argument_count = 1U;

    rc = plan_row_scalar_json_quote_argument(
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

static int plan_row_scalar_json_unquote_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *argument = NULL;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "JSON_UNQUOTE");
        return MYLITE_ERROR;
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION) {
        return plan_row_scalar_json_extract_arguments(
            database,
            child_at(argument, 0U),
            PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT,
            child_at(argument, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    }
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_BINARY_EXPRESSION &&
        mylite_sql_ast_node_operator(argument) == MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT) {
        return plan_row_scalar_json_extract_arguments(
            database,
            child_at(argument, 0U),
            PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT,
            child_at(argument, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE;
    out_expression->argument_count = 1U;

    rc = plan_row_scalar_json_unquote_argument(
        database,
        argument,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        &out_expression->arguments[0]
    );
    return rc;
}

static int plan_row_scalar_json_array_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *argument = NULL;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_ARRAY_FUNCTION) {
        set_native_function_parameter_count_error(database, "JSON_ARRAY");
        return MYLITE_ERROR;
    }
    rc = mylite_execution_scalar_json_collect_function_arguments(
        database,
        expression,
        "JSON_ARRAY",
        &arguments,
        &argument_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument_count > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY;
    out_expression->argument_count = argument_count;
    out_expression->arguments = argument_count == 0U
                                    ? NULL
                                    : (
                                          struct planned_row_scalar_expression *
                                      )calloc(argument_count, sizeof(*out_expression->arguments));
    if (argument_count != 0U && out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    argument = arguments == NULL ? NULL : child_at(arguments, 0U);
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         ++argument_index) {
        rc = plan_row_scalar_json_constructor_argument(
            database,
            argument,
            false,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            &out_expression->arguments[argument_index]
        );
        argument = argument->next_sibling;
    }
    if (rc == MYLITE_OK && argument != NULL) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int plan_row_scalar_json_object_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *argument = NULL;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_OBJECT_FUNCTION) {
        set_native_function_parameter_count_error(database, "JSON_OBJECT");
        return MYLITE_ERROR;
    }
    rc = mylite_execution_scalar_json_collect_function_arguments(
        database,
        expression,
        "JSON_OBJECT",
        &arguments,
        &argument_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if ((argument_count % 2U) != 0U) {
        set_native_function_parameter_count_error(database, "JSON_OBJECT");
        return MYLITE_ERROR;
    }
    if (argument_count > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT;
    out_expression->argument_count = argument_count;
    out_expression->arguments = argument_count == 0U
                                    ? NULL
                                    : (
                                          struct planned_row_scalar_expression *
                                      )calloc(argument_count, sizeof(*out_expression->arguments));
    if (argument_count != 0U && out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    argument = arguments == NULL ? NULL : child_at(arguments, 0U);
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         ++argument_index) {
        rc = plan_row_scalar_json_constructor_argument(
            database,
            argument,
            (argument_index % 2U) == 0U,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            &out_expression->arguments[argument_index]
        );
        argument = argument->next_sibling;
    }
    if (rc == MYLITE_OK && argument != NULL) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int plan_row_scalar_json_set_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    const char *function_name = NULL;
    enum planned_json_mutation_kind mutation_kind = PLANNED_JSON_MUTATION_SET;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    rc = json_mutation_kind_from_ast(expression, &function_name, &mutation_kind);
    if (rc != MYLITE_OK) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }
    rc = mylite_execution_scalar_json_collect_function_arguments(
        database,
        expression,
        function_name,
        &arguments,
        &argument_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_execution_scalar_json_require_mutation_argument_count(
        database,
        argument_count,
        function_name,
        mutation_kind
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument_count > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET;
    out_expression->json_mutation_kind = mutation_kind;
    out_expression->argument_count = argument_count;
    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    return plan_row_scalar_json_mutation_arguments(
        database,
        arguments,
        argument_count,
        function_name,
        mutation_kind,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        out_expression
    );
}

static int json_mutation_kind_from_ast(
    const struct mylite_sql_ast_node *expression,
    const char **out_function_name,
    enum planned_json_mutation_kind *out_mutation_kind
) {
    if (out_function_name == NULL || out_mutation_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_function_name = "JSON_SET";
    *out_mutation_kind = PLANNED_JSON_MUTATION_SET;
    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_JSON_SET_FUNCTION:
        return MYLITE_OK;
    case MYLITE_SQL_AST_JSON_INSERT_FUNCTION:
        *out_function_name = "JSON_INSERT";
        *out_mutation_kind = PLANNED_JSON_MUTATION_INSERT;
        return MYLITE_OK;
    case MYLITE_SQL_AST_JSON_REPLACE_FUNCTION:
        *out_function_name = "JSON_REPLACE";
        *out_mutation_kind = PLANNED_JSON_MUTATION_REPLACE;
        return MYLITE_OK;
    case MYLITE_SQL_AST_JSON_REMOVE_FUNCTION:
        *out_function_name = "JSON_REMOVE";
        *out_mutation_kind = PLANNED_JSON_MUTATION_REMOVE;
        return MYLITE_OK;
    default:
        return MYLITE_ERROR;
    }
}

static int plan_row_scalar_json_mutation_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    size_t argument_count,
    const char *function_name,
    enum planned_json_mutation_kind mutation_kind,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *argument = child_at(arguments, 0U);
    int rc = MYLITE_OK;

    rc = plan_row_scalar_json_set_document_argument(
        database,
        argument,
        function_name,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        &out_expression->arguments[0]
    );
    argument = argument == NULL ? NULL : argument->next_sibling;
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mutation_kind == PLANNED_JSON_MUTATION_REMOVE) {
        return plan_row_scalar_json_remove_path_arguments(
            database,
            argument,
            argument_count,
            function_name,
            out_expression->arguments
        );
    }
    return plan_row_scalar_json_set_pair_arguments(
        database,
        argument,
        function_name,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        out_expression
    );
}

static int plan_row_scalar_json_remove_path_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    size_t argument_count,
    const char *function_name,
    struct planned_row_scalar_expression *out_arguments
) {
    int rc = MYLITE_OK;

    for (size_t argument_index = 1U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         ++argument_index) {
        rc = plan_row_scalar_json_set_path_argument(
            database,
            argument,
            function_name,
            &out_arguments[argument_index]
        );
        argument = argument->next_sibling;
    }
    if (rc == MYLITE_OK && argument != NULL) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int plan_row_scalar_json_set_pair_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    const char *function_name,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    if (out_expression == NULL || out_expression->arguments == NULL) {
        return MYLITE_MISUSE;
    }
    argument_count = out_expression->argument_count;
    for (size_t argument_index = 1U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         argument_index += 2U) {
        rc = plan_row_scalar_json_set_path_argument(
            database,
            argument,
            function_name,
            &out_expression->arguments[argument_index]
        );
        argument = argument->next_sibling;
        if (rc == MYLITE_OK &&
            out_expression->arguments[argument_index].kind == PLANNED_ROW_SCALAR_EXPRESSION_VALUE &&
            out_expression->arguments[argument_index].value.is_null) {
            out_expression->arguments[argument_index + 1U].kind =
                PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->arguments[argument_index + 1U].value =
                (struct planned_value){.is_null = true};
            out_expression->argument_count = argument_index + 2U;
            return MYLITE_OK;
        }
        if (rc == MYLITE_OK) {
            rc = plan_row_scalar_json_set_value_argument(
                database,
                argument,
                function_name,
                has_source,
                source_context,
                table_columns,
                table_column_count,
                column_diagnostic_context,
                &out_expression->arguments[argument_index + 1U]
            );
        }
        argument = argument == NULL ? NULL : argument->next_sibling;
    }
    if (rc == MYLITE_OK && argument != NULL) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int plan_row_scalar_json_set_document_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (function_name == NULL) {
        return MYLITE_MISUSE;
    }
    const char *document_message =
        "JSON_SET() supports only string, NULL, and descriptor column documents";
    const char *column_message =
        "JSON_SET() supports only JSON and nonbinary string descriptor column documents";

    if (strcmp(function_name, "JSON_REPLACE") == 0) {
        document_message =
            "JSON_REPLACE() supports only string, NULL, and descriptor column documents";
        column_message =
            "JSON_REPLACE() supports only JSON and nonbinary string descriptor column documents";
    } else if (strcmp(function_name, "JSON_INSERT") == 0) {
        document_message =
            "JSON_INSERT() supports only string, NULL, and descriptor column documents";
        column_message =
            "JSON_INSERT() supports only JSON and nonbinary string descriptor column documents";
    } else if (strcmp(function_name, "JSON_REMOVE") == 0) {
        document_message =
            "JSON_REMOVE() supports only string, NULL, and descriptor column documents";
        column_message =
            "JSON_REMOVE() supports only JSON and nonbinary string descriptor column documents";
    }

    if (expression == NULL) {
        set_unsupported_error(database, document_message);
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
        return plan_row_scalar_json_text_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            column_message,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
        }
        set_invalid_json_data_type_error(database, function_name);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        set_invalid_json_data_type_error(database, function_name);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_CAST_BINARY_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION) {
        set_json_binary_charset_error(database);
        return MYLITE_ERROR;
    }

    set_unsupported_error(database, document_message);
    return MYLITE_ERROR;
}

static int plan_row_scalar_json_constructor_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool is_key,
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
            "JSON constructors support only string, integer, boolean, NULL, and descriptor column "
            "arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return plan_row_scalar_json_constructor_identifier(
            database,
            expression,
            is_key,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_json_constructor_literal(
            database,
            expression,
            is_key,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_json_constructor_integer(database, expression, out_expression);
    }

    set_unsupported_error(
        database,
        "JSON constructors support only string, integer, boolean, NULL, and descriptor column "
        "arguments"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_json_constructor_identifier(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool is_key,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    if (!has_source) {
        return set_json_constructor_unknown_column_error(database, expression);
    }
    return plan_row_scalar_json_constructor_column(
        database,
        expression,
        is_key,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        out_expression
    );
}

static int set_json_constructor_unknown_column_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
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

static int plan_row_scalar_json_constructor_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool is_key,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = mylite_sql_ast_node_literal_kind(expression);

    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL && is_key) {
        set_json_null_member_name_error(database);
        return MYLITE_ERROR;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        return plan_row_scalar_json_constructor_literal_value(
            database,
            expression,
            MYLITE_JSON_SQL_VALUE_STRING,
            out_expression
        );
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        return plan_row_scalar_json_constructor_integer(database, expression, out_expression);
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        return plan_row_scalar_json_constructor_literal_value(
            database,
            expression,
            MYLITE_JSON_SQL_VALUE_BOOLEAN,
            out_expression
        );
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        return plan_row_scalar_json_constructor_literal_value(
            database,
            expression,
            MYLITE_JSON_SQL_VALUE_NULL,
            out_expression
        );
    }

    set_unsupported_error(
        database,
        "JSON constructors support only string, integer, boolean, NULL, and descriptor column "
        "arguments"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_json_constructor_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_json_sql_value_kind json_value_kind,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = plan_row_scalar_literal_value(database, expression, out_expression);

    if (rc == MYLITE_OK) {
        out_expression->json_value_kind = json_value_kind;
    }
    return rc;
}

static int plan_row_scalar_json_constructor_integer(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = plan_row_scalar_integer_value(database, expression, out_expression);

    if (rc == MYLITE_OK) {
        out_expression->json_value_kind = MYLITE_JSON_SQL_VALUE_INTEGER;
    }
    return rc;
}

static int plan_row_scalar_json_constructor_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool is_key,
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
        "row-scalar SELECT JSON constructors support only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_binary_string_family(&column) || column_descriptor_is_bit(&column)) {
        set_json_binary_charset_error(database);
        return MYLITE_ERROR;
    }
    if (!is_key && column_descriptor_is_json(&column)) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
        out_expression->column = column;
        out_expression->json_value_kind = MYLITE_JSON_SQL_VALUE_JSON;
        return MYLITE_OK;
    }
    if (column_descriptor_is_string_family(&column)) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
        out_expression->column = column;
        out_expression->json_value_kind = MYLITE_JSON_SQL_VALUE_STRING;
        return MYLITE_OK;
    }
    if (strcmp(column.physical_type, "INTEGER") == 0) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
        out_expression->column = column;
        out_expression->json_value_kind = MYLITE_JSON_SQL_VALUE_INTEGER;
        return MYLITE_OK;
    }
    if (is_key && column_descriptor_is_json(&column)) {
        set_unsupported_error(database, "JSON_OBJECT() keys do not support JSON columns");
        return MYLITE_ERROR;
    }

    set_unsupported_error(
        database,
        "JSON constructors support only JSON, integer, and nonbinary string descriptor columns"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_json_extract_operator_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_unsupported_error(database, "JSON path operators support only column -> 'path'");
        return MYLITE_ERROR;
    }
    operator_kind = mylite_sql_ast_node_operator(expression);
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT) {
        return plan_row_scalar_json_extract_arguments(
            database,
            child_at(expression, 0U),
            PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT) {
        return plan_row_scalar_json_extract_arguments(
            database,
            child_at(expression, 0U),
            PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    }

    set_unsupported_error(database, "JSON path operators support only column -> 'path'");
    return MYLITE_ERROR;
}

static int plan_row_scalar_json_extract_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *document,
    enum planned_row_scalar_expression_kind expression_kind,
    const struct mylite_sql_ast_node *path,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = expression_kind;
    out_expression->argument_count = 2U;

    rc = plan_row_scalar_json_document_argument(
        database,
        document,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_json_path_argument(database, path, &out_expression->arguments[1]);
    }
    return rc;
}

static int plan_row_scalar_json_introspection_document_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "JSON introspection supports only string, NULL, JSON_EXTRACT(), JSON_SET(), "
            "JSON_INSERT(), JSON_REPLACE(), JSON_REMOVE(), and descriptor column documents"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_SET_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_INSERT_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_REPLACE_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_REMOVE_FUNCTION) {
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
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION) {
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
    }
    if (expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION &&
        mylite_sql_ast_node_operator(expression) == MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT) {
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
        return plan_row_scalar_json_text_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            "JSON introspection supports only JSON and nonbinary string descriptor columns",
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
        }
        set_invalid_json_data_type_error(database, function_name);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        set_invalid_json_data_type_error(database, function_name);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_CAST_BINARY_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION) {
        set_json_binary_charset_error(database);
        return MYLITE_ERROR;
    }

    set_unsupported_error(
        database,
        "JSON introspection supports only string, NULL, JSON_EXTRACT(), JSON_SET(), "
        "JSON_INSERT(), JSON_REPLACE(), JSON_REMOVE(), and descriptor column documents"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_json_value_document_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "JSON_VALUE() supports only string, NULL, and descriptor column documents"
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
        return plan_row_scalar_json_text_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            "JSON_VALUE() supports only JSON and nonbinary string descriptor columns",
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
        }
        set_invalid_json_data_type_error(database, "json_value");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        set_invalid_json_data_type_error(database, "json_value");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_CAST_BINARY_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION) {
        set_json_binary_charset_error(database);
        return MYLITE_ERROR;
    }

    set_unsupported_error(
        database,
        "JSON_VALUE() supports only string, NULL, and descriptor column documents"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_json_document_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "JSON_EXTRACT() supports only string, NULL, JSON_SET(), JSON_INSERT(), "
            "JSON_REPLACE(), JSON_REMOVE(), and descriptor column documents"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_SET_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_INSERT_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_REPLACE_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_REMOVE_FUNCTION) {
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
        return plan_row_scalar_json_text_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            "JSON_EXTRACT() supports only JSON and nonbinary string descriptor columns",
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
        }
        set_invalid_json_data_type_error(database, "JSON_EXTRACT");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        set_invalid_json_data_type_error(database, "JSON_EXTRACT");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_CAST_BINARY_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION) {
        set_json_binary_charset_error(database);
        return MYLITE_ERROR;
    }

    set_unsupported_error(
        database,
        "JSON_EXTRACT() supports only string, NULL, JSON_SET(), JSON_INSERT(), JSON_REPLACE(), "
        "JSON_REMOVE(), and descriptor column documents"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_json_path_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    struct planned_row_scalar_expression planned = {0};
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "JSON_EXTRACT() supports only string and NULL path literals"
        );
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING &&
        literal_kind != MYLITE_SQL_AST_LITERAL_NULL) {
        set_unsupported_error(
            database,
            "JSON_EXTRACT() supports only string and NULL path literals"
        );
        return MYLITE_ERROR;
    }

    rc = plan_row_scalar_literal_value(database, expression, &planned);
    if (rc == MYLITE_OK && !planned.value.is_null) {
        rc = mylite_json_path_validate(planned.value.text, planned.value.text_length, &result);
        rc = mylite_execution_scalar_json_finish_path_result(database, rc, &result);
    }
    if (rc == MYLITE_OK) {
        *out_expression = planned;
        planned = (struct planned_row_scalar_expression){0};
    }
    planned_row_scalar_expression_deinit(&planned);
    return rc;
}

static int plan_row_scalar_json_value_path_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    struct planned_row_scalar_expression planned = {0};
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(database, "JSON_VALUE() supports only string path literals");
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "JSON_VALUE() supports only string path literals");
        return MYLITE_ERROR;
    }

    rc = plan_row_scalar_literal_value(database, expression, &planned);
    if (rc == MYLITE_OK) {
        rc = mylite_json_path_validate(planned.value.text, planned.value.text_length, &result);
        rc = mylite_execution_scalar_json_finish_path_result(database, rc, &result);
    }
    if (rc == MYLITE_OK) {
        *out_expression = planned;
        planned = (struct planned_row_scalar_expression){0};
    }
    planned_row_scalar_expression_deinit(&planned);
    return rc;
}

static int plan_row_scalar_json_set_path_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (function_name == NULL) {
        return MYLITE_MISUSE;
    }
    const char *path_message = "JSON_SET() supports only string and NULL path literals";

    if (strcmp(function_name, "JSON_REPLACE") == 0) {
        path_message = "JSON_REPLACE() supports only string and NULL path literals";
    } else if (strcmp(function_name, "JSON_INSERT") == 0) {
        path_message = "JSON_INSERT() supports only string and NULL path literals";
    } else if (strcmp(function_name, "JSON_REMOVE") == 0) {
        path_message = "JSON_REMOVE() supports only string and NULL path literals";
    }

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(database, path_message);
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING &&
        literal_kind != MYLITE_SQL_AST_LITERAL_NULL) {
        set_unsupported_error(database, path_message);
        return MYLITE_ERROR;
    }
    return plan_row_scalar_literal_value(database, expression, out_expression);
}

static int plan_row_scalar_json_set_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (function_name == NULL) {
        return MYLITE_MISUSE;
    }
    if (expression == NULL) {
        const char *message =
            "JSON_SET() values support only string, integer, boolean, NULL, JSON functions, "
            "and descriptor column arguments";

        if (strcmp(function_name, "JSON_REPLACE") == 0) {
            message = "JSON_REPLACE() values support only string, integer, boolean, NULL, JSON "
                      "functions, and descriptor column arguments";
        } else if (strcmp(function_name, "JSON_INSERT") == 0) {
            message = "JSON_INSERT() values support only string, integer, boolean, NULL, JSON "
                      "functions, and descriptor column arguments";
        }
        set_unsupported_error(database, message);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_ARRAY_FUNCTION) {
        int rc = plan_row_scalar_json_array_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );

        if (rc == MYLITE_OK) {
            out_expression->json_value_kind = MYLITE_JSON_SQL_VALUE_JSON;
        }
        return rc;
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_OBJECT_FUNCTION) {
        int rc = plan_row_scalar_json_object_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );

        if (rc == MYLITE_OK) {
            out_expression->json_value_kind = MYLITE_JSON_SQL_VALUE_JSON;
        }
        return rc;
    }
    return plan_row_scalar_json_constructor_argument(
        database,
        expression,
        false,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        out_expression
    );
}

static int plan_row_scalar_json_search_path_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "JSON_EXTRACT() supports only string and NULL path literals"
        );
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING &&
        literal_kind != MYLITE_SQL_AST_LITERAL_NULL) {
        set_unsupported_error(
            database,
            "JSON_EXTRACT() supports only string and NULL path literals"
        );
        return MYLITE_ERROR;
    }

    return plan_row_scalar_literal_value(database, expression, out_expression);
}

static int plan_row_scalar_json_length_path_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    struct planned_row_scalar_expression planned = {0};
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "JSON_LENGTH() supports only string and NULL path literals"
        );
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING &&
        literal_kind != MYLITE_SQL_AST_LITERAL_NULL) {
        set_unsupported_error(
            database,
            "JSON_LENGTH() supports only string and NULL path literals"
        );
        return MYLITE_ERROR;
    }

    rc = plan_row_scalar_literal_value(database, expression, &planned);
    if (rc == MYLITE_OK && !planned.value.is_null) {
        rc = mylite_json_path_validate(planned.value.text, planned.value.text_length, &result);
        rc = mylite_execution_scalar_json_finish_length_path_result(database, rc, &result);
    }
    if (rc == MYLITE_OK) {
        *out_expression = planned;
        planned = (struct planned_row_scalar_expression){0};
    }
    planned_row_scalar_expression_deinit(&planned);
    return rc;
}

static int plan_row_scalar_json_keys_path_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    struct planned_row_scalar_expression planned = {0};
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(database, "JSON_KEYS() supports only string and NULL path literals");
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING &&
        literal_kind != MYLITE_SQL_AST_LITERAL_NULL) {
        set_unsupported_error(database, "JSON_KEYS() supports only string and NULL path literals");
        return MYLITE_ERROR;
    }

    rc = plan_row_scalar_literal_value(database, expression, &planned);
    if (rc == MYLITE_OK && !planned.value.is_null) {
        rc = mylite_json_path_validate(planned.value.text, planned.value.text_length, &result);
        rc = mylite_execution_scalar_json_finish_keys_path_result(database, rc, &result);
    }
    if (rc == MYLITE_OK) {
        *out_expression = planned;
        planned = (struct planned_row_scalar_expression){0};
    }
    planned_row_scalar_expression_deinit(&planned);
    return rc;
}

static int plan_row_scalar_json_unquote_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "JSON_UNQUOTE() supports only string, NULL, and descriptor column arguments"
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
        return plan_row_scalar_json_text_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            "JSON_UNQUOTE() supports only JSON and nonbinary string descriptor columns",
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
        }
        set_json_unquote_incorrect_type_error(database);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        set_json_unquote_incorrect_type_error(database);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_CAST_BINARY_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION) {
        set_json_binary_charset_error(database);
        return MYLITE_ERROR;
    }

    set_unsupported_error(
        database,
        "JSON_UNQUOTE() supports only string, NULL, and descriptor column arguments"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_json_quote_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "JSON_QUOTE() supports only string, NULL, and nonbinary string descriptor column "
            "arguments"
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
        return plan_row_scalar_json_quote_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
        }
        set_json_quote_incorrect_type_error(database);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        set_json_quote_incorrect_type_error(database);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_CAST_BINARY_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION) {
        set_json_binary_charset_error(database);
        return MYLITE_ERROR;
    }

    set_unsupported_error(
        database,
        "JSON_QUOTE() supports only string, NULL, and nonbinary string descriptor column arguments"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_json_quote_column(
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
        "row-scalar SELECT JSON_QUOTE() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!json_quote_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool json_quote_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column_descriptor_is_string_family(column)) {
        return true;
    }
    if (column_descriptor_is_binary_string_family(column) || column_descriptor_is_bit(column)) {
        set_json_binary_charset_error(database);
        return false;
    }
    if (column != NULL) {
        set_json_quote_incorrect_type_error(database);
        return false;
    }

    set_unsupported_error(database, "JSON_QUOTE() supports only nonbinary string columns");
    return false;
}

static int plan_row_scalar_json_text_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    const char *unsupported_message,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        column_diagnostic_context,
        "row-scalar SELECT JSON extraction supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!json_text_column_descriptor_is_supported(database, &column, unsupported_message)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool json_text_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const char *unsupported_message
) {
    if (column_descriptor_is_json(column) || column_descriptor_is_string_family(column)) {
        return true;
    }
    if (column_descriptor_is_binary_string_family(column) || column_descriptor_is_bit(column)) {
        set_json_binary_charset_error(database);
        return false;
    }
    set_unsupported_error(database, unsupported_message);
    return false;
}

static int plan_row_scalar_hex_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *argument = NULL;
    struct session_scalar_cell cell = {0};
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_HEX_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "HEX");
        return MYLITE_ERROR;
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (argument != NULL && (argument->kind == MYLITE_SQL_AST_IDENTIFIER ||
                             argument->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        if (!has_source) {
            char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
            char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
            size_t part_count = 0U;

            rc = collect_column_reference_parts(database, argument, parts, &part_count);
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
        return plan_row_scalar_hex_column(
            database,
            argument,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (has_source && argument != NULL &&
        (argument->kind == MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION ||
         argument->kind == MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION)) {
        out_expression->arguments =
            (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
        if (out_expression->arguments == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_HEX;
        out_expression->argument_count = 1U;
        rc = plan_row_scalar_weight_string_expression(
            database,
            argument,
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

    if (!is_hex_projection_expression(expression)) {
        set_hex_unsupported_error(database);
        return MYLITE_ERROR;
    }
    rc = hex_function_value(database, expression, &cell);
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        if (cell.value == NULL) {
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        } else {
            rc = copy_text_value(database, cell.value, &out_expression->value);
        }
    }
    session_scalar_cell_deinit(&cell);
    return rc;
}

static int plan_row_scalar_hex_column(
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
        "row-scalar SELECT HEX() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!hex_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_HEX;
    out_expression->argument_count = 1U;
    out_expression->arguments[0].kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->arguments[0].column = column;
    return MYLITE_OK;
}

static bool hex_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }
    if (column_descriptor_is_string_family(column) ||
        column_descriptor_is_binary_string_family(column)) {
        return true;
    }
    if (column_descriptor_is_decimal(column) || column_descriptor_is_approximate(column) ||
        column_descriptor_is_bit(column) || column_descriptor_is_date(column) ||
        column_descriptor_is_time(column) || column_descriptor_is_datetime(column) ||
        column_descriptor_is_timestamp(column) || column_descriptor_is_year(column)) {
        set_unsupported_error(
            database,
            "HEX() supports only integer, nonbinary string, and binary string columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "HEX() supports only integer, nonbinary string, and binary string columns"
    );
    return false;
}

static int plan_row_scalar_unhex_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *argument = NULL;
    struct session_scalar_cell cell = {0};
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_UNHEX_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "UNHEX");
        return MYLITE_ERROR;
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (argument != NULL && (argument->kind == MYLITE_SQL_AST_IDENTIFIER ||
                             argument->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        if (!has_source) {
            char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
            char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
            size_t part_count = 0U;

            rc = collect_column_reference_parts(database, argument, parts, &part_count);
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
        return plan_row_scalar_unhex_column(
            database,
            argument,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }

    if (!is_unhex_projection_expression(expression)) {
        set_unsupported_error(
            database,
            "UNHEX() supports only string, hex, integer, boolean, NULL, supported session "
            "scalar, and descriptor column arguments"
        );
        return MYLITE_ERROR;
    }
    rc = unhex_function_value(database, expression, &cell);
    if (rc == MYLITE_OK && cell.has_staged_unhex_incorrect_string_warning) {
        rc = append_session_scalar_cell_warnings(database, &cell);
    }
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        if (cell.value == NULL) {
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        } else {
            rc = copy_blob_value(database, cell.value, cell.value_size, &out_expression->value);
        }
    }
    session_scalar_cell_deinit(&cell);
    return rc;
}

static int plan_row_scalar_unhex_column(
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
        "row-scalar SELECT UNHEX() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!unhex_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_UNHEX;
    out_expression->argument_count = 1U;
    out_expression->arguments[0].kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->arguments[0].column = column;
    return MYLITE_OK;
}

static bool unhex_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }
    if (column_descriptor_is_string_family(column) ||
        column_descriptor_is_binary_string_family(column)) {
        return true;
    }
    if (column_descriptor_is_decimal(column) || column_descriptor_is_approximate(column) ||
        column_descriptor_is_bit(column) || column_descriptor_is_date(column) ||
        column_descriptor_is_time(column) || column_descriptor_is_datetime(column) ||
        column_descriptor_is_timestamp(column) || column_descriptor_is_year(column)) {
        set_unsupported_error(
            database,
            "UNHEX() supports only integer, nonbinary string, and binary string columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "UNHEX() supports only integer, nonbinary string, and binary string columns"
    );
    return false;
}

static int plan_row_scalar_base64_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *argument = NULL;
    const char *function_name = NULL;
    enum planned_row_scalar_expression_kind expression_kind = PLANNED_ROW_SCALAR_EXPRESSION_NONE;
    struct session_scalar_cell cell = {0};
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL ||
        (expression->kind != MYLITE_SQL_AST_TO_BASE64_FUNCTION &&
         expression->kind != MYLITE_SQL_AST_FROM_BASE64_FUNCTION) ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "TO_BASE64");
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_TO_BASE64_FUNCTION) {
        function_name = "TO_BASE64";
        expression_kind = PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64;
    } else {
        function_name = "FROM_BASE64";
        expression_kind = PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64;
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (argument != NULL && (argument->kind == MYLITE_SQL_AST_IDENTIFIER ||
                             argument->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        if (!has_source) {
            char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
            char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
            size_t part_count = 0U;

            rc = collect_column_reference_parts(database, argument, parts, &part_count);
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
        return plan_row_scalar_base64_column(
            database,
            argument,
            function_name,
            expression_kind,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }

    if (!is_base64_projection_expression(expression)) {
        set_base64_argument_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    rc = expression->kind == MYLITE_SQL_AST_TO_BASE64_FUNCTION
             ? to_base64_function_value(database, expression, &cell)
             : from_base64_function_value(database, expression, &cell);
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        if (cell.value == NULL) {
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        } else if (expression->kind == MYLITE_SQL_AST_TO_BASE64_FUNCTION) {
            rc = copy_text_value(database, cell.value, &out_expression->value);
        } else {
            rc = copy_blob_value(database, cell.value, cell.value_size, &out_expression->value);
        }
    }
    session_scalar_cell_deinit(&cell);
    return rc;
}

static int plan_row_scalar_base64_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    enum planned_row_scalar_expression_kind expression_kind,
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
        "row-scalar SELECT Base64 functions support only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!base64_column_descriptor_is_supported(database, &column, function_name)) {
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = expression_kind;
    out_expression->argument_count = 1U;
    out_expression->arguments[0].kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->arguments[0].column = column;
    return MYLITE_OK;
}

static bool base64_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const char *function_name
) {
    char message[base64_column_message_capacity];
    int written = snprintf(
        message,
        sizeof(message),
        "%s() supports only integer, nonbinary string, and binary string columns",
        function_name == NULL ? "Base64" : function_name
    );

    if (written < 0 || (size_t)written >= sizeof(message)) {
        set_runtime_error(database, "failed to format Base64 column message");
        return false;
    }
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }
    if (column_descriptor_is_string_family(column) ||
        column_descriptor_is_binary_string_family(column)) {
        return true;
    }
    if (column_descriptor_is_decimal(column) || column_descriptor_is_approximate(column) ||
        column_descriptor_is_bit(column) || column_descriptor_is_date(column) ||
        column_descriptor_is_time(column) || column_descriptor_is_datetime(column) ||
        column_descriptor_is_timestamp(column) || column_descriptor_is_year(column)) {
        set_unsupported_error(database, message);
        return false;
    }

    set_unsupported_error(database, message);
    return false;
}

static int plan_row_scalar_uuid_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *argument = NULL;
    const struct mylite_sql_ast_node *swap_argument = NULL;
    bool handled_source_expression = false;
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_uuid_unsupported_error(database, "UUID conversion");
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    rc = validate_row_scalar_uuid_expression_arity(database, expression, child_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (expression->kind == MYLITE_SQL_AST_UUID_FUNCTION) {
        if (has_source) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_UUID;
            out_expression->argument_count = 0U;
            out_expression->arguments = NULL;
            return MYLITE_OK;
        }
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    swap_argument = unwrap_parenthesized_expression(child_at(expression, 1U));
    rc = plan_row_scalar_uuid_source_argument_expression(
        database,
        expression,
        argument,
        swap_argument,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        out_expression,
        &handled_source_expression
    );
    if (rc != MYLITE_OK || handled_source_expression) {
        return rc;
    }

    return plan_row_scalar_constant_uuid_expression(database, expression, out_expression);
}

static int validate_row_scalar_uuid_expression_arity(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    size_t child_count
) {
    if (expression->kind == MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "UUID");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UUID_FUNCTION && child_count != 0U) {
        set_native_function_parameter_count_error(database, "UUID");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IS_UUID_FUNCTION && child_count != 1U) {
        set_native_function_parameter_count_error(database, "IS_UUID");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION && child_count != 1U &&
        child_count != 2U) {
        set_native_function_parameter_count_error(database, "UUID_TO_BIN");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION && child_count != 1U &&
        child_count != 2U) {
        set_native_function_parameter_count_error(database, "BIN_TO_UUID");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int plan_row_scalar_uuid_source_argument_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *argument,
    const struct mylite_sql_ast_node *swap_argument,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    bool *out_handled
) {
    *out_handled = false;

    if (argument != NULL && (argument->kind == MYLITE_SQL_AST_IDENTIFIER ||
                             argument->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        *out_handled = true;
        if (!has_source) {
            return uuid_unknown_column_argument(database, argument);
        }
        return plan_row_scalar_uuid_column_expression(
            database,
            &(struct row_scalar_uuid_column_expression_request){
                .function_expression = expression,
                .value_argument = argument,
                .swap_argument = swap_argument,
            },
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (has_source && expression->kind != MYLITE_SQL_AST_UUID_FUNCTION &&
        is_row_scalar_uuid_nested_function_argument(argument)) {
        *out_handled = true;
        return plan_row_scalar_uuid_nested_expression(
            database,
            &(struct row_scalar_uuid_column_expression_request){
                .function_expression = expression,
                .value_argument = argument,
                .swap_argument = swap_argument,
            },
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    return MYLITE_OK;
}

static bool is_row_scalar_uuid_nested_function_argument(
    const struct mylite_sql_ast_node *argument
) {
    if (argument == NULL) {
        return false;
    }
    if (argument->kind == MYLITE_SQL_AST_IS_UUID_FUNCTION ||
        argument->kind == MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION ||
        argument->kind == MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION) {
        return true;
    }
    return false;
}

static int plan_row_scalar_constant_uuid_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    struct session_scalar_cell cell = {0};
    int rc = MYLITE_OK;

    if (!is_uuid_projection_expression(expression)) {
        set_uuid_unsupported_error(database, "UUID conversion");
        return MYLITE_ERROR;
    }

    rc = uuid_function_value(database, expression, &cell);
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        if (cell.value == NULL) {
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        } else if (cell.has_value_size) {
            rc = copy_blob_value(database, cell.value, cell.value_size, &out_expression->value);
        } else {
            rc = copy_text_value(database, cell.value, &out_expression->value);
        }
    }
    session_scalar_cell_deinit(&cell);
    return rc;
}

static int plan_row_scalar_uuid_nested_expression(
    struct mylite_db *database,
    const struct row_scalar_uuid_column_expression_request *request,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *nested_argument = NULL;
    const struct mylite_sql_ast_node *nested_swap_argument = NULL;
    bool swap = false;
    size_t nested_child_count = 0U;
    int rc = MYLITE_OK;

    if (request == NULL || request->function_expression == NULL ||
        request->value_argument == NULL) {
        return MYLITE_MISUSE;
    }

    nested_child_count = mylite_sql_ast_node_child_count(request->value_argument);
    if (request->value_argument->kind == MYLITE_SQL_AST_IS_UUID_FUNCTION &&
        nested_child_count != 1U) {
        set_native_function_parameter_count_error(database, "IS_UUID");
        return MYLITE_ERROR;
    }
    if ((request->value_argument->kind == MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION ||
         request->value_argument->kind == MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION) &&
        nested_child_count != 1U && nested_child_count != 2U) {
        set_native_function_parameter_count_error(
            database,
            request->value_argument->kind == MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION ? "UUID_TO_BIN"
                                                                                 : "BIN_TO_UUID"
        );
        return MYLITE_ERROR;
    }

    nested_argument = unwrap_parenthesized_expression(child_at(request->value_argument, 0U));
    nested_swap_argument = unwrap_parenthesized_expression(child_at(request->value_argument, 1U));
    if (nested_argument == NULL || (nested_argument->kind != MYLITE_SQL_AST_IDENTIFIER &&
                                    nested_argument->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT nested UUID conversion supports only one descriptor column "
            "argument"
        );
        return MYLITE_ERROR;
    }

    out_expression->argument_count = request->swap_argument == NULL ? 1U : 2U;
    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(out_expression->argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    if (request->function_expression->kind == MYLITE_SQL_AST_IS_UUID_FUNCTION) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID;
    } else if (request->function_expression->kind == MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN;
    } else {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID;
    }

    rc = plan_row_scalar_uuid_column_expression(
        database,
        &(struct row_scalar_uuid_column_expression_request){
            .function_expression = request->value_argument,
            .value_argument = nested_argument,
            .swap_argument = nested_swap_argument,
        },
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK && request->swap_argument != NULL) {
        rc = uuid_swap_flag_value(database, request->swap_argument, &swap);
    }
    if (rc == MYLITE_OK && request->swap_argument != NULL) {
        out_expression->arguments[1].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[1].value.integer = 0;
        if (swap) {
            out_expression->arguments[1].value.integer = 1;
        }
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(out_expression);
    }
    return rc;
}

static int plan_row_scalar_uuid_column_expression(
    struct mylite_db *database,
    const struct row_scalar_uuid_column_expression_request *request,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    bool swap = false;
    int rc = MYLITE_OK;

    if (request == NULL || request->function_expression == NULL ||
        request->value_argument == NULL) {
        return MYLITE_MISUSE;
    }

    rc = resolve_descriptor_column_reference(
        database,
        request->value_argument,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT UUID conversion supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!uuid_column_descriptor_is_supported(
            database,
            request->function_expression->kind,
            &column
        )) {
        return MYLITE_ERROR;
    }
    if (request->swap_argument != NULL) {
        rc = uuid_swap_flag_value(database, request->swap_argument, &swap);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    out_expression->argument_count = request->swap_argument == NULL ? 1U : 2U;
    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(out_expression->argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    if (request->function_expression->kind == MYLITE_SQL_AST_IS_UUID_FUNCTION) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID;
    } else if (request->function_expression->kind == MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN;
    } else {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID;
    }
    out_expression->arguments[0].kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->arguments[0].column = column;
    if (request->swap_argument != NULL) {
        out_expression->arguments[1].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[1].value.integer = 0;
        if (swap) {
            out_expression->arguments[1].value.integer = 1;
        }
    }
    return MYLITE_OK;
}

static bool uuid_column_descriptor_is_supported(
    struct mylite_db *database,
    enum mylite_sql_ast_node_kind function_kind,
    const struct mylite_catalog_column_descriptor *column
) {
    if (function_kind == MYLITE_SQL_AST_IS_UUID_FUNCTION ||
        function_kind == MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION) {
        if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
            return true;
        }
        if (column_descriptor_is_string_family(column) ||
            column_descriptor_is_binary_string_family(column)) {
            return true;
        }
        set_unsupported_error(
            database,
            "IS_UUID() and UUID_TO_BIN() support only integer, nonbinary string, and binary "
            "string columns"
        );
        return false;
    }

    if (function_kind == MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION) {
        if (column_descriptor_is_string_family(column) ||
            column_descriptor_is_binary_string_family(column)) {
            return true;
        }
        set_unsupported_error(
            database,
            "BIN_TO_UUID() supports only nonbinary string and binary string columns"
        );
        return false;
    }

    set_uuid_unsupported_error(database, "UUID conversion");
    return false;
}

static int plan_row_scalar_binary_string_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    bool *out_handled
) {
    if (out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = true;

    switch (expression->kind) {
    case MYLITE_SQL_AST_UUID_FUNCTION:
    case MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_IS_UUID_FUNCTION:
    case MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION:
    case MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION:
        return plan_row_scalar_uuid_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_HEX_FUNCTION:
        return plan_row_scalar_hex_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_TO_BASE64_FUNCTION:
    case MYLITE_SQL_AST_FROM_BASE64_FUNCTION:
        return plan_row_scalar_base64_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_UNHEX_FUNCTION:
        return plan_row_scalar_unhex_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION:
    case MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION:
        return plan_row_scalar_weight_string_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_CHAR_FUNCTION:
        return plan_row_scalar_char_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    default:
        *out_handled = false;
        return MYLITE_OK;
    }
}

static int plan_row_scalar_char_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    size_t argument_count = 0U;
    size_t argument_index = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CHAR_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(
            database,
            "CHAR() supports one or more integer, boolean, NULL, and descriptor integer column "
            "arguments"
        );
        return MYLITE_ERROR;
    }

    arguments = child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        set_unsupported_error(
            database,
            "CHAR() supports one or more integer, boolean, NULL, and descriptor integer column "
            "arguments"
        );
        return MYLITE_ERROR;
    }
    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count == 0U) {
        set_unsupported_error(
            database,
            "CHAR() supports one or more integer, boolean, NULL, and descriptor integer column "
            "arguments"
        );
        return MYLITE_ERROR;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_CHAR;
    out_expression->argument_count = argument_count;

    for (const struct mylite_sql_ast_node *argument = child_at(arguments, 0U);
         rc == MYLITE_OK && argument != NULL;
         argument = argument->next_sibling) {
        rc = plan_row_scalar_char_argument(
            database,
            argument,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_index]
        );
        ++argument_index;
    }
    return rc;
}

static int plan_row_scalar_char_argument(
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
            "CHAR() supports only integer, boolean, NULL, and descriptor integer column arguments"
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
        return plan_row_scalar_char_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
            literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
        }
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
    }

    set_unsupported_error(
        database,
        "CHAR() supports only integer, boolean, NULL, and descriptor integer column arguments"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_char_column(
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
        "row-scalar SELECT CHAR() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!char_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool char_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }

    set_unsupported_error(database, "CHAR() supports only integer descriptor columns");
    return false;
}

static int plan_row_scalar_charset_collation_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_table_descriptor *table,
    struct planned_row_scalar_expression *out_expression
) {
    enum planned_charset_collation_function_kind function_kind =
        PLANNED_CHARSET_COLLATION_FUNCTION_NONE;
    const struct mylite_sql_ast_node *argument = NULL;
    const struct mylite_sql_ast_node *binary_column_argument = NULL;
    const char *result = NULL;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_CHARSET_COLLATION_FUNCTION_NONE
                                       : charset_collation_function_kind(expression->kind);
    if (function_kind == PLANNED_CHARSET_COLLATION_FUNCTION_NONE ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(
            database,
            "CHARSET(), COLLATION(), and COERCIBILITY() support exactly one argument"
        );
        return MYLITE_ERROR;
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (argument != NULL && (argument->kind == MYLITE_SQL_AST_IDENTIFIER ||
                             argument->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        if (!has_source) {
            return set_unknown_column_for_reference(database, argument);
        }
        return plan_row_scalar_charset_collation_column(
            database,
            argument,
            function_kind,
            source_context,
            table_columns,
            table_column_count,
            table,
            out_expression
        );
    }
    if (function_kind == PLANNED_CHARSET_COLLATION_FUNCTION_COERCIBILITY &&
        coercibility_binary_wrapper_column_reference(argument, &binary_column_argument)) {
        struct mylite_catalog_column_descriptor column = {0};

        if (!has_source) {
            return set_unknown_column_for_reference(database, binary_column_argument);
        }
        rc = resolve_descriptor_column_reference(
            database,
            binary_column_argument,
            source_context,
            COLUMN_REFERENCE_FIELD,
            "row-scalar SELECT COERCIBILITY() binary cast arguments support only descriptor "
            "columns",
            table_columns,
            table_column_count,
            &column
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        return copy_text_value(database, "2", &out_expression->value);
    }

    rc = charset_collation_scalar_result(database, function_kind, argument, &result);
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        rc = copy_text_value(database, result, &out_expression->value);
    }
    return rc;
}

static int plan_row_scalar_charset_collation_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum planned_charset_collation_function_kind function_kind,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_table_descriptor *table,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    const char *result = NULL;
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT CHARSET(), COLLATION(), and COERCIBILITY() support only descriptor "
        "columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = charset_collation_column_result(database, function_kind, table, &column, &result);
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        rc = copy_text_value(database, result, &out_expression->value);
    }
    return rc;
}

static int charset_collation_column_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    const char **out_result
) {
    const char *charset = NULL;
    const char *collation = NULL;

    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }
    if (function_kind == PLANNED_CHARSET_COLLATION_FUNCTION_COERCIBILITY) {
        return coercibility_column_result(database, column, out_result);
    }

    charset = column_effective_character_set_name(table, column);
    collation = column_effective_collation_name(table, column);
    if (charset == NULL || collation == NULL) {
        charset = "binary";
        collation = "binary";
    }
    return charset_collation_select_result(function_kind, charset, collation, out_result);
}

static int coercibility_column_result(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const char **out_result
) {
    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    if (column == NULL) {
        set_unsupported_error(database, "COERCIBILITY() supports only descriptor columns");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_string_family(column) ||
        column_descriptor_is_binary_string_family(column) || column_descriptor_is_bit(column) ||
        column_descriptor_is_enum(column) || column_descriptor_is_set(column) ||
        column_descriptor_is_json(column) || column_descriptor_is_spatial(column)) {
        *out_result = "2";
        return MYLITE_OK;
    }
    if (strcmp(column->physical_type, "INTEGER") == 0 || column_descriptor_is_decimal(column) ||
        column_descriptor_is_approximate(column) || column_descriptor_is_year(column) ||
        column_descriptor_is_date(column) || column_descriptor_is_time(column) ||
        column_descriptor_is_datetime(column) || column_descriptor_is_timestamp(column)) {
        *out_result = "5";
        return MYLITE_OK;
    }

    set_unsupported_error(
        database,
        "COERCIBILITY() supports only descriptor columns with known MyLite type metadata"
    );
    return MYLITE_ERROR;
}

static bool is_row_scalar_control_flow_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_IF_FUNCTION:
    case MYLITE_SQL_AST_IFNULL_FUNCTION:
    case MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_COALESCE_FUNCTION:
    case MYLITE_SQL_AST_NULLIF_FUNCTION:
    case MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ISNULL_FUNCTION:
    case MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR:
        return true;
    default:
        return false;
    }
}

static int plan_row_scalar_control_flow_expression(
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
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_IF_FUNCTION:
        return plan_row_scalar_if_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_IFNULL_FUNCTION:
        return plan_row_scalar_ifnull_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_COALESCE_FUNCTION:
        return plan_row_scalar_coalesce_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_NULLIF_FUNCTION:
        return plan_row_scalar_nullif_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_ISNULL_FUNCTION:
        return plan_row_scalar_isnull_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "IFNULL");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "NULLIF");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "ISNULL");
        return MYLITE_ERROR;
    default:
        break;
    }

    set_unsupported_error(
        database,
        "row-scalar SELECT control-flow functions support only IF(), IFNULL(), COALESCE(), "
        "NULLIF(), and ISNULL()"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_nested_control_flow_expression(
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
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_IF_FUNCTION:
        return plan_row_scalar_nested_if_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_IFNULL_FUNCTION:
        return plan_row_scalar_nested_ifnull_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_COALESCE_FUNCTION:
        return plan_row_scalar_nested_coalesce_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_NULLIF_FUNCTION:
        return plan_row_scalar_nested_nullif_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_ISNULL_FUNCTION:
        return plan_row_scalar_nested_isnull_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "IFNULL");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "NULLIF");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "ISNULL");
        return MYLITE_ERROR;
    default:
        break;
    }

    set_unsupported_error(
        database,
        "nested row-scalar SELECT control-flow functions support only IF(), IFNULL(), "
        "COALESCE(), NULLIF(), and ISNULL()"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_if_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_IF_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 3U) {
        set_native_function_parameter_count_error(database, "IF");
        return MYLITE_ERROR;
    }
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(3U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_IF;
    out_expression->argument_count = 3U;

    rc = plan_row_scalar_control_flow_condition_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_control_flow_value_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[1]
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_control_flow_value_argument(
            database,
            child_at(expression, 2U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[2]
        );
    }
    if (rc == MYLITE_OK) {
        out_expression->field_domain =
            row_scalar_control_flow_common_domain(&out_expression->arguments[1], 2U);
    }
    return rc;
}

static int plan_row_scalar_ifnull_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_IFNULL_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "IFNULL");
        return MYLITE_ERROR;
    }
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_IFNULL;
    out_expression->argument_count = 2U;

    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        rc = plan_row_scalar_control_flow_value_argument(
            database,
            child_at(expression, argument_index),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_index]
        );
    }
    if (rc == MYLITE_OK) {
        out_expression->field_domain =
            row_scalar_control_flow_common_domain(out_expression->arguments, 2U);
    }
    return rc;
}

static int plan_row_scalar_coalesce_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *arguments = child_at(expression, 0U);
    const struct mylite_sql_ast_node *argument = NULL;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_COALESCE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U || arguments == NULL ||
        arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (argument_count > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COALESCE;
    out_expression->argument_count = argument_count;

    argument = child_at(arguments, 0U);
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         ++argument_index) {
        rc = plan_row_scalar_control_flow_value_argument(
            database,
            argument,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_index]
        );
        argument = argument->next_sibling;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument != NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    out_expression->field_domain =
        row_scalar_control_flow_common_domain(out_expression->arguments, argument_count);
    return MYLITE_OK;
}

static int plan_row_scalar_nullif_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum planned_row_scalar_field_domain left_domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    enum planned_row_scalar_field_domain right_domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_NULLIF_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "NULLIF");
        return MYLITE_ERROR;
    }
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_NULLIF;
    out_expression->argument_count = 2U;

    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        rc = plan_row_scalar_control_flow_value_argument(
            database,
            child_at(expression, argument_index),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_index]
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    left_domain = row_scalar_control_flow_argument_domain(&out_expression->arguments[0]);
    right_domain = row_scalar_control_flow_argument_domain(&out_expression->arguments[1]);
    if (left_domain != PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE &&
        right_domain != PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE && left_domain != right_domain) {
        set_unsupported_error(
            database,
            "NULLIF() row projection does not support mixed string and numeric arguments"
        );
        return MYLITE_ERROR;
    }
    out_expression->field_domain =
        left_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE ? right_domain : left_domain;
    return MYLITE_OK;
}

static int plan_row_scalar_isnull_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_ISNULL_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "ISNULL");
        return MYLITE_ERROR;
    }
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_ISNULL;
    out_expression->argument_count = 1U;
    out_expression->field_domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;

    rc = plan_row_scalar_control_flow_value_argument(
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

static int plan_row_scalar_nested_if_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_IF_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 3U) {
        set_native_function_parameter_count_error(database, "IF");
        return MYLITE_ERROR;
    }
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(3U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_IF;
    out_expression->argument_count = 3U;

    rc = plan_row_scalar_control_flow_leaf_condition_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_control_flow_leaf_value_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[1]
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_control_flow_leaf_value_argument(
            database,
            child_at(expression, 2U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[2]
        );
    }
    if (rc == MYLITE_OK) {
        out_expression->field_domain =
            row_scalar_control_flow_common_domain(&out_expression->arguments[1], 2U);
    }
    return rc;
}

static int plan_row_scalar_nested_ifnull_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_IFNULL_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "IFNULL");
        return MYLITE_ERROR;
    }
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_IFNULL;
    out_expression->argument_count = 2U;

    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        rc = plan_row_scalar_control_flow_leaf_value_argument(
            database,
            child_at(expression, argument_index),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_index]
        );
    }
    if (rc == MYLITE_OK) {
        out_expression->field_domain =
            row_scalar_control_flow_common_domain(out_expression->arguments, 2U);
    }
    return rc;
}

static int plan_row_scalar_nested_coalesce_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *arguments = child_at(expression, 0U);
    const struct mylite_sql_ast_node *argument = NULL;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_COALESCE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U || arguments == NULL ||
        arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (argument_count > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COALESCE;
    out_expression->argument_count = argument_count;

    argument = child_at(arguments, 0U);
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         ++argument_index) {
        rc = plan_row_scalar_control_flow_leaf_value_argument(
            database,
            argument,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_index]
        );
        argument = argument->next_sibling;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument != NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    out_expression->field_domain =
        row_scalar_control_flow_common_domain(out_expression->arguments, argument_count);
    return MYLITE_OK;
}

static int plan_row_scalar_nested_nullif_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum planned_row_scalar_field_domain left_domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    enum planned_row_scalar_field_domain right_domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_NULLIF_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "NULLIF");
        return MYLITE_ERROR;
    }
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_NULLIF;
    out_expression->argument_count = 2U;

    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        rc = plan_row_scalar_control_flow_leaf_value_argument(
            database,
            child_at(expression, argument_index),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_index]
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    left_domain = row_scalar_control_flow_argument_domain(&out_expression->arguments[0]);
    right_domain = row_scalar_control_flow_argument_domain(&out_expression->arguments[1]);
    if (left_domain != PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE &&
        right_domain != PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE && left_domain != right_domain) {
        set_unsupported_error(
            database,
            "NULLIF() row projection does not support mixed string and numeric arguments"
        );
        return MYLITE_ERROR;
    }
    out_expression->field_domain =
        left_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE ? right_domain : left_domain;
    return MYLITE_OK;
}

static int plan_row_scalar_nested_isnull_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_ISNULL_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "ISNULL");
        return MYLITE_ERROR;
    }
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_ISNULL;
    out_expression->argument_count = 1U;
    out_expression->field_domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;

    rc = plan_row_scalar_control_flow_leaf_value_argument(
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

static int plan_row_scalar_control_flow_value_argument(
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
            "row-scalar SELECT control-flow functions support only descriptor columns, scalar "
            "literals, session scalar values, system variables, and nested control-flow functions"
        );
        return MYLITE_ERROR;
    }
    if (is_row_scalar_control_flow_expression(expression)) {
        return plan_row_scalar_nested_control_flow_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
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
        return plan_row_scalar_control_flow_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }

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

static int plan_row_scalar_control_flow_condition_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "IF() row conditions support only integer, boolean, NULL, descriptor integer column, "
            "and ISNULL() values"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
            literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
        }
        set_unsupported_error(
            database,
            "IF() row conditions support only integer, boolean, NULL, descriptor integer column, "
            "and ISNULL() values"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
    }
    if (expression->kind == MYLITE_SQL_AST_ISNULL_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR) {
        return plan_row_scalar_isnull_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
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
        return plan_row_scalar_control_flow_condition_column(
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
        "IF() row conditions support only integer, boolean, NULL, descriptor integer column, "
        "and ISNULL() values"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_control_flow_leaf_value_argument(
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
            "nested row-scalar SELECT control-flow functions support only descriptor columns "
            "and scalar literals"
        );
        return MYLITE_ERROR;
    }
    if (is_row_scalar_control_flow_expression(expression)) {
        set_unsupported_error(
            database,
            "nested row-scalar SELECT control-flow functions support only one nested "
            "control-flow layer"
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
        return plan_row_scalar_control_flow_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }

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

static int plan_row_scalar_control_flow_leaf_condition_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "nested IF() row conditions support only integer, boolean, NULL, and descriptor "
            "integer column values"
        );
        return MYLITE_ERROR;
    }
    if (is_row_scalar_control_flow_expression(expression)) {
        set_unsupported_error(
            database,
            "nested row-scalar SELECT control-flow functions support only one nested "
            "control-flow layer"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
            literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
        }
        set_unsupported_error(
            database,
            "nested IF() row conditions support only integer, boolean, NULL, and descriptor "
            "integer column values"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
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
        return plan_row_scalar_control_flow_condition_column(
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
        "nested IF() row conditions support only integer, boolean, NULL, and descriptor "
        "integer column values"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_control_flow_column(
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
        "row-scalar SELECT control-flow functions support only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!control_flow_value_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_control_flow_condition_column(
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
        "IF() row conditions support only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!control_flow_condition_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool control_flow_value_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }
    if (column_descriptor_is_string_family(column) || column_descriptor_is_date(column) ||
        column_descriptor_is_time(column) || column_descriptor_is_datetime(column) ||
        column_descriptor_is_timestamp(column) || column_descriptor_is_year(column)) {
        return true;
    }
    if (column_descriptor_is_decimal(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT control-flow functions do not support DECIMAL columns"
        );
        return false;
    }
    if (column_descriptor_is_binary_string_family(column) || column_descriptor_is_bit(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT control-flow functions do not support binary string or BIT columns"
        );
        return false;
    }
    if (column_descriptor_is_approximate(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT control-flow functions do not support approximate numeric columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "row-scalar SELECT control-flow functions support only integer, nonbinary string, YEAR, "
        "and temporal columns"
    );
    return false;
}

static bool control_flow_condition_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }
    set_unsupported_error(database, "IF() row conditions support only integer descriptor columns");
    return false;
}

static enum planned_row_scalar_field_domain row_scalar_control_flow_argument_domain(
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL) {
        return PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    }
    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        if (expression->value.is_null) {
            return PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
        }
        if (expression->value.is_text) {
            return PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
        }
        return PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return row_scalar_control_flow_column_domain(&expression->column);
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
        return expression->field_domain;
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
        return PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
        if (expression->conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_SIGNED ||
            expression->conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_UNSIGNED) {
            return PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
        }
        if (expression->conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_CHAR ||
            expression->conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_USING_CHARSET) {
            return PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
        }
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
        break;
    }
    return PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
}

static enum planned_row_scalar_field_domain row_scalar_control_flow_column_domain(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
    }
    if (column_descriptor_is_string_family(column) || column_descriptor_is_date(column) ||
        column_descriptor_is_time(column) || column_descriptor_is_datetime(column) ||
        column_descriptor_is_timestamp(column) || column_descriptor_is_year(column)) {
        return PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
    }
    return PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
}

static enum planned_row_scalar_field_domain row_scalar_control_flow_common_domain(
    const struct planned_row_scalar_expression *arguments,
    size_t argument_count
) {
    enum planned_row_scalar_field_domain domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;

    for (size_t argument_index = 0U; argument_index < argument_count; ++argument_index) {
        enum planned_row_scalar_field_domain argument_domain =
            row_scalar_control_flow_argument_domain(&arguments[argument_index]);

        if (argument_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE) {
            continue;
        }
        if (domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE) {
            domain = argument_domain;
        } else if (domain != argument_domain) {
            return PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
        }
    }

    return domain;
}

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
    default:
        break;
    }

    set_unsupported_error(
        database,
        "row-scalar SELECT supports only string, integer, boolean, and NULL literals"
    );
    return MYLITE_ERROR;
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
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!row_scalar_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_concat_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *arguments = child_at(expression, 0U);
    const struct mylite_sql_ast_node *argument = NULL;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_child_count(expression) != 1U || arguments == NULL ||
        arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        set_unsupported_error(database, "CONCAT() requires one or more arguments");
        return MYLITE_ERROR;
    }

    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count == 0U || argument_count > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_CONCAT;
    out_expression->argument_count = argument_count;

    argument = child_at(arguments, 0U);
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         ++argument_index) {
        const struct mylite_sql_ast_node *unwrapped_argument =
            unwrap_parenthesized_expression(argument);

        if (unwrapped_argument != NULL &&
            unwrapped_argument->kind == MYLITE_SQL_AST_CONCAT_FUNCTION) {
            set_unsupported_error(
                database,
                "row-scalar SELECT CONCAT() does not support nested CONCAT()"
            );
            rc = MYLITE_ERROR;
        } else {
            rc = plan_row_scalar_non_concat_expression(
                database,
                argument,
                has_source,
                source_context,
                table_columns,
                table_column_count,
                true,
                &out_expression->arguments[argument_index]
            );
        }
        argument = argument->next_sibling;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument != NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int plan_row_scalar_concat_operator_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    size_t argument_count = 0U;
    size_t next_argument = 0U;
    int rc = count_row_scalar_concat_operator_arguments(database, expression, &argument_count);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument_count == 0U || argument_count > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_CONCAT;
    out_expression->argument_count = argument_count;

    rc = plan_row_scalar_concat_operator_arguments(
        database,
        expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        out_expression->arguments,
        argument_count,
        &next_argument
    );
    if (rc == MYLITE_OK && next_argument != argument_count) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    return rc;
}

static int count_row_scalar_concat_operator_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    size_t *inout_argument_count
) {
    struct scalar_arithmetic_node_stack stack = {0};
    int rc = MYLITE_OK;

    if (expression == NULL || inout_argument_count == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    while (rc == MYLITE_OK && stack.count > 0U) {
        const struct mylite_sql_ast_node *current = NULL;

        --stack.count;
        current = unwrap_parenthesized_expression(stack.items[stack.count]);
        if (current == NULL) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
            break;
        }
        if (row_scalar_expression_is_concat_operator(current)) {
            if (!scalar_arithmetic_node_stack_push(&stack, child_at(current, 1U)) ||
                !scalar_arithmetic_node_stack_push(&stack, child_at(current, 0U))) {
                set_nomem_error(database);
                rc = MYLITE_NOMEM;
            }
            continue;
        }
        if (*inout_argument_count == SIZE_MAX) {
            set_nomem_error(database);
            rc = MYLITE_NOMEM;
            break;
        }
        ++(*inout_argument_count);
    }

    scalar_arithmetic_node_stack_deinit(&stack);
    return rc;
}

static int plan_row_scalar_concat_operator_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *arguments,
    size_t argument_count,
    size_t *inout_next_argument
) {
    struct scalar_arithmetic_node_stack stack = {0};
    int rc = MYLITE_OK;

    if (expression == NULL || arguments == NULL || inout_next_argument == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    while (rc == MYLITE_OK && stack.count > 0U) {
        const struct mylite_sql_ast_node *current = NULL;

        --stack.count;
        current = unwrap_parenthesized_expression(stack.items[stack.count]);
        if (current == NULL) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
            break;
        }
        if (row_scalar_expression_is_concat_operator(current)) {
            if (!scalar_arithmetic_node_stack_push(&stack, child_at(current, 1U)) ||
                !scalar_arithmetic_node_stack_push(&stack, child_at(current, 0U))) {
                set_nomem_error(database);
                rc = MYLITE_NOMEM;
            }
            continue;
        }
        if (current->kind == MYLITE_SQL_AST_CONCAT_FUNCTION ||
            current->kind == MYLITE_SQL_AST_CONCAT_WS_FUNCTION) {
            set_unsupported_error(
                database,
                "row-scalar SELECT || does not support nested CONCAT()"
            );
            rc = MYLITE_ERROR;
            break;
        }
        if (*inout_next_argument >= argument_count) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
            break;
        }

        rc = plan_row_scalar_non_concat_expression(
            database,
            current,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            true,
            &arguments[*inout_next_argument]
        );
        if (rc == MYLITE_OK) {
            ++(*inout_next_argument);
        }
    }

    scalar_arithmetic_node_stack_deinit(&stack);
    return rc;
}

static bool row_scalar_expression_is_concat_operator(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return false;
    }
    return mylite_sql_ast_node_operator(expression) == MYLITE_SQL_AST_OPERATOR_CONCAT;
}

static int plan_row_scalar_concat_ws_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *arguments = child_at(expression, 0U);
    const struct mylite_sql_ast_node *argument = NULL;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONCAT_WS_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U || arguments == NULL ||
        arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        set_native_function_parameter_count_error(database, "CONCAT_WS");
        return MYLITE_ERROR;
    }

    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count < 2U) {
        set_native_function_parameter_count_error(database, "CONCAT_WS");
        return MYLITE_ERROR;
    }
    if (argument_count > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS;
    out_expression->argument_count = argument_count;

    argument = child_at(arguments, 0U);
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         ++argument_index) {
        const struct mylite_sql_ast_node *unwrapped_argument =
            unwrap_parenthesized_expression(argument);

        if (unwrapped_argument != NULL &&
            (unwrapped_argument->kind == MYLITE_SQL_AST_CONCAT_FUNCTION ||
             unwrapped_argument->kind == MYLITE_SQL_AST_CONCAT_WS_FUNCTION)) {
            set_unsupported_error(
                database,
                "row-scalar SELECT CONCAT_WS() does not support nested CONCAT() or CONCAT_WS()"
            );
            rc = MYLITE_ERROR;
        } else {
            rc = plan_row_scalar_concat_ws_argument(
                database,
                argument,
                has_source,
                source_context,
                table_columns,
                table_column_count,
                &out_expression->arguments[argument_index]
            );
        }
        argument = argument->next_sibling;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument != NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int plan_row_scalar_concat_ws_argument(
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
            "CONCAT_WS() supports only string, integer, boolean, NULL, session scalar, "
            "system variable, scalar subquery, and descriptor column arguments"
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
        return plan_row_scalar_concat_ws_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }

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

static int plan_row_scalar_concat_ws_column(
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
        "row-scalar SELECT CONCAT_WS() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!concat_ws_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool concat_ws_column_descriptor_is_supported(
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
            "row-scalar SELECT CONCAT_WS() does not support binary string or BIT columns"
        );
        return false;
    }
    if (column_descriptor_is_approximate(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT CONCAT_WS() does not support approximate numeric columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "row-scalar SELECT CONCAT_WS() supports only integer, DECIMAL, nonbinary string, YEAR, "
        "and temporal columns"
    );
    return false;
}

static int plan_row_scalar_date_format_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_date_format_input_kind input_kind = MYLITE_DATE_FORMAT_INPUT_STRING;
    const char *input_kind_name = NULL;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_DATE_FORMAT_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "DATE_FORMAT");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(3U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT;
    out_expression->argument_count = 3U;

    rc = plan_row_scalar_date_format_value_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0],
        &input_kind
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_date_format_format_argument(
            database,
            child_at(expression, 1U),
            &out_expression->arguments[1]
        );
    }
    input_kind_name = mylite_date_format_input_kind_name(input_kind);
    if (rc == MYLITE_OK && input_kind_name != NULL) {
        rc = copy_text_value(database, input_kind_name, &out_expression->arguments[2].value);
        out_expression->arguments[2].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    return rc;
}

static int plan_row_scalar_get_format_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    struct session_scalar_cell cell = {0};
    int rc = get_format_function_value(database, expression, &cell);

    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        if (cell.value == NULL) {
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        } else {
            rc = copy_text_value(database, cell.value, &out_expression->value);
        }
    }
    session_scalar_cell_deinit(&cell);
    return rc;
}

static int plan_row_scalar_date_format_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_format_input_kind *out_input_kind
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_input_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input_kind = MYLITE_DATE_FORMAT_INPUT_STRING;
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "DATE_FORMAT() supports only string, DATE, DATETIME, TIMESTAMP, and NULL values"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
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
        return plan_row_scalar_date_format_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression,
            out_input_kind
        );
    }

    set_unsupported_error(
        database,
        "DATE_FORMAT() supports only string, DATE, DATETIME, TIMESTAMP, and NULL values"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_date_format_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_format_input_kind *out_input_kind
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT DATE_FORMAT() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_date(&column)) {
        *out_input_kind = MYLITE_DATE_FORMAT_INPUT_DATE;
    } else if (column_descriptor_is_datetime(&column)) {
        *out_input_kind = MYLITE_DATE_FORMAT_INPUT_DATETIME;
    } else if (column_descriptor_is_timestamp(&column)) {
        *out_input_kind = MYLITE_DATE_FORMAT_INPUT_TIMESTAMP;
    } else if (column_descriptor_is_string_family(&column)) {
        *out_input_kind = MYLITE_DATE_FORMAT_INPUT_STRING;
    } else if (column_descriptor_is_time(&column)) {
        set_unsupported_error(database, "DATE_FORMAT() does not yet support TIME values");
        return MYLITE_ERROR;
    } else {
        set_unsupported_error(
            database,
            "DATE_FORMAT() supports only string, DATE, DATETIME, TIMESTAMP, and NULL values"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_date_format_format_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_GET_FORMAT_FUNCTION) {
        return plan_row_scalar_get_format_format_argument(
            database,
            expression,
            "DATE_FORMAT() literals do not support NUL bytes",
            mylite_date_format_validate_format,
            out_expression
        );
    }
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "DATE_FORMAT() supports only string format literals and NULL"
        );
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "DATE_FORMAT() supports only string format literals and NULL"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        expression,
        "DATE_FORMAT() supports only string format literals and NULL",
        "DATE_FORMAT() literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, "DATE_FORMAT() literals do not support NUL bytes");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_date_format_validate_format(database, text, text_length);
    }
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        rc = assign_text_value(database, text, text_length, &out_expression->value);
        text = NULL;
    }
    free(text);
    return rc;
}

static int plan_row_scalar_get_format_format_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *literal_nul_message,
    int (*validate_format)(struct mylite_db *, const char *, size_t),
    struct planned_row_scalar_expression *out_expression
) {
    struct session_scalar_cell cell = {0};
    int rc = MYLITE_OK;

    if (out_expression == NULL || validate_format == NULL) {
        return MYLITE_MISUSE;
    }

    rc = get_format_function_value(database, expression, &cell);
    if (rc == MYLITE_OK && cell.value == NULL) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
    } else if (rc == MYLITE_OK) {
        size_t text_length = strlen(cell.value);

        if (memchr(cell.value, '\0', text_length) != NULL) {
            set_unsupported_error(database, literal_nul_message);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = validate_format(database, cell.value, text_length);
        }
        if (rc == MYLITE_OK) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            rc = copy_text_value(database, cell.value, &out_expression->value);
        }
    }
    session_scalar_cell_deinit(&cell);
    return rc;
}

static int plan_row_scalar_time_format_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_date_format_input_kind input_kind = MYLITE_DATE_FORMAT_INPUT_STRING;
    const char *input_kind_name = NULL;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_TIME_FORMAT_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "TIME_FORMAT");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(3U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT;
    out_expression->argument_count = 3U;

    rc = plan_row_scalar_time_format_value_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0],
        &input_kind
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_time_format_format_argument(
            database,
            child_at(expression, 1U),
            &out_expression->arguments[1]
        );
    }
    input_kind_name = mylite_date_format_input_kind_name(input_kind);
    if (rc == MYLITE_OK && input_kind_name != NULL) {
        rc = copy_text_value(database, input_kind_name, &out_expression->arguments[2].value);
        out_expression->arguments[2].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    return rc;
}

static int plan_row_scalar_time_format_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_format_input_kind *out_input_kind
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_input_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input_kind = MYLITE_DATE_FORMAT_INPUT_STRING;
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "TIME_FORMAT() supports only string, DATE, TIME, DATETIME, TIMESTAMP, and NULL values"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
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
        return plan_row_scalar_time_format_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression,
            out_input_kind
        );
    }

    set_unsupported_error(
        database,
        "TIME_FORMAT() supports only string, DATE, TIME, DATETIME, TIMESTAMP, and NULL values"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_time_format_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_format_input_kind *out_input_kind
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT TIME_FORMAT() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_time(&column)) {
        *out_input_kind = MYLITE_DATE_FORMAT_INPUT_TIME;
    } else if (column_descriptor_is_date(&column)) {
        *out_input_kind = MYLITE_DATE_FORMAT_INPUT_DATE;
    } else if (column_descriptor_is_datetime(&column)) {
        *out_input_kind = MYLITE_DATE_FORMAT_INPUT_DATETIME;
    } else if (column_descriptor_is_timestamp(&column)) {
        *out_input_kind = MYLITE_DATE_FORMAT_INPUT_TIMESTAMP;
    } else if (column_descriptor_is_string_family(&column)) {
        *out_input_kind = MYLITE_DATE_FORMAT_INPUT_STRING;
    } else {
        set_unsupported_error(
            database,
            "TIME_FORMAT() supports only string, DATE, TIME, DATETIME, TIMESTAMP, and NULL values"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_time_format_format_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_GET_FORMAT_FUNCTION) {
        return plan_row_scalar_get_format_format_argument(
            database,
            expression,
            "TIME_FORMAT() literals do not support NUL bytes",
            mylite_time_format_validate_format,
            out_expression
        );
    }
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "TIME_FORMAT() supports only string format literals and NULL"
        );
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "TIME_FORMAT() supports only string format literals and NULL"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        expression,
        "TIME_FORMAT() supports only string format literals and NULL",
        "TIME_FORMAT() literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, "TIME_FORMAT() literals do not support NUL bytes");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_time_format_validate_format(database, text, text_length);
    }
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        rc = assign_text_value(database, text, text_length, &out_expression->value);
        text = NULL;
    }
    free(text);
    return rc;
}

static int plan_row_scalar_str_to_date_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *value_expression = NULL;
    const struct mylite_sql_ast_node *format_expression = NULL;
    bool handled = false;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_STR_TO_DATE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "STR_TO_DATE");
        return MYLITE_ERROR;
    }

    value_expression = child_at(expression, 0U);
    format_expression = child_at(expression, 1U);
    rc = plan_row_scalar_str_to_date_null_short_circuit(
        database,
        value_expression,
        format_expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &handled,
        out_expression
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE;
    out_expression->argument_count = 2U;

    rc = plan_row_scalar_str_to_date_value_argument(
        database,
        value_expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_str_to_date_format_argument(
            database,
            format_expression,
            &out_expression->arguments[1]
        );
    }
    return rc;
}

static int plan_row_scalar_str_to_date_null_short_circuit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_expression,
    const struct mylite_sql_ast_node *format_expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool *out_handled,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *reference_expression = NULL;
    int rc = MYLITE_OK;

    if (out_handled == NULL || out_expression == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!str_to_date_child_is_null_literal(value_expression) &&
        !str_to_date_child_is_null_literal(format_expression)) {
        return MYLITE_OK;
    }

    if (str_to_date_child_is_null_literal(value_expression)) {
        reference_expression = format_expression;
    } else if (str_to_date_child_is_null_literal(format_expression)) {
        reference_expression = value_expression;
    }
    if (reference_expression != NULL) {
        rc = plan_row_scalar_str_to_date_resolve_short_circuit_references(
            database,
            reference_expression,
            has_source,
            source_context,
            table_columns,
            table_column_count
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
    *out_handled = true;
    return MYLITE_OK;
}

static int plan_row_scalar_str_to_date_resolve_short_circuit_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    struct mylite_catalog_column_descriptor column = {0};

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

    return resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT STR_TO_DATE() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );
}

static int plan_row_scalar_str_to_date_resolve_short_circuit_references(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    struct scalar_arithmetic_node_stack stack = {0};
    int rc = MYLITE_OK;

    if (expression == NULL) {
        return MYLITE_OK;
    }
    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    while (rc == MYLITE_OK && stack.count != 0U) {
        const struct mylite_sql_ast_node *current =
            unwrap_parenthesized_expression(stack.items[--stack.count]);
        size_t child_count = 0U;

        if (current == NULL) {
            continue;
        }
        if (str_to_date_child_is_identifier_reference(current)) {
            rc = plan_row_scalar_str_to_date_resolve_short_circuit_reference(
                database,
                current,
                has_source,
                source_context,
                table_columns,
                table_column_count
            );
            continue;
        }

        child_count = mylite_sql_ast_node_child_count(current);
        for (size_t index = child_count; rc == MYLITE_OK && index > 0U; --index) {
            if (!scalar_arithmetic_node_stack_push(&stack, child_at(current, index - 1U))) {
                set_nomem_error(database);
                rc = MYLITE_NOMEM;
            }
        }
    }
    scalar_arithmetic_node_stack_deinit(&stack);
    return rc;
}

static int plan_row_scalar_str_to_date_value_argument(
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
        set_unsupported_error(database, "STR_TO_DATE() supports only string and NULL values");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
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
        return plan_row_scalar_str_to_date_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }

    set_unsupported_error(database, "STR_TO_DATE() supports only string and NULL values");
    return MYLITE_ERROR;
}

static int plan_row_scalar_str_to_date_column(
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
        "row-scalar SELECT STR_TO_DATE() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!column_descriptor_is_string_family(&column)) {
        set_unsupported_error(database, "STR_TO_DATE() supports only nonbinary string columns");
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_str_to_date_format_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_GET_FORMAT_FUNCTION) {
        return plan_row_scalar_get_format_format_argument(
            database,
            expression,
            "STR_TO_DATE() literals do not support NUL bytes",
            mylite_str_to_date_validate_format,
            out_expression
        );
    }
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "STR_TO_DATE() supports only string format literals and NULL"
        );
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "STR_TO_DATE() supports only string format literals and NULL"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        expression,
        "STR_TO_DATE() supports only string format literals and NULL",
        "STR_TO_DATE() literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, "STR_TO_DATE() literals do not support NUL bytes");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_str_to_date_validate_format(database, text, text_length);
    }
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        rc = assign_text_value(database, text, text_length, &out_expression->value);
        text = NULL;
    }
    free(text);
    return rc;
}

static int plan_row_scalar_date_format_numeric_equal_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *date_format = NULL;
    const struct mylite_sql_ast_node *numeric = NULL;
    int rc = MYLITE_OK;

    if (!row_scalar_date_format_numeric_equal_sides(expression, &date_format, &numeric)) {
        set_unsupported_error(
            database,
            "DATE_FORMAT() numeric comparison supports only DATE_FORMAT(value, format) = "
            "numeric_literal"
        );
        return MYLITE_ERROR;
    }
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL;
    out_expression->argument_count = 2U;

    rc = plan_row_scalar_date_format_expression(
        database,
        date_format,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK &&
        !planned_date_format_numeric_equal_format_is_supported(&out_expression->arguments[0])) {
        set_unsupported_error(
            database,
            "DATE_FORMAT() numeric comparison supports only DATE_FORMAT(value, '%H.%i') = "
            "numeric_literal"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_date_format_numeric_literal(
            database,
            numeric,
            &out_expression->arguments[1]
        );
    }
    return rc;
}

static bool row_scalar_date_format_numeric_equal_sides(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node **out_date_format,
    const struct mylite_sql_ast_node **out_numeric
) {
    const struct mylite_sql_ast_node *left = NULL;
    const struct mylite_sql_ast_node *right = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (out_date_format == NULL || out_numeric == NULL || expression == NULL ||
        expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        mylite_sql_ast_node_operator(expression) != MYLITE_SQL_AST_OPERATOR_EQUAL) {
        return false;
    }
    left = unwrap_parenthesized_expression(child_at(expression, 0U));
    right = unwrap_parenthesized_expression(child_at(expression, 1U));
    if (left != NULL && left->kind == MYLITE_SQL_AST_DATE_FORMAT_FUNCTION &&
        date_format_numeric_literal_expression(right)) {
        *out_date_format = left;
        *out_numeric = right;
        return true;
    }
    if (right != NULL && right->kind == MYLITE_SQL_AST_DATE_FORMAT_FUNCTION &&
        date_format_numeric_literal_expression(left)) {
        *out_date_format = right;
        *out_numeric = left;
        return true;
    }
    return false;
}

static bool row_scalar_date_format_equal_attempt(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *left = NULL;
    const struct mylite_sql_ast_node *right = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        mylite_sql_ast_node_operator(expression) != MYLITE_SQL_AST_OPERATOR_EQUAL) {
        return false;
    }
    left = unwrap_parenthesized_expression(child_at(expression, 0U));
    right = unwrap_parenthesized_expression(child_at(expression, 1U));
    return ((left != NULL && left->kind == MYLITE_SQL_AST_DATE_FORMAT_FUNCTION) ||
            (right != NULL && right->kind == MYLITE_SQL_AST_DATE_FORMAT_FUNCTION)) != 0;
}

static bool date_format_numeric_literal_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(expression);

    if (literal != NULL && literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(literal);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        literal = unwrap_parenthesized_expression(child_at(literal, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }
    return (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER ||
            mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_DECIMAL) != 0;
}

static int plan_row_scalar_date_format_numeric_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(expression);
    bool is_negative = false;
    char *text = NULL;
    size_t text_length = 0U;

    if (literal != NULL && literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(literal);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, "DATE_FORMAT() numeric comparison requires a literal");
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(literal, 0U));
    }
    if (!date_format_numeric_literal_expression(expression) || literal == NULL) {
        set_unsupported_error(database, "DATE_FORMAT() numeric comparison requires a literal");
        return MYLITE_ERROR;
    }
    text_length = literal->span.length;
    if (is_negative) {
        ++text_length;
    }
    text = (char *)malloc(text_length + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (is_negative) {
        text[0] = '-';
        memcpy(text + 1, literal->span.text, literal->span.length);
    } else {
        memcpy(text, literal->span.text, literal->span.length);
    }
    text[text_length] = '\0';
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    return assign_text_value(database, text, text_length, &out_expression->value);
}

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
    if (expression == NULL || !is_date_interval_second_function_kind(expression->kind)) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }
    function_name = date_interval_second_function_name(expression->kind);
    rc = validate_date_interval_second_function_shape(database, expression, function_name);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (expression->kind != MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION) {
        rc = date_interval_unit_from_ast(
            database,
            date_interval_unit_node(expression),
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
        date_interval_second_temporal_node(expression),
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
            date_interval_second_interval_node(expression),
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
        if (date_interval_second_function_subtracts(expression->kind)) {
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
    if (!date_interval_second_message(
            unsupported_message,
            sizeof(unsupported_message),
            function_name,
            "supports only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor "
            "columns, string descriptor columns, and NULL"
        ) ||
        !date_interval_second_message(
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
    int rc = date_interval_second_interval_argument(
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

    if (!date_interval_second_message(message, sizeof(message), function_name, suffix)) {
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

    rc = resolve_temporal_extract_call(database, expression, &extract_kind, &argument, &mode);
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
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT temporal extract functions support only descriptor columns",
        table_columns,
        table_column_count,
        &column
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

static int plan_row_scalar_sec_to_time_expression(
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
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "SEC_TO_TIME");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME;
    out_expression->argument_count = 1U;

    rc = plan_row_scalar_sec_to_time_argument(
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

static int plan_row_scalar_sec_to_time_argument(
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
            "SEC_TO_TIME() supports only signed integer, boolean, NULL, and integer descriptor "
            "arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = false, .integer = 1};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = false, .integer = 0};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
            return plan_row_scalar_integer_value(database, expression, out_expression);
        }
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
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
        return plan_row_scalar_sec_to_time_column(
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
        "SEC_TO_TIME() supports only signed integer, boolean, NULL, and integer descriptor "
        "arguments"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_sec_to_time_column(
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
        "row-scalar SELECT SEC_TO_TIME() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (strcmp(column.physical_type, "INTEGER") != 0) {
        set_unsupported_error(database, "SEC_TO_TIME() supports only integer descriptor columns");
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_from_unixtime_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    size_t child_count = 0U;
    int rc = MYLITE_OK;

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

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME;
    out_expression->argument_count = 1U;

    rc = plan_row_scalar_from_unixtime_argument(
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

static int plan_row_scalar_from_unixtime_argument(
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
            "FROM_UNIXTIME() supports only signed integer, boolean, NULL, and integer "
            "descriptor arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = false, .integer = 1};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = false, .integer = 0};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
            return plan_row_scalar_from_unixtime_integer_value(
                database,
                expression,
                out_expression
            );
        }
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_from_unixtime_integer_value(database, expression, out_expression);
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
        return plan_row_scalar_from_unixtime_column(
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
        "FROM_UNIXTIME() supports only signed integer, boolean, NULL, and integer descriptor "
        "arguments"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_from_unixtime_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *literal = expression;
    bool is_negative = false;
    int64_t value = 0;
    int rc = MYLITE_OK;

    if (expression != NULL && expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "FROM_UNIXTIME() supports only signed integer, boolean, NULL, and integer "
                "descriptor arguments"
            );
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "FROM_UNIXTIME() supports only signed integer, boolean, NULL, and integer descriptor "
            "arguments"
        );
        return MYLITE_ERROR;
    }

    rc = from_unixtime_scalar_integer_literal_value(database, literal, is_negative, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    out_expression->value = (struct planned_value){.is_null = false, .integer = value};
    return MYLITE_OK;
}

static int plan_row_scalar_from_unixtime_column(
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
        "row-scalar SELECT FROM_UNIXTIME() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (strcmp(column.physical_type, "INTEGER") != 0) {
        set_unsupported_error(database, "FROM_UNIXTIME() supports only integer descriptor columns");
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_temporal_constructor_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    size_t argument_count = 0U;
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || !is_temporal_constructor_function_kind(expression->kind)) {
        set_unsupported_error(database, "temporal constructor function is not supported");
        return MYLITE_ERROR;
    }

    argument_count = temporal_constructor_function_argument_count(expression->kind);
    function_name = temporal_constructor_function_name(expression->kind);
    if (mylite_sql_ast_node_child_count(expression) != argument_count) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = temporal_constructor_planned_kind(expression->kind);
    out_expression->argument_count = argument_count;

    for (size_t index = 0U; rc == MYLITE_OK && index < argument_count; ++index) {
        rc = plan_row_scalar_temporal_constructor_argument(
            database,
            child_at(expression, index),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            function_name,
            &out_expression->arguments[index]
        );
    }
    return rc;
}

static int plan_row_scalar_temporal_constructor_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *function_name,
    struct planned_row_scalar_expression *out_expression
) {
    char unsupported_message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

    if (snprintf(
            unsupported_message,
            sizeof(unsupported_message),
            "%s() supports only signed integer, boolean, NULL, and integer descriptor arguments",
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
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = false, .integer = 1};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = false, .integer = 0};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
            return plan_row_scalar_integer_value(database, expression, out_expression);
        }
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
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
        return plan_row_scalar_temporal_constructor_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            function_name,
            out_expression
        );
    }

    set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static int plan_row_scalar_temporal_constructor_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *function_name,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT temporal constructors support only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (strcmp(column.physical_type, "INTEGER") != 0) {
        char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

        if (snprintf(
                message,
                sizeof(message),
                "%s() supports only integer descriptor columns",
                function_name == NULL ? "temporal constructor" : function_name
            ) < 0) {
            set_runtime_error(database, "failed to format temporal constructor diagnostic");
            return MYLITE_ERROR;
        }
        set_unsupported_error(database, message);
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static size_t temporal_constructor_function_argument_count(enum mylite_sql_ast_node_kind ast_kind) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_FROM_DAYS_FUNCTION:
        return 1U;
    case MYLITE_SQL_AST_MAKEDATE_FUNCTION:
        return 2U;
    case MYLITE_SQL_AST_MAKETIME_FUNCTION:
        return 3U;
    default:
        break;
    }
    return 0U;
}

static const char *temporal_constructor_function_name(enum mylite_sql_ast_node_kind ast_kind) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_FROM_DAYS_FUNCTION:
        return "FROM_DAYS";
    case MYLITE_SQL_AST_MAKEDATE_FUNCTION:
        return "MAKEDATE";
    case MYLITE_SQL_AST_MAKETIME_FUNCTION:
        return "MAKETIME";
    default:
        break;
    }
    return "temporal constructor";
}

static enum planned_row_scalar_expression_kind temporal_constructor_planned_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_FROM_DAYS_FUNCTION:
        return PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS;
    case MYLITE_SQL_AST_MAKEDATE_FUNCTION:
        return PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE;
    case MYLITE_SQL_AST_MAKETIME_FUNCTION:
        return PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME;
    default:
        break;
    }
    return PLANNED_ROW_SCALAR_EXPRESSION_NONE;
}

static bool is_temporal_constructor_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_FROM_DAYS_FUNCTION:
    case MYLITE_SQL_AST_MAKEDATE_FUNCTION:
    case MYLITE_SQL_AST_MAKETIME_FUNCTION:
        return true;
    default:
        break;
    }
    return false;
}

static int plan_row_scalar_period_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    function_name =
        period_function_name(expression == NULL ? MYLITE_SQL_AST_SCRIPT : expression->kind);
    if (expression == NULL ||
        (expression->kind != MYLITE_SQL_AST_PERIOD_ADD_FUNCTION &&
         expression->kind != MYLITE_SQL_AST_PERIOD_DIFF_FUNCTION) ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = period_planned_kind(expression->kind);
    out_expression->argument_count = 2U;

    for (size_t index = 0U; rc == MYLITE_OK && index < 2U; ++index) {
        rc = plan_row_scalar_temporal_constructor_argument(
            database,
            child_at(expression, index),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            function_name,
            &out_expression->arguments[index]
        );
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(out_expression);
    }
    return rc;
}

static const char *period_function_name(enum mylite_sql_ast_node_kind ast_kind) {
    if (ast_kind == MYLITE_SQL_AST_PERIOD_DIFF_FUNCTION) {
        return "PERIOD_DIFF";
    }
    return "PERIOD_ADD";
}

static enum planned_row_scalar_expression_kind period_planned_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    if (ast_kind == MYLITE_SQL_AST_PERIOD_DIFF_FUNCTION) {
        return PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF;
    }
    return PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD;
}

static int plan_row_scalar_convert_tz_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const char *temporal_message =
        "CONVERT_TZ() supports only string, DATE, DATETIME, TIMESTAMP, and NULL datetime values";
    const char *zone_message = "CONVERT_TZ() supports only string and NULL time-zone values";
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONVERT_TZ_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 3U) {
        set_native_function_parameter_count_error(database, "CONVERT_TZ");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(3U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ;
    out_expression->argument_count = 3U;

    rc = plan_row_scalar_convert_tz_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        temporal_message,
        true,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_convert_tz_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            zone_message,
            false,
            &out_expression->arguments[1]
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_convert_tz_argument(
            database,
            child_at(expression, 2U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            zone_message,
            false,
            &out_expression->arguments[2]
        );
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(out_expression);
    }
    return rc;
}

static int plan_row_scalar_convert_tz_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    bool temporal_argument,
    struct planned_row_scalar_expression *out_expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
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
        return plan_row_scalar_convert_tz_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            unsupported_message,
            temporal_argument,
            out_expression
        );
    }

    set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static int plan_row_scalar_convert_tz_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    bool temporal_argument,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT CONVERT_TZ() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_string_family(&column) ||
        (temporal_argument &&
         (column_descriptor_is_date(&column) || column_descriptor_is_datetime(&column) ||
          column_descriptor_is_timestamp(&column)))) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
        out_expression->column = column;
        return MYLITE_OK;
    }

    set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static int plan_row_scalar_weight_string_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *argument = NULL;
    struct session_scalar_cell cell = {0};
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || (expression->kind != MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION)) {
        set_unsupported_error(database, "WEIGHT_STRING() expression is unsupported");
        return MYLITE_ERROR;
    }
    argument_count = expression->kind == MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION ? 1U : 2U;
    if (mylite_sql_ast_node_child_count(expression) != argument_count) {
        set_unsupported_error(database, "WEIGHT_STRING() expression is unsupported");
        return MYLITE_ERROR;
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (has_source && argument != NULL &&
        (argument->kind == MYLITE_SQL_AST_IDENTIFIER ||
         argument->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        out_expression->arguments = (struct planned_row_scalar_expression *)
            calloc(argument_count, sizeof(*out_expression->arguments));
        if (out_expression->arguments == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        out_expression->kind = expression->kind == MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION
                                   ? PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING
                                   : PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY;
        out_expression->argument_count = argument_count;

        rc = plan_row_scalar_weight_string_argument(
            database,
            argument,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[0]
        );
        if (rc == MYLITE_OK && argument_count == 2U) {
            rc = plan_row_scalar_integer_value(
                database,
                child_at(expression, 1U),
                &out_expression->arguments[1]
            );
        }
        if (rc != MYLITE_OK) {
            planned_row_scalar_expression_deinit(out_expression);
        }
        return rc;
    }

    rc = weight_string_function_value(database, expression, &cell);
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        if (cell.value == NULL) {
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        } else {
            rc = copy_blob_value(database, cell.value, cell.value_size, &out_expression->value);
        }
    }
    session_scalar_cell_deinit(&cell);
    return rc;
}

static int plan_row_scalar_weight_string_argument(
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
            "WEIGHT_STRING() supports only scalar literals and descriptor columns"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (!has_source) {
            return set_unknown_column_for_reference(database, expression);
        }
        return plan_row_scalar_weight_string_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_literal_value(database, expression, out_expression);
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
    }

    set_unsupported_error(
        database,
        "WEIGHT_STRING() supports only scalar literals and descriptor columns"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_weight_string_column(
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
        "row-scalar SELECT WEIGHT_STRING() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!weight_string_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool weight_string_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }
    if (column_descriptor_is_string_family(column) ||
        column_descriptor_is_binary_string_family(column) ||
        column_descriptor_is_binary_blob_family(column)) {
        return true;
    }

    set_unsupported_error(
        database,
        "WEIGHT_STRING() supports only integer, string, binary string, and BLOB columns"
    );
    return false;
}

static bool planned_date_format_numeric_equal_format_is_supported(
    const struct planned_row_scalar_expression *expression
) {
    const struct planned_row_scalar_expression *format = NULL;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT ||
        expression->argument_count != 3U || expression->arguments == NULL) {
        return false;
    }

    format = &expression->arguments[1];
    if (format->kind != PLANNED_ROW_SCALAR_EXPRESSION_VALUE || format->value.is_null ||
        !format->value.is_text) {
        return false;
    }

    return date_format_numeric_equal_format_is_supported(
        format->value.text,
        format->value.text_length
    );
}

static int plan_row_scalar_datediff_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_datediff_input_kind left_kind = MYLITE_DATEDIFF_INPUT_STRING;
    enum mylite_datediff_input_kind right_kind = MYLITE_DATEDIFF_INPUT_STRING;
    const char *left_kind_name = NULL;
    const char *right_kind_name = NULL;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_DATEDIFF_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "DATEDIFF");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(4U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF;
    out_expression->argument_count = 4U;

    rc = plan_row_scalar_datediff_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0],
        &left_kind
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_datediff_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[2],
            &right_kind
        );
    }
    left_kind_name = mylite_datediff_input_kind_name(left_kind);
    right_kind_name = mylite_datediff_input_kind_name(right_kind);
    if (rc == MYLITE_OK && left_kind_name != NULL) {
        rc = copy_text_value(database, left_kind_name, &out_expression->arguments[1].value);
        out_expression->arguments[1].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    if (rc == MYLITE_OK && right_kind_name != NULL) {
        rc = copy_text_value(database, right_kind_name, &out_expression->arguments[3].value);
        out_expression->arguments[3].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    return rc;
}

static int plan_row_scalar_datediff_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_datediff_input_kind *out_input_kind
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_input_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input_kind = MYLITE_DATEDIFF_INPUT_STRING;
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "DATEDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
            "descriptor columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            return plan_row_scalar_datediff_string_literal(database, expression, out_expression);
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
        return plan_row_scalar_datediff_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression,
            out_input_kind
        );
    }

    set_unsupported_error(
        database,
        "DATEDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor "
        "columns, string descriptor columns, and NULL"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_datediff_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_datediff_input_kind *out_input_kind
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT DATEDIFF() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_date(&column)) {
        *out_input_kind = MYLITE_DATEDIFF_INPUT_DATE;
    } else if (column_descriptor_is_datetime(&column)) {
        *out_input_kind = MYLITE_DATEDIFF_INPUT_DATETIME;
    } else if (column_descriptor_is_timestamp(&column)) {
        *out_input_kind = MYLITE_DATEDIFF_INPUT_TIMESTAMP;
    } else if (column_descriptor_is_time(&column)) {
        set_unsupported_error(database, "DATEDIFF() does not yet support TIME values");
        return MYLITE_ERROR;
    } else if (column_descriptor_is_string_family(&column)) {
        *out_input_kind = MYLITE_DATEDIFF_INPUT_STRING;
    } else {
        set_unsupported_error(
            database,
            "DATEDIFF() supports only DATE, DATETIME, TIMESTAMP, string descriptor columns, "
            "string temporal literals, and NULL"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_datediff_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = decode_sql_string_literal(
        database,
        expression,
        "DATEDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
        "descriptor columns, string descriptor columns, and NULL",
        "DATEDIFF() literals do not support NUL bytes",
        &text,
        &text_length
    );

    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, "DATEDIFF() literals do not support NUL bytes");
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

static int plan_row_scalar_timediff_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_timediff_input_kind left_kind = MYLITE_TIMEDIFF_INPUT_STRING;
    enum mylite_timediff_input_kind right_kind = MYLITE_TIMEDIFF_INPUT_STRING;
    const struct mylite_sql_ast_node *left_argument = NULL;
    const char *left_kind_name = NULL;
    const char *right_kind_name = NULL;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_TIMEDIFF_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "TIMEDIFF");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(4U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF;
    out_expression->argument_count = 4U;

    left_argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (left_argument != NULL && left_argument->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(left_argument) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_expression->arguments[0].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[0].value = (struct planned_value){.is_null = true, .integer = 0};
        out_expression->arguments[2].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[2].value = (struct planned_value){.is_null = true, .integer = 0};
    } else {
        rc = plan_row_scalar_timediff_argument(
            database,
            child_at(expression, 0U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[0],
            &left_kind
        );
        if (rc == MYLITE_OK) {
            rc = plan_row_scalar_timediff_argument(
                database,
                child_at(expression, 1U),
                has_source,
                source_context,
                table_columns,
                table_column_count,
                &out_expression->arguments[2],
                &right_kind
            );
        }
    }
    left_kind_name = mylite_timediff_input_kind_name(left_kind);
    right_kind_name = mylite_timediff_input_kind_name(right_kind);
    if (rc == MYLITE_OK && left_kind_name != NULL) {
        rc = copy_text_value(database, left_kind_name, &out_expression->arguments[1].value);
        out_expression->arguments[1].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    if (rc == MYLITE_OK && right_kind_name != NULL) {
        rc = copy_text_value(database, right_kind_name, &out_expression->arguments[3].value);
        out_expression->arguments[3].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    return rc;
}

static int plan_row_scalar_timediff_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_timediff_input_kind *out_input_kind
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_input_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input_kind = MYLITE_TIMEDIFF_INPUT_STRING;
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "TIMEDIFF() supports only string temporal literals, DATE, TIME, DATETIME, TIMESTAMP "
            "descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            return plan_row_scalar_timediff_string_literal(database, expression, out_expression);
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
        return plan_row_scalar_timediff_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression,
            out_input_kind
        );
    }

    set_unsupported_error(
        database,
        "TIMEDIFF() supports only string temporal literals, DATE, TIME, DATETIME, TIMESTAMP "
        "descriptor columns, and NULL"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_timediff_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_timediff_input_kind *out_input_kind
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT TIMEDIFF() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_date(&column)) {
        *out_input_kind = MYLITE_TIMEDIFF_INPUT_DATE;
    } else if (column_descriptor_is_time(&column)) {
        *out_input_kind = MYLITE_TIMEDIFF_INPUT_TIME;
    } else if (column_descriptor_is_datetime(&column)) {
        *out_input_kind = MYLITE_TIMEDIFF_INPUT_DATETIME;
    } else if (column_descriptor_is_timestamp(&column)) {
        *out_input_kind = MYLITE_TIMEDIFF_INPUT_TIMESTAMP;
    } else if (column_descriptor_is_string_family(&column)) {
        set_unsupported_error(
            database,
            "TIMEDIFF() does not yet support string descriptor columns"
        );
        return MYLITE_ERROR;
    } else {
        set_unsupported_error(
            database,
            "TIMEDIFF() supports only DATE, TIME, DATETIME, TIMESTAMP descriptor columns, string "
            "temporal literals, and NULL"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_timediff_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = decode_sql_string_literal(
        database,
        expression,
        "TIMEDIFF() supports only string temporal literals, DATE, TIME, DATETIME, TIMESTAMP "
        "descriptor columns, and NULL",
        "TIMEDIFF() literals do not support NUL bytes",
        &text,
        &text_length
    );

    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, "TIMEDIFF() literals do not support NUL bytes");
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

static int plan_row_scalar_timestampdiff_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_timestampdiff_unit unit = MYLITE_TIMESTAMPDIFF_UNIT_DAY;
    enum mylite_timestampdiff_input_kind left_kind = MYLITE_TIMESTAMPDIFF_INPUT_STRING;
    enum mylite_timestampdiff_input_kind right_kind = MYLITE_TIMESTAMPDIFF_INPUT_STRING;
    const struct mylite_sql_ast_node *left_argument = NULL;
    const char *unit_name = NULL;
    const char *left_kind_name = NULL;
    const char *right_kind_name = NULL;
    int rc = MYLITE_OK;

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
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(planned_row_scalar_timestampdiff_argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF;
    out_expression->argument_count = planned_row_scalar_timestampdiff_argument_count;

    unit_name = mylite_timestampdiff_unit_name(unit);
    if (unit_name != NULL) {
        rc = copy_text_value(database, unit_name, &out_expression->arguments[0].value);
        out_expression->arguments[0].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    left_argument = unwrap_parenthesized_expression(child_at(expression, 1U));
    if (rc == MYLITE_OK && left_argument != NULL && left_argument->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(left_argument) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_expression->arguments[1].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[1].value = (struct planned_value){.is_null = true, .integer = 0};
        out_expression->arguments[3].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[3].value = (struct planned_value){.is_null = true, .integer = 0};
    } else if (rc == MYLITE_OK) {
        rc = plan_row_scalar_timestampdiff_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[1],
            &left_kind
        );
        if (rc == MYLITE_OK) {
            rc = plan_row_scalar_timestampdiff_argument(
                database,
                child_at(expression, 2U),
                has_source,
                source_context,
                table_columns,
                table_column_count,
                &out_expression->arguments[3],
                &right_kind
            );
        }
    }
    left_kind_name = mylite_timestampdiff_input_kind_name(left_kind);
    right_kind_name = mylite_timestampdiff_input_kind_name(right_kind);
    if (rc == MYLITE_OK && left_kind_name != NULL) {
        rc = copy_text_value(database, left_kind_name, &out_expression->arguments[2].value);
        out_expression->arguments[2].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    if (rc == MYLITE_OK && right_kind_name != NULL) {
        rc = copy_text_value(database, right_kind_name, &out_expression->arguments[4].value);
        out_expression->arguments[4].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    return rc;
}

static int plan_row_scalar_timestampdiff_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_timestampdiff_input_kind *out_input_kind
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_input_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input_kind = MYLITE_TIMESTAMPDIFF_INPUT_STRING;
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "TIMESTAMPDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
            "descriptor columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            return plan_row_scalar_timestampdiff_string_literal(
                database,
                expression,
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
        return plan_row_scalar_timestampdiff_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression,
            out_input_kind
        );
    }

    set_unsupported_error(
        database,
        "TIMESTAMPDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
        "descriptor columns, string descriptor columns, and NULL"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_timestampdiff_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_timestampdiff_input_kind *out_input_kind
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT TIMESTAMPDIFF() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_date(&column)) {
        *out_input_kind = MYLITE_TIMESTAMPDIFF_INPUT_DATE;
    } else if (column_descriptor_is_datetime(&column)) {
        *out_input_kind = MYLITE_TIMESTAMPDIFF_INPUT_DATETIME;
    } else if (column_descriptor_is_timestamp(&column)) {
        *out_input_kind = MYLITE_TIMESTAMPDIFF_INPUT_TIMESTAMP;
    } else if (column_descriptor_is_time(&column)) {
        set_unsupported_error(database, "TIMESTAMPDIFF() does not yet support TIME values");
        return MYLITE_ERROR;
    } else if (column_descriptor_is_string_family(&column)) {
        *out_input_kind = MYLITE_TIMESTAMPDIFF_INPUT_STRING;
    } else {
        set_unsupported_error(
            database,
            "TIMESTAMPDIFF() supports only DATE, DATETIME, TIMESTAMP, string descriptor "
            "columns, string temporal literals, and NULL"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_timestampdiff_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = decode_sql_string_literal(
        database,
        expression,
        "TIMESTAMPDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
        "descriptor columns, string descriptor columns, and NULL",
        "TIMESTAMPDIFF() literals do not support NUL bytes",
        &text,
        &text_length
    );

    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, "TIMESTAMPDIFF() literals do not support NUL bytes");
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

static int plan_row_scalar_unix_timestamp_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_unix_timestamp_input_kind input_kind = MYLITE_UNIX_TIMESTAMP_INPUT_STRING;
    struct session_scalar_cell cell = {0};
    const char *input_kind_name = NULL;
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION) {
        set_native_function_parameter_count_error(database, "UNIX_TIMESTAMP");
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count == 0U) {
        rc = session_scalar_value(database, expression, &cell);
        if (rc == MYLITE_OK) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            rc = copy_text_value(database, cell.value, &out_expression->value);
        }
        session_scalar_cell_deinit(&cell);
        return rc;
    }
    if (child_count != 1U) {
        set_native_function_parameter_count_error(database, "UNIX_TIMESTAMP");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP;
    out_expression->argument_count = 2U;

    rc = plan_row_scalar_unix_timestamp_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0],
        &input_kind
    );
    input_kind_name = mylite_unix_timestamp_input_kind_name(input_kind);
    if (rc == MYLITE_OK && input_kind_name != NULL) {
        rc = copy_text_value(database, input_kind_name, &out_expression->arguments[1].value);
        out_expression->arguments[1].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    return rc;
}

static int plan_row_scalar_unix_timestamp_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_unix_timestamp_input_kind *out_input_kind
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_input_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input_kind = MYLITE_UNIX_TIMESTAMP_INPUT_STRING;
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "UNIX_TIMESTAMP() supports only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            return plan_row_scalar_unix_timestamp_string_literal(
                database,
                expression,
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
        return plan_row_scalar_unix_timestamp_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression,
            out_input_kind
        );
    }

    set_unsupported_error(
        database,
        "UNIX_TIMESTAMP() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
        "descriptor columns, and NULL"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_unix_timestamp_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_unix_timestamp_input_kind *out_input_kind
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT UNIX_TIMESTAMP() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_date(&column)) {
        *out_input_kind = MYLITE_UNIX_TIMESTAMP_INPUT_DATE;
    } else if (column_descriptor_is_datetime(&column)) {
        *out_input_kind = MYLITE_UNIX_TIMESTAMP_INPUT_DATETIME;
    } else if (column_descriptor_is_timestamp(&column)) {
        *out_input_kind = MYLITE_UNIX_TIMESTAMP_INPUT_TIMESTAMP;
    } else if (column_descriptor_is_string_family(&column)) {
        set_unsupported_error(
            database,
            "UNIX_TIMESTAMP() does not yet support string descriptor columns"
        );
        return MYLITE_ERROR;
    } else {
        set_unsupported_error(
            database,
            "UNIX_TIMESTAMP() supports only DATE, DATETIME, TIMESTAMP descriptor columns, "
            "string temporal literals, and NULL"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_unix_timestamp_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = decode_sql_string_literal(
        database,
        expression,
        "UNIX_TIMESTAMP() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
        "descriptor columns, and NULL",
        "UNIX_TIMESTAMP() literals do not support NUL bytes",
        &text,
        &text_length
    );

    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, "UNIX_TIMESTAMP() literals do not support NUL bytes");
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

static int plan_row_scalar_timestamp_expression(
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
    const char *input_kind_name = NULL;
    size_t child_count = 0U;
    int rc = MYLITE_OK;

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

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(4U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP;
    out_expression->argument_count = 4U;

    rc = plan_row_scalar_timestamp_temporal_argument(
        database,
        child_at(expression, 0U),
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
    if (rc == MYLITE_OK && child_count == 2U) {
        rc = plan_row_scalar_timestamp_time_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[2]
        );
        out_expression->arguments[3].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[3].value = (struct planned_value){.is_null = false, .integer = 1};
    } else if (rc == MYLITE_OK) {
        out_expression->arguments[2].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[2].value = (struct planned_value){.is_null = true, .integer = 0};
        out_expression->arguments[3].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[3].value = (struct planned_value){.is_null = false, .integer = 0};
    }
    return rc;
}

static int plan_row_scalar_timestamp_temporal_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
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
        set_unsupported_error(
            database,
            "TIMESTAMP() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
            "descriptor columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_timestamp_temporal_literal(database, expression, out_expression);
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
        return plan_row_scalar_timestamp_temporal_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression,
            out_input_kind
        );
    }

    set_unsupported_error(
        database,
        "TIMESTAMP() supports only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor "
        "columns, string descriptor columns, and NULL"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_timestamp_temporal_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
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
        "row-scalar SELECT TIMESTAMP() supports only descriptor columns",
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
    } else if (column_descriptor_is_string_family(&column)) {
        *out_input_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING;
    } else if (column_descriptor_is_time(&column)) {
        set_unsupported_error(database, "TIMESTAMP() does not yet support TIME as first argument");
        return MYLITE_ERROR;
    } else {
        set_unsupported_error(
            database,
            "TIMESTAMP() supports only DATE, DATETIME, TIMESTAMP, string descriptor columns, "
            "string temporal literals, and NULL"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_timestamp_temporal_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
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
        "TIMESTAMP() supports only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor "
        "columns, string descriptor columns, and NULL",
        "TIMESTAMP() literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, "TIMESTAMP() literals do not support NUL bytes");
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

static int plan_row_scalar_timestamp_time_argument(
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
            "TIMESTAMP() time argument supports only string time literals, TIME descriptor "
            "columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_timestamp_time_literal(database, expression, out_expression);
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
        return plan_row_scalar_timestamp_time_column(
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
        "TIMESTAMP() time argument supports only string time literals, TIME descriptor columns, "
        "string descriptor columns, and NULL"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_timestamp_time_column(
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
        "row-scalar SELECT TIMESTAMP() time argument supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!column_descriptor_is_time(&column) && !column_descriptor_is_string_family(&column)) {
        set_unsupported_error(
            database,
            "TIMESTAMP() time argument supports only TIME and string descriptor columns"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_timestamp_time_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
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
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, "TIMESTAMP() time literals do not support NUL bytes");
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

static int plan_row_scalar_field_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    return plan_row_scalar_field_expression_with_context(
        database,
        expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT FIELD() supports only descriptor columns",
        out_expression
    );
}

static int plan_row_scalar_field_expression_with_context(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    const char *column_unsupported_message,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *arguments = child_at(expression, 0U);
    const struct mylite_sql_ast_node *argument = NULL;
    enum planned_row_scalar_field_domain domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_child_count(expression) != 1U || arguments == NULL ||
        arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        set_native_function_parameter_count_error(database, "FIELD");
        return MYLITE_ERROR;
    }

    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count < 2U) {
        set_native_function_parameter_count_error(database, "FIELD");
        return MYLITE_ERROR;
    }
    if (argument_count > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_FIELD;
    out_expression->argument_count = argument_count;

    argument = child_at(arguments, 0U);
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         ++argument_index) {
        rc = plan_row_scalar_field_argument(
            database,
            argument,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            column_unsupported_message,
            &out_expression->arguments[argument_index],
            &domain
        );
        argument = argument->next_sibling;
    }
    if (rc == MYLITE_OK && argument != NULL) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_expression->field_domain = domain;
    }
    return rc;
}

static int plan_row_scalar_field_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    const char *column_unsupported_message,
    struct planned_row_scalar_expression *out_expression,
    enum planned_row_scalar_field_domain *inout_domain
) {
    enum planned_row_scalar_field_domain domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "FIELD() supports only string, integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        rc = plan_row_scalar_literal_value(database, expression, out_expression);
    } else if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = plan_row_scalar_integer_value(database, expression, out_expression);
    } else if (
        expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER
    ) {
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
        rc = plan_row_scalar_field_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            column_unsupported_message,
            out_expression
        );
    } else {
        set_unsupported_error(
            database,
            "FIELD() supports only string, integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    domain = row_scalar_field_argument_domain(out_expression);
    if (out_expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_VALUE &&
        domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING &&
        !field_ascii_text_is_supported(
            out_expression->value.text,
            out_expression->value.text_length
        )) {
        set_unsupported_error(database, "FIELD() string literals support only ASCII values");
        return MYLITE_ERROR;
    }
    return merge_field_domain(database, domain, inout_domain);
}

static int plan_row_scalar_field_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    const char *column_unsupported_message,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    enum planned_row_scalar_field_domain domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        column_diagnostic_context,
        column_unsupported_message,
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    domain = row_scalar_field_column_domain(&column);
    if (domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE) {
        set_unsupported_error(
            database,
            "FIELD() supports only string, integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static enum planned_row_scalar_field_domain row_scalar_field_argument_domain(
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL) {
        return PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    }
    if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_VALUE) {
        if (expression->value.is_null) {
            return PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
        }
        if (expression->value.is_text) {
            return PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
        }
        return PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
    }
    if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        return row_scalar_field_column_domain(&expression->column);
    }
    return PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
}

static enum planned_row_scalar_field_domain row_scalar_field_column_domain(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
    }
    if (column_descriptor_is_string_family(column)) {
        return PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
    }
    return PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
}

static int plan_row_scalar_greatest_least_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const char *function_name = greatest_least_function_name(expression);
    const struct mylite_sql_ast_node *arguments = child_at(expression, 0U);
    const struct mylite_sql_ast_node *argument = NULL;
    enum planned_row_scalar_field_domain domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    if (function_name == NULL || mylite_sql_ast_node_child_count(expression) != 1U ||
        arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }

    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count < 2U) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }
    if (argument_count > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = expression->kind == MYLITE_SQL_AST_GREATEST_FUNCTION
                               ? PLANNED_ROW_SCALAR_EXPRESSION_GREATEST
                               : PLANNED_ROW_SCALAR_EXPRESSION_LEAST;
    out_expression->argument_count = argument_count;

    argument = child_at(arguments, 0U);
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         ++argument_index) {
        rc = plan_row_scalar_greatest_least_argument(
            database,
            argument,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_index],
            &domain
        );
        argument = argument->next_sibling;
    }
    if (rc == MYLITE_OK && argument != NULL) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_expression->field_domain = domain;
    }
    return rc;
}

static int plan_row_scalar_greatest_least_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum planned_row_scalar_field_domain *inout_domain
) {
    enum planned_row_scalar_field_domain domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "GREATEST() and LEAST() support only string, integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        rc = plan_row_scalar_greatest_least_literal_value(database, expression, out_expression);
    } else if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = plan_row_scalar_greatest_least_integer_value(database, expression, out_expression);
    } else if (
        expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER
    ) {
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
        rc = plan_row_scalar_greatest_least_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    } else {
        set_unsupported_error(
            database,
            "GREATEST() and LEAST() support only string, integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    domain = row_scalar_field_argument_domain(out_expression);
    if (out_expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_VALUE &&
        domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING &&
        !field_ascii_text_is_supported(
            out_expression->value.text,
            out_expression->value.text_length
        )) {
        set_unsupported_error(
            database,
            "GREATEST() and LEAST() string literals support only ASCII values"
        );
        return MYLITE_ERROR;
    }
    return merge_greatest_least_domain(database, domain, inout_domain);
}

static int plan_row_scalar_greatest_least_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    enum planned_row_scalar_field_domain domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT GREATEST() and LEAST() support only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    domain = row_scalar_field_column_domain(&column);
    if (domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE) {
        set_unsupported_error(
            database,
            "GREATEST() and LEAST() support only string, integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_greatest_least_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "GREATEST() and LEAST() support only string, integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_STRING:
        rc = decode_sql_string_literal(
            database,
            literal,
            "GREATEST() and LEAST() support only string literals",
            "GREATEST() and LEAST() string literals do not support NUL bytes",
            &text,
            &text_length
        );
        if (rc == MYLITE_OK) {
            rc = assign_text_value(database, text, text_length, &out_expression->value);
        }
        return rc;
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return plan_row_scalar_greatest_least_integer_value(database, literal, out_expression);
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
        "GREATEST() and LEAST() support only string, integer, boolean, and NULL arguments"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_greatest_least_integer_value(
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
                "GREATEST() and LEAST() support only signed integer literals"
            );
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "GREATEST() and LEAST() support only signed integer literals"
        );
        return MYLITE_ERROR;
    }

    rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
    if (rc != MYLITE_OK || (is_negative && magnitude > (uint64_t)INT64_MAX + 1U) ||
        (!is_negative && magnitude > (uint64_t)INT64_MAX)) {
        set_unsupported_error(
            database,
            "GREATEST() and LEAST() integer literals must fit the signed 64-bit range"
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

static int plan_row_scalar_interval_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *thresholds = NULL;
    const struct mylite_sql_ast_node *threshold = NULL;
    int64_t previous_threshold = 0;
    size_t threshold_count = 0U;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_INTERVAL_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_unsupported_error(database, "INTERVAL() supports only INTERVAL(search, value, ...)");
        return MYLITE_ERROR;
    }

    thresholds = child_at(expression, 1U);
    if (thresholds == NULL || thresholds->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        set_unsupported_error(database, "INTERVAL() supports only INTERVAL(search, value, ...)");
        return MYLITE_ERROR;
    }

    threshold_count = mylite_sql_ast_node_child_count(thresholds);
    if (threshold_count == 0U || threshold_count == SIZE_MAX ||
        threshold_count + 1U > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(threshold_count + 1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL;
    out_expression->argument_count = threshold_count + 1U;

    rc = plan_row_scalar_interval_search_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );

    threshold = child_at(thresholds, 0U);
    for (size_t threshold_index = 0U; rc == MYLITE_OK && threshold_index < threshold_count;
         ++threshold_index) {
        struct planned_row_scalar_expression *planned_threshold =
            &out_expression->arguments[threshold_index + 1U];

        if (threshold == NULL) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
            break;
        }
        rc = plan_row_scalar_interval_threshold_argument(database, threshold, planned_threshold);
        if (rc == MYLITE_OK && planned_threshold->value.is_null) {
            set_unsupported_error(database, "INTERVAL() threshold arguments cannot be NULL");
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK && threshold_index != 0U &&
            planned_threshold->value.integer < previous_threshold) {
            set_unsupported_error(
                database,
                "INTERVAL() threshold arguments must be sorted ascending"
            );
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            previous_threshold = planned_threshold->value.integer;
        }
        threshold = threshold->next_sibling;
    }
    if (rc == MYLITE_OK && threshold != NULL) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int plan_row_scalar_interval_search_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "INTERVAL() supports only integer, boolean, and NULL search arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_interval_integer_value(
            database,
            expression,
            out_expression,
            "INTERVAL() supports only signed integer search arguments"
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
        return plan_row_scalar_interval_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "INTERVAL() supports only integer, boolean, and NULL search arguments"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return plan_row_scalar_interval_integer_value(
            database,
            expression,
            out_expression,
            "INTERVAL() supports only signed integer search arguments"
        );
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
        "INTERVAL() supports only integer, boolean, and NULL search arguments"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_interval_threshold_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "INTERVAL() supports only integer and boolean threshold arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_interval_integer_value(
            database,
            expression,
            out_expression,
            "INTERVAL() supports only signed integer threshold arguments"
        );
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        set_unsupported_error(database, "INTERVAL() supports only literal threshold arguments");
        return MYLITE_ERROR;
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "INTERVAL() supports only integer and boolean threshold arguments"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return plan_row_scalar_interval_integer_value(
            database,
            expression,
            out_expression,
            "INTERVAL() supports only signed integer threshold arguments"
        );
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
        "INTERVAL() supports only integer and boolean threshold arguments"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_interval_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    enum planned_row_scalar_field_domain domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT INTERVAL() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    domain = row_scalar_field_column_domain(&column);
    if (domain != PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER) {
        set_unsupported_error(
            database,
            "INTERVAL() supports only integer, boolean, and NULL search arguments"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_interval_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression,
    const char *unsupported_message
) {
    const struct mylite_sql_ast_node *literal = expression;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int64_t value = 0;

    if (expression != NULL && expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, unsupported_message);
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        (is_negative && magnitude > (uint64_t)INT64_MAX + 1U) ||
        (!is_negative && magnitude > (uint64_t)INT64_MAX)) {
        set_unsupported_error(
            database,
            "INTERVAL() integer literals must fit the signed 64-bit range"
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

static void planned_row_scalar_expression_deinit(struct planned_row_scalar_expression *expression) {
    if (expression == NULL) {
        return;
    }

    while (expression->argument_count != 0U) {
        planned_row_scalar_expression_deinit_next_leaf(expression);
    }
    free(expression->arguments);
    planned_value_deinit(&expression->value);
    *expression = (struct planned_row_scalar_expression){0};
}

static void planned_row_scalar_expression_deinit_next_leaf(
    struct planned_row_scalar_expression *expression
) {
    struct planned_row_scalar_expression *parent = NULL;
    struct planned_row_scalar_expression *current = expression;

    while (current->argument_count != 0U) {
        parent = current;
        current = &current->arguments[current->argument_count - 1U];
    }

    free(current->arguments);
    planned_value_deinit(&current->value);
    *current = (struct planned_row_scalar_expression){0};
    if (parent != NULL) {
        --parent->argument_count;
    }
}

static bool row_scalar_column_descriptor_is_supported(
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
            "row-scalar SELECT CONCAT() does not support approximate numeric columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "row-scalar SELECT supports only integer, DECIMAL, string, binary string, BIT, YEAR, and "
        "temporal columns"
    );
    return false;
}

static bool row_scalar_expression_contains_row_function(
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
        if (current->kind == MYLITE_SQL_AST_CONCAT_FUNCTION ||
            current->kind == MYLITE_SQL_AST_CONCAT_WS_FUNCTION ||
            current->kind == MYLITE_SQL_AST_CONCAT_WS_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_FIELD_FUNCTION ||
            current->kind == MYLITE_SQL_AST_GREATEST_FUNCTION ||
            current->kind == MYLITE_SQL_AST_GREATEST_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_LEAST_FUNCTION ||
            current->kind == MYLITE_SQL_AST_LEAST_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_INTERVAL_FUNCTION ||
            current->kind == MYLITE_SQL_AST_DATE_FORMAT_FUNCTION ||
            current->kind == MYLITE_SQL_AST_GET_FORMAT_FUNCTION ||
            current->kind == MYLITE_SQL_AST_TIME_FORMAT_FUNCTION ||
            current->kind == MYLITE_SQL_AST_STR_TO_DATE_FUNCTION ||
            current->kind == MYLITE_SQL_AST_DATEDIFF_FUNCTION ||
            current->kind == MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_TIMEDIFF_FUNCTION ||
            current->kind == MYLITE_SQL_AST_TIMEDIFF_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_TIMESTAMPDIFF_FUNCTION ||
            current->kind == MYLITE_SQL_AST_TIMESTAMP_FUNCTION ||
            current->kind == MYLITE_SQL_AST_TIMESTAMP_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION ||
            current->kind == MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION ||
            current->kind == MYLITE_SQL_AST_SEC_TO_TIME_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION ||
            current->kind == MYLITE_SQL_AST_FROM_UNIXTIME_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_FROM_DAYS_FUNCTION ||
            current->kind == MYLITE_SQL_AST_FROM_DAYS_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_MAKEDATE_FUNCTION ||
            current->kind == MYLITE_SQL_AST_MAKEDATE_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_MAKETIME_FUNCTION ||
            current->kind == MYLITE_SQL_AST_MAKETIME_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_TIME_TO_SEC_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_TO_DAYS_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_TO_SECONDS_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_DAYNAME_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_DAYOFWEEK_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_DAYOFYEAR_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_LAST_DAY_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_MONTHNAME_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_EXTRACT_FUNCTION ||
            is_temporal_extract_function_kind(current->kind) ||
            is_string_length_function_kind(current->kind) ||
            is_string_codepoint_function_kind(current->kind) ||
            is_string_case_function_kind(current->kind) ||
            is_string_trim_function_kind(current->kind) ||
            is_string_slice_function_kind(current->kind) ||
            is_string_padding_function_kind(current->kind) ||
            is_string_bitmask_function_kind(current->kind) ||
            is_string_search_function_kind(current->kind) ||
            current->kind == MYLITE_SQL_AST_EXPORT_SET_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_MAKE_SET_ARGUMENT_COUNT_ERROR ||
            row_scalar_expression_is_window_function(current) ||
            current->kind == MYLITE_SQL_AST_REPLACE_FUNCTION ||
            current->kind == MYLITE_SQL_AST_INSERT_STRING_FUNCTION ||
            current->kind == MYLITE_SQL_AST_REVERSE_FUNCTION ||
            current->kind == MYLITE_SQL_AST_SOUNDEX_FUNCTION ||
            current->kind == MYLITE_SQL_AST_SOUNDEX_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_QUOTE_FUNCTION ||
            current->kind == MYLITE_SQL_AST_QUOTE_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION ||
            current->kind == MYLITE_SQL_AST_FIND_IN_SET_FUNCTION ||
            current->kind == MYLITE_SQL_AST_STRCMP_FUNCTION ||
            current->kind == MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION ||
            current->kind == MYLITE_SQL_AST_REGEXP_LIKE_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_REGEXP_INSTR_FUNCTION ||
            current->kind == MYLITE_SQL_AST_REGEXP_INSTR_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_REGEXP_SUBSTR_FUNCTION ||
            current->kind == MYLITE_SQL_AST_REGEXP_SUBSTR_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_REGEXP_REPLACE_FUNCTION ||
            current->kind == MYLITE_SQL_AST_REGEXP_REPLACE_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_JSON_VALID_FUNCTION ||
            current->kind == MYLITE_SQL_AST_JSON_VALID_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_JSON_CONTAINS_FUNCTION ||
            current->kind == MYLITE_SQL_AST_JSON_CONTAINS_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_JSON_CONTAINS_PATH_FUNCTION ||
            current->kind == MYLITE_SQL_AST_JSON_CONTAINS_PATH_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION ||
            current->kind == MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_JSON_VALUE_FUNCTION ||
            current->kind == MYLITE_SQL_AST_JSON_LENGTH_FUNCTION ||
            current->kind == MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_JSON_KEYS_FUNCTION ||
            current->kind == MYLITE_SQL_AST_JSON_KEYS_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_JSON_TYPE_FUNCTION ||
            current->kind == MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_JSON_SET_FUNCTION ||
            current->kind == MYLITE_SQL_AST_JSON_SET_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_JSON_INSERT_FUNCTION ||
            current->kind == MYLITE_SQL_AST_JSON_INSERT_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_JSON_REPLACE_FUNCTION ||
            current->kind == MYLITE_SQL_AST_JSON_REPLACE_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_JSON_REMOVE_FUNCTION ||
            current->kind == MYLITE_SQL_AST_JSON_REMOVE_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_JSON_QUOTE_FUNCTION ||
            current->kind == MYLITE_SQL_AST_JSON_QUOTE_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION ||
            current->kind == MYLITE_SQL_AST_JSON_UNQUOTE_ARGUMENT_COUNT_ERROR ||
            current->kind == MYLITE_SQL_AST_JSON_ARRAY_FUNCTION ||
            current->kind == MYLITE_SQL_AST_JSON_OBJECT_FUNCTION ||
            (current->kind == MYLITE_SQL_AST_BINARY_EXPRESSION &&
             (mylite_sql_ast_node_operator(current) == MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT ||
              mylite_sql_ast_node_operator(current) ==
                  MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT)) ||
            current->kind == MYLITE_SQL_AST_HEX_FUNCTION ||
            current->kind == MYLITE_SQL_AST_TO_BASE64_FUNCTION ||
            current->kind == MYLITE_SQL_AST_FROM_BASE64_FUNCTION ||
            current->kind == MYLITE_SQL_AST_UNHEX_FUNCTION ||
            current->kind == MYLITE_SQL_AST_UUID_FUNCTION ||
            current->kind == MYLITE_SQL_AST_IS_UUID_FUNCTION ||
            current->kind == MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION ||
            current->kind == MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION ||
            current->kind == MYLITE_SQL_AST_CHAR_FUNCTION ||
            current->kind == MYLITE_SQL_AST_DEFAULT_FUNCTION ||
            is_charset_collation_function_kind(current->kind)) {
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

static bool row_scalar_expression_contains_conversion_expression(
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
        if (row_scalar_conversion_kind_from_expression(current) !=
            PLANNED_ROW_SCALAR_CONVERSION_NONE) {
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

static bool row_scalar_expression_contains_rand_function(
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
        if (current->kind == MYLITE_SQL_AST_RAND_FUNCTION ||
            current->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION ||
            current->kind == MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR) {
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

static bool row_scalar_expression_contains_any_value_function(
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
        if (current->kind == MYLITE_SQL_AST_ANY_VALUE_FUNCTION ||
            current->kind == MYLITE_SQL_AST_ANY_VALUE_ARGUMENT_COUNT_ERROR) {
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

static bool row_scalar_expression_contains_integer_arithmetic_attempt(
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
        if (current->kind == MYLITE_SQL_AST_BINARY_EXPRESSION &&
            integer_arithmetic_operator_is_attempt(mylite_sql_ast_node_operator(current))) {
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

static bool integer_arithmetic_operator_is_attempt(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        return true;
    default:
        break;
    }
    return false;
}

static bool integer_arithmetic_operator_is_supported(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        return true;
    default:
        break;
    }
    return false;
}

static bool row_scalar_expression_contains_date_interval_second_function(
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
        if (is_date_interval_second_function_kind(current->kind)) {
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

static bool row_scalar_expression_contains_control_flow_function(
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
        if (is_row_scalar_control_flow_expression(current)) {
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

static bool row_scalar_expression_contains_statement_time_function(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    return (expression->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE ||
            expression->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR ||
            expression->kind == MYLITE_SQL_AST_SYSDATE_FUNCTION ||
            expression->kind == MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR ||
            expression->kind == MYLITE_SQL_AST_CURRENT_DATE_VALUE ||
            expression->kind == MYLITE_SQL_AST_CURRENT_TIME_VALUE ||
            expression->kind == MYLITE_SQL_AST_UTC_DATE_VALUE ||
            expression->kind == MYLITE_SQL_AST_UTC_TIME_VALUE ||
            expression->kind == MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE) != 0;
}

static int plan_select_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
) {
    const struct mylite_sql_ast_node *item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (select_list_is_wildcard(select_list)) {
        return plan_select_wildcard_columns(
            database,
            source_context,
            table_columns,
            table_column_count,
            out_plan
        );
    }

    item = child_at(select_list, 0U);
    while (item != NULL) {
        const struct mylite_sql_ast_node *column_node = NULL;
        const struct mylite_sql_ast_node *qualified_wildcard = NULL;
        struct mylite_catalog_column_descriptor column = {0};
        size_t source_index = 0U;
        int rc = MYLITE_OK;

        if (select_item_qualified_wildcard(item, &qualified_wildcard)) {
            rc = plan_select_qualified_wildcard_columns(
                database,
                qualified_wildcard,
                source_context,
                table_columns,
                table_column_count,
                out_plan
            );
            if (rc != MYLITE_OK) {
                return rc;
            }
            item = item->next_sibling;
            continue;
        }
        rc = select_item_column_reference(item, &column_node);
        if (rc != MYLITE_OK) {
            set_unsupported_error(database, "SELECT supports only descriptor table columns");
            return MYLITE_ERROR;
        }
        rc = resolve_descriptor_column_reference_with_source_index(
            database,
            column_node,
            source_context,
            COLUMN_REFERENCE_FIELD,
            "SELECT supports only descriptor table columns",
            table_columns,
            table_column_count,
            &column,
            &source_index
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = append_select_column_from_source(
            database,
            out_plan,
            &column,
            source_index,
            child_at(item, 1U)
        );
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
            return rc;
        }
        item = item->next_sibling;
    }

    return MYLITE_OK;
}

static int plan_select_qualified_wildcard_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *qualified_wildcard,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const struct mylite_catalog_column_descriptor *columns = table_columns;
    size_t column_count = table_column_count;
    size_t source_index = 0U;
    size_t part_count = 0U;
    bool matched = false;
    int rc = collect_qualified_wildcard_parts(database, qualified_wildcard, parts, &part_count);

    if (rc != MYLITE_OK) {
        return rc;
    }

    if (select_source_context_is_joined(source_context)) {
        for (size_t index = 0U; index < source_context->source_count; ++index) {
            const struct planned_select_source *source = &source_context->sources[index];

            if (!joined_source_matches_qualified_wildcard_parts(source, parts, part_count)) {
                continue;
            }
            columns = source->columns;
            column_count = source->column_count;
            source_index = index;
            matched = true;
            break;
        }
    } else {
        matched = select_source_matches_qualified_wildcard_parts(
            source_context,
            (const char (*)[MYLITE_CATALOG_IDENTIFIER_CAPACITY])parts,
            part_count
        );
    }

    if (!matched) {
        return set_unknown_qualified_wildcard_table_error(database, parts, part_count);
    }

    return append_visible_select_columns_from_source(
        database,
        out_plan,
        source_index,
        columns,
        column_count
    );
}

static int collect_qualified_wildcard_parts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *qualified_wildcard,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t *out_part_count
) {
    const struct mylite_sql_ast_node *qualifier = NULL;
    int rc = MYLITE_OK;

    *out_part_count = 0U;
    if (qualified_wildcard == NULL ||
        qualified_wildcard->kind != MYLITE_SQL_AST_QUALIFIED_WILDCARD) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    qualifier = child_at(qualified_wildcard, 0U);
    rc = collect_identifier_parts(
        qualifier,
        parts,
        table_name_part_capacity,
        out_part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (*out_part_count == 0U || *out_part_count > 2U) {
        set_unsupported_error(
            database,
            "SELECT supports only one- or two-part qualified wildcards"
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int set_unknown_qualified_wildcard_table_error(
    struct mylite_db *database,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count
) {
    char table_name[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int rc =
        format_column_reference_name(database, parts, part_count, table_name, sizeof(table_name));

    if (rc != MYLITE_OK) {
        return rc;
    }
    set_unknown_table_name_error(database, table_name);
    return MYLITE_ERROR;
}

static bool select_source_matches_qualified_wildcard_parts(
    const struct select_source_context *source_context,
    const char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count
) {
    if (source_context == NULL || source_context->source == NULL) {
        return false;
    }
    if (part_count == 1U) {
        const char *expected = source_context->source->table_name;

        if (source_context->has_alias) {
            expected = source_context->alias;
        }

        return text_equals_ascii_case_insensitive(parts[0], expected);
    }
    if (part_count == 2U && !source_context->has_alias) {
        bool schema_matches =
            text_equals_ascii_case_insensitive(parts[0], source_context->source->schema.name);
        bool table_matches =
            text_equals_ascii_case_insensitive(parts[1], source_context->source->table_name);

        if (!schema_matches) {
            return false;
        }
        return table_matches;
    }

    return false;
}

static bool joined_source_matches_qualified_wildcard_parts(
    const struct planned_select_source *source,
    const char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count
) {
    if (source == NULL) {
        return false;
    }
    if (part_count == 1U) {
        const char *expected = source->source.table_name;

        if (source->has_alias) {
            expected = source->alias;
        }

        return text_equals_ascii_case_insensitive(parts[0], expected);
    }
    if (part_count == 2U && !source->has_alias) {
        bool schema_matches =
            text_equals_ascii_case_insensitive(parts[0], source->source.schema.name);
        bool table_matches =
            text_equals_ascii_case_insensitive(parts[1], source->source.table_name);

        if (!schema_matches) {
            return false;
        }
        return table_matches;
    }

    return false;
}

static int append_visible_select_columns_from_source(
    struct mylite_db *database,
    struct planned_select *out_plan,
    size_t source_index,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
) {
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        int rc = MYLITE_OK;

        if (!columns[column_index].is_visible) {
            continue;
        }
        rc = append_select_column_from_source(
            database,
            out_plan,
            &columns[column_index],
            source_index,
            NULL
        );
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
            return rc;
        }
    }

    return MYLITE_OK;
}

static bool select_source_context_is_joined(const struct select_source_context *source_context) {
    if (source_context == NULL) {
        return false;
    }
    return source_context->source_count > 0U;
}

static int plan_select_wildcard_columns(
    struct mylite_db *database,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
) {
    if (select_source_context_is_joined(source_context)) {
        return plan_joined_select_wildcard_columns(database, source_context, out_plan);
    }
    return plan_single_source_select_wildcard_columns(
        database,
        table_columns,
        table_column_count,
        out_plan
    );
}

static int plan_joined_select_wildcard_columns(
    struct mylite_db *database,
    const struct select_source_context *source_context,
    struct planned_select *out_plan
) {
    for (size_t source_index = 0U; source_index < source_context->source_count; ++source_index) {
        const struct planned_select_source *source = &source_context->sources[source_index];
        int rc = append_visible_select_columns_from_source(
            database,
            out_plan,
            source_index,
            source->columns,
            source->column_count
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int plan_single_source_select_wildcard_columns(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
) {
    return append_visible_select_columns_from_source(
        database,
        out_plan,
        0U,
        table_columns,
        table_column_count
    );
}

static bool select_list_is_wildcard(const struct mylite_sql_ast_node *select_list) {
    const struct mylite_sql_ast_node *item = child_at(select_list, 0U);
    const struct mylite_sql_ast_node *expression = child_at(item, 0U);

    if (mylite_sql_ast_node_child_count(select_list) == 1U && expression != NULL &&
        expression->kind == MYLITE_SQL_AST_WILDCARD) {
        return true;
    }

    return false;
}

static int append_select_column_from_source(
    struct mylite_db *database,
    struct planned_select *plan,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    const struct mylite_sql_ast_node *alias
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    const struct mylite_sql_ast_node **aliases = NULL;
    size_t *source_indexes = NULL;
    size_t required_count = plan->column_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (required_count > SIZE_MAX / sizeof(*aliases)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (required_count > SIZE_MAX / sizeof(*source_indexes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    columns = realloc(plan->columns, required_count * sizeof(*columns));
    if (columns == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    plan->columns = columns;

    aliases = (const struct mylite_sql_ast_node **)
        realloc((void *)plan->column_aliases, required_count * sizeof(*aliases));
    if (aliases == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    plan->column_aliases = aliases;

    source_indexes = realloc(plan->column_source_indexes, required_count * sizeof(*source_indexes));
    if (source_indexes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    plan->column_source_indexes = source_indexes;

    plan->columns[plan->column_count] = *column;
    plan->column_aliases[plan->column_count] = alias;
    plan->column_source_indexes[plan->column_count] = source_index;
    plan->column_count = required_count;

    return MYLITE_OK;
}

static int select_item_column_reference(
    const struct mylite_sql_ast_node *item,
    const struct mylite_sql_ast_node **out_column
) {
    const struct mylite_sql_ast_node *expression = child_at(item, 0U);

    *out_column = NULL;
    if (item == NULL || item->kind != MYLITE_SQL_AST_SELECT_ITEM || expression == NULL ||
        (expression->kind != MYLITE_SQL_AST_IDENTIFIER &&
         expression->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        return MYLITE_ERROR;
    }

    *out_column = expression;

    return MYLITE_OK;
}

static bool select_item_qualified_wildcard(
    const struct mylite_sql_ast_node *item,
    const struct mylite_sql_ast_node **out_qualified_wildcard
) {
    const struct mylite_sql_ast_node *expression = child_at(item, 0U);

    *out_qualified_wildcard = NULL;
    if (item == NULL || item->kind != MYLITE_SQL_AST_SELECT_ITEM || expression == NULL ||
        expression->kind != MYLITE_SQL_AST_QUALIFIED_WILDCARD || child_at(item, 1U) != NULL) {
        return false;
    }

    *out_qualified_wildcard = expression;
    return true;
}

static int append_select_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_select *plan,
    const struct result_column_metadata_context *metadata_context,
    size_t column_index
) {
    const char *column_name = plan->columns[column_index].name;
    const struct mylite_sql_ast_node *alias = plan->column_aliases[column_index];
    struct mylite_result_column_descriptor descriptor = {0};
    char *alias_text = NULL;
    int rc = MYLITE_OK;

    if (alias != NULL) {
        rc = copy_select_item_alias_text(database, alias, &alias_text);
        if (rc != MYLITE_OK) {
            return rc;
        }
        column_name = alias_text;
    }

    rc = make_select_result_column_descriptor(
        database,
        plan,
        metadata_context,
        column_index,
        column_name,
        &descriptor
    );
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column_descriptor(result, &descriptor);
    }
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }
    free(alias_text);

    return rc;
}

static struct result_column_metadata_context result_column_metadata_context_init(void) {
    return (struct result_column_metadata_context){
        .primary_key = primary_key_info_init(),
        .indexes = NULL,
        .index_count = 0U,
        .has_single_source_metadata = false,
    };
}

static void result_column_metadata_context_deinit(struct result_column_metadata_context *context) {
    if (context == NULL) {
        return;
    }

    primary_key_info_deinit(&context->primary_key);
    loaded_index_infos_deinit(&context->indexes, &context->index_count);
    *context = result_column_metadata_context_init();
}

static int load_result_column_metadata_context(
    struct mylite_db *database,
    const struct planned_select *plan,
    struct result_column_metadata_context *context
) {
    if (context == NULL || plan == NULL) {
        return MYLITE_MISUSE;
    }
    if (plan->source_count != 0U) {
        return MYLITE_OK;
    }

    return load_result_column_metadata_context_for_table(database, plan->table.table_id, context);
}

static int load_result_column_metadata_context_for_table(
    struct mylite_db *database,
    int64_t table_id,
    struct result_column_metadata_context *context
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    int rc = MYLITE_OK;

    if (context == NULL) {
        return MYLITE_MISUSE;
    }

    rc = load_table_columns(database, table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc =
            load_primary_key_info(database, table_id, columns, column_count, &context->primary_key);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            table_id,
            columns,
            column_count,
            &context->indexes,
            &context->index_count
        );
    }
    free(columns);
    if (rc != MYLITE_OK) {
        result_column_metadata_context_deinit(context);
        return rc;
    }

    context->has_single_source_metadata = true;
    return MYLITE_OK;
}

static struct mylite_result_column_descriptor unknown_result_column_descriptor(const char *label) {
    return (struct mylite_result_column_descriptor){
        .label = label,
        .schema_name = "",
        .table_name = "",
        .origin_schema_name = "",
        .origin_table_name = "",
        .origin_column_name = "",
        .logical_type = MYLITE_RESULT_LOGICAL_TYPE_UNKNOWN,
        .flags = 0U,
        .charset_id = 0U,
        .collation_id = 0U,
        .display_length = 0U,
        .decimals = 0U,
        .nullable = true,
    };
}

static int make_select_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_select *plan,
    const struct result_column_metadata_context *metadata_context,
    size_t column_index,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
) {
    const struct mylite_catalog_column_descriptor *column = &plan->columns[column_index];
    const char *table_name = plan->source.table_name;

    if (plan->source_has_alias) {
        table_name = plan->source_alias;
    }

    *out_descriptor = (struct mylite_result_column_descriptor){
        .label = label,
        .schema_name = "",
        .table_name = "",
        .origin_schema_name = "",
        .origin_table_name = "",
        .origin_column_name = "",
        .logical_type = MYLITE_RESULT_LOGICAL_TYPE_UNKNOWN,
        .flags = 0U,
        .charset_id = 0U,
        .collation_id = 0U,
        .display_length = 0U,
        .decimals = 0U,
        .nullable = true,
    };

    if (metadata_context == NULL || !metadata_context->has_single_source_metadata) {
        return MYLITE_OK;
    }

    out_descriptor->schema_name = plan->source.schema.name;
    out_descriptor->table_name = table_name;
    out_descriptor->origin_schema_name = plan->source.schema.name;
    out_descriptor->origin_table_name = plan->table.name;
    out_descriptor->origin_column_name = column->name;
    out_descriptor->nullable = column->is_nullable;

    return populate_select_result_column_descriptor(
        database,
        &plan->table,
        column,
        metadata_context,
        out_descriptor
    );
}

static int populate_select_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    const struct result_column_metadata_context *metadata_context,
    struct mylite_result_column_descriptor *descriptor
) {
    bool matched = false;
    int rc = populate_numeric_result_column_descriptor(database, column, descriptor, &matched);

    if (rc == MYLITE_OK && !matched) {
        rc =
            populate_string_result_column_descriptor(database, table, column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_enum_result_column_descriptor(database, table, column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_set_result_column_descriptor(database, table, column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_json_result_column_descriptor(column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_spatial_result_column_descriptor(column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_binary_result_column_descriptor(database, column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_temporal_result_column_descriptor(column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_bit_result_column_descriptor(column, descriptor, &matched);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!matched) {
        set_unsupported_error(
            database,
            "result metadata supports only current baseline column descriptors"
        );
        return MYLITE_ERROR;
    }

    apply_result_column_descriptor_flags(column, metadata_context, descriptor);
    return MYLITE_OK;
}

static int populate_numeric_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
) {
    struct decimal_type_info decimal_info = {0};
    struct approximate_type_info approximate_info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    uint64_t display_length = 0U;
    bool is_unsigned = false;

    *out_matched = false;
    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    if (result_metadata_integer_descriptor(
            column->logical_type,
            &descriptor->logical_type,
            &display_length,
            &is_unsigned
        )) {
        descriptor->display_length = display_length;
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_NUM;
        if (is_unsigned) {
            descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_UNSIGNED;
        }
        *out_matched = true;
        return MYLITE_OK;
    }
    if (column_descriptor_is_year(column)) {
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_YEAR;
        descriptor->display_length = year_text_length;
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_UNSIGNED |
                             MYLITE_RESULT_COLUMN_FLAG_ZEROFILL | MYLITE_RESULT_COLUMN_FLAG_NUM;
        *out_matched = true;
        return MYLITE_OK;
    }
    if (column_descriptor_is_decimal(column)) {
        int rc = decimal_type_info_for_logical_type(column->logical_type, &decimal_info);

        if (rc != MYLITE_OK) {
            set_unsupported_error(database, "result metadata supports only baseline DECIMAL");
            return MYLITE_ERROR;
        }
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_NEWDECIMAL;
        descriptor->display_length = decimal_info.precision;
        if (decimal_info.scale != 0U) {
            ++descriptor->display_length;
        }
        if (!decimal_info.is_unsigned) {
            ++descriptor->display_length;
        }
        descriptor->decimals = (uint16_t)decimal_info.scale;
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_NUM;
        if (decimal_info.is_unsigned) {
            descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_UNSIGNED;
        }
        *out_matched = true;
        return MYLITE_OK;
    }
    if (column_descriptor_is_approximate(column)) {
        int rc = approximate_type_info_for_logical_type(column->logical_type, &approximate_info);

        if (rc != MYLITE_OK) {
            set_unsupported_error(
                database,
                "result metadata supports only baseline approximate numeric descriptors"
            );
            return MYLITE_ERROR;
        }
        if (approximate_info.type_class == APPROXIMATE_TYPE_FLOAT) {
            descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_FLOAT;
            descriptor->display_length = mysql_float_display_length;
        } else {
            descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_DOUBLE;
            descriptor->display_length = mysql_double_display_length;
        }
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_NUM;
        if (approximate_info.is_unsigned) {
            descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_UNSIGNED;
        }
        *out_matched = true;
        return MYLITE_OK;
    }

    return MYLITE_OK;
}

static int populate_string_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
) {
    const struct text_family_type_info *text_info = NULL;
    size_t length = 0U;
    bool is_national = column_descriptor_is_national_char_or_varchar(column);
    int rc = MYLITE_OK;

    *out_matched = false;
    if (!column_descriptor_is_string_family(column)) {
        return MYLITE_OK;
    }

    if (is_national) {
        descriptor->charset_id = mysql_collation_utf8mb3_general_ci_id;
        descriptor->collation_id = mysql_collation_utf8mb3_general_ci_id;
    } else {
        descriptor->charset_id =
            result_metadata_collation_id(column_effective_collation_name(table, column));
        descriptor->collation_id = descriptor->charset_id;
    }
    if (!is_national && column_effective_collation_is_binary(table, column)) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BINARY;
    }
    if (column_descriptor_is_char(column)) {
        rc = parse_char_descriptor_length(
            database,
            column->logical_type,
            "result metadata supports only baseline CHAR",
            &length
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_STRING;
        descriptor->display_length =
            (uint64_t)length * column_effective_max_bytes_per_character(table, column);
        *out_matched = true;
        return MYLITE_OK;
    }
    if (column_descriptor_is_varchar(column)) {
        rc = parse_varchar_descriptor_length(
            database,
            column->logical_type,
            "result metadata supports only baseline VARCHAR",
            &length
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->display_length =
            (uint64_t)length * column_effective_max_bytes_per_character(table, column);
        *out_matched = true;
        return MYLITE_OK;
    }

    text_info = text_family_type_info_for_logical_type(column->logical_type);
    if (text_info == NULL) {
        set_unsupported_error(database, "result metadata supports only baseline TEXT descriptors");
        return MYLITE_ERROR;
    }
    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_BLOB;
    descriptor->display_length =
        text_info->maximum_length * column_effective_max_bytes_per_character(table, column);
    descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BLOB;
    *out_matched = true;
    return MYLITE_OK;
}

static int populate_enum_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
) {
    struct enum_type_info info = {0};
    int rc = MYLITE_OK;

    *out_matched = false;
    if (!column_descriptor_is_enum(column)) {
        return MYLITE_OK;
    }
    rc = enum_type_info_for_logical_type(
        database,
        column->logical_type,
        "result metadata supports only baseline ENUM descriptors",
        &info
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_STRING;
    descriptor->charset_id =
        result_metadata_collation_id(column_effective_collation_name(table, column));
    descriptor->collation_id = descriptor->charset_id;
    descriptor->display_length = (uint64_t)info.max_label_character_length *
                                 column_effective_max_bytes_per_character(table, column);
    descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_ENUM;
    if (column_effective_collation_is_binary(table, column)) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BINARY;
    }
    *out_matched = true;
    return MYLITE_OK;
}

static int populate_set_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
) {
    struct set_type_info info = {0};
    int rc = MYLITE_OK;

    *out_matched = false;
    if (!column_descriptor_is_set(column)) {
        return MYLITE_OK;
    }
    rc = set_type_info_for_logical_type(
        database,
        column->logical_type,
        "result metadata supports only baseline SET descriptors",
        &info
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_STRING;
    descriptor->charset_id =
        result_metadata_collation_id(column_effective_collation_name(table, column));
    descriptor->collation_id = descriptor->charset_id;
    descriptor->display_length = (uint64_t)info.max_display_character_length *
                                 column_effective_max_bytes_per_character(table, column);
    descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_SET;
    if (column_effective_collation_is_binary(table, column)) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BINARY;
    }
    *out_matched = true;
    return MYLITE_OK;
}

static int populate_json_result_column_descriptor(
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
) {
    *out_matched = false;
    if (!column_descriptor_is_json(column)) {
        return MYLITE_OK;
    }

    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_JSON;
    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    descriptor->display_length = longtext_max_length;
    descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BLOB | MYLITE_RESULT_COLUMN_FLAG_BINARY;
    *out_matched = true;
    return MYLITE_OK;
}

static int populate_spatial_result_column_descriptor(
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
) {
    *out_matched = false;
    if (!column_descriptor_is_spatial(column)) {
        return MYLITE_OK;
    }

    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_GEOMETRY;
    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    descriptor->display_length = UINT32_MAX;
    descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BLOB | MYLITE_RESULT_COLUMN_FLAG_BINARY;
    *out_matched = true;
    return MYLITE_OK;
}

static int populate_binary_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info = NULL;

    (void)database;
    *out_matched = false;
    if (!column_descriptor_is_binary_string_family(column)) {
        return MYLITE_OK;
    }

    info = binary_string_type_info_for_logical_type(column->logical_type, &storage);
    if (info == NULL) {
        set_unsupported_error(
            database,
            "result metadata supports only baseline binary descriptors"
        );
        return MYLITE_ERROR;
    }

    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    descriptor->display_length = info->maximum_length;
    descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BINARY;
    if (info->blob_family) {
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_BLOB;
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BLOB;
    } else if (info->fixed_length) {
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_STRING;
    } else {
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
    }
    *out_matched = true;
    return MYLITE_OK;
}

static int populate_temporal_result_column_descriptor(
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
) {
    *out_matched = true;
    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BINARY;
    if (column_descriptor_is_date(column)) {
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_DATE;
        descriptor->display_length = date_text_length;
        return MYLITE_OK;
    }
    if (column_descriptor_is_time(column)) {
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_TIME;
        descriptor->display_length = time_text_maximum_length;
        return MYLITE_OK;
    }
    if (column_descriptor_is_datetime(column)) {
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_DATETIME;
        descriptor->display_length = datetime_text_length;
        return MYLITE_OK;
    }
    if (column_descriptor_is_timestamp(column)) {
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_TIMESTAMP;
        descriptor->display_length = datetime_text_length;
        return MYLITE_OK;
    }

    *out_matched = false;
    descriptor->flags &= ~MYLITE_RESULT_COLUMN_FLAG_BINARY;
    return MYLITE_OK;
}

static int populate_bit_result_column_descriptor(
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
) {
    uint64_t width = 0U;

    *out_matched = false;
    if (!column_descriptor_is_bit(column)) {
        return MYLITE_OK;
    }
    if (bit_width_for_logical_type(column->logical_type, &width) != MYLITE_OK) {
        return MYLITE_ERROR;
    }

    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_BIT;
    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    descriptor->display_length = width;
    descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_UNSIGNED;
    *out_matched = true;
    return MYLITE_OK;
}

static void apply_result_column_descriptor_flags(
    const struct mylite_catalog_column_descriptor *column,
    const struct result_column_metadata_context *metadata_context,
    struct mylite_result_column_descriptor *descriptor
) {
    struct loaded_index_info_span indexes = {
        .indexes = metadata_context->indexes,
        .count = metadata_context->index_count,
    };
    bool is_key_part = false;

    if (!column->is_nullable) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_NOT_NULL;
    }
    if (column->is_auto_increment) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_AUTO_INCREMENT;
    }
    if (!column->is_nullable && !column->is_auto_increment &&
        column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NONE) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT;
    }
    if (primary_key_info_contains_column_id(&metadata_context->primary_key, column->column_id)) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_PRI_KEY;
        is_key_part = true;
    } else if (column_has_unique_secondary_index(indexes, column->column_id)) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_UNIQUE_KEY;
        is_key_part = true;
    }
    if (column_has_nonunique_secondary_index(indexes, column->column_id)) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_MULTIPLE_KEY;
        is_key_part = true;
    }
    if (is_key_part) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_PART_KEY;
    }
}

static const char *column_effective_character_set_name(
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column_descriptor_is_national_char_or_varchar(column)) {
        return national_character_set_name;
    }
    if (column == NULL ||
        (!column_descriptor_is_string_family(column) && !column_descriptor_is_enum(column) &&
         !column_descriptor_is_set(column))) {
        return NULL;
    }
    if (column->character_set_name[0] != '\0' || column->collation_name[0] != '\0') {
        const struct mylite_execution_catalog_collation *collation = NULL;

        if (column->character_set_name[0] != '\0') {
            return column->character_set_name;
        }
        collation = collation_by_name(column->collation_name);
        if (collation != NULL) {
            return collation->character_set;
        }
        return MYLITE_CATALOG_DEFAULT_TABLE_CHARSET;
    }
    if (table != NULL && table->default_charset[0] != '\0') {
        return table->default_charset;
    }
    return MYLITE_CATALOG_DEFAULT_TABLE_CHARSET;
}

static const char *column_effective_collation_name(
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column_descriptor_is_national_char_or_varchar(column)) {
        return national_collation_name;
    }
    if (column == NULL ||
        (!column_descriptor_is_string_family(column) && !column_descriptor_is_enum(column) &&
         !column_descriptor_is_set(column))) {
        return NULL;
    }
    if (column->collation_name[0] != '\0') {
        return column->collation_name;
    }
    if (column->character_set_name[0] != '\0') {
        const struct mylite_execution_catalog_character_set *character_set =
            character_set_by_name(column->character_set_name);

        if (character_set != NULL) {
            return character_set->default_collation;
        }
        return MYLITE_CATALOG_DEFAULT_TABLE_COLLATION;
    }
    if (table != NULL && table->default_collation[0] != '\0') {
        return table->default_collation;
    }
    return MYLITE_CATALOG_DEFAULT_TABLE_COLLATION;
}

static bool column_effective_collation_is_binary(
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    const char *collation_name = column_effective_collation_name(table, column);

    return (collation_name != NULL &&
            (text_equals_ascii_case_insensitive(collation_name, "utf8mb4_bin") ||
             text_equals_ascii_case_insensitive(collation_name, "utf8mb4_0900_bin") ||
             text_equals_ascii_case_insensitive(collation_name, "ascii_bin"))) != 0;
}

static uint64_t column_effective_max_bytes_per_character(
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    const char *character_set_name = column_effective_character_set_name(table, column);
    const struct mylite_execution_catalog_character_set *character_set =
        character_set_by_name(character_set_name);
    struct mylite_sql_source_span maxlen_span = {0};
    uint64_t maxlen = 0U;

    if (character_set != NULL) {
        maxlen_span = (struct mylite_sql_source_span){
            .text = character_set->maxlen,
            .length = strlen(character_set->maxlen),
        };
        if (parse_unsigned_integer_literal(&maxlen_span, &maxlen) == MYLITE_OK && maxlen > 0U) {
            return maxlen;
        }
    }
    return logical_type_max_bytes_per_character(column == NULL ? NULL : column->logical_type);
}

static uint32_t result_metadata_collation_id(const char *collation_name) {
    const struct mylite_execution_catalog_collation *collation = NULL;
    const struct mylite_execution_catalog_scalar_collation *scalar_collation = NULL;
    struct mylite_sql_source_span id_span = {0};
    uint64_t id = 0U;

    scalar_collation = scalar_collation_info_by_name(collation_name);
    if (scalar_collation != NULL) {
        return scalar_collation->id;
    }

    collation = collation_by_name(collation_name);
    if (collation == NULL) {
        return mysql_collation_utf8mb4_0900_ai_ci_id;
    }

    id_span = (struct mylite_sql_source_span){
        .text = collation->id,
        .length = strlen(collation->id),
    };
    if (parse_unsigned_integer_literal(&id_span, &id) != MYLITE_OK || id > UINT32_MAX) {
        return mysql_collation_utf8mb4_0900_ai_ci_id;
    }
    return (uint32_t)id;
}

static bool result_metadata_integer_descriptor(
    const char *logical_type,
    enum mylite_result_logical_type *out_type,
    uint64_t *out_display_length,
    bool *out_unsigned
) {
    if (logical_type == NULL) {
        return false;
    }
    if (strcmp(logical_type, "TINYINT(1)") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_TINY;
        *out_display_length = mysql_tinyint_bool_display_length;
        *out_unsigned = false;
        return true;
    }
    if (strcmp(logical_type, "TINYINT") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_TINY;
        *out_display_length = mysql_signed_tinyint_display_length;
        *out_unsigned = false;
        return true;
    }
    if (strcmp(logical_type, "TINYINT UNSIGNED") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_TINY;
        *out_display_length = mysql_unsigned_tinyint_display_length;
        *out_unsigned = true;
        return true;
    }
    if (strcmp(logical_type, "SMALLINT") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_SHORT;
        *out_display_length = mysql_signed_smallint_display_length;
        *out_unsigned = false;
        return true;
    }
    if (strcmp(logical_type, "SMALLINT UNSIGNED") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_SHORT;
        *out_display_length = mysql_unsigned_smallint_display_length;
        *out_unsigned = true;
        return true;
    }
    if (strcmp(logical_type, "MEDIUMINT") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_INT24;
        *out_display_length = mysql_signed_mediumint_display_length;
        *out_unsigned = false;
        return true;
    }
    if (strcmp(logical_type, "MEDIUMINT UNSIGNED") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_INT24;
        *out_display_length = mysql_unsigned_mediumint_display_length;
        *out_unsigned = true;
        return true;
    }
    if (strcmp(logical_type, "INT") == 0 || strcmp(logical_type, "INTEGER") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_LONG;
        *out_display_length = mysql_signed_int_display_length;
        *out_unsigned = false;
        return true;
    }
    if (strcmp(logical_type, "INT UNSIGNED") == 0 ||
        strcmp(logical_type, "INTEGER UNSIGNED") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_LONG;
        *out_display_length = mysql_unsigned_int_display_length;
        *out_unsigned = true;
        return true;
    }
    if (strcmp(logical_type, "BIGINT") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG;
        *out_display_length = mysql_bigint_display_length;
        *out_unsigned = false;
        return true;
    }
    if (strcmp(logical_type, "BIGINT UNSIGNED") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG;
        *out_display_length = mysql_bigint_display_length;
        *out_unsigned = true;
        return true;
    }

    return false;
}

static int copy_select_item_alias_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias,
    char **out_text
) {
    int rc = MYLITE_OK;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (alias == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (alias->kind == MYLITE_SQL_AST_IDENTIFIER) {
        return copy_select_item_identifier_alias_text(database, alias, out_text);
    }
    if (alias->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(alias) == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = decode_table_option_string_literal(
            database,
            alias,
            out_text,
            (struct table_option_name_policy){
                .identifier_kind = "alias",
                .nul_message = "select-item aliases do not support NUL bytes",
            }
        );
        if (rc == MYLITE_OK) {
            rc = validate_select_item_alias_text(database, out_text);
        }
        return rc;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int copy_select_item_identifier_alias_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias,
    char **out_text
) {
    char identifier[select_item_alias_capacity];
    const char *source = NULL;
    size_t source_size = 0U;
    int rc = MYLITE_OK;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (alias == NULL || alias->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    source = alias->span.text;
    source_size = alias->span.length;
    if (source == NULL || source_size == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (source[0] == '`') {
        rc = copy_quoted_identifier_text(source, source_size, identifier, sizeof(identifier));
    } else {
        rc = copy_unquoted_identifier_text(source, source_size, identifier, sizeof(identifier));
    }
    if (rc != MYLITE_OK) {
        set_identifier_too_long_error(database, "alias");
        return rc;
    }

    return duplicate_text(database, identifier, out_text);
}

static int validate_select_item_alias_text(struct mylite_db *database, char **text) {
    if (text == NULL || *text == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (strlen(*text) > select_item_alias_max_length) {
        free(*text);
        *text = NULL;
        set_identifier_too_long_error(database, "alias");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int duplicate_text(struct mylite_db *database, const char *source, char **out_text) {
    size_t length = 0U;
    char *text = NULL;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (source == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    length = strlen(source);
    if (length == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    text = (char *)malloc(length + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(text, source, length + 1U);
    *out_text = text;

    return MYLITE_OK;
}

static int plan_select_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *out_predicate
) {
    const struct select_predicate_plan_options options = {
        .allow_exists = false,
    };

    return plan_select_predicate_with_options(
        database,
        where_clause,
        source_context,
        table_columns,
        table_column_count,
        &options,
        out_predicate
    );
}

static int plan_joined_select_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *source_context,
    struct planned_select_predicate *out_predicate
) {
    const struct select_predicate_plan_options options = {
        .allow_exists = false,
        .allow_same_scope_column_reference_rhs = true,
        .allow_date_format_numeric_predicate = true,
    };

    return plan_select_predicate_with_options(
        database,
        where_clause,
        source_context,
        NULL,
        0U,
        &options,
        out_predicate
    );
}

static int plan_select_predicate_with_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    int rc = MYLITE_OK;

    *out_predicate = (struct planned_select_predicate){0};
    out_predicate->qualify_column_references = select_source_context_is_joined(source_context);
    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    if (where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
        return MYLITE_ERROR;
    }

    rc = plan_select_predicate_node(
        database,
        child_at(where_clause, 0U),
        source_context,
        table_columns,
        table_column_count,
        options,
        out_predicate
    );
    if (rc != MYLITE_OK) {
        planned_select_predicate_deinit(out_predicate);
    }

    return rc;
}

static void planned_select_predicate_deinit(struct planned_select_predicate *predicate) {
    if (predicate == NULL) {
        return;
    }

    for (size_t node_index = 0U; node_index < predicate->node_count; ++node_index) {
        planned_value_deinit(&predicate->nodes[node_index].value);
        planned_value_deinit(&predicate->nodes[node_index].upper_value);
        for (size_t value_index = 0U; value_index < predicate->nodes[node_index].value_count;
             ++value_index) {
            planned_value_deinit(&predicate->nodes[node_index].values[value_index]);
        }
        free(predicate->nodes[node_index].values);
        if (predicate->nodes[node_index].row_scalar_expression != NULL) {
            planned_row_scalar_expression_deinit(
                predicate->nodes[node_index].row_scalar_expression
            );
        }
        free(predicate->nodes[node_index].row_scalar_expression);
        planned_exists_subquery_deinit(predicate->nodes[node_index].exists_subquery);
        free(predicate->nodes[node_index].exists_subquery);
        planned_in_subquery_deinit(predicate->nodes[node_index].in_subquery);
        free(predicate->nodes[node_index].in_subquery);
    }
    free(predicate->nodes);
    *predicate = (struct planned_select_predicate){0};
}

static void planned_select_predicate_deinit_without_exists(
    struct planned_select_predicate *predicate
) {
    if (predicate == NULL) {
        return;
    }

    for (size_t node_index = 0U; node_index < predicate->node_count; ++node_index) {
        planned_value_deinit(&predicate->nodes[node_index].value);
        planned_value_deinit(&predicate->nodes[node_index].upper_value);
        for (size_t value_index = 0U; value_index < predicate->nodes[node_index].value_count;
             ++value_index) {
            planned_value_deinit(&predicate->nodes[node_index].values[value_index]);
        }
        free(predicate->nodes[node_index].values);
        if (predicate->nodes[node_index].row_scalar_expression != NULL) {
            planned_row_scalar_expression_deinit(
                predicate->nodes[node_index].row_scalar_expression
            );
        }
        free(predicate->nodes[node_index].row_scalar_expression);
    }
    free(predicate->nodes);
    *predicate = (struct planned_select_predicate){0};
}

static void planned_exists_subquery_deinit(struct planned_exists_subquery *subquery) {
    if (subquery == NULL) {
        return;
    }

    free(subquery->source.columns);
    planned_select_predicate_deinit_without_exists(&subquery->predicate);
    *subquery = (struct planned_exists_subquery){.has_table_source = false};
}

static void planned_in_subquery_deinit(struct planned_in_subquery *subquery) {
    if (subquery == NULL) {
        return;
    }

    free(subquery->source.columns);
    planned_select_predicate_deinit_without_exists(&subquery->predicate);
    *subquery = (struct planned_in_subquery){0};
}

static int plan_select_predicate_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    struct predicate_work_item *items = NULL;
    size_t item_count = 0U;
    size_t *result_indexes = NULL;
    size_t result_index_count = 0U;
    int rc = append_predicate_work_node(database, &items, &item_count, predicate_node);

    while (rc == MYLITE_OK && item_count > 0U) {
        struct predicate_work_item item = items[--item_count];
        rc = plan_select_predicate_work_item(
            database,
            item,
            &items,
            &item_count,
            &result_indexes,
            &result_index_count,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate
        );
    }

    if (rc == MYLITE_OK && result_index_count == 1U) {
        out_predicate->root_index = result_indexes[0];
        out_predicate->has_root = true;
    } else if (rc == MYLITE_OK) {
        set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
        rc = MYLITE_ERROR;
    }

    free(result_indexes);
    free(items);
    return rc;
}

static int plan_select_predicate_node_without_exists(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    struct predicate_work_item *items = NULL;
    size_t item_count = 0U;
    size_t *result_indexes = NULL;
    size_t result_index_count = 0U;
    int rc = append_predicate_work_node(database, &items, &item_count, predicate_node);

    while (rc == MYLITE_OK && item_count > 0U) {
        struct predicate_work_item item = items[--item_count];
        rc = plan_select_predicate_work_item_without_exists(
            database,
            item,
            &items,
            &item_count,
            &result_indexes,
            &result_index_count,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate
        );
    }

    if (rc == MYLITE_OK && result_index_count == 1U) {
        out_predicate->root_index = result_indexes[0];
        out_predicate->has_root = true;
    } else if (rc == MYLITE_OK) {
        set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
        rc = MYLITE_ERROR;
    }

    free(result_indexes);
    free(items);
    return rc;
}

static int plan_select_predicate_work_item(
    struct mylite_db *database,
    struct predicate_work_item item,
    struct predicate_work_item **items,
    size_t *item_count,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    switch (item.kind) {
    case PREDICATE_WORK_DEPRECATED_AND_WARNING:
        return append_deprecated_logical_and_warning(database);
    case PREDICATE_WORK_DEPRECATED_OR_WARNING:
        return append_deprecated_logical_or_warning(database);
    case PREDICATE_WORK_FINISH_LOGICAL:
        return finish_planned_select_logical_predicate(
            database,
            item.operator_kind,
            result_indexes,
            result_index_count,
            out_predicate
        );
    case PREDICATE_WORK_FINISH_NOT:
        return finish_planned_select_not_predicate(
            database,
            result_indexes,
            result_index_count,
            out_predicate
        );
    case PREDICATE_WORK_NODE:
        return plan_select_predicate_ast_node(
            database,
            item.node,
            items,
            item_count,
            result_indexes,
            result_index_count,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate
        );
    }

    set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
    return MYLITE_ERROR;
}

static int plan_select_predicate_work_item_without_exists(
    struct mylite_db *database,
    struct predicate_work_item item,
    struct predicate_work_item **items,
    size_t *item_count,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    switch (item.kind) {
    case PREDICATE_WORK_DEPRECATED_AND_WARNING:
        return append_deprecated_logical_and_warning(database);
    case PREDICATE_WORK_DEPRECATED_OR_WARNING:
        return append_deprecated_logical_or_warning(database);
    case PREDICATE_WORK_FINISH_LOGICAL:
        return finish_planned_select_logical_predicate(
            database,
            item.operator_kind,
            result_indexes,
            result_index_count,
            out_predicate
        );
    case PREDICATE_WORK_FINISH_NOT:
        return finish_planned_select_not_predicate(
            database,
            result_indexes,
            result_index_count,
            out_predicate
        );
    case PREDICATE_WORK_NODE:
        return plan_select_predicate_ast_node_without_exists(
            database,
            item.node,
            items,
            item_count,
            result_indexes,
            result_index_count,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate
        );
    }

    set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
    return MYLITE_ERROR;
}

static int finish_planned_select_logical_predicate(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    size_t **result_indexes,
    size_t *result_index_count,
    struct planned_select_predicate *out_predicate
) {
    enum planned_select_predicate_kind kind = PLANNED_SELECT_PREDICATE_NONE;
    size_t left_index = 0U;
    size_t right_index = 0U;
    size_t node_index = 0U;
    struct planned_select_predicate_node logical_node = {0};
    int rc = MYLITE_OK;

    if (!planned_predicate_kind_for_operator(operator_kind, &kind)) {
        set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
        return MYLITE_ERROR;
    }

    rc = pop_predicate_result_index(*result_indexes, result_index_count, &right_index);
    if (rc == MYLITE_OK) {
        rc = pop_predicate_result_index(*result_indexes, result_index_count, &left_index);
    }
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
        return rc;
    }

    logical_node = (struct planned_select_predicate_node){
        .kind = kind,
        .operator_kind = operator_kind,
        .left_index = left_index,
        .right_index = right_index,
    };
    rc = append_planned_select_predicate_node(database, out_predicate, &logical_node, &node_index);
    if (rc == MYLITE_OK) {
        rc =
            append_predicate_result_index(database, result_indexes, result_index_count, node_index);
    }

    return rc;
}

static int finish_planned_select_not_predicate(
    struct mylite_db *database,
    size_t **result_indexes,
    size_t *result_index_count,
    struct planned_select_predicate *out_predicate
) {
    size_t child_index = 0U;
    size_t node_index = 0U;
    struct planned_select_predicate_node not_node = {0};
    int rc = pop_predicate_result_index(*result_indexes, result_index_count, &child_index);

    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
        return rc;
    }

    not_node = (struct planned_select_predicate_node){
        .kind = PLANNED_SELECT_PREDICATE_NOT,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT,
        .left_index = child_index,
        .right_index = SIZE_MAX,
    };
    rc = append_planned_select_predicate_node(database, out_predicate, &not_node, &node_index);
    if (rc == MYLITE_OK) {
        rc =
            append_predicate_result_index(database, result_indexes, result_index_count, node_index);
    }

    return rc;
}

static int plan_select_predicate_ast_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct predicate_work_item **items,
    size_t *item_count,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    const struct mylite_sql_ast_node *current = unwrap_parenthesized_predicate(predicate_node);

    if (current != NULL && current->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        return append_select_predicate_not_work(database, current, items, item_count);
    }
    if (is_logical_predicate_node(current)) {
        return append_select_predicate_logical_work(database, current, items, item_count);
    }
    if (current != NULL && (predicate_node_is_find_in_set_expression(current) ||
                            predicate_node_is_regexp_like_expression(current) ||
                            predicate_node_is_json_valid_expression(current) ||
                            predicate_node_is_json_contains_expression(current) ||
                            predicate_node_is_string_length_expression(current) ||
                            (options != NULL && options->allow_date_format_numeric_predicate &&
                             predicate_node_is_date_format_numeric_predicate_expression(current)) ||
                            predicate_node_is_temporal_extract_expression(current) ||
                            predicate_node_is_scalar_literal_expression(current) ||
                            current->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_BETWEEN_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_IN_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_EXISTS_PREDICATE)) {
        return plan_select_predicate_leaf_node(
            database,
            current,
            result_indexes,
            result_index_count,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate
        );
    }

    set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
    return MYLITE_ERROR;
}

static int plan_select_predicate_ast_node_without_exists(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct predicate_work_item **items,
    size_t *item_count,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    const struct mylite_sql_ast_node *current = unwrap_parenthesized_predicate(predicate_node);

    if (current != NULL && current->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        return append_select_predicate_not_work(database, current, items, item_count);
    }
    if (is_logical_predicate_node(current)) {
        return append_select_predicate_logical_work(database, current, items, item_count);
    }
    if (current != NULL && (predicate_node_is_find_in_set_expression(current) ||
                            predicate_node_is_regexp_like_expression(current) ||
                            predicate_node_is_json_valid_expression(current) ||
                            predicate_node_is_json_contains_expression(current) ||
                            predicate_node_is_string_length_expression(current) ||
                            (options != NULL && options->allow_date_format_numeric_predicate &&
                             predicate_node_is_date_format_numeric_predicate_expression(current)) ||
                            predicate_node_is_temporal_extract_expression(current) ||
                            predicate_node_is_scalar_literal_expression(current) ||
                            current->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_BETWEEN_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_IN_PREDICATE)) {
        return plan_select_predicate_leaf_node_without_exists(
            database,
            current,
            result_indexes,
            result_index_count,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate
        );
    }

    set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
    return MYLITE_ERROR;
}

static const struct mylite_sql_ast_node *unwrap_parenthesized_predicate(
    const struct mylite_sql_ast_node *node
) {
    const struct mylite_sql_ast_node *current = node;

    while (current != NULL && current->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        current = child_at(current, 0U);
    }

    return current;
}

static bool is_logical_predicate_node(const struct mylite_sql_ast_node *node) {
    if (node == NULL) {
        return false;
    }

    if (node->kind == MYLITE_SQL_AST_AND_PREDICATE) {
        return true;
    }
    if (node->kind == MYLITE_SQL_AST_OR_PREDICATE) {
        return true;
    }
    if (node->kind == MYLITE_SQL_AST_XOR_PREDICATE) {
        return true;
    }
    return false;
}

static int append_select_predicate_logical_work(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct predicate_work_item **items,
    size_t *item_count
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate_node);
    int rc = append_predicate_work_finish_logical(database, items, item_count, operator_kind);

    if (rc == MYLITE_OK) {
        rc = append_predicate_work_node(database, items, item_count, child_at(predicate_node, 1U));
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_deprecated_warning_work(
            database,
            operator_kind,
            items,
            item_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_predicate_work_node(database, items, item_count, child_at(predicate_node, 0U));
    }

    return rc;
}

static int append_select_predicate_not_work(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct predicate_work_item **items,
    size_t *item_count
) {
    int rc = append_predicate_work_finish_not(database, items, item_count);

    if (rc == MYLITE_OK) {
        rc = append_predicate_work_node(database, items, item_count, child_at(predicate_node, 0U));
    }

    return rc;
}

static int append_select_predicate_deprecated_warning_work(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    struct predicate_work_item **items,
    size_t *item_count
) {
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND) {
        return append_predicate_work_deprecated_and_warning(database, items, item_count);
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR) {
        return append_predicate_work_deprecated_or_warning(database, items, item_count);
    }

    return MYLITE_OK;
}

static int plan_select_predicate_leaf_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    size_t node_index = 0U;
    int rc = MYLITE_OK;

    if (predicate_node_is_find_in_set_expression(predicate_node)) {
        rc = plan_find_in_set_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_regexp_like_expression(predicate_node)) {
        rc = plan_regexp_like_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_json_valid_expression(predicate_node)) {
        rc = plan_json_valid_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_json_contains_expression(predicate_node)) {
        rc = plan_json_contains_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_string_length_expression(predicate_node)) {
        rc = plan_string_length_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_date_format_numeric_predicate_expression(predicate_node)) {
        rc = plan_date_format_numeric_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_temporal_extract_expression(predicate_node)) {
        rc = plan_temporal_extract_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_scalar_literal_expression(predicate_node)) {
        rc = plan_scalar_literal_truth_predicate(
            database,
            predicate_node,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        rc = plan_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        rc = plan_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE) {
        rc = plan_is_boolean_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_BETWEEN_PREDICATE) {
        rc = plan_between_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_IN_PREDICATE) {
        rc = plan_in_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate,
            &node_index
        );
    } else {
        rc = plan_exists_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate,
            &node_index
        );
    }
    if (rc == MYLITE_OK) {
        rc =
            append_predicate_result_index(database, result_indexes, result_index_count, node_index);
    }

    return rc;
}

static int plan_select_predicate_leaf_node_without_exists(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    size_t node_index = 0U;
    int rc = MYLITE_OK;

    if (predicate_node_is_find_in_set_expression(predicate_node)) {
        rc = plan_find_in_set_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_regexp_like_expression(predicate_node)) {
        rc = plan_regexp_like_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_json_valid_expression(predicate_node)) {
        rc = plan_json_valid_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_json_contains_expression(predicate_node)) {
        rc = plan_json_contains_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_string_length_expression(predicate_node)) {
        rc = plan_string_length_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_date_format_numeric_predicate_expression(predicate_node)) {
        rc = plan_date_format_numeric_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_temporal_extract_expression(predicate_node)) {
        rc = plan_temporal_extract_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_scalar_literal_expression(predicate_node)) {
        rc = plan_scalar_literal_truth_predicate(
            database,
            predicate_node,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        rc = plan_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        rc = plan_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE) {
        rc = plan_is_boolean_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_BETWEEN_PREDICATE) {
        rc = plan_between_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else {
        rc = plan_in_literal_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    }
    if (rc == MYLITE_OK) {
        rc =
            append_predicate_result_index(database, result_indexes, result_index_count, node_index);
    }

    return rc;
}

static bool planned_predicate_kind_for_operator(
    enum mylite_sql_ast_operator operator_kind,
    enum planned_select_predicate_kind *out_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND:
        *out_kind = PLANNED_SELECT_PREDICATE_AND;
        return true;
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR:
        *out_kind = PLANNED_SELECT_PREDICATE_OR;
        return true;
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
        *out_kind = PLANNED_SELECT_PREDICATE_XOR;
        return true;
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_LEFT_SHIFT:
    case MYLITE_SQL_AST_OPERATOR_RIGHT_SHIFT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_RLIKE:
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_CONCAT:
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
        break;
    }

    return false;
}

static int plan_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    bool row_scalar_function_handled = false;
    int row_scalar_function_rc = plan_row_scalar_function_comparison_predicate(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        predicate,
        out_node_index,
        &row_scalar_function_handled
    );

    if (row_scalar_function_handled || row_scalar_function_rc != MYLITE_OK) {
        return row_scalar_function_rc;
    }

    if (predicate_node_is_scalar_literal_expression(child_at(predicate_node, 0U))) {
        if (predicate_node_is_scalar_literal_expression(child_at(predicate_node, 1U))) {
            return plan_scalar_literal_comparison_predicate(
                database,
                predicate_node,
                predicate,
                out_node_index
            );
        }
        if (predicate_node_is_column_reference(child_at(predicate_node, 1U))) {
            return plan_literal_left_column_comparison_predicate(
                database,
                predicate_node,
                source_context,
                table_columns,
                table_column_count,
                predicate,
                out_node_index
            );
        }
    }

    if (comparison_predicate_rhs_is_column_reference(predicate_node)) {
        if (options != NULL && options->allow_same_scope_column_reference_rhs) {
            return plan_same_scope_column_comparison_predicate(
                database,
                predicate_node,
                source_context,
                table_columns,
                table_column_count,
                predicate,
                out_node_index
            );
        }
        if (options == NULL || !options->allow_column_reference_rhs) {
            set_unsupported_error(
                database,
                "WHERE column-to-column predicates are supported only inside EXISTS"
            );
            return MYLITE_ERROR;
        }
        return plan_column_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            options,
            predicate,
            out_node_index
        );
    }

    int rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );

    if (rc == MYLITE_OK) {
        rc = validate_comparison_predicate_column(database, &node);
    }
    if (rc == MYLITE_OK) {
        if (comparison_operator_is_like(node.operator_kind)) {
            node.like_uses_escape = (bool)!session_sql_mode_has(
                &database->session,
                MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES
            );
            rc = convert_predicate_string_pattern_literal(
                database,
                child_at(predicate_node, 1U),
                &node.value
            );
        } else if (comparison_operator_is_regexp(node.operator_kind)) {
            rc = convert_predicate_regexp_pattern_literal(
                database,
                child_at(predicate_node, 1U),
                &node.value
            );
        } else if (predicate_comparison_value_is_null(child_at(predicate_node, 1U))) {
            node.value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        } else {
            rc = convert_predicate_value_with_date_comparison_mode(
                database,
                child_at(predicate_node, 1U),
                &node.column,
                &node.value,
                &node.compare_date_as_datetime
            );
        }
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_value_deinit(&node.upper_value);
        return rc;
    }

    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static int plan_row_scalar_function_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate_node, 0U);

    *out_handled = true;
    if (predicate_node_is_find_in_set_expression(left_node)) {
        return plan_find_in_set_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_regexp_like_expression(left_node)) {
        return plan_regexp_like_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_json_valid_expression(left_node)) {
        return plan_json_valid_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_json_contains_expression(left_node)) {
        return plan_json_contains_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_string_length_expression(left_node)) {
        return plan_string_length_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_substring_expression(left_node)) {
        return plan_substring_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_temporal_extract_expression(left_node)) {
        return plan_temporal_extract_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }

    *out_handled = false;
    return MYLITE_OK;
}

static int plan_scalar_literal_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_where_scalar_literal_expression(database, predicate_node, &node);

    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_scalar_literal_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_where_scalar_literal_expression(database, child_at(predicate_node, 0U), &node);

    if (rc == MYLITE_OK) {
        rc = plan_where_scalar_literal_value(database, child_at(predicate_node, 1U), &node.value);
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_literal_left_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_COMPARISON,
        .operator_kind = comparison_operator_flipped(mylite_sql_ast_node_operator(predicate_node)),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 1U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );

    if (rc == MYLITE_OK) {
        rc = validate_comparison_predicate_column(database, &node);
    }
    if (rc == MYLITE_OK) {
        if (predicate_comparison_value_is_null(child_at(predicate_node, 0U))) {
            node.value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        } else {
            rc = convert_predicate_value_with_date_comparison_mode(
                database,
                child_at(predicate_node, 0U),
                &node.column,
                &node.value,
                &node.compare_date_as_datetime
            );
        }
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_value_deinit(&node.upper_value);
        return rc;
    }

    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static int plan_same_scope_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
        .value_is_column_reference = true,
    };
    int rc = MYLITE_OK;

    if (node.operator_kind != MYLITE_SQL_AST_OPERATOR_EQUAL) {
        set_unsupported_error(database, "WHERE column-to-column predicates support only =");
        return MYLITE_ERROR;
    }

    rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );
    if (rc == MYLITE_OK) {
        rc = resolve_predicate_column_with_source_index(
            database,
            child_at(predicate_node, 1U),
            source_context,
            table_columns,
            table_column_count,
            &node.value_column,
            &node.value_column_source_index
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!same_scope_column_comparison_is_supported(&node)) {
        set_unsupported_error(
            database,
            "WHERE column-to-column predicates support only same-family integer or string "
            "descriptor equality"
        );
        return MYLITE_ERROR;
    }

    predicate->qualify_column_references = true;
    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static bool same_scope_column_comparison_is_supported(
    const struct planned_select_predicate_node *node
) {
    return join_condition_columns_are_compatible(&node->column, &node->value_column);
}

static int plan_scalar_literal_is_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_value value = {.is_null = false, .is_text = false, .integer = 0};
    int rc = plan_where_scalar_literal_value(database, child_at(predicate_node, 0U), &value);
    bool truth = false;

    if (rc == MYLITE_OK) {
        truth = scalar_literal_is_result(&value, mylite_sql_ast_node_operator(predicate_node));
        rc = plan_constant_truth_predicate(database, truth, predicate, out_node_index);
    }
    planned_value_deinit(&value);
    return rc;
}

static int plan_constant_truth_predicate(
    struct mylite_db *database,
    bool truth,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = MYLITE_OK;

    node.row_scalar_expression = calloc(1U, sizeof(*node.row_scalar_expression));
    if (node.row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    node.row_scalar_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    node.row_scalar_expression->value =
        (struct planned_value){.is_null = false, .is_text = false, .integer = 0};
    if (truth) {
        node.row_scalar_expression->value.integer = 1;
    }

    rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_where_scalar_literal_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    node->row_scalar_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    rc = plan_where_scalar_literal_value(database, expression, &node->row_scalar_expression->value);
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_where_scalar_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = string_slice_signed_integer_value(
        database,
        expression,
        "WHERE scalar literal predicates support only integer, boolean, and NULL literals",
        "WHERE scalar literal integer literals must fit the signed 64-bit range",
        &value,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){.is_null = is_null, .is_text = false, .integer = value};
    if (is_null) {
        out_value->integer = 0;
    }
    return MYLITE_OK;
}

static bool predicate_node_is_scalar_literal_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    const struct mylite_sql_ast_node *node = unwrap_parenthesized_predicate(predicate_node);
    const struct mylite_sql_ast_node *literal = node;

    if (node != NULL && node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(node);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        literal = unwrap_parenthesized_predicate(child_at(node, 0U));
        return (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
                mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) != 0;
    }

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }
    switch (mylite_sql_ast_node_literal_kind(literal)) {
    case MYLITE_SQL_AST_LITERAL_INTEGER:
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
    case MYLITE_SQL_AST_LITERAL_NULL:
        return true;
    default:
        return false;
    }
}

static bool predicate_node_is_column_reference(const struct mylite_sql_ast_node *node) {
    return (node != NULL && (node->kind == MYLITE_SQL_AST_IDENTIFIER ||
                             node->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) != 0;
}

static bool predicate_comparison_value_is_null(const struct mylite_sql_ast_node *value_node) {
    return (value_node != NULL && value_node->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) != 0;
}

static enum mylite_sql_ast_operator comparison_operator_flipped(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return MYLITE_SQL_AST_OPERATOR_GREATER;
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL;
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return MYLITE_SQL_AST_OPERATOR_LESS;
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return MYLITE_SQL_AST_OPERATOR_LESS_EQUAL;
    default:
        return operator_kind;
    }
}

static bool scalar_literal_is_result(
    const struct planned_value *value,
    enum mylite_sql_ast_operator operator_kind
) {
    bool is_true = false;
    bool is_false = false;
    bool is_unknown = false;

    if (value == NULL || value->is_null) {
        is_unknown = true;
    } else if (value->integer == 0) {
        is_false = true;
    } else {
        is_true = true;
    }

    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
        return is_true;
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
        if (is_true) {
            return false;
        }
        return true;
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
        return is_false;
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
        if (is_false) {
            return false;
        }
        return true;
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
        return is_unknown;
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
        if (is_unknown) {
            return false;
        }
        return true;
    default:
        return false;
    }
}

static bool predicate_node_is_find_in_set_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    return (predicate_node != NULL &&
            predicate_node->kind == MYLITE_SQL_AST_FIND_IN_SET_FUNCTION) != 0;
}

static bool predicate_node_is_json_valid_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    return (predicate_node != NULL && predicate_node->kind == MYLITE_SQL_AST_JSON_VALID_FUNCTION) !=
           0;
}

static bool predicate_node_is_json_contains_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    return (predicate_node != NULL &&
            (predicate_node->kind == MYLITE_SQL_AST_JSON_CONTAINS_FUNCTION ||
             predicate_node->kind == MYLITE_SQL_AST_JSON_CONTAINS_PATH_FUNCTION)) != 0;
}

static bool predicate_node_is_regexp_like_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    return (predicate_node != NULL &&
            predicate_node->kind == MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION) != 0;
}

static bool predicate_node_is_string_length_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    return (predicate_node != NULL && is_string_length_function_kind(predicate_node->kind)) != 0;
}

static bool predicate_node_is_substring_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    if (predicate_node == NULL) {
        return false;
    }

    switch (predicate_node->kind) {
    case MYLITE_SQL_AST_SUBSTRING_FUNCTION:
    case MYLITE_SQL_AST_SUBSTR_FUNCTION:
    case MYLITE_SQL_AST_MID_FUNCTION:
        return true;
    default:
        return false;
    }
}

static bool predicate_node_is_temporal_extract_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    return (predicate_node != NULL && (predicate_node->kind == MYLITE_SQL_AST_EXTRACT_FUNCTION ||
                                       is_temporal_extract_function_kind(predicate_node->kind))) !=
           0;
}

static bool predicate_node_is_date_format_numeric_predicate_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    const struct mylite_sql_ast_node *left = NULL;

    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    if (predicate_node == NULL || predicate_node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return false;
    }

    left = unwrap_parenthesized_expression(child_at(predicate_node, 0U));
    return (left != NULL && left->kind == MYLITE_SQL_AST_DATE_FORMAT_FUNCTION) != 0;
}

static int plan_date_format_numeric_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "DATE_FORMAT() numeric predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node.row_scalar_expression = calloc(1U, sizeof(*node.row_scalar_expression));
    if (node.row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_date_format_numeric_equal_expression(
        database,
        predicate_node,
        true,
        source_context,
        table_columns,
        table_column_count,
        node.row_scalar_expression
    );
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_temporal_extract_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_temporal_extract_predicate_expression(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_temporal_extract_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_temporal_extract_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = plan_temporal_extract_predicate_value(
            database,
            child_at(predicate_node, 1U),
            &node.value
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_temporal_extract_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_temporal_extract_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_temporal_extract_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    enum mylite_temporal_extract_kind extract_kind = MYLITE_TEMPORAL_EXTRACT_DATE;
    const struct mylite_sql_ast_node *argument = NULL;
    int mode = 0;
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "temporal extract function predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    rc = resolve_temporal_extract_call(database, expression, &extract_kind, &argument, &mode);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument == NULL) {
        return MYLITE_MISUSE;
    }
    (void)mode;
    if (!temporal_extract_predicate_kind_is_numeric(extract_kind)) {
        set_unsupported_error(
            database,
            "temporal extract predicates support only numeric temporal extractor functions"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_temporal_extract_expression(
        database,
        expression,
        true,
        source_context,
        table_columns,
        table_column_count,
        node->row_scalar_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_temporal_extract_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(expression);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int64_t value = 0;
    bool is_null = false;

    if (literal != NULL && literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(literal);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        }
        literal = unwrap_parenthesized_expression(child_at(literal, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "temporal extract predicates support only integer, boolean, and NULL comparison "
            "literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        is_null = true;
    } else if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        value = 1;
    } else if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        value = 0;
    } else if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "temporal extract predicates support only integer, boolean, and NULL comparison "
            "literals"
        );
        return MYLITE_ERROR;
    } else if (
        parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        (is_negative && magnitude > (uint64_t)INT64_MAX + 1U) ||
        (!is_negative && magnitude > (uint64_t)INT64_MAX)
    ) {
        set_unsupported_error(
            database,
            "temporal extract predicate comparison literals must fit the signed 64-bit range"
        );
        return MYLITE_ERROR;
    } else if (is_negative && magnitude == (uint64_t)INT64_MAX + 1U) {
        value = INT64_MIN;
    } else if (is_negative) {
        value = -(int64_t)magnitude;
    } else {
        value = (int64_t)magnitude;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = is_null,
        .is_text = false,
        .integer = value,
    };
    return MYLITE_OK;
}

static bool temporal_extract_predicate_kind_is_numeric(enum mylite_temporal_extract_kind kind) {
    switch (kind) {
    case MYLITE_TEMPORAL_EXTRACT_YEAR:
    case MYLITE_TEMPORAL_EXTRACT_QUARTER:
    case MYLITE_TEMPORAL_EXTRACT_MONTH:
    case MYLITE_TEMPORAL_EXTRACT_DAY:
    case MYLITE_TEMPORAL_EXTRACT_YEAR_MONTH:
    case MYLITE_TEMPORAL_EXTRACT_DAYOFWEEK:
    case MYLITE_TEMPORAL_EXTRACT_DAYOFYEAR:
    case MYLITE_TEMPORAL_EXTRACT_WEEK:
    case MYLITE_TEMPORAL_EXTRACT_WEEKDAY:
    case MYLITE_TEMPORAL_EXTRACT_WEEKOFYEAR:
    case MYLITE_TEMPORAL_EXTRACT_YEARWEEK:
    case MYLITE_TEMPORAL_EXTRACT_HOUR:
    case MYLITE_TEMPORAL_EXTRACT_SIGNED_HOUR:
    case MYLITE_TEMPORAL_EXTRACT_MINUTE:
    case MYLITE_TEMPORAL_EXTRACT_SIGNED_MINUTE:
    case MYLITE_TEMPORAL_EXTRACT_SECOND:
    case MYLITE_TEMPORAL_EXTRACT_SIGNED_SECOND:
    case MYLITE_TEMPORAL_EXTRACT_MICROSECOND:
    case MYLITE_TEMPORAL_EXTRACT_SIGNED_MICROSECOND:
    case MYLITE_TEMPORAL_EXTRACT_DAY_HOUR:
    case MYLITE_TEMPORAL_EXTRACT_DAY_MINUTE:
    case MYLITE_TEMPORAL_EXTRACT_DAY_SECOND:
    case MYLITE_TEMPORAL_EXTRACT_HOUR_MINUTE:
    case MYLITE_TEMPORAL_EXTRACT_HOUR_SECOND:
    case MYLITE_TEMPORAL_EXTRACT_MINUTE_SECOND:
    case MYLITE_TEMPORAL_EXTRACT_TIME_TO_SEC:
    case MYLITE_TEMPORAL_EXTRACT_TO_DAYS:
    case MYLITE_TEMPORAL_EXTRACT_TO_SECONDS:
        return true;
    case MYLITE_TEMPORAL_EXTRACT_DATE:
    case MYLITE_TEMPORAL_EXTRACT_TIME:
    case MYLITE_TEMPORAL_EXTRACT_LAST_DAY:
    case MYLITE_TEMPORAL_EXTRACT_DAYNAME:
    case MYLITE_TEMPORAL_EXTRACT_MONTHNAME:
        return false;
    }
    return false;
}

static int plan_string_length_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_string_length_predicate_expression(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_string_length_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_string_length_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc =
            plan_string_length_predicate_value(database, child_at(predicate_node, 1U), &node.value);
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_string_length_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_string_length_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_string_length_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "string length function predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_string_length_expression(
        database,
        expression,
        true,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_WHERE,
        node->row_scalar_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_string_length_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(expression);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int64_t value = 0;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (literal != NULL && literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        literal = unwrap_parenthesized_expression(child_at(literal, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "string length predicates support only integer, boolean, and NULL comparison literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER &&
        literal_kind != MYLITE_SQL_AST_LITERAL_TRUE &&
        literal_kind != MYLITE_SQL_AST_LITERAL_FALSE &&
        literal_kind != MYLITE_SQL_AST_LITERAL_NULL) {
        set_unsupported_error(
            database,
            "string length predicates support only integer, boolean, and NULL comparison literals"
        );
        return MYLITE_ERROR;
    }

    rc = string_slice_signed_integer_value(
        database,
        expression,
        "string length predicates support only integer, boolean, and NULL comparison literals",
        "string length predicate comparison literals must fit the signed 64-bit range",
        &value,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    int64_t planned_integer = value;
    if (is_null) {
        planned_integer = 0;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = is_null,
        .is_text = false,
        .integer = planned_integer,
    };
    return MYLITE_OK;
}

static int plan_substring_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_substring_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = plan_substring_predicate_value(database, child_at(predicate_node, 1U), &node.value);
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_substring_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_substring_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_substring_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "SUBSTRING function predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_string_slice_expression(
        database,
        expression,
        true,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_WHERE,
        node->row_scalar_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_substring_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    if (predicate_comparison_value_is_null(expression)) {
        planned_value_deinit(out_value);
        *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        return MYLITE_OK;
    }

    return convert_predicate_string_literal(database, expression, out_value);
}

static int plan_find_in_set_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_find_in_set_predicate_expression(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_find_in_set_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_find_in_set_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = plan_find_in_set_predicate_integer_value(
            database,
            child_at(predicate_node, 1U),
            &node.value
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_find_in_set_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_find_in_set_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_find_in_set_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "FIND_IN_SET() predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_find_in_set_expression(
        database,
        expression,
        true,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_WHERE,
        node->row_scalar_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_find_in_set_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    int64_t value = 0;
    int64_t planned_integer = 0;
    bool is_null = false;
    int rc = string_slice_signed_integer_value(
        database,
        expression,
        "FIND_IN_SET() predicates support only integer and boolean comparison literals",
        "FIND_IN_SET() predicate comparison literals must fit the signed 64-bit range",
        &value,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    if (!is_null) {
        planned_integer = value;
    }
    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = is_null,
        .is_text = false,
        .integer = planned_integer,
    };
    return MYLITE_OK;
}

static int plan_regexp_like_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_regexp_like_predicate_expression(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_regexp_like_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_regexp_like_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = plan_regexp_like_predicate_integer_value(
            database,
            child_at(predicate_node, 1U),
            &node.value
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_regexp_like_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_regexp_like_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_regexp_like_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "REGEXP_LIKE() predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_regexp_like_expression(
        database,
        expression,
        true,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_WHERE,
        node->row_scalar_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_regexp_like_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = string_slice_signed_integer_value(
        database,
        expression,
        "REGEXP_LIKE() predicates support only integer and boolean comparison literals",
        "REGEXP_LIKE() predicate comparison literals must fit the signed 64-bit range",
        &value,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = is_null,
        .is_text = false,
        .integer = value,
    };
    return MYLITE_OK;
}

static int plan_json_valid_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_json_valid_predicate_expression(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_json_valid_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_json_valid_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = plan_json_valid_predicate_integer_value(
            database,
            child_at(predicate_node, 1U),
            &node.value
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_json_valid_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_json_valid_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_json_valid_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "JSON_VALID() predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_json_valid_expression(
        database,
        expression,
        true,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_WHERE,
        node->row_scalar_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_json_valid_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = string_slice_signed_integer_value(
        database,
        expression,
        "JSON_VALID() predicates support only integer and boolean comparison literals",
        "JSON_VALID() predicate comparison literals must fit the signed 64-bit range",
        &value,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = is_null,
        .is_text = false,
        .integer = value,
    };
    return MYLITE_OK;
}

static int plan_json_contains_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_json_contains_predicate_expression(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_json_contains_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_json_contains_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = plan_json_contains_predicate_integer_value(
            database,
            child_at(predicate_node, 1U),
            &node.value
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_json_contains_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_json_contains_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_json_contains_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "JSON_CONTAINS() predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_json_expression(
        database,
        expression,
        true,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_WHERE,
        node->row_scalar_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_json_contains_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = string_slice_signed_integer_value(
        database,
        expression,
        "JSON_CONTAINS() predicates support only integer and boolean comparison literals",
        "JSON_CONTAINS() predicate comparison literals must fit the signed 64-bit range",
        &value,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = is_null,
        .is_text = false,
        .integer = value,
    };
    return MYLITE_OK;
}

static bool comparison_predicate_rhs_is_column_reference(
    const struct mylite_sql_ast_node *predicate_node
) {
    const struct mylite_sql_ast_node *right = child_at(predicate_node, 1U);

    return (right != NULL && (right->kind == MYLITE_SQL_AST_IDENTIFIER ||
                              right->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) != 0;
}

static int validate_comparison_predicate_column(
    struct mylite_db *database,
    const struct planned_select_predicate_node *node
) {
    if (comparison_operator_is_like(node->operator_kind) &&
        column_descriptor_is_enum(&node->column)) {
        set_unsupported_error(database, "WHERE enum predicates support only =, <=>, <>, and !=");
        return MYLITE_ERROR;
    }
    if (comparison_operator_is_like(node->operator_kind) &&
        column_descriptor_is_set(&node->column)) {
        set_unsupported_error(database, "WHERE set predicates support only =, <=>, <>, and !=");
        return MYLITE_ERROR;
    }
    if (comparison_operator_is_like(node->operator_kind) &&
        !column_descriptor_is_string_family(&node->column)) {
        set_unsupported_error(database, "WHERE LIKE predicates support only string columns");
        return MYLITE_ERROR;
    }
    if (comparison_operator_is_regexp(node->operator_kind) &&
        !column_descriptor_is_string_family(&node->column)) {
        set_unsupported_error(database, "WHERE REGEXP predicates support only string columns");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_string_family(&node->column) &&
        !comparison_operator_is_string_predicate(node->operator_kind)) {
        set_unsupported_error(
            database,
            "WHERE string predicates support only =, <=>, <>, !=, <, <=, >, >=, LIKE, REGEXP, "
            "and RLIKE"
        );
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_enum(&node->column) &&
        !comparison_operator_is_enum_predicate(node->operator_kind)) {
        set_unsupported_error(database, "WHERE enum predicates support only =, <=>, <>, and !=");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_set(&node->column) &&
        !comparison_operator_is_enum_predicate(node->operator_kind)) {
        set_unsupported_error(database, "WHERE set predicates support only =, <=>, <>, and !=");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int plan_exists_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_EXISTS,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    struct planned_exists_subquery *subquery = NULL;
    int rc = MYLITE_OK;

    if (options == NULL || !options->allow_exists) {
        set_unsupported_error(database, "EXISTS subqueries are supported only in SELECT WHERE");
        return MYLITE_ERROR;
    }
    if (source_context == NULL || select_source_context_is_joined(source_context)) {
        set_unsupported_error(database, "EXISTS subqueries support only one outer table source");
        return MYLITE_ERROR;
    }

    subquery = calloc(1U, sizeof(*subquery));
    if (subquery == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_exists_subquery(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        subquery
    );
    if (rc != MYLITE_OK) {
        planned_exists_subquery_deinit(subquery);
        free(subquery);
        return rc;
    }

    node.exists_subquery = subquery;
    rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    if (rc != MYLITE_OK) {
        planned_exists_subquery_deinit(subquery);
        free(subquery);
        return rc;
    }

    predicate->qualify_column_references = true;
    return MYLITE_OK;
}

static int plan_exists_subquery(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_exists_subquery *out_subquery
) {
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *from_clause = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    int rc = MYLITE_OK;

    *out_subquery = (struct planned_exists_subquery){.has_table_source = false};
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "EXISTS supports only SELECT subqueries");
        return MYLITE_ERROR;
    }
    select_list = child_at(statement, 0U);
    from_clause = child_at(statement, 1U);
    optional_clause = child_at(statement, 2U);
    if (mylite_sql_ast_node_select_modifier(statement) != MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT ||
        mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        set_unsupported_error(database, "EXISTS subqueries do not support SELECT modifiers");
        return MYLITE_ERROR;
    }
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            limit_clause = optional_clause;
        } else {
            set_unsupported_error(database, "EXISTS subqueries support only WHERE and LIMIT");
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }
    if (from_clause != NULL && from_clause->kind != MYLITE_SQL_AST_FROM_DUAL) {
        rc = plan_exists_table_source(database, from_clause, out_subquery);
    }
    if (rc == MYLITE_OK) {
        rc = validate_exists_select_list(database, select_list, out_subquery);
    }
    if (rc == MYLITE_OK && where_clause != NULL && !out_subquery->has_table_source) {
        set_unsupported_error(database, "EXISTS tableless subqueries do not support WHERE");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && out_subquery->has_table_source) {
        rc = plan_exists_inner_predicate(
            database,
            where_clause,
            outer_source_context,
            outer_columns,
            outer_column_count,
            out_subquery
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_delete_limit(database, limit_clause, &out_subquery->limit);
    }

    return rc;
}

static int plan_exists_table_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_exists_subquery *out_subquery
) {
    int rc = MYLITE_OK;

    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(database, "EXISTS subqueries support only one table source");
        return MYLITE_ERROR;
    }

    rc = plan_joined_select_source(database, from_clause, &out_subquery->source);
    if (rc == MYLITE_OK) {
        out_subquery->has_table_source = true;
    }
    return rc;
}

static int validate_exists_select_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct planned_exists_subquery *subquery
) {
    struct select_source_context source_context = {0};

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (subquery->has_table_source) {
        source_context = (struct select_source_context){
            .source = &subquery->source.source,
            .has_alias = subquery->source.has_alias,
        };
        memcpy(source_context.alias, subquery->source.alias, sizeof(source_context.alias));
    }

    for (const struct mylite_sql_ast_node *item = child_at(select_list, 0U); item != NULL;
         item = item->next_sibling) {
        const struct mylite_sql_ast_node *expression = child_at(item, 0U);
        struct mylite_catalog_column_descriptor column = {0};

        if (expression == NULL) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        if (expression->kind == MYLITE_SQL_AST_WILDCARD ||
            exists_select_item_is_supported_literal(expression)) {
            continue;
        }
        if (!subquery->has_table_source) {
            set_unsupported_error(database, "EXISTS tableless subqueries support only literals");
            return MYLITE_ERROR;
        }
        if (expression->kind != MYLITE_SQL_AST_IDENTIFIER &&
            expression->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
            set_unsupported_error(
                database,
                "EXISTS subqueries support only literal or descriptor-column select items"
            );
            return MYLITE_ERROR;
        }
        int rc = resolve_descriptor_column_reference(
            database,
            expression,
            &source_context,
            COLUMN_REFERENCE_FIELD,
            "EXISTS subqueries support only literal or descriptor-column select items",
            subquery->source.columns,
            subquery->source.column_count,
            &column
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static bool exists_select_item_is_supported_literal(const struct mylite_sql_ast_node *expression) {
    enum mylite_sql_ast_literal_kind kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }
    kind = mylite_sql_ast_node_literal_kind(expression);
    return (kind == MYLITE_SQL_AST_LITERAL_INTEGER || kind == MYLITE_SQL_AST_LITERAL_TRUE ||
            kind == MYLITE_SQL_AST_LITERAL_FALSE || kind == MYLITE_SQL_AST_LITERAL_NULL ||
            kind == MYLITE_SQL_AST_LITERAL_STRING) != 0;
}

static int plan_exists_inner_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_exists_subquery *subquery
) {
    struct select_source_context inner_source_context = {
        .source = &subquery->source.source,
        .has_alias = subquery->source.has_alias,
    };
    const struct select_predicate_plan_options options = {
        .allow_column_reference_rhs = true,
        .outer_source_context = outer_source_context,
        .outer_columns = outer_columns,
        .outer_column_count = outer_column_count,
    };

    memcpy(inner_source_context.alias, subquery->source.alias, sizeof(inner_source_context.alias));
    int rc = MYLITE_OK;

    subquery->predicate = (struct planned_select_predicate){0};
    subquery->predicate.qualify_column_references =
        select_source_context_is_joined(&inner_source_context);
    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    if (where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
        return MYLITE_ERROR;
    }

    rc = plan_select_predicate_node_without_exists(
        database,
        child_at(where_clause, 0U),
        &inner_source_context,
        subquery->source.columns,
        subquery->source.column_count,
        &options,
        &subquery->predicate
    );
    if (rc != MYLITE_OK) {
        planned_select_predicate_deinit_without_exists(&subquery->predicate);
    }

    if (rc == MYLITE_OK && subquery->predicate.qualify_column_references) {
        offset_exists_inner_predicate_source_indexes(&subquery->predicate, 1U);
    }
    return rc;
}

static void offset_exists_inner_predicate_source_indexes(
    struct planned_select_predicate *predicate,
    size_t source_index_offset
) {
    if (predicate == NULL || source_index_offset == 0U) {
        return;
    }

    for (size_t node_index = 0U; node_index < predicate->node_count; ++node_index) {
        struct planned_select_predicate_node *node = &predicate->nodes[node_index];

        if (node->kind == PLANNED_SELECT_PREDICATE_COMPARISON && node->value_is_column_reference) {
            continue;
        }
        if (node->kind == PLANNED_SELECT_PREDICATE_COMPARISON ||
            node->kind == PLANNED_SELECT_PREDICATE_IS_NULL ||
            node->kind == PLANNED_SELECT_PREDICATE_IS_BOOLEAN ||
            node->kind == PLANNED_SELECT_PREDICATE_BETWEEN ||
            node->kind == PLANNED_SELECT_PREDICATE_IN) {
            node->column_source_index += source_index_offset;
        }
    }
}

static int plan_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
        .value_is_column_reference = true,
    };
    int rc = MYLITE_OK;

    if (node.operator_kind != MYLITE_SQL_AST_OPERATOR_EQUAL &&
        node.operator_kind != MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        set_unsupported_error(database, "EXISTS correlated predicates support only = and <=>");
        return MYLITE_ERROR;
    }

    rc = resolve_exists_column_reference(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        options->outer_source_context,
        options->outer_columns,
        options->outer_column_count,
        &node.column,
        &node.column_source_index
    );
    if (rc == MYLITE_OK) {
        rc = resolve_exists_column_reference(
            database,
            child_at(predicate_node, 1U),
            source_context,
            table_columns,
            table_column_count,
            options->outer_source_context,
            options->outer_columns,
            options->outer_column_count,
            &node.value_column,
            &node.value_column_source_index
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!exists_correlated_column_comparison_is_supported(&node)) {
        set_unsupported_error(
            database,
            "EXISTS correlated predicates support one inner and one outer integer column"
        );
        return MYLITE_ERROR;
    }

    predicate->qualify_column_references = true;
    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static int resolve_exists_column_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *inner_source_context,
    const struct mylite_catalog_column_descriptor *inner_columns,
    size_t inner_column_count,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char column_name[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t part_count = 0U;
    bool resolved = false;
    int rc = MYLITE_OK;

    *out_column = (struct mylite_catalog_column_descriptor){0};
    *out_source_index = 0U;
    if (column_node == NULL || (column_node->kind != MYLITE_SQL_AST_IDENTIFIER &&
                                column_node->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        set_unsupported_error(database, "EXISTS correlated predicates require descriptor columns");
        return MYLITE_ERROR;
    }

    rc = collect_column_reference_parts(database, column_node, parts, &part_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc =
        format_column_reference_name(database, parts, part_count, column_name, sizeof(column_name));
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = resolve_exists_column_reference_in_source(
        database,
        parts,
        part_count,
        column_name,
        inner_source_context,
        inner_columns,
        inner_column_count,
        out_column,
        &resolved
    );
    if (rc != MYLITE_OK || resolved) {
        *out_source_index = 1U;
        return rc;
    }

    rc = resolve_exists_column_reference_in_source(
        database,
        parts,
        part_count,
        column_name,
        outer_source_context,
        outer_columns,
        outer_column_count,
        out_column,
        &resolved
    );
    if (rc != MYLITE_OK || resolved) {
        *out_source_index = 0U;
        return rc;
    }

    set_unknown_column_reference_error(database, COLUMN_REFERENCE_WHERE, column_name);
    return MYLITE_ERROR;
}

static int resolve_exists_column_reference_in_source(
    struct mylite_db *database,
    const char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    const char *column_name,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct mylite_catalog_column_descriptor *out_column,
    bool *out_resolved
) {
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    *out_resolved = false;
    if (!column_reference_qualifier_matches_source(parts, part_count, source_context)) {
        return MYLITE_OK;
    }

    rc = find_column_index(columns, column_count, parts[part_count - 1U], &column_index);
    if (rc != MYLITE_OK) {
        if (part_count > 1U) {
            set_unknown_column_reference_error(database, COLUMN_REFERENCE_WHERE, column_name);
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    *out_column = columns[column_index];
    *out_resolved = true;
    return MYLITE_OK;
}

static bool exists_correlated_column_comparison_is_supported(
    const struct planned_select_predicate_node *node
) {
    bool left_is_outer = node->column_source_index == 0U;
    bool right_is_outer = node->value_column_source_index == 0U;

    if (left_is_outer == right_is_outer) {
        return false;
    }
    if (!join_condition_column_is_integer_family(&node->column)) {
        return false;
    }
    return join_condition_column_is_integer_family(&node->value_column);
}

static bool comparison_operator_is_string_predicate(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_RLIKE:
        return true;
    default:
        return false;
    }
}

static bool comparison_operator_is_enum_predicate(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return true;
    default:
        return false;
    }
}

static bool comparison_operator_is_like(enum mylite_sql_ast_operator operator_kind) {
    return operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE;
}

static bool comparison_operator_is_regexp(enum mylite_sql_ast_operator operator_kind) {
    return (operator_kind == MYLITE_SQL_AST_OPERATOR_REGEXP ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_RLIKE) != 0;
}

static int plan_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_IS_NULL,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };

    if (predicate_node_is_find_in_set_expression(child_at(predicate_node, 0U))) {
        return plan_find_in_set_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_regexp_like_expression(child_at(predicate_node, 0U))) {
        return plan_regexp_like_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_json_valid_expression(child_at(predicate_node, 0U))) {
        return plan_json_valid_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_json_contains_expression(child_at(predicate_node, 0U))) {
        return plan_json_contains_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_string_length_expression(child_at(predicate_node, 0U))) {
        return plan_string_length_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_substring_expression(child_at(predicate_node, 0U))) {
        return plan_substring_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_temporal_extract_expression(child_at(predicate_node, 0U))) {
        return plan_temporal_extract_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_scalar_literal_expression(child_at(predicate_node, 0U))) {
        return plan_scalar_literal_is_predicate(
            database,
            predicate_node,
            predicate,
            out_node_index
        );
    }

    int rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static int plan_is_boolean_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_IS_BOOLEAN,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    if (predicate_node_is_scalar_literal_expression(child_at(predicate_node, 0U))) {
        return plan_scalar_literal_is_predicate(
            database,
            predicate_node,
            predicate,
            out_node_index
        );
    }

    int rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (rc == MYLITE_OK) {
        struct integer_column_range range = {0};

        rc = integer_range_for_column(
            database,
            &node.column,
            "WHERE supports only integer boolean predicate columns",
            &range
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static int plan_between_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_BETWEEN,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );

    if (rc == MYLITE_OK) {
        if (column_descriptor_is_enum(&node.column)) {
            set_unsupported_error(database, "WHERE enum predicates do not yet support BETWEEN");
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_set(&node.column)) {
            set_unsupported_error(database, "WHERE set predicates do not yet support BETWEEN");
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_date(&node.column)) {
            rc = convert_predicate_date_between_values(
                database,
                child_at(predicate_node, 1U),
                child_at(predicate_node, 2U),
                &node.column,
                &node.value,
                &node.upper_value,
                &node.compare_date_as_datetime
            );
        } else {
            rc = convert_predicate_value(
                database,
                child_at(predicate_node, 1U),
                &node.column,
                &node.value
            );
            if (rc == MYLITE_OK) {
                rc = convert_predicate_value(
                    database,
                    child_at(predicate_node, 2U),
                    &node.column,
                    &node.upper_value
                );
            }
        }
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_value_deinit(&node.upper_value);
        return rc;
    }

    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static int plan_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    const struct mylite_sql_ast_node *right = NULL;
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_IN,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = MYLITE_OK;

    if (predicate_node == NULL || predicate_node->kind != MYLITE_SQL_AST_IN_PREDICATE) {
        set_unsupported_error(database, "WHERE supports only descriptor column IN predicates");
        return MYLITE_ERROR;
    }
    right = child_at(predicate_node, 1U);
    if (right == NULL || right->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return plan_in_literal_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }

    rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );
    if (rc == MYLITE_OK) {
        if (column_descriptor_is_enum(&node.column)) {
            set_unsupported_error(database, "WHERE enum predicates do not yet support IN");
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_set(&node.column)) {
            set_unsupported_error(database, "WHERE set predicates do not yet support IN");
            return MYLITE_ERROR;
        }
        if (options == NULL || !options->allow_in_subquery) {
            set_unsupported_error(database, "IN subqueries are supported only in SELECT WHERE");
            return MYLITE_ERROR;
        }
        if (source_context == NULL || select_source_context_is_joined(source_context)) {
            set_unsupported_error(database, "IN subqueries support only one outer table source");
            return MYLITE_ERROR;
        }
        rc = plan_in_subquery_predicate(
            database,
            right,
            source_context,
            table_columns,
            table_column_count,
            &node
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_in_subquery_deinit(node.in_subquery);
        free(node.in_subquery);
        return rc;
    }
    predicate->qualify_column_references = true;

    return rc;
}

static int plan_in_literal_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_IN,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = MYLITE_OK;

    if (predicate_node == NULL || predicate_node->kind != MYLITE_SQL_AST_IN_PREDICATE) {
        set_unsupported_error(database, "WHERE supports only descriptor column IN predicates");
        return MYLITE_ERROR;
    }
    if (child_at(predicate_node, 1U) != NULL &&
        child_at(predicate_node, 1U)->kind == MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "IN subqueries are supported only in SELECT WHERE");
        return MYLITE_ERROR;
    }

    rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );
    if (rc == MYLITE_OK) {
        if (column_descriptor_is_enum(&node.column)) {
            set_unsupported_error(database, "WHERE enum predicates do not yet support IN");
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_set(&node.column)) {
            set_unsupported_error(database, "WHERE set predicates do not yet support IN");
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_date(&node.column)) {
            rc = convert_predicate_date_in_value_list(
                database,
                child_at(predicate_node, 1U),
                &node.column,
                &node.values,
                &node.value_count,
                &node.compare_date_as_datetime
            );
        } else {
            rc = convert_predicate_in_value_list(
                database,
                child_at(predicate_node, 1U),
                &node.column,
                &node.values,
                &node.value_count
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        for (size_t value_index = 0U; value_index < node.value_count; ++value_index) {
            planned_value_deinit(&node.values[value_index]);
        }
        free(node.values);
        return rc;
    }

    return rc;
}

static int plan_in_subquery_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_statement,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_select_predicate_node *node
) {
    struct planned_in_subquery *subquery = NULL;
    int rc = MYLITE_OK;

    subquery = calloc(1U, sizeof(*subquery));
    if (subquery == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_in_subquery(
        database,
        select_statement,
        outer_source_context,
        outer_columns,
        outer_column_count,
        subquery
    );
    if (rc == MYLITE_OK && !in_subquery_columns_are_compatible(&node->column, &subquery->column)) {
        set_unsupported_error(
            database,
            "IN subqueries support same-family integer or string descriptor columns"
        );
        rc = MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        planned_in_subquery_deinit(subquery);
        free(subquery);
        return rc;
    }

    node->in_subquery = subquery;
    return MYLITE_OK;
}

static int plan_in_subquery(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_in_subquery *out_subquery
) {
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *from_clause = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    int rc = MYLITE_OK;

    *out_subquery = (struct planned_in_subquery){0};
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "IN subqueries support only SELECT subqueries");
        return MYLITE_ERROR;
    }
    select_list = child_at(statement, 0U);
    from_clause = child_at(statement, 1U);
    optional_clause = child_at(statement, 2U);
    if (mylite_sql_ast_node_select_modifier(statement) != MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT ||
        mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        set_unsupported_error(database, "IN subqueries do not support SELECT modifiers");
        return MYLITE_ERROR;
    }
    for (const struct mylite_sql_ast_node *clause = optional_clause; clause != NULL;
         clause = clause->next_sibling) {
        if (clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            set_in_subquery_limit_error(database);
            return MYLITE_ERROR;
        }
    }
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else {
            set_unsupported_error(database, "IN subqueries support only WHERE");
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }
    rc = plan_in_subquery_table_source(database, from_clause, out_subquery);
    if (rc == MYLITE_OK) {
        rc = plan_in_subquery_select_column(
            database,
            select_list,
            out_subquery,
            &out_subquery->column
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_in_subquery_inner_predicate(
            database,
            where_clause,
            outer_source_context,
            outer_columns,
            outer_column_count,
            out_subquery
        );
    }

    return rc;
}

static int plan_in_subquery_table_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_in_subquery *out_subquery
) {
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(database, "IN subqueries support one descriptor table source");
        return MYLITE_ERROR;
    }

    return plan_joined_select_source(database, from_clause, &out_subquery->source);
}

static int plan_in_subquery_select_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct planned_in_subquery *subquery,
    struct mylite_catalog_column_descriptor *out_column
) {
    const struct mylite_sql_ast_node *item = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    struct select_source_context source_context = {0};

    *out_column = (struct mylite_catalog_column_descriptor){0};
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(select_list) != 1U) {
        set_scalar_subquery_column_count_error(database);
        return MYLITE_ERROR;
    }
    item = child_at(select_list, 0U);
    if (item == NULL || item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    expression = child_at(item, 0U);
    if (expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (expression->kind != MYLITE_SQL_AST_IDENTIFIER &&
        expression->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        set_unsupported_error(
            database,
            "IN subqueries support one explicit descriptor select column"
        );
        return MYLITE_ERROR;
    }

    source_context = (struct select_source_context){
        .source = &subquery->source.source,
        .has_alias = subquery->source.has_alias,
    };
    memcpy(source_context.alias, subquery->source.alias, sizeof(source_context.alias));
    return resolve_descriptor_column_reference(
        database,
        expression,
        &source_context,
        COLUMN_REFERENCE_FIELD,
        "IN subqueries support one explicit descriptor select column",
        subquery->source.columns,
        subquery->source.column_count,
        out_column
    );
}

static bool in_subquery_columns_are_compatible(
    const struct mylite_catalog_column_descriptor *outer_column,
    const struct mylite_catalog_column_descriptor *inner_column
) {
    return join_condition_columns_are_compatible(outer_column, inner_column);
}

static int plan_in_subquery_inner_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_in_subquery *subquery
) {
    struct select_source_context inner_source_context = {
        .source = &subquery->source.source,
        .has_alias = subquery->source.has_alias,
    };
    const struct select_predicate_plan_options options = {
        .allow_column_reference_rhs = true,
        .outer_source_context = outer_source_context,
        .outer_columns = outer_columns,
        .outer_column_count = outer_column_count,
    };
    int rc = MYLITE_OK;

    memcpy(inner_source_context.alias, subquery->source.alias, sizeof(inner_source_context.alias));
    subquery->predicate = (struct planned_select_predicate){0};
    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    if (where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
        return MYLITE_ERROR;
    }

    rc = plan_select_predicate_node_without_exists(
        database,
        child_at(where_clause, 0U),
        &inner_source_context,
        subquery->source.columns,
        subquery->source.column_count,
        &options,
        &subquery->predicate
    );
    if (rc != MYLITE_OK) {
        planned_select_predicate_deinit_without_exists(&subquery->predicate);
    }
    if (rc == MYLITE_OK && subquery->predicate.qualify_column_references) {
        offset_exists_inner_predicate_source_indexes(&subquery->predicate, 1U);
    }
    return rc;
}

static int convert_predicate_in_value_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value **out_values,
    size_t *out_value_count
) {
    struct planned_value *values = NULL;
    size_t value_count = mylite_sql_ast_node_child_count(value_list);
    int rc = MYLITE_OK;

    *out_values = NULL;
    *out_value_count = 0U;
    if (value_list == NULL || value_list->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST ||
        value_count == 0U) {
        set_unsupported_error(database, "WHERE supports only nonempty IN predicate lists");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_year(column)) {
        return convert_predicate_year_in_value_list(
            database,
            value_list,
            column,
            out_values,
            out_value_count
        );
    }
    if (value_count > SIZE_MAX / sizeof(*values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    values = calloc(value_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t value_index = 0U; rc == MYLITE_OK && value_index < value_count; ++value_index) {
        rc = convert_predicate_in_value(
            database,
            child_at(value_list, value_index),
            column,
            &values[value_index]
        );
    }
    if (rc != MYLITE_OK) {
        for (size_t cleanup_index = 0U; cleanup_index < value_count; ++cleanup_index) {
            planned_value_deinit(&values[cleanup_index]);
        }
        free(values);
        return rc;
    }

    *out_values = values;
    *out_value_count = value_count;
    return MYLITE_OK;
}

static int convert_predicate_in_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_value = (struct planned_value){.is_null = true, .integer = 0};
        return MYLITE_OK;
    }

    return convert_predicate_value(database, value_node, column, out_value);
}

static int convert_predicate_year_in_value_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value **out_values,
    size_t *out_value_count
) {
    struct planned_value *values = NULL;
    size_t value_count = mylite_sql_ast_node_child_count(value_list);
    bool uses_string_conversion =
        year_in_value_list_uses_string_conversion(value_list, value_count);
    int rc = MYLITE_OK;

    if (value_count > SIZE_MAX / sizeof(*values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = calloc(value_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t value_index = 0U; rc == MYLITE_OK && value_index < value_count; ++value_index) {
        const struct mylite_sql_ast_node *value_node = child_at(value_list, value_index);

        if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
            values[value_index] = (struct planned_value){.is_null = true, .integer = 0};
        } else if (uses_string_conversion) {
            rc = convert_predicate_year_literal(database, value_node, column, &values[value_index]);
        } else {
            rc = convert_year_direct_value(database, value_node, &values[value_index]);
        }
    }
    if (rc != MYLITE_OK) {
        for (size_t cleanup_index = 0U; cleanup_index < value_count; ++cleanup_index) {
            planned_value_deinit(&values[cleanup_index]);
        }
        free(values);
        return rc;
    }

    *out_values = values;
    *out_value_count = value_count;
    return MYLITE_OK;
}

static bool year_in_value_list_uses_string_conversion(
    const struct mylite_sql_ast_node *value_list,
    size_t value_count
) {
    if (value_count == 1U) {
        return true;
    }
    for (size_t value_index = 0U; value_index < value_count; ++value_index) {
        const struct mylite_sql_ast_node *value_node =
            unwrap_parenthesized_expression(child_at(value_list, value_index));

        if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
            continue;
        }
        if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
            return false;
        }
    }
    return true;
}

static int append_planned_select_predicate_node(
    struct mylite_db *database,
    struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *out_node_index
) {
    struct planned_select_predicate_node *nodes = NULL;
    size_t required_count = predicate->node_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*nodes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    nodes = realloc(predicate->nodes, required_count * sizeof(*nodes));
    if (nodes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    predicate->nodes = nodes;
    predicate->nodes[predicate->node_count] = *node;
    if (out_node_index != NULL) {
        *out_node_index = predicate->node_count;
    }
    predicate->node_count = required_count;

    return MYLITE_OK;
}

static int append_deprecated_logical_and_warning(struct mylite_db *database) {
    int rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_deprecated_logical_and,
        "HY000",
        "'&&' is deprecated and will be removed in a future release. Please use AND instead"
    );

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int append_deprecated_logical_or_warning(struct mylite_db *database) {
    int rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_deprecated_logical_or,
        "HY000",
        "'|| as a synonym for OR' is deprecated and will be removed in a future release. Please "
        "use OR instead"
    );

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static bool planned_select_predicate_has_expression(
    const struct planned_select_predicate *predicate
) {
    if (predicate == NULL) {
        return false;
    }

    return predicate->has_root;
}

static int append_predicate_work_node(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count,
    const struct mylite_sql_ast_node *node
) {
    return append_predicate_work_item(
        database,
        items,
        item_count,
        (struct predicate_work_item){.kind = PREDICATE_WORK_NODE, .node = node}
    );
}

static int append_predicate_work_deprecated_and_warning(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count
) {
    return append_predicate_work_item(
        database,
        items,
        item_count,
        (struct predicate_work_item){.kind = PREDICATE_WORK_DEPRECATED_AND_WARNING}
    );
}

static int append_predicate_work_deprecated_or_warning(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count
) {
    return append_predicate_work_item(
        database,
        items,
        item_count,
        (struct predicate_work_item){.kind = PREDICATE_WORK_DEPRECATED_OR_WARNING}
    );
}

static int append_predicate_work_finish_logical(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count,
    enum mylite_sql_ast_operator operator_kind
) {
    return append_predicate_work_item(
        database,
        items,
        item_count,
        (struct predicate_work_item){
            .kind = PREDICATE_WORK_FINISH_LOGICAL,
            .operator_kind = operator_kind,
        }
    );
}

static int append_predicate_work_finish_not(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count
) {
    return append_predicate_work_item(
        database,
        items,
        item_count,
        (struct predicate_work_item){.kind = PREDICATE_WORK_FINISH_NOT}
    );
}

static int append_predicate_work_item(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count,
    struct predicate_work_item item
) {
    struct predicate_work_item *grown_items = NULL;
    size_t required_count = *item_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*grown_items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    grown_items = realloc(*items, required_count * sizeof(*grown_items));
    if (grown_items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    *items = grown_items;
    (*items)[*item_count] = item;
    *item_count = required_count;

    return MYLITE_OK;
}

static int append_predicate_result_index(
    struct mylite_db *database,
    size_t **indexes,
    size_t *index_count,
    size_t index
) {
    size_t *grown_indexes = NULL;
    size_t required_count = *index_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*grown_indexes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    grown_indexes = realloc(*indexes, required_count * sizeof(*grown_indexes));
    if (grown_indexes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    *indexes = grown_indexes;
    (*indexes)[*index_count] = index;
    *index_count = required_count;

    return MYLITE_OK;
}

static int pop_predicate_result_index(
    const size_t *indexes,
    size_t *index_count,
    size_t *out_index
) {
    if (indexes == NULL || index_count == NULL || *index_count == 0U || out_index == NULL) {
        return MYLITE_ERROR;
    }

    --(*index_count);
    *out_index = indexes[*index_count];
    return MYLITE_OK;
}

static int resolve_predicate_column_with_source_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
) {
    return resolve_descriptor_column_reference_with_source_index(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_WHERE,
        "WHERE supports only unqualified predicate columns",
        table_columns,
        table_column_count,
        out_column,
        out_source_index
    );
}

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
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            set_unsupported_error(
                database,
                "WHERE supports only integer or boolean predicate literals"
            );
            return MYLITE_ERROR;
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

static int convert_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    if (column_descriptor_is_string_family(column)) {
        return convert_predicate_string_literal(database, value_node, out_value);
    }
    if (column_descriptor_is_enum(column)) {
        return convert_predicate_enum_literal(database, value_node, column, out_value);
    }
    if (column_descriptor_is_set(column)) {
        return convert_predicate_set_literal(database, value_node, column, out_value);
    }
    if (column_descriptor_is_bit(column)) {
        return convert_bit_literal(database, value_node, column, 1U, false, out_value);
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
    *out_uses_datetime = false;
    for (size_t value_index = 0U; value_index < value_count; ++value_index) {
        const struct mylite_sql_ast_node *value_node = child_at(value_list, value_index);
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
    for (size_t value_index = 0U; value_index < value_count; ++value_index) {
        const struct mylite_sql_ast_node *value_node = child_at(value_list, value_index);

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

    rc = decode_sql_string_literal(
        database,
        value_node,
        "WHERE string predicates support only string literals",
        "WHERE string predicate literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_value_is_supported_string_key(text, text_length)) {
        free(text);
        set_unsupported_error(database, "WHERE string predicate literals support only ASCII text");
        return MYLITE_ERROR;
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

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "WHERE LIKE predicates support only string pattern literals"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "WHERE LIKE predicates support only string pattern literals",
        "WHERE LIKE pattern literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_value_is_supported_string_key(text, text_length)) {
        free(text);
        set_unsupported_error(database, "WHERE LIKE pattern literals support only ASCII text");
        return MYLITE_ERROR;
    }

    return assign_text_value(database, text, text_length, out_value);
}

static int convert_predicate_regexp_pattern_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
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
        mylite_regexp_compile_ascii_ci(text, text_length, &program);
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

static int convert_predicate_date_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    bool force_datetime_comparison,
    struct planned_value *out_value,
    bool *out_compare_as_datetime
) {
    char *text = NULL;
    size_t text_length = 0U;
    char normalized[datetime_text_length + 1U];
    bool append_warning = false;
    int rc = MYLITE_OK;

    if (out_compare_as_datetime != NULL) {
        *out_compare_as_datetime = false;
    }
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "WHERE DATE predicates support only string literals");
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "WHERE DATE predicates support only string literals",
        "WHERE DATE predicate literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (predicate_date_text_admitted(database, text, text_length)) {
        if (force_datetime_comparison) {
            rc = copy_date_midnight_predicate_value(database, text, out_value);
            free(text);
            return rc;
        }
        return assign_text_value(database, text, text_length, out_value);
    }

    if (normalize_date_datetime_predicate_text(text, text_length, normalized, &append_warning)) {
        if (!predicate_datetime_text_admitted(database, normalized, datetime_text_length)) {
            set_incorrect_date_literal_error(database, text);
            free(text);
            return MYLITE_ERROR;
        }
        if (append_warning) {
            rc = append_incorrect_date_predicate_warning(database, text, column->name);
            if (rc != MYLITE_OK) {
                free(text);
                return rc;
            }
        }
        if (out_compare_as_datetime != NULL) {
            *out_compare_as_datetime = true;
        }
        free(text);
        return copy_text_value(database, normalized, out_value);
    }

    set_incorrect_date_literal_error(database, text);
    free(text);
    return MYLITE_ERROR;
}

static bool normalize_date_datetime_predicate_text(
    const char *text,
    size_t text_length,
    char *out_text,
    bool *out_append_warning
) {
    int ignored_offset_minutes = 0;

    if (text == NULL || out_text == NULL || out_append_warning == NULL) {
        return false;
    }
    *out_append_warning = false;
    if (text_length == datetime_text_length && (text[datetime_date_time_separator_offset] == ' ' ||
                                                text[datetime_date_time_separator_offset] == 'T')) {
        memcpy(out_text, text, datetime_text_length);
        out_text[datetime_date_time_separator_offset] = ' ';
        out_text[datetime_text_length] = '\0';
        return datetime_text_has_canonical_shape(out_text, datetime_text_length);
    }
    if (text_length == datetime_text_length + temporal_predicate_offset_text_length &&
        (text[datetime_date_time_separator_offset] == ' ' ||
         text[datetime_date_time_separator_offset] == 'T')) {
        memcpy(out_text, text, datetime_text_length);
        out_text[datetime_date_time_separator_offset] = ' ';
        out_text[datetime_text_length] = '\0';
        if (!datetime_text_has_canonical_shape(out_text, datetime_text_length)) {
            return false;
        }
        return parse_temporal_predicate_offset(
            text + datetime_text_length,
            &ignored_offset_minutes
        );
    }
    if (text_length == datetime_text_length + 1U &&
        (text[datetime_date_time_separator_offset] == ' ' ||
         text[datetime_date_time_separator_offset] == 'T') &&
        (text[datetime_text_length] == 'Z' || text[datetime_text_length] == 'z')) {
        memcpy(out_text, text, datetime_text_length);
        out_text[datetime_date_time_separator_offset] = ' ';
        out_text[datetime_text_length] = '\0';
        *out_append_warning = true;
        return datetime_text_has_canonical_shape(out_text, datetime_text_length);
    }

    return false;
}

static int copy_date_midnight_predicate_value(
    struct mylite_db *database,
    const char *date_text,
    struct planned_value *out_value
) {
    char text[datetime_text_length + 1U];

    if (date_text == NULL) {
        return MYLITE_MISUSE;
    }
    memcpy(text, date_text, date_text_length);
    memcpy(text + date_text_length, " 00:00:00", datetime_text_length - date_text_length);
    text[datetime_text_length] = '\0';
    return copy_text_value(database, text, out_value);
}

static int convert_predicate_time_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int64_t seconds = 0;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "WHERE TIME predicates support only string literals");
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "WHERE TIME predicates support only string literals",
        "WHERE TIME predicate literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!time_text_to_seconds(text, text_length, &seconds)) {
        set_incorrect_time_value_error(database, text, column->name, 1U);
        free(text);
        return MYLITE_ERROR;
    }
    free(text);

    *out_value = (struct planned_value){.is_null = false, .is_text = false, .integer = seconds};
    return MYLITE_OK;
}

static int convert_predicate_datetime_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    char normalized[datetime_text_length + 1U];
    struct temporal_predicate_normalization_input normalization = {0};
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "WHERE DATETIME predicates support only string literals");
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "WHERE DATETIME predicates support only string literals",
        "WHERE DATETIME predicate literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    normalization.text = text;
    normalization.text_length = text_length;
    normalization.target_offset_minutes =
        database == NULL ? 0 : database->session.time_zone_offset_minutes;
    if (normalize_iso_temporal_predicate_text(&normalization, normalized)) {
        if (!predicate_datetime_text_admitted(database, normalized, datetime_text_length)) {
            set_incorrect_datetime_literal_error(database, text);
            free(text);
            return MYLITE_ERROR;
        }
        free(text);
        return copy_text_value(database, normalized, out_value);
    }
    if (normalize_z_temporal_predicate_text(text, text_length, normalized)) {
        if (!predicate_datetime_text_admitted(database, normalized, datetime_text_length)) {
            set_incorrect_datetime_literal_error(database, text);
            free(text);
            return MYLITE_ERROR;
        }
        rc = append_incorrect_datetime_predicate_warning(database, text, column->name);
        if (rc != MYLITE_OK) {
            free(text);
            return rc;
        }
        free(text);
        return copy_text_value(database, normalized, out_value);
    }
    if (!predicate_datetime_text_admitted(database, text, text_length)) {
        set_incorrect_datetime_literal_error(database, text);
        free(text);
        return MYLITE_ERROR;
    }

    return assign_text_value(database, text, text_length, out_value);
}

static int convert_predicate_timestamp_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    char normalized[datetime_text_length + 1U];
    struct temporal_predicate_normalization_input normalization = {0};
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "WHERE TIMESTAMP predicates support only string literals");
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "WHERE TIMESTAMP predicates support only string literals",
        "WHERE TIMESTAMP predicate literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    normalization.text = text;
    normalization.text_length = text_length;
    normalization.target_offset_minutes = 0;
    if (normalize_iso_temporal_predicate_text(&normalization, normalized)) {
        if (!predicate_timestamp_text_admitted(database, normalized, datetime_text_length)) {
            set_incorrect_timestamp_value_error(database, text);
            free(text);
            return MYLITE_ERROR;
        }
        free(text);
        return copy_text_value(database, normalized, out_value);
    }
    if (normalize_z_temporal_predicate_text(text, text_length, normalized)) {
        if (!predicate_timestamp_text_admitted(database, normalized, datetime_text_length)) {
            set_incorrect_timestamp_value_error(database, text);
            free(text);
            return MYLITE_ERROR;
        }
        rc = append_incorrect_datetime_predicate_warning(database, text, column->name);
        if (rc != MYLITE_OK) {
            free(text);
            return rc;
        }
        free(text);
        return copy_text_value(database, normalized, out_value);
    }
    if (!predicate_timestamp_text_admitted(database, text, text_length)) {
        set_incorrect_timestamp_value_error(database, text);
        free(text);
        return MYLITE_ERROR;
    }

    return assign_text_value(database, text, text_length, out_value);
}

static bool normalize_iso_temporal_predicate_text(
    const struct temporal_predicate_normalization_input *input,
    char *out_text
) {
    struct date_add_datetime_parts parts = {0};
    int source_offset_minutes = 0;

    if (input == NULL || input->text == NULL || out_text == NULL) {
        return false;
    }
    if (input->text_length == datetime_text_length &&
        input->text[datetime_date_time_separator_offset] == 'T') {
        memcpy(out_text, input->text, datetime_text_length);
        out_text[datetime_date_time_separator_offset] = ' ';
        out_text[datetime_text_length] = '\0';
        return datetime_text_has_canonical_shape(out_text, datetime_text_length);
    }
    if (input->text_length != datetime_text_length + sizeof("+00:00") - 1U ||
        (input->text[datetime_date_time_separator_offset] != ' ' &&
         input->text[datetime_date_time_separator_offset] != 'T')) {
        return false;
    }

    memcpy(out_text, input->text, datetime_text_length);
    out_text[datetime_date_time_separator_offset] = ' ';
    out_text[datetime_text_length] = '\0';
    if (!date_add_parse_datetime_text(out_text, datetime_text_length, &parts)) {
        return false;
    }
    if (!parse_temporal_predicate_offset(
            input->text + datetime_text_length,
            &source_offset_minutes
        )) {
        return false;
    }

    return shift_datetime_parts_by_minutes(
        &parts,
        input->target_offset_minutes - source_offset_minutes,
        out_text
    );
}

static bool normalize_z_temporal_predicate_text(
    const char *text,
    size_t text_length,
    char *out_text
) {
    if (text == NULL || out_text == NULL) {
        return false;
    }
    if (text_length != datetime_text_length + 1U ||
        (text[datetime_date_time_separator_offset] != ' ' &&
         text[datetime_date_time_separator_offset] != 'T') ||
        (text[datetime_text_length] != 'Z' && text[datetime_text_length] != 'z')) {
        return false;
    }

    memcpy(out_text, text, datetime_text_length);
    out_text[datetime_date_time_separator_offset] = ' ';
    out_text[datetime_text_length] = '\0';
    return datetime_text_has_canonical_shape(out_text, datetime_text_length);
}

static bool parse_temporal_predicate_offset(const char *text, int *out_offset_minutes) {
    int sign = 0;
    int hours = 0;
    int minutes = 0;
    int total_minutes = 0;

    if (text == NULL || out_offset_minutes == NULL) {
        return false;
    }
    if (text[temporal_predicate_offset_sign_index] == '+') {
        sign = 1;
    } else if (text[temporal_predicate_offset_sign_index] == '-') {
        sign = -1;
    } else {
        return false;
    }
    if (text[temporal_predicate_offset_hour_tens_index] < '0' ||
        text[temporal_predicate_offset_hour_tens_index] > '9' ||
        text[temporal_predicate_offset_hour_ones_index] < '0' ||
        text[temporal_predicate_offset_hour_ones_index] > '9' ||
        text[temporal_predicate_offset_separator_index] != ':' ||
        text[temporal_predicate_offset_minute_tens_index] < '0' ||
        text[temporal_predicate_offset_minute_tens_index] > '9' ||
        text[temporal_predicate_offset_minute_ones_index] < '0' ||
        text[temporal_predicate_offset_minute_ones_index] > '9' ||
        text[temporal_predicate_offset_text_length] != '\0') {
        return false;
    }

    hours = ((text[temporal_predicate_offset_hour_tens_index] - '0') * decimal_base) +
            (text[temporal_predicate_offset_hour_ones_index] - '0');
    minutes = ((text[temporal_predicate_offset_minute_tens_index] - '0') * decimal_base) +
              (text[temporal_predicate_offset_minute_ones_index] - '0');
    if (minutes > time_zone_max_minute) {
        return false;
    }
    total_minutes = (hours * time_zone_minutes_per_hour) + minutes;
    if (total_minutes > temporal_predicate_offset_max_minutes) {
        return false;
    }
    if (sign < 0 && total_minutes == 0) {
        return false;
    }

    *out_offset_minutes = sign * total_minutes;
    return true;
}

static bool shift_datetime_parts_by_minutes(
    const struct date_add_datetime_parts *parts,
    int delta_minutes,
    char *out_text
) {
    const int64_t seconds_per_day = date_add_seconds_per_day();
    struct date_add_day_second shifted = {0};
    struct date_add_datetime_parts output = {0};
    int64_t days = 0;
    int64_t day_seconds = 0;
    int64_t base_seconds = 0;
    int64_t shifted_seconds = 0;
    int64_t result_day_seconds = 0;

    if (parts == NULL || out_text == NULL) {
        return false;
    }

    days = date_add_days_from_datetime(parts);
    day_seconds = ((int64_t)parts->hour * (int64_t)time_second_per_hour) +
                  ((int64_t)parts->minute * (int64_t)time_second_per_minute) +
                  (int64_t)parts->second;
    base_seconds = (days * seconds_per_day) + day_seconds;
    if (!date_add_checked_add_int64(
            base_seconds,
            (int64_t)delta_minutes * time_zone_seconds_per_minute,
            &shifted_seconds
        )) {
        return false;
    }

    shifted = date_add_floor_divmod_seconds(shifted_seconds);
    date_add_civil_from_days(shifted.days, &output);
    if (output.year < date_minimum_year || output.year > date_maximum_year) {
        return false;
    }
    result_day_seconds = shifted.day_seconds;
    output.hour = (uint32_t)(result_day_seconds / time_second_per_hour);
    result_day_seconds %= time_second_per_hour;
    output.minute = (uint32_t)(result_day_seconds / time_second_per_minute);
    output.second = (uint32_t)(result_day_seconds % time_second_per_minute);

    return format_datetime_parts_text(&output, out_text);
}

static bool format_datetime_parts_text(
    const struct date_add_datetime_parts *parts,
    char *out_text
) {
    int written = 0;

    if (parts == NULL || out_text == NULL) {
        return false;
    }

    written = snprintf(
        out_text,
        datetime_text_length + 1U,
        "%04" PRId64 "-%02" PRIu32 "-%02" PRIu32 " %02" PRIu32 ":%02" PRIu32 ":%02" PRIu32,
        parts->year,
        parts->month,
        parts->day,
        parts->hour,
        parts->minute,
        parts->second
    );
    return written == datetime_text_length;
}

static bool predicate_date_text_admitted(
    const struct mylite_db *database,
    const char *text,
    size_t text_length
) {
    enum temporal_text_kind kind = classify_date_text(text, text_length);

    switch (kind) {
    case TEMPORAL_TEXT_NORMAL:
        return true;
    case TEMPORAL_TEXT_FULL_ZERO:
        if (temporal_sql_mode_has_no_zero_date(database)) {
            return false;
        }
        return true;
    case TEMPORAL_TEXT_PARTIAL_ZERO:
        if (temporal_sql_mode_has_no_zero_in_date(database)) {
            return false;
        }
        return true;
    case TEMPORAL_TEXT_ALLOW_INVALID_CANDIDATE:
        return temporal_sql_mode_has_allow_invalid_dates(database);
    case TEMPORAL_TEXT_INVALID:
    case TEMPORAL_TEXT_DEFERRED:
        return false;
    }
    return false;
}

static bool predicate_datetime_text_admitted(
    const struct mylite_db *database,
    const char *text,
    size_t text_length
) {
    enum temporal_text_kind kind = classify_datetime_text(text, text_length);

    switch (kind) {
    case TEMPORAL_TEXT_NORMAL:
        return true;
    case TEMPORAL_TEXT_FULL_ZERO:
        if (temporal_sql_mode_has_no_zero_date(database)) {
            return false;
        }
        return true;
    case TEMPORAL_TEXT_PARTIAL_ZERO:
        if (temporal_sql_mode_has_no_zero_in_date(database)) {
            return false;
        }
        return true;
    case TEMPORAL_TEXT_ALLOW_INVALID_CANDIDATE:
        return temporal_sql_mode_has_allow_invalid_dates(database);
    case TEMPORAL_TEXT_INVALID:
    case TEMPORAL_TEXT_DEFERRED:
        return false;
    }
    return false;
}

static bool predicate_timestamp_text_admitted(
    const struct mylite_db *database,
    const char *text,
    size_t text_length
) {
    if (timestamp_text_is_canonical_valid(text, text_length)) {
        return true;
    }
    if (datetime_text_is_zero_datetime(text, text_length)) {
        if (temporal_sql_mode_has_no_zero_date(database)) {
            return false;
        }
        return true;
    }
    return false;
}

static int convert_integer_for_predicate(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    int64_t *out_value
) {
    const uint64_t bigint_signed_negative_abs_max = 9223372036854775808ULL;
    struct integer_column_range range = {0};
    int rc = integer_range_for_column(
        database,
        column,
        "WHERE supports only baseline integer columns",
        &range
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_negative) {
        if ((range.negative_abs_max == 0U && magnitude != 0U) ||
            magnitude > range.negative_abs_max) {
            set_predicate_out_of_range_error(database, column->name);
            return MYLITE_ERROR;
        }
        if (magnitude == bigint_signed_negative_abs_max) {
            *out_value = INT64_MIN;
        } else {
            *out_value = -(int64_t)magnitude;
        }
        return MYLITE_OK;
    }
    if (magnitude > range.positive_max) {
        set_predicate_out_of_range_error(database, column->name);
        return MYLITE_ERROR;
    }

    *out_value = (int64_t)magnitude;
    return MYLITE_OK;
}

static int plan_select_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_field_order,
    bool allow_rand_order,
    struct planned_select_order *out_order
) {
    const struct mylite_sql_ast_node *order_items = NULL;
    int rc = MYLITE_OK;

    *out_order = (struct planned_select_order){
        .has_order = false,
        .direction = PLANNED_SELECT_ORDER_DEFAULT,
        .column_source_index = 0U,
        .qualify_column_reference = select_source_context_is_joined(source_context),
    };
    if (order_clause == NULL) {
        return MYLITE_OK;
    }
    if (order_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
        set_unsupported_error(database, "SELECT supports only descriptor ORDER BY columns");
        return MYLITE_ERROR;
    }

    order_items = child_at(order_clause, 0U);
    if (order_items != NULL && order_items->kind == MYLITE_SQL_AST_ORDER_BY_ITEM_LIST) {
        rc = plan_select_order_item_list(
            database,
            order_items,
            source_context,
            select_plan,
            table_columns,
            table_column_count,
            allow_field_order,
            allow_rand_order,
            out_order
        );
    } else {
        rc = plan_select_order_ast_item_and_append(
            database,
            (struct select_order_ast_item_nodes){
                .order_key = child_at(order_clause, 0U),
                .direction = child_at(order_clause, 1U),
            },
            source_context,
            select_plan,
            table_columns,
            table_column_count,
            allow_field_order,
            allow_rand_order,
            out_order
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_order->item_count > 0U) {
        out_order->has_order = true;
        if (out_order->items[0U].kind == PLANNED_SELECT_ORDER_ITEM_COLUMN) {
            out_order->column = out_order->items[0U].column;
            out_order->column_source_index = out_order->items[0U].column_source_index;
        }
        out_order->direction = out_order->items[0U].direction;
    }

    return MYLITE_OK;
}

static int plan_select_order_item_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_items,
    const struct select_source_context *source_context,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_field_order,
    bool allow_rand_order,
    struct planned_select_order *out_order
) {
    const struct mylite_sql_ast_node *item = child_at(order_items, 0U);

    if (order_item_list_contains_field_order_key(order_items) &&
        mylite_sql_ast_node_child_count(order_items) != 1U) {
        set_unsupported_error(database, "SELECT ORDER BY FIELD() supports only one order key");
        return MYLITE_ERROR;
    }
    if (order_item_list_contains_rand_order_key(order_items) &&
        mylite_sql_ast_node_child_count(order_items) != 1U) {
        set_unsupported_error(database, "SELECT ORDER BY RAND() supports only one order key");
        return MYLITE_ERROR;
    }

    while (item != NULL) {
        int rc = MYLITE_OK;

        if (item->kind != MYLITE_SQL_AST_ORDER_BY_ITEM) {
            set_unsupported_error(database, "SELECT supports only descriptor ORDER BY columns");
            return MYLITE_ERROR;
        }
        rc = plan_select_order_ast_item_and_append(
            database,
            (struct select_order_ast_item_nodes){
                .order_key = child_at(item, 0U),
                .direction = child_at(item, 1U),
            },
            source_context,
            select_plan,
            table_columns,
            table_column_count,
            allow_field_order,
            allow_rand_order,
            out_order
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        item = item->next_sibling;
    }

    return MYLITE_OK;
}

static int plan_select_order_ast_item_and_append(
    struct mylite_db *database,
    struct select_order_ast_item_nodes item_nodes,
    const struct select_source_context *source_context,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_field_order,
    bool allow_rand_order,
    struct planned_select_order *out_order
) {
    struct planned_select_order_item planned_item = {0};
    int rc = plan_select_order_ast_item(
        database,
        item_nodes,
        source_context,
        select_plan,
        table_columns,
        table_column_count,
        allow_field_order,
        allow_rand_order,
        &planned_item
    );

    if (rc == MYLITE_OK) {
        rc = append_planned_select_order_item(out_order, planned_item);
    }
    if (rc != MYLITE_OK) {
        planned_select_order_item_deinit(&planned_item);
    }

    return rc;
}

static int plan_select_order_ast_item(
    struct mylite_db *database,
    struct select_order_ast_item_nodes item_nodes,
    const struct select_source_context *source_context,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_field_order,
    bool allow_rand_order,
    struct planned_select_order_item *out_item
) {
    bool resolved_alias = false;
    int rc = MYLITE_OK;

    *out_item = (struct planned_select_order_item){
        .kind = PLANNED_SELECT_ORDER_ITEM_COLUMN,
        .direction = PLANNED_SELECT_ORDER_ASC,
        .column_source_index = 0U,
    };

    if (select_order_key_is_field_function(item_nodes.order_key)) {
        if (!allow_field_order) {
            set_unsupported_error(database, "SELECT ORDER BY supports only descriptor columns");
            return MYLITE_ERROR;
        }
        rc = plan_select_order_field_expression(
            database,
            item_nodes.order_key,
            source_context,
            table_columns,
            table_column_count,
            out_item
        );
        if (rc == MYLITE_OK && mylite_sql_ast_node_order_direction(item_nodes.direction) ==
                                   MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
            out_item->direction = PLANNED_SELECT_ORDER_DESC;
        }
        return rc;
    }
    if (select_order_key_is_rand_function(item_nodes.order_key)) {
        if (!allow_rand_order || select_source_context_is_joined(source_context)) {
            set_unsupported_error(database, "SELECT ORDER BY supports only descriptor columns");
            return MYLITE_ERROR;
        }
        rc = plan_select_order_rand_expression(database, item_nodes.order_key, out_item);
        if (rc == MYLITE_OK && mylite_sql_ast_node_order_direction(item_nodes.direction) ==
                                   MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
            out_item->direction = PLANNED_SELECT_ORDER_DESC;
        }
        return rc;
    }

    rc = resolve_order_alias(
        database,
        item_nodes.order_key,
        select_plan,
        &out_item->column,
        &out_item->column_source_index,
        &resolved_alias
    );
    if (rc == MYLITE_OK && !resolved_alias) {
        rc = resolve_order_column(
            database,
            item_nodes.order_key,
            source_context,
            table_columns,
            table_column_count,
            &out_item->column,
            &out_item->column_source_index
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_set(&out_item->column)) {
        set_unsupported_error(database, "ORDER BY does not yet support SET columns");
        return MYLITE_ERROR;
    }
    if (!column_descriptor_is_string_family(&out_item->column) &&
        !column_descriptor_is_date(&out_item->column) &&
        !column_descriptor_is_time(&out_item->column) &&
        !column_descriptor_is_datetime(&out_item->column) &&
        !column_descriptor_is_timestamp(&out_item->column) &&
        !column_descriptor_is_year(&out_item->column) &&
        !column_descriptor_is_bit(&out_item->column)) {
        struct integer_column_range range = {0};

        rc = integer_range_for_column(
            database,
            &out_item->column,
            "ORDER BY supports only integer, BIT, YEAR, DATE, TIME, DATETIME, TIMESTAMP, or "
            "nonbinary string descriptor columns",
            &range
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    if (mylite_sql_ast_node_order_direction(item_nodes.direction) ==
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
        out_item->direction = PLANNED_SELECT_ORDER_DESC;
    }

    return MYLITE_OK;
}

static bool order_item_list_contains_field_order_key(
    const struct mylite_sql_ast_node *order_items
) {
    const struct mylite_sql_ast_node *item = NULL;

    if (order_items == NULL || order_items->kind != MYLITE_SQL_AST_ORDER_BY_ITEM_LIST) {
        return false;
    }
    item = child_at(order_items, 0U);
    while (item != NULL) {
        if (item->kind == MYLITE_SQL_AST_ORDER_BY_ITEM &&
            select_order_key_is_field_function(child_at(item, 0U))) {
            return true;
        }
        item = item->next_sibling;
    }
    return false;
}

static bool order_item_list_contains_rand_order_key(const struct mylite_sql_ast_node *order_items) {
    const struct mylite_sql_ast_node *item = NULL;

    if (order_items == NULL || order_items->kind != MYLITE_SQL_AST_ORDER_BY_ITEM_LIST) {
        return false;
    }
    item = child_at(order_items, 0U);
    while (item != NULL) {
        if (item->kind == MYLITE_SQL_AST_ORDER_BY_ITEM &&
            select_order_key_is_rand_function(child_at(item, 0U))) {
            return true;
        }
        item = item->next_sibling;
    }
    return false;
}

static bool select_order_key_is_field_function(const struct mylite_sql_ast_node *order_key) {
    order_key = unwrap_parenthesized_expression(order_key);
    if (order_key == NULL) {
        return false;
    }
    if (order_key->kind == MYLITE_SQL_AST_FIELD_FUNCTION) {
        return true;
    }
    if (order_key->kind == MYLITE_SQL_AST_FIELD_ARGUMENT_COUNT_ERROR) {
        return true;
    }
    return false;
}

static bool select_order_key_is_rand_function(const struct mylite_sql_ast_node *order_key) {
    order_key = unwrap_parenthesized_expression(order_key);
    if (order_key == NULL) {
        return false;
    }
    if (order_key->kind == MYLITE_SQL_AST_RAND_FUNCTION ||
        order_key->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION ||
        order_key->kind == MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR) {
        return true;
    }
    return false;
}

static int plan_select_order_field_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_order_item *out_item
) {
    struct planned_row_scalar_expression *expression = NULL;
    int rc = MYLITE_OK;

    order_key = unwrap_parenthesized_expression(order_key);
    if (order_key == NULL) {
        set_unsupported_error(
            database,
            "SELECT ORDER BY supports only descriptor columns or FIELD(column, value, ...)"
        );
        return MYLITE_ERROR;
    }
    if (order_key->kind == MYLITE_SQL_AST_FIELD_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "FIELD");
        return MYLITE_ERROR;
    }

    expression = (struct planned_row_scalar_expression *)calloc(1U, sizeof(*expression));
    if (expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_field_expression_with_context(
        database,
        order_key,
        true,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_ORDER,
        "SELECT ORDER BY FIELD() supports only descriptor search columns",
        expression
    );
    if (rc == MYLITE_OK) {
        rc = validate_select_order_field_expression(database, expression);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(expression);
        free(expression);
        return rc;
    }

    out_item->kind = PLANNED_SELECT_ORDER_ITEM_FIELD;
    out_item->expression = expression;
    return MYLITE_OK;
}

static int plan_select_order_rand_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    struct planned_select_order_item *out_item
) {
    struct planned_row_scalar_expression *expression = NULL;
    int rc = MYLITE_OK;

    expression = (struct planned_row_scalar_expression *)calloc(1U, sizeof(*expression));
    if (expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_rand_expression(database, order_key, expression);
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(expression);
        free(expression);
        return rc;
    }

    out_item->kind = PLANNED_SELECT_ORDER_ITEM_RAND;
    out_item->expression = expression;
    return MYLITE_OK;
}

static int validate_select_order_field_expression(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_FIELD ||
        expression->argument_count < 2U || expression->arguments == NULL) {
        set_native_function_parameter_count_error(database, "FIELD");
        return MYLITE_ERROR;
    }
    if (expression->arguments[0U].kind != PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        set_unsupported_error(
            database,
            "SELECT ORDER BY FIELD() supports only descriptor search columns"
        );
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 1U; argument_index < expression->argument_count;
         ++argument_index) {
        if (expression->arguments[argument_index].kind != PLANNED_ROW_SCALAR_EXPRESSION_VALUE) {
            set_unsupported_error(
                database,
                "SELECT ORDER BY FIELD() supports only literal candidate values"
            );
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static void planned_select_order_item_deinit(struct planned_select_order_item *item) {
    if (item == NULL) {
        return;
    }
    if (item->expression != NULL) {
        planned_row_scalar_expression_deinit(item->expression);
        free(item->expression);
    }
    *item = (struct planned_select_order_item){0};
}

static int append_planned_select_order_item(
    struct planned_select_order *order,
    struct planned_select_order_item item
) {
    struct planned_select_order_item *items = NULL;
    size_t required_count = 0U;

    if (order->item_count >= SIZE_MAX / sizeof(*items)) {
        return MYLITE_NOMEM;
    }
    required_count = order->item_count + 1U;

    items = realloc(order->items, required_count * sizeof(*items));
    if (items == NULL) {
        return MYLITE_NOMEM;
    }

    order->items = items;
    order->items[order->item_count] = item;
    order->item_count = required_count;
    return MYLITE_OK;
}

static int resolve_order_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct planned_select *select_plan,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index,
    bool *out_resolved
) {
    char *order_name = NULL;
    bool found = false;
    int rc = MYLITE_OK;

    *out_resolved = false;
    if (select_plan == NULL || column_node == NULL ||
        column_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    rc = copy_select_item_identifier_alias_text(database, column_node, &order_name);
    if (rc != MYLITE_OK) {
        return rc;
    }

    for (size_t column_index = 0U; column_index < select_plan->column_count; ++column_index) {
        const struct mylite_sql_ast_node *alias = select_plan->column_aliases[column_index];
        char *alias_text = NULL;

        if (alias == NULL) {
            continue;
        }

        rc = copy_select_item_alias_text(database, alias, &alias_text);
        if (rc != MYLITE_OK) {
            free(order_name);
            return rc;
        }
        if (text_equals_ascii_case_insensitive(alias_text, order_name)) {
            if (found) {
                free(alias_text);
                set_ambiguous_order_column_error(database, order_name);
                free(order_name);
                return MYLITE_ERROR;
            }
            *out_column = select_plan->columns[column_index];
            if (out_source_index != NULL) {
                *out_source_index = select_plan->column_source_indexes[column_index];
            }
            found = true;
        }
        free(alias_text);
    }

    free(order_name);
    *out_resolved = found;
    return MYLITE_OK;
}

static int resolve_order_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
) {
    return resolve_descriptor_column_reference_with_source_index(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_ORDER,
        "ORDER BY supports only unqualified descriptor columns",
        table_columns,
        table_column_count,
        out_column,
        out_source_index
    );
}

static int plan_select_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
) {
    int rc = MYLITE_OK;

    out_limit->has_limit = false;
    out_limit->row_count = 0;
    out_limit->has_offset = false;
    out_limit->offset = 0;
    if (limit_clause == NULL) {
        return MYLITE_OK;
    }
    if (limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE) {
        set_unsupported_error(database, "SELECT supports only literal LIMIT clauses");
        return MYLITE_ERROR;
    }

    rc = convert_limit_integer_literal(database, child_at(limit_clause, 0U), &out_limit->row_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_limit->has_limit = true;

    if (child_at(limit_clause, 1U) != NULL) {
        rc =
            convert_limit_integer_literal(database, child_at(limit_clause, 1U), &out_limit->offset);
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_limit->has_offset = true;
    }

    return MYLITE_OK;
}

static int plan_delete_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
) {
    int rc = MYLITE_OK;

    out_limit->has_limit = false;
    out_limit->row_count = 0;
    out_limit->has_offset = false;
    out_limit->offset = 0;
    if (limit_clause == NULL) {
        return MYLITE_OK;
    }
    if (limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE) {
        set_unsupported_error(database, "DELETE supports only LIMIT row_count");
        return MYLITE_ERROR;
    }
    if (child_at(limit_clause, 1U) != NULL) {
        set_unsupported_error(database, "DELETE supports only LIMIT row_count");
        return MYLITE_ERROR;
    }

    rc = convert_limit_integer_literal(database, child_at(limit_clause, 0U), &out_limit->row_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_limit->has_limit = true;

    return MYLITE_OK;
}

static int plan_update_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
) {
    const struct mylite_sql_ast_node *assignment = child_at(assignment_list, 0U);
    const struct mylite_sql_ast_node *target = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    if (assignment_list == NULL || assignment_list->kind != MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(assignment_list) == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(assignment_list) != 1U) {
        rc = plan_update_multiple_assignments(
            database,
            assignment_list,
            table_columns,
            table_column_count,
            out_plan
        );
        if (rc == MYLITE_OK) {
            rc = validate_update_multiple_auto_update_columns(
                database,
                out_plan,
                table_columns,
                table_column_count
            );
        }
        return rc;
    }
    if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_UPDATE_ASSIGNMENT) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    target = child_at(assignment, 0U);
    if (target == NULL || target->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_unsupported_error(database, "UPDATE supports only unqualified assignment columns");
        return MYLITE_ERROR;
    }
    rc = copy_identifier_text(target, column_name, sizeof(column_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = find_column_index(table_columns, table_column_count, column_name, &column_index);
    if (rc != MYLITE_OK) {
        set_unknown_column_error(database, column_name);
        return MYLITE_ERROR;
    }

    out_plan->assignment_column = table_columns[column_index];
    out_plan->assignment_value_node = child_at(assignment, 1U);
    if (out_plan->assignment_value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (out_plan->assignment_column.is_generated) {
        const struct mylite_sql_ast_node *value_node =
            unwrap_parenthesized_expression(out_plan->assignment_value_node);

        if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE) {
            out_plan->generated_default_noop = true;
            return MYLITE_OK;
        }
        set_generated_column_value_error(
            database,
            out_plan->assignment_column.name,
            out_plan->table.name
        );
        return MYLITE_ERROR;
    }
    rc =
        plan_update_date_interval_assignment(database, table_columns, table_column_count, out_plan);
    if (rc != MYLITE_OK ||
        out_plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL) {
        return rc;
    }
    rc = plan_update_arithmetic_assignment(database, table_columns, table_column_count, out_plan);
    if (rc != MYLITE_OK ||
        out_plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC) {
        return rc;
    }

    return plan_update_scalar_subquery_assignment(database, out_plan);
}

static int plan_update_multiple_assignments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
) {
    size_t assignment_count = mylite_sql_ast_node_child_count(assignment_list);
    int rc = MYLITE_OK;

    out_plan->assignments = calloc(assignment_count, sizeof(*out_plan->assignments));
    if (out_plan->assignments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->assignment_count = assignment_count;

    for (size_t assignment_index = 0U; rc == MYLITE_OK && assignment_index < assignment_count;
         ++assignment_index) {
        rc = plan_update_multiple_assignment(
            database,
            child_at(assignment_list, assignment_index),
            table_columns,
            table_column_count,
            out_plan,
            assignment_index
        );
    }
    if (rc == MYLITE_OK && planned_update_executable_assignment_count(out_plan) == 0U) {
        out_plan->generated_default_noop = true;
    }

    return rc;
}

static int plan_update_multiple_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan,
    size_t assignment_index
) {
    const struct mylite_sql_ast_node *target = NULL;
    const struct mylite_sql_ast_node *value_node = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_UPDATE_ASSIGNMENT) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    target = child_at(assignment, 0U);
    if (target == NULL || target->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_update_multiple_assignment_unsupported_error(database);
        return MYLITE_ERROR;
    }
    rc = copy_identifier_text(target, column_name, sizeof(column_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = find_column_index(table_columns, table_column_count, column_name, &column_index);
    if (rc != MYLITE_OK) {
        set_unknown_column_error(database, column_name);
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; index < assignment_index; ++index) {
        if (out_plan->assignments[index].column.column_id ==
            table_columns[column_index].column_id) {
            set_update_duplicate_assignment_unsupported_error(database);
            return MYLITE_ERROR;
        }
    }
    if (table_columns[column_index].is_generated) {
        value_node = unwrap_parenthesized_expression(child_at(assignment, 1U));
        if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE) {
            out_plan->assignments[assignment_index].column = table_columns[column_index];
            out_plan->assignments[assignment_index].value_node = child_at(assignment, 1U);
            out_plan->assignments[assignment_index].generated_default_noop = true;
            return MYLITE_OK;
        }
        set_generated_column_value_error(
            database,
            table_columns[column_index].name,
            out_plan->table.name
        );
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_auto_increment(&table_columns[column_index]) ||
        update_column_id_is_keyed(out_plan, table_columns[column_index].column_id)) {
        set_update_multiple_assignment_unsupported_error(database);
        return MYLITE_ERROR;
    }

    value_node = child_at(assignment, 1U);
    if (!update_assignment_value_is_multi_constant_supported(value_node)) {
        set_update_multiple_assignment_unsupported_error(database);
        return MYLITE_ERROR;
    }

    out_plan->assignments[assignment_index].column = table_columns[column_index];
    out_plan->assignments[assignment_index].value_node = value_node;

    return MYLITE_OK;
}

static int plan_update_date_interval_assignment(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
) {
    const struct mylite_sql_ast_node *value_node = NULL;
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    if (out_plan == NULL) {
        return MYLITE_MISUSE;
    }
    value_node = unwrap_parenthesized_expression(out_plan->assignment_value_node);
    if (value_node == NULL || !is_date_interval_second_function_kind(value_node->kind) ||
        value_node->kind == MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION) {
        return MYLITE_OK;
    }

    function_name = date_interval_second_function_name(value_node->kind);
    rc = validate_date_interval_second_function_shape(database, value_node, function_name);
    if (rc == MYLITE_OK) {
        rc = plan_update_date_interval_source_column(
            database,
            date_interval_second_temporal_node(value_node),
            table_columns,
            table_column_count,
            &out_plan->assignment_column
        );
    }
    if (rc == MYLITE_OK) {
        rc = date_interval_unit_from_ast(
            database,
            date_interval_unit_node(value_node),
            function_name,
            &out_plan->date_interval_unit
        );
    }
    if (rc == MYLITE_OK) {
        rc = date_interval_second_interval_argument(
            database,
            function_name,
            date_interval_second_interval_node(value_node),
            out_plan->date_interval_unit,
            &out_plan->date_interval_value,
            &out_plan->date_interval_is_null
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_update_date_interval_target(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        out_plan->assignment_value_kind = PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL;
        out_plan->date_interval_subtract =
            date_interval_second_function_subtracts(value_node->kind);
    }
    return rc;
}

static int plan_update_date_interval_source_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor *assignment_column
) {
    char source_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t source_index = 0U;
    int rc = MYLITE_OK;

    source_node = unwrap_parenthesized_expression(source_node);
    if (source_node == NULL || source_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_update_date_interval_column_mismatch_error(database);
        return MYLITE_ERROR;
    }
    rc = copy_identifier_text(source_node, source_name, sizeof(source_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = find_column_index(table_columns, table_column_count, source_name, &source_index);
    if (rc != MYLITE_OK) {
        set_unknown_column_error(database, source_name);
        return MYLITE_ERROR;
    }
    if (assignment_column == NULL ||
        table_columns[source_index].column_id != assignment_column->column_id) {
        set_update_date_interval_column_mismatch_error(database);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int plan_update_date_interval_target(
    struct mylite_db *database,
    struct planned_update *plan
) {
    int rc = MYLITE_OK;

    if (plan == NULL) {
        return MYLITE_MISUSE;
    }
    if (update_assignment_column_is_keyed(plan)) {
        set_update_date_interval_keyed_target_error(database);
        return MYLITE_ERROR;
    }
    if (!update_date_interval_column_family_is_supported(&plan->assignment_column)) {
        set_update_date_interval_target_error(database);
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_date(&plan->assignment_column) &&
        mylite_date_interval_unit_has_time_part(plan->date_interval_unit)) {
        set_update_date_interval_date_time_unit_error(database);
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_char(&plan->assignment_column) ||
        column_descriptor_is_varchar(&plan->assignment_column)) {
        rc = validate_update_date_interval_string_target(database, &plan->assignment_column);
    }
    if (rc == MYLITE_OK) {
        rc = update_date_interval_input_kind_for_column(
            database,
            &plan->assignment_column,
            &plan->date_interval_input_kind
        );
    }
    return rc;
}

static int update_date_interval_input_kind_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    enum mylite_date_interval_second_input_kind *out_kind
) {
    if (out_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING;
    if (column_descriptor_is_date(column)) {
        *out_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_DATE;
        return MYLITE_OK;
    }
    if (column_descriptor_is_datetime(column)) {
        *out_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_DATETIME;
        return MYLITE_OK;
    }
    if (column_descriptor_is_string_family(column)) {
        *out_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING;
        return MYLITE_OK;
    }
    set_update_date_interval_target_error(database);
    return MYLITE_ERROR;
}

static bool update_date_interval_column_family_is_supported(
    const struct mylite_catalog_column_descriptor *column
) {
    return (column_descriptor_is_date(column) || column_descriptor_is_datetime(column) ||
            column_descriptor_is_string_family(column)) != 0;
}

static int validate_update_date_interval_string_target(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    size_t declared_length = 0U;
    int rc = MYLITE_OK;

    if (column_descriptor_is_char(column)) {
        rc = char_length_for_column(database, column, &declared_length);
    } else if (column_descriptor_is_varchar(column)) {
        rc = varchar_length_for_column(database, column, &declared_length);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (declared_length < datetime_text_length) {
        set_update_date_interval_target_error(database);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static void set_update_date_interval_column_mismatch_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "UPDATE DATE interval assignment supports only the assigned column as input"
    );
}

static void set_update_date_interval_target_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "UPDATE DATE interval assignment supports only DATE, DATETIME, and nonbinary string targets"
    );
}

static void set_update_date_interval_date_time_unit_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "UPDATE DATE interval assignment does not yet support time units for DATE targets"
    );
}

static void set_update_date_interval_keyed_target_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "UPDATE DATE interval assignment does not yet support key columns"
    );
}

static int plan_update_arithmetic_assignment(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
) {
    const struct mylite_sql_ast_node *value_node = NULL;
    const struct mylite_sql_ast_node *source = NULL;
    const struct mylite_sql_ast_node *delta = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    char source_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t source_index = 0U;
    bool is_constant_arithmetic = false;
    int rc = MYLITE_OK;

    value_node = unwrap_parenthesized_expression(out_plan->assignment_value_node);
    if (value_node == NULL) {
        return MYLITE_OK;
    }
    if (insert_value_is_unix_timestamp_arithmetic(value_node)) {
        return validate_update_unix_timestamp_arithmetic_assignment_target(
            database,
            &out_plan->assignment_column
        );
    }
    if (value_node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return MYLITE_OK;
    }
    rc = update_value_is_constant_arithmetic_expression(
        database,
        value_node,
        &is_constant_arithmetic
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_constant_arithmetic) {
        return MYLITE_OK;
    }

    operator_kind = mylite_sql_ast_node_operator(value_node);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_ADD &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_SUBTRACT) {
        set_update_arithmetic_unsupported_error(database);
        return MYLITE_ERROR;
    }

    source = child_at(value_node, 0U);
    delta = child_at(value_node, 1U);
    if (source == NULL || source->kind != MYLITE_SQL_AST_IDENTIFIER || delta == NULL ||
        delta->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(delta) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_update_arithmetic_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = copy_identifier_text(source, source_name, sizeof(source_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = find_column_index(table_columns, table_column_count, source_name, &source_index);
    if (rc != MYLITE_OK) {
        set_unknown_column_error(database, source_name);
        return MYLITE_ERROR;
    }
    if (table_columns[source_index].column_id != out_plan->assignment_column.column_id) {
        set_update_arithmetic_unsupported_error(database);
        return MYLITE_ERROR;
    }

    out_plan->assignment_value_kind = PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC;
    out_plan->arithmetic_operator = operator_kind;
    out_plan->arithmetic_delta_node = delta;

    return validate_update_arithmetic_assignment_target(database, out_plan);
}

static int update_value_is_constant_arithmetic_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    bool *out_is_constant
) {
    struct scalar_arithmetic_node_stack stack = {0};
    const struct mylite_sql_ast_node *current = NULL;
    bool admitted = true;
    int rc = MYLITE_OK;

    if (out_is_constant == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    *out_is_constant = false;

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return MYLITE_OK;
    }
    if (!scalar_arithmetic_node_stack_push(&stack, value_node)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    while (stack.count != 0U && admitted) {
        --stack.count;
        current = unwrap_parenthesized_expression(stack.items[stack.count]);
        rc = update_constant_arithmetic_visit_node(database, &stack, current, &admitted);
        if (rc != MYLITE_OK) {
            goto done;
        }
    }

    *out_is_constant = admitted;

done:
    scalar_arithmetic_node_stack_deinit(&stack);
    return rc;
}

static int update_constant_arithmetic_visit_node(
    struct mylite_db *database,
    struct scalar_arithmetic_node_stack *stack,
    const struct mylite_sql_ast_node *value_node,
    bool *in_out_admitted
) {
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    int rc = MYLITE_OK;

    if (value_node == NULL) {
        *in_out_admitted = false;
        return MYLITE_OK;
    }

    switch (value_node->kind) {
    case MYLITE_SQL_AST_LITERAL:
        *in_out_admitted = update_constant_arithmetic_literal_is_admitted(
            mylite_sql_ast_node_literal_kind(value_node)
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        operator_kind = mylite_sql_ast_node_operator(value_node);
        if (!update_constant_arithmetic_unary_operator_is_admitted(operator_kind)) {
            *in_out_admitted = false;
            return MYLITE_OK;
        }
        return update_constant_arithmetic_stack_push(database, stack, child_at(value_node, 0U));
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        operator_kind = mylite_sql_ast_node_operator(value_node);
        if (!update_constant_arithmetic_binary_operator_is_admitted(operator_kind)) {
            *in_out_admitted = false;
            return MYLITE_OK;
        }
        rc = update_constant_arithmetic_stack_push(database, stack, child_at(value_node, 0U));
        if (rc != MYLITE_OK) {
            return rc;
        }
        return update_constant_arithmetic_stack_push(database, stack, child_at(value_node, 1U));
    default:
        *in_out_admitted = false;
        return MYLITE_OK;
    }
}

static bool update_constant_arithmetic_literal_is_admitted(
    enum mylite_sql_ast_literal_kind literal_kind
) {
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_INTEGER:
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
    case MYLITE_SQL_AST_LITERAL_NULL:
        return true;
    default:
        return false;
    }
}

static bool update_constant_arithmetic_unary_operator_is_admitted(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
        return true;
    default:
        return false;
    }
}

static bool update_constant_arithmetic_binary_operator_is_admitted(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        return true;
    default:
        return false;
    }
}

static int update_constant_arithmetic_stack_push(
    struct mylite_db *database,
    struct scalar_arithmetic_node_stack *stack,
    const struct mylite_sql_ast_node *value_node
) {
    if (!scalar_arithmetic_node_stack_push(stack, value_node)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    return MYLITE_OK;
}

static int validate_update_arithmetic_assignment_target(
    struct mylite_db *database,
    const struct planned_update *plan
) {
    struct integer_column_range range = {0};
    int rc = integer_range_for_column(
        database,
        &plan->assignment_column,
        "UPDATE arithmetic assignment supports only integer columns",
        &range
    );

    (void)range;
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_auto_increment(&plan->assignment_column) ||
        update_assignment_column_is_keyed(plan)) {
        set_update_arithmetic_unsupported_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_update_unix_timestamp_arithmetic_assignment_target(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    struct integer_column_range range = {0};
    int rc = MYLITE_OK;

    if (column_descriptor_is_auto_increment(column)) {
        set_unsupported_error(
            database,
            "UPDATE UNIX_TIMESTAMP arithmetic does not yet support AUTO_INCREMENT targets"
        );
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_string_family(column)) {
        return MYLITE_OK;
    }

    rc = integer_range_for_column(
        database,
        column,
        "UPDATE UNIX_TIMESTAMP arithmetic supports only integer and nonbinary string targets",
        &range
    );
    (void)range;
    return rc;
}

static bool update_assignment_column_is_keyed(const struct planned_update *plan) {
    if (plan == NULL) {
        return false;
    }
    return update_column_id_is_keyed(plan, plan->assignment_column.column_id);
}

static bool update_column_id_is_keyed(const struct planned_update *plan, int64_t column_id) {
    if (plan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct loaded_index_info *key = &plan->indexes[index];

        if ((key->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY || key->index.is_unique) &&
            loaded_index_contains_column_id(key, column_id)) {
            return true;
        }
    }

    return false;
}

static int validate_update_ignore_assignment_support(
    struct mylite_db *database,
    const struct planned_update *plan
) {
    if (plan == NULL || !plan->ignore_errors || plan->generated_default_noop) {
        return MYLITE_OK;
    }
    if (planned_update_has_multiple_assignments(plan)) {
        for (size_t index = 0U; index < plan->assignment_count; ++index) {
            const struct planned_update_assignment *assignment = &plan->assignments[index];

            if (planned_update_assignment_is_noop(assignment)) {
                continue;
            }
            if (column_descriptor_is_auto_increment(&assignment->column) ||
                update_column_id_is_keyed(plan, assignment->column.column_id)) {
                set_unsupported_error(
                    database,
                    "UPDATE IGNORE does not yet support key assignment conflict demotion"
                );
                return MYLITE_ERROR;
            }
        }
        return MYLITE_OK;
    }
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL) {
        set_unsupported_error(
            database,
            "UPDATE IGNORE does not yet support DATE interval assignments"
        );
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_auto_increment(&plan->assignment_column) ||
        update_assignment_column_is_keyed(plan)) {
        set_unsupported_error(
            database,
            "UPDATE IGNORE does not yet support key assignment conflict demotion"
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static bool update_assignment_value_is_multi_constant_supported(
    const struct mylite_sql_ast_node *value_node
) {
    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL) {
        return false;
    }

    switch (value_node->kind) {
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_DML_DEFAULT_VALUE:
    case MYLITE_SQL_AST_DEFAULT_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SYSDATE_FUNCTION:
    case MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CURRENT_DATE_VALUE:
    case MYLITE_SQL_AST_CURRENT_TIME_VALUE:
    case MYLITE_SQL_AST_UTC_DATE_VALUE:
    case MYLITE_SQL_AST_UTC_TIME_VALUE:
    case MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE:
        return true;
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return update_assignment_unary_value_is_multi_constant_supported(value_node);
    default:
        return false;
    }
}

static bool update_assignment_unary_value_is_multi_constant_supported(
    const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *operand = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NULL;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return false;
    }
    operator_kind = mylite_sql_ast_node_operator(value_node);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        return false;
    }

    operand = child_at(value_node, 0U);
    if (operand == NULL || operand->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(operand);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
        literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT) {
        return true;
    }
    return false;
}

static int validate_update_multiple_auto_update_columns(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    for (size_t column_index = 0U; column_index < table_column_count; ++column_index) {
        if (planned_update_column_has_auto_update(plan, &table_columns[column_index]) &&
            update_column_id_is_keyed(plan, table_columns[column_index].column_id)) {
            set_update_multiple_assignment_unsupported_error(database);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static void set_update_multiple_assignment_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "UPDATE multiple assignments support only distinct unqualified non-key constant "
        "assignments"
    );
}

static void set_update_duplicate_assignment_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(database, "UPDATE multiple assignments do not support duplicate targets");
}

static int validate_update_default_function_sources(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    if (plan == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (planned_update_has_multiple_assignments(plan)) {
        int rc = MYLITE_OK;

        for (size_t index = 0U; rc == MYLITE_OK && index < plan->assignment_count; ++index) {
            rc = validate_update_default_function_source(
                database,
                plan->assignments[index].value_node,
                plan,
                table_columns,
                table_column_count
            );
        }
        return rc;
    }

    return validate_update_default_function_source(
        database,
        plan->assignment_value_node,
        plan,
        table_columns,
        table_column_count
    );
}

static int validate_update_default_function_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct planned_update *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    struct select_source_context source_context = {0};

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_DEFAULT_FUNCTION) {
        return MYLITE_OK;
    }

    source_context.source = &plan->target;
    return validate_default_function_source_value(
        database,
        value_node,
        &source_context,
        table_columns,
        table_column_count
    );
}

static int plan_update_scalar_subquery_assignment(
    struct mylite_db *database,
    struct planned_update *out_plan
) {
    const struct mylite_sql_ast_node *value_node = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    int rc = MYLITE_OK;

    if (out_plan == NULL) {
        return MYLITE_MISUSE;
    }

    value_node = unwrap_parenthesized_expression(out_plan->assignment_value_node);
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_SCALAR_SUBQUERY) {
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_child_count(value_node) != 1U) {
        set_unsupported_error(
            database,
            "UPDATE scalar subquery assignment supports only parenthesized SELECT"
        );
        return MYLITE_ERROR;
    }

    statement = child_at(value_node, 0U);
    rc = validate_update_scalar_subquery_select(database, statement);
    if (rc == MYLITE_OK) {
        rc = plan_select(database, statement, false, &out_plan->assignment_subquery);
    }
    if (rc == MYLITE_OK && out_plan->assignment_subquery.column_count != 1U) {
        set_scalar_subquery_column_count_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK &&
        out_plan->assignment_subquery.table.table_id == out_plan->table.table_id) {
        set_update_table_used_error(database, out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_plan->assignment_value_is_scalar_subquery = true;
    }

    return rc;
}

static int validate_update_scalar_subquery_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *from_clause = NULL;

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(
            database,
            "UPDATE scalar subquery assignment supports only parenthesized SELECT"
        );
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_select_modifier(statement) != MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT ||
        mylite_sql_ast_node_select_options(statement) != 0U ||
        mylite_sql_ast_node_select_calc_found_rows(statement) != 0 ||
        mylite_sql_ast_node_select_locking_clause(statement) !=
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_NONE) {
        set_unsupported_error(
            database,
            "UPDATE scalar subquery assignment supports only plain inner SELECT"
        );
        return MYLITE_ERROR;
    }

    select_list = child_at(statement, 0U);
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        select_list_is_wildcard(select_list)) {
        set_unsupported_error(
            database,
            "UPDATE scalar subquery assignment supports only one explicit source column"
        );
        return MYLITE_ERROR;
    }

    from_clause = child_at(statement, 1U);
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(
            database,
            "UPDATE scalar subquery assignment supports only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int convert_update_value(
    struct mylite_db *database,
    const struct planned_update *plan,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *value_node = NULL;
    const struct mylite_catalog_column_descriptor *column = NULL;

    if (plan == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    value_node = unwrap_parenthesized_expression(plan->assignment_value_node);
    column = &plan->assignment_column;
    if (value_node == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    if (value_node->kind == MYLITE_SQL_AST_SCALAR_SUBQUERY) {
        planned_value_deinit(out_value);
        *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        return materialize_update_scalar_subquery_value(database, plan, out_value);
    }
    if (value_node->kind == MYLITE_SQL_AST_DEFAULT_FUNCTION) {
        struct select_source_context source_context = {.source = &plan->target};

        return convert_default_function_value_for_target(
            database,
            value_node,
            &source_context,
            plan->columns,
            plan->column_count,
            column,
            plan->ignore_errors,
            out_value
        );
    }

    return convert_update_value_for_column(
        database,
        value_node,
        column,
        1U,
        plan->ignore_errors,
        out_value
    );
}

static int convert_update_value_for_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    bool handled = false;
    bool adjust_missing_default = dml_allows_missing_default_adjustment(database, ignore_errors);
    bool is_constant_arithmetic = false;
    int rc = MYLITE_OK;

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
    if (value_node->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE) {
        return materialize_dml_default_value(database, column, adjust_missing_default, out_value);
    }
    rc = convert_statement_time_value_for_column(database, value_node, column, out_value, &handled);
    if (handled) {
        return rc;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        if (!column->is_nullable) {
            if (!adjust_missing_default) {
                if (column_descriptor_is_spatial(column)) {
                    set_spatial_bad_null_error(database, column->name);
                    return MYLITE_ERROR;
                }
                set_bad_null_error(database, column->name);
                return MYLITE_ERROR;
            }
            return convert_null_insert_value(database, column, true, out_value);
        }
        return MYLITE_OK;
    }
    if (insert_value_is_unix_timestamp_arithmetic(value_node)) {
        return convert_update_unix_timestamp_arithmetic_value(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    rc = update_value_is_constant_arithmetic_expression(
        database,
        value_node,
        &is_constant_arithmetic
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_constant_arithmetic) {
        return convert_update_constant_arithmetic_value(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }

    return convert_update_column_value(
        database,
        value_node,
        column,
        row_number,
        ignore_errors,
        out_value
    );
}

static int convert_update_constant_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    static const uint64_t int64_min_magnitude = 9223372036854775808ULL;
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    bool is_negative = false;
    uint64_t magnitude = 0U;
    bool is_constant_arithmetic = false;
    bool adjust_missing_default = dml_allows_missing_default_adjustment(database, ignore_errors);
    int rc = MYLITE_OK;

    rc = update_value_is_constant_arithmetic_expression(
        database,
        value_node,
        &is_constant_arithmetic
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!is_constant_arithmetic) {
        set_unsupported_error(
            database,
            "UPDATE supports only integer, boolean, NULL, and DEFAULT assignment values"
        );
        return MYLITE_ERROR;
    }

    rc = validate_update_constant_arithmetic_assignment_target(database, column);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = evaluate_scalar_arithmetic_expression(database, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value.division_by_zero_warning_count != 0U) {
        set_unsupported_error(
            database,
            "UPDATE constant arithmetic assignment does not support division"
        );
        return MYLITE_ERROR;
    }
    if (value.is_null) {
        if (!column->is_nullable) {
            if (!adjust_missing_default) {
                set_bad_null_error(database, column->name);
                return MYLITE_ERROR;
            }
            rc = append_bad_null_warning(database, column->name);
            if (rc != MYLITE_OK) {
                return rc;
            }
            return make_insert_ignore_implicit_value(database, column, out_value);
        }
        *out_value = (struct planned_value){.is_null = true};
        return MYLITE_OK;
    }

    if (value.integer < 0) {
        is_negative = true;
        if (value.integer == INT64_MIN) {
            magnitude = int64_min_magnitude;
        } else {
            magnitude = (uint64_t)-value.integer;
        }
    } else {
        magnitude = (uint64_t)value.integer;
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

static int convert_update_unix_timestamp_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    static const struct unix_timestamp_arithmetic_messages messages = {
        .unsupported =
            "UPDATE UNIX_TIMESTAMP arithmetic supports only UNIX_TIMESTAMP(), "
            "UNIX_TIMESTAMP() +/- signed integer literals, and UNIX_TIMESTAMP() +/- NULL",
        .delta_range =
            "UPDATE UNIX_TIMESTAMP arithmetic supports only signed 64-bit integer deltas",
    };
    static const uint64_t int64_min_magnitude = 9223372036854775808ULL;
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    bool adjust_missing_default = dml_allows_missing_default_adjustment(database, ignore_errors);
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int rc = validate_update_unix_timestamp_arithmetic_assignment_target(database, column);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = evaluate_insert_unix_timestamp_arithmetic_value(database, value_node, &messages, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value.is_null) {
        return convert_null_insert_value(database, column, adjust_missing_default, out_value);
    }

    if (column_descriptor_is_string_family(column)) {
        return finish_insert_unix_timestamp_text_value(
            database,
            value.integer,
            column,
            row_number,
            dml_allows_string_truncation_adjustment(database, ignore_errors),
            out_value
        );
    }

    if (value.integer < 0) {
        is_negative = true;
        if (value.integer == INT64_MIN) {
            magnitude = int64_min_magnitude;
        } else {
            magnitude = (uint64_t)-value.integer;
        }
    } else {
        magnitude = (uint64_t)value.integer;
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

static int validate_update_constant_arithmetic_assignment_target(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    struct integer_column_range range = {0};
    int rc = integer_range_for_column(
        database,
        column,
        "UPDATE constant arithmetic assignment supports only integer columns",
        &range
    );

    (void)range;
    return rc;
}

static int convert_statement_time_value_for_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value,
    bool *out_handled
) {
    if (out_handled == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    *out_handled = true;

    switch (value_node->kind) {
    case MYLITE_SQL_AST_CURRENT_DATE_VALUE:
        if (column_descriptor_is_date(column)) {
            return make_current_date_value(database, out_value);
        }
        set_unsupported_error(database, "CURRENT_DATE values are supported only for DATE columns");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_CURRENT_TIME_VALUE:
        if (column_descriptor_is_time(column)) {
            return make_current_time_value(database, out_value);
        }
        set_unsupported_error(database, "CURRENT_TIME values are supported only for TIME columns");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE:
        if (column_descriptor_is_datetime(column) || column_descriptor_is_timestamp(column)) {
            return make_current_timestamp_value(database, out_value);
        }
        set_unsupported_error(
            database,
            "CURRENT_TIMESTAMP values are supported only for DATETIME and TIMESTAMP columns"
        );
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_SYSDATE_FUNCTION:
        if (column_descriptor_is_datetime(column) || column_descriptor_is_timestamp(column)) {
            return make_sysdate_value(database, out_value);
        }
        set_unsupported_error(
            database,
            "SYSDATE values are supported only for DATETIME and TIMESTAMP columns"
        );
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_UTC_DATE_VALUE:
        if (column_descriptor_is_date(column)) {
            return make_utc_date_value(database, out_value);
        }
        set_unsupported_error(database, "UTC_DATE values are supported only for DATE columns");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_UTC_TIME_VALUE:
        if (column_descriptor_is_time(column)) {
            return make_utc_time_value(database, out_value);
        }
        set_unsupported_error(database, "UTC_TIME values are supported only for TIME columns");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE:
        if (column_descriptor_is_datetime(column) || column_descriptor_is_timestamp(column)) {
            return make_utc_timestamp_value(database, out_value);
        }
        set_unsupported_error(
            database,
            "UTC_TIMESTAMP values are supported only for DATETIME and TIMESTAMP columns"
        );
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "CURRENT_TIMESTAMP");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "SYSDATE");
        return MYLITE_ERROR;
    default:
        *out_handled = false;
        return MYLITE_OK;
    }
}

static int convert_update_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    if (column_descriptor_is_char(column)) {
        return convert_char_literal(
            database,
            value_node,
            column,
            row_number,
            dml_allows_string_truncation_adjustment(database, ignore_errors),
            out_value
        );
    }
    if (column_descriptor_is_varchar(column)) {
        return convert_varchar_literal(
            database,
            value_node,
            column,
            row_number,
            dml_allows_string_truncation_adjustment(database, ignore_errors),
            out_value
        );
    }
    if (column_descriptor_is_text_family(column)) {
        return convert_text_family_literal(
            database,
            value_node,
            column,
            row_number,
            dml_allows_string_truncation_adjustment(database, ignore_errors),
            false,
            out_value
        );
    }
    if (column_descriptor_is_json(column)) {
        return convert_json_literal(database, value_node, column, out_value);
    }
    if (column_descriptor_is_enum(column)) {
        return convert_enum_literal(database, value_node, column, 1U, out_value);
    }
    if (column_descriptor_is_set(column)) {
        return convert_set_literal(database, value_node, column, 1U, out_value);
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
        return convert_update_date_literal(
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
        return convert_update_datetime_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_timestamp(column)) {
        return convert_update_timestamp_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_spatial(column)) {
        set_unsupported_error(database, "Spatial DML supports only NULL or DEFAULT values");
        return MYLITE_ERROR;
    }

    return convert_update_integer_literal(
        database,
        value_node,
        column,
        row_number,
        ignore_errors,
        out_value
    );
}

static int convert_update_date_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char normalized[date_text_length + 1U];
    char *copy = NULL;
    char *text = NULL;
    size_t text_length = 0U;
    struct temporal_predicate_normalization_input normalization = {0};
    enum temporal_storage_truncation truncation = TEMPORAL_STORAGE_TRUNCATION_NONE;
    int rc = MYLITE_OK;

    if (!ignore_errors || value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        return convert_date_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "DATE values support only string literals",
        "DATE values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    normalization.text = text;
    normalization.text_length = text_length;
    normalization.target_offset_minutes =
        database == NULL ? 0 : database->session.time_zone_offset_minutes;
    if (normalize_date_storage_text(&normalization, normalized, &truncation)) {
        copy = copy_temporal_text(database, normalized, date_text_length);
        if (copy == NULL) {
            free(text);
            return MYLITE_NOMEM;
        }
        rc = canonicalize_date_text(
            database,
            copy,
            date_text_length,
            column->name,
            row_number,
            ignore_errors,
            out_value
        );
        if (rc != MYLITE_OK) {
            free(text);
            return rc;
        }
        if (truncation == TEMPORAL_STORAGE_TRUNCATION_NOTE) {
            if (temporal_sql_mode_is_strict(database)) {
                rc = append_incorrect_date_value_note(database, text, column->name, row_number);
            } else {
                rc = append_data_truncated_note(database, column->name, row_number);
            }
        } else if (truncation == TEMPORAL_STORAGE_TRUNCATION_WARNING) {
            rc = append_data_truncated_warning(database, column->name, row_number);
        }
        free(text);
        return rc;
    }
    if (!date_text_has_canonical_shape(text, text_length)) {
        return make_date_zero_with_warning(database, text, column->name, row_number, out_value);
    }

    return canonicalize_date_text(
        database,
        text,
        text_length,
        column->name,
        row_number,
        ignore_errors,
        out_value
    );
}

static int convert_update_datetime_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char normalized[datetime_text_length + 1U];
    char *copy = NULL;
    char *text = NULL;
    size_t text_length = 0U;
    struct temporal_predicate_normalization_input normalization = {0};
    int rc = MYLITE_OK;

    if (!ignore_errors || value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        return convert_datetime_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "DATETIME values support only string literals",
        "DATETIME values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    normalization.text = text;
    normalization.text_length = text_length;
    normalization.target_offset_minutes =
        database == NULL ? 0 : database->session.time_zone_offset_minutes;
    if (normalize_iso_temporal_predicate_text(&normalization, normalized)) {
        free(text);
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            return MYLITE_NOMEM;
        }
        return canonicalize_datetime_text(
            database,
            copy,
            datetime_text_length,
            column->name,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (normalize_z_temporal_predicate_text(text, text_length, normalized)) {
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            free(text);
            return MYLITE_NOMEM;
        }
        rc = canonicalize_datetime_text(
            database,
            copy,
            datetime_text_length,
            column->name,
            row_number,
            ignore_errors,
            out_value
        );
        if (rc == MYLITE_OK) {
            rc = append_data_truncated_warning(database, column->name, row_number);
        }
        free(text);
        return rc;
    }
    if (!datetime_text_has_canonical_shape(text, text_length)) {
        return make_datetime_zero_with_warning(database, text, column->name, row_number, out_value);
    }

    return canonicalize_datetime_text(
        database,
        text,
        text_length,
        column->name,
        row_number,
        ignore_errors,
        out_value
    );
}

static int convert_update_timestamp_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char normalized[datetime_text_length + 1U];
    char *copy = NULL;
    char *text = NULL;
    size_t text_length = 0U;
    struct temporal_predicate_normalization_input normalization = {0};
    int rc = MYLITE_OK;

    if (!ignore_errors || value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        return convert_timestamp_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "TIMESTAMP values support only string literals",
        "TIMESTAMP values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    normalization.text = text;
    normalization.text_length = text_length;
    normalization.target_offset_minutes = 0;
    if (normalize_iso_temporal_predicate_text(&normalization, normalized)) {
        free(text);
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            return MYLITE_NOMEM;
        }
        return canonicalize_timestamp_text(
            database,
            copy,
            datetime_text_length,
            column->name,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (normalize_z_temporal_predicate_text(text, text_length, normalized)) {
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            free(text);
            return MYLITE_NOMEM;
        }
        rc = canonicalize_timestamp_text(
            database,
            copy,
            datetime_text_length,
            column->name,
            row_number,
            ignore_errors,
            out_value
        );
        if (rc == MYLITE_OK) {
            rc = append_data_truncated_warning(database, column->name, row_number);
        }
        free(text);
        return rc;
    }
    if (!datetime_text_has_canonical_shape(text, text_length)) {
        return make_timestamp_zero_with_warning(
            database,
            text,
            column->name,
            row_number,
            out_value
        );
    }

    return canonicalize_timestamp_text(
        database,
        text,
        text_length,
        column->name,
        row_number,
        ignore_errors,
        out_value
    );
}

static int materialize_update_scalar_subquery_value(
    struct mylite_db *database,
    const struct planned_update *plan,
    struct planned_value *out_value
) {
    sqlite3_stmt *statement = NULL;
    struct planned_select scalar_plan = plan->assignment_subquery;
    char *sql = NULL;
    int rc = MYLITE_OK;

    if (!plan->assignment_value_is_scalar_subquery ||
        plan->assignment_subquery.column_count != 1U) {
        set_unsupported_error(
            database,
            "UPDATE scalar subquery assignment supports only one descriptor source column"
        );
        return MYLITE_ERROR;
    }

    cap_update_scalar_subquery_limit(&scalar_plan.limit);
    rc = build_select_sql(&scalar_plan, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_parameters(statement, &scalar_plan);
    }
    if (rc == MYLITE_OK) {
        rc = step_update_scalar_subquery_value(database, statement, plan, out_value);
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }

    return rc;
}

static void cap_update_scalar_subquery_limit(struct planned_select_limit *limit) {
    if (limit == NULL) {
        return;
    }
    if (!limit->has_limit) {
        *limit = (struct planned_select_limit){
            .has_limit = true,
            .row_count = 2,
            .has_offset = false,
            .offset = 0,
        };
        return;
    }
    if (limit->row_count > 2) {
        limit->row_count = 2;
    }
}

static int step_update_scalar_subquery_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_update *plan,
    struct planned_value *out_value
) {
    int sqlite_rc = sqlite3_step(statement);
    int rc = MYLITE_OK;

    if (sqlite_rc == SQLITE_DONE) {
        return convert_null_insert_value(
            database,
            &plan->assignment_column,
            plan->ignore_errors,
            out_value
        );
    }
    if (sqlite_rc != SQLITE_ROW) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    rc = materialize_update_scalar_subquery_sqlite_value(database, statement, plan, out_value);
    if (rc != MYLITE_OK) {
        return rc;
    }

    sqlite_rc = sqlite3_step(statement);
    if (sqlite_rc == SQLITE_ROW) {
        set_scalar_subquery_row_count_error(database);
        return MYLITE_ERROR;
    }
    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int materialize_update_scalar_subquery_sqlite_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_update *plan,
    struct planned_value *out_value
) {
    const struct mylite_catalog_column_descriptor *source_column =
        &plan->assignment_subquery.columns[0];
    const struct mylite_catalog_column_descriptor *target_column = &plan->assignment_column;
    int sqlite_type = sqlite3_column_type(statement, 0);

    if (sqlite_type == SQLITE_NULL) {
        return convert_null_insert_value(database, target_column, plan->ignore_errors, out_value);
    }
    if (!update_scalar_subquery_source_target_types_are_compatible(source_column, target_column)) {
        set_unsupported_error(
            database,
            update_scalar_subquery_implicit_conversion_message(target_column)
        );
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_approximate(target_column)) {
        return materialize_update_scalar_subquery_approximate_value(
            database,
            statement,
            target_column,
            out_value
        );
    }
    if (update_scalar_subquery_target_uses_text_storage(target_column)) {
        return materialize_update_scalar_subquery_text_storage_value(
            database,
            statement,
            target_column,
            out_value
        );
    }
    if (update_scalar_subquery_target_uses_blob_storage(target_column)) {
        return materialize_update_scalar_subquery_blob_storage_value(
            database,
            statement,
            target_column,
            out_value
        );
    }

    return materialize_update_scalar_subquery_integer_value(
        database,
        statement,
        sqlite_type,
        target_column,
        out_value
    );
}

static bool update_scalar_subquery_target_uses_text_storage(
    const struct mylite_catalog_column_descriptor *target_column
) {
    if (column_descriptor_is_string_family(target_column)) {
        return true;
    }
    if (column_descriptor_is_json(target_column)) {
        return true;
    }
    if (column_descriptor_is_decimal(target_column)) {
        return true;
    }
    if (column_descriptor_is_date(target_column)) {
        return true;
    }
    if (column_descriptor_is_time(target_column)) {
        return true;
    }
    if (column_descriptor_is_datetime(target_column)) {
        return true;
    }
    if (column_descriptor_is_timestamp(target_column)) {
        return true;
    }
    if (column_descriptor_is_year(target_column)) {
        return true;
    }
    return false;
}

static bool update_scalar_subquery_target_uses_blob_storage(
    const struct mylite_catalog_column_descriptor *target_column
) {
    return (column_descriptor_is_binary_string_family(target_column) ||
            column_descriptor_is_bit(target_column)) != 0;
}

static int materialize_update_scalar_subquery_text_storage_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct mylite_catalog_column_descriptor *target_column,
    struct planned_value *out_value
) {
    int rc = validate_update_scalar_subquery_text_storage_value(database, statement, target_column);

    if (rc == MYLITE_OK) {
        rc = copy_update_scalar_subquery_text_value(database, statement, 0, out_value);
    }
    return rc;
}

static int materialize_update_scalar_subquery_blob_storage_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct mylite_catalog_column_descriptor *target_column,
    struct planned_value *out_value
) {
    if (column_descriptor_is_bit(target_column)) {
        return materialize_sqlite_bit_value(database, statement, 0, target_column, 1U, out_value);
    }
    return materialize_sqlite_binary_string_value(
        database,
        statement,
        0,
        target_column,
        1U,
        out_value
    );
}

static int validate_update_scalar_subquery_text_storage_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct mylite_catalog_column_descriptor *target_column
) {
    if (column_descriptor_is_string_family(target_column)) {
        return validate_insert_select_string_value(database, statement, 0, target_column, 1U);
    }
    if (column_descriptor_is_json(target_column)) {
        return validate_insert_select_json_value(database, statement, 0, target_column);
    }
    if (column_descriptor_is_decimal(target_column)) {
        return validate_insert_select_decimal_value(database, statement, 0, target_column, 1U);
    }
    if (column_descriptor_is_date(target_column)) {
        return validate_insert_select_date_value(database, statement, 0, target_column, 1U);
    }
    if (column_descriptor_is_time(target_column)) {
        return validate_insert_select_time_value(database, statement, 0, target_column, 1U);
    }
    if (column_descriptor_is_datetime(target_column)) {
        return validate_insert_select_datetime_value(database, statement, 0, target_column, 1U);
    }
    if (column_descriptor_is_year(target_column)) {
        return validate_insert_select_year_value(database, statement, 0, target_column, 1U);
    }
    return validate_insert_select_timestamp_value(database, statement, 0, target_column, 1U);
}

static int materialize_update_scalar_subquery_approximate_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct mylite_catalog_column_descriptor *target_column,
    struct planned_value *out_value
) {
    int rc = validate_insert_select_approximate_value(database, statement, 0, target_column, 1U);

    if (rc == MYLITE_OK) {
        *out_value = (struct planned_value){
            .is_null = false,
            .is_real = true,
            .real = sqlite3_column_double(statement, 0),
        };
    }
    return rc;
}

static int materialize_update_scalar_subquery_integer_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int sqlite_type,
    const struct mylite_catalog_column_descriptor *target_column,
    struct planned_value *out_value
) {
    int rc = MYLITE_OK;

    if (sqlite_type != SQLITE_INTEGER) {
        set_unsupported_error(
            database,
            "UPDATE scalar subquery assignment does not support implicit integer conversion"
        );
        return MYLITE_ERROR;
    }

    rc = validate_insert_select_integer_value(database, statement, 0, target_column, 1U, false);
    if (rc == MYLITE_OK) {
        *out_value = (struct planned_value){
            .is_null = false,
            .integer = (int64_t)sqlite3_column_int64(statement, 0),
        };
    }
    return rc;
}

static int copy_update_scalar_subquery_text_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    struct planned_value *out_value
) {
    const unsigned char *text = sqlite3_column_text(statement, selected_column_index);
    int byte_count = sqlite3_column_bytes(statement, selected_column_index);
    char *copy = NULL;

    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    copy = malloc((size_t)byte_count + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(copy, text, (size_t)byte_count);
    copy[(size_t)byte_count] = '\0';

    return assign_text_value(database, copy, (size_t)byte_count, out_value);
}

static bool update_scalar_subquery_source_target_types_are_compatible(
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column
) {
    return insert_select_source_target_types_are_compatible(source_column, target_column);
}

static const char *update_scalar_subquery_implicit_conversion_message(
    const struct mylite_catalog_column_descriptor *target_column
) {
    if (column_descriptor_is_string_family(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit string conversion";
    }
    if (column_descriptor_is_json(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit JSON conversion";
    }
    if (column_descriptor_is_enum(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit ENUM conversion";
    }
    if (column_descriptor_is_set(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit SET conversion";
    }
    if (column_descriptor_is_binary_string_family(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit binary string "
               "conversion";
    }
    if (column_descriptor_is_bit(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit BIT conversion";
    }
    if (column_descriptor_is_decimal(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit DECIMAL conversion";
    }
    if (column_descriptor_is_approximate(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit FLOAT or DOUBLE "
               "conversion";
    }
    if (column_descriptor_is_date(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit DATE conversion";
    }
    if (column_descriptor_is_time(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit TIME conversion";
    }
    if (column_descriptor_is_datetime(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit DATETIME conversion";
    }
    if (column_descriptor_is_timestamp(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit TIMESTAMP conversion";
    }
    if (column_descriptor_is_year(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit YEAR conversion";
    }

    return "UPDATE scalar subquery assignment does not support implicit integer conversion";
}

static int convert_update_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "UPDATE supports only integer, boolean, NULL, and DEFAULT assignment values"
            );
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (!boolean_literal_magnitude(literal, &magnitude)) {
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            (mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER &&
             mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_STRING)) {
            set_unsupported_error(
                database,
                "UPDATE supports only integer, boolean, NULL, and DEFAULT assignment values"
            );
            return MYLITE_ERROR;
        }
        if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
            return convert_integer_string_literal(
                database,
                literal,
                column,
                row_number,
                ignore_errors,
                out_value
            );
        }

        rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
        if (rc != MYLITE_OK) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
    }

    out_value->is_null = false;
    rc = convert_integer_for_column_with_policy(
        database,
        magnitude,
        is_negative,
        column,
        row_number,
        ignore_errors,
        &out_value->integer
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return MYLITE_OK;
}

static int plan_update_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
) {
    int rc = MYLITE_OK;

    out_limit->has_limit = false;
    out_limit->row_count = 0;
    out_limit->has_offset = false;
    out_limit->offset = 0;
    if (limit_clause == NULL) {
        return MYLITE_OK;
    }
    if (limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE) {
        set_unsupported_error(database, "UPDATE supports only LIMIT row_count");
        return MYLITE_ERROR;
    }
    if (child_at(limit_clause, 1U) != NULL) {
        set_unsupported_error(database, "UPDATE supports only LIMIT row_count");
        return MYLITE_ERROR;
    }

    rc = convert_limit_integer_literal(database, child_at(limit_clause, 0U), &out_limit->row_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_limit->has_limit = true;

    return MYLITE_OK;
}

static int convert_limit_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    int64_t *out_value
) {
    const uint64_t sqlite_int64_positive_max = 9223372036854775807ULL;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(database, "LIMIT supports only unsigned decimal integer literals");
        return MYLITE_ERROR;
    }

    rc = parse_unsigned_integer_literal(&value_node->span, &magnitude);
    if (rc != MYLITE_OK || magnitude > sqlite_int64_positive_max) {
        set_limit_out_of_range_error(database);
        return MYLITE_ERROR;
    }

    *out_value = (int64_t)magnitude;
    return MYLITE_OK;
}

static int integer_range_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const char *unsupported_message,
    struct integer_column_range *out_range
) {
    return integer_range_for_logical_type(
        database,
        (struct integer_logical_type_range_request){
            .logical_type = column->logical_type,
            .unsupported_message = unsupported_message,
        },
        out_range
    );
}

static int integer_range_for_logical_type(
    struct mylite_db *database,
    struct integer_logical_type_range_request request,
    struct integer_column_range *out_range
) {
    const uint64_t int_signed_positive_max = 2147483647ULL;
    const uint64_t int_signed_negative_abs_max = 2147483648ULL;
    const uint64_t int_unsigned_max = 4294967295ULL;
    const uint64_t mediumint_signed_positive_max = 8388607ULL;
    const uint64_t mediumint_signed_negative_abs_max = 8388608ULL;
    const uint64_t mediumint_unsigned_max = 16777215ULL;
    const uint64_t smallint_signed_positive_max = 32767ULL;
    const uint64_t smallint_signed_negative_abs_max = 32768ULL;
    const uint64_t smallint_unsigned_max = 65535ULL;
    const uint64_t tinyint_signed_positive_max = 127ULL;
    const uint64_t tinyint_signed_negative_abs_max = 128ULL;
    const uint64_t tinyint_unsigned_max = 255ULL;
    const uint64_t bigint_signed_positive_max = 9223372036854775807ULL;
    const uint64_t bigint_signed_negative_abs_max = 9223372036854775808ULL;

    if (strcmp(request.logical_type, "TINYINT") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = tinyint_signed_positive_max,
            .negative_abs_max = tinyint_signed_negative_abs_max,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "TINYINT(1)") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = tinyint_signed_positive_max,
            .negative_abs_max = tinyint_signed_negative_abs_max,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "TINYINT UNSIGNED") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = tinyint_unsigned_max,
            .negative_abs_max = 0U,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "SMALLINT") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = smallint_signed_positive_max,
            .negative_abs_max = smallint_signed_negative_abs_max,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "SMALLINT UNSIGNED") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = smallint_unsigned_max,
            .negative_abs_max = 0U,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "MEDIUMINT") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = mediumint_signed_positive_max,
            .negative_abs_max = mediumint_signed_negative_abs_max,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "MEDIUMINT UNSIGNED") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = mediumint_unsigned_max,
            .negative_abs_max = 0U,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "INT") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = int_signed_positive_max,
            .negative_abs_max = int_signed_negative_abs_max,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "INT UNSIGNED") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = int_unsigned_max,
            .negative_abs_max = 0U,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "BIGINT") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = bigint_signed_positive_max,
            .negative_abs_max = bigint_signed_negative_abs_max,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "BIGINT UNSIGNED") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = bigint_signed_positive_max,
            .negative_abs_max = 0U,
        };
        return MYLITE_OK;
    }

    if (database != NULL) {
        set_unsupported_error(database, request.unsupported_message);
    }
    return MYLITE_ERROR;
}
