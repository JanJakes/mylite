static bool generic_spatial_function_kind(
    const struct mylite_sql_ast_node *expression,
    enum mylite_spatial_function_kind *out_kind
) {
    const struct mylite_sql_ast_node *schema_identifier = NULL;
    const struct mylite_sql_ast_node *function_identifier = NULL;
    enum mylite_spatial_function_kind kind = MYLITE_SPATIAL_FUNCTION_NONE;

    if (out_kind != NULL) {
        *out_kind = MYLITE_SPATIAL_FUNCTION_NONE;
    }
    if (generic_function_identifier_nodes(
            expression,
            &schema_identifier,
            &function_identifier
        ) != MYLITE_OK ||
        schema_identifier != NULL || function_identifier == NULL) {
        return false;
    }
    if (!mylite_spatial_function_kind_from_name(
            function_identifier->span.text,
            function_identifier->span.length,
            &kind
        )) {
        return false;
    }
    if (out_kind != NULL) {
        *out_kind = kind;
    }
    return true;
}

static bool expression_contains_spatial_function(const struct mylite_sql_ast_node *expression) {
    struct scalar_arithmetic_node_stack stack = {0};
    bool found = false;

    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        return false;
    }
    while (stack.count != 0U && !found) {
        const struct mylite_sql_ast_node *current =
            unwrap_parenthesized_expression(stack.items[--stack.count]);

        if (current == NULL) {
            continue;
        }
        if (generic_spatial_function_kind(current, NULL)) {
            found = true;
            break;
        }
        if (current->kind == MYLITE_SQL_AST_SCALAR_SUBQUERY) {
            continue;
        }
        if (!scalar_arithmetic_node_stack_push_children(&stack, current)) {
            scalar_arithmetic_node_stack_deinit(&stack);
            return false;
        }
    }
    scalar_arithmetic_node_stack_deinit(&stack);
    return found;
}

static int spatial_function_session_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *arguments_node = generic_system_function_arguments(expression);
    enum mylite_spatial_function_kind kind = MYLITE_SPATIAL_FUNCTION_NONE;
    struct session_scalar_cell *cells = NULL;
    struct mylite_spatial_argument *arguments = NULL;
    struct mylite_spatial_result result = {0};
    struct mylite_spatial_error error = {0};
    size_t argument_count =
        arguments_node == NULL ? 0U : mylite_sql_ast_node_child_count(arguments_node);
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (!generic_spatial_function_kind(expression, &kind)) {
        set_unsupported_error(database, "unsupported spatial function");
        return MYLITE_ERROR;
    }
    if (argument_count != 0U) {
        cells = (struct session_scalar_cell *)calloc(argument_count, sizeof(*cells));
        arguments = (struct mylite_spatial_argument *)calloc(argument_count, sizeof(*arguments));
        if (cells == NULL || arguments == NULL) {
            free(cells);
            free(arguments);
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < argument_count; ++index) {
        rc = spatial_function_session_scalar_argument(
            database,
            child_at(arguments_node, index),
            &cells[index],
            &arguments[index]
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_spatial_evaluate(kind, arguments, argument_count, &result, &error);
    }
    if (rc == MYLITE_OK) {
        rc = spatial_function_session_scalar_result(database, &result, out_cell);
    } else if (error.is_nomem) {
        set_nomem_error(database);
        rc = MYLITE_NOMEM;
    } else if (error.message[0] != '\0') {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            error.code,
            error.sqlstate,
            error.message
        );
        rc = MYLITE_ERROR;
    }

    mylite_spatial_result_deinit(&result);
    session_scalar_cell_array_deinit(cells, argument_count);
    free(arguments);
    return rc;
}

static int spatial_function_session_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    struct mylite_spatial_argument *out_argument
) {
    int rc = MYLITE_OK;

    if (out_cell == NULL || out_argument == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    *out_argument = (struct mylite_spatial_argument){0};
    if (spatial_function_numeric_literal_expression(expression)) {
        return spatial_function_numeric_literal_argument(
            database,
            expression,
            out_cell,
            out_argument
        );
    }

    rc = session_scalar_value(database, expression, out_cell);
    if (rc == MYLITE_OK) {
        *out_argument = (struct mylite_spatial_argument){
            .bytes = out_cell->value,
            .byte_count = scalar_cell_text_length(out_cell),
            .is_null = out_cell->value == NULL,
            .has_numeric = false,
        };
    }
    return rc;
}

static bool spatial_function_numeric_literal_expression(
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(expression);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

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
    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    return literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
           literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL ||
           literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT;
}

static int spatial_function_numeric_literal_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    struct mylite_spatial_argument *out_argument
) {
    const struct mylite_sql_ast_node *numeric = unwrap_parenthesized_expression(expression);
    const struct mylite_sql_ast_node *literal = numeric;
    char *text = NULL;
    char *parse_end = NULL;
    double value = 0.0;
    size_t text_length = 0U;
    bool is_negative = false;

    if (database == NULL || numeric == NULL || out_cell == NULL || out_argument == NULL) {
        return MYLITE_MISUSE;
    }
    if (numeric->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(numeric);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_runtime_error(database, "failed to parse spatial numeric literal");
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(numeric, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_runtime_error(database, "failed to parse spatial numeric literal");
        return MYLITE_ERROR;
    }
    if (literal->span.length > SIZE_MAX - (is_negative ? 2U : 1U)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    text_length = literal->span.length + (is_negative ? 1U : 0U);
    text = (char *)malloc(text_length + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (is_negative) {
        text[0] = '-';
        if (literal->span.length != 0U) {
            memcpy(text + 1U, literal->span.text, literal->span.length);
        }
    } else if (literal->span.length != 0U) {
        memcpy(text, literal->span.text, literal->span.length);
    }
    text[text_length] = '\0';

    errno = 0;
    if (mylite_execution_parse_c_locale_double(text, &parse_end, &value) != MYLITE_OK ||
        parse_end == text) {
        free(text);
        set_runtime_error(database, "failed to parse spatial numeric literal");
        return MYLITE_ERROR;
    }
    while (parse_end != NULL && *parse_end != '\0' && isspace((unsigned char)*parse_end)) {
        ++parse_end;
    }
    if (parse_end == NULL || *parse_end != '\0') {
        free(text);
        set_runtime_error(database, "failed to parse spatial numeric literal");
        return MYLITE_ERROR;
    }

    out_cell->owned_text = text;
    out_cell->value = text;
    out_cell->value_size = text_length;
    out_cell->has_value_size = true;
    *out_argument = (struct mylite_spatial_argument){
        .bytes = out_cell->value,
        .byte_count = out_cell->value_size,
        .is_null = false,
        .has_numeric = true,
        .numeric = value,
    };
    return MYLITE_OK;
}

static int spatial_function_session_scalar_result(
    struct mylite_db *database,
    const struct mylite_spatial_result *result,
    struct session_scalar_cell *out_cell
) {
    char *copy = NULL;
    int written = 0;

    if (result == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    switch (result->kind) {
    case MYLITE_SPATIAL_RESULT_NULL:
        return MYLITE_OK;
    case MYLITE_SPATIAL_RESULT_INTEGER:
        written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRId64,
            result->integer
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            set_runtime_error(database, "failed to format spatial integer result");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    case MYLITE_SPATIAL_RESULT_DOUBLE:
        if (mylite_execution_format_double_text(
                database,
                result->real,
                "spatial",
                out_cell->double_text,
                sizeof(out_cell->double_text)
            ) != MYLITE_OK) {
            set_runtime_error(database, "failed to format spatial double result");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->double_text;
        return MYLITE_OK;
    case MYLITE_SPATIAL_RESULT_GEOMETRY:
    case MYLITE_SPATIAL_RESULT_TEXT:
    case MYLITE_SPATIAL_RESULT_BLOB:
        if (result->byte_count == SIZE_MAX) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        copy = (char *)malloc(result->byte_count + 1U);
        if (copy == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        memcpy(copy, result->bytes, result->byte_count);
        copy[result->byte_count] = '\0';
        return assign_scalar_cell_owned_text(database, out_cell, copy, result->byte_count);
    }
    set_runtime_error(database, "unknown spatial result kind");
    return MYLITE_ERROR;
}

static int populate_spatial_function_result_column_descriptor(
    struct mylite_db *database,
    enum mylite_spatial_function_kind kind,
    struct mylite_result_column_descriptor *descriptor
) {
    if (database == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    if (mylite_spatial_function_returns_geometry(kind)) {
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_GEOMETRY;
        descriptor->charset_id = mysql_collation_binary_id;
        descriptor->collation_id = mysql_collation_binary_id;
        descriptor->display_length = mysql_json_document_display_length;
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_BLOB;
        descriptor->nullable = true;
        return MYLITE_OK;
    }
    if (mylite_spatial_function_returns_text(kind)) {
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_json_document_display_length,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    }
    if (mylite_spatial_function_returns_wkb(kind)) {
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_BLOB;
        descriptor->charset_id = mysql_collation_binary_id;
        descriptor->collation_id = mysql_collation_binary_id;
        descriptor->display_length = mysql_json_document_display_length;
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_BLOB;
        descriptor->nullable = true;
        return MYLITE_OK;
    }
    if (mylite_spatial_function_returns_integer(kind)) {
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    }
    if (mylite_spatial_function_returns_double(kind)) {
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_DOUBLE,
                .display_length = mysql_scalar_double_display_length,
                .decimals = mysql_approximate_decimals,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    }
    return MYLITE_OK;
}
