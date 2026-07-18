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
        rc = mylite_execution_evaluate_regexp_like_match_type_argument(
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
        rc = mylite_execution_validate_regexp_like_pattern(
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
    if (row_scalar_expression_contains_explicit_binary_conversion(expression)) {
        set_unsupported_error(database, "REGEXP_LIKE() does not support binary values");
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
    int rc = mylite_execution_evaluate_regexp_like_text_argument(
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
    if (retain_planned_row_scalar_column(database, &out_expression->column, &column) != MYLITE_OK) {
        return MYLITE_NOMEM;
    }
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
    kind = mylite_execution_regexp_string_function_kind(expression->kind);
    if (kind == PLANNED_REGEXP_STRING_FUNCTION_NONE) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    expected_argument_count = kind == PLANNED_REGEXP_STRING_FUNCTION_REPLACE ? 3U : 2U;
    child_count = mylite_execution_regexp_string_function_argument_count(expression);
    if (child_count != expected_argument_count) {
        if (child_count > expected_argument_count) {
            set_unsupported_error(
                database,
                "REGEXP string function optional arguments are not supported"
            );
        } else {
            set_native_function_parameter_count_error(
                database,
                mylite_execution_regexp_string_function_name(kind)
            );
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
        mylite_execution_regexp_string_function_argument_at(expression, 0U),
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
            mylite_execution_regexp_string_function_argument_at(expression, 1U),
            &out_expression->arguments[1]
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_row_scalar_regexp_string_pattern(database, &out_expression->arguments[1]);
    }
    if (rc == MYLITE_OK && kind == PLANNED_REGEXP_STRING_FUNCTION_REPLACE) {
        rc = plan_row_scalar_regexp_string_argument(
            database,
            mylite_execution_regexp_string_function_argument_at(expression, 2U),
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
    return mylite_execution_validate_regexp_like_pattern(
        database,
        pattern_argument->value.text,
        pattern_argument->value.text_length,
        false,
        "REGEXP string function patterns support only MyLite's baseline ASCII regular expression "
        "subset"
    );
}
