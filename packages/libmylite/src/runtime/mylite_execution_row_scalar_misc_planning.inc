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
            mylite_execution_is_temporal_extract_function_kind(current->kind) ||
            mylite_execution_is_string_length_function_kind(current->kind) ||
            mylite_execution_is_string_codepoint_function_kind(current->kind) ||
            mylite_execution_is_string_case_function_kind(current->kind) ||
            mylite_execution_is_string_trim_function_kind(current->kind) ||
            mylite_execution_is_string_slice_function_kind(current->kind) ||
            mylite_execution_is_string_padding_function_kind(current->kind) ||
            mylite_execution_is_string_bitmask_function_kind(current->kind) ||
            mylite_execution_is_string_search_function_kind(current->kind) ||
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
            mylite_execution_is_charset_collation_function_kind(current->kind)) {
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
        if (mylite_execution_is_date_interval_second_function_kind(current->kind)) {
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
