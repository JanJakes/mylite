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
