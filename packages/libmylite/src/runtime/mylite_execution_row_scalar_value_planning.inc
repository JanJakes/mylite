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
                                       : mylite_execution_charset_collation_function_kind(expression->kind);
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
            return mylite_execution_set_unknown_column_reference_error(database, argument);
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
        mylite_execution_coercibility_binary_wrapper_column_reference(argument, &binary_column_argument)) {
        struct mylite_catalog_column_descriptor column = {0};

        if (!has_source) {
            return mylite_execution_set_unknown_column_reference_error(database, binary_column_argument);
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

    rc = mylite_execution_charset_collation_scalar_result(database, function_kind, argument, &result);
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
    return mylite_execution_charset_collation_select_result(function_kind, charset, collation, out_result);
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
