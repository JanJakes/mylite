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

static int plan_row_scalar_json_schema_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    const char *function_name = NULL;
    const char *diagnostic_function_name = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (generic_function_name_equals(expression, "JSON_SCHEMA_VALID")) {
        function_name = "JSON_SCHEMA_VALID";
        diagnostic_function_name = "json_schema_valid";
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALID;
    } else if (generic_function_name_equals(expression, "JSON_SCHEMA_VALIDATION_REPORT")) {
        function_name = "JSON_SCHEMA_VALIDATION_REPORT";
        diagnostic_function_name = "json_schema_validation_report";
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALIDATION_REPORT;
    } else {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    arguments = expression == NULL ? NULL : expression->first_child;
    while (arguments != NULL && arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        arguments = arguments->next_sibling;
    }
    if (arguments == NULL || mylite_sql_ast_node_child_count(arguments) != 2U) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }

    return plan_row_scalar_json_schema_arguments(
        database,
        arguments,
        diagnostic_function_name,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        out_expression
    );
}

static int plan_row_scalar_json_schema_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    const char *diagnostic_function_name,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->argument_count = 2U;

    for (size_t index = 0U; rc == MYLITE_OK && index < 2U; ++index) {
        rc = plan_row_scalar_json_schema_argument(
            database,
            child_at(arguments, index),
            diagnostic_function_name,
            index + 1U,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[index]
        );
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(out_expression);
    }
    return rc;
}

static int plan_row_scalar_json_schema_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *diagnostic_function_name,
    size_t argument_index,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    enum mylite_session_user_variable_value_kind variable_kind =
        MYLITE_SESSION_USER_VARIABLE_VALUE_NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "JSON schema validation supports string, JSON, NULL, user-variable, and descriptor "
            "column arguments"
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
            COLUMN_REFERENCE_FIELD,
            "JSON schema validation supports only JSON and nonbinary string descriptor columns",
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
        }
        return set_json_schema_invalid_data_type_error(
            database,
            diagnostic_function_name,
            argument_index
        );
    }
    if (expression->kind == MYLITE_SQL_AST_USER_VARIABLE) {
        int rc = session_user_variable_value_kind(database, expression, &variable_kind);

        if (rc != MYLITE_OK) {
            return rc;
        }
        if (variable_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_NULL &&
            variable_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_STRING) {
            return set_json_schema_invalid_data_type_error(
                database,
                diagnostic_function_name,
                argument_index
            );
        }
        return plan_row_scalar_session_value(database, expression, out_expression);
    }

    set_unsupported_error(
        database,
        "JSON schema validation supports string, JSON, NULL, user-variable, and descriptor column "
        "arguments"
    );
    return MYLITE_ERROR;
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
        rc = plan_row_scalar_json_contains_candidate_argument(
            database,
            child_at(expression, 1U),
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

static int plan_row_scalar_json_contains_candidate_argument(
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
    if (has_source && expression != NULL && expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
            literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
        }
    }
    if (has_source && expression != NULL && expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
    }
    return plan_row_scalar_json_introspection_document_argument(
        database,
        expression,
        "JSON_CONTAINS",
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        out_expression
    );
}

static int plan_row_scalar_json_overlaps_expression(
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
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_OVERLAPS_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "JSON_OVERLAPS");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_JSON_OVERLAPS;
    out_expression->argument_count = 2U;

    rc = plan_row_scalar_json_introspection_document_argument(
        database,
        child_at(expression, 0U),
        "JSON_OVERLAPS",
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
            "JSON_OVERLAPS",
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            &out_expression->arguments[1]
        );
    }
    return rc;
}

static int plan_row_scalar_json_member_of_expression(
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
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_MEMBER_OF_FUNCTION ||
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
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_JSON_MEMBER_OF;
    out_expression->argument_count = 2U;

    rc = plan_row_scalar_json_member_of_left_argument(
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
        rc = plan_row_scalar_json_introspection_document_argument(
            database,
            child_at(expression, 1U),
            "MEMBER OF",
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            &out_expression->arguments[1]
        );
    }
    return rc;
}

static int plan_row_scalar_json_member_of_left_argument(
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
            "MEMBER OF() supports only string, integer, boolean, NULL, JSON_ARRAY(), "
            "JSON_OBJECT(), and descriptor column left operands"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_ARRAY_FUNCTION) {
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
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_OBJECT_FUNCTION) {
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

static int plan_row_scalar_json_search_expression(
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
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_SEARCH_FUNCTION) {
        set_native_function_parameter_count_error(database, "JSON_SEARCH");
        return MYLITE_ERROR;
    }
    rc = mylite_execution_scalar_json_collect_function_arguments(
        database,
        expression,
        "JSON_SEARCH",
        &arguments,
        &argument_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument_count < 3U) {
        set_native_function_parameter_count_error(database, "JSON_SEARCH");
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
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_JSON_SEARCH;
    out_expression->argument_count = argument_count;

    argument = child_at(arguments, 0U);
    rc = plan_row_scalar_json_introspection_document_argument(
        database,
        argument,
        "JSON_SEARCH",
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        &out_expression->arguments[0]
    );
    argument = argument == NULL ? NULL : argument->next_sibling;
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_json_search_mode_argument(
            database,
            argument,
            &out_expression->arguments[1]
        );
    }
    argument = argument == NULL ? NULL : argument->next_sibling;
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_json_search_scalar_argument(
            database,
            argument,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            "JSON_SEARCH() supports only string, NULL, and descriptor column search_str arguments",
            &out_expression->arguments[2]
        );
    }
    argument = argument == NULL ? NULL : argument->next_sibling;
    if (rc == MYLITE_OK && argument_count >= 4U) {
        rc = plan_row_scalar_json_search_scalar_argument(
            database,
            argument,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            "JSON_SEARCH() supports only string, NULL, and descriptor column escape_char arguments",
            &out_expression->arguments[3]
        );
        argument = argument == NULL ? NULL : argument->next_sibling;
    }
    for (size_t argument_index = 4U;
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

static int plan_row_scalar_json_search_mode_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    struct planned_row_scalar_expression planned = {0};
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "JSON_SEARCH() supports only string and NULL one_or_all arguments"
        );
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        return plan_row_scalar_literal_value(database, expression, out_expression);
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        set_invalid_json_one_or_all_function_error(database, "json_search");
        return MYLITE_ERROR;
    }
    rc = plan_row_scalar_literal_value(database, expression, &planned);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!mylite_execution_text_equals_ascii_case_insensitive(planned.value.text, "one") &&
        !mylite_execution_text_equals_ascii_case_insensitive(planned.value.text, "all")) {
        planned_row_scalar_expression_deinit(&planned);
        set_invalid_json_one_or_all_function_error(database, "json_search");
        return MYLITE_ERROR;
    }
    *out_expression = planned;
    return MYLITE_OK;
}

static int plan_row_scalar_json_search_scalar_argument(
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
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (!has_source) {
            return mylite_execution_set_unknown_column_reference_error(database, expression);
        }
        return plan_row_scalar_json_text_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            unsupported_message,
            out_expression
        );
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING &&
        literal_kind != MYLITE_SQL_AST_LITERAL_NULL) {
        set_unsupported_error(database, unsupported_message);
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

static int plan_row_scalar_json_depth_expression(
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
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_DEPTH_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "JSON_DEPTH");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_JSON_DEPTH;
    out_expression->argument_count = 1U;

    rc = plan_row_scalar_json_introspection_document_argument(
        database,
        child_at(expression, 0U),
        "JSON_DEPTH",
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        &out_expression->arguments[0]
    );
    return rc;
}

static int plan_row_scalar_json_storage_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    enum mylite_sql_ast_node_kind function_kind,
    const char *function_name,
    enum planned_row_scalar_expression_kind planned_kind,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != function_kind ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = planned_kind;
    out_expression->argument_count = 1U;

    rc = plan_row_scalar_json_introspection_document_argument(
        database,
        child_at(expression, 0U),
        function_name,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        column_diagnostic_context,
        &out_expression->arguments[0]
    );
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

static int plan_row_scalar_json_pretty_expression(
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
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_PRETTY_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "JSON_PRETTY");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_JSON_PRETTY;
    out_expression->argument_count = 1U;

    rc = plan_row_scalar_json_introspection_document_argument(
        database,
        child_at(expression, 0U),
        "JSON_PRETTY",
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
    out_expression->arguments =
        argument_count == 0U ? NULL
                             : (struct planned_row_scalar_expression *)
                                   calloc(argument_count, sizeof(*out_expression->arguments));
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
    out_expression->arguments =
        argument_count == 0U ? NULL
                             : (struct planned_row_scalar_expression *)
                                   calloc(argument_count, sizeof(*out_expression->arguments));
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
    case MYLITE_SQL_AST_JSON_ARRAY_APPEND_FUNCTION:
        *out_function_name = "JSON_ARRAY_APPEND";
        *out_mutation_kind = PLANNED_JSON_MUTATION_ARRAY_APPEND;
        return MYLITE_OK;
    case MYLITE_SQL_AST_JSON_ARRAY_INSERT_FUNCTION:
        *out_function_name = "JSON_ARRAY_INSERT";
        *out_mutation_kind = PLANNED_JSON_MUTATION_ARRAY_INSERT;
        return MYLITE_OK;
    case MYLITE_SQL_AST_JSON_MERGE_FUNCTION:
        *out_function_name = "JSON_MERGE";
        *out_mutation_kind = PLANNED_JSON_MUTATION_MERGE;
        return MYLITE_OK;
    case MYLITE_SQL_AST_JSON_MERGE_PATCH_FUNCTION:
        *out_function_name = "JSON_MERGE_PATCH";
        *out_mutation_kind = PLANNED_JSON_MUTATION_MERGE_PATCH;
        return MYLITE_OK;
    case MYLITE_SQL_AST_JSON_MERGE_PRESERVE_FUNCTION:
        *out_function_name = "JSON_MERGE_PRESERVE";
        *out_mutation_kind = PLANNED_JSON_MUTATION_MERGE_PRESERVE;
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

    if (json_mutation_kind_is_merge(mutation_kind)) {
        return plan_row_scalar_json_merge_arguments(
            database,
            arguments,
            argument_count,
            function_name,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            column_diagnostic_context,
            out_expression
        );
    }

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

static bool json_mutation_kind_is_merge(enum planned_json_mutation_kind mutation_kind) {
    return mutation_kind == PLANNED_JSON_MUTATION_MERGE ||
           mutation_kind == PLANNED_JSON_MUTATION_MERGE_PATCH ||
           mutation_kind == PLANNED_JSON_MUTATION_MERGE_PRESERVE;
}

static bool json_mutation_kind_preserves_first_null(enum planned_json_mutation_kind mutation_kind) {
    return mutation_kind == PLANNED_JSON_MUTATION_MERGE ||
           mutation_kind == PLANNED_JSON_MUTATION_MERGE_PRESERVE;
}

static int plan_row_scalar_json_merge_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    size_t argument_count,
    const char *function_name,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    enum column_reference_diagnostic_context column_diagnostic_context,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *argument = child_at(arguments, 0U);
    int rc = MYLITE_OK;

    if (out_expression == NULL || out_expression->arguments == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         ++argument_index) {
        rc = plan_row_scalar_json_introspection_document_argument(
            database,
            argument,
            function_name,
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
    } else if (strcmp(function_name, "JSON_ARRAY_APPEND") == 0) {
        document_message =
            "JSON_ARRAY_APPEND() supports only string, NULL, and descriptor column documents";
        column_message = "JSON_ARRAY_APPEND() supports only JSON and nonbinary string descriptor "
                         "column documents";
    } else if (strcmp(function_name, "JSON_ARRAY_INSERT") == 0) {
        document_message =
            "JSON_ARRAY_INSERT() supports only string, NULL, and descriptor column documents";
        column_message = "JSON_ARRAY_INSERT() supports only JSON and nonbinary string descriptor "
                         "column documents";
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
            "JSON introspection supports only current JSON document producers and descriptor "
            "columns"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_SET_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_INSERT_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_ARRAY_APPEND_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_ARRAY_INSERT_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_REPLACE_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_REMOVE_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_MERGE_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_MERGE_PATCH_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_MERGE_PRESERVE_FUNCTION) {
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
    if (expression->kind == MYLITE_SQL_AST_JSON_ARRAY_FUNCTION) {
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
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_OBJECT_FUNCTION) {
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
        "JSON introspection supports only current JSON document producers and descriptor columns"
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
            "JSON_EXTRACT() supports only current JSON document producers and descriptor columns"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_SET_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_INSERT_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_ARRAY_APPEND_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_ARRAY_INSERT_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_REPLACE_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_REMOVE_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_MERGE_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_MERGE_PATCH_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_MERGE_PRESERVE_FUNCTION) {
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
        "JSON_EXTRACT() supports only current JSON document producers and descriptor columns"
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
    } else if (strcmp(function_name, "JSON_ARRAY_APPEND") == 0) {
        path_message = "JSON_ARRAY_APPEND() supports only string and NULL path literals";
    } else if (strcmp(function_name, "JSON_ARRAY_INSERT") == 0) {
        path_message = "JSON_ARRAY_INSERT() supports only string and NULL path literals";
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
        } else if (strcmp(function_name, "JSON_ARRAY_APPEND") == 0) {
            message = "JSON_ARRAY_APPEND() values support only string, integer, boolean, NULL, "
                      "JSON functions, and descriptor column arguments";
        } else if (strcmp(function_name, "JSON_ARRAY_INSERT") == 0) {
            message = "JSON_ARRAY_INSERT() values support only string, integer, boolean, NULL, "
                      "JSON functions, and descriptor column arguments";
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
