static int plan_row_scalar_numeric_function_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum planned_row_scalar_numeric_function_kind function_kind = PLANNED_ROW_SCALAR_NUMERIC_NONE;
    const char *function_name = NULL;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    if (out_expression == NULL) {
        return MYLITE_MISUSE;
    }
    planned_row_scalar_expression_deinit(out_expression);

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    function_name = row_scalar_numeric_function_display_name(expression->kind);
    if (row_scalar_numeric_function_kind_is_error(expression->kind)) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }

    function_kind = row_scalar_numeric_function_kind_from_ast(expression->kind);
    if (function_kind == PLANNED_ROW_SCALAR_NUMERIC_NONE || function_name == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    argument_count = mylite_sql_ast_node_child_count(expression);
    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (argument_count != 0U && out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_FUNCTION;
    out_expression->numeric_function_kind = function_kind;
    out_expression->argument_count = argument_count;
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < argument_count;
         ++argument_index) {
        rc = plan_row_scalar_numeric_function_argument(
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
        planned_row_scalar_expression_deinit(out_expression);
    }
    return rc;
}

static int plan_row_scalar_numeric_function_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_argument
) {
    int rc = MYLITE_OK;

    argument = unwrap_parenthesized_expression(argument);
    if (argument == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (argument->kind == MYLITE_SQL_AST_IDENTIFIER ||
        argument->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return plan_row_scalar_numeric_function_column_argument(
            database,
            argument,
            source_context,
            table_columns,
            table_column_count,
            out_argument
        );
    }

    rc = plan_row_scalar_expression(
        database,
        argument,
        has_source,
        source_context,
        NULL,
        table_columns,
        table_column_count,
        out_argument
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    return validate_row_scalar_numeric_function_argument(database, out_argument);
}

static int plan_row_scalar_numeric_function_column_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_argument
) {
    struct mylite_catalog_column_descriptor column = {0};
    size_t source_index = 0U;
    int rc = resolve_descriptor_column_reference_with_source_index(
        database,
        argument,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar numeric functions support only descriptor columns",
        table_columns,
        table_column_count,
        &column,
        &source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!row_scalar_numeric_function_column_descriptor_is_supported(&column)) {
        set_unsupported_error(
            database,
            "row-scalar numeric functions support only numeric or string descriptor columns"
        );
        return MYLITE_ERROR;
    }

    out_argument->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_argument->column = column;
    out_argument->column_has_source_index = select_source_context_is_joined(source_context);
    out_argument->column_source_index = source_index;
    return MYLITE_OK;
}

static int validate_row_scalar_numeric_function_argument(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *argument
) {
    if (argument == NULL) {
        return MYLITE_MISUSE;
    }
    switch (argument->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        if (!argument->value.is_blob) {
            return MYLITE_OK;
        }
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        if (row_scalar_numeric_function_column_descriptor_is_supported(&argument->column)) {
            return MYLITE_OK;
        }
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_FUNCTION:
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_ELT:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_COMPARISON:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLLATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DIGEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_COMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESSED_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_RANDOM_BYTES:
        break;
    }

    set_unsupported_error(
        database,
        "row-scalar numeric functions support only numeric row-scalar arguments"
    );
    return MYLITE_ERROR;
}

static bool row_scalar_numeric_function_column_descriptor_is_supported(
    const struct mylite_catalog_column_descriptor *column
) {
    return column != NULL && (strcmp(column->physical_type, "INTEGER") == 0 ||
                              column_descriptor_is_decimal(column) ||
                              column_descriptor_is_approximate(column) ||
                              column_descriptor_is_string_family(column));
}

static enum planned_row_scalar_numeric_function_kind row_scalar_numeric_function_kind_from_ast(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_ABS_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_ABS;
    case MYLITE_SQL_AST_SIGN_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_SIGN;
    case MYLITE_SQL_AST_CEIL_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_CEIL;
    case MYLITE_SQL_AST_CEILING_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_CEILING;
    case MYLITE_SQL_AST_FLOOR_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_FLOOR;
    case MYLITE_SQL_AST_ROUND_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_ROUND;
    case MYLITE_SQL_AST_SQRT_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_SQRT;
    case MYLITE_SQL_AST_DEGREES_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_DEGREES;
    case MYLITE_SQL_AST_RADIANS_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_RADIANS;
    case MYLITE_SQL_AST_ACOS_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_ACOS;
    case MYLITE_SQL_AST_ASIN_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_ASIN;
    case MYLITE_SQL_AST_ATAN_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_ATAN;
    case MYLITE_SQL_AST_ATAN2_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_ATAN2;
    case MYLITE_SQL_AST_SIN_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_SIN;
    case MYLITE_SQL_AST_COS_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_COS;
    case MYLITE_SQL_AST_TAN_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_TAN;
    case MYLITE_SQL_AST_COT_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_COT;
    case MYLITE_SQL_AST_EXP_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_EXP;
    case MYLITE_SQL_AST_LN_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_LN;
    case MYLITE_SQL_AST_LOG_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_LOG;
    case MYLITE_SQL_AST_LOG10_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_LOG10;
    case MYLITE_SQL_AST_LOG2_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_LOG2;
    case MYLITE_SQL_AST_POW_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_POW;
    case MYLITE_SQL_AST_POWER_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_POWER;
    case MYLITE_SQL_AST_BIT_COUNT_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_BIT_COUNT;
    default:
        break;
    }
    return PLANNED_ROW_SCALAR_NUMERIC_NONE;
}

static const char *row_scalar_numeric_function_display_name(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_ABS_FUNCTION:
    case MYLITE_SQL_AST_ABS_ARGUMENT_COUNT_ERROR:
        return "ABS";
    case MYLITE_SQL_AST_SIGN_FUNCTION:
    case MYLITE_SQL_AST_SIGN_ARGUMENT_COUNT_ERROR:
        return "SIGN";
    case MYLITE_SQL_AST_CEIL_FUNCTION:
    case MYLITE_SQL_AST_CEIL_ARGUMENT_COUNT_ERROR:
        return "CEIL";
    case MYLITE_SQL_AST_CEILING_FUNCTION:
    case MYLITE_SQL_AST_CEILING_ARGUMENT_COUNT_ERROR:
        return "CEILING";
    case MYLITE_SQL_AST_FLOOR_FUNCTION:
    case MYLITE_SQL_AST_FLOOR_ARGUMENT_COUNT_ERROR:
        return "FLOOR";
    case MYLITE_SQL_AST_ROUND_FUNCTION:
    case MYLITE_SQL_AST_ROUND_ARGUMENT_COUNT_ERROR:
        return "ROUND";
    case MYLITE_SQL_AST_SQRT_FUNCTION:
    case MYLITE_SQL_AST_SQRT_ARGUMENT_COUNT_ERROR:
        return "SQRT";
    case MYLITE_SQL_AST_DEGREES_FUNCTION:
    case MYLITE_SQL_AST_DEGREES_ARGUMENT_COUNT_ERROR:
        return "DEGREES";
    case MYLITE_SQL_AST_RADIANS_FUNCTION:
    case MYLITE_SQL_AST_RADIANS_ARGUMENT_COUNT_ERROR:
        return "RADIANS";
    case MYLITE_SQL_AST_ACOS_FUNCTION:
    case MYLITE_SQL_AST_ACOS_ARGUMENT_COUNT_ERROR:
        return "ACOS";
    case MYLITE_SQL_AST_ASIN_FUNCTION:
    case MYLITE_SQL_AST_ASIN_ARGUMENT_COUNT_ERROR:
        return "ASIN";
    case MYLITE_SQL_AST_ATAN_FUNCTION:
    case MYLITE_SQL_AST_ATAN_ARGUMENT_COUNT_ERROR:
        return "ATAN";
    case MYLITE_SQL_AST_ATAN2_FUNCTION:
    case MYLITE_SQL_AST_ATAN2_ARGUMENT_COUNT_ERROR:
        return "ATAN2";
    case MYLITE_SQL_AST_SIN_FUNCTION:
    case MYLITE_SQL_AST_SIN_ARGUMENT_COUNT_ERROR:
        return "SIN";
    case MYLITE_SQL_AST_COS_FUNCTION:
    case MYLITE_SQL_AST_COS_ARGUMENT_COUNT_ERROR:
        return "COS";
    case MYLITE_SQL_AST_TAN_FUNCTION:
    case MYLITE_SQL_AST_TAN_ARGUMENT_COUNT_ERROR:
        return "TAN";
    case MYLITE_SQL_AST_COT_FUNCTION:
    case MYLITE_SQL_AST_COT_ARGUMENT_COUNT_ERROR:
        return "COT";
    case MYLITE_SQL_AST_EXP_FUNCTION:
    case MYLITE_SQL_AST_EXP_ARGUMENT_COUNT_ERROR:
        return "EXP";
    case MYLITE_SQL_AST_LN_FUNCTION:
    case MYLITE_SQL_AST_LN_ARGUMENT_COUNT_ERROR:
        return "LN";
    case MYLITE_SQL_AST_LOG_FUNCTION:
        return "LOG";
    case MYLITE_SQL_AST_LOG10_FUNCTION:
    case MYLITE_SQL_AST_LOG10_ARGUMENT_COUNT_ERROR:
        return "LOG10";
    case MYLITE_SQL_AST_LOG2_FUNCTION:
    case MYLITE_SQL_AST_LOG2_ARGUMENT_COUNT_ERROR:
        return "LOG2";
    case MYLITE_SQL_AST_POW_FUNCTION:
    case MYLITE_SQL_AST_POW_ARGUMENT_COUNT_ERROR:
        return "POW";
    case MYLITE_SQL_AST_POWER_FUNCTION:
    case MYLITE_SQL_AST_POWER_ARGUMENT_COUNT_ERROR:
        return "POWER";
    case MYLITE_SQL_AST_BIT_COUNT_FUNCTION:
    case MYLITE_SQL_AST_BIT_COUNT_ARGUMENT_COUNT_ERROR:
        return "BIT_COUNT";
    default:
        break;
    }
    return NULL;
}

static bool row_scalar_numeric_function_kind_is_error(enum mylite_sql_ast_node_kind ast_kind) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_ABS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SIGN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CEIL_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CEILING_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FLOOR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ROUND_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SQRT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DEGREES_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_RADIANS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ACOS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ASIN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ATAN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ATAN2_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SIN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_COS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TAN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_COT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_EXP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LOG10_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LOG2_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_POW_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_POWER_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_BIT_COUNT_ARGUMENT_COUNT_ERROR:
        return true;
    default:
        break;
    }
    return false;
}

static bool row_scalar_numeric_function_kind_is_supported(enum mylite_sql_ast_node_kind ast_kind) {
    return row_scalar_numeric_function_kind_from_ast(ast_kind) != PLANNED_ROW_SCALAR_NUMERIC_NONE ||
           row_scalar_numeric_function_kind_is_error(ast_kind);
}
