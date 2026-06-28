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
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUES_OUTSIDE_ODKU:
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        if (row_scalar_numeric_function_column_descriptor_is_supported(&argument->column)) {
            return MYLITE_OK;
        }
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_GET_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_FREE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_USED_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_ALL_LOCKS:
    case PLANNED_ROW_SCALAR_EXPRESSION_BENCHMARK:
    case PLANNED_ROW_SCALAR_EXPRESSION_SLEEP:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_SHORT:
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_LOAD_FILE:
    case PLANNED_ROW_SCALAR_EXPRESSION_EXTRACT_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_UPDATE_XML:
    case PLANNED_ROW_SCALAR_EXPRESSION_SYS_FUNCTION:
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_EXTRA:
        if (argument->numeric_extra_kind != PLANNED_ROW_SCALAR_NUMERIC_EXTRA_FORMAT) {
            return MYLITE_OK;
        }
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_BITWISE:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_ELT:
    case PLANNED_ROW_SCALAR_EXPRESSION_ICU_VERSION:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_DEPTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_SIZE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_FREE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_PRETTY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OVERLAPS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_MEMBER_OF:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_ADDTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBTIME:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_AES_ENCRYPT:
    case PLANNED_ROW_SCALAR_EXPRESSION_AES_DECRYPT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SPATIAL_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_ATON:
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_NTOA:
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_GET:
    case PLANNED_ROW_SCALAR_EXPRESSION_BASE_CONVERSION:
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
    return column != NULL &&
           (strcmp(column->physical_type, "INTEGER") == 0 || column_descriptor_is_decimal(column) ||
            column_descriptor_is_approximate(column) || column_descriptor_is_string_family(column));
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

static const char *row_scalar_numeric_function_display_name(enum mylite_sql_ast_node_kind ast_kind
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

static int plan_row_scalar_base_conversion_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum planned_row_scalar_base_conversion_kind function_kind =
        PLANNED_ROW_SCALAR_BASE_CONVERSION_NONE;
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
    function_name = row_scalar_base_conversion_display_name(expression->kind);
    if (row_scalar_base_conversion_kind_is_error(expression->kind)) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }

    function_kind = row_scalar_base_conversion_kind_from_ast(expression->kind);
    if (function_kind == PLANNED_ROW_SCALAR_BASE_CONVERSION_NONE || function_name == NULL) {
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

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_BASE_CONVERSION;
    out_expression->base_conversion_kind = function_kind;
    out_expression->argument_count = argument_count;
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < argument_count;
         ++argument_index) {
        rc = plan_row_scalar_base_conversion_argument(
            database,
            child_at(expression, argument_index),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            function_name,
            &out_expression->arguments[argument_index]
        );
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(out_expression);
    }
    return rc;
}

static int plan_row_scalar_base_conversion_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *function_name,
    struct planned_row_scalar_expression *out_argument
) {
    enum {
        base_conversion_unsupported_message_capacity = 192,
        base_conversion_range_message_capacity = 96,
    };

    char unsupported_message[base_conversion_unsupported_message_capacity];
    char range_message[base_conversion_range_message_capacity];
    int written = 0;

    if (function_name == NULL) {
        return MYLITE_MISUSE;
    }

    written = snprintf(
        unsupported_message,
        sizeof(unsupported_message),
        "%s() supports only signed integer, boolean, NULL, integer descriptor, "
        "and supported integer expression arguments",
        function_name
    );
    if (written < 0 || (size_t)written >= sizeof(unsupported_message)) {
        set_runtime_error(database, "failed to format base conversion support message");
        return MYLITE_ERROR;
    }
    written = snprintf(
        range_message,
        sizeof(range_message),
        "%s() arguments must fit the signed 64-bit range",
        function_name
    );
    if (written < 0 || (size_t)written >= sizeof(range_message)) {
        set_runtime_error(database, "failed to format base conversion range message");
        return MYLITE_ERROR;
    }

    return plan_row_scalar_integer_like_argument(
        database,
        argument,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        unsupported_message,
        range_message,
        out_argument
    );
}

static enum planned_row_scalar_base_conversion_kind row_scalar_base_conversion_kind_from_ast(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_BIN_FUNCTION:
        return PLANNED_ROW_SCALAR_BASE_CONVERSION_BIN;
    case MYLITE_SQL_AST_OCT_FUNCTION:
        return PLANNED_ROW_SCALAR_BASE_CONVERSION_OCT;
    case MYLITE_SQL_AST_CONV_FUNCTION:
        return PLANNED_ROW_SCALAR_BASE_CONVERSION_CONV;
    default:
        break;
    }
    return PLANNED_ROW_SCALAR_BASE_CONVERSION_NONE;
}

static const char *row_scalar_base_conversion_display_name(enum mylite_sql_ast_node_kind ast_kind) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_BIN_FUNCTION:
    case MYLITE_SQL_AST_BIN_ARGUMENT_COUNT_ERROR:
        return "BIN";
    case MYLITE_SQL_AST_OCT_FUNCTION:
    case MYLITE_SQL_AST_OCT_ARGUMENT_COUNT_ERROR:
        return "OCT";
    case MYLITE_SQL_AST_CONV_FUNCTION:
    case MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR:
        return "CONV";
    default:
        break;
    }
    return NULL;
}

static bool row_scalar_base_conversion_kind_is_error(enum mylite_sql_ast_node_kind ast_kind) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_BIN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_OCT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR:
        return true;
    default:
        break;
    }
    return false;
}

static bool row_scalar_base_conversion_kind_is_supported(enum mylite_sql_ast_node_kind ast_kind) {
    return row_scalar_base_conversion_kind_from_ast(ast_kind) !=
               PLANNED_ROW_SCALAR_BASE_CONVERSION_NONE ||
           row_scalar_base_conversion_kind_is_error(ast_kind);
}

static int plan_row_scalar_numeric_extra_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum planned_row_scalar_numeric_extra_kind function_kind =
        PLANNED_ROW_SCALAR_NUMERIC_EXTRA_NONE;
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
    function_name = row_scalar_numeric_extra_display_name(expression->kind);
    if (row_scalar_numeric_extra_kind_is_error(expression->kind)) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_FORMAT_LOCALE_UNSUPPORTED) {
        set_unsupported_error(database, "FORMAT() locale arguments are not supported");
        return MYLITE_ERROR;
    }

    function_kind = row_scalar_numeric_extra_kind_from_ast(expression->kind);
    if (function_kind == PLANNED_ROW_SCALAR_NUMERIC_EXTRA_NONE || function_name == NULL) {
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

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_EXTRA;
    out_expression->numeric_extra_kind = function_kind;
    out_expression->argument_count = argument_count;
    if (function_kind == PLANNED_ROW_SCALAR_NUMERIC_EXTRA_CRC32) {
        if (argument_count != 1U) {
            set_native_function_parameter_count_error(database, "CRC32");
            rc = MYLITE_ERROR;
        } else {
            rc = plan_row_scalar_numeric_extra_value_argument(
                database,
                child_at(expression, 0U),
                has_source,
                source_context,
                table_columns,
                table_column_count,
                function_name,
                &out_expression->arguments[0]
            );
        }
    } else if (function_kind == PLANNED_ROW_SCALAR_NUMERIC_EXTRA_FORMAT ||
               function_kind == PLANNED_ROW_SCALAR_NUMERIC_EXTRA_TRUNCATE) {
        if (argument_count != 2U) {
            set_native_function_parameter_count_error(database, function_name);
            rc = MYLITE_ERROR;
        } else {
            rc = plan_row_scalar_format_truncate_value_argument(
                database,
                child_at(expression, 0U),
                has_source,
                source_context,
                table_columns,
                table_column_count,
                function_name,
                &out_expression->arguments[0]
            );
            if (rc == MYLITE_OK) {
                rc = plan_row_scalar_format_truncate_places_argument(
                    database,
                    child_at(expression, 1U),
                    has_source,
                    source_context,
                    table_columns,
                    table_column_count,
                    function_name,
                    &out_expression->arguments[1]
                );
            }
            if (rc == MYLITE_OK) {
                plan_row_scalar_numeric_extra_result_shape(function_kind, out_expression);
            }
        }
    } else if (function_kind == PLANNED_ROW_SCALAR_NUMERIC_EXTRA_PI && argument_count != 0U) {
        set_native_function_parameter_count_error(database, "PI");
        rc = MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(out_expression);
    }
    return rc;
}

static int plan_row_scalar_numeric_extra_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *function_name,
    struct planned_row_scalar_expression *out_argument
) {
    (void)function_name;
    return plan_row_scalar_nested_value_argument(
        database,
        argument,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        out_argument
    );
}

static int plan_row_scalar_format_truncate_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *function_name,
    struct planned_row_scalar_expression *out_argument
) {
    argument = unwrap_parenthesized_expression(argument);
    if (argument == NULL || function_name == NULL) {
        set_unsupported_error(
            database,
            "FORMAT() and TRUNCATE() support only exact-decimal row arguments"
        );
        return MYLITE_ERROR;
    }
    if (argument->kind == MYLITE_SQL_AST_IDENTIFIER ||
        argument->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return plan_row_scalar_format_truncate_column_argument(
            database,
            argument,
            source_context,
            table_columns,
            table_column_count,
            function_name,
            out_argument
        );
    }
    if (argument->kind == MYLITE_SQL_AST_LITERAL ||
        argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_format_truncate_literal_argument(
            database,
            argument,
            function_name,
            out_argument
        );
    }
    if (has_source && row_scalar_expression_contains_integer_arithmetic_attempt(argument)) {
        return plan_row_scalar_integer_arithmetic_expression(
            database,
            argument,
            source_context,
            table_columns,
            table_column_count,
            out_argument
        );
    }
    if (has_source && row_scalar_numeric_function_kind_is_supported(argument->kind)) {
        return plan_row_scalar_numeric_function_expression(
            database,
            argument,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_argument
        );
    }

    set_unsupported_error(
        database,
        "FORMAT() and TRUNCATE() support only exact-decimal row arguments"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_format_truncate_literal_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    const char *function_name,
    struct planned_row_scalar_expression *out_argument
) {
    const struct mylite_sql_ast_node *unwrapped = unwrap_parenthesized_expression(argument);
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(argument);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (literal != NULL && literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        literal = unwrap_parenthesized_expression(child_at(literal, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "FORMAT() and TRUNCATE() support only exact-decimal literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL) {
        return plan_row_scalar_numeric_extra_decimal_literal_text(
            database,
            argument,
            function_name,
            out_argument
        );
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
        literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
        literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        if (unwrapped != NULL && unwrapped->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
            return plan_row_scalar_integer_value(database, argument, out_argument);
        }
        return plan_row_scalar_literal_value(database, literal, out_argument);
    }

    set_unsupported_error(database, "FORMAT() and TRUNCATE() support only exact-decimal literals");
    return MYLITE_ERROR;
}

static int plan_row_scalar_format_truncate_column_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *function_name,
    struct planned_row_scalar_expression *out_argument
) {
    struct mylite_catalog_column_descriptor column = {0};
    size_t source_index = 0U;
    int rc = resolve_descriptor_column_reference_with_source_index(
        database,
        argument,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT FORMAT() and TRUNCATE() support only descriptor columns",
        table_columns,
        table_column_count,
        &column,
        &source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!row_scalar_numeric_extra_format_truncate_column_is_supported(&column)) {
        set_unsupported_error(
            database,
            "FORMAT() and TRUNCATE() support only integer and exact DECIMAL columns"
        );
        return MYLITE_ERROR;
    }

    out_argument->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_argument->column = column;
    out_argument->column_has_source_index = select_source_context_is_joined(source_context);
    out_argument->column_source_index = source_index;
    (void)function_name;
    return MYLITE_OK;
}

static int plan_row_scalar_format_truncate_places_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *function_name,
    struct planned_row_scalar_expression *out_argument
) {
    enum {
        message_capacity = 160,
    };

    char unsupported_message[message_capacity];
    int written = 0;

    if (function_name == NULL) {
        return MYLITE_MISUSE;
    }
    written = snprintf(
        unsupported_message,
        sizeof(unsupported_message),
        "%s() decimal places support only signed integer row arguments",
        function_name
    );
    if (written < 0 || (size_t)written >= sizeof(unsupported_message)) {
        set_runtime_error(database, "failed to format FORMAT()/TRUNCATE() support message");
        return MYLITE_ERROR;
    }
    return plan_row_scalar_integer_like_argument(
        database,
        argument,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        unsupported_message,
        "FORMAT()/TRUNCATE() decimal places must fit the signed 64-bit range",
        out_argument
    );
}

static void plan_row_scalar_numeric_extra_result_shape(
    enum planned_row_scalar_numeric_extra_kind function_kind,
    struct planned_row_scalar_expression *out_expression
) {
    struct decimal_type_info info = {0};

    if (function_kind != PLANNED_ROW_SCALAR_NUMERIC_EXTRA_TRUNCATE || out_expression == NULL ||
        out_expression->argument_count != 2U || out_expression->arguments == NULL ||
        out_expression->arguments[0].kind != PLANNED_ROW_SCALAR_EXPRESSION_COLUMN ||
        out_expression->arguments[1].kind == PLANNED_ROW_SCALAR_EXPRESSION_VALUE ||
        !column_descriptor_is_decimal(&out_expression->arguments[0].column) ||
        decimal_type_info_for_logical_type(
            out_expression->arguments[0].column.logical_type,
            &info
        ) != MYLITE_OK) {
        return;
    }

    out_expression->numeric_extra_has_decimal_scale = true;
    out_expression->numeric_extra_decimal_scale = (size_t)info.scale;
}

static int plan_row_scalar_numeric_extra_decimal_literal_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    const char *function_name,
    struct planned_row_scalar_expression *out_argument
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(argument);
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    size_t prefix_size = 0U;
    size_t text_length = 0U;
    char *text = NULL;

    if (literal != NULL && literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        operator_kind = mylite_sql_ast_node_operator(literal);
        if (operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "FORMAT() and TRUNCATE() support only signed decimal literals"
            );
            return MYLITE_ERROR;
        }
        prefix_size = 1U;
        literal = unwrap_parenthesized_expression(child_at(literal, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_DECIMAL ||
        literal->span.text == NULL || literal->span.length == 0U ||
        literal->span.length > SIZE_MAX - prefix_size - 1U) {
        set_unsupported_error(
            database,
            "FORMAT() and TRUNCATE() support only signed decimal literals"
        );
        return MYLITE_ERROR;
    }

    text_length = prefix_size + literal->span.length;
    text = (char *)malloc(text_length + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (prefix_size != 0U) {
        text[0] = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE ? '-' : '+';
    }
    memcpy(text + prefix_size, literal->span.text, literal->span.length);
    text[text_length] = '\0';

    out_argument->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    (void)function_name;
    return assign_text_value(database, text, text_length, &out_argument->value);
}

static bool row_scalar_numeric_extra_format_truncate_column_is_supported(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column == NULL) {
        return false;
    }
    return strcmp(column->physical_type, "INTEGER") == 0 || column_descriptor_is_decimal(column);
}

static enum planned_row_scalar_numeric_extra_kind row_scalar_numeric_extra_kind_from_ast(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_CRC32_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_EXTRA_CRC32;
    case MYLITE_SQL_AST_FORMAT_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_EXTRA_FORMAT;
    case MYLITE_SQL_AST_TRUNCATE_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_EXTRA_TRUNCATE;
    case MYLITE_SQL_AST_PI_FUNCTION:
        return PLANNED_ROW_SCALAR_NUMERIC_EXTRA_PI;
    default:
        break;
    }
    return PLANNED_ROW_SCALAR_NUMERIC_EXTRA_NONE;
}

static const char *row_scalar_numeric_extra_display_name(enum mylite_sql_ast_node_kind ast_kind) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_CRC32_FUNCTION:
    case MYLITE_SQL_AST_CRC32_ARGUMENT_COUNT_ERROR:
        return "CRC32";
    case MYLITE_SQL_AST_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_FORMAT_LOCALE_UNSUPPORTED:
    case MYLITE_SQL_AST_FORMAT_ARGUMENT_COUNT_ERROR:
        return "FORMAT";
    case MYLITE_SQL_AST_TRUNCATE_FUNCTION:
    case MYLITE_SQL_AST_TRUNCATE_ARGUMENT_COUNT_ERROR:
        return "TRUNCATE";
    case MYLITE_SQL_AST_PI_FUNCTION:
        return "PI";
    default:
        break;
    }
    return NULL;
}

static bool row_scalar_numeric_extra_kind_is_error(enum mylite_sql_ast_node_kind ast_kind) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_CRC32_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FORMAT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TRUNCATE_ARGUMENT_COUNT_ERROR:
        return true;
    default:
        break;
    }
    return false;
}

static bool row_scalar_numeric_extra_kind_is_supported(enum mylite_sql_ast_node_kind ast_kind) {
    return row_scalar_numeric_extra_kind_from_ast(ast_kind) !=
               PLANNED_ROW_SCALAR_NUMERIC_EXTRA_NONE ||
           row_scalar_numeric_extra_kind_is_error(ast_kind) ||
           ast_kind == MYLITE_SQL_AST_FORMAT_LOCALE_UNSUPPORTED;
}

static bool row_scalar_numeric_extra_expression_needs_real_sql_context(
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_EXTRA) {
        return false;
    }

    return expression->numeric_extra_kind == PLANNED_ROW_SCALAR_NUMERIC_EXTRA_TRUNCATE ||
           expression->numeric_extra_kind == PLANNED_ROW_SCALAR_NUMERIC_EXTRA_PI;
}
