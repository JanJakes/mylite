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
        if (retain_planned_row_scalar_column(database, &out_expression->column, &column) !=
            MYLITE_OK) {
            return MYLITE_NOMEM;
        }
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

    rc = mylite_execution_scalar_weight_string_function_value(database, expression, &cell);
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
            return mylite_execution_set_unknown_column_reference_error(database, expression);
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
    if (retain_planned_row_scalar_column(database, &out_expression->column, &column) != MYLITE_OK) {
        return MYLITE_NOMEM;
    }
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
