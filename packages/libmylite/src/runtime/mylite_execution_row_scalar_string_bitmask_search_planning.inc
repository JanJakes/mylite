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
                                       : mylite_execution_string_bitmask_function_kind(expression->kind);
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
    if (!mylite_execution_string_slice_length_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "string bitmask functions support only signed integer, boolean, NULL, and integer "
            "descriptor bitmask arguments"
        );
        return MYLITE_ERROR;
    }
    rc = mylite_execution_string_slice_signed_integer_value(
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
    if (!mylite_execution_string_slice_length_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "EXPORT_SET() number_of_bits supports only signed integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    rc = mylite_execution_string_slice_signed_integer_value(
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
                                       : mylite_execution_string_search_function_kind(expression->kind);
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
    if (!mylite_execution_string_slice_length_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "LOCATE() position supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    rc = mylite_execution_string_slice_signed_integer_value(
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
