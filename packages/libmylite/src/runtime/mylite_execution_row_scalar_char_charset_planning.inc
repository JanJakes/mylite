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
    if (retain_planned_row_scalar_column(database, &out_expression->column, &column) != MYLITE_OK) {
        return MYLITE_NOMEM;
    }
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
    function_kind = expression == NULL
                        ? PLANNED_CHARSET_COLLATION_FUNCTION_NONE
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
        mylite_execution_coercibility_binary_wrapper_column_reference(
            argument,
            &binary_column_argument
        )) {
        struct mylite_catalog_column_descriptor column = {0};

        if (!has_source) {
            return mylite_execution_set_unknown_column_reference_error(
                database,
                binary_column_argument
            );
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

    rc = mylite_execution_charset_collation_scalar_result(
        database,
        function_kind,
        argument,
        &result
    );
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
    return mylite_execution_charset_collation_select_result(
        function_kind,
        charset,
        collation,
        out_result
    );
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
