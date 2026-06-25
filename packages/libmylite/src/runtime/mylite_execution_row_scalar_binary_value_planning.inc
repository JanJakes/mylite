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
    rc = mylite_execution_scalar_hex_function_value(database, expression, &cell);
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
    rc = mylite_execution_scalar_unhex_function_value(database, expression, &cell);
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
        mylite_execution_scalar_set_base64_argument_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    rc = expression->kind == MYLITE_SQL_AST_TO_BASE64_FUNCTION
             ? mylite_execution_scalar_to_base64_function_value(database, expression, &cell)
             : mylite_execution_scalar_from_base64_function_value(database, expression, &cell);
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

static int plan_row_scalar_registered_one_argument_expression(
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
    enum planned_row_scalar_expression_kind expression_kind =
        PLANNED_ROW_SCALAR_EXPRESSION_NONE;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "native");
        return MYLITE_ERROR;
    }

    function_name = registered_one_argument_function_name(expression->kind);
    expression_kind = registered_one_argument_expression_kind(expression->kind);
    if (function_name == NULL || expression_kind == PLANNED_ROW_SCALAR_EXPRESSION_NONE) {
        set_unsupported_error(database, "registered one-argument function is unsupported");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    out_expression->kind = expression_kind;
    out_expression->argument_count = 1U;
    rc = plan_row_scalar_non_concat_expression(
        database,
        argument,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        false,
        &out_expression->arguments[0]
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(out_expression);
    }
    return rc;
}

static enum planned_row_scalar_expression_kind registered_one_argument_expression_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_COMPRESS_FUNCTION:
        return PLANNED_ROW_SCALAR_EXPRESSION_COMPRESS;
    case MYLITE_SQL_AST_UNCOMPRESS_FUNCTION:
        return PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESS;
    case MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_FUNCTION:
        return PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESSED_LENGTH;
    case MYLITE_SQL_AST_RANDOM_BYTES_FUNCTION:
        return PLANNED_ROW_SCALAR_EXPRESSION_RANDOM_BYTES;
    case MYLITE_SQL_AST_INET_ATON_FUNCTION:
        return PLANNED_ROW_SCALAR_EXPRESSION_INET_ATON;
    case MYLITE_SQL_AST_INET_NTOA_FUNCTION:
        return PLANNED_ROW_SCALAR_EXPRESSION_INET_NTOA;
    case MYLITE_SQL_AST_LAST_INSERT_ID_SET_FUNCTION:
        return PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_SET;
    default:
        break;
    }
    return PLANNED_ROW_SCALAR_EXPRESSION_NONE;
}

static const char *registered_one_argument_function_name(enum mylite_sql_ast_node_kind ast_kind) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_COMPRESS_FUNCTION:
        return "COMPRESS";
    case MYLITE_SQL_AST_UNCOMPRESS_FUNCTION:
        return "UNCOMPRESS";
    case MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_FUNCTION:
        return "UNCOMPRESSED_LENGTH";
    case MYLITE_SQL_AST_RANDOM_BYTES_FUNCTION:
        return "RANDOM_BYTES";
    case MYLITE_SQL_AST_INET_ATON_FUNCTION:
        return "INET_ATON";
    case MYLITE_SQL_AST_INET_NTOA_FUNCTION:
        return "INET_NTOA";
    case MYLITE_SQL_AST_LAST_INSERT_ID_SET_FUNCTION:
        return "LAST_INSERT_ID";
    default:
        break;
    }
    return NULL;
}

static int plan_row_scalar_digest_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *argument = NULL;
    const struct mylite_sql_ast_node *length_argument = NULL;
    struct row_scalar_digest_ast_arguments digest_arguments = {0};
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    rc = validate_row_scalar_digest_expression_arity(database, expression);
    if (rc != MYLITE_OK) {
        return rc;
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (expression->kind == MYLITE_SQL_AST_SHA2_FUNCTION) {
        length_argument = unwrap_parenthesized_expression(child_at(expression, 1U));
    }
    if (argument != NULL && (argument->kind == MYLITE_SQL_AST_IDENTIFIER ||
                             argument->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        if (!has_source) {
            return mylite_execution_set_unknown_column_reference_error(database, argument);
        }
        digest_arguments.function_expression = expression;
        digest_arguments.argument = argument;
        digest_arguments.length_argument = length_argument;
        return plan_row_scalar_digest_column_expression(
            database,
            &digest_arguments,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }

    return plan_row_scalar_digest_constant_expression(database, expression, out_expression);
}

static int validate_row_scalar_digest_expression_arity(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    size_t child_count = 0U;
    const char *function_name = row_scalar_digest_error_function_name(expression);

    if (function_name == NULL || expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_MD5_ARGUMENT_COUNT_ERROR ||
        expression->kind == MYLITE_SQL_AST_SHA_ARGUMENT_COUNT_ERROR ||
        expression->kind == MYLITE_SQL_AST_SHA1_ARGUMENT_COUNT_ERROR ||
        expression->kind == MYLITE_SQL_AST_SHA2_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }

    child_count = mylite_sql_ast_node_child_count(expression);
    if (expression->kind == MYLITE_SQL_AST_SHA2_FUNCTION) {
        if (child_count != 2U) {
            set_native_function_parameter_count_error(database, function_name);
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }
    if (child_count != 1U) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int plan_row_scalar_digest_column_expression(
    struct mylite_db *database,
    const struct row_scalar_digest_ast_arguments *arguments,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    size_t source_index = 0U;
    bool length_is_null = false;
    int rc = MYLITE_OK;

    if (arguments == NULL || arguments->function_expression == NULL ||
        arguments->argument == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = resolve_descriptor_column_reference_with_source_index(
        database,
        arguments->argument,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT digest functions support only descriptor columns",
        table_columns,
        table_column_count,
        &column,
        &source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!digest_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->argument_count =
        arguments->function_expression->kind == MYLITE_SQL_AST_SHA2_FUNCTION ? 2U : 1U;
    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(out_expression->argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    if (arguments->function_expression->kind == MYLITE_SQL_AST_SHA2_FUNCTION) {
        rc = plan_row_scalar_sha2_length_argument(
            database,
            arguments->length_argument,
            &out_expression->arguments[1],
            &length_is_null
        );
        if (rc != MYLITE_OK) {
            planned_row_scalar_expression_deinit(out_expression);
            return rc;
        }
        if (length_is_null) {
            planned_row_scalar_expression_deinit(out_expression);
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
            return MYLITE_OK;
        }
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_DIGEST;
    out_expression->digest_kind =
        row_scalar_digest_kind_from_ast(arguments->function_expression->kind);
    out_expression->arguments[0].kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->arguments[0].column = column;
    out_expression->arguments[0].column_has_source_index =
        select_source_context_is_joined(source_context);
    out_expression->arguments[0].column_source_index = source_index;
    return MYLITE_OK;
}

static int plan_row_scalar_digest_constant_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    struct session_scalar_cell cell = {0};
    int rc = MYLITE_OK;

    if (!is_digest_projection_expression(expression)) {
        set_unsupported_error(
            database,
            "digest functions support only string, hex, integer, boolean, NULL, supported "
            "session scalar, and descriptor column arguments"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_execution_scalar_digest_function_value(database, expression, &cell);
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

static bool digest_column_descriptor_is_supported(
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

    set_unsupported_error(
        database,
        "digest functions support only integer, nonbinary string, and binary string columns"
    );
    return false;
}

static int plan_row_scalar_sha2_length_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    struct planned_row_scalar_expression *out_expression,
    bool *out_is_null
) {
    enum mylite_digest_algorithm algorithm = MYLITE_DIGEST_ALGORITHM_SHA256;
    int64_t value = 0;

    if (out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_is_null = false;
    if (!sha2_length_literal_algorithm(argument, &algorithm, &value, out_is_null)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT SHA2() supports only hash lengths 224, 256, 384, 512, 0, "
            "FALSE, or NULL"
        );
        return MYLITE_ERROR;
    }
    if (*out_is_null) {
        return MYLITE_OK;
    }

    (void)algorithm;
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    out_expression->value = (struct planned_value){.is_null = false, .integer = value};
    return MYLITE_OK;
}

static bool sha2_length_literal_algorithm(
    const struct mylite_sql_ast_node *literal,
    enum mylite_digest_algorithm *out_algorithm,
    int64_t *out_value,
    bool *out_is_null
) {
    bool is_negative = false;
    uint64_t magnitude = 0U;

    literal = unwrap_parenthesized_expression(literal);
    if (literal == NULL || out_algorithm == NULL || out_value == NULL || out_is_null == NULL) {
        return false;
    }
    *out_is_null = false;
    if (literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(literal);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            return false;
        }
        literal = unwrap_parenthesized_expression(child_at(literal, 0U));
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            return false;
        }
    }
    if (literal->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }

    switch (mylite_sql_ast_node_literal_kind(literal)) {
    case MYLITE_SQL_AST_LITERAL_NULL:
        *out_is_null = true;
        return true;
    case MYLITE_SQL_AST_LITERAL_FALSE:
        return sha2_length_value_from_magnitude(0U, false, out_algorithm, out_value);
    case MYLITE_SQL_AST_LITERAL_TRUE:
        return false;
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
            return false;
        }
        return sha2_length_value_from_magnitude(
            magnitude,
            is_negative,
            out_algorithm,
            out_value
        );
    default:
        break;
    }
    return false;
}

static bool sha2_length_value_from_magnitude(
    uint64_t magnitude,
    bool is_negative,
    enum mylite_digest_algorithm *out_algorithm,
    int64_t *out_value
) {
    if (is_negative || out_algorithm == NULL || out_value == NULL) {
        return false;
    }
    switch (magnitude) {
    case MYLITE_DIGEST_SHA2_LENGTH_DEFAULT:
        *out_algorithm = MYLITE_DIGEST_ALGORITHM_SHA256;
        *out_value = MYLITE_DIGEST_SHA2_LENGTH_DEFAULT;
        return true;
    case MYLITE_DIGEST_SHA2_LENGTH_SHA224:
        *out_algorithm = MYLITE_DIGEST_ALGORITHM_SHA224;
        *out_value = MYLITE_DIGEST_SHA2_LENGTH_SHA224;
        return true;
    case MYLITE_DIGEST_SHA2_LENGTH_SHA256:
        *out_algorithm = MYLITE_DIGEST_ALGORITHM_SHA256;
        *out_value = MYLITE_DIGEST_SHA2_LENGTH_SHA256;
        return true;
    case MYLITE_DIGEST_SHA2_LENGTH_SHA384:
        *out_algorithm = MYLITE_DIGEST_ALGORITHM_SHA384;
        *out_value = MYLITE_DIGEST_SHA2_LENGTH_SHA384;
        return true;
    case MYLITE_DIGEST_SHA2_LENGTH_SHA512:
        *out_algorithm = MYLITE_DIGEST_ALGORITHM_SHA512;
        *out_value = MYLITE_DIGEST_SHA2_LENGTH_SHA512;
        return true;
    default:
        break;
    }
    return false;
}

static enum planned_row_scalar_digest_kind row_scalar_digest_kind_from_ast(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_MD5_FUNCTION:
        return PLANNED_ROW_SCALAR_DIGEST_MD5;
    case MYLITE_SQL_AST_SHA_FUNCTION:
        return PLANNED_ROW_SCALAR_DIGEST_SHA;
    case MYLITE_SQL_AST_SHA1_FUNCTION:
        return PLANNED_ROW_SCALAR_DIGEST_SHA1;
    case MYLITE_SQL_AST_SHA2_FUNCTION:
        return PLANNED_ROW_SCALAR_DIGEST_SHA2;
    default:
        break;
    }
    return PLANNED_ROW_SCALAR_DIGEST_NONE;
}

static const char *row_scalar_digest_error_function_name(
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL) {
        return NULL;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_MD5_FUNCTION:
    case MYLITE_SQL_AST_MD5_ARGUMENT_COUNT_ERROR:
        return "MD5";
    case MYLITE_SQL_AST_SHA_FUNCTION:
    case MYLITE_SQL_AST_SHA_ARGUMENT_COUNT_ERROR:
        return "SHA";
    case MYLITE_SQL_AST_SHA1_FUNCTION:
    case MYLITE_SQL_AST_SHA1_ARGUMENT_COUNT_ERROR:
        return "SHA1";
    case MYLITE_SQL_AST_SHA2_FUNCTION:
    case MYLITE_SQL_AST_SHA2_ARGUMENT_COUNT_ERROR:
        return "SHA2";
    default:
        break;
    }
    return NULL;
}

static const char *row_scalar_digest_sql_function_name(
    enum planned_row_scalar_digest_kind digest_kind
) {
    switch (digest_kind) {
    case PLANNED_ROW_SCALAR_DIGEST_MD5:
        return "_mylite_md5";
    case PLANNED_ROW_SCALAR_DIGEST_SHA:
        return "_mylite_sha";
    case PLANNED_ROW_SCALAR_DIGEST_SHA1:
        return "_mylite_sha1";
    case PLANNED_ROW_SCALAR_DIGEST_SHA2:
        return "_mylite_sha2";
    case PLANNED_ROW_SCALAR_DIGEST_NONE:
        break;
    }
    return NULL;
}

static enum mylite_digest_algorithm row_scalar_digest_algorithm(
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL) {
        return MYLITE_DIGEST_ALGORITHM_SHA512;
    }
    switch (expression->digest_kind) {
    case PLANNED_ROW_SCALAR_DIGEST_MD5:
        return MYLITE_DIGEST_ALGORITHM_MD5;
    case PLANNED_ROW_SCALAR_DIGEST_SHA:
    case PLANNED_ROW_SCALAR_DIGEST_SHA1:
        return MYLITE_DIGEST_ALGORITHM_SHA1;
    case PLANNED_ROW_SCALAR_DIGEST_SHA2:
        if (expression->argument_count == 2U && expression->arguments != NULL &&
            expression->arguments[1].kind == PLANNED_ROW_SCALAR_EXPRESSION_VALUE &&
            !expression->arguments[1].value.is_null) {
            switch (expression->arguments[1].value.integer) {
            case MYLITE_DIGEST_SHA2_LENGTH_DEFAULT:
            case MYLITE_DIGEST_SHA2_LENGTH_SHA256:
                return MYLITE_DIGEST_ALGORITHM_SHA256;
            case MYLITE_DIGEST_SHA2_LENGTH_SHA224:
                return MYLITE_DIGEST_ALGORITHM_SHA224;
            case MYLITE_DIGEST_SHA2_LENGTH_SHA384:
                return MYLITE_DIGEST_ALGORITHM_SHA384;
            case MYLITE_DIGEST_SHA2_LENGTH_SHA512:
                return MYLITE_DIGEST_ALGORITHM_SHA512;
            default:
                break;
            }
        }
        return MYLITE_DIGEST_ALGORITHM_SHA512;
    case PLANNED_ROW_SCALAR_DIGEST_NONE:
        break;
    }
    return MYLITE_DIGEST_ALGORITHM_SHA512;
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
        mylite_execution_scalar_set_uuid_unsupported_error(database, "UUID conversion");
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    rc = validate_row_scalar_uuid_expression_arity(database, expression, child_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (expression->kind == MYLITE_SQL_AST_UUID_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_UUID_SHORT_FUNCTION) {
        if (has_source) {
            out_expression->kind = expression->kind == MYLITE_SQL_AST_UUID_SHORT_FUNCTION
                                       ? PLANNED_ROW_SCALAR_EXPRESSION_UUID_SHORT
                                       : PLANNED_ROW_SCALAR_EXPRESSION_UUID;
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
    if (expression->kind == MYLITE_SQL_AST_UUID_SHORT_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "UUID_SHORT");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UUID_SHORT_FUNCTION && child_count != 0U) {
        set_native_function_parameter_count_error(database, "UUID_SHORT");
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
            return mylite_execution_set_unknown_column_reference_error(database, argument);
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
        mylite_execution_scalar_set_uuid_unsupported_error(database, "UUID conversion");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_scalar_uuid_function_value(database, expression, &cell);
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
        rc = mylite_execution_scalar_uuid_swap_flag_value(database, request->swap_argument, &swap);
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
        rc = mylite_execution_scalar_uuid_swap_flag_value(database, request->swap_argument, &swap);
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

    mylite_execution_scalar_set_uuid_unsupported_error(database, "UUID conversion");
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
    case MYLITE_SQL_AST_MD5_FUNCTION:
    case MYLITE_SQL_AST_MD5_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SHA_FUNCTION:
    case MYLITE_SQL_AST_SHA_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SHA1_FUNCTION:
    case MYLITE_SQL_AST_SHA1_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SHA2_FUNCTION:
    case MYLITE_SQL_AST_SHA2_ARGUMENT_COUNT_ERROR:
        return plan_row_scalar_digest_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_UUID_FUNCTION:
    case MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UUID_SHORT_FUNCTION:
    case MYLITE_SQL_AST_UUID_SHORT_ARGUMENT_COUNT_ERROR:
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
    case MYLITE_SQL_AST_COMPRESS_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESS_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_RANDOM_BYTES_FUNCTION:
    case MYLITE_SQL_AST_INET_ATON_FUNCTION:
    case MYLITE_SQL_AST_INET_NTOA_FUNCTION:
    case MYLITE_SQL_AST_LAST_INSERT_ID_SET_FUNCTION:
        return plan_row_scalar_registered_one_argument_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_INET_ATON_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "INET_ATON");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_INET_NTOA_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "INET_NTOA");
        return MYLITE_ERROR;
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
